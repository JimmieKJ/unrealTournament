// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "SUTScaleBox.h"

#if !UE_SERVER

void SUTScaleBox::Construct(const SUTScaleBox::FArguments& InArgs)
{
	bMaintainAspectRatio = InArgs._bMaintainAspectRatio;
	bFullScreen = InArgs._bFullScreen;

	ChildSlot
		[
			InArgs._Content.Widget
		];
}

void SUTScaleBox::OnArrangeChildren(const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren) const
{
	const EVisibility ChildVisibility = ChildSlot.GetWidget()->GetVisibility();
	if (ArrangedChildren.Accepts(ChildVisibility))
	{
		FVector2D DesiredDrawSize = ChildSlot.GetWidget()->GetDesiredSize();
		FVector2D FinalOffset(0, 0);

		if (bMaintainAspectRatio)
		{

			if (AllottedGeometry.Size != DesiredDrawSize)
			{
				float Aspect = DesiredDrawSize.X / DesiredDrawSize.Y;
				float Scale = AllottedGeometry.Size.Y / (bFullScreen ? 720.0f : DesiredDrawSize.Y);
				DesiredDrawSize.Y *= Scale;
				DesiredDrawSize.X = DesiredDrawSize.Y * Aspect;

				if (DesiredDrawSize.X > AllottedGeometry.Size.X)
				{
					Aspect = DesiredDrawSize.Y / DesiredDrawSize.X;
					DesiredDrawSize.X = AllottedGeometry.Size.X;
					DesiredDrawSize.Y = DesiredDrawSize.X * Aspect;
				}

				FinalOffset.X = AllottedGeometry.Size.X * 0.5 - DesiredDrawSize.X * 0.5;
				FinalOffset.Y = AllottedGeometry.Size.Y * 0.5 - DesiredDrawSize.Y * 0.5;
			}
		}
		else
		{
			DesiredDrawSize = AllottedGeometry.Size;
		}

		float FinalScale = 1;

		ArrangedChildren.AddWidget(ChildVisibility, AllottedGeometry.MakeChild(
			ChildSlot.GetWidget(),
			FinalOffset,
			DesiredDrawSize,
			FinalScale
			));
	}
}


int32 SUTScaleBox::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	return SCompoundWidget::OnPaint(Args, AllottedGeometry, MyClippingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
}

void SUTScaleBox::SetContent(TSharedRef<SWidget> InContent)
{
	ChildSlot
		[
			InContent
		];
}

#endif