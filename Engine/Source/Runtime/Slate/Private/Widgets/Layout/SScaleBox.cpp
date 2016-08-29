// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "SlatePrivatePCH.h"

#include "LayoutUtils.h"
#include "SScaleBox.h"


/* SScaleBox interface
 *****************************************************************************/

void SScaleBox::Construct( const SScaleBox::FArguments& InArgs )
{
	Stretch = InArgs._Stretch;
	RefreshSafeZoneScale();

	StretchDirection = InArgs._StretchDirection;
	UserSpecifiedScale = InArgs._UserSpecifiedScale;
	IgnoreInheritedScale = InArgs._IgnoreInheritedScale;

	ChildSlot
	.HAlign(InArgs._HAlign)
	.VAlign(InArgs._VAlign)
	[
		InArgs._Content.Widget
	];
}

/* SWidget overrides
 *****************************************************************************/

void SScaleBox::OnArrangeChildren( const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren ) const
{
	const EVisibility ChildVisibility = ChildSlot.GetWidget()->GetVisibility();
	if ( ArrangedChildren.Accepts(ChildVisibility) )
	{
		const FVector2D AreaSize = AllottedGeometry.GetLocalSize();
		FVector2D SlotWidgetDesiredSize = ChildSlot.GetWidget()->GetDesiredSize();

		float FinalScale = 1;

		const EStretch::Type CurrentStretch = Stretch.Get();
		const EStretchDirection::Type CurrentStretchDirection = StretchDirection.Get();

		if (SlotWidgetDesiredSize.X != 0 && SlotWidgetDesiredSize.Y != 0 )
		{
			switch ( CurrentStretch )
			{
			case EStretch::None:
				break;
			case EStretch::Fill:
				SlotWidgetDesiredSize = AreaSize;
				break;
			case EStretch::ScaleToFit:
				FinalScale = FMath::Min(AreaSize.X / SlotWidgetDesiredSize.X, AreaSize.Y / SlotWidgetDesiredSize.Y);
				break;
			case EStretch::ScaleToFitX:
				FinalScale = AreaSize.X / SlotWidgetDesiredSize.X;
				break;
			case EStretch::ScaleToFitY:
				FinalScale = AreaSize.Y / SlotWidgetDesiredSize.Y;
				break;
			case EStretch::ScaleToFill:
				FinalScale = FMath::Max(AreaSize.X / SlotWidgetDesiredSize.X, AreaSize.Y / SlotWidgetDesiredSize.Y);
				break;
			case EStretch::ScaleBySafeZone:
				FinalScale = SafeZoneScale;
				break;
			case EStretch::UserSpecified:
				FinalScale = UserSpecifiedScale.Get(1.0f);
				break;
			}

			switch ( CurrentStretchDirection )
			{
			case EStretchDirection::DownOnly:
				FinalScale = FMath::Min(FinalScale, 1.0f);
				break;
			case EStretchDirection::UpOnly:
				FinalScale = FMath::Max(FinalScale, 1.0f);
				break;
			case EStretchDirection::Both:
				break;
			}
		}

		if (IgnoreInheritedScale.Get(false) && AllottedGeometry.Scale != 0)
		{
			FinalScale /= AllottedGeometry.Scale;
		}

		FVector2D FinalOffset(0, 0);

		// If we're just filling, there's no scale applied, we're just filling the area.
		if ( CurrentStretch != EStretch::Fill )
		{
			const FMargin SlotPadding(ChildSlot.SlotPadding.Get());
			AlignmentArrangeResult XResult = AlignChild<Orient_Horizontal>(AreaSize.X, ChildSlot, SlotPadding, FinalScale, false);
			AlignmentArrangeResult YResult = AlignChild<Orient_Vertical>(AreaSize.Y, ChildSlot, SlotPadding, FinalScale, false);

			FinalOffset = FVector2D(XResult.Offset, YResult.Offset) / FinalScale;

			// If the layout horizontally is fill, then we need the desired size to be the whole size of the widget, 
			// but scale the inverse of the scale we're applying.
			if ( ChildSlot.HAlignment == HAlign_Fill )
			{
				SlotWidgetDesiredSize.X = AreaSize.X / FinalScale;
			}

			// If the layout vertically is fill, then we need the desired size to be the whole size of the widget, 
			// but scale the inverse of the scale we're applying.
			if ( ChildSlot.VAlignment == VAlign_Fill )
			{
				SlotWidgetDesiredSize.Y = AreaSize.Y / FinalScale;
			}
		}

		if ( CurrentStretch != EStretch::UserSpecified && CurrentStretch != EStretch::ScaleBySafeZone )
		{
			// We need to run another pre-pass now that we know the final scale.
			// This will allow things that don't scale linearly (such as text) to update their size and layout correctly.
			ChildSlot.GetWidget()->SlatePrepass(AllottedGeometry.GetAccumulatedLayoutTransform().GetScale() * FinalScale);
		}

		ArrangedChildren.AddWidget(ChildVisibility, AllottedGeometry.MakeChild(
			ChildSlot.GetWidget(),
			FinalOffset,
			SlotWidgetDesiredSize,
			FinalScale
		) );
	}
}

int32 SScaleBox::OnPaint( const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const
{
	return SCompoundWidget::OnPaint(Args, AllottedGeometry, MyClippingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
}

void SScaleBox::SetContent(TSharedRef<SWidget> InContent)
{
	ChildSlot
	[
		InContent
	];
}

void SScaleBox::SetHAlign(EHorizontalAlignment HAlign)
{
	ChildSlot.HAlignment = HAlign;
}

void SScaleBox::SetVAlign(EVerticalAlignment VAlign)
{
	ChildSlot.VAlignment = VAlign;
}

void SScaleBox::SetStretchDirection(EStretchDirection::Type InStretchDirection)
{
	StretchDirection = InStretchDirection;
}

void SScaleBox::SetStretch(EStretch::Type InStretch)
{
	Stretch = InStretch;
	RefreshSafeZoneScale();
}

void SScaleBox::SetUserSpecifiedScale(float InUserSpecifiedScale)
{
	UserSpecifiedScale = InUserSpecifiedScale;
}

void SScaleBox::SetIgnoreInheritedScale(bool InIgnoreInheritedScale)
{
	IgnoreInheritedScale = InIgnoreInheritedScale;
}

FVector2D SScaleBox::ComputeDesiredSize(float InScale) const
{
	const float LayoutScale = GetLayoutScale();
	return LayoutScale * SCompoundWidget::ComputeDesiredSize(InScale);
}

float SScaleBox::GetRelativeLayoutScale(const FSlotBase& Child) const
{
	return GetLayoutScale();
}

float SScaleBox::GetLayoutScale() const
{
	const EStretch::Type CurrentStretch = Stretch.Get();

	switch (CurrentStretch)
	{
	case EStretch::ScaleBySafeZone:
		return SafeZoneScale;
	case EStretch::UserSpecified:
		return UserSpecifiedScale.Get(1.0f);
	default:
		// Because our scale is determined by our size, we always report a scale of 1.0 here, 
		// as reporting our actual scale can cause a feedback loop whereby the calculated size changes each frame.
		// We workaround this by forcibly pre-passing our child content a second time once we know its final scale in OnArrangeChildren.
		return 1.0f;
	}
}

void SScaleBox::RefreshSafeZoneScale()
{
	float ScaleDownBy = 0.f;

	if (Stretch.Get() == EStretch::ScaleBySafeZone)
	{
		TSharedPtr<SViewport> GameViewport = FSlateApplication::Get().GetGameViewport();
		if (GameViewport.IsValid())
		{
			TSharedPtr<ISlateViewport> ViewportInterface = GameViewport->GetViewportInterface().Pin();
			if (ViewportInterface.IsValid())
			{
				FIntPoint ViewportSize = ViewportInterface->GetSize();

				FDisplayMetrics Metrics;
				FSlateApplication::Get().GetDisplayMetrics(Metrics);

				// Safe zones are uniform, so the axis we check is irrelevant
				ScaleDownBy = (Metrics.TitleSafePaddingSize.X * 2.f) / (float)ViewportSize.X;
			}
		}
		
	}

	SafeZoneScale = 1.f - ScaleDownBy;
}
