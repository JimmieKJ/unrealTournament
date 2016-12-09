// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "Widgets/Layout/SWindowTitleBarArea.h"
#include "Types/PaintArgs.h"
#include "Layout/ArrangedChildren.h"
#include "Layout/LayoutUtils.h"
#include "Widgets/SWindow.h"

SWindowTitleBarArea::SWindowTitleBarArea()
: ChildSlot()
{
	bCanTick = false;
	bCanSupportFocus = false;
}

void SWindowTitleBarArea::Construct( const FArguments& InArgs )
{
	ChildSlot
		.HAlign( InArgs._HAlign )
		.VAlign( InArgs._VAlign )
		.Padding( InArgs._Padding )
	[
		InArgs._Content.Widget
	];

	OnDoubleClick = InArgs._OnDoubleClick;
}

void SWindowTitleBarArea::SetContent(const TSharedRef< SWidget >& InContent)
{
	ChildSlot
	[
		InContent
	];
}

void SWindowTitleBarArea::SetHAlign(EHorizontalAlignment HAlign)
{
	ChildSlot.HAlignment = HAlign;
}

void SWindowTitleBarArea::SetVAlign(EVerticalAlignment VAlign)
{
	ChildSlot.VAlignment = VAlign;
}

void SWindowTitleBarArea::SetPadding(const TAttribute<FMargin>& InPadding)
{
	ChildSlot.SlotPadding = InPadding;
}

FVector2D SWindowTitleBarArea::ComputeDesiredSize( float ) const
{
	EVisibility ChildVisibility = ChildSlot.GetWidget()->GetVisibility();

	if ( ChildVisibility != EVisibility::Collapsed )
	{
		return ChildSlot.GetWidget()->GetDesiredSize() + ChildSlot.SlotPadding.Get().GetDesiredSize();
	}
	
	return FVector2D::ZeroVector;
}

void SWindowTitleBarArea::OnArrangeChildren( const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren ) const
{
	const EVisibility& MyCurrentVisibility = this->GetVisibility();
	if ( ArrangedChildren.Accepts( MyCurrentVisibility ) )
	{
		const FMargin SlotPadding(ChildSlot.SlotPadding.Get());
		AlignmentArrangeResult XAlignmentResult = AlignChild<Orient_Horizontal>( AllottedGeometry.Size.X, ChildSlot, SlotPadding );
		AlignmentArrangeResult YAlignmentResult = AlignChild<Orient_Vertical>( AllottedGeometry.Size.Y, ChildSlot, SlotPadding );

		ArrangedChildren.AddWidget(
			AllottedGeometry.MakeChild(
				ChildSlot.GetWidget(),
				FVector2D(XAlignmentResult.Offset, YAlignmentResult.Offset),
				FVector2D(XAlignmentResult.Size, YAlignmentResult.Size)
			)
		);
	}
}

FChildren* SWindowTitleBarArea::GetChildren()
{
	return &ChildSlot;
}

int32 SWindowTitleBarArea::OnPaint( const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const
{
	// An SWindowTitleBarArea just draws its only child
	FArrangedChildren ArrangedChildren(EVisibility::Visible);
	this->ArrangeChildren(AllottedGeometry, ArrangedChildren);

	// Maybe none of our children are visible
	if( ArrangedChildren.Num() > 0 )
	{
		check( ArrangedChildren.Num() == 1 );
		FArrangedWidget& TheChild = ArrangedChildren[0];

		const FSlateRect ChildClippingRect = AllottedGeometry.GetClippingRect().InsetBy( ChildSlot.SlotPadding.Get() * AllottedGeometry.Scale ).IntersectionWith(MyClippingRect);

		return TheChild.Widget->Paint( Args.WithNewParent(this), TheChild.Geometry, ChildClippingRect, OutDrawElements, LayerId, InWidgetStyle, ShouldBeEnabled( bParentEnabled ) );
	}

	return LayerId;
}

FReply SWindowTitleBarArea::OnMouseButtonDoubleClick(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	// Handle double click here in fullscreen mode only. In windowed mode double click is handled via window actions.
	if (GameWindow.IsValid() && GameWindow->GetWindowMode() != EWindowMode::Windowed)
	{
		if (OnDoubleClick.IsBound())
		{
			OnDoubleClick.Execute();
			return FReply::Handled();
		}
	}

	return FReply::Unhandled();
}

EWindowZone::Type SWindowTitleBarArea::GetWindowZoneOverride() const
{
	EWindowZone::Type Zone = EWindowZone::TitleBar;
	if (GameWindow.IsValid() && GameWindow->GetWindowMode() != EWindowMode::Windowed)
	{
		// In fullscreen, return ClientArea to prevent window from being moved
		Zone = EWindowZone::ClientArea;
	}
	return Zone;
}
