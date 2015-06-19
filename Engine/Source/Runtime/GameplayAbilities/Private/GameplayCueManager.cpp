// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AbilitySystemPrivatePCH.h"
#include "AssetRegistryModule.h"
#include "GameplayCueInterface.h"
#include "GameplayCueManager.h"
#include "GameplayCueSet.h"
#include "GameplayTagsModule.h"
#include "GameplayCueNotify_Static.h"
#include "AbilitySystemComponent.h"

#if WITH_EDITOR
#include "UnrealEd.h"
#endif

int32 DisplayGameplayCues = 0;
static FAutoConsoleVariableRef CVarDisplayGameplayCues(TEXT("AbilitySystem.DisplayGameplayCues"),	DisplayGameplayCues, TEXT("Display GameplayCue events in world as text."), ECVF_Default	);

int32 DisableGameplayCues = 0;
static FAutoConsoleVariableRef CVarDisableGameplayCues(TEXT("AbilitySystem.DisableGameplayCues"),	DisableGameplayCues, TEXT("Disables all GameplayCue events in the world."), ECVF_Default );

float DisplayGameplayCueDuration = 5.f;
static FAutoConsoleVariableRef CVarDurationeGameplayCues(TEXT("AbilitySystem.GameplayCue.DisplayDuration"),	DisplayGameplayCueDuration, TEXT("Disables all GameplayCue events in the world."), ECVF_Default );

UGameplayCueManager::UGameplayCueManager(const FObjectInitializer& PCIP)
: Super(PCIP)
{
#if WITH_EDITOR
	bAccelerationMapOutdated = true;
	RegisteredEditorCallbacks = false;
#endif

	GlobalCueSet = NewObject<UGameplayCueSet>(this, TEXT("GlobalCueSet"));
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
	if (DisableGameplayCues)
	{
		return;
	}

	if (TargetActor == nullptr)
	{
		ABILITY_LOG(Warning, TEXT("UGameplayCueManager::HandleGameplayCue called on null TargetActor. GameplayCueTag: %s."), *GameplayCueTag.ToString());
		return;
	}

	if (DisplayGameplayCues)
	{
		FString DebugStr = FString::Printf(TEXT("%s - %s"), *GameplayCueTag.ToString(), *EGameplayCueEventToString(EventType) );
		FColor DebugColor = FColor::Green;
		DrawDebugString(TargetActor->GetWorld(), FVector(0.f, 0.f, 100.f), DebugStr, TargetActor, DebugColor, DisplayGameplayCueDuration);
	}
	// Give the global set a chance
	check(GlobalCueSet);
	GlobalCueSet->HandleGameplayCue(TargetActor, GameplayCueTag, EventType, Parameters);

	// Use the interface even if it's not in the map
	IGameplayCueInterface* GameplayCueInterface = Cast<IGameplayCueInterface>(TargetActor);
	if (GameplayCueInterface)
	{
		GameplayCueInterface->HandleGameplayCue(TargetActor, GameplayCueTag, EventType, Parameters);
	}
}

void UGameplayCueManager::EndGameplayCuesFor(AActor* TargetActor)
{
	TMap<TWeakObjectPtr<UClass>, TWeakObjectPtr<AGameplayCueNotify_Actor>> FoundMapActor;
	if (NotifyMapActor.RemoveAndCopyValue(TargetActor, FoundMapActor))
	{
		for (auto It = FoundMapActor.CreateConstIterator(); It; ++It)
		{
			AGameplayCueNotify_Actor* InstancedCue = It.Value().Get();
			if (InstancedCue)
			{
				InstancedCue->OnOwnerDestroyed();
			}
		}
	}
}

AGameplayCueNotify_Actor* UGameplayCueManager::GetInstancedCueActor(AActor* TargetActor, UClass* CueClass)
{
	if (auto InnerMap = NotifyMapActor.Find(TargetActor))
	{
		if (auto WeakPtrPtr = InnerMap->Find(CueClass))
		{
			return WeakPtrPtr->Get();
		}
	}

	// We don't have an instance for this, and we need one, so make one
	AGameplayCueNotify_Actor* SpawnedCue = nullptr;
	if (ensure(TargetActor) && ensure(CueClass))
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = TargetActor;
		SpawnedCue = TargetActor->GetWorld()->SpawnActor<AGameplayCueNotify_Actor>(CueClass, TargetActor->GetActorLocation(), TargetActor->GetActorRotation(), SpawnParams);
		if (ensure(SpawnedCue))
		{
			auto& InnerMap = NotifyMapActor.Add(TargetActor);
			InnerMap.Add(CueClass) = SpawnedCue;
		}
	}

	return SpawnedCue;
}

// ------------------------------------------------------------------------

void UGameplayCueManager::LoadObjectLibraryFromPaths(const TArray<FString>& InPaths)
{
	if (!GameplayCueNotifyActorObjectLibrary)
	{
		GameplayCueNotifyActorObjectLibrary = UObjectLibrary::CreateLibrary(AGameplayCueNotify_Actor::StaticClass(), true, GIsEditor && !IsRunningCommandlet());
	}
	if (!GameplayCueNotifyStaticObjectLibrary)
	{
		GameplayCueNotifyStaticObjectLibrary = UObjectLibrary::CreateLibrary(UGameplayCueNotify_Static::StaticClass(), true, GIsEditor && !IsRunningCommandlet());
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

	// ---------------------------------------------------------
	// Determine loading scheme.
	// Sync at startup in commandlets like cook.
	// Async at startup in all other cases
	// ---------------------------------------------------------

	const bool bSyncFullyLoad = IsRunningCommandlet();
	const bool bAsyncLoadAtStartup = !bSyncFullyLoad;
	if (bSyncFullyLoad)
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

	check(GlobalCueSet);
	GlobalCueSet->Empty();

	TArray<FGameplayCueReferencePair> CuesToAdd;
	BuildCuesToAddToGlobalSet(ActorAssetDatas, GET_MEMBER_NAME_CHECKED(AGameplayCueNotify_Actor, GameplayCueName), bAsyncLoadAtStartup, CuesToAdd);
	BuildCuesToAddToGlobalSet(StaticAssetDatas, GET_MEMBER_NAME_CHECKED(UGameplayCueNotify_Static, GameplayCueName), bAsyncLoadAtStartup, CuesToAdd);

	check(GlobalCueSet);
	GlobalCueSet->AddCues(CuesToAdd);
}

void UGameplayCueManager::BuildCuesToAddToGlobalSet(const TArray<FAssetData>& AssetDataList, FName TagPropertyName, bool bAsyncLoadAfterAdd, TArray<FGameplayCueReferencePair>& OutCuesToAdd)
{
	IGameplayTagsModule& GameplayTagsModule = IGameplayTagsModule::Get();

	for (FAssetData Data: AssetDataList)
	{
		const FString* FoundGameplayTag = Data.TagsAndValues.Find(TagPropertyName);
		if (FoundGameplayTag && FoundGameplayTag->Equals(TEXT("None")) == false)
		{
			const FString* GeneratedClassTag = Data.TagsAndValues.Find(TEXT("GeneratedClass"));
			if (GeneratedClassTag == nullptr)
			{
				ABILITY_LOG(Warning, TEXT("Unable to find GeneratedClass value for AssetData %s"), *Data.ObjectPath.ToString());
				continue;
			}

			ABILITY_LOG(Log, TEXT("GameplayCueManager Found: %s / %s"), **FoundGameplayTag, **GeneratedClassTag);

			FGameplayTag  GameplayCueTag = GameplayTagsModule.GetGameplayTagsManager().RequestGameplayTag(FName(**FoundGameplayTag), false);
			if (GameplayCueTag.IsValid())
			{
				// Add a new NotifyData entry to our flat list for this one
				FStringAssetReference StringRef;
				StringRef.AssetLongPathname = FPackageName::ExportTextPathToObjectPath(*GeneratedClassTag);

				OutCuesToAdd.Add(FGameplayCueReferencePair(GameplayCueTag, StringRef));

				if (bAsyncLoadAfterAdd)
				{
					StreamableManager.SimpleAsyncLoad(StringRef);
				}
			}
			else
			{
				ABILITY_LOG(Warning, TEXT("Found GameplayCue tag %s in asset %s but there is no corresponding tag in the GameplayTagMAnager."), **FoundGameplayTag, *Data.PackageName.ToString());
			}
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
				StringRef.AssetLongPathname = Blueprint->GeneratedClass->GetPathName();

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
			StringRefToRemove.AssetLongPathname = Blueprint->GeneratedClass->GetPathName();
		}
	}

	if (StringRefToRemove.IsValid())
	{
		TArray<FStringAssetReference> StringRefs;
		StringRefs.Add(StringRefToRemove);
		check(GlobalCueSet);
		GlobalCueSet->RemoveCuesByStringRefs(StringRefs);
	}
}

#endif

void UGameplayCueManager::PrintGameplayCueNotifyMap()
{
	check(GlobalCueSet);
	GlobalCueSet->PrintCues();
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

FScopedGameplayCueSendContext::FScopedGameplayCueSendContext()
{
	UAbilitySystemGlobals::Get().GetGameplayCueManager()->StartGameplayCueSendContext();
}
FScopedGameplayCueSendContext::~FScopedGameplayCueSendContext()
{
	UAbilitySystemGlobals::Get().GetGameplayCueManager()->EndGameplayCueSendContext();
}

void UGameplayCueManager::InvokeGameplayCueExecuted_FromSpec(UAbilitySystemComponent* OwningComponent, const FGameplayEffectSpecForRPC Spec, FPredictionKey PredictionKey)
{
	FGameplayCuePendingExecute PendingCue;
	PendingCue.PayloadType = EGameplayCuePayloadType::FromSpec;
	PendingCue.OwningComponent = OwningComponent;
	PendingCue.FromSpec = Spec;
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

void UGameplayCueManager::InvokeGameplayCueExecuted(UAbilitySystemComponent* OwningComponent, const FGameplayTag GameplayCueTag, FPredictionKey PredictionKey, FGameplayEffectContextHandle EffectContext)
{
	FGameplayCuePendingExecute PendingCue;
	PendingCue.PayloadType = EGameplayCuePayloadType::EffectContext;
	PendingCue.GameplayCueTag = GameplayCueTag;
	PendingCue.OwningComponent = OwningComponent;
	PendingCue.CueParameters.EffectContext = EffectContext;
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
	PendingCue.GameplayCueTag = GameplayCueTag;
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
	for (int32 i = 0; i < PendingExecuteCues.Num(); i++)
	{
		FGameplayCuePendingExecute& PendingCue = PendingExecuteCues[i];

		// Our component may have gone away
		if (PendingCue.OwningComponent)
		{
			bool bHasAuthority = PendingCue.OwningComponent->IsOwnerActorAuthoritative();
			bool bLocalPredictionKey = PendingCue.PredictionKey.IsLocalClientKey();

			// TODO: Could implement non-rpc method for replicating if desired
			switch (PendingCue.PayloadType)
			{
			case EGameplayCuePayloadType::CueParameters:
				if (bHasAuthority)
				{
					PendingCue.OwningComponent->ForceReplication();
					PendingCue.OwningComponent->NetMulticast_InvokeGameplayCueExecuted_WithParams(PendingCue.GameplayCueTag, PendingCue.PredictionKey, PendingCue.CueParameters);
				}
				else if (bLocalPredictionKey)
				{
					PendingCue.OwningComponent->InvokeGameplayCueEvent(PendingCue.GameplayCueTag, EGameplayCueEvent::Executed, PendingCue.CueParameters);
				}
				break;
			case EGameplayCuePayloadType::EffectContext:
				if (bHasAuthority)
				{
					PendingCue.OwningComponent->ForceReplication();
					PendingCue.OwningComponent->NetMulticast_InvokeGameplayCueExecuted(PendingCue.GameplayCueTag, PendingCue.PredictionKey, PendingCue.CueParameters.EffectContext);
				}
				else if (bLocalPredictionKey)
				{
					PendingCue.OwningComponent->InvokeGameplayCueEvent(PendingCue.GameplayCueTag, EGameplayCueEvent::Executed, PendingCue.CueParameters.EffectContext);
				}
				break;
			case EGameplayCuePayloadType::FromSpec:
				if (bHasAuthority)
				{
					PendingCue.OwningComponent->ForceReplication();
					PendingCue.OwningComponent->NetMulticast_InvokeGameplayCueExecuted_FromSpec(PendingCue.FromSpec, PendingCue.PredictionKey);
				}
				else if (bLocalPredictionKey)
				{
					PendingCue.OwningComponent->InvokeGameplayCueEvent(PendingCue.FromSpec, EGameplayCueEvent::Executed);
				}
				break;
			}
		}
	}

	PendingExecuteCues.Empty();
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
		if (PendingCue.GameplayCueTag != ExistingCue.GameplayCueTag)
		{
			return false;
		}
	}

	return true;
}

