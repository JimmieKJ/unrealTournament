// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AbilitySystemPrivatePCH.h"
#include "GameplayCueSet.h"
#include "GameplayCueManager.h"
#include "GameplayCueNotify_Static.h"

// --------------------------------------------------------------------------------------------------------------------------------------------------------
//
//	UGameplayCueSet
//
// --------------------------------------------------------------------------------------------------------------------------------------------------------


UGameplayCueSet::UGameplayCueSet(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

bool UGameplayCueSet::HandleGameplayCue(AActor* TargetActor, FGameplayTag GameplayCueTag, EGameplayCueEvent::Type EventType, FGameplayCueParameters Parameters)
{
	// GameplayCueTags could have been removed from the dictionary but not content. When the content is resaved the old tag will be cleaned up, but it could still come through here
	// at runtime. Since we only populate the map with dictionary gameplaycue tags, we may not find it here.
	int32* Ptr = GameplayCueDataMap.Find(GameplayCueTag);
	if (Ptr)
	{
		int32 DataIdx = *Ptr;
		return HandleGameplayCueNotify_Internal(TargetActor, DataIdx, EventType, Parameters);
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
			for (FGameplayCueNotifyData Data : GameplayCueData)
			{
				if (Data.GameplayCueTag == GameplayCueTag)
				{
					ABILITY_LOG(Warning, TEXT("AddGameplayCueData_Internal called for [%s,%s] when it already existed [%s,%s]. Skipping."), *GameplayCueTag.ToString(), *StringRef.ToString(), *Data.GameplayCueTag.ToString(), *Data.GameplayCueNotifyObj.ToString());
					return;
				}
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

bool UGameplayCueSet::HandleGameplayCueNotify_Internal(AActor* TargetActor, int32 DataIdx, EGameplayCueEvent::Type EventType, FGameplayCueParameters Parameters)
{	
	bool bReturnVal = false;

	if (DataIdx != INDEX_NONE)
	{
		check(GameplayCueData.IsValidIndex(DataIdx));

		FGameplayCueNotifyData& CueData = GameplayCueData[DataIdx];

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

					ABILITY_LOG(Warning, TEXT("GameplayCueNotify %s was not loaded when GameplayCue was invoked. Starting async loading."), *CueData.GameplayCueNotifyObj.ToString());
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
					AGameplayCueNotify_Actor* SpawnedInstancedCue = CueManager->GetInstancedCueActor(TargetActor, CueData.LoadedGameplayCueClass);
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

		GameplayCueDataMap.Add(ThisGameplayCueTag) = GameplayCueDataMap.FindChecked(Parent);
	}


	// Build up parentIdx on each item in GameplayCUeData
	for (FGameplayCueNotifyData& Data : GameplayCueData)
	{
		FGameplayTag Parent = Data.GameplayCueTag.RequestDirectParent();
		while (Parent != BaseGameplayCueTag())
		{
			int32* idxPtr = GameplayCueDataMap.Find(Parent);
			if (idxPtr)
			{
				Data.ParentDataIdx = *idxPtr;
				break;
			}
			Parent = Parent.RequestDirectParent();
		}
	}

	// PrintGameplayCueNotifyMap();
}

FGameplayTag UGameplayCueSet::BaseGameplayCueTag()
{
	static FGameplayTag BaseGameplayCueTag = IGameplayTagsModule::Get().GetGameplayTagsManager().RequestGameplayTag(TEXT("GameplayCue"), false);
	return BaseGameplayCueTag;
}