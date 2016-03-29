// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "TextLayout.h"

class SLATE_API ISlateRun : public IRun
{
public:

	virtual int32 OnPaint( const FPaintArgs& Args, const FTextLayout::FLineView& Line, const TSharedRef< ILayoutBlock >& Block, const FTextBlockStyle& DefaultStyle, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const = 0;

	virtual const TArray< TSharedRef<SWidget> >& GetChildren() = 0;

	virtual void ArrangeChildren( const TSharedRef< ILayoutBlock >& Block, const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren ) const = 0;
};
