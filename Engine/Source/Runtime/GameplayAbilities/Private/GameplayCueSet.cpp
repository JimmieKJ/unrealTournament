// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "AbilitySystemPrivatePCH.h"
#include "GameplayCueSet.h"
#include "GameplayCueManager.h"
#include "GameplayCueNotify_Static.h"
#include "GameplayCueManager.h"

// --------------------------------------------------------------------------------------------------------------------------------------------------------
//
//	UGameplayCueSet
//
// --------------------------------------------------------------------------------------------------------------------------------------------------------


UGameplayCueSet::UGameplayCueSet(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

bool UGameplayCueSet::HandleGameplayCue(AActor* TargetActor, FGameplayTag GameplayCueTag, EGameplayCueEvent::Type EventType, const FGameplayCueParameters& Parameters)
{
	// GameplayCueTags could have been removed from the dictionary but not content. When the content is resaved the old tag will be cleaned up, but it could still come through here
	// at runtime. Since we only populate the map with dictionary gameplaycue tags, we may not find it here.
	int32* Ptr = GameplayCueDataMap.Find(GameplayCueTag);
	if (Ptr)
	{
		int32 DataIdx = *Ptr;

		// TODO - resolve internal handler modifying params before passing them on with new const-ref params.
		FGameplayCueParameters writableParameters = Parameters;
		return HandleGameplayCueNotify_Internal(TargetActor, DataIdx, EventType, writableParameters);
	}

	return false;
}

void UGameplayCueSet::AddCues(const TArray<FGameplayCueReferencePair>& CuesToAdd)
{
	if (CuesToAdd.Num() > 0)
	{
		for (const FGameplayCueReferencePair& CueRefPair : CuesToAdd)
		{
			const FGameplayTag& GameplayCueTag = CueRefPair.GameplayCueTag;
			const FStringAssetReference& StringRef = CueRefPair.StringRef;

			// Check for duplicates: we may want to remove this eventually.
			bool bDupe = false;
			for (FGameplayCueNotifyData& Data : GameplayCueData)
			{
				if (Data.GameplayCueTag == GameplayCueTag)
				{
					ABILITY_LOG(Verbose, TEXT("AddGameplayCueData_Internal called for [%s,%s] when it already existed [%s,%s]. Skipping."), *GameplayCueTag.ToString(), *StringRef.ToString(), *Data.GameplayCueTag.ToString(), *Data.GameplayCueNotifyObj.ToString());
					bDupe = true;
					break;
				}
			}

			if (bDupe)
			{
				continue;
			}

			FGameplayCueNotifyData NewData;
			NewData.GameplayCueNotifyObj = StringRef;
			NewData.GameplayCueTag = GameplayCueTag;

			GameplayCueData.Add(NewData);
		}

		BuildAccelerationMap_Internal();
	}
}

void UGameplayCueSet::RemoveCuesByTags(const FGameplayTagContainer& TagsToRemove)
{
}

void UGameplayCueSet::RemoveCuesByStringRefs(const TArray<FStringAssetReference>& CuesToRemove)
{
	for (const FStringAssetReference& StringRefToRemove : CuesToRemove)
	{
		for (int32 idx = 0; idx < GameplayCueData.Num(); ++idx)
		{
			if (GameplayCueData[idx].GameplayCueNotifyObj == StringRefToRemove)
			{
				GameplayCueData.RemoveAt(idx);
				BuildAccelerationMap_Internal();
				break;
			}
		}
	}
}

void UGameplayCueSet::RemoveLoadedClass(UClass* Class)
{
	for (int32 idx = 0; idx < GameplayCueData.Num(); ++idx)
	{
		if (GameplayCueData[idx].LoadedGameplayCueClass == Class)
		{
			GameplayCueData[idx].LoadedGameplayCueClass = nullptr;
		}
	}
}

#if WITH_EDITOR
void UGameplayCueSet::UpdateCueByStringRefs(const FStringAssetReference& CueToRemove, FString NewPath)
{
	
	for (int32 idx = 0; idx < GameplayCueData.Num(); ++idx)
	{
		if (GameplayCueData[idx].GameplayCueNotifyObj == CueToRemove)
		{
			GameplayCueData[idx].GameplayCueNotifyObj = NewPath;
			BuildAccelerationMap_Internal();
			break;
		}
	}
}
#endif

void UGameplayCueSet::Empty()
{
	GameplayCueData.Empty();
	GameplayCueDataMap.Empty();
}

void UGameplayCueSet::PrintCues() const
{
	FGameplayTagContainer AllGameplayCueTags = IGameplayTagsModule::Get().GetGameplayTagsManager().RequestGameplayTagChildren(BaseGameplayCueTag());

	for (FGameplayTag ThisGameplayCueTag : AllGameplayCueTags)
	{
		int32 idx = GameplayCueDataMap.FindChecked(ThisGameplayCueTag);
		if (idx != INDEX_NONE)
		{
			ABILITY_LOG(Warning, TEXT("   %s -> %d"), *ThisGameplayCueTag.ToString(), idx);
		}
		else
		{
			ABILITY_LOG(Warning, TEXT("   %s -> unmapped"), *ThisGameplayCueTag.ToString());
		}
	}
}

bool UGameplayCueSet::HandleGameplayCueNotify_Internal(AActor* TargetActor, int32 DataIdx, EGameplayCueEvent::Type EventType, FGameplayCueParameters& Parameters)
{	
	bool bReturnVal = false;

	if (DataIdx != INDEX_NONE)
	{
		check(GameplayCueData.IsValidIndex(DataIdx));

		FGameplayCueNotifyData& CueData = GameplayCueData[DataIdx];

		Parameters.MatchedTagName = CueData.GameplayCueTag;

		// If object is not loaded yet
		if (CueData.LoadedGameplayCueClass == nullptr)
		{
			// Ignore removed events if this wasn't already loaded (only call Removed if we handled OnActive/WhileActive)
			if (EventType == EGameplayCueEvent::Removed)
			{
				return false;
			}

			// See if the object is loaded but just not hooked up here
			CueData.LoadedGameplayCueClass = FindObject<UClass>(nullptr, *CueData.GameplayCueNotifyObj.ToString());
			if (CueData.LoadedGameplayCueClass == nullptr)
			{
				// Not loaded: start async loading and return
				UGameplayCueManager* CueManager = UAbilitySystemGlobals::Get().GetGameplayCueManager();
				if (ensure(CueManager))
				{
					CueManager->StreamableManager.SimpleAsyncLoad(CueData.GameplayCueNotifyObj);

					ABILITY_LOG(Display, TEXT("GameplayCueNotify %s was not loaded when GameplayCue was invoked. Starting async loading."), *CueData.GameplayCueNotifyObj.ToString());
				}
				return false;
			}
		}

		// Handle the Notify if we found something
		if (UGameplayCueNotify_Static* NonInstancedCue = Cast<UGameplayCueNotify_Static>(CueData.LoadedGameplayCueClass->ClassDefaultObject))
		{
			if (NonInstancedCue->HandlesEvent(EventType))
			{
				NonInstancedCue->HandleGameplayCue(TargetActor, EventType, Parameters);
				bReturnVal = true;
				if (!NonInstancedCue->IsOverride)
				{
					HandleGameplayCueNotify_Internal(TargetActor, CueData.ParentDataIdx, EventType, Parameters);
				}
			}
			else
			{
				//Didn't even handle it, so IsOverride should not apply.
				HandleGameplayCueNotify_Internal(TargetActor, CueData.ParentDataIdx, EventType, Parameters);
			}
		}
		else if (AGameplayCueNotify_Actor* InstancedCue = Cast<AGameplayCueNotify_Actor>(CueData.LoadedGameplayCueClass->ClassDefaultObject))
		{
			if (InstancedCue->HandlesEvent(EventType))
			{
				UGameplayCueManager* CueManager = UAbilitySystemGlobals::Get().GetGameplayCueManager();
				if (ensure(CueManager))
				{
					//Get our instance. We should probably have a flag or something to determine if we want to reuse or stack instances. That would mean changing our map to have a list of active instances.
					AGameplayCueNotify_Actor* SpawnedInstancedCue = CueManager->GetInstancedCueActor(TargetActor, CueData.LoadedGameplayCueClass, Parameters);
					if (ensure(SpawnedInstancedCue))
					{
						SpawnedInstancedCue->HandleGameplayCue(TargetActor, EventType, Parameters);
						bReturnVal = true;
						if (!SpawnedInstancedCue->IsOverride)
						{
							HandleGameplayCueNotify_Internal(TargetActor, CueData.ParentDataIdx, EventType, Parameters);
						}
					}
				}
			}
			else
			{
				//Didn't even handle it, so IsOverride should not apply.
				HandleGameplayCueNotify_Internal(TargetActor, CueData.ParentDataIdx, EventType, Parameters);
			}
		}
	}

	return bReturnVal;
}

void UGameplayCueSet::BuildAccelerationMap_Internal()
{
	// ---------------------------------------------------------
	//	Build up the rest of the acceleration map: every GameplayCue tag should have an entry in the map that points to the index into GameplayCueData to use when it is invoked.
	//	(or to -1 if no GameplayCueNotify is associated with that tag)
	// 
	// ---------------------------------------------------------

	GameplayCueDataMap.Empty();
	GameplayCueDataMap.Add(BaseGameplayCueTag()) = INDEX_NONE;

	for (int32 idx = 0; idx < GameplayCueData.Num(); ++idx)
	{
		GameplayCueDataMap.FindOrAdd(GameplayCueData[idx].GameplayCueTag) = idx;
	}

	FGameplayTagContainer AllGameplayCueTags = IGameplayTagsModule::Get().GetGameplayTagsManager().RequestGameplayTagChildren(BaseGameplayCueTag());


	// Create entries for children.
	// E.g., if "a.b" notify exists but "a.b.c" does not, point "a.b.c" entry to "a.b"'s notify.
	for (FGameplayTag ThisGameplayCueTag : AllGameplayCueTags)
	{
		if (GameplayCueDataMap.Contains(ThisGameplayCueTag))
		{
			continue;
		}

		FGameplayTag Parent = ThisGameplayCueTag.RequestDirectParent();

		int32 ParentValue = GameplayCueDataMap.FindChecked(Parent);
		GameplayCueDataMap.Add(ThisGameplayCueTag, ParentValue);
	}


	// Build up parentIdx on each item in GameplayCUeData
	for (FGameplayCueNotifyData& Data : GameplayCueData)
	{
		FGameplayTag Parent = Data.GameplayCueTag.RequestDirectParent();
		while (Parent != BaseGameplayCueTag() && Parent.IsValid())
		{
			int32* idxPtr = GameplayCueDataMap.Find(Parent);
			if (idxPtr)
			{
				Data.ParentDataIdx = *idxPtr;
				break;
			}
			Parent = Parent.RequestDirectParent();
			if (Parent.GetTagName() == NAME_None)
			{
				break;
			}
		}
	}

	// PrintGameplayCueNotifyMap();
}

FGameplayTag UGameplayCueSet::BaseGameplayCueTag()
{
	// Note we should not cache this off as a static variable, since for new projects the GameplayCue tag will not be found until one is created.
	return IGameplayTagsModule::Get().GetGameplayTagsManager().RequestGameplayTag(TEXT("GameplayCue"), false);
}