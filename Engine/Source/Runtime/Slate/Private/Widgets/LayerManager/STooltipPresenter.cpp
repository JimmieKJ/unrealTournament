// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SlatePrivatePCH.h"
#include "STooltipPresenter.h"

void STooltipPresenter::Construct(const FArguments& InArgs)
{
	this->ChildSlot
	[
		InArgs._Content.Widget
	];
	LocalCursorPosition = FVector2D::ZeroVector;
}

void STooltipPresenter::SetContent(const TSharedRef<SWidget>& InWidget)
{
	ChildSlot
	[
		InWidget
	];
}

void STooltipPresenter::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	// Have to do this in Tick because we need Desktop-space geometry. OnArrangedChildren is called from Paint() and uses window-space geometry.
	LocalCursorPosition = AllottedGeometry.AbsoluteToLocal(FSlateApplication::Get().GetCursorPos());
}

void STooltipPresenter::OnArrangeChildren(const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren) const
{
	static const FVector2D CursorSize = FVector2D(12, 12);

	const FSlateRect CursorAnchorRect(LocalCursorPosition, LocalCursorPosition + CursorSize);
	const FSlateRect TooltipPopup(LocalCursorPosition + CursorSize, LocalCursorPosition + CursorSize + ChildSlot.GetWidget()->GetDesiredSize());
	
	const FVector2D TooltipPosition = ComputePopupFitInRect(CursorAnchorRect, TooltipPopup, EOrientation::Orient_Vertical, FSlateRect(FVector2D::ZeroVector, AllottedGeometry.GetLocalSize()));

	ArrangedChildren.AddWidget(AllottedGeometry.MakeChild(
		ChildSlot.GetWidget(),
		ChildSlot.GetWidget()->GetDesiredSize(),
		FSlateLayoutTransform(TooltipPosition)
	));
}

FVector2D STooltipPresenter::ComputeDesiredSize( float ) const
{
	return ChildSlot.GetWidget()->GetDesiredSize();
}

FChildren* STooltipPresenter::GetChildren()
{
	return &ChildSlot;
}
