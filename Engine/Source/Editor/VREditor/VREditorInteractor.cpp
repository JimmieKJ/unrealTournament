// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "VREditorModule.h"
#include "VREditorInteractor.h"
#include "VREditorWorldInteraction.h"

#include "VREditorMode.h"
#include "VREditorFloatingText.h"

#include "Interactables/VREditorButton.h"
#include "VREditorDockableWindow.h"

#define LOCTEXT_NAMESPACE "VREditor"

UVREditorInteractor::UVREditorInteractor( const FObjectInitializer& Initializer ) :
	Super( Initializer ),
	VRMode( nullptr ),
	bIsModifierPressed( false ),
	SelectAndMoveTriggerValue( 0.0f ),
	bHasUIInFront( false ),
	bHasUIOnForearm( false ),
	bIsClickingOnUI( false ),
	bIsRightClickingOnUI( false ),
	bIsHoveringOverUI( false ),
	UIScrollVelocity( 0.0f ),
	LastClickReleaseTime( 0.0f ),
	bWantHelpLabels( false ),
	HelpLabelShowOrHideStartTime( FTimespan::MinValue() )
{

}

UVREditorInteractor::~UVREditorInteractor()
{
	Shutdown();
}

void UVREditorInteractor::Init( FVREditorMode* InVRMode )
{
	VRMode = InVRMode;
	KeyToActionMap.Reset();
}

void UVREditorInteractor::Shutdown()
{
	for ( auto& KeyAndValue : HelpLabels )
	{
		AFloatingText* FloatingText = KeyAndValue.Value;
		GetVRMode().DestroyTransientActor( FloatingText );
	}

	HelpLabels.Empty();	

	VRMode = nullptr;

	Super::Shutdown();
}

void UVREditorInteractor::Tick( const float DeltaTime )
{
	Super::Tick( DeltaTime );
}

FHitResult UVREditorInteractor::GetHitResultFromLaserPointer( TArray<AActor*>* OptionalListOfIgnoredActors /*= nullptr*/, const bool bIgnoreGizmos /*= false*/,
	TArray<UClass*>* ObjectsInFrontOfGizmo /*= nullptr */, const bool bEvenIfBlocked /*= false */, const float LaserLengthOverride /*= 0.0f */ )
{
	static TArray<AActor*> IgnoredActors;
	IgnoredActors.Reset();
	if ( OptionalListOfIgnoredActors == nullptr )
	{
		OptionalListOfIgnoredActors = &IgnoredActors;
	}

	OptionalListOfIgnoredActors->Add( GetVRMode().GetAvatarMeshActor() );
	
	// Ignore UI widgets too
	if ( GetDraggingMode() == EViewportInteractionDraggingMode::ActorsAtLaserImpact )
	{
		for( TActorIterator<AVREditorFloatingUI> UIActorIt( WorldInteraction->GetViewportWorld() ); UIActorIt; ++UIActorIt )
		{
			OptionalListOfIgnoredActors->Add( *UIActorIt );
		}
	}

	static TArray<UClass*> PriorityOverGizmoObjects;
	PriorityOverGizmoObjects.Reset();
	if ( ObjectsInFrontOfGizmo == nullptr )
	{
		ObjectsInFrontOfGizmo = &PriorityOverGizmoObjects;
	}

	ObjectsInFrontOfGizmo->Add( AVREditorButton::StaticClass() );
	ObjectsInFrontOfGizmo->Add( AVREditorFloatingUI::StaticClass() );
	ObjectsInFrontOfGizmo->Add( AVREditorDockableWindow::StaticClass() );

	return UViewportInteractor::GetHitResultFromLaserPointer( OptionalListOfIgnoredActors, bIgnoreGizmos, ObjectsInFrontOfGizmo, bEvenIfBlocked, LaserLengthOverride );
}

void UVREditorInteractor::ResetHoverState( const float DeltaTime )
{
	bIsHoveringOverUI = false;
}

void UVREditorInteractor::OnStartDragging( UActorComponent* ClickedComponent, const FVector& HitLocation, const bool bIsPlacingActors )
{
	// If the user is holding down the modifier key, go ahead and duplicate the selection first.  This needs to
	// happen before we create our transformables, because duplicating actors will change which actors are
	// selected!  Don't do this if we're placing objects right now though.
	if ( bIsModifierPressed && !bIsPlacingActors )
	{
		WorldInteraction->Duplicate();
	}
}

float UVREditorInteractor::GetSlideDelta()
{
	return 0.0f;
}

bool UVREditorInteractor::IsHoveringOverUI() const
{
	return bIsHoveringOverUI;
}

void UVREditorInteractor::SetHasUIInFront( const bool bInHasUIInFront )
{
	bHasUIInFront = bInHasUIInFront;
}

bool UVREditorInteractor::HasUIInFront() const
{
	return bHasUIInFront;
}

void UVREditorInteractor::SetHasUIOnForearm( const bool bInHasUIOnForearm )
{
	bHasUIOnForearm = bInHasUIOnForearm;
}

bool UVREditorInteractor::HasUIOnForearm() const
{
	return bHasUIOnForearm;
}

UWidgetComponent* UVREditorInteractor::GetHoveringOverWidgetComponent() const
{
	return InteractorData.HoveringOverWidgetComponent;
}

void UVREditorInteractor::SetHoveringOverWidgetComponent( UWidgetComponent* NewHoveringOverWidgetComponent )
{
	InteractorData.HoveringOverWidgetComponent = NewHoveringOverWidgetComponent;
}

bool UVREditorInteractor::IsModifierPressed() const
{
	return bIsModifierPressed;
}

void UVREditorInteractor::SetIsClickingOnUI( const bool bInIsClickingOnUI )
{
	bIsClickingOnUI = bInIsClickingOnUI;
}

bool UVREditorInteractor::IsClickingOnUI() const
{
	return bIsClickingOnUI;
}

void UVREditorInteractor::SetIsHoveringOverUI( const bool bInIsHoveringOverUI )
{
	bIsHoveringOverUI = bInIsHoveringOverUI;
}

void UVREditorInteractor::SetIsRightClickingOnUI( const bool bInIsRightClickingOnUI )
{
	bIsRightClickingOnUI = bInIsRightClickingOnUI;
}

bool UVREditorInteractor::IsRightClickingOnUI() const
{
	return bIsRightClickingOnUI;
}

void UVREditorInteractor::SetLastClickReleaseTime( const double InLastClickReleaseTime )
{
	LastClickReleaseTime = InLastClickReleaseTime;
}

double UVREditorInteractor::GetLastClickReleaseTime() const
{
	return LastClickReleaseTime;
}

void UVREditorInteractor::SetUIScrollVelocity( const float InUIScrollVelocity )
{
	UIScrollVelocity = InUIScrollVelocity;
}

float UVREditorInteractor::GetUIScrollVelocity() const
{
	return UIScrollVelocity;
}

float UVREditorInteractor::GetSelectAndMoveTriggerValue() const
{
	return SelectAndMoveTriggerValue;
}

bool UVREditorInteractor::GetIsLaserBlocked()
{
	return bHasUIInFront;
}

#undef LOCTEXT_NAMESPACE