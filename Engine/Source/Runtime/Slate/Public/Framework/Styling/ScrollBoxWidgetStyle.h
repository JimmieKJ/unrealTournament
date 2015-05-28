// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SlateWidgetStyleContainerBase.h"
#include "ScrollBoxWidgetStyle.generated.h"

/**
 */
UCLASS(hidecategories=Object, MinimalAPI)
class UScrollBoxWidgetStyle : public USlateWidgetStyleContainerBase
{
	GENERATED_BODY()

public:
	/** The actual data describing the scrollbox's appearance. */
	UPROPERTY(Category=Appearance, EditAnywhere, meta=(ShowOnlyInnerProperties))
	FScrollBoxStyle ScrollBoxStyle;

	virtual const struct FSlateWidgetStyle* const GetStyle() const override
	{
		return static_cast< const struct FSlateWidgetStyle* >( &ScrollBoxStyle );
	}
};
