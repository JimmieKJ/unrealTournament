// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BlendSpaceBase.h"

#include "BlendSpace.generated.h"

/**
 * Contains a grid of data points with weights from sample points in the space
 */
UCLASS(config=Engine, hidecategories=Object, MinimalAPI, BlueprintType)
class UBlendSpace : public UBlendSpaceBase
{
	GENERATED_UCLASS_BODY()

public:

	/** If you have input interpolation, which axis to drive animation speed (scale) - i.e. for locomotion animation, speed axis will drive animation speed (thus scale)**/
	UPROPERTY(EditAnywhere, Category=InputInterpolation)
	TEnumAsByte<EBlendSpaceAxis> AxisToScaleAnimation;

	/** Get the Editor Element from Index
	 * 
	 * @param	XIndex	Index of X
	 * @param	YIndex	Index of Y
	 *
	 * @return	FEditorElement * return the grid data
	 */
	const FEditorElement* GetEditorElement(int32 XIndex, int32 YIndex) const;

	/** return true if all sample data is additive **/
	virtual bool IsValidAdditive() const override;

	/** 
	 * Get Grid Samples from BlendInput, From Input, it will return the 4 points of the grid that this input belongs to. 
	 * 
	 * @param	BlendInput	BlendInput X, Y, Z corresponds to BlendParameters[0], [1], [2]
	 * 
	 * @return	LeftTop, RightTop, LeftBottom, RightBottom	4 corner of the grid this BlendInput belongs to 
	 *			It's possible they return INDEX_NONE, but that case the weight also should be 0.f
	 *
	 */
	ENGINE_API void GetGridSamplesFromBlendInput(const FVector &BlendInput, FGridBlendSample & LeftBottom, FGridBlendSample & RightBottom, FGridBlendSample & LeftTop, FGridBlendSample& RightTop) const;

protected:
	// Begin UBlendSpaceBase interface
	virtual void SnapToBorder(FBlendSample& Sample) const override;
	virtual EBlendSpaceAxis GetAxisToScale() const override;
	virtual bool IsSameSamplePoint(const FVector& SamplePointA, const FVector& SamplePointB) const override;
	virtual void GetRawSamplesFromBlendInput(const FVector &BlendInput, TArray<FGridBlendSample> & OutBlendSamples) const override;
	// End UBlendSpaceBase interface

private:
	/** 
	 * Calculate threshold for sample points - if within this threashold, considered to be same, so reject it
	 * this is to avoid any accidental same points of samples entered, causing confusing in triangulation 
	 */
	FVector2D CalculateThreshold() const;
};

