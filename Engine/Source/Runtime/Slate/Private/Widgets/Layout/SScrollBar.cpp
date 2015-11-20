// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
 
#include "SlatePrivatePCH.h"


static const int32 NUM_SCROLLBAR_SLOTS = 3;


/** Arrange 3 widgets: the top track, bottom track, and thumb. */
class SScrollBarTrack : public SPanel
{
public:

	/** A ListPanel slot is very simple - it just stores a widget. */
	class FSlot : public TSlotBase<FSlot>
	{
	public:
		FSlot()
		: TSlotBase<FSlot>()
		{}
	};

	SLATE_BEGIN_ARGS( SScrollBarTrack )
		: _TopSlot()
		, _ThumbSlot()
		, _BottomSlot()
		, _Orientation(Orient_Vertical)
		{
			_Visibility = EVisibility::SelfHitTestInvisible;
		}

		SLATE_NAMED_SLOT( FArguments, TopSlot)
		SLATE_NAMED_SLOT( FArguments, ThumbSlot)
		SLATE_NAMED_SLOT( FArguments, BottomSlot)
		SLATE_ARGUMENT( EOrientation, Orientation )

	SLATE_END_ARGS()

	SScrollBarTrack()
	: Children()
	{
	}

	/**
	 * Construct the widget from a Declaration
	 *
	 * @param InArgs  Declaration from which to construct the widget.
	 */
	void Construct( const FArguments& InArgs)
	{
		OffsetFraction = 0;
		ThumbSizeFraction = 0;
		MinThumbSize = 35;
		Orientation = InArgs._Orientation;

		// This panel only supports 3 children!
		for (int32 CurChild=0; CurChild < NUM_SCROLLBAR_SLOTS; ++CurChild)
		{
			Children.Add( new FSlot() );
		}

		Children[ TOP_SLOT_INDEX ]
		[
			InArgs._TopSlot.Widget
		];
		
		Children[ BOTTOM_SLOT_INDEX ]
		[
			InArgs._BottomSlot.Widget
		];
		
		Children[ THUMB_SLOT_INDEX ]
		[
			InArgs._ThumbSlot.Widget
		];
	}

	struct FTrackSizeInfo
	{
		FTrackSizeInfo( const FGeometry& TrackGeometry, EOrientation InOrientation, float InMinThumbSize, float ThumbSizeAsFractionOfTrack, float ThumbOffsetAsFractionOfTrack )
		{
			BiasedTrackSize = ((InOrientation == Orient_Horizontal) ? TrackGeometry.Size.X : TrackGeometry.Size.Y) - InMinThumbSize;
			const float AccurateThumbSize = ThumbSizeAsFractionOfTrack * (BiasedTrackSize);
			ThumbStart = BiasedTrackSize * ThumbOffsetAsFractionOfTrack;
			ThumbSize = InMinThumbSize + AccurateThumbSize;
			
		}

		float BiasedTrackSize;
		float ThumbStart;
		float ThumbSize;
		float GetThumbEnd()
		{
			return ThumbStart + ThumbSize;
		}
	};

	FTrackSizeInfo GetTrackSizeInfo( const FGeometry& InTrackGeometry ) const
	{
		return FTrackSizeInfo( InTrackGeometry, Orientation, MinThumbSize, this->ThumbSizeFraction, this->OffsetFraction );
	}

	/**
	 * Panels arrange their children in a space described by the AllottedGeometry parameter. The results of the arrangement
	 * should be returned by appending a FArrangedWidget pair for every child widget. See StackPanel for an example
	 *
	 * @param AllottedGeometry    The geometry allotted for this widget by its parent.
	 * @param ArrangedChildren    The array to which to add the WidgetGeometries that represent the arranged children.
	 */
	virtual void OnArrangeChildren( const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren ) const
	{
		const float Width = AllottedGeometry.Size.X;
		const float Height = AllottedGeometry.Size.Y;
		
		// We only need to show all three children when the thumb is visible, otherwise we only need to show the track
		if ( IsNeeded() )
		{
			FTrackSizeInfo TrackSizeInfo = this->GetTrackSizeInfo(AllottedGeometry);

			// Arrange top half of the track
			FVector2D ChildSize = (Orientation == Orient_Horizontal)
				? FVector2D(TrackSizeInfo.ThumbStart, Height)
				: FVector2D(Width, TrackSizeInfo.ThumbStart);

			FVector2D ChildPos(0, 0);
			ArrangedChildren.AddWidget(			
				AllottedGeometry.MakeChild( Children[TOP_SLOT_INDEX].GetWidget(), ChildPos, ChildSize )
			);

			// Arrange bottom half of the track
			ChildPos = (Orientation == Orient_Horizontal)
				? FVector2D(TrackSizeInfo.GetThumbEnd(), 0)
				: FVector2D(0, TrackSizeInfo.GetThumbEnd());

			ChildSize = (Orientation == Orient_Horizontal)
				? FVector2D(AllottedGeometry.Size.X - TrackSizeInfo.GetThumbEnd(), Height)
				: FVector2D(Width, AllottedGeometry.Size.Y - TrackSizeInfo.GetThumbEnd());

			ArrangedChildren.AddWidget( 
				AllottedGeometry.MakeChild( Children[BOTTOM_SLOT_INDEX].GetWidget(), ChildPos, ChildSize )
			);

			// Arrange the thumb
			ChildPos = (Orientation == Orient_Horizontal)
				? FVector2D(TrackSizeInfo.ThumbStart, 0)
				: FVector2D(0, TrackSizeInfo.ThumbStart);

			ChildSize = (Orientation == Orient_Horizontal)
				? FVector2D(TrackSizeInfo.ThumbSize, Height)
				: FVector2D(Width, TrackSizeInfo.ThumbSize);

			ArrangedChildren.AddWidget(
				AllottedGeometry.MakeChild( Children[THUMB_SLOT_INDEX].GetWidget(), ChildPos, ChildSize )
			);
		}
		else
		{
			// No thumb is visible, so just show the top half of the track at the current width/height
			ArrangedChildren.AddWidget(			
				AllottedGeometry.MakeChild( Children[TOP_SLOT_INDEX].GetWidget(), FVector2D(0, 0), FVector2D(Width, Height) )
			);
		}

	}
	
	/**
	 * A Panel's desired size in the space required to arrange of its children on the screen while respecting all of
	 * the children's desired sizes and any layout-related options specified by the user. See StackPanel for an example.
	 *
	 * @return The desired size.
	 */
	virtual FVector2D ComputeDesiredSize(float) const
	{
		if ( Orientation == Orient_Horizontal )
		{
			const float DesiredHeight = FMath::Max3( Children[0].GetWidget()->GetDesiredSize().Y, Children[1].GetWidget()->GetDesiredSize().Y, Children[2].GetWidget()->GetDesiredSize().Y );
			return FVector2D( MinThumbSize, DesiredHeight );
		}
		else
		{
			const float DesiredWidth = FMath::Max3( Children[0].GetWidget()->GetDesiredSize().X, Children[1].GetWidget()->GetDesiredSize().X, Children[2].GetWidget()->GetDesiredSize().X );
			return FVector2D( DesiredWidth, MinThumbSize );
		}
	}
	
	/** @return  The children of a panel in a slot-agnostic way. */
	virtual FChildren* GetChildren()
	{
		return &Children;
	}

	void SetSizes( float InThumbOffsetFraction, float InThumbSizeFraction )
	{
		OffsetFraction = InThumbOffsetFraction;
		ThumbSizeFraction = InThumbSizeFraction;
	}

	bool IsNeeded() const
	{
		// We use a small epsilon here to avoid the scroll bar showing up when all of the content is already in view, due to
		// floating point precision when the scroll bar state is set
		return ThumbSizeFraction < ( 1.0f - KINDA_SMALL_NUMBER );
	}

	float DistanceFromTop() const
	{
		return OffsetFraction;
	}

	float DistanceFromBottom() const
	{
		return 1.0f - (OffsetFraction + ThumbSizeFraction);
	}

	float GetMinThumbSize() const
	{
		return MinThumbSize;
	}

	float GetThumbSizeFraction() const
	{
		return ThumbSizeFraction;
	}

protected:

	static const int32 TOP_SLOT_INDEX = 0;
	static const int32 BOTTOM_SLOT_INDEX = 1;
	static const int32 THUMB_SLOT_INDEX = 2;
	
	TPanelChildren< FSlot > Children;

	float OffsetFraction;
	float ThumbSizeFraction;
	float MinThumbSize;
	EOrientation Orientation;
};


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

void SScrollBar::SetStyle(const FScrollBarStyle* InStyle)
{
	check(InStyle);

	NormalThumbImage = &InStyle->NormalThumbImage;
	HoveredThumbImage = &InStyle->HoveredThumbImage;
	DraggedThumbImage = &InStyle->DraggedThumbImage;

	if (Orientation == Orient_Vertical)
	{
		BackgroundBrush = &InStyle->VerticalBackgroundImage;
		TopBrush = &InStyle->VerticalTopSlotImage;
		BottomBrush = &InStyle->VerticalBottomSlotImage;
	}
	else
	{
		BackgroundBrush = &InStyle->HorizontalBackgroundImage;
		TopBrush = &InStyle->HorizontalTopSlotImage;
		BottomBrush = &InStyle->HorizontalBottomSlotImage;
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
