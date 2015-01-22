// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#include "GameplayDebuggerPrivate.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/HUD.h"
#include "GameFramework/PlayerInput.h"
#include "BehaviorTreeDelegates.h"
#include "TimerManager.h"
#include "VisualLogger/VisualLogger.h"

DEFINE_LOG_CATEGORY_STATIC(LogGameplayDebugging, Log, All);

#define USE_ALTERNATIVE_KEYS 0
#define BUGIT_VIEWS (1<<EAIDebugDrawDataView::Basic) | (1 << EAIDebugDrawDataView::OverHead)
#define BREAK_LINE_TEXT TEXT("________________________________________________________________")

UGameplayDebuggingControllerComponent::UGameplayDebuggingControllerComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, KeyPressActivationTime(0.4f)
{
	PrimaryComponentTick.bCanEverTick = true;
	bWantsInitializeComponent = true;
	bAutoActivate = false;
	bTickInEditor=true;
	PrimaryComponentTick.bStartWithTickEnabled = false;

	DebugAITargetActor = NULL;
	
	bToolActivated = false;
	bWaitingForOwnersComponent = false;

	ControlKeyPressedTime = 0;
	ActivationKey = FInputChord(EKeys::Apostrophe, false, false, false, false);
}

void UGameplayDebuggingControllerComponent::OnRegister()
{
	Super::OnRegister();
	BindActivationKeys();
	SetActiveViews(GameplayDebuggerSettings().DebuggerShowFlags);
}

AGameplayDebuggingReplicator* UGameplayDebuggingControllerComponent::GetDebuggingReplicator() const
{
	return Cast<AGameplayDebuggingReplicator>(GetOuter());
}

void UGameplayDebuggingControllerComponent::BeginDestroy()
{
	AHUD* GameHUD = PlayerOwner.IsValid() ? PlayerOwner->GetHUD() : NULL;
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
		APawn* Pawn = PlayerOwner->GetPawn();
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
	GameplayDebuggerSettings(GetDebuggingReplicator()).DebuggerShowFlags = InActiveViews;

	if (GetDebuggingReplicator())
	{
		for (uint32 Index = 0; Index < EAIDebugDrawDataView::MAX; ++Index)
		{
			EAIDebugDrawDataView::Type CurrentView = (EAIDebugDrawDataView::Type)Index;
			EnableActiveView(CurrentView, IsViewActive(CurrentView));
		}
	}
}

void UGameplayDebuggingControllerComponent::EnableActiveView(EAIDebugDrawDataView::Type View, bool bEnable)
{
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
}

void UGameplayDebuggingControllerComponent::BindActivationKeys()
{
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
}

void UGameplayDebuggingControllerComponent::OnActivationKeyPressed()
{
	if (GetDebuggingReplicator() && PlayerOwner.IsValid())
	{
		if (!bToolActivated)
		{
			Activate();
			SetComponentTickEnabled(true);

			BindAIDebugViewKeys();
			GetDebuggingReplicator()->EnableDraw(true);
			GetDebuggingReplicator()->ServerReplicateMessage(NULL, EDebugComponentMessage::ActivateReplication, EAIDebugDrawDataView::Empty);
		}

		ControlKeyPressedTime = GetWorld()->GetTimeSeconds();
		EnableTargetSelection(true);
	}
}

void UGameplayDebuggingControllerComponent::OnActivationKeyReleased()
{
	const float KeyPressedTime = GetWorld()->GetTimeSeconds() - ControlKeyPressedTime;

	EnableTargetSelection(false);
	if (GetDebuggingReplicator() && bToolActivated)
	{
		if (KeyPressedTime < KeyPressActivationTime)
		{
			CloseDebugTool();
		}
		else
		{
			APawn* TargetPawn = GetDebuggingReplicator()->GetDebugComponent() ? Cast<APawn>(GetDebuggingReplicator()->GetDebugComponent()->GetSelectedActor()) : NULL;
			if (TargetPawn != NULL)
			{
				FBehaviorTreeDelegates::OnDebugLocked.Broadcast(TargetPawn);
			}
		}
	}
}

void UGameplayDebuggingControllerComponent::OpenDebugTool()
{
	if (GetDebuggingReplicator() && IsActive())
	{
		bToolActivated = true;
	}

}

void UGameplayDebuggingControllerComponent::CloseDebugTool()
{
	if (GetDebuggingReplicator())
	{
		Deactivate();
		SetComponentTickEnabled(false);
		GetDebuggingReplicator()->ServerReplicateMessage(NULL, EDebugComponentMessage::DeactivateReplilcation, EAIDebugDrawDataView::Empty);
		GetDebuggingReplicator()->EnableDraw(false);
		GetDebuggingReplicator()->ServerReplicateMessage(NULL, EDebugComponentMessage::DeactivateReplilcation, EAIDebugDrawDataView::Empty);
		bToolActivated = false;
	}
}

void UGameplayDebuggingControllerComponent::EnableTargetSelection(bool bEnable)
{
	GetDebuggingReplicator()->ServerEnableTargetSelection(bEnable, PlayerOwner.Get());
}

void UGameplayDebuggingControllerComponent::BindAIDebugViewKeys()
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (!AIDebugViewInputComponent)
	{
		AIDebugViewInputComponent = ConstructObject<UInputComponent>(UInputComponent::StaticClass(), GetOwner(), TEXT("AIDebugViewInputComponent0"));
		AIDebugViewInputComponent->RegisterComponent();

		AIDebugViewInputComponent->BindKey(EKeys::NumPadZero, IE_Pressed, this, &UGameplayDebuggingControllerComponent::ToggleAIDebugView_SetView0);
		AIDebugViewInputComponent->BindKey(EKeys::NumPadOne, IE_Pressed, this, &UGameplayDebuggingControllerComponent::ToggleAIDebugView_SetView1);
		AIDebugViewInputComponent->BindKey(EKeys::NumPadTwo, IE_Pressed, this, &UGameplayDebuggingControllerComponent::ToggleAIDebugView_SetView2);
		AIDebugViewInputComponent->BindKey(EKeys::NumPadThree, IE_Pressed, this, &UGameplayDebuggingControllerComponent::ToggleAIDebugView_SetView3);
		AIDebugViewInputComponent->BindKey(EKeys::NumPadFour, IE_Pressed, this, &UGameplayDebuggingControllerComponent::ToggleAIDebugView_SetView4);
		AIDebugViewInputComponent->BindKey(EKeys::NumPadFive, IE_Pressed, this, &UGameplayDebuggingControllerComponent::ToggleAIDebugView_SetView5);
		AIDebugViewInputComponent->BindKey(EKeys::NumPadSix, IE_Pressed, this, &UGameplayDebuggingControllerComponent::ToggleAIDebugView_SetView6);
		AIDebugViewInputComponent->BindKey(EKeys::NumPadSeven, IE_Pressed, this, &UGameplayDebuggingControllerComponent::ToggleAIDebugView_SetView7);
		AIDebugViewInputComponent->BindKey(EKeys::NumPadEight, IE_Pressed, this, &UGameplayDebuggingControllerComponent::ToggleAIDebugView_SetView8);
		AIDebugViewInputComponent->BindKey(EKeys::NumPadNine, IE_Pressed, this, &UGameplayDebuggingControllerComponent::ToggleAIDebugView_SetView9);
		AIDebugViewInputComponent->BindKey(EKeys::Add, IE_Released, this, &UGameplayDebuggingControllerComponent::NextEQSQuery);
#if USE_ALTERNATIVE_KEYS
		AIDebugViewInputComponent->BindKey(FInputChord(EKeys::Zero, false, false, true, false), IE_Pressed, this, &UGameplayDebuggingControllerComponent::ToggleAIDebugView_SetView0);
		AIDebugViewInputComponent->BindKey(FInputChord(EKeys::One, false, false, true, false), IE_Pressed, this, &UGameplayDebuggingControllerComponent::ToggleAIDebugView_SetView1);
		AIDebugViewInputComponent->BindKey(FInputChord(EKeys::Two, false, false, true, false), IE_Pressed, this, &UGameplayDebuggingControllerComponent::ToggleAIDebugView_SetView2);
		AIDebugViewInputComponent->BindKey(FInputChord(EKeys::Three, false, false, true, false), IE_Pressed, this, &UGameplayDebuggingControllerComponent::ToggleAIDebugView_SetView3);
		AIDebugViewInputComponent->BindKey(FInputChord(EKeys::Four, false, false, true, false), IE_Pressed, this, &UGameplayDebuggingControllerComponent::ToggleAIDebugView_SetView4);
		AIDebugViewInputComponent->BindKey(FInputChord(EKeys::Five, false, false, true, false), IE_Pressed, this, &UGameplayDebuggingControllerComponent::ToggleAIDebugView_SetView5);
		AIDebugViewInputComponent->BindKey(FInputChord(EKeys::Six, false, false, true, false), IE_Pressed, this, &UGameplayDebuggingControllerComponent::ToggleAIDebugView_SetView6);
		AIDebugViewInputComponent->BindKey(FInputChord(EKeys::Seven, false, false, true, false), IE_Pressed, this, &UGameplayDebuggingControllerComponent::ToggleAIDebugView_SetView7);
		AIDebugViewInputComponent->BindKey(FInputChord(EKeys::Eight, false, false, true, false), IE_Pressed, this, &UGameplayDebuggingControllerComponent::ToggleAIDebugView_SetView8);
		AIDebugViewInputComponent->BindKey(FInputChord(EKeys::Nine, false, false, true, false), IE_Pressed, this, &UGameplayDebuggingControllerComponent::ToggleAIDebugView_SetView9);
		AIDebugViewInputComponent->BindKey(FInputChord(EKeys::Equals, false, false, true, false), IE_Released, this, &UGameplayDebuggingControllerComponent::NextEQSQuery);
#endif
	}
	
	if (PlayerOwner.IsValid())
	{
		PlayerOwner->PushInputComponent(AIDebugViewInputComponent);
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
	APawn* Pawn = PlayerOwner.IsValid() ? PlayerOwner->GetPawn() : NULL;

	if (PlayerOwner.IsValid() && Pawn && bToolActivated && GetDebuggingReplicator())
	{
		if (UGameplayDebuggingComponent* OwnerComp = GetDebuggingReplicator() ? GetDebuggingReplicator()->GetDebugComponent() : NULL)
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

void UGameplayDebuggingControllerComponent::NextEQSQuery()
{
	if (IsViewActive(EAIDebugDrawDataView::EQS))
	{
		GetDebuggingReplicator()->OnChangeEQSQuery.Broadcast();
	}
}

#ifdef DEFAULT_TOGGLE_HANDLER
# undef DEFAULT_TOGGLE_HANDLER
#endif

void UGameplayDebuggingControllerComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (GetWorld()->GetTimeSeconds() - ControlKeyPressedTime > KeyPressActivationTime && !bToolActivated)
	{
		OpenDebugTool();
	}

	if (!bToolActivated)
	{
		return;
	}
}

float UGameplayDebuggingControllerComponent::GetUpdateNavMeshTimeRemaining() const
{
	return GetWorld()->GetTimerManager().GetTimerRemaining(TimerHandle_UpdateNavMeshTimer);
}

void UGameplayDebuggingControllerComponent::UpdateNavMeshTimer()
{
	const APawn* PlayerPawn = PlayerOwner.IsValid() ? PlayerOwner->GetPawn() : NULL;
	UGameplayDebuggingComponent* DebuggingComponent = GetDebuggingReplicator() ? GetDebuggingReplicator()->GetDebugComponent() : NULL;
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
}
