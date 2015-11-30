// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "GameplayDebuggerPrivate.h"
#include "Misc/CoreMisc.h"
#include "GameplayDebuggingComponent.h"
#include "GameplayDebuggingHUDComponent.h"
#include "GameplayDebuggingReplicator.h"
#include "GameplayDebuggingControllerComponent.h"
#include "AISystem.h"
#include "GameplayDebuggerSettings.h"
#include "GameFramework/PlayerState.h"
#if WITH_EDITOR
#include "Editor/EditorEngine.h"
#include "ISettingsModule.h"
#include "LevelEditor.h"
#endif // WITH_EDITOR

#define LOCTEXT_NAMESPACE "FGameplayDebugger"
FGameplayDebuggerSettings GameplayDebuggerSettings(class AGameplayDebuggingReplicator* Replicator)
{
	uint32& Settings = UGameplayDebuggerSettings::StaticClass()->GetDefaultObject<UGameplayDebuggerSettings>()->GetSettings();
	return FGameplayDebuggerSettings(Replicator == NULL ? Settings : Replicator->DebuggerShowFlags);
}

class FGameplayDebugger : public FSelfRegisteringExec, public IGameplayDebugger
{
public:
	// Begin IModuleInterface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	// End IModuleInterface

	void WorldAdded(UWorld* InWorld);
	void WorldDestroyed(UWorld* InWorld);
#if WITH_EDITOR
	void OnLevelActorAdded(AActor* InActor);
	void OnLevelActorDeleted(AActor* InActor);
	TSharedRef<FExtender> OnExtendLevelEditorViewMenu(const TSharedRef<FUICommandList> CommandList);
	void CreateSnappingOptionsMenu(FMenuBuilder& Builder);
	void CreateSettingSubMenu(FMenuBuilder& Builder);
	void HandleSettingChanged(FName PropertyName);
#endif

	TArray<TWeakObjectPtr<AGameplayDebuggingReplicator> >& GetAllReplicators(UWorld* InWorld);
	void AddReplicator(UWorld* InWorld, AGameplayDebuggingReplicator* InReplicator);
	void RemoveReplicator(UWorld* InWorld, AGameplayDebuggingReplicator* InReplicator);

	// Begin FExec Interface
	virtual bool Exec(UWorld* Inworld, const TCHAR* Cmd, FOutputDevice& Ar) override;
	// End FExec Interface

private:
	virtual bool CreateGameplayDebuggerForPlayerController(APlayerController* PlayerController) override;
	virtual bool IsGameplayDebuggerActiveForPlayerController(APlayerController* PlayerController) override;

	bool DoesGameplayDebuggingReplicatorExistForPlayerController(APlayerController* PlayerController);

	TMap<TWeakObjectPtr<UWorld>, TArray<TWeakObjectPtr<AGameplayDebuggingReplicator> > > AllReplilcatorsPerWorlds;

#if WITH_EDITOR
	FLevelEditorModule::FLevelEditorMenuExtender ViewMenuExtender;
#endif
};

IMPLEMENT_MODULE(FGameplayDebugger, GameplayDebugger)

// This code will execute after your module is loaded into memory (but after global variables are initialized, of course.)
void FGameplayDebugger::StartupModule()
{ 
	//EMIT_CUSTOM_WARNING("/Engine/Source/Developer/GameplayDebugger module is deprecated and it's going to be removed with next UE4 version. Please use GameplayDebuggerPlugin instead.");
	//UE_LOG(LogGameplayDebugger, Warning, TEXT("/Engine/Source/Developer/GameplayDebugger module is deprecated and it's going to be removed with next UE4 version. Please use GameplayDebuggerPlugin instead."));

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (GEngine)
	{
		GEngine->OnWorldAdded().AddRaw(this, &FGameplayDebugger::WorldAdded);
		GEngine->OnWorldDestroyed().AddRaw(this, &FGameplayDebugger::WorldDestroyed);
#if WITH_EDITOR
		GEngine->OnLevelActorAdded().AddRaw(this, &FGameplayDebugger::OnLevelActorAdded);
		GEngine->OnLevelActorDeleted().AddRaw(this, &FGameplayDebugger::OnLevelActorDeleted);

		ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");
		if (SettingsModule != nullptr)
		{
			SettingsModule->RegisterSettings("Editor", "General", "GameplayDebugger",
				LOCTEXT("AIToolsSettingsName", "Gameplay Debugger"),
				LOCTEXT("AIToolsSettingsDescription", "General settings for UE4 AI Tools."),
				UGameplayDebuggerSettings::StaticClass()->GetDefaultObject()
				);
		}

		UGameplayDebuggerSettings::StaticClass()->GetDefaultObject<UGameplayDebuggerSettings>()->OnSettingChanged().AddRaw(this, &FGameplayDebugger::HandleSettingChanged);

#		if ADD_LEVEL_EDITOR_EXTENSIONS
		const bool bExtendViewportMenu = UGameplayDebuggerSettings::StaticClass()->GetDefaultObject<UGameplayDebuggerSettings>()->bExtendViewportMenu;
		if (!IsRunningCommandlet() && bExtendViewportMenu)
		{
			// Register the extension with the level editor
			{
				ViewMenuExtender = FLevelEditorModule::FLevelEditorMenuExtender::CreateRaw(this, &FGameplayDebugger::OnExtendLevelEditorViewMenu);
				FLevelEditorModule* LevelEditor = FModuleManager::LoadModulePtr<FLevelEditorModule>(TEXT("LevelEditor"));
				if (LevelEditor)
				{
					LevelEditor->GetAllLevelViewportOptionsMenuExtenders().Add(ViewMenuExtender);
				}
			}
		}
#		endif //ADD_LEVEL_EDITOR_EXTENSIONS
#	endif //WITH_EDITOR
	}
#endif
}

#if WITH_EDITOR
void FGameplayDebugger::HandleSettingChanged(FName PropertyName)
{
#if ADD_LEVEL_EDITOR_EXTENSIONS
	const bool bExtendViewportMenu = UGameplayDebuggerSettings::StaticClass()->GetDefaultObject<UGameplayDebuggerSettings>()->bExtendViewportMenu;
	if (PropertyName == TEXT("bExtendViewportMenu"))
	{
		if (bExtendViewportMenu)
		{
			// Register the extension with the level editor
			{
				ViewMenuExtender = FLevelEditorModule::FLevelEditorMenuExtender::CreateRaw(this, &FGameplayDebugger::OnExtendLevelEditorViewMenu);
				FLevelEditorModule* LevelEditor = FModuleManager::LoadModulePtr<FLevelEditorModule>(TEXT("LevelEditor"));
				if (LevelEditor)
				{
					LevelEditor->GetAllLevelViewportOptionsMenuExtenders().Add(ViewMenuExtender);
				}
			}
		}
		else
		{
			// Unregister the level editor extensions
			{
				FLevelEditorModule* LevelEditor = FModuleManager::LoadModulePtr<FLevelEditorModule>(TEXT("LevelEditor"));
				if (LevelEditor)
				{
					LevelEditor->GetAllLevelViewportOptionsMenuExtenders().Remove(ViewMenuExtender);
				}
			}
		}
	}
#endif
}
#endif

// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
// we call this function before unloading the module.
void FGameplayDebugger::ShutdownModule()
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (GEngine)
	{
		GEngine->OnWorldAdded().RemoveAll(this);
		GEngine->OnWorldDestroyed().RemoveAll(this);
#if WITH_EDITOR
		GEngine->OnLevelActorAdded().RemoveAll(this);
		GEngine->OnLevelActorDeleted().RemoveAll(this);

		UGameplayDebuggerSettings::StaticClass()->GetDefaultObject<UGameplayDebuggerSettings>()->OnSettingChanged().RemoveAll(this);

		ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");
		if (SettingsModule != nullptr)
		{
			SettingsModule->UnregisterSettings("Editor", "General", "GameplayDebugger");
		}

#	if ADD_LEVEL_EDITOR_EXTENSIONS
		if (UObjectInitialized() && !IsRunningCommandlet())
		{
			// Unregister the level editor extensions
			{
				FLevelEditorModule* LevelEditor = FModuleManager::LoadModulePtr<FLevelEditorModule>(TEXT("LevelEditor"));
				if (LevelEditor)
				{
					LevelEditor->GetAllLevelViewportOptionsMenuExtenders().Remove(ViewMenuExtender);
				}
			}
		}
#	endif //ADD_LEVEL_EDITOR_EXTENSIONS
#endif
	}
#endif
}

#if WITH_EDITOR

void FGameplayDebugger::CreateSettingSubMenu(FMenuBuilder& Builder)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	Builder.AddMenuEntry(
		LOCTEXT("Test_GameplayDebugger_Menu", "Test Gameplay Debugger Option"),
		LOCTEXT("Test_GameplayDebugger_Menu_Tooltip", "If Enabled, actors will snap to the nearest location on the constraint plane (NOTE: Only works correctly in perspective views right now!)"),
		FSlateIcon(),
		FUIAction(
		FExecuteAction()/*FExecuteAction::CreateRaw(PlanarPolicy.Get(), &FPlanarConstraintSnapPolicy::ToggleEnabled)*/,
		FCanExecuteAction(),
		FIsActionChecked()/*FIsActionChecked::CreateRaw(PlanarPolicy.Get(), &FPlanarConstraintSnapPolicy::IsEnabled)*/
		),
		NAME_None,
		EUserInterfaceActionType::Button,
		NAME_None);
#endif
}

void FGameplayDebugger::CreateSnappingOptionsMenu(FMenuBuilder& Builder)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (GCurrentLevelEditingViewportClient && GCurrentLevelEditingViewportClient->EngineShowFlags.DebugAI && GCurrentLevelEditingViewportClient->IsSimulateInEditorViewport())
	{
		Builder.AddMenuSeparator();
		Builder.AddSubMenu(
			LOCTEXT("Test_GameplayDebugger_Menu", "Gameplay Debugger"),
			LOCTEXT("Test_GameplayDebugger_Menu_Tooltip", "Quick setting for Gameplay Debugger tool in selected view"),
			FNewMenuDelegate::CreateRaw(this, &FGameplayDebugger::CreateSettingSubMenu)
			);
	}
#endif
}

TSharedRef<FExtender> FGameplayDebugger::OnExtendLevelEditorViewMenu(const TSharedRef<FUICommandList> CommandList)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST) && WITH_EDITOR
	TSharedRef<FExtender> Extender(new FExtender());
	if (GEditor && GEditor->bIsSimulatingInEditor)
	{

		Extender->AddMenuExtension(
			"LevelViewportLayouts",
			EExtensionHook::After,
			NULL,
			FMenuExtensionDelegate::CreateRaw(this, &FGameplayDebugger::CreateSnappingOptionsMenu));

	}
#endif
	return Extender;
}
#endif

bool FGameplayDebugger::DoesGameplayDebuggingReplicatorExistForPlayerController(APlayerController* PlayerController)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (PlayerController == NULL)
	{
		return false;
	}

	UWorld* World = PlayerController->GetWorld();
	if (World == NULL)
	{
		return false;
	}

	for (auto It = GetAllReplicators(World).CreateConstIterator(); It; ++It)
	{
		TWeakObjectPtr<AGameplayDebuggingReplicator> Replicator = *It;
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

bool FGameplayDebugger::CreateGameplayDebuggerForPlayerController(APlayerController* PlayerController)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (PlayerController == NULL)
	{
		return false;
	}

	bool bIsServer = PlayerController->GetNetMode() < ENetMode::NM_Client; // (Only create on some sort of server)
	if (!bIsServer)
	{
		return false;
	}

	UWorld* World = PlayerController->GetWorld();
	if (World == NULL)
	{
		return false;
	}

	if (DoesGameplayDebuggingReplicatorExistForPlayerController(PlayerController))
	{
		// No need to create one if we already have one.
		return false;
	}

	FActorSpawnParameters SpawnInfo;
	SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnInfo.ObjectFlags = RF_Transient;
	SpawnInfo.Name = *FString::Printf(TEXT("GameplayDebuggingReplicator_%s"), *PlayerController->GetName());
	AGameplayDebuggingReplicator* DestActor = World->SpawnActor<AGameplayDebuggingReplicator>(FVector::ZeroVector, FRotator::ZeroRotator, SpawnInfo);
	if (DestActor != NULL)
	{
		DestActor->SetLocalPlayerOwner(PlayerController);
		DestActor->SetReplicates(true);
		DestActor->SetAsGlobalInWorld(false);
#if WITH_EDITOR
		UEditorEngine* EEngine = Cast<UEditorEngine>(GEngine);
		if (EEngine && EEngine->bIsSimulatingInEditor)
		{
			DestActor->CreateTool();
			DestActor->EnableTool();
		}
#endif
		AddReplicator(World, DestActor);
		return true;
	}
#endif
	
	return false;
}

bool FGameplayDebugger::IsGameplayDebuggerActiveForPlayerController(APlayerController* PlayerController)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (PlayerController == NULL)
	{
		return false;
	}

	UWorld* World = PlayerController->GetWorld();
	if (World == NULL)
	{
		return false;
	}

	for (auto It = GetAllReplicators(World).CreateConstIterator(); It; ++It)
	{
		TWeakObjectPtr<AGameplayDebuggingReplicator> Replicator = *It;
		if (Replicator.IsValid())
		{
			if (Replicator->GetLocalPlayerOwner() == PlayerController)
			{
				return Replicator->IsDrawEnabled();
			}
		}
	}
#endif

	return false;
}

TArray<TWeakObjectPtr<AGameplayDebuggingReplicator> >& FGameplayDebugger::GetAllReplicators(UWorld* InWorld)
{
	return AllReplilcatorsPerWorlds.FindOrAdd(InWorld);
}

void FGameplayDebugger::AddReplicator(UWorld* InWorld, AGameplayDebuggingReplicator* InReplicator)
{
	GetAllReplicators(InWorld).Add(InReplicator);
}

void FGameplayDebugger::RemoveReplicator(UWorld* InWorld, AGameplayDebuggingReplicator* InReplicator)
{
	GetAllReplicators(InWorld).RemoveSwap(InReplicator);
}

void FGameplayDebugger::WorldAdded(UWorld* InWorld)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	bool bIsServer = InWorld && InWorld->GetNetMode() < ENetMode::NM_Client; // (Only server code)
	if (!bIsServer)
	{
		return;
	}

	if (InWorld == NULL || InWorld->IsPendingKill() || InWorld->IsGameWorld() == false)
	{
		return;
	}

	for (auto It = GetAllReplicators(InWorld).CreateConstIterator(); It; ++It)
	{
		TWeakObjectPtr<AGameplayDebuggingReplicator> Replicator = *It;
		if (Replicator.IsValid() && Replicator->IsGlobalInWorld())
		{
			// Ok, we have global replicator on level
			return;
		}
	}

	// create global replicator on level
	FActorSpawnParameters SpawnInfo;
	SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnInfo.ObjectFlags = RF_Transient;
	SpawnInfo.Name = *FString::Printf(TEXT("GameplayDebuggingReplicator_Global"));
	AGameplayDebuggingReplicator* DestActor = InWorld->SpawnActor<AGameplayDebuggingReplicator>(FVector::ZeroVector, FRotator::ZeroRotator, SpawnInfo);
	if (DestActor != NULL)
	{
		DestActor->SetLocalPlayerOwner(NULL);
		DestActor->SetReplicates(false);
		DestActor->SetActorTickEnabled(true);
		DestActor->SetAsGlobalInWorld(true);
		AddReplicator(InWorld, DestActor);
	}
#endif //!(UE_BUILD_SHIPPING || UE_BUILD_TEST)
}

void FGameplayDebugger::WorldDestroyed(UWorld* InWorld)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	bool bIsServer = InWorld && InWorld->GetNetMode() < ENetMode::NM_Client; // (Only work on  server)
	if (!bIsServer)
	{
		return;
	}

	// remove global replicator from level
	AllReplilcatorsPerWorlds.Remove(InWorld);
#endif //!(UE_BUILD_SHIPPING || UE_BUILD_TEST)
}

#if WITH_EDITOR
void FGameplayDebugger::OnLevelActorAdded(AActor* InActor)
{
	// This function doesn't help much, because it's only called in EDITOR!
	// We need a function that is called in the game!  So instead of creating it automatically, I'm leaving it
	// to be created explicitly by any player controller that needs to create it.
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	APlayerController* PC = Cast<APlayerController>(InActor);
	if (PC && PC->GetNetMode() < ENetMode::NM_Client)
	{
		UWorld* PCWorld = PC->GetWorld();
		if (PCWorld && PCWorld->IsGameWorld())
		{
			CreateGameplayDebuggerForPlayerController(PC);
		}
	}
#endif
}

void FGameplayDebugger::OnLevelActorDeleted(AActor* InActor)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	APlayerController* PC = Cast<APlayerController>(InActor);
	if (!PC)
	{
		return;
	}

	UWorld* World = PC->GetWorld();
	if (!World)
	{
		return;
	}

	for (auto It = GetAllReplicators(World).CreateConstIterator(); It; ++It)
	{
		TWeakObjectPtr<AGameplayDebuggingReplicator> Replicator = *It;
		if (Replicator != NULL)
		{
			if (Replicator->GetLocalPlayerOwner() == PC)
			{
				RemoveReplicator(World, Replicator.Get());
				Replicator->MarkPendingKill();
				//World->DestroyActor(Replicator.Get());
			}
		}
	}
#endif
}
#endif //WITH_EDITOR

bool FGameplayDebugger::Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	bool bHandled = false;

	if (FParse::Command(&Cmd, TEXT("RunEQS")) && InWorld)
	{
		APlayerController* MyPC = InWorld->GetGameInstance() ? InWorld->GetGameInstance()->GetFirstLocalPlayerController() : nullptr;
		UAISystem* AISys = UAISystem::GetCurrent(*InWorld);

		UEnvQueryManager* EQS = AISys ? AISys->GetEnvironmentQueryManager() : NULL;
		if (MyPC && EQS)
		{
			AGameplayDebuggingReplicator* DebuggingReplicator = NULL;
			for (TActorIterator<AGameplayDebuggingReplicator> It(InWorld); It; ++It)
			{
				AGameplayDebuggingReplicator* A = *It;
				if (!A->IsPendingKill())
				{
					DebuggingReplicator = A;
					if (!DebuggingReplicator->IsGlobalInWorld() && DebuggingReplicator->GetLocalPlayerOwner() == MyPC)
					{
						break;
					}
				}
			}

			UObject* Target = DebuggingReplicator != NULL ? DebuggingReplicator->GetSelectedActorToDebug() : NULL;
			FString QueryName = FParse::Token(Cmd, 0);
			if (Target)
			{
				AISys->RunEQS(QueryName, Target);
			}
			else
			{
				MyPC->ClientMessage(TEXT("No debugging target to run EQS"));
			}
		}

		return true;
	}


	if (FParse::Command(&Cmd, TEXT("EnableGDT")) == false)
	{
		return false;
	}

	FString UniquePlayerId = FParse::Token(Cmd, 0);
	APlayerController* LocalPC = NULL;
	UWorld* MyWorld = InWorld;

	if (MyWorld == nullptr)
	{
		if (UniquePlayerId.Len() > 0)
		{
			// let's find correct world based on Player Id
			const TIndirectArray<FWorldContext> WorldContexts = GEngine->GetWorldContexts();
			for (auto& Context : WorldContexts)
			{
				if (Context.WorldType != EWorldType::Game && Context.WorldType != EWorldType::PIE)
				{
					continue;
				}

				UWorld *CurrentWorld = Context.World();
				for (FConstPlayerControllerIterator Iterator = CurrentWorld->GetPlayerControllerIterator(); Iterator; ++Iterator)
				{
					APlayerController* PC = *Iterator;
					if (PC && PC->PlayerState->UniqueId.ToString() == UniquePlayerId)
					{
						LocalPC = PC;
						MyWorld = PC->GetWorld();
						break;
					}
				}

				if (LocalPC && MyWorld)
				{
					break;
				}
			}
		}
	}

	if (MyWorld == nullptr)
	{
		return false;
	}

	if (LocalPC == nullptr)
	{
		if (UniquePlayerId.Len() > 0)
		{
			for (FConstPlayerControllerIterator Iterator = MyWorld->GetPlayerControllerIterator(); Iterator; ++Iterator)
			{
				APlayerController* PlayerController = *Iterator;
				UE_LOG(LogGameplayDebugger, Log, TEXT("- Client: %s"), *PlayerController->PlayerState->UniqueId.ToString());
				if (PlayerController && PlayerController->PlayerState->UniqueId.ToString() == UniquePlayerId)
				{
					LocalPC = PlayerController;
					break;
				}
			}
		}

		if (!LocalPC && MyWorld->GetNetMode() != NM_DedicatedServer)
		{
			LocalPC = MyWorld->GetGameInstance() ? MyWorld->GetGameInstance()->GetFirstLocalPlayerController() : nullptr;
		}
	}

	if (LocalPC == nullptr)
	{
		return false;
	}

	if (MyWorld->GetNetMode() == NM_Client)
	{
		AGameplayDebuggingReplicator* Replicator = NULL;
		for (TActorIterator<AGameplayDebuggingReplicator> It(MyWorld); It; ++It)
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
			Replicator = NULL;
		}

		if (!Replicator)
		{
			LocalPC->ClientMessage(TEXT("Enabling GameplayDebugger on server, please wait for replicated data..."));
			if (LocalPC->PlayerState)
			{
				const FString ServerCheatString = FString::Printf(TEXT("cheat EnableGDT %s"), *LocalPC->PlayerState->UniqueId.ToString());
				UE_LOG(LogGameplayDebugger, Warning, TEXT("Sending to Server: %s"), *ServerCheatString);
				LocalPC->ConsoleCommand(*ServerCheatString);
				bHandled = true;
			}
		}
		else
		{
			if (Replicator->IsToolCreated() == false)
			{
				Replicator->CreateTool();
			}
			Replicator->EnableTool();
			bHandled = true;
		}
	}
	else
	{
		UE_LOG(LogGameplayDebugger, Warning, TEXT("Got from client: EnableGDT %s"), *UniquePlayerId);
		{
			AGameplayDebuggingReplicator* Replicator = NULL;
			for (TActorIterator<AGameplayDebuggingReplicator> It(MyWorld); It; ++It)
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
				Replicator = NULL;
			}

			if (!Replicator)
			{
				CreateGameplayDebuggerForPlayerController(LocalPC);
				for (TActorIterator<AGameplayDebuggingReplicator> It(MyWorld); It; ++It)
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
					Replicator = NULL;
				}
			}

			if (MyWorld->GetNetMode() != NM_DedicatedServer)
			{
				if (Replicator && !Replicator->IsToolCreated())
				{
					Replicator->CreateTool();
					Replicator->EnableTool();
					bHandled = true;
				}
			}
			else
			{
				if (Replicator)
				{
					Replicator->ClientAutoActivate();
					bHandled = true;
				}
			}
		}
	}

	return bHandled;
#else
	return false;
#endif //!(UE_BUILD_SHIPPING || UE_BUILD_TEST)
}

#undef LOCTEXT_NAMESPACE
