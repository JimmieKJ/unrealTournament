// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "SlatePrivatePCH.h"

#include "SConstraintCanvas.h"
#include "LayoutUtils.h"


/* SConstraintCanvas interface
 *****************************************************************************/

SConstraintCanvas::SConstraintCanvas()
: Children()
{
	bCanTick = false;
	bCanSupportFocus = false;
}

void SConstraintCanvas::Construct( const SConstraintCanvas::FArguments& InArgs )
{
	const int32 NumSlots = InArgs.Slots.Num();
	for ( int32 SlotIndex = 0; SlotIndex < NumSlots; ++SlotIndex )
	{
		Children.Add( InArgs.Slots[SlotIndex] );
	}
}

void SConstraintCanvas::ClearChildren()
{
	if ( Children.Num() )
	{
		Invalidate(EInvalidateWidget::Layout);
		Children.Empty();
	}
}

int32 SConstraintCanvas::RemoveSlot( const TSharedRef<SWidget>& SlotWidget )
{
	Invalidate(EInvalidateWidget::Layout);

	for (int32 SlotIdx = 0; SlotIdx < Children.Num(); ++SlotIdx)
	{
		if (SlotWidget == Children[SlotIdx].GetWidget())
		{
			Children.RemoveAt(SlotIdx);
			return SlotIdx;
		}
	}

	return -1;
}

struct FChildZOrder
{
	int32 ChildIndex;
	float ZOrder;
};

struct FSortSlotsByZOrder
{
	FORCEINLINE bool operator()(const FChildZOrder& A, const FChildZOrder& B) const
	{
		return A.ZOrder == B.ZOrder ? A.ChildIndex < B.ChildIndex : A.ZOrder < B.ZOrder;
	}
};

/* SWidget overrides
 *****************************************************************************/

void SConstraintCanvas::OnArrangeChildren( const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren ) const
{
	CachedGeometry = AllottedGeometry;

	if (Children.Num() > 0)
	{
		// Sort the children based on zorder.
		TArray< FChildZOrder, TInlineAllocator<64> > SlotOrder;
		SlotOrder.Reserve(Children.Num());

		for ( int32 ChildIndex = 0; ChildIndex < Children.Num(); ++ChildIndex )
		{
			const SConstraintCanvas::FSlot& CurChild = Children[ChildIndex];

			FChildZOrder Order;
			Order.ChildIndex = ChildIndex;
			Order.ZOrder = CurChild.ZOrderAttr.Get();
			SlotOrder.Add( Order );
		}

		SlotOrder.Sort(FSortSlotsByZOrder());

		// Arrange the children now in their proper z-order.
		for (int32 ChildIndex = 0; ChildIndex < Children.Num(); ++ChildIndex)
		{
			const FChildZOrder& CurSlot = SlotOrder[ChildIndex];
			const SConstraintCanvas::FSlot& CurChild = Children[CurSlot.ChildIndex];
			const TSharedRef<SWidget>& CurWidget = CurChild.GetWidget();

			const EVisibility ChildVisibility = CurWidget->GetVisibility();
			if ( ArrangedChildren.Accepts(ChildVisibility) )
			{
				const FMargin Offset = CurChild.OffsetAttr.Get();
				const FVector2D Alignment = CurChild.AlignmentAttr.Get();
				const FAnchors Anchors = CurChild.AnchorsAttr.Get();

				const bool AutoSize = CurChild.AutoSizeAttr.Get();

				const FMargin AnchorPixels =
					FMargin(Anchors.Minimum.X * AllottedGeometry.Size.X,
					Anchors.Minimum.Y * AllottedGeometry.Size.Y,
					Anchors.Maximum.X * AllottedGeometry.Size.X,
					Anchors.Maximum.Y * AllottedGeometry.Size.Y);

				const bool bIsHorizontalStretch = Anchors.Minimum.X != Anchors.Maximum.X;
				const bool bIsVerticalStretch = Anchors.Minimum.Y != Anchors.Maximum.Y;

				const FVector2D SlotSize = FVector2D(Offset.Right, Offset.Bottom);
				const FVector2D WidgetDesiredSize = CurWidget->GetDesiredSize();

				const FVector2D Size = AutoSize ? WidgetDesiredSize : SlotSize;

				// Calculate the offset based on the pivot position.
				FVector2D AlignmentOffset = Size * Alignment;

				// Calculate the local position based on the anchor and position offset.
				FVector2D LocalPosition, LocalSize;

				// Calculate the position and size based on the horizontal stretch or non-stretch
				if ( bIsHorizontalStretch )
				{
					LocalPosition.X = AnchorPixels.Left + Offset.Left;
					LocalSize.X = AnchorPixels.Right - LocalPosition.X - Offset.Right;
				}
				else
				{
					LocalPosition.X = AnchorPixels.Left + Offset.Left - AlignmentOffset.X;
					LocalSize.X = Size.X;
				}

				// Calculate the position and size based on the vertical stretch or non-stretch
				if ( bIsVerticalStretch )
				{
					LocalPosition.Y = AnchorPixels.Top + Offset.Top;
					LocalSize.Y = AnchorPixels.Bottom - LocalPosition.Y - Offset.Bottom;
				}
				else
				{
					LocalPosition.Y = AnchorPixels.Top + Offset.Top - AlignmentOffset.Y;
					LocalSize.Y = Size.Y;
				}

				// Add the information about this child to the output list (ArrangedChildren)
				ArrangedChildren.AddWidget(ChildVisibility, AllottedGeometry.MakeChild(
					// The child widget being arranged
					CurWidget,
					// Child's local position (i.e. position within parent)
					LocalPosition,
					// Child's size
					LocalSize
					));
			}
		}
	}
}

int32 SConstraintCanvas::OnPaint( const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const
{
	//FPlatformMisc::BeginNamedEvent(FColor::Orange, "SConstraintCanvas");

	FArrangedChildren ArrangedChildren(EVisibility::Visible);
	ArrangeChildren(AllottedGeometry, ArrangedChildren);

	bool bForwardedEnabled = ShouldBeEnabled(bParentEnabled);

	// Because we paint multiple children, we must track the maximum layer id that they produced in case one of our parents
	// wants to an overlay for all of its contents.
	int32 MaxLayerId = LayerId;

	const FPaintArgs NewArgs = Args.WithNewParent(this);

	for (int32 ChildIndex = 0; ChildIndex < ArrangedChildren.Num(); ++ChildIndex)
	{
		FArrangedWidget& CurWidget = ArrangedChildren[ChildIndex];

		bool bWereOverlapping;
		FSlateRect ChildClipRect = MyClippingRect.IntersectionWith(CurWidget.Geometry.GetClippingRect(), bWereOverlapping);

		if ( bWereOverlapping )
		{
			const int32 CurWidgetsMaxLayerId = CurWidget.Widget->Paint(NewArgs, CurWidget.Geometry, ChildClipRect, OutDrawElements, MaxLayerId + 1, InWidgetStyle, bForwardedEnabled);

			MaxLayerId = FMath::Max(MaxLayerId, CurWidgetsMaxLayerId);
		}
	}

	//FPlatformMisc::EndNamedEvent();

	return MaxLayerId;
}

FVector2D SConstraintCanvas::ComputeDesiredSize( float ) const
{
	FVector2D FinalDesiredSize(0,0);

	// Arrange the children now in their proper z-order.
	for ( int32 ChildIndex = 0; ChildIndex < Children.Num(); ++ChildIndex )
	{
		const SConstraintCanvas::FSlot& CurChild = Children[ChildIndex];
		const TSharedRef<SWidget>& Widget = CurChild.GetWidget();
		const EVisibility ChildVisibilty = Widget->GetVisibility();

		// As long as the widgets are not collapsed, they should contribute to the desired size.
		if ( ChildVisibilty != EVisibility::Collapsed )
		{
			const FMargin Offset = CurChild.OffsetAttr.Get();
			const FVector2D Alignment = CurChild.AlignmentAttr.Get();
			const FAnchors Anchors = CurChild.AnchorsAttr.Get();

			const FVector2D SlotSize = FVector2D(Offset.Right, Offset.Bottom);
			const FVector2D WidgetDesiredSize = Widget->GetDesiredSize();

			const bool AutoSize = CurChild.AutoSizeAttr.Get();

			const FVector2D Size = AutoSize ? WidgetDesiredSize : SlotSize;

			const bool bIsDockedHorizontally = ( Anchors.Minimum.X == Anchors.Maximum.X ) && ( Anchors.Minimum.X == 0 || Anchors.Minimum.X == 1 );
			const bool bIsDockedVertically = ( Anchors.Minimum.Y == Anchors.Maximum.Y ) && ( Anchors.Minimum.Y == 0 || Anchors.Minimum.Y == 1 );

			FinalDesiredSize.X = FMath::Max(FinalDesiredSize.X, Size.X + ( bIsDockedHorizontally ? FMath::Abs(Offset.Left) : 0.0f ));
			FinalDesiredSize.Y = FMath::Max(FinalDesiredSize.Y, Size.Y + ( bIsDockedVertically ? FMath::Abs(Offset.Top) : 0.0f ));
		}
	}

	return FinalDesiredSize;
}

FChildren* SConstraintCanvas::GetChildren()
{
	return &Children;
}
