// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
 
#include "Widgets/Layout/SScrollBar.h"
#include "Widgets/Layout/SScrollBarTrack.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"

/**
 * Construct this widget
 *
 * @param	InArgs	The declaration data for this widget
 */
void SScrollBar::Construct(const FArguments& InArgs)
{
	OnUserScrolled = InArgs._OnUserScrolled;
	Orientation = InArgs._Orientation;
	UserVisibility = InArgs._Visibility;

	check(InArgs._Style);
	SetStyle(InArgs._Style);

	EHorizontalAlignment HorizontalAlignment = Orientation == Orient_Vertical ? HAlign_Center : HAlign_Fill;
	EVerticalAlignment VerticalAlignment = Orientation == Orient_Vertical ? VAlign_Fill : VAlign_Center;

	SBorder::Construct( SBorder::FArguments()
		.BorderImage(FCoreStyle::Get().GetBrush("NoBorder"))
		.BorderBackgroundColor( this, &SScrollBar::GetTrackOpacity )
		.ColorAndOpacity( this, &SScrollBar::GetThumbOpacity )
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.FillHeight( 1 )
			[
				SNew(SBorder)
				.BorderImage(BackgroundBrush)
				.HAlign(HorizontalAlignment)
				.VAlign(VerticalAlignment)
				.Padding(0)
				[
					SAssignNew(Track, SScrollBarTrack)
					.Orientation(InArgs._Orientation)
					.TopSlot()
					[
						SNew(SBox)
						.HAlign(HorizontalAlignment)
						.VAlign(VerticalAlignment)
						[
							SNew(SImage)
							.Image(TopBrush)
						]
					]
					.ThumbSlot()
					[
						SAssignNew(DragThumb, SBorder)
						.BorderImage( this, &SScrollBar::GetDragThumbImage )
						.HAlign(HAlign_Center)
						.VAlign(VAlign_Center)
						[
							SAssignNew(ThicknessSpacer, SSpacer)
							.Size(InArgs._Thickness)
						]
					]
					.BottomSlot()
					[
						SNew(SBox)
						.HAlign(HorizontalAlignment)
						.VAlign(VerticalAlignment)
						[
							SNew(SImage)
							.Image(BottomBrush)
						]
					]
				]
			]
		]
	);

	this->EnabledState = TAttribute<bool>( Track.ToSharedRef(), &SScrollBarTrack::IsNeeded );
	SetScrollBarAlwaysVisible(InArgs._AlwaysShowScrollbar);
}

void SScrollBar::SetOnUserScrolled( const FOnUserScrolled& InHandler )
{
	OnUserScrolled = InHandler;
}

void SScrollBar::SetState( float InOffsetFraction, float InThumbSizeFraction )
{
	// Note that the maximum offset depends on how many items fit per screen
	// It is 1.0f-InThumbSizeFraction.
	Track->SetSizes( InOffsetFraction, InThumbSizeFraction );
}

/**
 * The system calls this method to notify the widget that a mouse button was pressed within it. This event is bubbled.
 *
 * @param MyGeometry The Geometry of the widget receiving the event
 * @param MouseEvent Information about the input event
 *
 * @return Whether the event was handled along with possible requests for the system to take action.
 */
FReply SScrollBar::OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	if ( MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton )
	{
		FGeometry ThumbGeometry = this->FindChildGeometry(MyGeometry, DragThumb.ToSharedRef());

		if( DragThumb->IsHovered() )
		{
			// Clicking on the scrollbar drag thumb
			if( Orientation == Orient_Horizontal )
			{
				DragGrabOffset = ThumbGeometry.AbsoluteToLocal( MouseEvent.GetScreenSpacePosition() ).X;
			}
			else
			{
				DragGrabOffset = ThumbGeometry.AbsoluteToLocal( MouseEvent.GetScreenSpacePosition() ).Y;
			}

			bDraggingThumb = true;
		}
		else if( OnUserScrolled.IsBound() )
		{
			// Clicking in the non drag thumb area of the scrollbar
			DragGrabOffset = Orientation == Orient_Horizontal ? (ThumbGeometry.Size.X * 0.5f) : (ThumbGeometry.Size.Y * 0.5f);
			bDraggingThumb = true;

			ExecuteOnUserScrolled( MyGeometry, MouseEvent );
		}
	}

	if( bDraggingThumb )
	{
		return FReply::Handled().CaptureMouse(AsShared()).SetUserFocus(AsShared(), EFocusCause::Mouse);
	}
	else
	{
		return FReply::Unhandled();
	}
}

/**
 * The system calls this method to notify the widget that a mouse button was release within it. This event is bubbled.
 *
 * @param MyGeometry The Geometry of the widget receiving the event
 * @param MouseEvent Information about the input event
 *
 * @return Whether the event was handled along with possible requests for the system to take action.
 */
FReply SScrollBar::OnMouseButtonUp( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	if ( MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton )
	{
		bDraggingThumb = false;
		return FReply::Handled().ReleaseMouseCapture();
	}
	else
	{
		return FReply::Unhandled();
	}
}

/**
 * The system calls this method to notify the widget that a mouse moved within it. This event is bubbled.
 *
 * @param MyGeometry The Geometry of the widget receiving the event
 * @param MouseEvent Information about the input event
 *
 * @return Whether the event was handled along with possible requests for the system to take action.
 */
FReply SScrollBar::OnMouseMove( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	if ( this->HasMouseCapture() && !MouseEvent.GetCursorDelta().IsZero() )
	{
		if ( OnUserScrolled.IsBound() )
		{
			ExecuteOnUserScrolled( MyGeometry, MouseEvent );
		}
		return FReply::Handled();
	}
	else
	{
		return FReply::Unhandled();
	}
}

void SScrollBar::ExecuteOnUserScrolled( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	const int32 AxisId = (Orientation == Orient_Horizontal) ? 0 : 1;
	const FGeometry TrackGeometry = FindChildGeometry( MyGeometry, Track.ToSharedRef() );
	const float UnclampedOffsetInTrack = TrackGeometry.AbsoluteToLocal( MouseEvent.GetScreenSpacePosition() )[ AxisId ] - DragGrabOffset;
	const float TrackSizeBiasedByMinThumbSize = TrackGeometry.Size[ AxisId ] - Track->GetMinThumbSize();
	const float ThumbOffsetInTrack = FMath::Clamp( UnclampedOffsetInTrack, 0.0f, TrackSizeBiasedByMinThumbSize );
	const float ThumbOffset = ThumbOffsetInTrack / TrackSizeBiasedByMinThumbSize;
	OnUserScrolled.ExecuteIfBound( ThumbOffset );
}

bool SScrollBar::IsNeeded() const
{
	return Track->IsNeeded();
}

float SScrollBar::DistanceFromTop() const
{
	return Track->DistanceFromTop();
}

float SScrollBar::DistanceFromBottom() const
{
	return Track->DistanceFromBottom();
}

SScrollBar::SScrollBar()
: bDraggingThumb(false)
, DragGrabOffset( 0.0f )
{

}


FSlateColor SScrollBar::GetTrackOpacity() const
{
	if ( bDraggingThumb || this->IsHovered() )
	{
		return FLinearColor(1,1,1,1);
	}
	else
	{
		return FLinearColor(1,1,1,0.0f);
	}
}


FLinearColor SScrollBar::GetThumbOpacity() const
{
	if ( bDraggingThumb || this->IsHovered() )
	{
		return FLinearColor(1,1,1,1);
	}
	else
	{
		return FLinearColor(1,1,1,0.75f);
	}
}


const FSlateBrush* SScrollBar::GetDragThumbImage() const
{
	if ( bDraggingThumb )
	{
		return DraggedThumbImage;
	}
	else if (DragThumb->IsHovered())
	{
		return HoveredThumbImage;
	}
	else
	{
		return NormalThumbImage;
	}
}


EVisibility SScrollBar::ShouldBeVisible() const
{
	if ( this->HasMouseCapture() )
	{
		return EVisibility::Visible;
	}
	else if( Track->IsNeeded() )
	{
		return UserVisibility.Get();
	}
	else
	{
		return EVisibility::Collapsed;
	}
}

bool SScrollBar::IsScrolling() const
{
	return bDraggingThumb;
}

EOrientation SScrollBar::GetOrientation() const
{
	return Orientation;
}

void SScrollBar::SetStyle(const FScrollBarStyle* InStyle)
{
	const FScrollBarStyle* Style = InStyle;

	if (Style == nullptr)
	{
		FArguments Defaults;
		Style = Defaults._Style;
	}

	check(Style);

	NormalThumbImage = &Style->NormalThumbImage;
	HoveredThumbImage = &Style->HoveredThumbImage;
	DraggedThumbImage = &Style->DraggedThumbImage;

	if (Orientation == Orient_Vertical)
	{
		BackgroundBrush = &Style->VerticalBackgroundImage;
		TopBrush = &Style->VerticalTopSlotImage;
		BottomBrush = &Style->VerticalBottomSlotImage;
	}
	else
	{
		BackgroundBrush = &Style->HorizontalBackgroundImage;
		TopBrush = &Style->HorizontalTopSlotImage;
		BottomBrush = &Style->HorizontalBottomSlotImage;
	}
}

void SScrollBar::SetThickness(TAttribute<FVector2D> InThickness)
{
	ThicknessSpacer->SetSize(InThickness);
}

void SScrollBar::SetScrollBarAlwaysVisible(bool InAlwaysVisible)
{
	if ( InAlwaysVisible )
	{
		Visibility = EVisibility::Visible;
	}
	else
	{
		Visibility = TAttribute<EVisibility>(SharedThis(this), &SScrollBar::ShouldBeVisible);
	}
}
