// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/**
 * Blend Space. Contains a grid of data points with weights from sample points in the space
 *
 */

#pragma once 

#include "BlendSpace.h"

#include "AimOffsetBlendSpace.generated.h"

struct FBlendSample;

UCLASS(config=Engine, hidecategories=Object, MinimalAPI, BlueprintType)
class UAimOffsetBlendSpace : public UBlendSpace
{
	GENERATED_UCLASS_BODY()

	virtual bool IsValidAdditive() const override;

	/** Validate sample input. Return true if it's all good to go **/
	virtual bool ValidateSampleInput(FBlendSample & BlendSample, int32 OriginalIndex=INDEX_NONE) const override;
};

