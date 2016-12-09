// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "Widgets/Layout/SSafeZone.h"
#include "Layout/LayoutUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/CoreDelegates.h"

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
	bIsTitleSafe = InArgs._IsTitleSafe;
	bPadLeft = InArgs._PadLeft;
	bPadRight = InArgs._PadRight;
	bPadTop = InArgs._PadTop;
	bPadBottom = InArgs._PadBottom;

	SetTitleSafe(bIsTitleSafe);

	FCoreDelegates::OnSafeFrameChangedEvent.AddSP(this, &SSafeZone::SafeAreaUpdated);
}

void SSafeZone::SafeAreaUpdated()
{
	SetTitleSafe(bIsTitleSafe);
}

void SSafeZone::SetTitleSafe( bool InIsTitleSafe )
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

	SafeMargin = FMargin(bPadLeft ? SafeMargin.Left : 0.0f, bPadTop ? SafeMargin.Top : 0.0f, bPadRight ? SafeMargin.Right : 0.0f, bPadBottom ? SafeMargin.Bottom : 0.0f);
}

void SSafeZone::SetSidesToPad(bool InPadLeft, bool InPadRight, bool InPadTop, bool InPadBottom)
{
	bPadLeft = InPadLeft;
	bPadRight = InPadRight;
	bPadTop = InPadTop;
	bPadBottom = InPadBottom;

	SetTitleSafe(bIsTitleSafe);
}

void SSafeZone::SetSafeAreaScale(FMargin InSafeAreaScale)
{
	SafeAreaScale = InSafeAreaScale;
}

FMargin SSafeZone::ComputeScaledSafeMargin(float Scale) const
{
	const float InvScale = 1.0f / Scale;
	const FMargin ScaledSafeMargin(
		FMath::RoundToFloat(SafeMargin.Left * InvScale),
		FMath::RoundToFloat(SafeMargin.Top * InvScale),
		FMath::RoundToFloat(SafeMargin.Right * InvScale),
		FMath::RoundToFloat(SafeMargin.Bottom * InvScale));
	return ScaledSafeMargin;
}

void SSafeZone::OnArrangeChildren( const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren ) const
{
	const EVisibility& MyCurrentVisibility = this->GetVisibility();
	if ( ArrangedChildren.Accepts( MyCurrentVisibility ) )
	{
		const FMargin SlotPadding               = Padding.Get() + (ComputeScaledSafeMargin(AllottedGeometry.Scale) * SafeAreaScale);
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
		const FMargin SlotPadding = Padding.Get() + (ComputeScaledSafeMargin(LayoutScale) * SafeAreaScale);
		FVector2D BaseDesiredSize = SBox::ComputeDesiredSize(LayoutScale);

		return BaseDesiredSize + SlotPadding.GetDesiredSize();
	}

	return FVector2D(0, 0);
}
