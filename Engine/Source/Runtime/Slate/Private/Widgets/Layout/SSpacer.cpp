// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SlatePrivatePCH.h"


/**
 * Construct this widget
 *
 * @param	InArgs	The declaration data for this widget
 */
void SSpacer::Construct( const FArguments& InArgs )
{
	SpacerSize = InArgs._Size;

	SWidget::Construct( 
		InArgs._ToolTipText,
		InArgs._ToolTip,
		InArgs._Cursor, 
		InArgs._IsEnabled,
		InArgs._Visibility,
		InArgs._RenderTransform,
		InArgs._RenderTransformPivot,
		InArgs._Tag,
		InArgs._ForceVolatile,
		InArgs.MetaData);
}


int32 SSpacer::OnPaint( const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const
{
	// We did not paint anything. The parent's current LayerId is probably the max we were looking for.
	return LayerId;
}

FVector2D SSpacer::ComputeDesiredSize( float ) const
{
	return SpacerSize.Get();
}
