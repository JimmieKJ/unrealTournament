// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "GameplayDebuggerPrivate.h"
#include "Engine/GameInstance.h"
#include "Debug/DebugDrawService.h"
#include "GameFramework/HUD.h"
#include "GameplayDebuggingComponent.h"
#include "GameplayDebuggingHUDComponent.h"
#include "GameplayDebuggingReplicator.h"
#include "BehaviorTreeDelegates.h"
#if WITH_EDITOR
#include "Editor/EditorEngine.h"
#include "LevelEditorViewport.h"
#endif // WITH_EDITOR
#include "UnrealNetwork.h"

FOnSelectionChanged AGameplayDebuggingReplicator::OnSelectionChangedDelegate;

AGameplayDebuggingReplicator::AGameplayDebuggingReplicator(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, MaxEQSQueries(5)
	, bIsGlobalInWorld(true)
	, LastDrawAtFrame(0)
	, PlayerControllersUpdateDelay(0)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		ConstructorHelpers::FObjectFinderOptional<UTexture2D> RedIcon;
		ConstructorHelpers::FObjectFinderOptional<UTexture2D> GreenIcon;

		// both icons are needed to debug AI
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
	PrimaryActorTick.bStartWithTickEnabled = false;
	
	USceneComponent* SceneComponent = CreateDefaultSubobject<USceneComponent>(TEXT("SceneComponent"));
	RootComponent = SceneComponent;

#if WITH_EDITOR
	SetIsTemporarilyHiddenInEditor(true);
#endif
#if WITH_EDITORONLY_DATA
	SetTickableWhenPaused(true);
	SetActorHiddenInGame(false);
	bHiddenEdLevel = true;
	bHiddenEdLayer = true;
	bHiddenEd = true;
	bEditable = false;
#endif

	DebuggerShowFlags =  GameplayDebuggerSettings().DebuggerShowFlags;

	FGameplayDebuggerSettings Settings = GameplayDebuggerSettings(this);
#define UPDATE_VIEW_PROPS(__FlagName__)  __FlagName__ = Settings.CheckFlag(EAIDebugDrawDataView::__FlagName__);
	UPDATE_VIEW_PROPS(OverHead);
	UPDATE_VIEW_PROPS(Basic);
	UPDATE_VIEW_PROPS(BehaviorTree);
	UPDATE_VIEW_PROPS(EQS);
	UPDATE_VIEW_PROPS(Perception);
	UPDATE_VIEW_PROPS(GameView1);
	UPDATE_VIEW_PROPS(GameView2);
	UPDATE_VIEW_PROPS(GameView3);
	UPDATE_VIEW_PROPS(GameView4);
	UPDATE_VIEW_PROPS(GameView5);
#undef UPDATE_VIEW_PROPS

	EnableEQSOnHUD = true;

	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		SetActorTickEnabled(true);

		bReplicates = false;
		SetRemoteRoleForBackwardsCompat(ROLE_SimulatedProxy);
		SetReplicates(true);

		AGameplayDebuggingReplicator::OnSelectionChangedDelegate.AddUObject(this, &AGameplayDebuggingReplicator::ServerSetActorToDebug);
	}
#endif //!(UE_BUILD_SHIPPING || UE_BUILD_TEST)
}

void AGameplayDebuggingReplicator::GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		DOREPLIFETIME_CONDITION(AGameplayDebuggingReplicator, DebugComponent, COND_OwnerOnly);
		DOREPLIFETIME_CONDITION(AGameplayDebuggingReplicator, LocalPlayerOwner, COND_OwnerOnly);
		DOREPLIFETIME_CONDITION(AGameplayDebuggingReplicator, bIsGlobalInWorld, COND_OwnerOnly);
		DOREPLIFETIME_CONDITION(AGameplayDebuggingReplicator, LastSelectedActorToDebug, COND_OwnerOnly);
#endif
}

bool AGameplayDebuggingReplicator::IsNetRelevantFor(const AActor* RealViewer, const AActor* ViewTarget, const FVector& SrcLocation) const
{
	return LocalPlayerOwner == RealViewer;
}

void AGameplayDebuggingReplicator::PostInitializeComponents()
{
	Super::PostInitializeComponents();
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	SetActorTickEnabled(true);
#endif
}

#if WITH_EDITOR
void AGameplayDebuggingReplicator::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (!PropertyChangedEvent.Property)
	{
		return;
	}

	FGameplayDebuggerSettings Settings = GameplayDebuggerSettings(this);

#define CHECK_AND_UPDATE_FLAGS(__FlagName_) \
	if (PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(AGameplayDebuggingReplicator, __FlagName_)) \
	{ \
		__FlagName_ ? Settings.SetFlag(EAIDebugDrawDataView::__FlagName_) : Settings.ClearFlag(EAIDebugDrawDataView::__FlagName_); \
		GetDebugComponent()->ServerReplicateData(Settings.CheckFlag(EAIDebugDrawDataView::__FlagName_) ? EDebugComponentMessage::ActivateDataView : EDebugComponentMessage::DeactivateDataView, EAIDebugDrawDataView::__FlagName_); \
	}else

	CHECK_AND_UPDATE_FLAGS(OverHead)
	CHECK_AND_UPDATE_FLAGS(Basic)
	CHECK_AND_UPDATE_FLAGS(BehaviorTree)
	CHECK_AND_UPDATE_FLAGS(EQS)
	CHECK_AND_UPDATE_FLAGS(Perception)
	CHECK_AND_UPDATE_FLAGS(GameView1)
	CHECK_AND_UPDATE_FLAGS(GameView2)
	CHECK_AND_UPDATE_FLAGS(GameView3)
	CHECK_AND_UPDATE_FLAGS(GameView4)
	CHECK_AND_UPDATE_FLAGS(GameView5) {}

#if WITH_EQS
	if (PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(AGameplayDebuggingReplicator, EQS))
	{
		GetDebugComponent()->EnableClientEQSSceneProxy(EQS);
		GetDebugComponent()->SetEQSIndex(ActiveEQSIndex);
		GetDebugComponent()->MarkRenderStateDirty();
	}

	if (PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(AGameplayDebuggingReplicator, ActiveEQSIndex))
	{
		GetDebugComponent()->SetEQSIndex(ActiveEQSIndex);
	}
#endif // WITH_EQS
#undef CHECK_AND_UPDATE_FLAGS

#endif //!(UE_BUILD_SHIPPING || UE_BUILD_TEST)
}
#endif //WITH_EDITOR

void AGameplayDebuggingReplicator::BeginPlay()
{
	Super::BeginPlay();

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (Role == ROLE_Authority)
	{
		bReplicates = false;
		SetRemoteRoleForBackwardsCompat(ROLE_SimulatedProxy);
		SetReplicates(true);

		if (!DebugComponentClass.IsValid() && GetWorld() && GetNetMode() < ENetMode::NM_Client)
		{
			DebugComponentClass = StaticLoadClass(UGameplayDebuggingComponent::StaticClass(), NULL, *DebugComponentClassName, NULL, LOAD_None, NULL);
			if (!DebugComponentClass.IsValid())
			{
				DebugComponentClass = UGameplayDebuggingComponent::StaticClass();
			}
		}
		GetDebugComponent();
	}

	if (GetWorld() && GetNetMode() != ENetMode::NM_DedicatedServer)
	{
		if (GIsEditor)
		{
			UDebugDrawService::Register(TEXT("DebugAI"), FDebugDrawDelegate::CreateUObject(this, &AGameplayDebuggingReplicator::OnDebugAIDelegate));
		}
		UDebugDrawService::Register(TEXT("Game"), FDebugDrawDelegate::CreateUObject(this, &AGameplayDebuggingReplicator::DrawDebugDataDelegate));

		if (!DebugComponentHUDClass.IsValid())
		{
			DebugComponentHUDClass = StaticLoadClass(AGameplayDebuggingHUDComponent::StaticClass(), NULL, *DebugComponentHUDClassName, NULL, LOAD_None, NULL);
			if (!DebugComponentHUDClass.IsValid())
			{
				DebugComponentHUDClass = AGameplayDebuggingHUDComponent::StaticClass();
			}
		}
	}

#if WITH_EDITOR
	const UEditorEngine* EEngine = Cast<UEditorEngine>(GEngine);
	if (EEngine && (EEngine->bIsSimulatingInEditor || EEngine->EditorWorld) && GetWorld() != EEngine->EditorWorld && !IsGlobalInWorld() && GCurrentLevelEditingViewportClient && GCurrentLevelEditingViewportClient->EngineShowFlags.DebugAI)
	{
		SetIsTemporarilyHiddenInEditor(false);
		SetActorHiddenInGame(false);
		bHiddenEdLevel = false;
		bHiddenEdLayer = false;
		bHiddenEd = false;
		bEditable = true;

		if (DebugComponent)
		{
			DebugComponent->ServerReplicateData(EDebugComponentMessage::ActivateReplication, EAIDebugDrawDataView::Empty);

			FGameplayDebuggerSettings Settings = GameplayDebuggerSettings(this);
			DebugComponent->ServerReplicateData(Settings.CheckFlag(EAIDebugDrawDataView::OverHead) ? EDebugComponentMessage::ActivateDataView : EDebugComponentMessage::DeactivateDataView, EAIDebugDrawDataView::OverHead);
			DebugComponent->ServerReplicateData(Settings.CheckFlag(EAIDebugDrawDataView::Basic) ? EDebugComponentMessage::ActivateDataView : EDebugComponentMessage::DeactivateDataView, EAIDebugDrawDataView::Basic);
			DebugComponent->ServerReplicateData(Settings.CheckFlag(EAIDebugDrawDataView::BehaviorTree) ? EDebugComponentMessage::ActivateDataView : EDebugComponentMessage::DeactivateDataView, EAIDebugDrawDataView::BehaviorTree);
			DebugComponent->ServerReplicateData(Settings.CheckFlag(EAIDebugDrawDataView::EQS) ? EDebugComponentMessage::ActivateDataView : EDebugComponentMessage::DeactivateDataView, EAIDebugDrawDataView::EQS);
			DebugComponent->ServerReplicateData(Settings.CheckFlag(EAIDebugDrawDataView::Perception) ? EDebugComponentMessage::ActivateDataView : EDebugComponentMessage::DeactivateDataView, EAIDebugDrawDataView::Perception);
			DebugComponent->ServerReplicateData(Settings.CheckFlag(EAIDebugDrawDataView::GameView1) ? EDebugComponentMessage::ActivateDataView : EDebugComponentMessage::DeactivateDataView, EAIDebugDrawDataView::GameView1);
			DebugComponent->ServerReplicateData(Settings.CheckFlag(EAIDebugDrawDataView::GameView2) ? EDebugComponentMessage::ActivateDataView : EDebugComponentMessage::DeactivateDataView, EAIDebugDrawDataView::GameView2);
			DebugComponent->ServerReplicateData(Settings.CheckFlag(EAIDebugDrawDataView::GameView3) ? EDebugComponentMessage::ActivateDataView : EDebugComponentMessage::DeactivateDataView, EAIDebugDrawDataView::GameView3);
			DebugComponent->ServerReplicateData(Settings.CheckFlag(EAIDebugDrawDataView::GameView4) ? EDebugComponentMessage::ActivateDataView : EDebugComponentMessage::DeactivateDataView, EAIDebugDrawDataView::GameView4);
			DebugComponent->ServerReplicateData(Settings.CheckFlag(EAIDebugDrawDataView::GameView5) ? EDebugComponentMessage::ActivateDataView : EDebugComponentMessage::DeactivateDataView, EAIDebugDrawDataView::GameView5);
		}
	}
	else
	{
		SetTickableWhenPaused(true);
		SetIsTemporarilyHiddenInEditor(true);
		SetActorHiddenInGame(false);
		bHiddenEdLevel = true;
		bHiddenEdLayer = true;
		bHiddenEd = true;
		bEditable = false;
		if (DebugComponent)
		{
			DebugComponent->ServerReplicateData(EDebugComponentMessage::DeactivateDataView, EAIDebugDrawDataView::Empty);
		}
	}
#endif

	if (GetWorld() && GetNetMode() != ENetMode::NM_DedicatedServer)
	{
		if (GIsEditor)
		{
			UDebugDrawService::Register(TEXT("DebugAI"), FDebugDrawDelegate::CreateUObject(this, &AGameplayDebuggingReplicator::OnDebugAIDelegate));
		}
		UDebugDrawService::Register(TEXT("Game"), FDebugDrawDelegate::CreateUObject(this, &AGameplayDebuggingReplicator::DrawDebugDataDelegate));

		if (!DebugComponentHUDClass.IsValid())
		{
			DebugComponentHUDClass = StaticLoadClass(AGameplayDebuggingHUDComponent::StaticClass(), NULL, *DebugComponentHUDClassName, NULL, LOAD_None, NULL);
			if (!DebugComponentHUDClass.IsValid())
			{
				DebugComponentHUDClass = AGameplayDebuggingHUDComponent::StaticClass();
			}
		}
	}

	if (bAutoActivate)
	{
		OnRep_AutoActivate();
	}

#endif //!(UE_BUILD_SHIPPING || UE_BUILD_TEST)
}

void AGameplayDebuggingReplicator::PostNetInit()
{
	Super::PostNetInit();
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (bAutoActivate)
	{
		OnRep_AutoActivate();
	}
#endif
}

void AGameplayDebuggingReplicator::ClientAutoActivate_Implementation()
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	// we are already replicated so let's activate tool
	if (GetWorld() && GetNetMode() == ENetMode::NM_Client && !IsToolCreated() && !IsGlobalInWorld())
	{
		CreateTool();
		EnableTool();
	}
#endif
}

void AGameplayDebuggingReplicator::OnRep_AutoActivate()
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	// we are already replicated so let's activate tool
	if (GetWorld() && GetNetMode() == ENetMode::NM_Client && !IsToolCreated() && !IsGlobalInWorld())
	{
		CreateTool();
		EnableTool();
	}
#endif
}

UGameplayDebuggingComponent* AGameplayDebuggingReplicator::GetDebugComponent()
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (IsPendingKill() == false && !DebugComponent && DebugComponentClass.IsValid() && GetNetMode() < ENetMode::NM_Client)
	{
		DebugComponent = NewObject<UGameplayDebuggingComponent>(this, DebugComponentClass.Get());
		DebugComponent->SetIsReplicated(true);
		DebugComponent->RegisterComponent();
		DebugComponent->Activate();
	}
#endif
	return DebugComponent;
}

class UNetConnection* AGameplayDebuggingReplicator::GetNetConnection() const
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (LocalPlayerOwner && LocalPlayerOwner->IsPendingKill() == false && IsPendingKill() == false)
	{
		return LocalPlayerOwner->GetNetConnection();
	}
#endif
	return NULL;
}

bool AGameplayDebuggingReplicator::ClientEnableTargetSelection_Validate(bool, APlayerController* )
{
	return true;
}

void AGameplayDebuggingReplicator::ClientEnableTargetSelection_Implementation(bool bEnable, APlayerController* Context)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (GetDebugComponent())
	{
		GetDebugComponent()->ClientEnableTargetSelection(bEnable);
	}
#endif
}

bool AGameplayDebuggingReplicator::ClientReplicateMessage_Validate(class AActor* Actor, uint32 InMessage, uint32 DataView)
{
	return true;
}

void AGameplayDebuggingReplicator::ClientReplicateMessage_Implementation(class  AActor* Actor, uint32 InMessage, uint32 DataView)
{

}

bool AGameplayDebuggingReplicator::ServerReplicateMessage_Validate(class AActor* Actor, uint32 InMessage, uint32 DataView)
{
	return true;
}

void AGameplayDebuggingReplicator::ServerReplicateMessage_Implementation(class  AActor* Actor, uint32 InMessage, uint32 DataView)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if ((EDebugComponentMessage::Type)InMessage == EDebugComponentMessage::DeactivateReplilcation)
	{
		ServerSetActorToDebug(NULL);
		MarkComponentsRenderStateDirty();
	}

	if (GetDebugComponent())
	{
		GetDebugComponent()->ServerReplicateData((EDebugComponentMessage::Type)InMessage, (EAIDebugDrawDataView::Type)DataView);
	}
#endif //!(UE_BUILD_SHIPPING || UE_BUILD_TEST)
}

bool AGameplayDebuggingReplicator::IsDrawEnabled()
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	return bEnabledDraw && GetWorld() && GetNetMode() != ENetMode::NM_DedicatedServer;
#else
	return false;
#endif
}

void AGameplayDebuggingReplicator::EnableDraw(bool bEnable)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	bEnabledDraw = bEnable;

	if (AHUD* const GameHUD = LocalPlayerOwner ? LocalPlayerOwner->GetHUD() : NULL)
	{
		GameHUD->bShowHUD = bEnable ? false : true;
	}
	GEngine->bEnableOnScreenDebugMessages = bEnable ? false : true;

	if (DebugComponent)
	{
		const bool bEnabledEQSView = GameplayDebuggerSettings(this).CheckFlag(EAIDebugDrawDataView::EQS);
		DebugComponent->EnableClientEQSSceneProxy(bEnable && bEnabledEQSView ? true : false);
		DebugComponent->MarkRenderStateDirty();
	}
#endif
}

bool AGameplayDebuggingReplicator::IsToolCreated()
{
	UGameplayDebuggingControllerComponent*  GDC = FindComponentByClass<UGameplayDebuggingControllerComponent>();
	return LocalPlayerOwner && GDC;
}

void AGameplayDebuggingReplicator::CreateTool()
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (GetWorld() && GetNetMode() != ENetMode::NM_DedicatedServer)
	{
		UGameplayDebuggingControllerComponent*  GDC = FindComponentByClass<UGameplayDebuggingControllerComponent>();
		if (!GDC)
		{
			DebugComponentControllerClass = StaticLoadClass(UGameplayDebuggingControllerComponent::StaticClass(), NULL, *DebugComponentControllerClassName, NULL, LOAD_None, NULL);
			if (!DebugComponentControllerClass.IsValid())
			{
				DebugComponentControllerClass = AGameplayDebuggingHUDComponent::StaticClass();
			}
			GDC = NewObject<UGameplayDebuggingControllerComponent>(this, DebugComponentControllerClass.Get());
			GDC->SetPlayerOwner(LocalPlayerOwner);
			GDC->RegisterComponent();
		}
	}
#endif
}

void AGameplayDebuggingReplicator::EnableTool()
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (GetWorld() && GetNetMode() != ENetMode::NM_DedicatedServer)
	{
		UGameplayDebuggingControllerComponent*  GDC = FindComponentByClass<UGameplayDebuggingControllerComponent>();
		if (GDC)
		{
			// simulate key press
			GDC->OnActivationKeyPressed();
			GDC->OnActivationKeyReleased();
		}
	}
#endif
}

void AGameplayDebuggingReplicator::TickActor(float DeltaTime, enum ELevelTick TickType, FActorTickFunction& ThisTickFunction)
{
	Super::TickActor(DeltaTime, TickType, ThisTickFunction);

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	UWorld* World = GetWorld();
	if (!IsGlobalInWorld() || !World || GetNetMode() == ENetMode::NM_Client || !IGameplayDebugger::IsAvailable())
	{
		// global level replicator don't have any local player and it's prepared to work only on servers
		return;
	}

	UGameInstance* GameInstance = World->GetGameInstance();
	if (!GameInstance || !World->IsGameWorld())
	{
		return;
	}

	PlayerControllersUpdateDelay -= DeltaTime;
	if (PlayerControllersUpdateDelay <= 0)
	{
		for (FConstPlayerControllerIterator Iterator = World->GetPlayerControllerIterator(); Iterator; Iterator++)
		{
			APlayerController* PC = *Iterator;
			if (PC)
			{
				IGameplayDebugger& Debugger = IGameplayDebugger::Get();
				Debugger.CreateGameplayDebuggerForPlayerController(PC);
			}
		}
		PlayerControllersUpdateDelay = 5;
	}
#endif
}

bool AGameplayDebuggingReplicator::ServerSetActorToDebug_Validate(AActor* InActor)
{
	return true;
}

void AGameplayDebuggingReplicator::ServerSetActorToDebug_Implementation(AActor* InActor)
{ 
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (LastSelectedActorToDebug != InActor)
	{
		LastSelectedActorToDebug = InActor;
		UGameplayDebuggingComponent::OnDebuggingTargetChangedDelegate.Broadcast(InActor, InActor ? InActor->IsSelected() : false);
		APawn* TargetPawn = Cast<APawn>(InActor);
		if (TargetPawn)
		{
			FBehaviorTreeDelegates::OnDebugSelected.Broadcast(TargetPawn);
		}
	}

	if (UGameplayDebuggingComponent* DebuggingComponent = GetDebugComponent())
	{
		DebuggingComponent->SetActorToDebug(InActor);
	}
#endif
}

void AGameplayDebuggingReplicator::OnDebugAIDelegate(class UCanvas* Canvas, class APlayerController* PC)
{
#if WITH_EDITOR && !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (!GIsEditor)
	{
		return;
	}

	if (!LocalPlayerOwner || IsGlobalInWorld())
	{
		return;
	}

	UEditorEngine* EEngine = Cast<UEditorEngine>(GEngine);
	if (GFrameNumber == LastDrawAtFrame || !EEngine || !EEngine->bIsSimulatingInEditor)
	{
		return;
	}

	if (!Canvas || !Canvas->SceneView || Canvas->SceneView->bIsGameView == false)
	{
		return;
	}
	LastDrawAtFrame = GFrameNumber;

	FEngineShowFlags EngineShowFlags = Canvas && Canvas->SceneView && Canvas->SceneView->Family ? Canvas->SceneView->Family->EngineShowFlags : FEngineShowFlags(GIsEditor ? EShowFlagInitMode::ESFIM_Editor : EShowFlagInitMode::ESFIM_Game);
	if (!EngineShowFlags.DebugAI)
	{
		return;
	}

	EnableDraw(true);
	UWorld* World = GetWorld();
	UGameplayDebuggingComponent* DebuggingComponent = GetDebugComponent();
	if (World && DebuggingComponent && DebuggingComponent->GetOwnerRole() == ROLE_Authority)
	{
		UGameplayDebuggingControllerComponent*  GDC = FindComponentByClass<UGameplayDebuggingControllerComponent>();
		TArray<int32> OryginalReplicateViewDataCounters;

		OryginalReplicateViewDataCounters = DebuggingComponent->ReplicateViewDataCounters;
		for (uint32 Index = 0; Index < EAIDebugDrawDataView::MAX; ++Index)
		{
			DebuggingComponent->ReplicateViewDataCounters[Index] = GameplayDebuggerSettings(this).CheckFlag((EAIDebugDrawDataView::Type)Index) ? 1 : 0;
		}

		// looks like Simulate in UE4 Editor - let's find selected Pawn to debug
		AActor* FullSelectedTarget = NULL;
		for (FConstPawnIterator Iterator = World->GetPawnIterator(); Iterator; ++Iterator)
		{
			AActor* NewTarget = Cast<AActor>(*Iterator);

			if (NewTarget->IsSelected() && !FullSelectedTarget)
			{
				FullSelectedTarget = NewTarget;
				continue;
			}

			//We needs to collect data manually in Simulate
			DebuggingComponent->SetActorToDebug(NewTarget);
			DebuggingComponent->CollectDataToReplicate(NewTarget->IsSelected());
			DrawDebugData(Canvas, PC);
		}

		const AActor* OldActor = LastSelectedActorToDebug;
		ServerSetActorToDebug(FullSelectedTarget);
		if (FullSelectedTarget)
		{
			DebuggingComponent->CollectDataToReplicate(true);
			DebuggingComponent->SetEQSIndex(ActiveEQSIndex);
			DrawDebugData(Canvas, PC);
		}

		if (GetSelectedActorToDebug() != OldActor)
		{
			DebuggingComponent->MarkRenderStateDirty();
		}

		DebuggingComponent->ReplicateViewDataCounters = OryginalReplicateViewDataCounters;

	}
#endif
}

void AGameplayDebuggingReplicator::DrawDebugDataDelegate(class UCanvas* Canvas, class APlayerController* PC)
{
#if !(UE_BUILD_SHIPPING && UE_BUILD_TEST)
	if (GetWorld() == NULL || IsPendingKill() || Canvas == NULL || Canvas->IsPendingKill())
	{
		return;
	}

	if (!LocalPlayerOwner || IsGlobalInWorld() || !IsDrawEnabled())
	{
		return;
	}

	if (Canvas->SceneView != NULL && !Canvas->SceneView->bIsGameView)
	{
		return;
	}

	if (GFrameNumber == LastDrawAtFrame)
	{
		return;
	}
	LastDrawAtFrame = GFrameNumber;

	const UGameplayDebuggingControllerComponent*  GDC = FindComponentByClass<UGameplayDebuggingControllerComponent>();
	if (!GDC)
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

			GetDebugComponent()->SetActorToDebug(NewTarget);
			GetDebugComponent()->CollectDataToReplicate(true);
		}
	}

	DrawDebugData(Canvas, PC);
#endif
}

void AGameplayDebuggingReplicator::DrawDebugData(class UCanvas* Canvas, class APlayerController* PC)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (!LocalPlayerOwner)
	{
		return;
	}

	bool bAllowToDraw = Canvas && Canvas->SceneView && (Canvas->SceneView->ViewActor == LocalPlayerOwner->AcknowledgedPawn || Canvas->SceneView->ViewActor == LocalPlayerOwner->GetPawnOrSpectator());
	if (!bAllowToDraw)
	{
		// check for spectator debug camera
		UGameplayDebuggingControllerComponent* GDC = FindComponentByClass<UGameplayDebuggingControllerComponent>();
		bAllowToDraw = GDC && GDC->GetDebugCameraController().IsValid() && Canvas->SceneView->ViewActor->GetInstigatorController() == GDC->GetDebugCameraController().Get();
		
		if (!bAllowToDraw)
		{
			return;
		}
	}

	if (!DebugRenderer.IsValid() && DebugComponentHUDClass.IsValid())
	{
		FActorSpawnParameters SpawnInfo;
		SpawnInfo.Owner = NULL;
		SpawnInfo.Instigator = NULL;
		SpawnInfo.bNoCollisionFail = true;

		DebugRenderer = GetWorld()->SpawnActor<AGameplayDebuggingHUDComponent>(DebugComponentHUDClass.Get(), SpawnInfo);
		DebugRenderer->SetCanvas(Canvas);
		DebugRenderer->SetPlayerOwner(LocalPlayerOwner);
		DebugRenderer->SetWorld(GetWorld());
	}

	if (DebugRenderer != NULL)
	{
		DebugRenderer->SetCanvas(Canvas);
		DebugRenderer->Render();
	}

#endif
}

void AGameplayDebuggingReplicator::DebugNextPawn(UClass* CompareClass, APawn* CurrentPawn)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
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

void AGameplayDebuggingReplicator::DebugPrevPawn(UClass* CompareClass, APawn* CurrentPawn)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
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
