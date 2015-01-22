// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SlateCore.h"
#include "SlateBrushAsset.generated.h"

/**
 * An asset describing how a texture can exist in slate's DPI-aware environment
 * and how this texture responds to resizing. e.g. Scale9-stretching? Tiling?
 */
UCLASS(hidecategories=Object, MinimalAPI, BlueprintType)
class USlateBrushAsset : public UObject
{
	GENERATED_UCLASS_BODY()

public:
	/** The slate brush resource describing the texture's behavior. */
	UPROPERTY(Category=Brush, EditAnywhere, meta=(ShowOnlyInnerProperties))
	FSlateBrush Brush;

	// Begin UObject Interface
	virtual void PostLoad() override;
	// End UObject Interface
};
