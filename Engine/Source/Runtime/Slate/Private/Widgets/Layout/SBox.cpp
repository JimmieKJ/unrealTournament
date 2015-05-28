// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SlatePrivatePCH.h"
#include "LayoutUtils.h"

SBox::SBox()
: ChildSlot()
{
}

void SBox::Construct( const FArguments& InArgs )
{
	WidthOverride = InArgs._WidthOverride;
	HeightOverride = InArgs._HeightOverride;

	MinDesiredWidth = InArgs._MinDesiredWidth;
	MinDesiredHeight = InArgs._MinDesiredHeight;
	MaxDesiredWidth = InArgs._MaxDesiredWidth;
	MaxDesiredHeight = InArgs._MaxDesiredHeight;

	ChildSlot
		.HAlign( InArgs._HAlign )
		.VAlign( InArgs._VAlign )
		.Padding( InArgs._Padding )
	[
		InArgs._Content.Widget
	];
}

void SBox::SetContent(const TSharedRef< SWidget >& InContent)
{
	ChildSlot
	[
		InContent
	];
}

void SBox::SetHAlign(EHorizontalAlignment HAlign)
{
	ChildSlot.HAlignment = HAlign;
}

void SBox::SetVAlign(EVerticalAlignment VAlign)
{
	ChildSlot.VAlignment = VAlign;
}

void SBox::SetPadding(const TAttribute<FMargin>& InPadding)
{
	ChildSlot.SlotPadding = InPadding;
}

void SBox::SetWidthOverride(TAttribute<FOptionalSize> InWidthOverride)
{
	WidthOverride = InWidthOverride;
}

void SBox::SetHeightOverride(TAttribute<FOptionalSize> InHeightOverride)
{
	HeightOverride = InHeightOverride;
}

void SBox::SetMinDesiredWidth(TAttribute<FOptionalSize> InMinDesiredWidth)
{
	MinDesiredWidth = InMinDesiredWidth;
}

void SBox::SetMinDesiredHeight(TAttribute<FOptionalSize> InMinDesiredHeight)
{
	MinDesiredHeight = InMinDesiredHeight;
}

void SBox::SetMaxDesiredWidth(TAttribute<FOptionalSize> InMaxDesiredWidth)
{
	MaxDesiredWidth = InMaxDesiredWidth;
}

void SBox::SetMaxDesiredHeight(TAttribute<FOptionalSize> InMaxDesiredHeight)
{
	MaxDesiredHeight = InMaxDesiredHeight;
}

FVector2D SBox::ComputeDesiredSize( float ) const
{
	EVisibility ChildVisibility = ChildSlot.GetWidget()->GetVisibility();

	if ( ChildVisibility != EVisibility::Collapsed )
	{
		// If the user specified a fixed width or height, those values override the Box's content.
		const FVector2D& UnmodifiedChildDesiredSize = ChildSlot.GetWidget()->GetDesiredSize() + ChildSlot.SlotPadding.Get().GetDesiredSize();
		const FOptionalSize CurrentWidthOverride = WidthOverride.Get();
		const FOptionalSize CurrentHeightOverride = HeightOverride.Get();
		const FOptionalSize CurrentMinDesiredWidth = MinDesiredWidth.Get();
		const FOptionalSize CurrentMinDesiredHeight = MinDesiredHeight.Get();
		const FOptionalSize CurrentMaxDesiredWidth = MaxDesiredWidth.Get();
		const FOptionalSize CurrentMaxDesiredHeight = MaxDesiredHeight.Get();

		float CurrentWidth = UnmodifiedChildDesiredSize.X;

		if ( CurrentMinDesiredWidth.IsSet() )
		{
			CurrentWidth = FMath::Max(CurrentWidth, CurrentMinDesiredWidth.Get());
		}

		if ( CurrentMaxDesiredWidth.IsSet() )
		{
			CurrentWidth = FMath::Min(CurrentWidth, CurrentMaxDesiredWidth.Get());
		}

		float CurrentHeight = UnmodifiedChildDesiredSize.Y;

		if ( CurrentMinDesiredHeight.IsSet() )
		{
			CurrentHeight = FMath::Max(CurrentHeight, CurrentMinDesiredHeight.Get());
		}

		if ( CurrentMaxDesiredHeight.IsSet() )
		{
			CurrentHeight = FMath::Min(CurrentHeight, CurrentMaxDesiredHeight.Get());
		}

		return FVector2D(
			( CurrentWidthOverride.IsSet() ) ? CurrentWidthOverride.Get() : CurrentWidth,
			( CurrentHeightOverride.IsSet() ) ? CurrentHeightOverride.Get() : CurrentHeight
		);
	}
	
	return FVector2D::ZeroVector;
}

void SBox::OnArrangeChildren( const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren ) const
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

FChildren* SBox::GetChildren()
{
	return &ChildSlot;
}

int32 SBox::OnPaint( const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const
{
	// An SBox just draws its only child
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
