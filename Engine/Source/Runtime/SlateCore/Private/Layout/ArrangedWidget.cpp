// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SlateCorePrivatePCH.h"


FArrangedWidget FArrangedWidget::NullWidget(SNullWidget::NullWidget, FGeometry());

FString FArrangedWidget::ToString( ) const
{
	return FString::Printf(TEXT("%s @ %s"), *Widget->ToString(), *Geometry.ToString());
}


FWidgetAndPointer::FWidgetAndPointer()
: FArrangedWidget(FArrangedWidget::NullWidget)
, PointerPosition(TSharedPtr<FVirtualPointerPosition>())
{}

FWidgetAndPointer::FWidgetAndPointer( const FArrangedWidget& InWidget, const TSharedPtr<FVirtualPointerPosition>& InPosition )
: FArrangedWidget(InWidget)
, PointerPosition(InPosition)
{}