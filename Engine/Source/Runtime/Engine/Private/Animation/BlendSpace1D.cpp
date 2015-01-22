// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	BlendSpace1D.cpp: 1D BlendSpace functionality
=============================================================================*/ 

#include "EnginePrivate.h"
#include "AnimationRuntime.h"
#include "AnimationUtils.h"
#include "Animation/BlendSpace1D.h"

UBlendSpace1D::UBlendSpace1D(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	NumOfDimension = 1;
}

float UBlendSpace1D::CalculateParamStep() const
{
	float GridSize = BlendParameters[0].GridNum;
	float CoordDim = BlendParameters[0].Max - BlendParameters[0].Min;
	return CoordDim/GridSize;
}

float UBlendSpace1D::CalculateThreshold() const
{
	return CalculateParamStep()*0.3f;
}

bool UBlendSpace1D::IsValidAdditive() const
{
	return IsValidAdditiveInternal(AAT_LocalSpaceBase);
}

void UBlendSpace1D::SnapToBorder(FBlendSample& Sample) const
{
	FVector& SampleValue = Sample.SampleValue;

	const float Threshold = CalculateThreshold();
	const float GridMin = BlendParameters[0].Min;
	const float GridMax = BlendParameters[0].Max;

	if (SampleValue.X != GridMax && SampleValue.X+Threshold > GridMax)
	{
		SampleValue.X = GridMax;
	}
	if (SampleValue.X != GridMin && SampleValue.X-Threshold < GridMin)
	{
		SampleValue.X = GridMin;
	}
}

EBlendSpaceAxis UBlendSpace1D::GetAxisToScale() const
{
	return bScaleAnimation ? BSA_X : BSA_None;
}

bool UBlendSpace1D::IsSameSamplePoint(const FVector& SamplePointA, const FVector& SamplePointB) const
{
	const float Threshold = CalculateThreshold();
	const FVector Diff = (SamplePointA-SamplePointB).GetAbs();
	return (Diff.X < Threshold);
}

void UBlendSpace1D::GetRawSamplesFromBlendInput(const FVector &BlendInput, TArray<FGridBlendSample> & OutBlendSamples) const
{

	FVector NormalizedBlendInput = GetNormalizedBlendInput(BlendInput);

	float GridIndex = FMath::TruncToFloat(NormalizedBlendInput.X);
	float Remainder = NormalizedBlendInput.X - GridIndex;

	const FEditorElement* BeforeElement = GetGridSampleInternal(GridIndex);

	if (BeforeElement)
	{
		FGridBlendSample NewSample;
		NewSample.GridElement = *BeforeElement;
		// now calculate weight - GridElement has weights to nearest samples, here we weight the grid element
		NewSample.BlendWeight = (1.f-Remainder);
		OutBlendSamples.Add(NewSample);
	}
	else
	{
		FGridBlendSample NewSample;
		NewSample.GridElement = FEditorElement();
		NewSample.BlendWeight = 0.f;
		OutBlendSamples.Add(NewSample);
	}

	const FEditorElement* AfterElement = GetGridSampleInternal(GridIndex+1);

	if (AfterElement)
	{
		FGridBlendSample NewSample;
		NewSample.GridElement = *AfterElement;
		// now calculate weight - GridElement has weights to nearest samples, here we weight the grid element
		NewSample.BlendWeight = (Remainder);
		OutBlendSamples.Add(NewSample);
	}
	else
	{
		FGridBlendSample NewSample;
		NewSample.GridElement = FEditorElement();
		NewSample.BlendWeight = 0.f;
		OutBlendSamples.Add(NewSample);
	}
}