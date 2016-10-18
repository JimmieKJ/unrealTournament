// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "VREditorModule.h"
#include "VREditorRadialMenu.h"
#include "VREditorRadialMenuItem.h"
#include "VREditorFloatingUI.h"
#include "VREditorUISystem.h"
#include "VREditorMode.h"
#include "VREditorWorldInteraction.h"
#include "VREditorTransformGizmo.h"
#include "VREditorInteractor.h"
#include "VREditorMotionControllerInteractor.h"

#include "Editor/LevelEditor/Public/LevelEditor.h"

#define LOCTEXT_NAMESPACE "VREditorRadialMenu"

namespace VREd
{
	static FAutoConsoleVariable RadialMenuRiftSelectionRadius( TEXT( "VREd.RadialMenuRiftSelectionRadius" ), 0.3f, TEXT( "The radius that is used for selecting buttons on the radial menu on Oculus Rift" ) );
	static FAutoConsoleVariable RadialMenuViveSelectionRadius( TEXT( "VREd.RadialMenuViveSelectionRadius" ), 0.3f, TEXT( "The radius that is used for selecting buttons on the radial menu on Vive" ) );
}

UVREditorRadialMenu::UVREditorRadialMenu( const FObjectInitializer& ObjectInitializer )
: Super( ObjectInitializer ),
CurrentItem( nullptr ),
PreviousItem( nullptr )
{
}


void UVREditorRadialMenu::Update( const UVREditorInteractor* Interactor )
{
	const UVREditorMotionControllerInteractor* MotionControllerInteractor = Cast< UVREditorMotionControllerInteractor >( Interactor );
	if( MotionControllerInteractor )
	{
		TrackpadPosition = MotionControllerInteractor->GetTrackpadPosition();

		if ( !GetOwner()->bHidden )
		{
			// Check if position is from the center
			if ( !IsInMenuRadius() )
			{
				const float AnglePerItem = 360 / MenuItems.Num();
				float Angle = FRotator::NormalizeAxis( FMath::RadiansToDegrees( FMath::Atan2( TrackpadPosition.X, TrackpadPosition.Y ) ) );
				TrackpadAngle = Angle;

				Angle += AnglePerItem / 2.0f;
				if ( Angle < 0 )
				{
					Angle = Angle + 360;
				}

				const int32 Index = ( Angle / AnglePerItem );
				UVREditorRadialMenuItem* NewMenuItem = MenuItems[ Index ];
				if ( NewMenuItem )
				{
					CurrentItem = NewMenuItem;
				}

				// Update the visuals
				if ( CurrentItem != PreviousItem )
				{
					if ( CurrentItem )
					{
						CurrentItem->OnEnterHover();
					}

					if ( PreviousItem )
					{
						PreviousItem->OnLeaveHover();
					}
				}

				PreviousItem = CurrentItem;
			}
			else
			{
				if ( CurrentItem )
				{
					CurrentItem->OnLeaveHover();
					CurrentItem = nullptr;
					PreviousItem = nullptr;
				}
			}
		}
	}
}

void UVREditorRadialMenu::SetButtons( UVREditorRadialMenuItem* InitTopItem, UVREditorRadialMenuItem* InitTopRightItem, UVREditorRadialMenuItem* InitRightItem, UVREditorRadialMenuItem* InitBottomRightItem,
	UVREditorRadialMenuItem* InitBottomItem, UVREditorRadialMenuItem* InitLeftBottomItem, UVREditorRadialMenuItem* InitLeftItem, UVREditorRadialMenuItem* InitTopLeftItem )
{
	InitTopItem->OnPressedDelegate.AddUObject( this, &UVREditorRadialMenu::OnGizmoCycle );
	MenuItems.Add( InitTopItem );
	UpdateGizmoTypeLabel();

	InitTopRightItem->OnPressedDelegate.AddUObject( this, &UVREditorRadialMenu::OnCopyButtonClicked );
	InitTopRightItem->SetLabel( FString( "Copy" ) );
	MenuItems.Add( InitTopRightItem );

	InitRightItem->OnPressedDelegate.AddUObject( this, &UVREditorRadialMenu::OnPasteButtonClicked );
	InitRightItem->SetLabel( FString( "Paste" ) );
	MenuItems.Add( InitRightItem );

	InitBottomRightItem->OnPressedDelegate.AddUObject( this, &UVREditorRadialMenu::OnRedoButtonClicked );
	InitBottomRightItem->SetLabel( FString( "Redo" ) );
	MenuItems.Add( InitBottomRightItem );

	InitBottomItem->OnPressedDelegate.AddUObject( this, &UVREditorRadialMenu::OnSnapActorsToGroundClicked );
	InitBottomItem->SetLabel( FString( "Snap To Ground" ) );
	MenuItems.Add( InitBottomItem );

	InitLeftBottomItem->OnPressedDelegate.AddUObject( this, &UVREditorRadialMenu::OnUndoButtonClicked );
	InitLeftBottomItem->SetLabel( FString( "Undo" ) );
	MenuItems.Add( InitLeftBottomItem );

	InitLeftItem->OnPressedDelegate.AddUObject( this, &UVREditorRadialMenu::OnDuplicateButtonClicked );
	InitLeftItem->SetLabel( FString( "Duplicate" ) );
	MenuItems.Add( InitLeftItem );

	InitTopLeftItem->OnPressedDelegate.AddUObject( this, &UVREditorRadialMenu::OnDeleteButtonClicked );
	InitTopLeftItem->SetLabel( FString( "Delete" ) );
	MenuItems.Add( InitTopLeftItem );
}

void UVREditorRadialMenu::ResetItem()
{
	if (PreviousItem)
	{
		PreviousItem->OnLeaveHover();
	}

	if (CurrentItem)
	{
		CurrentItem->OnLeaveHover();
	}
}

void UVREditorRadialMenu::SelectCurrentItem()
{
	if (IsInMenuRadius())
	{
		GetOwner()->GetOwner().TogglePanelsVisibility();
	}
	else if (CurrentItem)
	{
		CurrentItem->OnPressed();
	}
}

bool UVREditorRadialMenu::IsInMenuRadius() const
{
	const float RadialMenuSelectionRadius = Owner->GetOwner().GetOwner().GetHMDDeviceType() == EHMDDeviceType::DT_OculusRift ? VREd::RadialMenuRiftSelectionRadius->GetFloat() : VREd::RadialMenuViveSelectionRadius->GetFloat();
	return FVector2D::Distance( FVector2D::ZeroVector, TrackpadPosition ) < RadialMenuSelectionRadius;
}

void UVREditorRadialMenu::UpdateGizmoTypeLabel()
{
	FString Result;

	switch (GetOwner()->GetOwner().GetOwner().GetWorldInteraction().GetCurrentGizmoType())
	{
	case EGizmoHandleTypes::All:
		Result = LOCTEXT( "AllGizmoType", "Universal Gizmo" ).ToString();
		break;
	case EGizmoHandleTypes::Translate:
		Result = LOCTEXT( "TranslateGizmoType", "Translate Gizmo" ).ToString();
		break;
	case EGizmoHandleTypes::Rotate:
		Result = LOCTEXT( "RotateGizmoType", "Rotate Gizmo" ).ToString();
		break;
	case EGizmoHandleTypes::Scale:
		Result = LOCTEXT( "ScaleGizmoType", "Scale Gizmo" ).ToString();
		break;
	default:
		break;
	}

	MenuItems[0]->SetLabel( Result );
}

void UVREditorRadialMenu::OnGizmoCycle()
{
	GetOwner()->GetOwner().GetOwner().CycleTransformGizmoHandleType();
	UpdateGizmoTypeLabel();
}

void UVREditorRadialMenu::OnDuplicateButtonClicked()
{
	GetOwner()->GetOwner().GetOwner().GetWorldInteraction().Duplicate();
}


void UVREditorRadialMenu::OnDeleteButtonClicked()
{
	GetOwner()->GetOwner().GetOwner().GetWorldInteraction().DeleteSelectedObjects();
}


void UVREditorRadialMenu::OnUndoButtonClicked()
{
	GetOwner()->GetOwner().GetOwner().GetWorldInteraction().Undo();
}


void UVREditorRadialMenu::OnRedoButtonClicked()
{
	GetOwner()->GetOwner().GetOwner().GetWorldInteraction().Redo();
}


void UVREditorRadialMenu::OnCopyButtonClicked()
{
	GetOwner()->GetOwner().GetOwner().GetWorldInteraction().Copy();
}


void UVREditorRadialMenu::OnPasteButtonClicked()
{
	GetOwner()->GetOwner().GetOwner().GetWorldInteraction().Paste();
}

void UVREditorRadialMenu::OnSnapActorsToGroundClicked()
{
	UVREditorWorldInteraction* WorldInteraction = Cast<UVREditorWorldInteraction>( &( GetOwner()->GetOwner().GetOwner().GetWorldInteraction() ) );
	if ( WorldInteraction )
	{
		WorldInteraction->SnapSelectedActorsToGround();
	}
}


#undef LOCTEXT_NAMESPACE
