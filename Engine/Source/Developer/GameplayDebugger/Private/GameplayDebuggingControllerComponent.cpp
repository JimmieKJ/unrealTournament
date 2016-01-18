// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#include "GameplayDebuggerPrivate.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/HUD.h"
#include "GameFramework/PlayerInput.h"
#include "BehaviorTreeDelegates.h"
#include "TimerManager.h"
#include "VisualLogger/VisualLogger.h"
#include "GameplayDebuggerSettings.h"
#include "Engine/Player.h"

DEFINE_LOG_CATEGORY_STATIC(LogGameplayDebugging, Log, All);

#define USE_ALTERNATIVE_KEYS 0
#define BUGIT_VIEWS (1<<EAIDebugDrawDataView::Basic) | (1 << EAIDebugDrawDataView::OverHead)
#define BREAK_LINE_TEXT TEXT("________________________________________________________________")

AGaneplayDebuggerProxyHUD::AGaneplayDebuggerProxyHUD(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
}

void AGaneplayDebuggerProxyHUD::PostRender()
{
	if (RedirectedHUD.IsValid())
	{
		RedirectedHUD->SetCanvas(Canvas, DebugCanvas);
		RedirectedHUD->PostRender();
	}
}

UGameplayDebuggingControllerComponent::UGameplayDebuggingControllerComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, KeyPressActivationTime(0.4f)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	PrimaryComponentTick.bCanEverTick = true;
	bWantsInitializeComponent = true;
	bAutoActivate = false;
	bTickInEditor=true;
	PrimaryComponentTick.bStartWithTickEnabled = false;

	DebugAITargetActor = nullptr;
	
	bToolActivated = false;
	bWaitingForOwnersComponent = false;

	ControlKeyPressedTime = 0;

	ActivationKey = FInputChord(EKeys::Apostrophe, false, false, false, false);
	CategoryZeroBind = FInputChord(EKeys::NumPadZero, false, false, false, false);
	CategoryOneBind = FInputChord(EKeys::NumPadOne, false, false, false, false);
	CategoryTwoBind = FInputChord(EKeys::NumPadTwo, false, false, false, false);
	CategoryThreeBind = FInputChord(EKeys::NumPadThree, false, false, false, false);
	CategoryFourBind = FInputChord(EKeys::NumPadFour, false, false, false, false);
	CategoryFiveBind = FInputChord(EKeys::NumPadFive, false, false, false, false);
	CategorySixBind = FInputChord(EKeys::NumPadSix, false, false, false, false);
	CategorySevenBind = FInputChord(EKeys::NumPadSeven, false, false, false, false);
	CategoryEightBind = FInputChord(EKeys::NumPadEight, false, false, false, false);
	CategoryNineBind = FInputChord(EKeys::NumPadNine, false, false, false, false);
	CycleDetailsViewBind = FInputChord(EKeys::Add, false, false, false, false);
	DebugCameraBind = FInputChord(EKeys::Tab, false, false, false, false);
	OnScreenDebugMessagesBind = FInputChord(EKeys::Tab, false, true, false, false);
	GameHUDBind = FInputChord(EKeys::Tilde, false, true, false, false);
#endif //!(UE_BUILD_SHIPPING || UE_BUILD_TEST)
}

#if WITH_HOT_RELOAD_CTORS
UGameplayDebuggingControllerComponent::UGameplayDebuggingControllerComponent(FVTableHelper& Helper)
	: Super(Helper)
	, KeyPressActivationTime(0.0f)
{

}
#endif // WITH_HOT_RELOAD_CTORS

void UGameplayDebuggingControllerComponent::OnRegister()
{
	Super::OnRegister();
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	BindActivationKeys();
	SetActiveViews(GameplayDebuggerSettings().DebuggerShowFlags);
#endif
}

AGameplayDebuggingReplicator* UGameplayDebuggingControllerComponent::GetDebuggingReplicator() const
{
	return Cast<AGameplayDebuggingReplicator>(GetOuter());
}

void UGameplayDebuggingControllerComponent::BeginDestroy()
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	AHUD* GameHUD = PlayerOwner.IsValid() ? PlayerOwner->GetHUD() : nullptr;
	if (GameHUD)
	{
		GameHUD->bShowHUD = true;
	}

	if (PlayerOwner.IsValid() && PlayerOwner->InputComponent && !PlayerOwner->IsPendingKill())
	{
		for (int32 Index = PlayerOwner->InputComponent->KeyBindings.Num() - 1; Index >= 0; --Index)
		{
			const FInputKeyBinding& KeyBind = PlayerOwner->InputComponent->KeyBindings[Index];
			if (KeyBind.KeyDelegate.IsBoundToObject(this))
			{
				PlayerOwner->InputComponent->KeyBindings.RemoveAtSwap(Index);
			}
		}
	}

	if (GetDebuggingReplicator() && PlayerOwner.IsValid() && !PlayerOwner->IsPendingKill())
	{
		APawn* Pawn = PlayerOwner->GetPawnOrSpectator();
		if (Pawn && !Pawn->IsPendingKill())
		{
			GetDebuggingReplicator()->ServerReplicateMessage(Pawn, EDebugComponentMessage::DeactivateReplilcation, 0);
		}

		for (uint32 Index = 0; Index < EAIDebugDrawDataView::MAX; ++Index)
		{
			GetDebuggingReplicator()->ServerReplicateMessage(DebugAITargetActor, EDebugComponentMessage::DeactivateDataView, (EAIDebugDrawDataView::Type)Index);
		}
		GetDebuggingReplicator()->ServerReplicateMessage(DebugAITargetActor, EDebugComponentMessage::DisableExtendedView);
	}
#endif //!(UE_BUILD_SHIPPING || UE_BUILD_TEST)

	Super::BeginDestroy();
}

bool UGameplayDebuggingControllerComponent::IsViewActive(EAIDebugDrawDataView::Type View) const
{
	return GameplayDebuggerSettings(GetDebuggingReplicator()).CheckFlag(View);
}

uint32 UGameplayDebuggingControllerComponent::GetActiveViews()
{
	return GameplayDebuggerSettings(GetDebuggingReplicator()).DebuggerShowFlags;
}

void UGameplayDebuggingControllerComponent::SetActiveViews(uint32 InActiveViews)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	GameplayDebuggerSettings(GetDebuggingReplicator()).DebuggerShowFlags = InActiveViews;

	if (GetDebuggingReplicator())
	{
		for (uint32 Index = 0; Index < EAIDebugDrawDataView::MAX; ++Index)
		{
			EAIDebugDrawDataView::Type CurrentView = (EAIDebugDrawDataView::Type)Index;
			EnableActiveView(CurrentView, IsViewActive(CurrentView));
		}
	}
#endif //!(UE_BUILD_SHIPPING || UE_BUILD_TEST)
}

void UGameplayDebuggingControllerComponent::EnableActiveView(EAIDebugDrawDataView::Type View, bool bEnable)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	bEnable ? GameplayDebuggerSettings(GetDebuggingReplicator()).SetFlag(View) : GameplayDebuggerSettings(GetDebuggingReplicator()).ClearFlag(View);

	if (GetDebuggingReplicator())
	{
		GetDebuggingReplicator()->ServerReplicateMessage(DebugAITargetActor, bEnable ? EDebugComponentMessage::ActivateDataView : EDebugComponentMessage::DeactivateDataView, View);
#if WITH_EQS
		if (GetDebuggingReplicator()->GetDebugComponent() && View == EAIDebugDrawDataView::EQS)
		{
			GetDebuggingReplicator()->GetDebugComponent()->EnableClientEQSSceneProxy(IsViewActive(EAIDebugDrawDataView::EQS));
		}
#endif // WITH_EQS
	}
#endif //!(UE_BUILD_SHIPPING || UE_BUILD_TEST)
}

void UGameplayDebuggingControllerComponent::BindActivationKeys()
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (PlayerOwner.IsValid() && PlayerOwner->InputComponent && PlayerOwner->PlayerInput)
	{
		// find current activation key used for 'EnableGDT' binding
		for (uint32 BindIndex = 0; BindIndex < (uint32)PlayerOwner->PlayerInput->DebugExecBindings.Num(); BindIndex++)
		{
			if (PlayerOwner->PlayerInput->DebugExecBindings[BindIndex].Command == TEXT("EnableGDT"))
			{
				ActivationKey.Key = PlayerOwner->PlayerInput->DebugExecBindings[BindIndex].Key;
				ActivationKey.bCtrl = PlayerOwner->PlayerInput->DebugExecBindings[BindIndex].Control;
				ActivationKey.bAlt = PlayerOwner->PlayerInput->DebugExecBindings[BindIndex].Alt;
				ActivationKey.bShift = PlayerOwner->PlayerInput->DebugExecBindings[BindIndex].Shift;
				break;
			}
		}

		PlayerOwner->InputComponent->BindKey(ActivationKey, IE_Pressed, this, &UGameplayDebuggingControllerComponent::OnActivationKeyPressed);
		PlayerOwner->InputComponent->BindKey(ActivationKey, IE_Released, this, &UGameplayDebuggingControllerComponent::OnActivationKeyReleased);
	}
#endif //!(UE_BUILD_SHIPPING || UE_BUILD_TEST)
}

void UGameplayDebuggingControllerComponent::OnActivationKeyPressed()
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (GetDebuggingReplicator() && PlayerOwner.IsValid())
	{
		if (!bToolActivated)
		{
			Activate();
			SetComponentTickEnabled(true);

			BindAIDebugViewKeys(AIDebugViewInputComponent);
			if (PlayerOwner.IsValid())
			{
				PlayerOwner->PushInputComponent(AIDebugViewInputComponent);
			}

			GetDebuggingReplicator()->EnableDraw(true);
			GetDebuggingReplicator()->ServerReplicateMessage(nullptr, EDebugComponentMessage::ActivateReplication, EAIDebugDrawDataView::Empty);
		}

		ControlKeyPressedTime = GetWorld()->GetTimeSeconds();
		EnableTargetSelection(true);
	}
#endif //!(UE_BUILD_SHIPPING || UE_BUILD_TEST)
}

void UGameplayDebuggingControllerComponent::OnActivationKeyReleased()
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	const float KeyPressedTime = GetWorld()->GetTimeSeconds() - ControlKeyPressedTime;

	EnableTargetSelection(false);
	if (GetDebuggingReplicator() && bToolActivated)
	{
		if (KeyPressedTime < KeyPressActivationTime && DebugCameraController.IsValid() == false)
		{
			CloseDebugTool();
		}
		else
		{
			APawn* TargetPawn = GetDebuggingReplicator()->GetDebugComponent() ? Cast<APawn>(GetDebuggingReplicator()->GetDebugComponent()->GetSelectedActor()) : nullptr;
			if (TargetPawn != nullptr)
			{
				FBehaviorTreeDelegates::OnDebugLocked.Broadcast(TargetPawn);
			}
		}
	}
#endif //!(UE_BUILD_SHIPPING || UE_BUILD_TEST)
}

void UGameplayDebuggingControllerComponent::OpenDebugTool()
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (GetDebuggingReplicator() && IsActive())
	{
		bToolActivated = true;
	}
#endif //!(UE_BUILD_SHIPPING || UE_BUILD_TEST)
}

void UGameplayDebuggingControllerComponent::CloseDebugTool()
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (GetDebuggingReplicator())
	{
		Deactivate();
		SetComponentTickEnabled(false);
		GetDebuggingReplicator()->ServerReplicateMessage(nullptr, EDebugComponentMessage::DeactivateReplilcation, EAIDebugDrawDataView::Empty);
		GetDebuggingReplicator()->EnableDraw(false);
		GetDebuggingReplicator()->ServerReplicateMessage(nullptr, EDebugComponentMessage::DeactivateReplilcation, EAIDebugDrawDataView::Empty);

		if (PlayerOwner.IsValid())
		{
			PlayerOwner->PopInputComponent(AIDebugViewInputComponent);
		}
		AIDebugViewInputComponent = nullptr;
		bToolActivated = false;
	}
#endif //!(UE_BUILD_SHIPPING || UE_BUILD_TEST)
}

void UGameplayDebuggingControllerComponent::EnableTargetSelection(bool bEnable)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	GetDebuggingReplicator()->ClientEnableTargetSelection(bEnable, PlayerOwner.Get());
#endif //!(UE_BUILD_SHIPPING || UE_BUILD_TEST)
}

void UGameplayDebuggingControllerComponent::BindAIDebugViewKeys(class UInputComponent*& InputComponent)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (!InputComponent)
	{
		InputComponent = NewObject<UInputComponent>(GetOwner(), TEXT("AIDebugViewInputComponent0"));
		InputComponent->RegisterComponent();

		InputComponent->BindKey(CategoryZeroBind, IE_Pressed, this, &UGameplayDebuggingControllerComponent::ToggleAIDebugView_SetView0);
		InputComponent->BindKey(CategoryOneBind, IE_Pressed, this, &UGameplayDebuggingControllerComponent::ToggleAIDebugView_SetView1);
		InputComponent->BindKey(CategoryTwoBind, IE_Pressed, this, &UGameplayDebuggingControllerComponent::ToggleAIDebugView_SetView2);
		InputComponent->BindKey(CategoryThreeBind, IE_Pressed, this, &UGameplayDebuggingControllerComponent::ToggleAIDebugView_SetView3);
		InputComponent->BindKey(CategoryFourBind, IE_Pressed, this, &UGameplayDebuggingControllerComponent::ToggleAIDebugView_SetView4);
		InputComponent->BindKey(CategoryFiveBind, IE_Pressed, this, &UGameplayDebuggingControllerComponent::ToggleAIDebugView_SetView5);
		InputComponent->BindKey(CategorySixBind, IE_Pressed, this, &UGameplayDebuggingControllerComponent::ToggleAIDebugView_SetView6);
		InputComponent->BindKey(CategorySevenBind, IE_Pressed, this, &UGameplayDebuggingControllerComponent::ToggleAIDebugView_SetView7);
		InputComponent->BindKey(CategoryEightBind, IE_Pressed, this, &UGameplayDebuggingControllerComponent::ToggleAIDebugView_SetView8);
		InputComponent->BindKey(CategoryNineBind, IE_Pressed, this, &UGameplayDebuggingControllerComponent::ToggleAIDebugView_SetView9);
		InputComponent->BindKey(CycleDetailsViewBind, IE_Released, this, &UGameplayDebuggingControllerComponent::CycleDetailsView);
		InputComponent->BindKey(DebugCameraBind, IE_Released, this, &UGameplayDebuggingControllerComponent::ToggleDebugCamera);
		InputComponent->BindKey(OnScreenDebugMessagesBind, IE_Released, this, &UGameplayDebuggingControllerComponent::ToggleOnScreenDebugMessages);
		InputComponent->BindKey(GameHUDBind, IE_Released, this, &UGameplayDebuggingControllerComponent::ToggleGameHUD);
	}
#endif //!(UE_BUILD_SHIPPING || UE_BUILD_TEST)
}

bool UGameplayDebuggingControllerComponent::CanToggleView()
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	return  (PlayerOwner.IsValid() && bToolActivated /*&& DebugAITargetActor*/);
#else
	return false;
#endif
}

void UGameplayDebuggingControllerComponent::ToggleAIDebugView_SetView0()
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	APawn* Pawn = PlayerOwner.IsValid() ? PlayerOwner->GetPawnOrSpectator() : nullptr;

	if (PlayerOwner.IsValid() && Pawn && bToolActivated && GetDebuggingReplicator())
	{
		if (UGameplayDebuggingComponent* OwnerComp = GetDebuggingReplicator() ? GetDebuggingReplicator()->GetDebugComponent() : nullptr)
		{
			GameplayDebuggerSettings(GetDebuggingReplicator()).DebuggerShowFlags ^= 1 << EAIDebugDrawDataView::NavMesh;

			if (IsViewActive(EAIDebugDrawDataView::NavMesh))
			{
				GetWorld()->GetTimerManager().SetTimer(TimerHandle_UpdateNavMeshTimer, this, &UGameplayDebuggingControllerComponent::UpdateNavMeshTimer, 5.0f, true);
				UpdateNavMeshTimer();

				GetDebuggingReplicator()->ServerReplicateMessage(Pawn, EDebugComponentMessage::ActivateDataView, EAIDebugDrawDataView::NavMesh);
				OwnerComp->MarkRenderStateDirty();
			}
			else
			{
				GetWorld()->GetTimerManager().ClearTimer(TimerHandle_UpdateNavMeshTimer);

				GetDebuggingReplicator()->ServerReplicateMessage(Pawn, EDebugComponentMessage::DeactivateDataView, EAIDebugDrawDataView::NavMesh);
				OwnerComp->ServerDiscardNavmeshData();
				OwnerComp->MarkRenderStateDirty();
			}
		}
	}
#endif //!(UE_BUILD_SHIPPING || UE_BUILD_TEST)
}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
#	define DEFAULT_TOGGLE_HANDLER(__func_name__, __DataView__) \
	void UGameplayDebuggingControllerComponent::__func_name__() \
	{ \
		if (CanToggleView()) \
		{ \
			ToggleActiveView(__DataView__); \
		} \
	}

#else
#	define DEFAULT_TOGGLE_HANDLER(__func_name__, __DataView__) \
	void UGameplayDebuggingControllerComponent::__func_name__() { }
#endif 

DEFAULT_TOGGLE_HANDLER(ToggleAIDebugView_SetView1, EAIDebugDrawDataView::Basic);
DEFAULT_TOGGLE_HANDLER(ToggleAIDebugView_SetView2, EAIDebugDrawDataView::BehaviorTree);
DEFAULT_TOGGLE_HANDLER(ToggleAIDebugView_SetView3, EAIDebugDrawDataView::EQS);
DEFAULT_TOGGLE_HANDLER(ToggleAIDebugView_SetView4, EAIDebugDrawDataView::Perception);
DEFAULT_TOGGLE_HANDLER(ToggleAIDebugView_SetView5, EAIDebugDrawDataView::GameView1);
DEFAULT_TOGGLE_HANDLER(ToggleAIDebugView_SetView6, EAIDebugDrawDataView::GameView2);
DEFAULT_TOGGLE_HANDLER(ToggleAIDebugView_SetView7, EAIDebugDrawDataView::GameView3);
DEFAULT_TOGGLE_HANDLER(ToggleAIDebugView_SetView8, EAIDebugDrawDataView::GameView4);
DEFAULT_TOGGLE_HANDLER(ToggleAIDebugView_SetView9, EAIDebugDrawDataView::GameView5);

void UGameplayDebuggingControllerComponent::CycleDetailsView()
{
	GetDebuggingReplicator()->OnCycleDetailsView.Broadcast();
}

#ifdef DEFAULT_TOGGLE_HANDLER
# undef DEFAULT_TOGGLE_HANDLER
#endif

void UGameplayDebuggingControllerComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (GetWorld()->GetTimeSeconds() - ControlKeyPressedTime > KeyPressActivationTime && !bToolActivated)
	{
		OpenDebugTool();
	}

	if (!bToolActivated)
	{
		return;
	}
#endif //!(UE_BUILD_SHIPPING || UE_BUILD_TEST)
}

float UGameplayDebuggingControllerComponent::GetUpdateNavMeshTimeRemaining() const
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	return GetWorld()->GetTimerManager().GetTimerRemaining(TimerHandle_UpdateNavMeshTimer);
#else
	return FLT_MAX;
#endif
}

void UGameplayDebuggingControllerComponent::UpdateNavMeshTimer()
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	const APawn* PlayerPawn = PlayerOwner.IsValid() ? PlayerOwner->GetPawnOrSpectator() : nullptr;
	UGameplayDebuggingComponent* DebuggingComponent = GetDebuggingReplicator() ? GetDebuggingReplicator()->GetDebugComponent() : nullptr;
	if (DebuggingComponent)
	{
		const AActor* SelectedActor = DebuggingComponent->GetSelectedActor();
		const APawn* SelectedActorAsPawn = Cast<APawn>(SelectedActor);

		const FVector AdditionalTargetLoc =
			SelectedActorAsPawn ? SelectedActorAsPawn->GetNavAgentLocation() :
			SelectedActor ? SelectedActor->GetActorLocation() :
			PlayerPawn ? PlayerPawn->GetNavAgentLocation() :
			FVector::ZeroVector;

		if (AdditionalTargetLoc != FVector::ZeroVector)
		{
			DebuggingComponent->ServerCollectNavmeshData(AdditionalTargetLoc);
		}
	}
#endif //!(UE_BUILD_SHIPPING || UE_BUILD_TEST)
}

void UGameplayDebuggingControllerComponent::ToggleOnScreenDebugMessages()
{
	if (!bToolActivated || PlayerOwner == nullptr)
	{
		return;
	}

	if (GEngine)
	{
		GEngine->bEnableOnScreenDebugMessages = !GEngine->bEnableOnScreenDebugMessages;
	}
}

void UGameplayDebuggingControllerComponent::ToggleGameHUD()
{
	if (!bToolActivated || PlayerOwner == nullptr)
	{
		return;
	}

	if (AHUD* const GameHUD = PlayerOwner->GetHUD())
	{
		GameHUD->bShowHUD = !GameHUD->bShowHUD;
	}
}

void UGameplayDebuggingControllerComponent::ToggleDebugCamera()
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if(!bToolActivated || PlayerOwner == nullptr)
	{
		return;
	}

	if (DebugCameraController.IsValid() == false)
	{
		{
			FActorSpawnParameters SpawnInfo;
			SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			SpawnInfo.Owner = PlayerOwner->GetWorldSettings();
			SpawnInfo.Instigator = PlayerOwner->Instigator;
			DebugCameraController = GetWorld()->SpawnActor<ADebugCameraController>(SpawnInfo);
		}

		if (DebugCameraController.IsValid())
		{
			// set up new controller
			DebugCameraController->OnActivate(PlayerOwner.Get());

			// then switch to it
			PlayerOwner->Player->SwitchController(DebugCameraController.Get());

			FActorSpawnParameters SpawnInfo;
			SpawnInfo.Owner = PlayerOwner.Get();
			SpawnInfo.Instigator = PlayerOwner->Instigator;
			SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			SpawnInfo.ObjectFlags |= RF_Transient;	// We never want these to save into a map
			AGaneplayDebuggerProxyHUD* ProxyHUD = GetWorld()->SpawnActor<AGaneplayDebuggerProxyHUD>(SpawnInfo);
			ProxyHUD->RedirectedHUD = PlayerOwner->MyHUD;
			DebugCameraController->MyHUD = ProxyHUD;
			BindAIDebugViewKeys(DebugCameraInputComponent);
			DebugCameraInputComponent->BindKey(ActivationKey, IE_Pressed, this, &UGameplayDebuggingControllerComponent::OnActivationKeyPressed);
			DebugCameraInputComponent->BindKey(ActivationKey, IE_Released, this, &UGameplayDebuggingControllerComponent::OnActivationKeyReleased);
			DebugCameraController->PushInputComponent(DebugCameraInputComponent);

			DebugCameraController->ChangeState(NAME_Default);
			DebugCameraController->ChangeState(NAME_Spectating);

			if (PlayerOwner.IsValid())
			{
				PlayerOwner->PopInputComponent(AIDebugViewInputComponent);
			}
			AIDebugViewInputComponent = nullptr;
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

		if(AIDebugViewInputComponent == nullptr)
		{
			BindAIDebugViewKeys(AIDebugViewInputComponent);
			if (PlayerOwner.IsValid())
			{
				PlayerOwner->PushInputComponent(AIDebugViewInputComponent);
			}
		}

	}

#endif //!(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	}
