// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SlateCorePrivatePCH.h"

int32 SCompoundWidget::OnPaint( const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const
{
	// A CompoundWidget just draws its children
	FArrangedChildren ArrangedChildren(EVisibility::Visible);
	{
		this->ArrangeChildren(AllottedGeometry, ArrangedChildren);
	}

	// There may be zero elements in this array if our child collapsed/hidden
	if( ArrangedChildren.Num() > 0 )
	{
		check( ArrangedChildren.Num() == 1 );
		FArrangedWidget& TheChild = ArrangedChildren[0];

		const FSlateRect ChildClippingRect = AllottedGeometry.GetClippingRect().InsetBy( ChildSlot.SlotPadding.Get() * AllottedGeometry.Scale ).IntersectionWith(MyClippingRect);

		FWidgetStyle CompoundedWidgetStyle = FWidgetStyle(InWidgetStyle)
			.BlendColorAndOpacityTint(ColorAndOpacity.Get())
			.SetForegroundColor( GetForegroundColor() );

		return TheChild.Widget->Paint( Args.WithNewParent(this), TheChild.Geometry, ChildClippingRect, OutDrawElements, LayerId + 1, CompoundedWidgetStyle, ShouldBeEnabled( bParentEnabled ) );
	}
	return LayerId;
}

FChildren* SCompoundWidget::GetChildren()
{
	return &ChildSlot;
}


FVector2D SCompoundWidget::ComputeDesiredSize( float ) const
{
	EVisibility ChildVisibility = ChildSlot.GetWidget()->GetVisibility();
	if ( ChildVisibility != EVisibility::Collapsed )
	{
		return ChildSlot.GetWidget()->GetDesiredSize() + ChildSlot.SlotPadding.Get().GetDesiredSize();
	}
	
	return FVector2D::ZeroVector;
}

void SCompoundWidget::OnArrangeChildren( const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren ) const
{
	ArrangeSingleChild( AllottedGeometry, ArrangedChildren, ChildSlot, ContentScale );
}

FSlateColor SCompoundWidget::GetForegroundColor() const
{
	return ForegroundColor.Get();
}

SCompoundWidget::SCompoundWidget()
	: ChildSlot()
	, ContentScale( FVector2D(1.0f,1.0f) )
	, ColorAndOpacity( FLinearColor::White )
	, ForegroundColor( FSlateColor::UseForeground() )
{
}

void SCompoundWidget::SetVisibility( TAttribute<EVisibility> InVisibility )
{
	SWidget::SetVisibility( InVisibility );
}