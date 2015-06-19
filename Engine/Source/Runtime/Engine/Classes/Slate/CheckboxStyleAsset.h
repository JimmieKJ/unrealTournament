// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SlateCore.h"
#include "CheckboxStyleAsset.generated.h"

/**
 * An asset describing a CheckBox's appearance.
 * Just a wrapper for the struct with real data in it.
 */
UCLASS(hidecategories=Object, MinimalAPI)
class UCheckBoxStyleAsset : public UObject
{
	GENERATED_UCLASS_BODY()

public:
	/** The actual data describing the Check Box's appearance. */
	UPROPERTY(Category=Appearance, EditAnywhere, meta=(ShowOnlyInnerProperties))
	FCheckBoxStyle CheckBoxStyle;
};
