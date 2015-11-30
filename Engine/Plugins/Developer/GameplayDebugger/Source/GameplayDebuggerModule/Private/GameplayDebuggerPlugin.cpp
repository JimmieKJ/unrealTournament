// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "GameplayDebuggerPluginPrivatePCH.h"
#include "Misc/CoreMisc.h"
#include "GameplayDebuggerReplicator.h"
#include "GameplayDebuggerModuleSettings.h"
#include "GameFramework/PlayerState.h"
#include "AssetRegistryModule.h"

#if WITH_EDITOR
#include "Editor/EditorEngine.h"
#include "ISettingsModule.h"
#include "LevelEditor.h"
#include "PropertyEditorModule.h"
#include "GameplayDebuggerDetails.h"
#endif // WITH_EDITOR

#define LOCTEXT_NAMESPACE "FGameplayDebuggerPlugin"

#if ENABLED_GAMEPLAY_DEBUGGER
static TAutoConsoleVariable<int32> CVarUseGameplayDebuggerPlugin(
	TEXT("ai.gd.UseGameplayDebuggerPlugin"),
	0,
	TEXT("Enable or disable new GameplayDebugger plugin.\n")
	TEXT(" 0: Disable details (default)\n")
	TEXT(" 1: Enable details"),
	ECVF_Cheat);
#endif

class FGameplayDebuggerModule : public IGameplayDebuggerPlugin, private FSelfRegisteringExec
{
public:
	// Begine IModuleInterface implementation
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	// End IModuleInterface implementation

	// Begin FSelfRegisteringExec Interface
	virtual bool Exec(UWorld* Inworld, const TCHAR* Cmd, FOutputDevice& Ar) override;
	// End FSelfRegisteringExec Interface

	// Begine IGameplayDebuggerPlugin implementation
	virtual AActor* CreateGameplayDebuggerForPlayerController(APlayerController* PlayerController) override;
	// End IGameplayDebuggerPlugin implementation

private:
	void AssetRegistryLoadComplete();
	void InMemoryAssetDeleted(UObject*);
	void InMemoryAssetCreated(UObject*);

	void WorldAdded(UWorld* InWorld);
	bool DoesGameplayDebuggingReplicatorExistForPlayerController(const APlayerController* PlayerController);
	void HandleSettingChanged(FName PropertyName);

	// Module Helpers
	bool SpawnHelperActor(UWorld* InWorld);
	void UpdateOrAddMapping(const struct FInputActionKeyMapping& Mapping, TArray<struct FInputActionKeyMapping>& ActionMappings);
	void UpdateOrAddBinding(const struct FKeyBind& Binding, TArray<struct FKeyBind>& DebugExecBindings);
	void HackyHelperSpawns();
	void InitializeRuntimeServerActors();
	void CleanupRuntimeServerActors();
	void RegisterGameplayDebuggerBaseClasses();
	void SetupKeyboardBindings();
	void CleanupKeyboardBindings();
	void SetupEditor();
	void CleanupEditor();

	TMap<TWeakObjectPtr<UWorld>, TArray<TWeakObjectPtr<AGameplayDebuggerReplicator> > > AllReplilcatorsPerWorlds;
	uint32 NotActivatedByOldGameplayDebugger : 1;
};

IMPLEMENT_MODULE(FGameplayDebuggerModule, GameplayDebuggerModule)

void FGameplayDebuggerModule::StartupModule()
{
	// This code will execute after your module is loaded into memory (but after global variables are initialized, of course.)
	UE_LOG(LogGameplayDebugger, Log, TEXT("FGameplayDebuggerPlugin::StartupModule()"));

#if ENABLED_GAMEPLAY_DEBUGGER
	NotActivatedByOldGameplayDebugger = FModuleManager::Get().IsModuleLoaded("GameplayDebugger");
	if (NotActivatedByOldGameplayDebugger)
	{
		UE_LOG(LogGameplayDebugger, Warning, TEXT("Deprecated '/Engine/Source/Developer/GameplayDebugger' module is active. Please switch over to new GameplayDebugger plugin."));
	}

	if (GEngine)
	{
		GEngine->OnWorldAdded().AddRaw(this, &FGameplayDebuggerModule::WorldAdded);

		InitializeRuntimeServerActors();
		
		HackyHelperSpawns();// HACK: PostEngineInit module is loaded after world init on dedicated server (reported bug) so we have to hack it a little for now

		RegisterGameplayDebuggerBaseClasses();
		SetupEditor();
		SetupKeyboardBindings();
	}
#endif //ENABLED_GAMEPLAY_DEBUGGER
}

void FGameplayDebuggerModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
#if ENABLED_GAMEPLAY_DEBUGGER
	if (GEngine)
	{
		GEngine->OnWorldAdded().RemoveAll(this);

		CleanupRuntimeServerActors();
		CleanupEditor();
		CleanupKeyboardBindings();
	}
#endif //ENABLED_GAMEPLAY_DEBUGGER
}

void FGameplayDebuggerModule::AssetRegistryLoadComplete()
{
#if ENABLED_GAMEPLAY_DEBUGGER
	UGameplayDebuggerModuleSettings* Settings = UGameplayDebuggerModuleSettings::StaticClass()->GetDefaultObject<UGameplayDebuggerModuleSettings>();
	Settings->LoadAnyMissingClasses();
	Settings->UpdateCategories(true);
#endif// ENABLED_GAMEPLAY_DEBUGGER
}

void FGameplayDebuggerModule::InMemoryAssetDeleted(UObject* Obj)
{
#if ENABLED_GAMEPLAY_DEBUGGER
	UBlueprint* Blueprint = Cast<UBlueprint>(Obj);
	if (Blueprint && Blueprint->ParentClass && Blueprint->ParentClass->IsChildOf<UGameplayDebuggerBaseObject>())
	{
		UGameplayDebuggerModuleSettings* Settings = UGameplayDebuggerModuleSettings::StaticClass()->GetDefaultObject<UGameplayDebuggerModuleSettings>();
		Settings->LoadAnyMissingClasses();
		Settings->UpdateCategories();
		Settings->UpdateDefaultConfigFile(); //force update config, to remove old class(es)
	}
#endif// ENABLED_GAMEPLAY_DEBUGGER
}

void FGameplayDebuggerModule::InMemoryAssetCreated(UObject* Obj)
{
#if ENABLED_GAMEPLAY_DEBUGGER
	UBlueprint* Blueprint = Cast<UBlueprint>(Obj);
	if (Blueprint && Blueprint->ParentClass && Blueprint->ParentClass->IsChildOf<UGameplayDebuggerBaseObject>())
	{
		UGameplayDebuggerModuleSettings* Settings = UGameplayDebuggerModuleSettings::StaticClass()->GetDefaultObject<UGameplayDebuggerModuleSettings>();
		Settings->LoadAnyMissingClasses();
		Settings->UpdateCategories();
	}
#endif// ENABLED_GAMEPLAY_DEBUGGER
}

void FGameplayDebuggerModule::HandleSettingChanged(FName PropertyName)
{
#if ENABLED_GAMEPLAY_DEBUGGER && WITH_EDITOR
	const FInputActionKeyMapping& Mapping = UGameplayDebuggerModuleSettings::StaticClass()->GetDefaultObject<UGameplayDebuggerModuleSettings>()->ActivationAction;
	UpdateOrAddMapping(Mapping, UInputSettings::StaticClass()->GetDefaultObject<UInputSettings>()->ActionMappings);

	TArray<UObject *> Results;
	GetObjectsOfClass(UInputSettings::StaticClass(), Results, /*bIncludeDerivedClasses =*/ true, /*EObjectFlags AdditionalExcludeFlags =*/ RF_ClassDefaultObject);
	for (UObject* Obj : Results)
	{
		UInputSettings* InputSetting = Cast<UInputSettings>(Obj);
		UpdateOrAddMapping(Mapping, InputSetting->ActionMappings);
	}

	FKeyBind KeyBind;
	KeyBind.Command = Mapping.ActionName.ToString() + TEXT(" DebugExec");
	KeyBind.Key = Mapping.Key;
	KeyBind.Alt = Mapping.bAlt;
	KeyBind.Control = Mapping.bCtrl;
	KeyBind.Shift = Mapping.bShift;
	KeyBind.Cmd = Mapping.bCmd;
	UpdateOrAddBinding(KeyBind, UPlayerInput::StaticClass()->GetDefaultObject<UPlayerInput>()->DebugExecBindings);

	Results.Reset();
	GetObjectsOfClass(UPlayerInput::StaticClass(), Results, /*bIncludeDerivedClasses =*/ true, /*EObjectFlags AdditionalExcludeFlags =*/ RF_ClassDefaultObject);
	for (UObject* Obj : Results)
	{
		UPlayerInput* PlayerInput = Cast<UPlayerInput>(Obj);
		if (PlayerInput)
		{
			UpdateOrAddBinding(KeyBind, PlayerInput->DebugExecBindings);
			UpdateOrAddMapping(Mapping, PlayerInput->ActionMappings);
		}
	}
#endif //ENABLED_GAMEPLAY_DEBUGGER && WITH_EDITOR
}

bool FGameplayDebuggerModule::DoesGameplayDebuggingReplicatorExistForPlayerController(const APlayerController* PlayerController)
{
#if ENABLED_GAMEPLAY_DEBUGGER
	const UWorld* World = PlayerController ? PlayerController->GetWorld() : nullptr;
	if (World == NULL)
	{
		return false;
	}

	auto& WorldReplicatorActors = AllReplilcatorsPerWorlds.FindOrAdd(World);
	for (auto It = WorldReplicatorActors.CreateConstIterator(); It; ++It)
	{
		TWeakObjectPtr<AGameplayDebuggerReplicator> Replicator = *It;
		if (Replicator.IsValid())
		{
			if (Replicator->GetLocalPlayerOwner() == PlayerController)
			{
				return true;
			}
		}
	}
#endif

	return false;
}

AActor* FGameplayDebuggerModule::CreateGameplayDebuggerForPlayerController(APlayerController* PlayerController)
{
#if ENABLED_GAMEPLAY_DEBUGGER
	if (PlayerController == NULL || Cast<ADebugCameraController>(PlayerController) != nullptr )
	{
		return nullptr;
	}

	bool bIsServer = PlayerController->GetNetMode() < ENetMode::NM_Client; // (Only create on some sort of server)
	if (!bIsServer)
	{
		return nullptr;
	}

	UWorld* World = PlayerController->GetWorld();
	if (World == NULL)
	{
		return nullptr;
	}

	if (DoesGameplayDebuggingReplicatorExistForPlayerController(PlayerController))
	{
		// No need to create one if we already have one.
		return nullptr;
	}

	FActorSpawnParameters SpawnInfo;
	SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnInfo.ObjectFlags = RF_Transient;
	SpawnInfo.Name = *FString::Printf(TEXT("GameplayDebuggerReplicator_%s"), *PlayerController->GetName());
	AGameplayDebuggerReplicator* DestActor = World->SpawnActor<AGameplayDebuggerReplicator>(FVector::ZeroVector, FRotator::ZeroRotator, SpawnInfo);
	if (DestActor != NULL)
	{
		DestActor->PrimaryActorTick.bStartWithTickEnabled = false;
		DestActor->SetActorTickEnabled(false);
		DestActor->SetLocalPlayerOwner(PlayerController);
		DestActor->SetReplicates(true);

		auto& WorldReplicatorActors = AllReplilcatorsPerWorlds.FindOrAdd(World);
		WorldReplicatorActors.Add(DestActor);
		return DestActor;
	}
#endif

	return nullptr;
}

void FGameplayDebuggerModule::WorldAdded(UWorld* InWorld)
{
#if ENABLED_GAMEPLAY_DEBUGGER
	bool bIsStandalone = InWorld && InWorld->GetNetMode() == ENetMode::NM_Standalone; // (Only standalone game code)
	if (InWorld == NULL || InWorld->IsPendingKill() || !bIsStandalone)
	{
		return;
	}

	for (TActorIterator<AGameplayDebuggerServerHelper> It(InWorld); It; ++It)
	{
		if (*It)
		{
			//already with helper
			return;
		}
	}

	// create global replicator on level
	SpawnHelperActor(InWorld);
#endif //ENABLED_GAMEPLAY_DEBUGGER
}

/************************************************************************/
/* Gameplay Debugger custom Exec                                        */
/************************************************************************/

bool FGameplayDebuggerModule::Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar)
{
#if ENABLED_GAMEPLAY_DEBUGGER

	if (InWorld == nullptr || InWorld->GetNetMode() == NM_DedicatedServer)
	{
		// Just wait for Replicator to be created on server - AGameplayDebuggerReplicator cares about "gd.activate" input action. 
		// AGameplayDebuggerReplicator actor is going to be created by AGameplayDebuggerServerHelper on servers.
		return false;
	}

	bool bEnableGDT = FParse::Command(&Cmd, TEXT("EnableGDT"));
	const bool bOldGameplayDebuggerActive = FModuleManager::Get().IsModuleLoaded("GameplayDebugger");
	if (bOldGameplayDebuggerActive)
	{
		return false;
	}

	if (!bEnableGDT && InWorld->GetNetMode() == NM_Client)
	{
		return false;
	}

	const FInputActionKeyMapping& Mapping = UGameplayDebuggerModuleSettings::StaticClass()->GetDefaultObject<UGameplayDebuggerModuleSettings>()->ActivationAction;
	const bool bIsGameplayDebuggetCommand = bEnableGDT || FParse::Command(&Cmd, *Mapping.ActionName.ToString());
	const bool bIsRunAsDebugExec = bIsGameplayDebuggetCommand ? FParse::Command(&Cmd, TEXT("DebugExec")) : false;

	APlayerController* LocalPC = bIsGameplayDebuggetCommand && InWorld->GetGameInstance() ? InWorld->GetGameInstance()->GetFirstLocalPlayerController() : nullptr;
	if (LocalPC == nullptr)
	{
		return false;
	}

	// let's find replicator connected with LocalPC player controller
	AGameplayDebuggerReplicator* Replicator = nullptr;
	for (TActorIterator<AGameplayDebuggerReplicator> It(InWorld); It; ++It)
	{
		Replicator = *It;
		if (Replicator && !Replicator->IsPendingKill())
		{
			APlayerController* PCOwner = Replicator->GetLocalPlayerOwner();
			if (LocalPC == PCOwner)
			{
				break;
			}
		}
		Replicator = nullptr;
	}

	if (Replicator == nullptr && (InWorld->GetNetMode() == NM_Standalone || InWorld->GetNetMode() == NM_ListenServer))
	{
		Replicator = Cast<AGameplayDebuggerReplicator>(CreateGameplayDebuggerForPlayerController(LocalPC));
		if (Replicator)
		{
			bIsRunAsDebugExec ? Replicator->OnActivationKeyPressed() : Replicator->ActivateFromCommand();
		}
		return Replicator != nullptr;
	}

	if (bEnableGDT && Replicator != nullptr)
	{
		Replicator->ActivateFromCommand();
		return true;
	}

#endif //ENABLED_GAMEPLAY_DEBUGGER

	return false;
}

/************************************************************************/
/* Module Helpers                                                       */
/************************************************************************/

bool FGameplayDebuggerModule::SpawnHelperActor(UWorld* InWorld)
{
#if ENABLED_GAMEPLAY_DEBUGGER
	FActorSpawnParameters SpawnInfo;
	SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnInfo.ObjectFlags = RF_Transient;
	SpawnInfo.Name = *FString::Printf(TEXT("GameplayDebuggerServerHelper"));
	const AGameplayDebuggerServerHelper* ServerHelper = InWorld->SpawnActor<AGameplayDebuggerServerHelper>(FVector::ZeroVector, FRotator::ZeroRotator, SpawnInfo);

	return ServerHelper != nullptr;
#else

	return false;
#endif
}

void FGameplayDebuggerModule::UpdateOrAddMapping(const struct FInputActionKeyMapping& Mapping, TArray<struct FInputActionKeyMapping>& ActionMappings)
{
	bool bUpdatedMapping = false;
	for (FInputActionKeyMapping& CurrentMapping : ActionMappings)
	{
		if (CurrentMapping.ActionName == Mapping.ActionName)
		{
			CurrentMapping = Mapping;
			bUpdatedMapping = true;
			break;
		}
	}
	if (!bUpdatedMapping)
	{
		ActionMappings.Add(Mapping);
	}
}

void FGameplayDebuggerModule::UpdateOrAddBinding(const struct FKeyBind& Binding, TArray<struct FKeyBind>& DebugExecBindings)
{
	bool bUpdatedBinding = false;
	for (FKeyBind& CurrentBinding : DebugExecBindings)
	{
		if (CurrentBinding.Command == Binding.Command)
		{
			CurrentBinding = Binding;
			bUpdatedBinding = true;
			break;
		}
	}
	if (!bUpdatedBinding)
	{
		DebugExecBindings.Add(Binding);
	}
}

void FGameplayDebuggerModule::HackyHelperSpawns()
{
	const TIndirectArray<FWorldContext>& WorldContexts = GEngine->GetWorldContexts();
	for (auto& WorldContext : WorldContexts)
	{
		UWorld* World = WorldContext.World();
		if (World && World->IsGameWorld())
		{
			AGameplayDebuggerServerHelper* ServerHelper = nullptr;
			for (TActorIterator<AGameplayDebuggerServerHelper> It(World); It; ++It)
			{
				ServerHelper = *It;
				if (ServerHelper)
				{
					//already with helper
					break;
				}
			}

			if (!ServerHelper)
			{
				UE_LOG(LogGameplayDebugger, Warning, TEXT("Spawning GameplayDebugger Helper acter a hacky way."));
				SpawnHelperActor(World);
			}
		}
	}
}

void FGameplayDebuggerModule::InitializeRuntimeServerActors()
{
	// InitializeRuntimeServerActors
	bool bDebuggerReplicatorActorPresent = GEngine->RuntimeServerActors.ContainsByPredicate(
		[](const FString& CurServerActor)
	{
		return CurServerActor == TEXT("/Script/GameplayDebugger.GameplayDebuggerServerHelper");
	});

	if (!bDebuggerReplicatorActorPresent)
	{
		UE_LOG(LogGameplayDebugger, Log, TEXT("GameplayDebuggerServerHelper not present in RuntimeServerActors - adding one"));

		GEngine->RuntimeServerActors.Add(TEXT("/Script/GameplayDebugger.GameplayDebuggerServerHelper"));
	}
}

void FGameplayDebuggerModule::CleanupRuntimeServerActors()
{
	bool bDebuggerReplicatorActorPresent = GEngine->RuntimeServerActors.ContainsByPredicate(
		[](const FString& CurServerActor)
	{
		return CurServerActor == TEXT("/Script/GameplayDebugger.GameplayDebuggerServerHelper");
	});

	if (bDebuggerReplicatorActorPresent)
	{
		UE_LOG(LogGameplayDebugger, Log, TEXT("GameplayDebuggerServerHelper is present in RuntimeServerActors - removng it"));

		GEngine->RuntimeServerActors.Remove(TEXT("/Script/GameplayDebugger.GameplayDebuggerServerHelper"));
	}
}

void FGameplayDebuggerModule::RegisterGameplayDebuggerBaseClasses()
{
	TArray<UClass *> ArrayOfClasses;
	UGameplayDebuggerModuleSettings* DebuggerSettings = UGameplayDebuggerModuleSettings::StaticClass()->GetDefaultObject<UGameplayDebuggerModuleSettings>();
	GetDerivedClasses(UGameplayDebuggerBaseObject::StaticClass(), ArrayOfClasses, /*bRecursive =*/ true);
	for (UClass* CurrentClass : ArrayOfClasses)
	{
		DebuggerSettings->RegisterClass(CurrentClass);
	}
}

void FGameplayDebuggerModule::SetupKeyboardBindings()
{
	const FInputActionKeyMapping& Mapping = UGameplayDebuggerModuleSettings::StaticClass()->GetDefaultObject<UGameplayDebuggerModuleSettings>()->ActivationAction;
	UpdateOrAddMapping(Mapping, UInputSettings::StaticClass()->GetDefaultObject<UInputSettings>()->ActionMappings);

	TArray<UObject *> Results;
	GetObjectsOfClass(UInputSettings::StaticClass(), Results, /*bIncludeDerivedClasses =*/ true, /*EObjectFlags AdditionalExcludeFlags =*/ RF_ClassDefaultObject);
	for (UObject* Obj : Results)
	{
		UInputSettings* InputSetting = Cast<UInputSettings>(Obj);
		UpdateOrAddMapping(Mapping, InputSetting->ActionMappings);
	}

	FKeyBind KeyBind;
	KeyBind.Command = Mapping.ActionName.ToString() + TEXT(" DebugExec");
	KeyBind.Key = Mapping.Key;
	KeyBind.Alt = Mapping.bAlt;
	KeyBind.Control = Mapping.bCtrl;
	KeyBind.Shift = Mapping.bShift;
	KeyBind.Cmd = Mapping.bCmd;
	UpdateOrAddBinding(KeyBind, UPlayerInput::StaticClass()->GetDefaultObject<UPlayerInput>()->DebugExecBindings);

	Results.Reset();
	GetObjectsOfClass(UPlayerInput::StaticClass(), Results, /*bIncludeDerivedClasses =*/ true, /*EObjectFlags AdditionalExcludeFlags =*/ RF_ClassDefaultObject);
	for (UObject* Obj : Results)
	{
		UPlayerInput* PlayerInput = Cast<UPlayerInput>(Obj);
		if (PlayerInput)
		{
			UpdateOrAddBinding(KeyBind, PlayerInput->DebugExecBindings);
			UpdateOrAddMapping(Mapping, PlayerInput->ActionMappings);
		}
	}
}

void FGameplayDebuggerModule::CleanupKeyboardBindings()
{
	const FInputActionKeyMapping& Mapping = UGameplayDebuggerModuleSettings::StaticClass()->GetDefaultObject<UGameplayDebuggerModuleSettings>()->ActivationAction;
	UInputSettings::StaticClass()->GetDefaultObject<UInputSettings>()->ActionMappings.Remove(Mapping);
}

void FGameplayDebuggerModule::SetupEditor()
{
#	if WITH_EDITOR
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyModule.RegisterCustomPropertyTypeLayout("InputConfig", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FGameplayDebuggerInputConfigDetails::MakeInstance));
	PropertyModule.RegisterCustomPropertyTypeLayout("ActionInputConfig", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FGameplayDebuggerActionInputConfigDetails::MakeInstance));
	PropertyModule.RegisterCustomPropertyTypeLayout("GameplayDebuggerCategorySettings", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FGameplayDebuggerCategorySettingsDetails::MakeInstance));

	if (GIsEditor)
	{
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(AssetRegistryConstants::ModuleName);
		AssetRegistryModule.Get().OnInMemoryAssetCreated().AddRaw(this, &FGameplayDebuggerModule::InMemoryAssetCreated);
		AssetRegistryModule.Get().OnInMemoryAssetDeleted().AddRaw(this, &FGameplayDebuggerModule::InMemoryAssetDeleted);
		AssetRegistryModule.Get().OnFilesLoaded().AddRaw(this, &FGameplayDebuggerModule::AssetRegistryLoadComplete);

		ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");
		if (SettingsModule != nullptr)
		{
			SettingsModule->RegisterSettings("Project", "Plugins", "GameplayDebugger",
				LOCTEXT("AIToolsSettingsName", "Gameplay Debugger"),
				LOCTEXT("AIToolsSettingsDescription", "General settings for UE4 AI Tools."),
				UGameplayDebuggerModuleSettings::StaticClass()->GetDefaultObject()
				);
		}
	}

	UGameplayDebuggerModuleSettings::StaticClass()->GetDefaultObject<UGameplayDebuggerModuleSettings>()->OnSettingChanged().AddRaw(this, &FGameplayDebuggerModule::HandleSettingChanged);
#	endif //WITH_EDITOR
}

void FGameplayDebuggerModule::CleanupEditor()
{
#if WITH_EDITOR
	if (GIsEditor)
	{
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(AssetRegistryConstants::ModuleName);
		AssetRegistryModule.Get().OnFilesLoaded().RemoveAll(this);
	}
	UGameplayDebuggerModuleSettings::StaticClass()->GetDefaultObject<UGameplayDebuggerModuleSettings>()->OnSettingChanged().RemoveAll(this);

	ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");
	if (SettingsModule != nullptr)
	{
		SettingsModule->UnregisterSettings("Project", "Plugins", "GameplayDebugger");
	}

	if (FModuleManager::Get().IsModuleLoaded("PropertyEditor"))
	{
		FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.UnregisterCustomPropertyTypeLayout("InputConfig");
		PropertyModule.UnregisterCustomPropertyTypeLayout("ActionInputConfig");
		PropertyModule.UnregisterCustomPropertyTypeLayout("GameplayDebuggerObjectSettings");
		PropertyModule.UnregisterCustomPropertyTypeLayout("GameplayDebuggerCategorySettings");
	}
#endif //WITH_EDITOR
}

#undef LOCTEXT_NAMESPACE

