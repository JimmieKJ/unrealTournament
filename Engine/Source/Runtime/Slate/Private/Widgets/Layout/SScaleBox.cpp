// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SlatePrivatePCH.h"

#include "LayoutUtils.h"
#include "SScaleBox.h"


/* SScaleBox interface
 *****************************************************************************/

void SScaleBox::Construct( const SScaleBox::FArguments& InArgs )
{
	Stretch = InArgs._Stretch;
	StretchDirection = InArgs._StretchDirection;
	UserSpecifiedScale = InArgs._UserSpecifiedScale;

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
		FVector2D DesiredSize = ChildSlot.GetWidget()->GetDesiredSize();

		float FinalScale = 1;

		EStretch::Type CurrentStretch = Stretch.Get();
		EStretchDirection::Type CurrentStretchDirection = StretchDirection.Get();

		if ( DesiredSize.X != 0 && DesiredSize.Y != 0 )
		{
			switch ( CurrentStretch )
			{
			case EStretch::None:
				break;
			case EStretch::Fill:
				DesiredSize = AllottedGeometry.Size;
				break;
			case EStretch::ScaleToFit:
				FinalScale = FMath::Min(AllottedGeometry.Size.X / DesiredSize.X, AllottedGeometry.Size.Y / DesiredSize.Y);
				break;
			case EStretch::ScaleToFill:
				FinalScale = FMath::Max(AllottedGeometry.Size.X / DesiredSize.X, AllottedGeometry.Size.Y / DesiredSize.Y);
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

		FVector2D FinalOffset(0, 0);

		if ( CurrentStretch != EStretch::Fill )
		{
			const FMargin SlotPadding(ChildSlot.SlotPadding.Get());
			AlignmentArrangeResult XResult = AlignChild<Orient_Horizontal>(AllottedGeometry.Size.X, ChildSlot, SlotPadding, FinalScale, false);
			AlignmentArrangeResult YResult = AlignChild<Orient_Vertical>(AllottedGeometry.Size.Y, ChildSlot, SlotPadding, FinalScale, false);

			FinalOffset = FVector2D(XResult.Offset, YResult.Offset) * ( 1.0f / FinalScale );
		}

		ArrangedChildren.AddWidget(ChildVisibility, AllottedGeometry.MakeChild(
			ChildSlot.GetWidget(),
			FinalOffset,
			DesiredSize,
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
}

void SScaleBox::SetUserSpecifiedScale(float InUserSpecifiedScale)
{
	UserSpecifiedScale = InUserSpecifiedScale;
}
