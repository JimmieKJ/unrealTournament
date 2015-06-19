// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SlatePrivatePCH.h"
#include "LayoutUtils.h"


SScrollBox::FSlot& SScrollBox::Slot()
{
	return *(new SScrollBox::FSlot());
}


class SScrollPanel : public SPanel
{
public:
	
	SLATE_BEGIN_ARGS(SScrollPanel)
		{
			_Visibility = EVisibility::SelfHitTestInvisible;
		}

		SLATE_ARGUMENT(EOrientation, Orientation)

	SLATE_END_ARGS()

	SScrollPanel()
	: Children()
	{
	}

	void Construct( const FArguments& InArgs, const TArray<SScrollBox::FSlot*>& InSlots )
	{
		PhysicalOffset = 0;
		Children.Reserve( InSlots.Num() );
		for ( int32 SlotIndex=0; SlotIndex < InSlots.Num(); ++SlotIndex )
		{
			Children.Add( InSlots[ SlotIndex ] );
		}
		Orientation = InArgs._Orientation;
	}

public:

	EOrientation GetOrientation()
	{
		return Orientation;
	}

	void SetOrientation(EOrientation InOrientation)
	{
		Orientation = InOrientation;
	}

	virtual void OnArrangeChildren( const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren ) const override
	{
		float CurChildOffset = -PhysicalOffset;

		for( int32 SlotIndex=0; SlotIndex < Children.Num(); ++SlotIndex )
		{
			const SScrollBox::FSlot& ThisSlot = Children[SlotIndex];
			const EVisibility ChildVisibility = ThisSlot.GetWidget()->GetVisibility();

			if ( ChildVisibility != EVisibility::Collapsed )
			{
				if (Orientation == Orient_Vertical)
				{
					CurChildOffset = ArrangeChildVerticalAndReturnOffset(AllottedGeometry, ArrangedChildren, ThisSlot, CurChildOffset);
				}
				else
				{
					CurChildOffset = ArrangeChildHorizontalAndReturnOffset(AllottedGeometry, ArrangedChildren, ThisSlot, CurChildOffset);
				}
			}
		}
	}


	virtual FVector2D ComputeDesiredSize(float) const override
	{
		FVector2D ThisDesiredSize = FVector2D::ZeroVector;
		for(int32 SlotIndex=0; SlotIndex < Children.Num(); ++SlotIndex )
		{
			const SScrollBox::FSlot& ThisSlot = Children[SlotIndex];
			if (ThisSlot.GetWidget()->GetVisibility() != EVisibility::Collapsed)
			{
				const FVector2D ChildDesiredSize = ThisSlot.GetWidget()->GetDesiredSize();
				if (Orientation == Orient_Vertical)
				{
					ThisDesiredSize.X = FMath::Max(ChildDesiredSize.X, ThisDesiredSize.X);
					ThisDesiredSize.Y += ChildDesiredSize.Y + ThisSlot.SlotPadding.Get().GetTotalSpaceAlong<Orient_Vertical>();
				}
				else
				{
					ThisDesiredSize.X += ChildDesiredSize.X + ThisSlot.SlotPadding.Get().GetTotalSpaceAlong<Orient_Horizontal>();
					ThisDesiredSize.Y = FMath::Max(ChildDesiredSize.Y, ThisDesiredSize.Y);
				}
			}
		}

		return ThisDesiredSize;
	}

	virtual FChildren* GetChildren() override
	{
		return &Children;
	}

	float PhysicalOffset;
	TPanelChildren<SScrollBox::FSlot> Children;	

private:
	
	float ArrangeChildVerticalAndReturnOffset(const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren, const SScrollBox::FSlot& ThisSlot, float CurChildOffset) const
	{
		const FMargin& ThisPadding = ThisSlot.SlotPadding.Get();
		const FVector2D& WidgetDesiredSize = ThisSlot.GetWidget()->GetDesiredSize();
		const float ThisSlotDesiredHeight = WidgetDesiredSize.Y + ThisPadding.GetTotalSpaceAlong<Orient_Vertical>();

		// Figure out the size and local position of the child within the slot.  There is no vertical alignment, because 
		// it does not make sense in a panel where items are stacked vertically end-to-end.
		AlignmentArrangeResult XAlignmentResult = AlignChild<Orient_Horizontal>(AllottedGeometry.Size.X, ThisSlot, ThisPadding);

		ArrangedChildren.AddWidget(AllottedGeometry.MakeChild(ThisSlot.GetWidget(), FVector2D(XAlignmentResult.Offset, CurChildOffset + ThisPadding.Top), FVector2D(XAlignmentResult.Size, WidgetDesiredSize.Y)));
		return CurChildOffset + ThisSlotDesiredHeight;
	}

	float ArrangeChildHorizontalAndReturnOffset(const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren, const SScrollBox::FSlot& ThisSlot, float CurChildOffset) const
	{
		const FMargin& ThisPadding = ThisSlot.SlotPadding.Get();
		const FVector2D& WidgetDesiredSize = ThisSlot.GetWidget()->GetDesiredSize();
		const float ThisSlotDesiredWidth = WidgetDesiredSize.X + ThisPadding.GetTotalSpaceAlong<Orient_Horizontal>();

		// Figure out the size and local position of the child within the slot.  There is no horizontal alignment, because
		// it doesn't make sense in a panel where items are stacked horizontally end-to-end.
		AlignmentArrangeResult YAlignmentResult = AlignChild<Orient_Vertical>(AllottedGeometry.Size.Y, ThisSlot, ThisPadding);

		ArrangedChildren.AddWidget(AllottedGeometry.MakeChild(ThisSlot.GetWidget(), FVector2D(CurChildOffset + ThisPadding.Left, YAlignmentResult.Offset), FVector2D(WidgetDesiredSize.X, YAlignmentResult.Size)));
		return CurChildOffset + ThisSlotDesiredWidth;
	}

private:

	EOrientation Orientation;
};



void SScrollBox::Construct( const FArguments& InArgs )
{
	check(InArgs._Style);

	Style = InArgs._Style;
	ScrollBarStyle = InArgs._ScrollBarStyle;
	DesiredScrollOffset = 0;
	bIsScrolling = false;
	bAnimateScroll = false;
	AmountScrolledWhileRightMouseDown = 0;
	bShowSoftwareCursor = false;
	SoftwareCursorPosition = FVector2D::ZeroVector;
	OnUserScrolled = InArgs._OnUserScrolled;
	Orientation = InArgs._Orientation;
	bScrollToEnd = false;
	bIsActiveTimerRegistered = false;
	ConsumeMouseWheel = InArgs._ConsumeMouseWheel;

	if (InArgs._ExternalScrollbar.IsValid())
	{
		// An external scroll bar was specified by the user
		ScrollBar = InArgs._ExternalScrollbar;
		ScrollBar->SetOnUserScrolled(FOnUserScrolled::CreateSP(this, &SScrollBox::ScrollBar_OnUserScrolled));
		bScrollBarIsExternal = true;
	}
	else
	{
		// Make a scroll bar 
		ScrollBar = ConstructScrollBar();
		ScrollBar->SetThickness(InArgs._ScrollBarThickness);
		ScrollBar->SetUserVisibility(InArgs._ScrollBarVisibility);
		ScrollBar->SetScrollBarAlwaysVisible(InArgs._ScrollBarAlwaysVisible);

		bScrollBarIsExternal = false;
	}

	SAssignNew(ScrollPanel, SScrollPanel, InArgs.Slots)
		.Orientation(Orientation);

	if (Orientation == Orient_Vertical)
	{
		ConstructVerticalLayout();
	}
	else
	{
		ConstructHorizontalLayout();
	}

	ScrollBar->SetState( 0.0f, 1.0f );
}

TSharedPtr<SScrollBar> SScrollBox::ConstructScrollBar()
{
	return TSharedPtr<SScrollBar>(SNew(SScrollBar)
		.Style(ScrollBarStyle)
		.Orientation(Orientation)
		.OnUserScrolled(this, &SScrollBox::ScrollBar_OnUserScrolled));
}

void SScrollBox::ConstructVerticalLayout()
{
	TSharedPtr<SHorizontalBox> PanelAndScrollbar;
	this->ChildSlot
		[
			SAssignNew(PanelAndScrollbar, SHorizontalBox)

			+ SHorizontalBox::Slot()
			.FillWidth(1)
			[
				SNew(SOverlay)

				+ SOverlay::Slot()
				.Padding(FMargin(0.0f, 0.0f, 0.0f, 1.0f))
				[
					// Scroll panel that presents the scrolled content
					ScrollPanel.ToSharedRef()
				]

				+ SOverlay::Slot()
					.HAlign(HAlign_Fill)
					.VAlign(VAlign_Top)
					[
						// Shadow: Hint to scroll up
						SNew(SImage)
						.Visibility(EVisibility::HitTestInvisible)
						.ColorAndOpacity(this, &SScrollBox::GetStartShadowOpacity)
						.Image(&Style->TopShadowBrush)
					]

				+ SOverlay::Slot()
					.HAlign(HAlign_Fill)
					.VAlign(VAlign_Bottom)
					[
						// Shadow: a hint to scroll down
						SNew(SImage)
						.Visibility(EVisibility::HitTestInvisible)
						.ColorAndOpacity(this, &SScrollBox::GetEndShadowOpacity)
						.Image(&Style->BottomShadowBrush)
					]
			]

		];

	if (!bScrollBarIsExternal)
	{
		PanelAndScrollbar->AddSlot()
			.AutoWidth()
			[
				ScrollBar.ToSharedRef()
			];
	}
}

void SScrollBox::ConstructHorizontalLayout()
{
	TSharedPtr<SVerticalBox> PanelAndScrollbar;
	this->ChildSlot
		[
			SAssignNew(PanelAndScrollbar, SVerticalBox)

			+ SVerticalBox::Slot()
			.FillHeight(1)
			[
				SNew(SOverlay)

				+ SOverlay::Slot()
				.Padding(FMargin(0.0f, 0.0f, 1.0f, 0.0f))
				[
					// Scroll panel that presents the scrolled content
					ScrollPanel.ToSharedRef()
				]

				+ SOverlay::Slot()
					.HAlign(HAlign_Left)
					.VAlign(VAlign_Fill)
					[
						// Shadow: Hint to left
						SNew(SImage)
						.Visibility(EVisibility::HitTestInvisible)
						.ColorAndOpacity(this, &SScrollBox::GetStartShadowOpacity)
						.Image(&Style->LeftShadowBrush)
					]

				+ SOverlay::Slot()
					.HAlign(HAlign_Right)
					.VAlign(VAlign_Fill)
					[
						// Shadow: a hint to scroll right
						SNew(SImage)
						.Visibility(EVisibility::HitTestInvisible)
						.ColorAndOpacity(this, &SScrollBox::GetEndShadowOpacity)
						.Image(&Style->RightShadowBrush)
					]
			]

		];

	if (!bScrollBarIsExternal)
	{
		PanelAndScrollbar->AddSlot()
			.AutoHeight()
			[
				ScrollBar.ToSharedRef()
			];
	}
}

/** Adds a slot to SScrollBox */
SScrollBox::FSlot& SScrollBox::AddSlot()
{
	SScrollBox::FSlot& NewSlot = *new SScrollBox::FSlot();
	ScrollPanel->Children.Add( &NewSlot );

	return NewSlot;
}

/** Removes a slot at the specified location */
void SScrollBox::RemoveSlot( const TSharedRef<SWidget>& WidgetToRemove )
{
	TPanelChildren<SScrollBox::FSlot>& Children = ScrollPanel->Children;
	for( int32 SlotIndex=0; SlotIndex < Children.Num(); ++SlotIndex )
	{
		if ( Children[SlotIndex].GetWidget() == WidgetToRemove )
		{
			Children.RemoveAt(SlotIndex);
			return;
		}
	}
}

void SScrollBox::ClearChildren()
{
	ScrollPanel->Children.Empty();
}

bool SScrollBox::IsRightClickScrolling() const
{
	return AmountScrolledWhileRightMouseDown >= FSlateApplication::Get().GetDragTriggerDistance() && this->ScrollBar->IsNeeded();
}

float SScrollBox::GetScrollOffset()
{
	return DesiredScrollOffset;
}

void SScrollBox::SetScrollOffset( float NewScrollOffset )
{
	DesiredScrollOffset = NewScrollOffset;
	bScrollToEnd = false;
}

void SScrollBox::ScrollToStart()
{
	SetScrollOffset(0);
}

void SScrollBox::ScrollToEnd()
{
	bScrollToEnd = true;
}

bool SScrollBox::ScrollDescendantIntoView(const FGeometry& MyGeometry, const TSharedPtr<SWidget>& WidgetToFind, bool InAnimateScroll)
{
	// We need to safely find the one WidgetToFind among our descendants.
	TSet< TSharedRef<SWidget> > WidgetsToFind;
	{
		WidgetsToFind.Add( WidgetToFind.ToSharedRef() );
	}
	TMap<TSharedRef<SWidget>, FArrangedWidget> Result;

	FindChildGeometries( MyGeometry, WidgetsToFind, Result );

	FArrangedWidget* WidgetGeometry = Result.Find( WidgetToFind.ToSharedRef() );
	if ( ensureMsg( WidgetGeometry, TEXT("Unable to scroll to descendant as it's not a child of the scrollbox") ) )
	{
		// @todo: This is a workaround because DesiredScrollOffset can exceed the ScrollMax when mouse dragging on the scroll bar and we need it clamped here or the offset is wrong
		ScrollBy(MyGeometry, 0, false);

		// Calculate how much we would need to scroll to bring this to the top/left of the scroll box
		const float WidgetPosition = GetScrollComponentFromVector(WidgetGeometry->Geometry.AbsolutePosition);
		const float MyPosition = GetScrollComponentFromVector(MyGeometry.AbsolutePosition);
		const float ScrollOffset = WidgetPosition - MyPosition;
		ScrollBy(MyGeometry, ScrollOffset, InAnimateScroll);
		return true;
	}
	return false;
}

EOrientation SScrollBox::GetOrientation()
{
	return Orientation;
}

void SScrollBox::SetOrientation(EOrientation InOrientation)
{
	if (Orientation != InOrientation)
	{
		Orientation = InOrientation;
		if (!bScrollBarIsExternal)
		{
			ScrollBar = ConstructScrollBar();
		}
		ScrollPanel->SetOrientation(Orientation);
		if (Orientation == Orient_Vertical)
		{
			ConstructVerticalLayout();
		}
		else
		{
			ConstructHorizontalLayout();
		}
	}
}

void SScrollBox::SetScrollBarVisibility(EVisibility InVisibility)
{
	ScrollBar->SetUserVisibility(InVisibility);
}

void SScrollBox::SetScrollBarAlwaysVisible(bool InAlwaysVisible)
{
	ScrollBar->SetScrollBarAlwaysVisible(InAlwaysVisible);
}

void SScrollBox::SetScrollBarThickness(FVector2D InThickness)
{
	ScrollBar->SetThickness(InThickness);
}

EActiveTimerReturnType SScrollBox::UpdateInertialScroll(double InCurrentTime, float InDeltaTime)
{
	InertialScrollManager.UpdateScrollVelocity(InDeltaTime);

	if (bIsScrolling)
	{
		const float ScrollVelocity = InertialScrollManager.GetScrollVelocity();
		// Do not apply inertial scrolling while the user is actively scrolling via RMB.
		if (ScrollVelocity != 0.f && !IsRightClickScrolling())
		{
			ScrollBy(CachedGeometry, ScrollVelocity * InDeltaTime, true);
		}

		return EActiveTimerReturnType::Continue;
	}

	// Clear the scroll velocity so there isn't any delayed minuscule movement
	InertialScrollManager.ClearScrollVelocity();

	bIsActiveTimerRegistered = false;
	return EActiveTimerReturnType::Stop;
}

void SScrollBox::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	CachedGeometry = AllottedGeometry;

	const FGeometry ScrollPanelGeometry = FindChildGeometry( AllottedGeometry, ScrollPanel.ToSharedRef() );
	const float ContentSize = GetScrollComponentFromVector(ScrollPanel->GetDesiredSize());

	if ( bScrollToEnd )
	{
		DesiredScrollOffset = ContentSize;
		bScrollToEnd = false;
	}

	// If this scroll box has no size, do not compute a view fraction because it will be wrong and causes pop in when the size is available
	const float ViewFraction = GetScrollComponentFromVector(AllottedGeometry.Size) > 0 ? GetScrollComponentFromVector(ScrollPanelGeometry.Size) / ContentSize : 1;
	const float ViewOffset = FMath::Clamp<float>( DesiredScrollOffset/ContentSize, 0.0, 1.0 - ViewFraction );
	
	// Update the scrollbar with the clamped version of the offset
	const float TargetPhysicalOffset = GetScrollComponentFromVector(ViewOffset*ScrollPanel->GetDesiredSize());
	bIsScrolling = !FMath::IsNearlyEqual(TargetPhysicalOffset, ScrollPanel->PhysicalOffset, 0.001f);
	ScrollPanel->PhysicalOffset = (bAnimateScroll)
		? FMath::FInterpTo(ScrollPanel->PhysicalOffset, TargetPhysicalOffset, InDeltaTime, 15.f)
		: TargetPhysicalOffset;
	
	ScrollBar->SetState(ViewOffset, ViewFraction);
	if (!ScrollBar->IsNeeded())
	{
		// We cannot scroll, so ensure that there is no offset.
		ScrollPanel->PhysicalOffset = 0.0f;
	}
	else if (bIsScrolling && !bIsActiveTimerRegistered)
	{
		// If scrolling and the scrollbar is needed, make sure the active timer is registered (possible b/c we may need to scroll when our geometry changes)
		bIsActiveTimerRegistered = true;
		RegisterActiveTimer(0.f, FWidgetActiveTimerDelegate::CreateSP(this, &SScrollBox::UpdateInertialScroll));
	}
}

FReply SScrollBox::OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	// Zero the scroll velocity so the panel stops immediately on mouse down, even if the user does not drag
	this->InertialScrollManager.ClearScrollVelocity();

	if ( MouseEvent.GetEffectingButton() == EKeys::RightMouseButton && ScrollBar->IsNeeded() )
	{
		AmountScrolledWhileRightMouseDown = 0;

		// NOTE: We don't bother capturing the mouse, unless the user starts dragging a few pixels (see the
		// mouse move handling here.)  This is important so that the item row has a chance to select
		// items when the right mouse button is released.  Just keep in mind that you might not get
		// an OnMouseButtonUp event for the right mouse button if the user moves off of the table before
		// they reach our scroll threshold
		return FReply::Handled();
	}
	return FReply::Unhandled();	
}


FReply SScrollBox::OnMouseButtonUp( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	if ( MouseEvent.GetEffectingButton() == EKeys::RightMouseButton )
	{
		if (!bIsActiveTimerRegistered && IsRightClickScrolling())
		{
			// Register the active timer to handle the inertial scrolling
			CachedGeometry = MyGeometry;
			bIsScrolling = true;
			bIsActiveTimerRegistered = true;
			RegisterActiveTimer(0.f, FWidgetActiveTimerDelegate::CreateSP(this, &SScrollBox::UpdateInertialScroll));
		}

		AmountScrolledWhileRightMouseDown = 0;

		FReply Reply = FReply::Handled().ReleaseMouseCapture();
		bShowSoftwareCursor = false;

		// If we have mouse capture, snap the mouse back to the closest location that is within the panel's bounds
		if ( HasMouseCapture() )
		{
			FSlateRect PanelScreenSpaceRect = MyGeometry.GetClippingRect();
			FVector2D CursorPosition = MyGeometry.LocalToAbsolute( SoftwareCursorPosition );

			FIntPoint BestPositionInPanel(
				FMath::RoundToInt( FMath::Clamp( CursorPosition.X, PanelScreenSpaceRect.Left, PanelScreenSpaceRect.Right ) ),
				FMath::RoundToInt( FMath::Clamp( CursorPosition.Y, PanelScreenSpaceRect.Top, PanelScreenSpaceRect.Bottom ) )
				);

			Reply.SetMousePos(BestPositionInPanel);
		}

		return Reply;
	}
	return FReply::Unhandled();
}

FReply SScrollBox::OnMouseMove( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{	
	if( MouseEvent.IsMouseButtonDown( EKeys::RightMouseButton ) )
	{
		const float ScrollByAmount = GetScrollComponentFromVector(MouseEvent.GetCursorDelta());
		// If scrolling with the right mouse button, we need to remember how much we scrolled.
		// If we did not scroll at all, we will bring up the context menu when the mouse is released.
		AmountScrolledWhileRightMouseDown += FMath::Abs( ScrollByAmount );

		// Has the mouse moved far enough with the right mouse button held down to start capturing
		// the mouse and dragging the view?
		if( IsRightClickScrolling() )
		{
			this->InertialScrollManager.AddScrollSample(-ScrollByAmount, FPlatformTime::Seconds());
			const bool bDidScroll = this->ScrollBy( MyGeometry, -ScrollByAmount );

			FReply Reply = FReply::Handled();

			// Capture the mouse if we need to
			if(this->HasMouseCapture() == false)
			{
				Reply.CaptureMouse( AsShared() ).UseHighPrecisionMouseMovement( AsShared() );
				SoftwareCursorPosition = MyGeometry.AbsoluteToLocal( MouseEvent.GetScreenSpacePosition() );
				bShowSoftwareCursor = true;
			}

			// Check if the mouse has moved.
			if( bDidScroll )
			{
				SetScrollComponentOnVector(SoftwareCursorPosition, GetScrollComponentFromVector(SoftwareCursorPosition) + GetScrollComponentFromVector(MouseEvent.GetCursorDelta()));
			}

			return Reply;
		}
	}

	return FReply::Unhandled();
}

void SScrollBox::OnMouseLeave( const FPointerEvent& MouseEvent )
{
	if(this->HasMouseCapture() == false)
	{
		// No longer scrolling (unless we have mouse capture)
		AmountScrolledWhileRightMouseDown = 0;
	}
}

FReply SScrollBox::OnMouseWheel( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	if (ScrollBar->IsNeeded())
	{
		// Make sure scroll velocity is cleared so it doesn't fight with the mouse wheel input
		InertialScrollManager.ClearScrollVelocity();

		const bool bScrollWasHandled = this->ScrollBy( MyGeometry, -MouseEvent.GetWheelDelta()*WheelScrollAmount );

		if (bScrollWasHandled && !bIsActiveTimerRegistered)
		{
			// Register the active timer to handle the inertial scrolling
			CachedGeometry = MyGeometry;
			bIsScrolling = true;
			bIsActiveTimerRegistered = true;
			RegisterActiveTimer(0.f, FWidgetActiveTimerDelegate::CreateSP(this, &SScrollBox::UpdateInertialScroll));
		}

		return bScrollWasHandled ? FReply::Handled() : FReply::Unhandled();
	}
	else
	{
		return FReply::Unhandled();
	}
}

bool SScrollBox::ScrollBy( const FGeometry& AllottedGeometry, float ScrollAmount, bool InAnimateScroll )
{
	bAnimateScroll = InAnimateScroll;

	const float ContentSize = GetScrollComponentFromVector(ScrollPanel->GetDesiredSize());
	const FGeometry ScrollPanelGeometry = FindChildGeometry( AllottedGeometry, ScrollPanel.ToSharedRef() );

	float PreviousScrollOffset = DesiredScrollOffset;

	DesiredScrollOffset += ScrollAmount;

	const float ScrollMin = 0.0f;
	const float ScrollMax = ContentSize - GetScrollComponentFromVector(ScrollPanelGeometry.Size);
	DesiredScrollOffset = FMath::Clamp(DesiredScrollOffset, ScrollMin, ScrollMax);

	OnUserScrolled.ExecuteIfBound(DesiredScrollOffset);

	return ConsumeMouseWheel == EConsumeMouseWheel::Always || DesiredScrollOffset != PreviousScrollOffset;
}

FReply SScrollBox::OnDragDetected( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	return FReply::Handled().CaptureMouse( SharedThis(this) );
}

FCursorReply SScrollBox::OnCursorQuery( const FGeometry& MyGeometry, const FPointerEvent& CursorEvent ) const
{
	if ( IsRightClickScrolling() )
	{
		// We hide the native cursor as we'll be drawing the software EMouseCursor::GrabHandClosed cursor
		return FCursorReply::Cursor( EMouseCursor::None );
	}
	else
	{
		return FCursorReply::Unhandled();
	}
}

int32 SScrollBox::OnPaint( const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const
{
	int32 NewLayerId = SCompoundWidget::OnPaint( Args, AllottedGeometry, MyClippingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled );

	if( !bShowSoftwareCursor )
	{
		return NewLayerId;
	}

	const FSlateBrush* Brush = FCoreStyle::Get().GetBrush(TEXT("SoftwareCursor_Grab"));

	FSlateDrawElement::MakeBox(
		OutDrawElements,
		++NewLayerId,
		AllottedGeometry.ToPaintGeometry( SoftwareCursorPosition - ( Brush->ImageSize / 2 ), Brush->ImageSize ),
		Brush,
		MyClippingRect
		);

	return NewLayerId;
}

void SScrollBox::ScrollBar_OnUserScrolled( float InScrollOffsetFraction )
{
	DesiredScrollOffset = InScrollOffsetFraction * GetScrollComponentFromVector(ScrollPanel->GetDesiredSize());
	OnUserScrolled.ExecuteIfBound(DesiredScrollOffset);
}

const float ShadowFadeDistance = 32.0f;

FSlateColor SScrollBox::GetStartShadowOpacity() const
{
	// The shadow should only be visible when the user needs a hint that they can scroll up.
	const float ShadowOpacity = FMath::Clamp( ScrollPanel->PhysicalOffset/ShadowFadeDistance, 0.0f, 1.0f);
	
	return FLinearColor(1.0f, 1.0f, 1.0f, ShadowOpacity);
}

FSlateColor SScrollBox::GetEndShadowOpacity() const
{
	// The shadow should only be visible when the user needs a hint that they can scroll down.
	const float ShadowOpacity = (ScrollBar->DistanceFromBottom() * GetScrollComponentFromVector(ScrollPanel->GetDesiredSize()) / ShadowFadeDistance);
	
	return FLinearColor(1.0f, 1.0f, 1.0f, ShadowOpacity);
}
