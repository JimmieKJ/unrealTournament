// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "../Public/UnrealTournament.h"
#include "SUWScaleBox.h"

void SUWScaleBox::Construct(const SUWScaleBox::FArguments& InArgs)
{
	ChildSlot
		[
			InArgs._Content.Widget
		];
}

void SUWScaleBox::OnArrangeChildren(const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren) const
{
	const EVisibility ChildVisibility = ChildSlot.Widget->GetVisibility();
	if (ArrangedChildren.Accepts(ChildVisibility))
	{
		FVector2D DesiredSize = ChildSlot.Widget->GetDesiredSize();
		if (DesiredSize.X > AllottedGeometry.Size.X)
		{
			float Aspect = DesiredSize.Y / DesiredSize.X;
			DesiredSize.X = AllottedGeometry.Size.X;
			DesiredSize.Y = DesiredSize.X * Aspect;
		}

		float FinalScale = 1;

		FVector2D FinalOffset(0, 0);
		ArrangedChildren.AddWidget(ChildVisibility, AllottedGeometry.MakeChild(
			ChildSlot.Widget,
			FinalOffset,
			DesiredSize,
			FinalScale
			));
	}
}


int32 SUWScaleBox::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	return SCompoundWidget::OnPaint(Args, AllottedGeometry, MyClippingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
}

void SUWScaleBox::SetContent(TSharedRef<SWidget> InContent)
{
	ChildSlot
		[
			InContent
		];
}
