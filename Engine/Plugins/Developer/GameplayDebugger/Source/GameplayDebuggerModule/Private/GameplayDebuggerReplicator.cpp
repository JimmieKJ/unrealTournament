// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "GameplayDebuggerPluginPrivatePCH.h"
#include "GameplayDebuggerModuleSettings.h"
#include "DebugRenderSceneProxy.h"
#include "ModuleManager.h"
#include "Engine/Selection.h"
#include "Engine/GameInstance.h"
#include "Engine/DebugCameraController.h"
#include "Debug/DebugDrawService.h"
#include "GameFramework/HUD.h"
#include "GameplayDebuggerReplicator.h"
#include "BehaviorTreeDelegates.h"
#if WITH_EDITOR
#include "Editor.h"
#include "Editor/EditorEngine.h"
#include "Editor/BehaviorTreeEditor/Public/BehaviorTreeEditorModule.h"
#include "LevelEditorViewport.h"
#endif // WITH_EDITOR
#include "UnrealNetwork.h"

namespace GameplayDebuggerHelpers
{
	const float ActivationKeyTimePch = 0.25;
}

AGameplayDebuggerReplicator::AGameplayDebuggerReplicator(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, PlayerControllersUpdateDelay(0)
	, ActivationKeyTime(0)
{
#if ENABLED_GAMEPLAY_DEBUGGER
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		ConstructorHelpers::FObjectFinderOptional<UTexture2D> RedIcon;
		ConstructorHelpers::FObjectFinderOptional<UTexture2D> GreenIcon;

		// both icons are needed to debug AI and we hold static initialization for cooker
		FConstructorStatics()
			: RedIcon(TEXT("/Engine/EngineResources/AICON-Red.AICON-Red"))
			, GreenIcon(TEXT("/Engine/EngineResources/AICON-Green.AICON-Green"))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

	DefaultTexture_Red = ConstructorStatics.RedIcon.Get();
	DefaultTexture_Green = ConstructorStatics.GreenIcon.Get();

	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bAllowTickOnDedicatedServer = true;
	PrimaryActorTick.bTickEvenWhenPaused = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	USceneComponent* SceneComponent = CreateDefaultSubobject<USceneComponent>(TEXT("SceneComponent"));
	RootComponent = SceneComponent;

	CreateDefaultSubobject<UGameplayDebuggerDrawComponent>(TEXT("GameplayDebuggerRenderingComponent"));

#if WITH_EDITOR
	SetIsTemporarilyHiddenInEditor(true);
#endif

#if WITH_EDITORONLY_DATA
	bHiddenEdLevel = true;
	bHiddenEdLayer = true;
	bHiddenEd = true;
	bEditable = false;
#endif

	MaxMenuWidth = 0;
#endif //ENABLED_GAMEPLAY_DEBUGGER
}

void AGameplayDebuggerReplicator::GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

#if ENABLED_GAMEPLAY_DEBUGGER
	DOREPLIFETIME_CONDITION(AGameplayDebuggerReplicator, LocalPlayerOwner, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(AGameplayDebuggerReplicator, LastSelectedActorToDebug, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(AGameplayDebuggerReplicator, ReplicatedObjects, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(AGameplayDebuggerReplicator, Categories, COND_OwnerOnly);
#endif
}

bool AGameplayDebuggerReplicator::ReplicateSubobjects(class UActorChannel *Channel, class FOutBunch *Bunch, FReplicationFlags *RepFlags)
{
	bool bWroteSomething = Super::ReplicateSubobjects(Channel, Bunch, RepFlags);
#if ENABLED_GAMEPLAY_DEBUGGER

	for (UGameplayDebuggerBaseObject* MyObject : ReplicatedObjects)
	{
		if (MyObject)
		{
			bWroteSomething |= Channel->ReplicateSubobject(MyObject, *Bunch, *RepFlags);
		}
	}
#endif
	return bWroteSomething;
}


bool AGameplayDebuggerReplicator::IsNetRelevantFor(const AActor* RealViewer, const AActor* ViewTarget, const FVector& SrcLocation) const
{
	return LocalPlayerOwner == RealViewer;
}

void AGameplayDebuggerReplicator::OnRep_ReplicatedCategories()
{
#if ENABLED_GAMEPLAY_DEBUGGER
	MarkComponentsRenderStateDirty();
#endif
}

void AGameplayDebuggerReplicator::BindKeyboardInput(class UInputComponent*& InInputComponent)
{
#if ENABLED_GAMEPLAY_DEBUGGER
	if (!InInputComponent)
	{
		InInputComponent = NewObject<UInputComponent>(this, TEXT("AIDebugViewInInputComponent0"));
		InInputComponent->RegisterComponent();

		UGameplayDebuggerModuleSettings* Settings = UGameplayDebuggerModuleSettings::StaticClass()->GetDefaultObject<UGameplayDebuggerModuleSettings>();
		FInputConfiguration& InputConfiguration = Settings->GetInputConfiguration();
		InInputComponent->BindKey(InputConfiguration.SelectPlayer, IE_Pressed, this, &AGameplayDebuggerReplicator::OnSelectPlayer);
		InInputComponent->BindKey(InputConfiguration.ZeroCategory, IE_Pressed, this, &AGameplayDebuggerReplicator::OnNumPadZero);
		InInputComponent->BindKey(InputConfiguration.OneCategory, IE_Pressed, this, &AGameplayDebuggerReplicator::OnNumPadOne);
		InInputComponent->BindKey(InputConfiguration.TwoCategory, IE_Pressed, this, &AGameplayDebuggerReplicator::OnNumPadTwo);
		InInputComponent->BindKey(InputConfiguration.ThreeCategory, IE_Pressed, this, &AGameplayDebuggerReplicator::OnNumPadThree);
		InInputComponent->BindKey(InputConfiguration.FourCategory, IE_Pressed, this, &AGameplayDebuggerReplicator::OnNumPadFour);
		InInputComponent->BindKey(InputConfiguration.FiveCategory, IE_Pressed, this, &AGameplayDebuggerReplicator::OnNumPadFive);
		InInputComponent->BindKey(InputConfiguration.SixCategory, IE_Pressed, this, &AGameplayDebuggerReplicator::OnNumPadSix);
		InInputComponent->BindKey(InputConfiguration.SevenCategory, IE_Pressed, this, &AGameplayDebuggerReplicator::OnNumPadSeven);
		InInputComponent->BindKey(InputConfiguration.EightCategory, IE_Pressed, this, &AGameplayDebuggerReplicator::OnNumPadEight);
		InInputComponent->BindKey(InputConfiguration.NineCategory, IE_Pressed, this, &AGameplayDebuggerReplicator::OnNumPadNine);

		InInputComponent->BindKey(InputConfiguration.NextMenuLine, IE_Released, this, &AGameplayDebuggerReplicator::OnNextMenuLine);
		InInputComponent->BindKey(InputConfiguration.PreviousMenuLine, IE_Released, this, &AGameplayDebuggerReplicator::OnPreviousMenuLine);
		InInputComponent->BindKey(InputConfiguration.DebugCamera, IE_Released, this, &AGameplayDebuggerReplicator::ToggleDebugCamera);
		InInputComponent->BindKey(InputConfiguration.OnScreenDebugMessages, IE_Released, this, &AGameplayDebuggerReplicator::ToggleOnScreenDebugMessages);
		InInputComponent->BindKey(InputConfiguration.GameHUD, IE_Released, this, &AGameplayDebuggerReplicator::ToggleGameHUD);

		for (UGameplayDebuggerBaseObject* Obj : ReplicatedObjects)
		{
			if (Obj)
			{
				Obj->BindKeyboardImput(InInputComponent);
			}
		} 

		if (LocalPlayerOwner)
		{
			LocalPlayerOwner->PushInputComponent(InInputComponent);
		}
	}
#endif //ENABLED_GAMEPLAY_DEBUGGER 
} 

void AGameplayDebuggerReplicator::ToggleCategory(int32 CategoryIndex)
{
#if ENABLED_GAMEPLAY_DEBUGGER
	const int32 FinalIndex = CurrentMenuLine * 10 + CategoryIndex;
	if (Categories.IsValidIndex(FinalIndex))
	{
		ServerToggleCategory(Categories[FinalIndex].CategoryName); 
	}

	MarkComponentsRenderStateDirty();
#endif
} 

void AGameplayDebuggerReplicator::OnNumPadZero() { ToggleCategory(0); }
void AGameplayDebuggerReplicator::OnNumPadOne()  { ToggleCategory(1); }
void AGameplayDebuggerReplicator::OnNumPadTwo() { ToggleCategory(2); }
void AGameplayDebuggerReplicator::OnNumPadThree() { ToggleCategory(3); }
void AGameplayDebuggerReplicator::OnNumPadFour() { ToggleCategory(4); }
void AGameplayDebuggerReplicator::OnNumPadFive() { ToggleCategory(5); }
void AGameplayDebuggerReplicator::OnNumPadSix() { ToggleCategory(6); }
void AGameplayDebuggerReplicator::OnNumPadSeven() { ToggleCategory(7); }
void AGameplayDebuggerReplicator::OnNumPadEight() { ToggleCategory(8); }
void AGameplayDebuggerReplicator::OnNumPadNine() { ToggleCategory(9); }

void AGameplayDebuggerReplicator::ToggleOnScreenDebugMessages()
{
#if ENABLED_GAMEPLAY_DEBUGGER
	if (!IsActorTickEnabled() || LocalPlayerOwner == nullptr)
	{
		return;
	}

	if (GEngine)
	{
		GEngine->bEnableOnScreenDebugMessages = !GEngine->bEnableOnScreenDebugMessages;
	}
#endif
}

void AGameplayDebuggerReplicator::ToggleGameHUD()
{
#if ENABLED_GAMEPLAY_DEBUGGER
	if (!IsActorTickEnabled() || LocalPlayerOwner == nullptr)
	{
		return;
	}

	if (AHUD* const GameHUD = LocalPlayerOwner->GetHUD())
	{
		GameHUD->bShowHUD = !GameHUD->bShowHUD;
	}
#endif
}

void AGameplayDebuggerReplicator::ToggleDebugCamera()
{
#if ENABLED_GAMEPLAY_DEBUGGER
	if (LocalPlayerOwner == nullptr)
	{
		return;
	}

	if (DebugCameraController.IsValid() == false)
	{
		{
			FActorSpawnParameters SpawnInfo;
			SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			SpawnInfo.Owner = LocalPlayerOwner->GetWorldSettings();
			SpawnInfo.Instigator = LocalPlayerOwner->Instigator;
			DebugCameraController = GetWorld()->SpawnActor<ADebugCameraController>(SpawnInfo);
		}

		if (DebugCameraController.IsValid())
		{
			// set up new controller
			DebugCameraController->OnActivate(LocalPlayerOwner);

			// then switch to it
			LocalPlayerOwner->Player->SwitchController(DebugCameraController.Get());

			FActorSpawnParameters SpawnInfo;
			SpawnInfo.Owner = LocalPlayerOwner;
			SpawnInfo.Instigator = LocalPlayerOwner->Instigator;
			SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			SpawnInfo.ObjectFlags |= RF_Transient;	// We never want these to save into a map
			AGameplayDebuggerHUD* ProxyHUD = GetWorld()->SpawnActor<AGameplayDebuggerHUD>(SpawnInfo);
			ProxyHUD->RedirectedHUD = LocalPlayerOwner->MyHUD;
			DebugCameraController->MyHUD = ProxyHUD;
			BindKeyboardInput(DebugCameraInputComponent);

			const FInputActionKeyMapping& Mapping = UGameplayDebuggerModuleSettings::StaticClass()->GetDefaultObject<UGameplayDebuggerModuleSettings>()->ActivationAction;

			FInputActionBinding& ActivationKeyPressed = DebugCameraInputComponent->BindAction(*Mapping.ActionName.ToString(), IE_Pressed, this, &AGameplayDebuggerReplicator::OnActivationKeyPressed);
			ActivationKeyPressed.bConsumeInput = true;

			FInputActionBinding& ActivationKeyReleased = DebugCameraInputComponent->BindAction(*Mapping.ActionName.ToString(), IE_Released, this, &AGameplayDebuggerReplicator::OnActivationKeyReleased);
			ActivationKeyReleased.bConsumeInput = true;


			DebugCameraController->ChangeState(NAME_Default);
			DebugCameraController->ChangeState(NAME_Spectating);

			if (LocalPlayerOwner)
			{
				LocalPlayerOwner->PopInputComponent(InputComponent);
			}
			DebugCameraController->PushInputComponent(DebugCameraInputComponent);
			InputComponent = nullptr;
		}
	}
	else
	{
		DebugCameraController->PopInputComponent(DebugCameraInputComponent);
		DebugCameraInputComponent = nullptr;
		DebugCameraController->OriginalPlayer->SwitchController(DebugCameraController->OriginalControllerRef);
		DebugCameraController->OnDeactivate(DebugCameraController->OriginalControllerRef);
		GetWorld()->DestroyActor(DebugCameraController.Get(), false, false);
		DebugCameraController = nullptr;

		if (InputComponent == nullptr)
		{
			BindKeyboardInput(InputComponent);
			if (LocalPlayerOwner)
			{
				LocalPlayerOwner->PushInputComponent(InputComponent);
			}
		}

	}

#endif //ENABLED_GAMEPLAY_DEBUGGER
}

void AGameplayDebuggerReplicator::OnNextMenuLine()
{
#if ENABLED_GAMEPLAY_DEBUGGER
	const int32 NumberOfLines = Categories.Num() / 10 + 1;
	CurrentMenuLine = FMath::Clamp(CurrentMenuLine+1, 0, NumberOfLines-1);
#endif
}

void AGameplayDebuggerReplicator::OnPreviousMenuLine()
{
#if ENABLED_GAMEPLAY_DEBUGGER
	const int32 NumberOfLines = Categories.Num() / 10 + 1;
	CurrentMenuLine = FMath::Clamp(CurrentMenuLine-1, 0, NumberOfLines - 1);
#endif
}

bool AGameplayDebuggerReplicator::ServerToggleCategory_Validate(const FString& CategoryName)
{
#if ENABLED_GAMEPLAY_DEBUGGER
	return true;
#endif
	return false;
}

void AGameplayDebuggerReplicator::ServerToggleCategory_Implementation(const FString& CategoryName)
{
#if ENABLED_GAMEPLAY_DEBUGGER
	for (FGameplayDebuggerCategorySettings& CategorySettings : Categories)
	{
		if (CategorySettings.CategoryName == CategoryName)
		{
			CategorySettings.bPIE = !CategorySettings.bPIE;
		}
	}
#endif
}

bool AGameplayDebuggerReplicator::IsCategoryEnabled(const FString& CategoryName)
{
#if ENABLED_GAMEPLAY_DEBUGGER
	for (FGameplayDebuggerCategorySettings& CategorySettings : Categories)
	{
		if (CategorySettings.CategoryName == CategoryName)
		{
			return CategorySettings.bPIE;
		}
	}
#endif
	return false;
}

void AGameplayDebuggerReplicator::BeginPlay()
{
	Super::BeginPlay();

#if ENABLED_GAMEPLAY_DEBUGGER

	UGameplayDebuggerModuleSettings* Settings = UGameplayDebuggerModuleSettings::StaticClass()->GetDefaultObject<UGameplayDebuggerModuleSettings>();
	if (Role == ROLE_Authority)
	{
		Settings->LoadAnyMissingClasses();

		TArray<UClass *> Results = Settings->GetAllGameplayDebuggerClasses();
		for (UClass* Class : Results)
		{
			UGameplayDebuggerBaseObject* BaseObject = NewObject<UGameplayDebuggerBaseObject>(this, Class, NAME_None, RF_Transient);
			ReplicatedObjects.AddUnique(BaseObject);
		}
	}

	// start with settings categories
	Categories = Settings->GetCategories();

	IConsoleVariable* cvarHighlightSelectedActor = IConsoleManager::Get().FindConsoleVariable(TEXT("ai.gd.HighlightSelectedActor"));
	if (cvarHighlightSelectedActor)
	{
		cvarHighlightSelectedActor->Set(Settings->bHighlightSelectedActor);
	}

	const FInputActionKeyMapping& Mapping = Settings->ActivationAction;
	ActivationKeyDisplayName = Mapping.Key.GetDisplayName().ToString();
	ActivationKeyName = Mapping.Key.GetFName().ToString();

	if (GetNetMode() != ENetMode::NM_DedicatedServer && LocalPlayerOwner && LocalPlayerOwner->InputComponent)
	{
		FInputActionBinding& ActivationKeyPressed = LocalPlayerOwner->InputComponent->BindAction(Mapping.ActionName, IE_Pressed, this, &AGameplayDebuggerReplicator::OnActivationKeyPressed);
		ActivationKeyPressed.bConsumeInput = false;

		FInputActionBinding& ActivationKeyReleased = LocalPlayerOwner->InputComponent->BindAction(Mapping.ActionName, IE_Released, this, &AGameplayDebuggerReplicator::OnActivationKeyReleased);
		ActivationKeyReleased.bConsumeInput = false;

		// register 'Game' show flag for Game or PIE, Simulate doesn't use it
		UDebugDrawService::Register(TEXT("Game"), FDebugDrawDelegate::CreateUObject(this, &AGameplayDebuggerReplicator::DrawDebugDataDelegate));
		if (GIsEditor)
		{
			// register 'DebugAI' show flag for Simulate, Game or PIE don't use DebugAI show flag but 'Game'
			UDebugDrawService::Register(TEXT("DebugAI"), FDebugDrawDelegate::CreateUObject(this, &AGameplayDebuggerReplicator::OnDebugAIDelegate));
		}
	}
#endif //ENABLED_GAMEPLAY_DEBUGGER
}

void AGameplayDebuggerReplicator::SetLocalPlayerOwner(APlayerController* PC)
{
#if ENABLED_GAMEPLAY_DEBUGGER
	LocalPlayerOwner = PC;
	if (GetNetMode() != ENetMode::NM_DedicatedServer && LocalPlayerOwner  && LocalPlayerOwner->InputComponent && ReplicatedObjects.Num() > 0)
	{
		const FInputActionKeyMapping& Mapping = UGameplayDebuggerModuleSettings::StaticClass()->GetDefaultObject<UGameplayDebuggerModuleSettings>()->ActivationAction;

		FInputActionBinding& ActivationKeyPressed = LocalPlayerOwner->InputComponent->BindAction(Mapping.ActionName, IE_Pressed, this, &AGameplayDebuggerReplicator::OnActivationKeyPressed);
		ActivationKeyPressed.bConsumeInput = false;

		FInputActionBinding& ActivationKeyReleased = LocalPlayerOwner->InputComponent->BindAction(Mapping.ActionName, IE_Released, this, &AGameplayDebuggerReplicator::OnActivationKeyReleased);
		ActivationKeyReleased.bConsumeInput = false;

		// register 'Game' show flag for Game or PIE, Simulate doesn't use it
		UDebugDrawService::Register(TEXT("Game"), FDebugDrawDelegate::CreateUObject(this, &AGameplayDebuggerReplicator::DrawDebugDataDelegate));
		if (GIsEditor)
		{
			// register 'DebugAI' show flag for Simulate, Game or PIE don't use DebugAI show flag but 'Game'
			UDebugDrawService::Register(TEXT("DebugAI"), FDebugDrawDelegate::CreateUObject(this, &AGameplayDebuggerReplicator::OnDebugAIDelegate));
		}
	}
#endif
}

bool AGameplayDebuggerReplicator::ClientActivateGameplayDebugger_Validate(bool bActivate)
{
#if ENABLED_GAMEPLAY_DEBUGGER
	return true;
#endif
	return false;
}

void AGameplayDebuggerReplicator::ClientActivateGameplayDebugger_Implementation(bool bActivate)
{
#if ENABLED_GAMEPLAY_DEBUGGER
	SetActorTickEnabled(bActivate);
	MarkComponentsRenderStateDirty();
#endif
}

bool AGameplayDebuggerReplicator::ServerActivateGameplayDebugger_Validate(bool bActivate)
{
#if ENABLED_GAMEPLAY_DEBUGGER
	return true;
#endif
	return false;
}
void AGameplayDebuggerReplicator::ServerActivateGameplayDebugger_Implementation(bool bActivate)
{
#if ENABLED_GAMEPLAY_DEBUGGER
	bReplicates = !bActivate;
	SetRemoteRoleForBackwardsCompat(ROLE_SimulatedProxy);
	SetReplicates(bActivate);
	SetActorTickEnabled(bActivate);
#endif
}


void AGameplayDebuggerReplicator::BeginDestroy()
{
#if ENABLED_GAMEPLAY_DEBUGGER
	if (GEngine)
	{
		GEngine->bEnableOnScreenDebugMessages = true;
	}
#endif
	Super::BeginDestroy();
}

class UNetConnection* AGameplayDebuggerReplicator::GetNetConnection() const
{
#if ENABLED_GAMEPLAY_DEBUGGER
	if (LocalPlayerOwner && LocalPlayerOwner->IsPendingKill() == false && IsPendingKill() == false)
	{
		return LocalPlayerOwner->GetNetConnection();
	}
#endif
	return nullptr;
}

bool AGameplayDebuggerReplicator::ClientEnableTargetSelection_Validate(bool, APlayerController*)
{
#if ENABLED_GAMEPLAY_DEBUGGER
	return true;
#endif
	return false;
}

void AGameplayDebuggerReplicator::ClientEnableTargetSelection_Implementation(bool bEnable, APlayerController* Context)
{
#if ENABLED_GAMEPLAY_DEBUGGER
	bEnabledTargetSelection = bEnable;
#endif
}

void AGameplayDebuggerReplicator::EnableDraw(bool bEnable)
{
#if ENABLED_GAMEPLAY_DEBUGGER
	if (AHUD* const GameHUD = LocalPlayerOwner ? LocalPlayerOwner->GetHUD() : nullptr)
	{
		GameHUD->bShowHUD = bEnable ? false : true;
	}
	GEngine->bEnableOnScreenDebugMessages = bEnable ? false : true;
#endif
}

void AGameplayDebuggerReplicator::ActivateFromCommand()
{
#if ENABLED_GAMEPLAY_DEBUGGER
	OnActivationKeyPressed();
	if (GetLocalPlayerOwner())
	{
		SelectTargetToDebug();
	}
	OnActivationKeyReleased();
#endif
}

void AGameplayDebuggerReplicator::OnActivationKeyPressed()
{
#if ENABLED_GAMEPLAY_DEBUGGER
	const bool bIsActive = IsActorTickEnabled();
	bEnabledTargetSelection = true;
	if (DebugCameraController.IsValid())
	{
		return;
	}

	ActivationKeyTime = 0;
	bActivationKeyPressed = true;
	if (!bIsActive)
	{
		SetActorTickEnabled(true);
	}
#endif
}

void AGameplayDebuggerReplicator::OnActivationKeyReleased()
{
#if ENABLED_GAMEPLAY_DEBUGGER
	bEnabledTargetSelection = false;
	if (DebugCameraController.IsValid())
	{
		return;
	}

	if (ActivationKeyTime < GameplayDebuggerHelpers::ActivationKeyTimePch && InputComponent)
	{
		if (LocalPlayerOwner)
		{
			LocalPlayerOwner->PopInputComponent(InputComponent);
			InputComponent = nullptr;
		}
		ServerActivateGameplayDebugger(false);
		ClientActivateGameplayDebugger(false);

		GEngine->bEnableOnScreenDebugMessages = true;
		if (AHUD* const GameHUD = LocalPlayerOwner ? LocalPlayerOwner->GetHUD() : nullptr)
		{
			GameHUD->bShowHUD = true;
		}
	}
	else if (ActivationKeyTime < GameplayDebuggerHelpers::ActivationKeyTimePch && !InputComponent)
	{
		GEngine->bEnableOnScreenDebugMessages = false;
		if (AHUD* const GameHUD = LocalPlayerOwner ? LocalPlayerOwner->GetHUD() : nullptr)
		{
			GameHUD->bShowHUD = false;
		}
		BindKeyboardInput(InputComponent);

		ServerActivateGameplayDebugger(true);
		ClientActivateGameplayDebugger(true);
	}

	bActivationKeyPressed = false;
#endif
}

void AGameplayDebuggerReplicator::TickActor(float DeltaTime, enum ELevelTick TickType, FActorTickFunction& ThisTickFunction)
{
	Super::TickActor(DeltaTime, TickType, ThisTickFunction);

#if ENABLED_GAMEPLAY_DEBUGGER
	UWorld* World = GetWorld();
	const ENetMode NetMode = GetNetMode();
	if (!World)
	{
		// return without world
		return;
	}

	UGameInstance* GameInstance = World->GetGameInstance();
	if (!GameInstance || !World->IsGameWorld())
	{
		return;
	}

	if (NetMode != NM_DedicatedServer)
	{
		if (bActivationKeyPressed)
		{
			ActivationKeyTime += DeltaTime;
			if (ActivationKeyTime >= GameplayDebuggerHelpers::ActivationKeyTimePch)
			{
				GEngine->bEnableOnScreenDebugMessages = false;
				if (AHUD* const GameHUD = LocalPlayerOwner ? LocalPlayerOwner->GetHUD() : nullptr)
				{
					GameHUD->bShowHUD = false;
				}
				BindKeyboardInput(InputComponent);

				ServerActivateGameplayDebugger(true);
				ClientActivateGameplayDebugger(true);
				bActivationKeyPressed = false;
			}
		}

		if (bEnabledTargetSelection)
		{
			if (GetLocalPlayerOwner())
			{
				SelectTargetToDebug();
			}
		}

		bool bMarkComponentsAsRenderStateDirty = false;
		for (UGameplayDebuggerBaseObject* Obj : ReplicatedObjects)
		{
			if (Obj && Obj->IsRenderStateDirty())
			{
				if (!bMarkComponentsAsRenderStateDirty)
				{
					MarkComponentsRenderStateDirty();
				}
				bMarkComponentsAsRenderStateDirty = true;
				Obj->CleanRenderStateDirtyFlag();
			}
		}
	}

	if (NetMode < NM_Client && LocalPlayerOwner)
	{
		TMap<FString, TArray<UGameplayDebuggerBaseObject*> > CategoryToClasses;
		for (UGameplayDebuggerBaseObject* Obj : ReplicatedObjects)
		{
			if (Obj)
			{
				FString Category = Obj->GetCategoryName();
				if (IsCategoryEnabled(Category))
				{
					CategoryToClasses.FindOrAdd(Category).Add(Obj);
				}
			}
		}

		for (auto It(CategoryToClasses.CreateIterator()); It; ++It)
		{
			TArray<UGameplayDebuggerBaseObject*>& CurrentObjects = It.Value();
			for (UGameplayDebuggerBaseObject* Obj : CurrentObjects)
			{
				Obj->CollectDataToReplicateOnServer(LocalPlayerOwner, LastSelectedActorToDebug);
			}
		}
	}

#endif
}

void AGameplayDebuggerReplicator::OnSelectPlayer()
{
#if ENABLED_GAMEPLAY_DEBUGGER
	APlayerController* MyPC = DebugCameraController.IsValid() ? DebugCameraController.Get() : GetLocalPlayerOwner();
	APawn* MyPawn = MyPC ? MyPC->GetPawnOrSpectator() : nullptr;
	if (MyPawn)
	{
		ServerSetActorToDebug(MyPawn);
	}
#endif
}

void AGameplayDebuggerReplicator::SelectTargetToDebug()
{
#if ENABLED_GAMEPLAY_DEBUGGER
	APlayerController* MyPC = DebugCameraController.IsValid() ? DebugCameraController.Get() : GetLocalPlayerOwner();

	if (MyPC)
	{
		float bestAim = 0;
		FVector CamLocation;
		FRotator CamRotation;
		check(MyPC->PlayerCameraManager != nullptr);
		MyPC->PlayerCameraManager->GetCameraViewPoint(CamLocation, CamRotation);
		FVector FireDir = CamRotation.Vector();
		UWorld* World = MyPC->GetWorld();
		check(World);

		APawn* PossibleTarget = nullptr;
		for (FConstPawnIterator Iterator = World->GetPawnIterator(); Iterator; ++Iterator)
		{
			APawn* NewTarget = *Iterator;
			if (NewTarget == nullptr || NewTarget == MyPC->GetPawn()
				|| (NewTarget->PlayerState != nullptr && NewTarget->PlayerState->bIsABot == false)
				|| NewTarget->GetActorEnableCollision() == false)
			{
				continue;
			}

			if (NewTarget != MyPC->GetPawn())
			{
				// look for best controlled pawn target
				const FVector AimDir = NewTarget->GetActorLocation() - CamLocation;
				float FireDist = AimDir.SizeSquared();
				// only find targets which are < 25000 units away
				if (FireDist < 625000000.f)
				{
					FireDist = FMath::Sqrt(FireDist);
					float newAim = FVector::DotProduct(FireDir, AimDir);
					newAim = newAim / FireDist;
					if (newAim > bestAim)
					{
						PossibleTarget = NewTarget;
						bestAim = newAim;
					}
				}
			}
		}

		if (PossibleTarget != nullptr && PossibleTarget != LastSelectedActorToDebug)
		{
			ServerSetActorToDebug(Cast<AActor>(PossibleTarget));
		}
	}
#endif //ENABLED_GAMEPLAY_DEBUGGER
}

bool AGameplayDebuggerReplicator::ServerSetActorToDebug_Validate(AActor* InActor)
{
#if ENABLED_GAMEPLAY_DEBUGGER
	return true;
#endif
	return false;
}

void AGameplayDebuggerReplicator::ServerSetActorToDebug_Implementation(AActor* InActor)
{
#if ENABLED_GAMEPLAY_DEBUGGER
	if (LastSelectedActorToDebug != InActor)
	{
		LastSelectedActorToDebug = InActor;
	}
#endif
}

void AGameplayDebuggerReplicator::OnDebugAIDelegate(class UCanvas* Canvas, class APlayerController* PC)
{
#if WITH_EDITOR && ENABLED_GAMEPLAY_DEBUGGER
	if (!GIsEditor)
	{
		return;
	}

	if (!LocalPlayerOwner)
	{
		return;
	}

	UEditorEngine* EEngine = Cast<UEditorEngine>(GEngine);
	if (!EEngine || !EEngine->bIsSimulatingInEditor)
	{
		return;
	}

	if (!Canvas || !Canvas->SceneView || Canvas->SceneView->bIsGameView == false)
	{
		return;
	}

	FEngineShowFlags EngineShowFlags = Canvas && Canvas->SceneView && Canvas->SceneView->Family ? Canvas->SceneView->Family->EngineShowFlags : FEngineShowFlags(GIsEditor ? EShowFlagInitMode::ESFIM_Editor : EShowFlagInitMode::ESFIM_Game);
	if (!EngineShowFlags.DebugAI)
	{
		return;
	}

	EnableDraw(true);
	UWorld* World = GetWorld();
	if (World && Role == ROLE_Authority)
	{
		if (IsActorTickEnabled() == false)
		{
			SetActorTickEnabled(true);
		}

		// looks like Simulate in UE4 Editor - let's find selected Pawn to debug
		AActor* SelectedActor = nullptr;
		for (FSelectionIterator It = EEngine->GetSelectedActorIterator(); It; ++It)
		{
			SelectedActor = Cast<APawn>(*It); //we only work with pawns for now
			if (SelectedActor)
			{
				break;
			}
		}

		if (LastSelectedActorToDebug != SelectedActor)
		{
			MarkComponentsRenderStateDirty();
		}

		ServerSetActorToDebug(SelectedActor);
		DrawDebugData(Canvas, PC, true);
	}
#endif
}

void AGameplayDebuggerReplicator::DrawDebugDataDelegate(class UCanvas* Canvas, class APlayerController* PC)
{
#if ENABLED_GAMEPLAY_DEBUGGER
	if (GetWorld() == nullptr || IsPendingKill() || Canvas == nullptr || Canvas->IsPendingKill())
	{
		return;
	}

	if (!LocalPlayerOwner || !IsActorTickEnabled())
	{
		return;
	}

	if (Canvas->SceneView != nullptr && !Canvas->SceneView->bIsGameView)
	{
		return;
	}

	if (GetWorld()->bPlayersOnly && Role == ROLE_Authority)
	{
		for (FConstPawnIterator Iterator = GetWorld()->GetPawnIterator(); Iterator; ++Iterator)
		{
			AActor* NewTarget = Cast<AActor>(*Iterator);
			if (NewTarget->IsSelected() && GetSelectedActorToDebug() != NewTarget)
			{
				ServerSetActorToDebug(NewTarget);
			}
		}
	}
	DrawDebugData(Canvas, PC);
#endif
}

void AGameplayDebuggerReplicator::DrawDebugData(class UCanvas* Canvas, class APlayerController* PC, bool bHideMenu)
{
#if ENABLED_GAMEPLAY_DEBUGGER
	if (!LocalPlayerOwner && IsActorTickEnabled())
	{
		return;
	}

	const bool bAllowToDraw = Canvas && Canvas->SceneView && (Canvas->SceneView->ViewActor == LocalPlayerOwner->AcknowledgedPawn || Canvas->SceneView->ViewActor == LocalPlayerOwner->GetPawnOrSpectator());
	if (!bAllowToDraw)
	{
		// check for spectator debug camera during debug camera
		if (DebugCameraController.IsValid() == false || Canvas->SceneView->ViewActor->GetInstigatorController() != DebugCameraController.Get())
		{
			return;
		}
	}

	const float DebugInfoStartX = UGameplayDebuggerModuleSettings::StaticClass()->GetDefaultObject<UGameplayDebuggerModuleSettings>()->DebugInfoStart.X;
	const float DebugInfoStartY = UGameplayDebuggerModuleSettings::StaticClass()->GetDefaultObject<UGameplayDebuggerModuleSettings>()->DebugInfoStart.Y;
	const FVector SelectedActorLoc = LastSelectedActorToDebug ? LastSelectedActorToDebug->GetActorLocation() + FVector(0, 0, LastSelectedActorToDebug->GetSimpleCollisionHalfHeight()) : DebugTools::InvalidLocation;
	
	UGameplayDebuggerHelper::FPrintContext DefaultContext(GEngine->GetSmallFont(), Canvas, DebugInfoStartX, DebugInfoStartY);
	DefaultContext.FontRenderInfo.bEnableShadow = true;
	const bool bDrawFullData = SelectedActorLoc != DebugTools::InvalidLocation;
	const FVector ScreenLoc = SelectedActorLoc != DebugTools::InvalidLocation ? UGameplayDebuggerHelper::ProjectLocation(DefaultContext, SelectedActorLoc) : FVector::ZeroVector;
	UGameplayDebuggerHelper::FPrintContext OverHeadContext(GEngine->GetSmallFont(), Canvas, ScreenLoc.X, ScreenLoc.Y);

	UGameplayDebuggerHelper::SetOverHeadContext(OverHeadContext);
	UGameplayDebuggerHelper::SetDefaultContext(DefaultContext);

	if (DefaultContext.Canvas != nullptr)
	{
		float XL, YL;
		const FString ToolName = FString::Printf(TEXT("Gameplay Debugger [Timestamp: %05.03f]"), GetWorld()->TimeSeconds);
		UGameplayDebuggerHelper::CalulateStringSize(DefaultContext, nullptr, ToolName, XL, YL);
		UGameplayDebuggerHelper::PrintString(DefaultContext, FColorList::White, ToolName, DefaultContext.Canvas->ClipX / 2.0f - XL / 2.0f, 0);
	}

	if (!bHideMenu)
	{
		DrawMenu(DefaultContext, OverHeadContext);
	}

	TMap<FString, TArray<UGameplayDebuggerBaseObject*> > CategoryToClasses;
	for (UGameplayDebuggerBaseObject* Obj : ReplicatedObjects)
	{
		if (Obj)
		{
			FString Category = Obj->GetCategoryName();
			CategoryToClasses.FindOrAdd(Category).Add(Obj);
		}
	}
	CategoryToClasses.KeySort(TLess<FString>());

	for (auto It(CategoryToClasses.CreateIterator()); It; ++It)
	{
		const FGameplayDebuggerCategorySettings* Element = Categories.FindByPredicate([&](const FGameplayDebuggerCategorySettings& C){ return It.Key() == C.CategoryName; });
		if (Element == nullptr || Element->bPIE == false)
		{
			continue;
		}

		UGameplayDebuggerHelper::PrintString(UGameplayDebuggerHelper::GetDefaultContext(), FString::Printf(TEXT("\n{R=0,G=255,B=0,A=255}%s\n"), *It.Key()));
		TArray<UGameplayDebuggerBaseObject*>& CurrentObjects = It.Value();
		for (UGameplayDebuggerBaseObject* Obj : CurrentObjects)
		{
			Obj->DrawCollectedData(LocalPlayerOwner, LastSelectedActorToDebug);
		}
	}

	const IConsoleVariable* cvarHighlightSelectedActor = IConsoleManager::Get().FindConsoleVariable(TEXT("ai.gd.HighlightSelectedActor"));
	const bool bHighlightSelectedActor = !cvarHighlightSelectedActor || cvarHighlightSelectedActor->GetInt();
	if (LastSelectedActorToDebug && bHighlightSelectedActor)
	{
		FBox ComponentsBoundingBox = LastSelectedActorToDebug->GetComponentsBoundingBox(false);
		DrawDebugBox(GetWorld(), ComponentsBoundingBox.GetCenter(), ComponentsBoundingBox.GetExtent(), FColor::Red, false);
		DrawDebugSolidBox(GetWorld(), ComponentsBoundingBox.GetCenter(), ComponentsBoundingBox.GetExtent(), FColor::Red.WithAlpha(25));
	}
#endif
}

void AGameplayDebuggerReplicator::DrawMenu(UGameplayDebuggerBaseObject::FPrintContext& DefaultContext, UGameplayDebuggerBaseObject::FPrintContext& OverHeadContext)
{
#if ENABLED_GAMEPLAY_DEBUGGER
	const float OldX = DefaultContext.CursorX;
	const float OldY = DefaultContext.CursorY;

	const UGameplayDebuggerModuleSettings* GameplayDebuggerSettings = UGameplayDebuggerModuleSettings::StaticClass()->GetDefaultObject<UGameplayDebuggerModuleSettings>();
	const float MenuStartX = GameplayDebuggerSettings->MenuStart.X;
	const float MenuStartY = GameplayDebuggerSettings->MenuStart.Y;

	if (DefaultContext.Canvas != nullptr)
	{
		UFont* OldFont = DefaultContext.Font.Get();
		DefaultContext.Font = GEngine->GetMediumFont();

		TArray<float> CategoriesWidth;
		CategoriesWidth.AddZeroed(Categories.Num());

		APlayerController* const MyPC = Cast<APlayerController>(LocalPlayerOwner);

		FString KeyDesc = ActivationKeyName != ActivationKeyDisplayName ? FString::Printf(TEXT("(%s key)"), *ActivationKeyName) : TEXT("");
		FString HeaderDesc;
		if (UGameplayDebuggerModuleSettings::StaticClass()->GetDefaultObject<UGameplayDebuggerModuleSettings>()->UseAlternateKeys())
		{
			HeaderDesc = FString::Printf(TEXT("{LightBlue}Tap {white}%s %s {LightBlue}to close, {white}Alt + number {LightBlue}to toggle categories, use {white}+ {LightBlue}or {white}- {LightBlue}to select active line with categories"), *ActivationKeyDisplayName, *KeyDesc);
		}
		else
		{
			HeaderDesc = FString::Printf(TEXT("{LightBlue}Use {white}%s %s{LightBlue} to close, {white}Numpad numbers {LightBlue}to toggle categories or use {white}+ {LightBlue}and {white}- {LightBlue}to select active line with categories"), *ActivationKeyDisplayName, *KeyDesc);
		}

		FString DefaultMenu;
		{
			DefaultMenu += FString::Printf(TEXT(" {white}[Tab]: %s%s  "), DebugCameraController.IsValid() ? TEXT("{Green}") : TEXT("{LightBlue}"), TEXT("Debug Camera"));
		}
		{
			DefaultMenu += FString::Printf(TEXT(" {white}[Ctrl+Tab]: %s%s  "), GEngine && GEngine->bEnableOnScreenDebugMessages ? TEXT("{Green}") : TEXT("{LightBlue}"), TEXT("DebugMessages"));
		}
		{
			const AHUD* GameHUD = MyPC ? MyPC->GetHUD() : nullptr;
			DefaultMenu += FString::Printf(TEXT(" {white}[Ctrl+Tilde]: %s%s  "), GameHUD && GameHUD->bShowHUD ? TEXT("{Green}") : TEXT("{LightBlue}"), TEXT("GameHUD"));
		}
		{
			KeyDesc = UGameplayDebuggerModuleSettings::StaticClass()->GetDefaultObject<UGameplayDebuggerModuleSettings>()->GetInputConfiguration().SelectPlayer.Key.GetFName().ToString();
			const bool SelectedPlayer = MyPC && MyPC->GetPawnOrSpectator() && LastSelectedActorToDebug == MyPC->GetPawnOrSpectator();
			DefaultMenu += FString::Printf(TEXT(" {white}[%s]: %sPlayer  "), *KeyDesc, SelectedPlayer ? TEXT("{Green}") : TEXT("{LightBlue}"));
		}

		FString MenuString = HeaderDesc + TEXT("\n") + DefaultMenu + TEXT("\n");
		const int32 NumberOfLines = Categories.Num() / 10 + 1;
		for (int32 LineIndex = 0; LineIndex < NumberOfLines; ++LineIndex)
		{
			const int32 ItemsInLine = FMath::Clamp(Categories.Num() - LineIndex * 10, 0, 10);
			for (int32 MenuItemIndex = 0; MenuItemIndex < ItemsInLine; ++MenuItemIndex)
			{
				const int32 i = LineIndex * 10 + MenuItemIndex;

				const bool bIsActive = IsCategoryEnabled(Categories[i].CategoryName);
				const bool bIsDisabled = LastSelectedActorToDebug == nullptr;
				const FString CategoryColor = bIsDisabled ? (bIsActive ? TEXT("DarkGreen") : TEXT("LightGrey")) : (bIsActive ? TEXT("Green") : TEXT("LightBlue"));
				if (CurrentMenuLine == LineIndex)
				{
					MenuString += FString::Printf(TEXT("   {white}%d:{%s}%s "), MenuItemIndex, *CategoryColor, *Categories[i].CategoryName);
				}
				else
				{
					MenuString += FString::Printf(TEXT("   {%s}%s "), *CategoryColor, *Categories[i].CategoryName);
				}
			}
			MenuString += TEXT(" \n");
		}
		float MenuWidth, MenuHeight;
		UGameplayDebuggerBaseObject::CalulateStringSize(DefaultContext, nullptr, MenuString, MenuWidth, MenuHeight);
		MaxMenuWidth = FMath::Max(MaxMenuWidth, MenuWidth+4);
		MenuHeight += 4;

		FCanvasTileItem TileItem(FVector2D(0, 0), GWhiteTexture, FVector2D(MaxMenuWidth, MenuHeight), FColor(0, 0, 0, 30));
		TileItem.BlendMode = SE_BLEND_Translucent;
		UGameplayDebuggerBaseObject::DrawItem(DefaultContext, TileItem, MenuStartX, MenuStartY);

		float CharWidth, CharHeight;
		UGameplayDebuggerBaseObject::CalulateStringSize(DefaultContext, nullptr, TEXT("X"), CharWidth, CharHeight);
		TileItem = FCanvasTileItem(FVector2D(0, 0), GWhiteTexture, FVector2D(MaxMenuWidth, CharHeight), FColor(0, 0, 0, 50));
		TileItem.BlendMode = SE_BLEND_Translucent;
		UGameplayDebuggerBaseObject::DrawItem(DefaultContext, TileItem, MenuStartX, MenuStartY + 2 + (CurrentMenuLine+2) * CharHeight);
		UGameplayDebuggerBaseObject::PrintString(DefaultContext, MenuString, MenuStartX + 2, MenuStartY + 2);

		DefaultContext.Font = OldFont;
	}

	if (LastSelectedActorToDebug == nullptr)
	{
		UGameplayDebuggerBaseObject::PrintString(DefaultContext, "{red}No Pawn selected - waiting for data to replicate from server. {green}Press and hold ' to select Pawn \n");
	}

	if (DebugCameraController.IsValid())
	{
		ADebugCameraController* DebugCamController = DebugCameraController.Get();
		if (DebugCamController != nullptr)
		{
			FVector const CamLoc = DebugCamController->PlayerCameraManager->GetCameraLocation();
			FRotator const CamRot = DebugCamController->PlayerCameraManager->GetCameraRotation();

			FString HitString;
			FCollisionQueryParams TraceParams(NAME_None, true, this);
			FHitResult Hit;
			bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, CamLoc, CamRot.Vector() * 100000.f + CamLoc, ECC_Pawn, TraceParams);
			if (bHit && Hit.GetActor() != nullptr)
			{
				HitString = FString::Printf(TEXT("{white}Under cursor: {yellow}'%s'"), *Hit.GetActor()->GetName());
				DrawDebugLine(GetWorld(), Hit.Location, Hit.Location + Hit.Normal*60.f, FColor::White, /*bPersistentLines =*/ false, /*LifeTime =*/ -1.f, /*DepthPriority =*/ 0, /*Thickness =*/ 3.f);
			}
			else
			{
				HitString = FString::Printf(TEXT("Not actor under cursor"));
			}

			UGameplayDebuggerBaseObject::PrintString(DefaultContext, FColor::White, HitString/*, MenuStartX, MenuStartY + 40*/);
		}
	}


	DefaultContext.CursorX = OldX;
	DefaultContext.CursorY = OldY;
#endif //ENABLED_GAMEPLAY_DEBUGGER
}

void AGameplayDebuggerReplicator::DebugNextPawn(UClass* CompareClass, APawn* CurrentPawn)
{
#if ENABLED_GAMEPLAY_DEBUGGER
	APawn* LastSeen = nullptr;
	APawn* FirstSeen = nullptr;
	// Search through the list looking for the next one of this type
	for (FConstPawnIterator It = GetWorld()->GetPawnIterator(); It; It++)
	{
		APawn* IterPawn = *It;
		if (IterPawn->IsA(CompareClass))
		{
			if (LastSeen == CurrentPawn)
			{
				ServerSetActorToDebug(IterPawn);
				return;
			}
			LastSeen = IterPawn;
			if (FirstSeen == nullptr)
			{
				FirstSeen = IterPawn;
			}
		}
	}
	// See if we need to wrap around the list
	if (FirstSeen != nullptr)
	{
		ServerSetActorToDebug(FirstSeen);
	}
#endif
}

void AGameplayDebuggerReplicator::DebugPrevPawn(UClass* CompareClass, APawn* CurrentPawn)
{
#if ENABLED_GAMEPLAY_DEBUGGER
	APawn* LastSeen = nullptr;
	APawn* PrevSeen = nullptr;
	APawn* FirstSeen = nullptr;
	// Search through the list looking for the prev one of this type
	for (FConstPawnIterator It = GetWorld()->GetPawnIterator(); It; It++)
	{
		APawn* IterPawn = *It;
		if (IterPawn->IsA(CompareClass))
		{
			if (LastSeen == CurrentPawn && FirstSeen != CurrentPawn)
			{
				ServerSetActorToDebug(PrevSeen);
				return;
			}
			PrevSeen = LastSeen;
			LastSeen = IterPawn;
			if (FirstSeen == nullptr)
			{
				FirstSeen = IterPawn;
				if (CurrentPawn == nullptr)
				{
					ServerSetActorToDebug(FirstSeen);
					return;
				}
			}
		}
	}
	// Wrap from the beginning to the end
	if (FirstSeen == CurrentPawn)
	{
		ServerSetActorToDebug(LastSeen);
	}
	// Handle getting the previous to the end
	else if (LastSeen == CurrentPawn)
	{
		ServerSetActorToDebug(PrevSeen);
	}
#endif
}

//////////////////////////////////////////////////////////////////////////
//----------------------------------------------------------------------//
// Composite Scene proxy
//----------------------------------------------------------------------//
class FDebugRenderSceneCompositeProxy : public FDebugRenderSceneProxy
{
public:
	FDebugRenderSceneCompositeProxy(const UPrimitiveComponent* InComponent)
		: FDebugRenderSceneProxy(InComponent)
	{
	}

	virtual ~FDebugRenderSceneCompositeProxy()
	{
		for (int32 Index = 0; Index < ChildProxies.Num(); ++Index)
		{
			delete ChildProxies[Index];
		}
	}

	virtual void DrawStaticElements(FStaticPrimitiveDrawInterface* PDI) override
	{
		for (int32 Index = 0; Index < ChildProxies.Num(); ++Index)
		{
			ChildProxies[Index]->DrawStaticElements(PDI);
		}
	}

	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override
	{
		for (int32 Index = 0; Index < ChildProxies.Num(); ++Index)
		{
			ChildProxies[Index]->GetDynamicMeshElements(Views, ViewFamily, VisibilityMap, Collector);
		}
	}

	virtual void RegisterDebugDrawDelgate() override
	{
		for (int32 Index = 0; Index < ChildProxies.Num(); ++Index)
		{
			ChildProxies[Index]->RegisterDebugDrawDelgate();
		}
	}

	virtual void UnregisterDebugDrawDelgate() override
	{
		for (int32 Index = 0; Index < ChildProxies.Num(); ++Index)
		{
			ChildProxies[Index]->UnregisterDebugDrawDelgate();
		}
	}

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override
	{
		FPrimitiveViewRelevance Result;
		for (int32 Index = 0; Index < ChildProxies.Num(); ++Index)
		{
			Result |= ChildProxies[Index]->GetViewRelevance(View);
		}
		return Result;
	}

	virtual uint32 GetMemoryFootprint(void) const override
	{
		return sizeof(*this) + GetAllocatedSize();
	}

	uint32 GetAllocatedSize(void) const
	{
		uint32 Size = 0;
		for (int32 Index = 0; Index < ChildProxies.Num(); ++Index)
		{
			ChildProxies[Index]->GetMemoryFootprint();
		}

		return Size + ChildProxies.GetAllocatedSize();
	}

	void AddChild(FDebugRenderSceneProxy* NewChild)
	{
		ChildProxies.AddUnique(NewChild);
	}

protected:
	TArray<FDebugRenderSceneProxy*> ChildProxies;
};

//////////////////////////////////////////////////////////////////////////
//----------------------------------------------------------------------//
// UGameplayDebuggerDrawComponent
//----------------------------------------------------------------------//
UGameplayDebuggerDrawComponent::UGameplayDebuggerDrawComponent(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer) { }

FPrimitiveSceneProxy* UGameplayDebuggerDrawComponent::CreateSceneProxy()
{
	FDebugRenderSceneCompositeProxy* CompositeProxy = nullptr;
#if ENABLED_GAMEPLAY_DEBUGGER
	AGameplayDebuggerReplicator* Replicator = Cast<AGameplayDebuggerReplicator>(GetOwner());
	if (!Replicator || Replicator->IsActorTickEnabled() == false || !World || World->GetNetMode() == NM_DedicatedServer || IsPendingKill())
	{
		return nullptr;
	}

	CompositeProxy = new FDebugRenderSceneCompositeProxy(this);

	for (UGameplayDebuggerBaseObject* Obj : Replicator->ReplicatedObjects)
	{
		if (Obj && Replicator->IsCategoryEnabled(Obj->GetCategoryName()))
		{
			FDebugRenderSceneProxy* ChildSceneProxy = Obj->CreateSceneProxy(this, World, Replicator->GetSelectedActorToDebug());
			if (ChildSceneProxy)
			{
				CompositeProxy->AddChild(ChildSceneProxy);
			}
		}
	}

#endif //ENABLED_GAMEPLAY_DEBUGGER
	return CompositeProxy;
}

FBoxSphereBounds UGameplayDebuggerDrawComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	//we know nothing about 
	FBox MyBounds(FVector(-HALF_WORLD_MAX1, -HALF_WORLD_MAX1, -HALF_WORLD_MAX1), FVector(HALF_WORLD_MAX1, HALF_WORLD_MAX1, HALF_WORLD_MAX1));
	return MyBounds;
}

void UGameplayDebuggerDrawComponent::CreateRenderState_Concurrent()
{
	Super::CreateRenderState_Concurrent();

#if WITH_EDITOR
	if (SceneProxy)
	{
		static_cast<FDebugRenderSceneCompositeProxy*>(SceneProxy)->RegisterDebugDrawDelgate();
	}
#endif
}

void UGameplayDebuggerDrawComponent::DestroyRenderState_Concurrent()
{
#if WITH_EDITOR
	if (SceneProxy)
	{
		static_cast<FDebugRenderSceneCompositeProxy*>(SceneProxy)->UnregisterDebugDrawDelgate();
	}
#endif

	Super::DestroyRenderState_Concurrent();
}

//////////////////////////////////////////////////////////////////////////
//----------------------------------------------------------------------//
// AGaneplayDebuggerHUD
//----------------------------------------------------------------------//
AGameplayDebuggerHUD::AGameplayDebuggerHUD(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
}

void AGameplayDebuggerHUD::PostRender()
{
#if ENABLED_GAMEPLAY_DEBUGGER
	if (RedirectedHUD.IsValid())
	{
		RedirectedHUD->SetCanvas(Canvas, DebugCanvas);
		RedirectedHUD->PostRender();
	}
#endif
}

