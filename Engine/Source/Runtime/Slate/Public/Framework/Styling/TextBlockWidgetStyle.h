// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SlateWidgetStyleContainerBase.h"
#include "TextBlockWidgetStyle.generated.h"

/**
 */
UCLASS(hidecategories=Object, MinimalAPI)
class UTextBlockWidgetStyle : public USlateWidgetStyleContainerBase
{
	GENERATED_BODY()

public:
	/** The actual data describing the button's appearance. */
	UPROPERTY(Category=Appearance, EditAnywhere, meta=(ShowOnlyInnerProperties))
	FTextBlockStyle TextBlockStyle;

	virtual const struct FSlateWidgetStyle* const GetStyle() const override
	{
		return static_cast< const struct FSlateWidgetStyle* >( &TextBlockStyle );
	}
};
