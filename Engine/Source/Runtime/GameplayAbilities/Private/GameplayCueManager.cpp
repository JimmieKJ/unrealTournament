// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AbilitySystemPrivatePCH.h"
#include "AssetRegistryModule.h"
#include "GameplayCueInterface.h"
#include "GameplayCueManager.h"
#include "GameplayTagsModule.h"
#include "GameplayCueNotify_Static.h"

#if WITH_EDITOR
#include "UnrealEd.h"
#endif


UGameplayCueManager::UGameplayCueManager(const FObjectInitializer& PCIP)
: Super(PCIP)
{
#if WITH_EDITOR
	bAccelerationMapOutdated = true;
	RegisteredEditorCallbacks = false;
#endif
}


void UGameplayCueManager::HandleGameplayCues(AActor* TargetActor, const FGameplayTagContainer& GameplayCueTags, EGameplayCueEvent::Type EventType, FGameplayCueParameters Parameters)
{
	for (auto It = GameplayCueTags.CreateConstIterator(); It; ++It)
	{
		HandleGameplayCue(TargetActor, *It, EventType, Parameters);
	}
}

void UGameplayCueManager::HandleGameplayCue(AActor* TargetActor, FGameplayTag GameplayCueTag, EGameplayCueEvent::Type EventType, FGameplayCueParameters Parameters)
{
	if (TargetActor == nullptr)
	{
		ABILITY_LOG(Warning, TEXT("UGameplayCueManager::HandleGameplayCue called on null TargetActor. GameplayCueTag: %s."), *GameplayCueTag.ToString());
		return;
	}


	// GameplayCueTags could have been removed from the dictionary but not content. When the content is resaved the old tag will be cleaned up, but it could still come through here
	// at runtime. Since we only populate the map with dictionary gameplaycue tags, we may not find it here.
	int32* Ptr=GameplayCueDataMap.Find(GameplayCueTag);
	if (Ptr)
	{
		int32 DataIdx = *Ptr;
		HandleGameplayCueNotify_Internal(TargetActor, DataIdx, EventType, Parameters);
	}

	IGameplayCueInterface* GameplayCueInterface = Cast<IGameplayCueInterface>(TargetActor);
	if (GameplayCueInterface)
	{
		GameplayCueInterface->HandleGameplayCue(TargetActor, GameplayCueTag, EventType, Parameters);
	}
}

void UGameplayCueManager::HandleGameplayCueNotify_Internal(AActor* TargetActor, int32 DataIdx, EGameplayCueEvent::Type EventType, FGameplayCueParameters Parameters)
{	
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
				return;
			}

			// See if the object is loaded but just not hooked up here
			UObject* FoundObject = FindObject<UObject>(nullptr, *CueData.GameplayCueNotifyObj.ToString());
			if (FoundObject == nullptr)
			{
				// Not loaded: start async loading and return
				StreamableManager.SimpleAsyncLoad(CueData.GameplayCueNotifyObj);

				ABILITY_LOG(Warning, TEXT("GameplayCueNotify %s was not loaded when GameplayCue was invoked. Starting async loading."), *CueData.GameplayCueNotifyObj.ToString());
				return;
			}
			else
			{
				// Found it - did we load a non-instanced version (UGameplayCueNotify_Static) or an instanced version (AGameplayCueNotify_Actor)
				UObject* LoadedGameplayCueNotify = Cast<AGameplayCueNotify_Actor>(FoundObject);
				if (!LoadedGameplayCueNotify)
				{
					//Try the other class
					LoadedGameplayCueNotify = Cast<UGameplayCueNotify_Static>(FoundObject);
				}
				if (!LoadedGameplayCueNotify)
				{
					// Not a dataasset - maybe a blueprint
					UBlueprint* GameplayCueBlueprint = Cast<UBlueprint>(FoundObject);
					if (GameplayCueBlueprint && GameplayCueBlueprint->GeneratedClass)
					{
						CueData.LoadedGameplayCueClass = GameplayCueBlueprint->GeneratedClass;
					}
					else
					{
						ABILITY_LOG(Warning, TEXT("GameplayCueNotify %s loaded object %s that is not a GameplayCueNotify"), *CueData.GameplayCueNotifyObj.ToString(), *FoundObject->GetName());
						return;
					}
				}
			}
		}

		// Handle the Notify if we found something
		if (UGameplayCueNotify_Static* NonInstancedCue = Cast<UGameplayCueNotify_Static>(CueData.LoadedGameplayCueClass->ClassDefaultObject))
		{
			if (NonInstancedCue->HandlesEvent(EventType))
			{
				NonInstancedCue->HandleGameplayCue(TargetActor, EventType, Parameters);
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
			AGameplayCueNotify_Actor* SpawnedInstancedCue = nullptr;
			if (auto InnerMap = NotifyMapActor.Find(TargetActor))
			{
				if (auto WeakPtrPtr = InnerMap->Find(DataIdx))
				{
					SpawnedInstancedCue = WeakPtrPtr->Get();
				}
			}

			//Get our instance. We should probably have a flag or something to determine if we want to reuse or stack instances. That would mean changing our map to have a list of active instances.
			if (SpawnedInstancedCue == nullptr)
			{
				// We don't have an instance for this, and we need one, so make one
				//SpawnedInstancedCue = static_cast<AGameplayCueNotify_Actor*>(StaticDuplicateObject(InstancedCue, this, TEXT("None"), ~RF_RootSet));
				SpawnedInstancedCue = TargetActor->GetWorld()->SpawnActor<AGameplayCueNotify_Actor>(InstancedCue->GetActorClass(), TargetActor->GetActorLocation(), TargetActor->GetActorRotation());
				auto& InnerMap = NotifyMapActor.FindOrAdd(TargetActor);
				InnerMap.Add(DataIdx) = SpawnedInstancedCue;
			}
			check(SpawnedInstancedCue);
			if (SpawnedInstancedCue->HandlesEvent(EventType))
			{
				SpawnedInstancedCue->HandleGameplayCue(TargetActor, EventType, Parameters);
				if (!SpawnedInstancedCue->IsOverride)
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
	}
}

void UGameplayCueManager::EndGameplayCuesFor(AActor* TargetActor)
{
	TMap<int32, TWeakObjectPtr<AGameplayCueNotify_Actor>> FoundMapActor;
	if (NotifyMapActor.RemoveAndCopyValue(TargetActor, FoundMapActor))
	{
		for (auto It = FoundMapActor.CreateConstIterator(); It; ++It)
		{
			AGameplayCueNotify_Actor* InstancedCue =  It.Value().Get();
			if (InstancedCue)
			{
				InstancedCue->OnOwnerDestroyed();
			}
		}
	}
}

// ------------------------------------------------------------------------

void UGameplayCueManager::LoadObjectLibraryFromPaths(const TArray<FString>& InPaths, bool InFullyLoad)
{
	if (!GameplayCueNotifyActorObjectLibrary)
	{
		GameplayCueNotifyActorObjectLibrary = UObjectLibrary::CreateLibrary(AGameplayCueNotify_Actor::StaticClass(), true, GIsEditor);
	}
	if (!GameplayCueNotifyStaticObjectLibrary)
	{
		GameplayCueNotifyStaticObjectLibrary = UObjectLibrary::CreateLibrary(UGameplayCueNotify_Static::StaticClass(), true, GIsEditor);
	}

	LoadedPaths = InPaths;
	bFullyLoad = InFullyLoad;

	LoadObjectLibrary_Internal();
#if WITH_EDITOR
	bAccelerationMapOutdated = false;
	if (!RegisteredEditorCallbacks)
	{
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
		AssetRegistryModule.Get().OnInMemoryAssetCreated().AddUObject(this, &UGameplayCueManager::HandleAssetAdded);
		AssetRegistryModule.Get().OnInMemoryAssetDeleted().AddUObject(this, &UGameplayCueManager::HandleAssetDeleted);
		FWorldDelegates::OnPreWorldInitialization.AddUObject(this, &UGameplayCueManager::ReloadObjectLibrary);
		RegisteredEditorCallbacks = true;
	}
#endif
}

#if WITH_EDITOR
void UGameplayCueManager::ReloadObjectLibrary(UWorld* World, const UWorld::InitializationValues IVS)
{
	if (bAccelerationMapOutdated)
	{
		LoadObjectLibrary_Internal();
	}
}
#endif

void UGameplayCueManager::LoadObjectLibrary_Internal()
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("Loading Library"), STAT_ObjectLibrary, STATGROUP_LoadTime);

#if WITH_EDITOR
	bAccelerationMapOutdated = false;
	FFormatNamedArguments Args;
	FScopedSlowTask SlowTask(0, FText::Format(NSLOCTEXT("AbilitySystemEditor", "BeginLoadingGameplayCueNotify", "Loading GameplayCue Library"), Args));
	SlowTask.MakeDialog();
#endif

	FScopeCycleCounterUObject PreloadScopeActor(GameplayCueNotifyActorObjectLibrary);
	GameplayCueNotifyActorObjectLibrary->LoadBlueprintAssetDataFromPaths(LoadedPaths);
	GameplayCueNotifyStaticObjectLibrary->LoadBlueprintAssetDataFromPaths(LoadedPaths);		//No separate cycle counter for this.

	if (bFullyLoad)
	{
#if STATS
		FString PerfMessage = FString::Printf(TEXT("Fully Loaded GameplayCueNotify object library"));
		SCOPE_LOG_TIME_IN_SECONDS(*PerfMessage, nullptr)
#endif
		GameplayCueNotifyActorObjectLibrary->LoadAssetsFromAssetData();
		GameplayCueNotifyStaticObjectLibrary->LoadAssetsFromAssetData();
	}

	// ---------------------------------------------------------
	// Look for GameplayCueNotifies that handle events
	// ---------------------------------------------------------
	
	TArray<FAssetData> ActorAssetDatas;
	GameplayCueNotifyActorObjectLibrary->GetAssetDataList(ActorAssetDatas);

	TArray<FAssetData> StaticAssetDatas;
	GameplayCueNotifyStaticObjectLibrary->GetAssetDataList(StaticAssetDatas);

	IGameplayTagsModule& GameplayTagsModule = IGameplayTagsModule::Get();

	GameplayCueData.Empty();
	GameplayCueDataMap.Empty();

	for (FAssetData Data: ActorAssetDatas)
	{
		const FString* FoundGameplayTag = Data.TagsAndValues.Find(GET_MEMBER_NAME_CHECKED(AGameplayCueNotify_Actor, GameplayCueName));
		if (FoundGameplayTag && FoundGameplayTag->Equals(TEXT("None")) == false)
		{
			ABILITY_LOG(Warning, TEXT("Found: %s"), **FoundGameplayTag);

			FGameplayTag  GameplayCueTag = GameplayTagsModule.GetGameplayTagsManager().RequestGameplayTag(FName(**FoundGameplayTag), false);
			if (GameplayCueTag.IsValid())
			{
				// Add a new NotifyData entry to our flat list for this one
				FStringAssetReference StringRef;
				StringRef.AssetLongPathname = Data.ObjectPath.ToString();
				AddGameplayCueData_Internal(GameplayCueTag, StringRef);				
			}
			else
			{
				ABILITY_LOG(Warning, TEXT("Found GameplayCue tag %s in asset %s but there is no corresponding tag in the GameplayTagMAnager."), **FoundGameplayTag, *Data.PackageName.ToString());
			}
		}
	}
	for (FAssetData Data : StaticAssetDatas)
	{
		const FString* FoundGameplayTag = Data.TagsAndValues.Find(GET_MEMBER_NAME_CHECKED(UGameplayCueNotify_Static, GameplayCueName));
		if (FoundGameplayTag && FoundGameplayTag->Equals(TEXT("None")) == false)
		{
			ABILITY_LOG(Warning, TEXT("Found: %s"), **FoundGameplayTag);

			FGameplayTag  GameplayCueTag = GameplayTagsModule.GetGameplayTagsManager().RequestGameplayTag(FName(**FoundGameplayTag), false);
			if (GameplayCueTag.IsValid())
			{
				// Add a new NotifyData entry to our flat list for this one
				FStringAssetReference StringRef;
				StringRef.AssetLongPathname = Data.ObjectPath.ToString();
				AddGameplayCueData_Internal(GameplayCueTag, StringRef);
			}
			else
			{
				ABILITY_LOG(Warning, TEXT("Found GameplayCue tag %s in asset %s but there is no corresponding tag in the GameplayTagMAnager."), **FoundGameplayTag, *Data.PackageName.ToString());
			}
		}
	}

	BuildAccelerationMap_Internal();
}

void UGameplayCueManager::AddGameplayCueData_Internal(FGameplayTag  GameplayCueTag, FStringAssetReference StringRef)
{
	// Check for duplicates: we may want to remove this eventually.
	for (FGameplayCueNotifyData Data : GameplayCueData)
	{
		if (Data.GameplayCueTag == GameplayCueTag)
		{
			ABILITY_LOG(Warning, TEXT("AddGameplayCueData_Internal called for [%s,%s] when it already existed [%s,%s]. Skipping."), *GameplayCueTag.ToString(), *StringRef.AssetLongPathname, *Data.GameplayCueTag.ToString(), *Data.GameplayCueNotifyObj.AssetLongPathname);
			return;
		}
	}

	FGameplayCueNotifyData NewData;
	NewData.GameplayCueNotifyObj = StringRef;
	NewData.GameplayCueTag = GameplayCueTag;

	GameplayCueData.Add(NewData);
}

void UGameplayCueManager::BuildAccelerationMap_Internal()
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

	for (FGameplayTag ThisGameplayCueTag : AllGameplayCueTags)
	{
		if (GameplayCueDataMap.Contains(ThisGameplayCueTag))
		{
			continue;
		}

		FGameplayTag Parent = ThisGameplayCueTag.RequestDirectParent();

		GameplayCueDataMap.Add(ThisGameplayCueTag) = GameplayCueDataMap.FindChecked(Parent);
	}

	// PrintGameplayCueNotifyMap();
}

int32 UGameplayCueManager::FinishLoadingGameplayCueNotifies()
{
	int32 NumLoadeded = 0;
	return NumLoadeded;
}

void UGameplayCueManager::BeginLoadingGameplayCueNotify(FGameplayTag GameplayCueTag)
{

}

#if WITH_EDITOR

void UGameplayCueManager::HandleAssetAdded(UObject *Object)
{
	UBlueprint* Blueprint = Cast<UBlueprint>(Object);
	if (Blueprint && Blueprint->GeneratedClass)
	{
		UGameplayCueNotify_Static* StaticCDO = Cast<UGameplayCueNotify_Static>(Blueprint->GeneratedClass->ClassDefaultObject);
		FStringAssetReference StringRef;
		StringRef.AssetLongPathname = Blueprint->GetPathName();
		if (StaticCDO && StaticCDO->GameplayCueTag.IsValid())
		{
			AddGameplayCueData_Internal(StaticCDO->GameplayCueTag, StringRef);
			BuildAccelerationMap_Internal();		//RICKH Could this just be done by setting the acceleration map rebuild bool to true?
		}
		else
		{
			AGameplayCueNotify_Actor* ActorCDO = Cast<AGameplayCueNotify_Actor>(Blueprint->GeneratedClass->ClassDefaultObject);
			if (ActorCDO && ActorCDO->GameplayCueTag.IsValid())
			{
				AddGameplayCueData_Internal(ActorCDO->GameplayCueTag, StringRef);
				BuildAccelerationMap_Internal();		//RICKH Could this just be done by setting the acceleration map rebuild bool to true?
			}
		}
	}
}

/** Handles cleaning up an object library if it matches the passed in object */
void UGameplayCueManager::HandleAssetDeleted(UObject *Object)
{
	FGameplayTag TagtoRemove;

	UBlueprint* Blueprint = Cast<UBlueprint>(Object);
	if (Blueprint && Blueprint->GeneratedClass)
	{
		AGameplayCueNotify_Actor* CDO = Cast<AGameplayCueNotify_Actor>(Blueprint->GeneratedClass->ClassDefaultObject);
		if (CDO && CDO->GameplayCueTag.IsValid())
		{
			TagtoRemove = CDO->GameplayCueTag;
		}
	}

	AGameplayCueNotify_Actor* Notify = Cast<AGameplayCueNotify_Actor>(Object);
	if (Notify && Notify->GameplayCueTag.IsValid())
	{
		TagtoRemove = Notify->GameplayCueTag;
	}

	if (TagtoRemove.IsValid())
	{
		for (int32 idx = 0; idx < GameplayCueData.Num(); ++idx)
		{
			if (GameplayCueData[idx].GameplayCueTag == TagtoRemove)
			{
				GameplayCueData.RemoveAt(idx);
				BuildAccelerationMap_Internal();
				break;
			}
		}
	}
}

#endif

void UGameplayCueManager::PrintGameplayCueNotifyMap()
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

FGameplayTag UGameplayCueManager::BaseGameplayCueTag()
{
	static FGameplayTag  BaseGameplayCueTag = IGameplayTagsModule::Get().GetGameplayTagsManager().RequestGameplayTag(TEXT("GameplayCue"), false);
	return BaseGameplayCueTag;
}

static void	PrintGameplayCueNotifyMapConsoleCommandFunc(UWorld* InWorld)
{
	UAbilitySystemGlobals::Get().GetGameplayCueManager()->PrintGameplayCueNotifyMap();
}

FAutoConsoleCommandWithWorld PrintGameplayCueNotifyMapConsoleCommand(
	TEXT("GameplayCue.PrintGameplayCueNotifyMap"),
	TEXT("Displays GameplayCue notify map"),
	FConsoleCommandWithWorldDelegate::CreateStatic(PrintGameplayCueNotifyMapConsoleCommandFunc)
	);
