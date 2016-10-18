// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "SlatePrivatePCH.h"

#include "SSafeZone.h"

void SSafeZone::Construct( const FArguments& InArgs )
{
	SBox::Construct(SBox::FArguments()
		.HAlign(InArgs._HAlign)
		.VAlign(InArgs._VAlign)
		[
			InArgs._Content.Widget
		]
	);

	Padding = InArgs._Padding;
	SafeAreaScale = InArgs._SafeAreaScale;

	SetTitleSafe( InArgs._IsTitleSafe );
}

void SSafeZone::SetTitleSafe( bool bIsTitleSafe )
{
	FDisplayMetrics Metrics;
	FSlateApplication::Get().GetDisplayMetrics( Metrics );

	if ( bIsTitleSafe )
	{
		SafeMargin = FMargin( Metrics.TitleSafePaddingSize.X, Metrics.TitleSafePaddingSize.Y );
	}
	else
	{
		SafeMargin = FMargin( Metrics.ActionSafePaddingSize.X, Metrics.ActionSafePaddingSize.Y );
	}
}

void SSafeZone::SetSafeAreaScale(FMargin InSafeAreaScale)
{
	SafeAreaScale = InSafeAreaScale;
}

void SSafeZone::OnArrangeChildren( const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren ) const
{
	const EVisibility& MyCurrentVisibility = this->GetVisibility();
	if ( ArrangedChildren.Accepts( MyCurrentVisibility ) )
	{
		const FMargin SlotPadding               = Padding.Get() + ( SafeMargin * SafeAreaScale * ( 1.f / AllottedGeometry.Scale ) );
		AlignmentArrangeResult XAlignmentResult = AlignChild<Orient_Horizontal>( AllottedGeometry.Size.X, ChildSlot, SlotPadding );
		AlignmentArrangeResult YAlignmentResult = AlignChild<Orient_Vertical>( AllottedGeometry.Size.Y, ChildSlot, SlotPadding );

		ArrangedChildren.AddWidget(
			AllottedGeometry.MakeChild(
			ChildSlot.GetWidget(),
			FVector2D( XAlignmentResult.Offset, YAlignmentResult.Offset ),
			FVector2D( XAlignmentResult.Size, YAlignmentResult.Size )
			)
		);
	}
}

FVector2D SSafeZone::ComputeDesiredSize(float LayoutScale) const
{
	EVisibility ChildVisibility = ChildSlot.GetWidget()->GetVisibility();

	if ( ChildVisibility != EVisibility::Collapsed )
	{
		const FMargin SlotPadding = Padding.Get() + ( SafeMargin * SafeAreaScale * ( 1.f / LayoutScale ) );

		FVector2D BaseDesiredSize = SBox::ComputeDesiredSize(LayoutScale);

		return BaseDesiredSize + SlotPadding.GetDesiredSize();
	}

	return FVector2D(0, 0);
}
