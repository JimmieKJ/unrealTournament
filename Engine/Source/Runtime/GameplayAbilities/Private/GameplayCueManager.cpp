// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "AbilitySystemPrivatePCH.h"
#include "AssetRegistryModule.h"
#include "GameplayCueInterface.h"
#include "GameplayCueManager.h"
#include "GameplayCueSet.h"
#include "GameplayTagsModule.h"
#include "GameplayCueNotify_Static.h"
#include "AbilitySystemComponent.h"
#include "Net/DataReplication.h"
#include "Engine/ActorChannel.h"
#include "UnrealNetwork.h"

#if WITH_EDITOR
#include "UnrealEd.h"
#include "SNotificationList.h"
#include "NotificationManager.h"
#include "ISequenceRecorder.h"
#define LOCTEXT_NAMESPACE "GameplayCueManager"
#endif

int32 LogGameplayCueActorSpawning = 0;
static FAutoConsoleVariableRef CVarLogGameplayCueActorSpawning(TEXT("AbilitySystem.LogGameplayCueActorSpawning"),	LogGameplayCueActorSpawning, TEXT("Log when we create GameplayCueNotify_Actors"), ECVF_Default	);

int32 DisplayGameplayCues = 0;
static FAutoConsoleVariableRef CVarDisplayGameplayCues(TEXT("AbilitySystem.DisplayGameplayCues"),	DisplayGameplayCues, TEXT("Display GameplayCue events in world as text."), ECVF_Default	);

int32 DisableGameplayCues = 0;
static FAutoConsoleVariableRef CVarDisableGameplayCues(TEXT("AbilitySystem.DisableGameplayCues"),	DisableGameplayCues, TEXT("Disables all GameplayCue events in the world."), ECVF_Default );

float DisplayGameplayCueDuration = 5.f;
static FAutoConsoleVariableRef CVarDurationeGameplayCues(TEXT("AbilitySystem.GameplayCue.DisplayDuration"),	DisplayGameplayCueDuration, TEXT("Disables all GameplayCue events in the world."), ECVF_Default );

int32 GameplayCueRunOnDedicatedServer = 0;
static FAutoConsoleVariableRef CVarDedicatedServerGameplayCues(TEXT("AbilitySystem.GameplayCue.RunOnDedicatedServer"), GameplayCueRunOnDedicatedServer, TEXT("Run gameplay cue events on dedicated server"), ECVF_Default );

#if WITH_EDITOR
USceneComponent* UGameplayCueManager::PreviewComponent = nullptr;
UWorld* UGameplayCueManager::PreviewWorld = nullptr;
#endif

UGameplayCueManager::UGameplayCueManager(const FObjectInitializer& PCIP)
: Super(PCIP)
{
#if WITH_EDITOR
	bAccelerationMapOutdated = true;
	RegisteredEditorCallbacks = false;
#endif

	GlobalCueSet = NewObject<UGameplayCueSet>(this, TEXT("GlobalCueSet"));
	CurrentWorld = nullptr;
}

void UGameplayCueManager::OnCreated()
{
	FWorldDelegates::OnWorldCleanup.AddUObject(this, &UGameplayCueManager::OnWorldCleanup);
	FWorldDelegates::OnPreWorldFinishDestroy.AddUObject(this, &UGameplayCueManager::OnWorldCleanup, true, true);

	FNetworkReplayDelegates::OnPreScrub.AddUObject(this, &UGameplayCueManager::OnPreReplayScrub);
}

bool IsDedicatedServerForGameplayCue()
{
#if WITH_EDITOR
	// This will handle dedicated server PIE case properly
	return GEngine->ShouldAbsorbCosmeticOnlyEvent();
#else
	// When in standalone non editor, this is the fastest way to check
	return IsRunningDedicatedServer();
#endif
}


void UGameplayCueManager::HandleGameplayCues(AActor* TargetActor, const FGameplayTagContainer& GameplayCueTags, EGameplayCueEvent::Type EventType, const FGameplayCueParameters& Parameters)
{
	if (GameplayCueRunOnDedicatedServer == 0 && IsDedicatedServerForGameplayCue())
	{
		return;
	}

	for (auto It = GameplayCueTags.CreateConstIterator(); It; ++It)
	{
		HandleGameplayCue(TargetActor, *It, EventType, Parameters);
	}
}

void UGameplayCueManager::HandleGameplayCue(AActor* TargetActor, FGameplayTag GameplayCueTag, EGameplayCueEvent::Type EventType, const FGameplayCueParameters& Parameters)
{
	if (DisableGameplayCues)
	{
		return;
	}

	if (GameplayCueRunOnDedicatedServer == 0 && IsDedicatedServerForGameplayCue())
	{
		return;
	}

#if WITH_EDITOR
	if (GIsEditor && TargetActor == nullptr && UGameplayCueManager::PreviewComponent)
	{
		TargetActor = Cast<AActor>(AActor::StaticClass()->GetDefaultObject());
	}
#endif

	if (TargetActor == nullptr)
	{
		ABILITY_LOG(Warning, TEXT("UGameplayCueManager::HandleGameplayCue called on null TargetActor. GameplayCueTag: %s."), *GameplayCueTag.ToString());
		return;
	}

	IGameplayCueInterface* GameplayCueInterface = Cast<IGameplayCueInterface>(TargetActor);
	bool bAcceptsCue = true;
	if (GameplayCueInterface)
	{
		bAcceptsCue = GameplayCueInterface->ShouldAcceptGameplayCue(TargetActor, GameplayCueTag, EventType, Parameters);
	}

	if (DisplayGameplayCues)
	{
		FString DebugStr = FString::Printf(TEXT("%s - %s"), *GameplayCueTag.ToString(), *EGameplayCueEventToString(EventType) );
		FColor DebugColor = FColor::Green;
		DrawDebugString(TargetActor->GetWorld(), FVector(0.f, 0.f, 100.f), DebugStr, TargetActor, DebugColor, DisplayGameplayCueDuration);
	}

	CurrentWorld = TargetActor->GetWorld();

	// Don't handle gameplay cues when world is tearing down
	if (!GetWorld() || GetWorld()->bIsTearingDown)
	{
		return;
	}

	// Give the global set a chance
	check(GlobalCueSet);
	if (bAcceptsCue)
	{
		GlobalCueSet->HandleGameplayCue(TargetActor, GameplayCueTag, EventType, Parameters);
	}

	// Use the interface even if it's not in the map
	if (GameplayCueInterface && bAcceptsCue)
	{
		GameplayCueInterface->HandleGameplayCue(TargetActor, GameplayCueTag, EventType, Parameters);
	}

	CurrentWorld = nullptr;
}

void UGameplayCueManager::EndGameplayCuesFor(AActor* TargetActor)
{
	for (auto It = NotifyMapActor.CreateIterator(); It; ++It)
	{
		FGCNotifyActorKey& Key = It.Key();
		if (Key.TargetActor == TargetActor)
		{
			AGameplayCueNotify_Actor* InstancedCue = It.Value().Get();
			if (InstancedCue)
			{
				InstancedCue->OnOwnerDestroyed(TargetActor);
			}
			It.RemoveCurrent();
		}
	}
}

int32 GameplayCueActorRecycle = 1;
static FAutoConsoleVariableRef CVarGameplayCueActorRecycle(TEXT("AbilitySystem.GameplayCueActorRecycle"), GameplayCueActorRecycle, TEXT("Allow recycling of GameplayCue Actors"), ECVF_Default );

int32 GameplayCueActorRecycleDebug = 0;
static FAutoConsoleVariableRef CVarGameplayCueActorRecycleDebug(TEXT("AbilitySystem.GameplayCueActorRecycleDebug"), GameplayCueActorRecycleDebug, TEXT("Prints logs for GC actor recycling debugging"), ECVF_Default );

bool UGameplayCueManager::IsGameplayCueRecylingEnabled()
{
	return GameplayCueActorRecycle > 0;
}

AGameplayCueNotify_Actor* UGameplayCueManager::GetInstancedCueActor(AActor* TargetActor, UClass* CueClass, const FGameplayCueParameters& Parameters)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_GameplayCueManager_GetInstancedCueActor);


	// First, see if this actor already have a GameplayCueNotifyActor already going for this CueClass
	AGameplayCueNotify_Actor* CDO = Cast<AGameplayCueNotify_Actor>(CueClass->ClassDefaultObject);
	FGCNotifyActorKey	NotifyKey(TargetActor, CueClass, 
							CDO->bUniqueInstancePerInstigator ? Parameters.GetInstigator() : nullptr, 
							CDO->bUniqueInstancePerSourceObject ? Parameters.GetSourceObject() : nullptr);

	AGameplayCueNotify_Actor* SpawnedCue = nullptr;
	if (TWeakObjectPtr<AGameplayCueNotify_Actor>* WeakPtrPtr = NotifyMapActor.Find(NotifyKey))
	{		
		SpawnedCue = WeakPtrPtr->Get();
		// If the cue is scheduled to be destroyed, don't reuse it, create a new one instead
		if (SpawnedCue && SpawnedCue->GameplayCuePendingRemove() == false)
		{
			if (SpawnedCue->GetOwner() != TargetActor)
			{
				// This should not happen. This means we think we can recycle and GC actor that is currently being used by someone else.
				ABILITY_LOG(Warning, TEXT("GetInstancedCueActor attempting to reuse GC Actor with a different owner! %s (Target: %s). Using GC Actor: %s. Current Owner: %s"), *GetNameSafe(CueClass), *GetNameSafe(TargetActor), *GetNameSafe(SpawnedCue), *GetNameSafe(SpawnedCue->GetOwner()));
			}
			else
			{
				UE_CLOG((GameplayCueActorRecycleDebug>0), LogAbilitySystem, Display, TEXT("::GetInstancedCueActor Using Existing %s (Target: %s). Using GC Actor: %s"), *GetNameSafe(CueClass), *GetNameSafe(TargetActor), *GetNameSafe(SpawnedCue));
				return SpawnedCue;
			}
		}

		// We aren't going to use this existing cue notify actor, so clear it.
		SpawnedCue = nullptr;
	}

	UWorld* World = GetWorld();

	// We don't have an instance for this, and we need one, so make one
	if (ensure(TargetActor) && ensure(CueClass))
	{
		AActor* NewOwnerActor = TargetActor;
#if WITH_EDITOR
		// Don't set owner if we are using fake CDO actor to do anim previewing
		NewOwnerActor= (TargetActor && TargetActor->HasAnyFlags(RF_ClassDefaultObject) == false ? TargetActor : nullptr);
#endif

		// Look to reuse an existing one that is stored on the CDO:
		if (GameplayCueActorRecycle > 0)
		{
			FPreallocationInfo& Info = GetPreallocationInfo(World);
			TArray<AGameplayCueNotify_Actor*>* PreallocatedList = Info.PreallocatedInstances.Find(CueClass);
			if (PreallocatedList && PreallocatedList->Num() > 0)
			{
				SpawnedCue = nullptr;
				while (true)
				{
					SpawnedCue = PreallocatedList->Pop(false);
					if (SpawnedCue && SpawnedCue->IsPendingKill() == false)
					{
						break;
					}
					
					// outside of replays, this should not happen. GC Notifies should not be actually destroyed.
					checkf(World->DemoNetDriver, TEXT("Spawned Cue is pending kill or null: %s."), *GetNameSafe(SpawnedCue));

					if (PreallocatedList->Num() <= 0)
					{
						// Ran out of preallocated instances... break and create a new one.
						break;
					}
				}

				if (SpawnedCue)
				{
					SpawnedCue->bInRecycleQueue = false;
					SpawnedCue->SetOwner(NewOwnerActor);
					SpawnedCue->SetActorLocationAndRotation(TargetActor->GetActorLocation(), TargetActor->GetActorRotation());
					SpawnedCue->ReuseAfterRecycle();					
				}

				UE_CLOG((GameplayCueActorRecycleDebug>0), LogAbilitySystem, Display, TEXT("GetInstancedCueActor Popping Recycled %s (Target: %s). Using GC Actor: %s"), *GetNameSafe(CueClass), *GetNameSafe(TargetActor), *GetNameSafe(SpawnedCue));
#if WITH_EDITOR
				// let things know that we 'spawned'
				ISequenceRecorder& SequenceRecorder	= FModuleManager::LoadModuleChecked<ISequenceRecorder>("SequenceRecorder");
				SequenceRecorder.NotifyActorStartRecording(SpawnedCue);
#endif
			}
		}

		// If we can't reuse, then spawn a new one
		if (SpawnedCue == nullptr)
		{
			FActorSpawnParameters SpawnParams;
			SpawnParams.Owner = NewOwnerActor;
			if (SpawnedCue == nullptr)
			{
				if (LogGameplayCueActorSpawning)
				{
					ABILITY_LOG(Warning, TEXT("Spawning GameplaycueActor: %s"), *CueClass->GetName());
				}

				SpawnedCue = GetWorld()->SpawnActor<AGameplayCueNotify_Actor>(CueClass, TargetActor->GetActorLocation(), TargetActor->GetActorRotation(), SpawnParams);
			}
		}

		// Associate this GameplayCueNotifyActor with this target actor/key
		if (ensure(SpawnedCue))
		{
			SpawnedCue->NotifyKey = NotifyKey;
			NotifyMapActor.Add(NotifyKey, SpawnedCue);
		}
	}

	UE_CLOG((GameplayCueActorRecycleDebug>0), LogAbilitySystem, Display, TEXT("GetInstancedCueActor  Returning %s (Target: %s). Using GC Actor: %s"), *GetNameSafe(CueClass), *GetNameSafe(TargetActor), *GetNameSafe(SpawnedCue));
	return SpawnedCue;
}

void UGameplayCueManager::NotifyGameplayCueActorFinished(AGameplayCueNotify_Actor* Actor)
{
	if (GameplayCueActorRecycle)
	{
		if (Actor->bInRecycleQueue)
		{
			// We are already in the recycle queue. This can happen normally
			// (For example the GC is removed and the owner is destroyed in the same frame)
			return;
		}

		AGameplayCueNotify_Actor* CDO = Actor->GetClass()->GetDefaultObject<AGameplayCueNotify_Actor>();
		if (CDO && Actor->Recycle())
		{
			if (Actor->IsPendingKill())
			{
				ensureMsgf(GetWorld()->DemoNetDriver, TEXT("GameplayCueNotify %s is pending kill in ::NotifyGameplayCueActorFinished (and not in network demo)"), *GetNameSafe(Actor));
				return;
			}
			Actor->bInRecycleQueue = true;

			// Remove this now from our internal map so that it doesn't get reused like a currently active cue would
			if (TWeakObjectPtr<AGameplayCueNotify_Actor>* WeakPtrPtr = NotifyMapActor.Find(Actor->NotifyKey))
			{
				// Only remove if this is the current actor in the map!
				// This could happen if a GC notify actor has a delayed removal and another GC event happens before the delayed removal happens (the old GC actor could replace the latest one in the map)
				if (WeakPtrPtr->Get() == Actor)
				{
					WeakPtrPtr->Reset();
				}
			}

			UE_CLOG((GameplayCueActorRecycleDebug>0), LogAbilitySystem, Display, TEXT("NotifyGameplayCueActorFinished %s"), *GetNameSafe(Actor));

			FPreallocationInfo& Info = GetPreallocationInfo(Actor->GetWorld());
			TArray<AGameplayCueNotify_Actor*>& PreAllocatedList = Info.PreallocatedInstances.FindOrAdd(Actor->GetClass());

			// Put the actor back in the list
			if (ensureMsgf(PreAllocatedList.Contains(Actor)==false, TEXT("GC Actor PreallocationList already contains Actor %s"), *GetNameSafe(Actor)))
			{
				PreAllocatedList.Push(Actor);
			}
			
#if WITH_EDITOR
			// let things know that we 'de-spawned'
			ISequenceRecorder& SequenceRecorder	= FModuleManager::LoadModuleChecked<ISequenceRecorder>("SequenceRecorder");
			SequenceRecorder.NotifyActorStopRecording(Actor);
#endif
			return;
		}
	}	

	// We didn't recycle, so just destroy
	Actor->Destroy();
}

// ------------------------------------------------------------------------

void UGameplayCueManager::LoadObjectLibraryFromPaths(const TArray<FString>& InPaths)
{
	if (!GameplayCueNotifyActorObjectLibrary)
	{
		GameplayCueNotifyActorObjectLibrary = UObjectLibrary::CreateLibrary(AGameplayCueNotify_Actor::StaticClass(), true, GIsEditor && !IsRunningCommandlet());
		if (GIsEditor)
		{
			GameplayCueNotifyActorObjectLibrary->bIncludeOnlyOnDiskAssets = false;
		}
	}
	if (!GameplayCueNotifyStaticObjectLibrary)
	{
		GameplayCueNotifyStaticObjectLibrary = UObjectLibrary::CreateLibrary(UGameplayCueNotify_Static::StaticClass(), true, GIsEditor && !IsRunningCommandlet());
		if (GIsEditor)
		{
			GameplayCueNotifyStaticObjectLibrary->bIncludeOnlyOnDiskAssets = false;
		}
	}

	LoadedPaths = InPaths;

	LoadObjectLibrary_Internal();
#if WITH_EDITOR
	bAccelerationMapOutdated = false;
	if (!RegisteredEditorCallbacks)
	{
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
		AssetRegistryModule.Get().OnInMemoryAssetCreated().AddUObject(this, &UGameplayCueManager::HandleAssetAdded);
		AssetRegistryModule.Get().OnInMemoryAssetDeleted().AddUObject(this, &UGameplayCueManager::HandleAssetDeleted);
		AssetRegistryModule.Get().OnAssetRenamed().AddUObject(this, &UGameplayCueManager::HandleAssetRenamed);
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
		check(GlobalCueSet);
		GlobalCueSet->Empty();

		LoadObjectLibrary_Internal();
	}
}
#endif

void UGameplayCueManager::LoadObjectLibrary_Internal()
{
	FOnGameplayCueNotifySetLoaded OnLoadDelegate = FOnGameplayCueNotifySetLoaded::CreateUObject(this, &UGameplayCueManager::OnGameplayCueNotifyAsyncLoadComplete);
	InitObjectLibraries(LoadedPaths, GameplayCueNotifyActorObjectLibrary, GameplayCueNotifyStaticObjectLibrary, OnLoadDelegate);
}

static void SearchDynamicClassCues(const FName PropertyName, const TArray<FString>& Paths, TArray<FGameplayCueReferencePair>& CuesToAdd, TArray<FStringAssetReference>& AssetsToLoad)
{
	// Iterate over all Dynamic Classes (nativized Blueprints). Search for ones with GameplayCueName tag.

	IGameplayTagsModule& GameplayTagsModule = IGameplayTagsModule::Get();
	TMap<FName, FDynamicClassStaticData>& DynamicClassMap = GetDynamicClassMap();
	for (auto PairIter : DynamicClassMap)
	{
		const FName* FoundGameplayTag = PairIter.Value.SelectedSearchableValues.Find(PropertyName);
		if (!FoundGameplayTag)
		{
			continue;
		}

		const FString ClassPath = PairIter.Key.ToString();
		for (const FString& Path : Paths)
		{
			const bool PathContainsClass = ClassPath.StartsWith(Path); // TODO: is it enough?
			if (!PathContainsClass)
			{
				continue;
			}

			ABILITY_LOG(Log, TEXT("GameplayCueManager Found a Dynamic Class: %s / %s"), *FoundGameplayTag->ToString(), *ClassPath);

			FGameplayTag  GameplayCueTag = GameplayTagsModule.GetGameplayTagsManager().RequestGameplayTag(*FoundGameplayTag, false);
			if (GameplayCueTag.IsValid())
			{
				FStringAssetReference StringRef(ClassPath); // TODO: is there any translation needed?
				ensure(StringRef.IsValid());

				CuesToAdd.Add(FGameplayCueReferencePair(GameplayCueTag, StringRef));
				AssetsToLoad.Add(StringRef);
			}
			else
			{
				ABILITY_LOG(Warning, TEXT("Found GameplayCue tag %s in Dynamic Class %s but there is no corresponding tag in the GameplayTagManager."), *FoundGameplayTag->ToString(), *ClassPath);
			}

			break;
		}
	}
}

void UGameplayCueManager::InitObjectLibraries(TArray<FString> Paths, UObjectLibrary* ActorObjectLibrary, UObjectLibrary* StaticObjectLibrary, FOnGameplayCueNotifySetLoaded OnLoadDelegate, FShouldLoadGCNotifyDelegate ShouldLoadDelegate)
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("Loading Library"), STAT_ObjectLibrary, STATGROUP_LoadTime);

	if (ActorObjectLibrary==nullptr || StaticObjectLibrary==nullptr)
	{
		return;
	}

#if WITH_EDITOR
	bAccelerationMapOutdated = false;
	FFormatNamedArguments Args;
	FScopedSlowTask SlowTask(0, FText::Format(NSLOCTEXT("AbilitySystemEditor", "BeginLoadingGameplayCueNotify", "Loading GameplayCue Library"), Args));
	SlowTask.MakeDialog();
#endif

	FScopeCycleCounterUObject PreloadScopeActor(ActorObjectLibrary);
	ActorObjectLibrary->LoadBlueprintAssetDataFromPaths(Paths);
	StaticObjectLibrary->LoadBlueprintAssetDataFromPaths(Paths);

	// ---------------------------------------------------------
	// Determine loading scheme.
	// Sync at startup in commandlets like cook.
	// Async at startup in all other cases
	// ---------------------------------------------------------

	const bool bSyncFullyLoad = IsRunningCommandlet();
	const bool bAsyncLoadAtStartup = !bSyncFullyLoad && ShouldAsyncLoadAtStartup();
	if (bSyncFullyLoad)
	{
#if STATS
		FString PerfMessage = FString::Printf(TEXT("Fully Loaded GameplayCueNotify object library"));
		SCOPE_LOG_TIME_IN_SECONDS(*PerfMessage, nullptr)
#endif
		ActorObjectLibrary->LoadAssetsFromAssetData();
		StaticObjectLibrary->LoadAssetsFromAssetData();
	}

	// ---------------------------------------------------------
	// Look for GameplayCueNotifies that handle events
	// ---------------------------------------------------------
	
	TArray<FAssetData> ActorAssetDatas;
	ActorObjectLibrary->GetAssetDataList(ActorAssetDatas);

	TArray<FAssetData> StaticAssetDatas;
	StaticObjectLibrary->GetAssetDataList(StaticAssetDatas);

	TArray<FGameplayCueReferencePair> CuesToAdd;
	TArray<FStringAssetReference> AssetsToLoad;

	// Build Cue lists for loading. Determines what from the obj library needs to be loaded
	BuildCuesToAddToGlobalSet(ActorAssetDatas, GET_MEMBER_NAME_CHECKED(AGameplayCueNotify_Actor, GameplayCueName), CuesToAdd, AssetsToLoad, ShouldLoadDelegate);
	BuildCuesToAddToGlobalSet(StaticAssetDatas, GET_MEMBER_NAME_CHECKED(UGameplayCueNotify_Static, GameplayCueName), CuesToAdd, AssetsToLoad, ShouldLoadDelegate);

	const FName PropertyName = GET_MEMBER_NAME_CHECKED(AGameplayCueNotify_Actor, GameplayCueName);
	check(PropertyName == GET_MEMBER_NAME_CHECKED(UGameplayCueNotify_Static, GameplayCueName));
	SearchDynamicClassCues(PropertyName, Paths, CuesToAdd, AssetsToLoad);

	// Add these cues to the global set
	check(GlobalCueSet);
	GlobalCueSet->AddCues(CuesToAdd);

	// Start loading them if necessary
	if (bAsyncLoadAtStartup)
	{
		auto ForwardLambda = [](TArray<FStringAssetReference> AssetList, FOnGameplayCueNotifySetLoaded OnLoadedDelegate)
		{
			OnLoadedDelegate.ExecuteIfBound(AssetList);
		};

		if (AssetsToLoad.Num() > 0)
		{			
			StreamableManager.RequestAsyncLoad(AssetsToLoad, FStreamableDelegate::CreateStatic( ForwardLambda, AssetsToLoad, OnLoadDelegate));
		}
		else
		{
			// Still fire the delegate even if nothing was found to load
			OnLoadDelegate.ExecuteIfBound(AssetsToLoad);
		}
	}
}

void UGameplayCueManager::BuildCuesToAddToGlobalSet(const TArray<FAssetData>& AssetDataList, FName TagPropertyName, TArray<FGameplayCueReferencePair>& OutCuesToAdd, TArray<FStringAssetReference>& OutAssetsToLoad, FShouldLoadGCNotifyDelegate ShouldLoad)
{
	IGameplayTagsModule& GameplayTagsModule = IGameplayTagsModule::Get();

	OutAssetsToLoad.Reserve(OutAssetsToLoad.Num() + AssetDataList.Num());

	for (FAssetData Data: AssetDataList)
	{
		// If ShouldLoad delegate is bound and it returns false, don't load this one
		if (ShouldLoad.IsBound() && (ShouldLoad.Execute(Data) == false))
		{
			continue;
		}

		const FName FoundGameplayTag = Data.GetTagValueRef<FName>(TagPropertyName);
		if (!FoundGameplayTag.IsNone())
		{
			const FString GeneratedClassTag = Data.GetTagValueRef<FString>("GeneratedClass");
			if (GeneratedClassTag.IsEmpty())
			{
				ABILITY_LOG(Warning, TEXT("Unable to find GeneratedClass value for AssetData %s"), *Data.ObjectPath.ToString());
				continue;
			}

			ABILITY_LOG(Log, TEXT("GameplayCueManager Found: %s / %s"), *FoundGameplayTag.ToString(), **GeneratedClassTag);

			FGameplayTag  GameplayCueTag = GameplayTagsModule.GetGameplayTagsManager().RequestGameplayTag(FoundGameplayTag, false);
			if (GameplayCueTag.IsValid())
			{
				// Add a new NotifyData entry to our flat list for this one
				FStringAssetReference StringRef;
				StringRef.SetPath(FPackageName::ExportTextPathToObjectPath(*GeneratedClassTag));

				OutCuesToAdd.Add(FGameplayCueReferencePair(GameplayCueTag, StringRef));

				OutAssetsToLoad.Add(StringRef);
			}
			else
			{
				ABILITY_LOG(Warning, TEXT("Found GameplayCue tag %s in asset %s but there is no corresponding tag in the GameplayTagManager."), *FoundGameplayTag.ToString(), *Data.PackageName.ToString());
			}
		}
	}
}

int32 GameplayCueCheckForTooManyRPCs = 1;
static FAutoConsoleVariableRef CVarGameplayCueCheckForTooManyRPCs(TEXT("AbilitySystem.GameplayCueCheckForTooManyRPCs"), GameplayCueCheckForTooManyRPCs, TEXT("Warns if gameplay cues are being throttled by network code"), ECVF_Default );

void UGameplayCueManager::CheckForTooManyRPCs(FName FuncName, const FGameplayCuePendingExecute& PendingCue, const FString& CueID, const FGameplayEffectContext* EffectContext)
{
	if (GameplayCueCheckForTooManyRPCs)
	{
		static IConsoleVariable* MaxRPCPerNetUpdateCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("net.MaxRPCPerNetUpdate"));
		if (MaxRPCPerNetUpdateCVar)
		{
			AActor* Owner = PendingCue.OwningComponent ? PendingCue.OwningComponent->GetOwner() : nullptr;
			UWorld* World = Owner ? Owner->GetWorld() : nullptr;
			UNetDriver* NetDriver = World ? World->GetNetDriver() : nullptr;
			if (NetDriver)
			{
				const int32 MaxRPCs = MaxRPCPerNetUpdateCVar->GetInt();
				for (UNetConnection* ClientConnection : NetDriver->ClientConnections)
				{
					if (ClientConnection)
					{
						UActorChannel** OwningActorChannelPtr = ClientConnection->ActorChannels.Find(Owner);
						TSharedRef<FObjectReplicator>* ComponentReplicatorPtr = (OwningActorChannelPtr && *OwningActorChannelPtr) ? (*OwningActorChannelPtr)->ReplicationMap.Find(PendingCue.OwningComponent) : nullptr;
						if (ComponentReplicatorPtr)
						{
							const TArray<FObjectReplicator::FRPCCallInfo>& RemoteFuncInfo = (*ComponentReplicatorPtr)->RemoteFuncInfo;
							for (const FObjectReplicator::FRPCCallInfo& CallInfo : RemoteFuncInfo)
							{
								if (CallInfo.FuncName == FuncName)
								{
									if (CallInfo.Calls > MaxRPCs)
									{
										const FString Instigator = EffectContext ? EffectContext->ToString() : TEXT("None");
										ABILITY_LOG(Warning, TEXT("Attempted to fire %s when no more RPCs are allowed this net update. Max:%d Cue:%s Instigator:%s Component:%s"), *FuncName.ToString(), MaxRPCs, *CueID, *Instigator, *GetPathNameSafe(PendingCue.OwningComponent));
									
										// Returning here to only log once per offending RPC.
										return;
									}

									break;
								}
							}
						}
					}
				}
			}
		}
	}
}

void UGameplayCueManager::OnGameplayCueNotifyAsyncLoadComplete(TArray<FStringAssetReference> AssetList)
{
	for (FStringAssetReference StringRef : AssetList)
	{
		UClass* GCClass = FindObject<UClass>(nullptr, *StringRef.ToString());
		if (ensure(GCClass))
		{
			LoadedGameplayCueNotifyClasses.Add(GCClass);
			CheckForPreallocation(GCClass);
		}
	}
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

bool UGameplayCueManager::IsAssetInLoadedPaths(UObject *Object) const
{
	for (const FString& Path : LoadedPaths)
	{
		if (Object->GetPathName().StartsWith(Path))
		{
			return true;
		}
	}

	return false;
}

void UGameplayCueManager::HandleAssetAdded(UObject *Object)
{
	UBlueprint* Blueprint = Cast<UBlueprint>(Object);
	if (Blueprint && Blueprint->GeneratedClass)
	{
		UGameplayCueNotify_Static* StaticCDO = Cast<UGameplayCueNotify_Static>(Blueprint->GeneratedClass->ClassDefaultObject);
		AGameplayCueNotify_Actor* ActorCDO = Cast<AGameplayCueNotify_Actor>(Blueprint->GeneratedClass->ClassDefaultObject);
		
		if (StaticCDO || ActorCDO)
		{
			if (IsAssetInLoadedPaths(Object))
			{
				FStringAssetReference StringRef;
				StringRef.SetPath(Blueprint->GeneratedClass->GetPathName());

				TArray<FGameplayCueReferencePair> CuesToAdd;
				if (StaticCDO)
				{
					CuesToAdd.Add(FGameplayCueReferencePair(StaticCDO->GameplayCueTag, StringRef));
				}
				else if (ActorCDO)
				{
					CuesToAdd.Add(FGameplayCueReferencePair(ActorCDO->GameplayCueTag, StringRef));
				}

				check(GlobalCueSet);
				GlobalCueSet->AddCues(CuesToAdd);

				OnGameplayCueNotifyAddOrRemove.Broadcast();

			
			}
			else
			{
				VerifyNotifyAssetIsInValidPath(Blueprint->GetOuter()->GetPathName());
			}
		}
	}
}

/** Handles cleaning up an object library if it matches the passed in object */
void UGameplayCueManager::HandleAssetDeleted(UObject *Object)
{
	FStringAssetReference StringRefToRemove;
	UBlueprint* Blueprint = Cast<UBlueprint>(Object);
	if (Blueprint && Blueprint->GeneratedClass)
	{
		UGameplayCueNotify_Static* StaticCDO = Cast<UGameplayCueNotify_Static>(Blueprint->GeneratedClass->ClassDefaultObject);
		AGameplayCueNotify_Actor* ActorCDO = Cast<AGameplayCueNotify_Actor>(Blueprint->GeneratedClass->ClassDefaultObject);
		
		if (StaticCDO || ActorCDO)
		{
			StringRefToRemove.SetPath(Blueprint->GeneratedClass->GetPathName());
		}
	}

	if (StringRefToRemove.IsValid())
	{
		TArray<FStringAssetReference> StringRefs;
		StringRefs.Add(StringRefToRemove);
		check(GlobalCueSet);
		GlobalCueSet->RemoveCuesByStringRefs(StringRefs);

		OnGameplayCueNotifyAddOrRemove.Broadcast();
	}
}

/** Handles cleaning up an object library if it matches the passed in object */
void UGameplayCueManager::HandleAssetRenamed(const FAssetData& Data, const FString& String)
{
	const FString ParentClassName = Data.GetTagValueRef<FString>("ParentClass");
	if (!ParentClassName.IsEmpty())
	{
		UClass* DataClass = FindObject<UClass>(nullptr, *ParentClassName);
		if (DataClass)
		{
			UGameplayCueNotify_Static* StaticCDO = Cast<UGameplayCueNotify_Static>(DataClass->ClassDefaultObject);
			AGameplayCueNotify_Actor* ActorCDO = Cast<AGameplayCueNotify_Actor>(DataClass->ClassDefaultObject);
			if (StaticCDO || ActorCDO)
			{
				VerifyNotifyAssetIsInValidPath(Data.PackagePath.ToString());
				GlobalCueSet->UpdateCueByStringRefs(String + TEXT("_C"), Data.ObjectPath.ToString() + TEXT("_C"));
				OnGameplayCueNotifyAddOrRemove.Broadcast();
			}
		}
	}
}

void UGameplayCueManager::VerifyNotifyAssetIsInValidPath(FString Path)
{
	bool ValidPath = false;
	for (FString& str: GetValidGameplayCuePaths())
	{
		if (Path.Contains(str))
		{
			ValidPath = true;
		}
	}

	if (!ValidPath)
	{
		FString MessageTry = FString::Printf(TEXT("Warning: Invalid GameplayCue Path %s"));
		MessageTry += TEXT("\n\nGameplayCue Notifies should only be saved in the following folders:");

		ABILITY_LOG(Warning, TEXT("Warning: Invalid GameplayCuePath: %s"), *Path);
		ABILITY_LOG(Warning, TEXT("Valid Paths: "));
		for (FString& str: GetValidGameplayCuePaths())
		{
			ABILITY_LOG(Warning, TEXT("  %s"), *str);
			MessageTry += FString::Printf(TEXT("\n  %s"), *str);
		}

		MessageTry += FString::Printf(TEXT("\n\nThis asset must be moved to a valid location to work in game."));

		const FText MessageText = FText::FromString(MessageTry);
		const FText TitleText = NSLOCTEXT("GameplayCuePathWarning", "GameplayCuePathWarningTitle", "Invalid GameplayCue Path");
		FMessageDialog::Open(EAppMsgType::Ok, MessageText, &TitleText);
	}
}

void UGameplayCueManager::LoadAllGameplayCueNotifiesForEditor()
{
	// Spft load all valid paths
	TArray<FString> ValidPaths = GetValidGameplayCuePaths();

	GlobalCueSet->Empty();
	InitObjectLibraries(ValidPaths, GameplayCueNotifyActorObjectLibrary, GameplayCueNotifyStaticObjectLibrary, FOnGameplayCueNotifySetLoaded());
}
#endif


UWorld* UGameplayCueManager::GetWorld() const
{
#if WITH_EDITOR
	if (PreviewWorld)
		return PreviewWorld;
#endif

	return CurrentWorld;
}

void UGameplayCueManager::PrintGameplayCueNotifyMap()
{
	check(GlobalCueSet);
	GlobalCueSet->PrintCues();
}

void UGameplayCueManager::PrintLoadedGameplayCueNotifyClasses()
{
	for (UClass* NotifyClass : LoadedGameplayCueNotifyClasses)
	{
		ABILITY_LOG(Display, TEXT("%s"), *GetNameSafe(NotifyClass));
	}
	ABILITY_LOG(Display, TEXT("%d total classes"), LoadedGameplayCueNotifyClasses.Num());
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

static void	PrintLoadedGameplayCueNotifyClasses(UWorld* InWorld)
{
	UAbilitySystemGlobals::Get().GetGameplayCueManager()->PrintLoadedGameplayCueNotifyClasses();
}

FAutoConsoleCommandWithWorld PrintLoadedGameplayCueNotifyClassesCommand(
	TEXT("GameplayCue.PrintLoadedGameplayCueNotifyClasses"),
	TEXT("Displays GameplayCue Notify classes that are loaded"),
	FConsoleCommandWithWorldDelegate::CreateStatic(PrintLoadedGameplayCueNotifyClasses)
	);

FScopedGameplayCueSendContext::FScopedGameplayCueSendContext()
{
	UAbilitySystemGlobals::Get().GetGameplayCueManager()->StartGameplayCueSendContext();
}
FScopedGameplayCueSendContext::~FScopedGameplayCueSendContext()
{
	UAbilitySystemGlobals::Get().GetGameplayCueManager()->EndGameplayCueSendContext();
}

template<class AllocatorType>
void PullGameplayCueTagsFromSpec(const FGameplayEffectSpec& Spec, TArray<FGameplayTag, AllocatorType>& OutArray)
{
	// Add all GameplayCue Tags from the GE into the GameplayCueTags PendingCue.list
	for (const FGameplayEffectCue& EffectCue : Spec.Def->GameplayCues)
	{
		for (const FGameplayTag& Tag: EffectCue.GameplayCueTags)
		{
			if (Tag.IsValid())
			{
				OutArray.Add(Tag);
			}
		}
	}
}

/**
 *	Enabling AbilitySystemAlwaysConvertGESpecToGCParams will mean that all calls to gameplay cues with GameplayEffectSpecs will be converted into GameplayCue Parameters server side and then replicated.
 *	This potentially saved bandwidth but also has less information, depending on how the GESpec is converted to GC Parameters and what your GC's need to know.
 */

int32 AbilitySystemAlwaysConvertGESpecToGCParams = 0;
static FAutoConsoleVariableRef CVarAbilitySystemAlwaysConvertGESpecToGCParams(TEXT("AbilitySystem.AlwaysConvertGESpecToGCParams"), AbilitySystemAlwaysConvertGESpecToGCParams, TEXT("Always convert a GameplayCue from GE Spec to GC from GC Parameters on the server"), ECVF_Default );

void UGameplayCueManager::InvokeGameplayCueAddedAndWhileActive_FromSpec(UAbilitySystemComponent* OwningComponent, const FGameplayEffectSpec& Spec, FPredictionKey PredictionKey)
{
	if (Spec.Def->GameplayCues.Num() == 0)
	{
		return;
	}

	if (AbilitySystemAlwaysConvertGESpecToGCParams)
	{
		// Transform the GE Spec into GameplayCue parmameters here (on the server)

		FGameplayCueParameters Parameters;
		UAbilitySystemGlobals::Get().InitGameplayCueParameters_GESpec(Parameters, Spec);

		static TArray<FGameplayTag, TInlineAllocator<4> > Tags;
		Tags.Reset();

		PullGameplayCueTagsFromSpec(Spec, Tags);

		if (Tags.Num() == 1)
		{
			OwningComponent->NetMulticast_InvokeGameplayCueAddedAndWhileActive_WithParams(Tags[0], PredictionKey, Parameters);
			
		}
		else if (Tags.Num() > 1)
		{
			OwningComponent->NetMulticast_InvokeGameplayCuesAddedAndWhileActive_WithParams(FGameplayTagContainer::CreateFromArray(Tags), PredictionKey, Parameters);
		}
		else
		{
			ABILITY_LOG(Warning, TEXT("No actual gameplay cue tags found in GameplayEffect %s (despite it having entries in its gameplay cue list!"), *Spec.Def->GetName());

		}
	}
	else
	{
		OwningComponent->NetMulticast_InvokeGameplayCueAddedAndWhileActive_FromSpec(Spec, PredictionKey);

	}
}

void UGameplayCueManager::InvokeGameplayCueExecuted_FromSpec(UAbilitySystemComponent* OwningComponent, const FGameplayEffectSpec& Spec, FPredictionKey PredictionKey)
{	
	if (Spec.Def->GameplayCues.Num() == 0)
	{
		// This spec doesn't have any GCs, so early out
		ABILITY_LOG(Verbose, TEXT("No GCs in this Spec, so early out: %s"), *Spec.Def->GetName());
		return;
	}

	FGameplayCuePendingExecute PendingCue;

	if (AbilitySystemAlwaysConvertGESpecToGCParams)
	{
		// Transform the GE Spec into GameplayCue parmameters here (on the server)
		PendingCue.PayloadType = EGameplayCuePayloadType::CueParameters;
		PendingCue.OwningComponent = OwningComponent;
		PendingCue.PredictionKey = PredictionKey;

		PullGameplayCueTagsFromSpec(Spec, PendingCue.GameplayCueTags);
		if (PendingCue.GameplayCueTags.Num() == 0)
		{
			ABILITY_LOG(Warning, TEXT("GE %s has GameplayCues but not valid GameplayCue tag."), *Spec.Def->GetName());			
			return;
		}
		
		UAbilitySystemGlobals::Get().InitGameplayCueParameters_GESpec(PendingCue.CueParameters, Spec);
	}
	else
	{
		// Transform the GE Spec into a FGameplayEffectSpecForRPC (holds less information than the GE Spec itself, but more information that the FGamepalyCueParameter)
		PendingCue.PayloadType = EGameplayCuePayloadType::FromSpec;
		PendingCue.OwningComponent = OwningComponent;
		PendingCue.FromSpec = FGameplayEffectSpecForRPC(Spec);
		PendingCue.PredictionKey = PredictionKey;
	}

	if (ProcessPendingCueExecute(PendingCue))
	{
		PendingExecuteCues.Add(PendingCue);
	}

	if (GameplayCueSendContextCount == 0)
	{
		// Not in a context, flush now
		FlushPendingCues();
	}
}

void UGameplayCueManager::InvokeGameplayCueExecuted(UAbilitySystemComponent* OwningComponent, const FGameplayTag GameplayCueTag, FPredictionKey PredictionKey, FGameplayEffectContextHandle EffectContext)
{
	FGameplayCuePendingExecute PendingCue;
	PendingCue.PayloadType = EGameplayCuePayloadType::CueParameters;
	PendingCue.GameplayCueTags.Add(GameplayCueTag);
	PendingCue.OwningComponent = OwningComponent;
	UAbilitySystemGlobals::Get().InitGameplayCueParameters(PendingCue.CueParameters, EffectContext);
	PendingCue.PredictionKey = PredictionKey;

	if (ProcessPendingCueExecute(PendingCue))
	{
		PendingExecuteCues.Add(PendingCue);
	}

	if (GameplayCueSendContextCount == 0)
	{
		// Not in a context, flush now
		FlushPendingCues();
	}
}

void UGameplayCueManager::InvokeGameplayCueExecuted_WithParams(UAbilitySystemComponent* OwningComponent, const FGameplayTag GameplayCueTag, FPredictionKey PredictionKey, FGameplayCueParameters GameplayCueParameters)
{
	FGameplayCuePendingExecute PendingCue;
	PendingCue.PayloadType = EGameplayCuePayloadType::CueParameters;
	PendingCue.GameplayCueTags.Add(GameplayCueTag);
	PendingCue.OwningComponent = OwningComponent;
	PendingCue.CueParameters = GameplayCueParameters;
	PendingCue.PredictionKey = PredictionKey;

	if (ProcessPendingCueExecute(PendingCue))
	{
		PendingExecuteCues.Add(PendingCue);
	}

	if (GameplayCueSendContextCount == 0)
	{
		// Not in a context, flush now
		FlushPendingCues();
	}
}

void UGameplayCueManager::StartGameplayCueSendContext()
{
	GameplayCueSendContextCount++;
}

void UGameplayCueManager::EndGameplayCueSendContext()
{
	GameplayCueSendContextCount--;

	if (GameplayCueSendContextCount == 0)
	{
		FlushPendingCues();
	}
	else if (GameplayCueSendContextCount < 0)
	{
		ABILITY_LOG(Warning, TEXT("UGameplayCueManager::EndGameplayCueSendContext called too many times! Negative context count"));
	}
}

void UGameplayCueManager::FlushPendingCues()
{
	TArray<FGameplayCuePendingExecute> LocalPendingExecuteCues = PendingExecuteCues;
	PendingExecuteCues.Empty();
	for (int32 i = 0; i < LocalPendingExecuteCues.Num(); i++)
	{
		FGameplayCuePendingExecute& PendingCue = LocalPendingExecuteCues[i];

		// Our component may have gone away
		if (PendingCue.OwningComponent)
		{
			bool bHasAuthority = PendingCue.OwningComponent->IsOwnerActorAuthoritative();
			bool bLocalPredictionKey = PendingCue.PredictionKey.IsLocalClientKey();

			// TODO: Could implement non-rpc method for replicating if desired
			switch (PendingCue.PayloadType)
			{
			case EGameplayCuePayloadType::CueParameters:
				if (ensure(PendingCue.GameplayCueTags.Num() >= 1))
				{
					if (bHasAuthority)
					{
						PendingCue.OwningComponent->ForceReplication();
						if (PendingCue.GameplayCueTags.Num() > 1)
						{
							PendingCue.OwningComponent->NetMulticast_InvokeGameplayCuesExecuted_WithParams(FGameplayTagContainer::CreateFromArray(PendingCue.GameplayCueTags), PendingCue.PredictionKey, PendingCue.CueParameters);
						}
						else
						{
							PendingCue.OwningComponent->NetMulticast_InvokeGameplayCueExecuted_WithParams(PendingCue.GameplayCueTags[0], PendingCue.PredictionKey, PendingCue.CueParameters);
							static FName NetMulticast_InvokeGameplayCueExecuted_WithParamsName = TEXT("NetMulticast_InvokeGameplayCueExecuted_WithParams");
							CheckForTooManyRPCs(NetMulticast_InvokeGameplayCueExecuted_WithParamsName, PendingCue, PendingCue.GameplayCueTags[0].ToString(), nullptr);
						}
					}
					else if (bLocalPredictionKey)
					{
						for (const FGameplayTag& Tag : PendingCue.GameplayCueTags)
						{
							PendingCue.OwningComponent->InvokeGameplayCueEvent(Tag, EGameplayCueEvent::Executed, PendingCue.CueParameters);
						}
					}
				}
				break;
			case EGameplayCuePayloadType::EffectContext:
				if (ensure(PendingCue.GameplayCueTags.Num() >= 1))
				{
					if (bHasAuthority)
					{
						PendingCue.OwningComponent->ForceReplication();
						if (PendingCue.GameplayCueTags.Num() > 1)
						{
							PendingCue.OwningComponent->NetMulticast_InvokeGameplayCuesExecuted(FGameplayTagContainer::CreateFromArray(PendingCue.GameplayCueTags), PendingCue.PredictionKey, PendingCue.CueParameters.EffectContext);
						}
						else
						{
							PendingCue.OwningComponent->NetMulticast_InvokeGameplayCueExecuted(PendingCue.GameplayCueTags[0], PendingCue.PredictionKey, PendingCue.CueParameters.EffectContext);
							static FName NetMulticast_InvokeGameplayCueExecutedName = TEXT("NetMulticast_InvokeGameplayCueExecuted");
							CheckForTooManyRPCs(NetMulticast_InvokeGameplayCueExecutedName, PendingCue, PendingCue.GameplayCueTags[0].ToString(), PendingCue.CueParameters.EffectContext.Get());
						}
					}
					else if (bLocalPredictionKey)
					{
						for (const FGameplayTag& Tag : PendingCue.GameplayCueTags)
						{
							PendingCue.OwningComponent->InvokeGameplayCueEvent(Tag, EGameplayCueEvent::Executed, PendingCue.CueParameters.EffectContext);
						}
					}
				}
				break;
			case EGameplayCuePayloadType::FromSpec:
				if (bHasAuthority)
				{
					PendingCue.OwningComponent->ForceReplication();
					PendingCue.OwningComponent->NetMulticast_InvokeGameplayCueExecuted_FromSpec(PendingCue.FromSpec, PendingCue.PredictionKey);
					static FName NetMulticast_InvokeGameplayCueExecuted_FromSpecName = TEXT("NetMulticast_InvokeGameplayCueExecuted_FromSpec");
					CheckForTooManyRPCs(NetMulticast_InvokeGameplayCueExecuted_FromSpecName, PendingCue, PendingCue.FromSpec.Def ? PendingCue.FromSpec.ToSimpleString() : TEXT("FromSpecWithNoDef"), PendingCue.FromSpec.EffectContext.Get());
				}
				else if (bLocalPredictionKey)
				{
					PendingCue.OwningComponent->InvokeGameplayCueEvent(PendingCue.FromSpec, EGameplayCueEvent::Executed);
				}
				break;
			}
		}
	}
}

bool UGameplayCueManager::ProcessPendingCueExecute(FGameplayCuePendingExecute& PendingCue)
{
	// Subclasses can do something here
	return true;
}

bool UGameplayCueManager::DoesPendingCueExecuteMatch(FGameplayCuePendingExecute& PendingCue, FGameplayCuePendingExecute& ExistingCue)
{
	const FHitResult* PendingHitResult = NULL;
	const FHitResult* ExistingHitResult = NULL;

	if (PendingCue.PayloadType != ExistingCue.PayloadType)
	{
		return false;
	}

	if (PendingCue.OwningComponent != ExistingCue.OwningComponent)
	{
		return false;
	}

	if (PendingCue.PredictionKey.PredictiveConnection != ExistingCue.PredictionKey.PredictiveConnection)
	{
		// They can both by null, but if they were predicted by different people exclude it
		return false;
	}

	if (PendingCue.PayloadType == EGameplayCuePayloadType::FromSpec)
	{
		if (PendingCue.FromSpec.Def != ExistingCue.FromSpec.Def)
		{
			return false;
		}

		if (PendingCue.FromSpec.Level != ExistingCue.FromSpec.Level)
		{
			return false;
		}
	}
	else
	{
		if (PendingCue.GameplayCueTags != ExistingCue.GameplayCueTags)
		{
			return false;
		}
	}

	return true;
}

void UGameplayCueManager::CheckForPreallocation(UClass* GCClass)
{
	if (AGameplayCueNotify_Actor* InstancedCue = Cast<AGameplayCueNotify_Actor>(GCClass->ClassDefaultObject))
	{
		if (InstancedCue->NumPreallocatedInstances > 0 && GameplayCueClassesForPreallocation.Contains(InstancedCue) == false)
		{
			// Add this to the global list
			GameplayCueClassesForPreallocation.Add(InstancedCue);

			// Add it to any world specific lists
			for (FPreallocationInfo& Info : PreallocationInfoList_Internal)
			{
				ensure(Info.ClassesNeedingPreallocation.Contains(InstancedCue)==false);
				Info.ClassesNeedingPreallocation.Push(InstancedCue);
			}
		}
	}
}

// -------------------------------------------------------------

void UGameplayCueManager::ResetPreallocation(UWorld* World)
{
	FPreallocationInfo& Info = GetPreallocationInfo(World);

	Info.PreallocatedInstances.Reset();
	Info.ClassesNeedingPreallocation = GameplayCueClassesForPreallocation;
}

void UGameplayCueManager::UpdatePreallocation(UWorld* World)
{
	FPreallocationInfo& Info = GetPreallocationInfo(World);

	if (Info.ClassesNeedingPreallocation.Num() > 0)
	{
		AGameplayCueNotify_Actor* CDO = Info.ClassesNeedingPreallocation.Last();
		TArray<AGameplayCueNotify_Actor*>& PreallocatedList = Info.PreallocatedInstances.FindOrAdd(CDO->GetClass());

		AGameplayCueNotify_Actor* PrespawnedInstance = Cast<AGameplayCueNotify_Actor>(World->SpawnActor(CDO->GetClass()));
		if (ensureMsgf(PrespawnedInstance, TEXT("Failed to prespawn GC notify for: %s"), *GetNameSafe(CDO)))
		{
			ensureMsgf(PrespawnedInstance->IsPendingKill() == false, TEXT("Newly spawned GC is PendingKILL: %s"), *GetNameSafe(CDO));

			if (LogGameplayCueActorSpawning)
			{
				ABILITY_LOG(Warning, TEXT("Prespawning GC %s"), *GetNameSafe(CDO));
			}

			PrespawnedInstance->bInRecycleQueue = true;
			PreallocatedList.Push(PrespawnedInstance);
			PrespawnedInstance->SetActorHiddenInGame(true);

			if (PreallocatedList.Num() >= CDO->NumPreallocatedInstances)
			{
				Info.ClassesNeedingPreallocation.Pop(false);
			}
		}
	}
}

FPreallocationInfo& UGameplayCueManager::GetPreallocationInfo(UWorld* World)
{
	for (FPreallocationInfo& Info : PreallocationInfoList_Internal)
	{
		if (FObjectKey(World) == Info.OwningWorldKey)
		{
			return Info;
		}
	}

	FPreallocationInfo NewInfo;
	NewInfo.OwningWorldKey = FObjectKey(World);

	PreallocationInfoList_Internal.Add(NewInfo);
	return PreallocationInfoList_Internal.Last();
}

void UGameplayCueManager::OnWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	DumpPreallocationStats(World);
#endif

	for (int32 idx=0; idx < PreallocationInfoList_Internal.Num(); ++idx)
	{
		if (PreallocationInfoList_Internal[idx].OwningWorldKey == FObjectKey(World))
		{
			ABILITY_LOG(Display, TEXT("UGameplayCueManager::OnWorldCleanup Removing PreallocationInfoList_Internal element %d"), idx);
			PreallocationInfoList_Internal.RemoveAtSwap(idx, 1, false);
			idx--;
		}
	}
}

void UGameplayCueManager::DumpPreallocationStats(UWorld* World)
{
	if (World == nullptr)
	{
		return;
	}

	FPreallocationInfo& Info = GetPreallocationInfo(World);
	for (auto &It : Info.PreallocatedInstances)
	{
		if (UClass* ThisClass = It.Key)
		{
			if (AGameplayCueNotify_Actor* CDO = ThisClass->GetDefaultObject<AGameplayCueNotify_Actor>())
			{
				TArray<AGameplayCueNotify_Actor*>& List = It.Value;
				if (List.Num() > CDO->NumPreallocatedInstances)
				{
					ABILITY_LOG(Display, TEXT("Notify class: %s was used simultaneously %d times. The CDO default is %d preallocated instanced."), *ThisClass->GetName(), List.Num(),  CDO->NumPreallocatedInstances); 
				}
			}
		}
	}
}

void UGameplayCueManager::OnPreReplayScrub(UWorld* World)
{
	FPreallocationInfo& Info = GetPreallocationInfo(World);
	Info.PreallocatedInstances.Reset();
}

#if WITH_EDITOR
#undef LOCTEXT_NAMESPACE
#endif
