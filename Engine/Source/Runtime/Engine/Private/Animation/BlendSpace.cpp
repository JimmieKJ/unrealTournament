// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	BlendSpace.cpp: 2D BlendSpace functionality
=============================================================================*/ 

#include "EnginePrivate.h"
#include "Animation/BlendSpaceBase.h"
#include "Animation/BlendSpace.h"
#include "AnimationUtils.h"

UBlendSpace::UBlendSpace(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	NumOfDimension = 2;
}

void UBlendSpace::GetGridSamplesFromBlendInput(const FVector &BlendInput, FGridBlendSample & LeftBottom, FGridBlendSample & RightBottom, FGridBlendSample & LeftTop, FGridBlendSample& RightTop) const
{
	FVector NormalizedBlendInput = GetNormalizedBlendInput(BlendInput);
	
	FVector GridIndex;
	GridIndex.X = FMath::TruncToFloat(NormalizedBlendInput.X);
	GridIndex.Y = FMath::TruncToFloat(NormalizedBlendInput.Y);
	GridIndex.Z = 0.f;

	FVector Remainder = NormalizedBlendInput - GridIndex;

	// bi-linear very simple interpolation
	const FEditorElement* EleLT = GetEditorElement(GridIndex.X, GridIndex.Y+1);
	if (EleLT)
	{
		LeftTop.GridElement = *EleLT;
		// now calculate weight - distance to each corner since input is already normalized within grid, we can just calculate distance 
		LeftTop.BlendWeight = (1.f-Remainder.X)*Remainder.Y;
	}
	else
	{
		LeftTop.GridElement = FEditorElement();
		LeftTop.BlendWeight = 0.f;
	}

	const FEditorElement* EleRT = GetEditorElement(GridIndex.X+1, GridIndex.Y+1);
	if (EleRT)
	{
		RightTop.GridElement = *EleRT;
		RightTop.BlendWeight = Remainder.X*Remainder.Y;
	}
	else
	{
		RightTop.GridElement = FEditorElement();
		RightTop.BlendWeight = 0.f;
	}

	const FEditorElement* EleLB = GetEditorElement(GridIndex.X, GridIndex.Y);
	if (EleLB)
	{
		LeftBottom.GridElement = *EleLB;
		LeftBottom.BlendWeight = (1.f-Remainder.X)*(1.f-Remainder.Y);
	}
	else
	{
		LeftBottom.GridElement = FEditorElement();
		LeftBottom.BlendWeight = 0.f;
	}

	const FEditorElement* EleRB = GetEditorElement(GridIndex.X+1, GridIndex.Y);
	if (EleRB)
	{
		RightBottom.GridElement = *EleRB;
		RightBottom.BlendWeight = Remainder.X*(1.f-Remainder.Y);
	}
	else
	{
		RightBottom.GridElement = FEditorElement();
		RightBottom.BlendWeight = 0.f;
	}
}

void UBlendSpace::GetRawSamplesFromBlendInput(const FVector &BlendInput, TArray<FGridBlendSample, TInlineAllocator<4> > & OutBlendSamples) const
{
	OutBlendSamples.Reset();
	OutBlendSamples.AddUninitialized(4);

	GetGridSamplesFromBlendInput(BlendInput, OutBlendSamples[0], OutBlendSamples[1], OutBlendSamples[2], OutBlendSamples[3]);
}

const FEditorElement* UBlendSpace::GetEditorElement(int32 XIndex, int32 YIndex) const
{
	int32 Index = XIndex*(BlendParameters[1].GridNum+1) + YIndex;
	return GetGridSampleInternal(Index);
}

FVector2D UBlendSpace::CalculateThreshold() const
{
	FVector2D GridSize(BlendParameters[0].GridNum, BlendParameters[1].GridNum);
	FVector GridMin(BlendParameters[0].Min, BlendParameters[1].Min, 0.f);
	FVector GridMax(BlendParameters[0].Max, BlendParameters[1].Max, 0.f);
	FBox GridDim(GridMin, GridMax);
	FVector2D CoordDim (GridDim.GetSize());

	// it does not make sense to put a lot of samples between grid
	// since grid is the sample points at the end, you don't like to have too many samples within the grid. 
	// make sure it has enough space between - also it avoids tiny triangles
	return CoordDim/GridSize*0.3f;
}

bool UBlendSpace::IsValidAdditive()  const
{
	return IsValidAdditiveInternal(AAT_LocalSpaceBase) || IsValidAdditiveInternal(AAT_RotationOffsetMeshSpace);
}

void UBlendSpace::SnapToBorder(FBlendSample& Sample) const
{
	FVector& SampleValue = Sample.SampleValue;

	FVector2D Threshold = CalculateThreshold();
	FVector GridMin(BlendParameters[0].Min, BlendParameters[1].Min, 0.f);
	FVector GridMax(BlendParameters[0].Max, BlendParameters[1].Max, 0.f);

	if (SampleValue.X != GridMax.X && SampleValue.X+Threshold.X > GridMax.X)
	{
		SampleValue.X = GridMax.X;
	}
	if (SampleValue.X != GridMin.X && SampleValue.X-Threshold.X < GridMin.X)
	{
		SampleValue.X = GridMin.X;
	}

	if (SampleValue.Y != GridMax.Y && SampleValue.Y+Threshold.Y > GridMax.Y)
	{
		SampleValue.Y = GridMax.Y;
	}
	if (SampleValue.Y != GridMin.Y && SampleValue.Y-Threshold.Y < GridMin.Y)
	{
		SampleValue.Y = GridMin.Y;
	}
}

EBlendSpaceAxis UBlendSpace::GetAxisToScale() const
{
	return AxisToScaleAnimation;
}

bool UBlendSpace::IsSameSamplePoint(const FVector& SamplePointA, const FVector& SamplePointB) const
{
	FVector2D Threshold = CalculateThreshold();
	FVector Diff = (SamplePointA-SamplePointB).GetAbs();
	return (Diff.X < Threshold.X) && (Diff.Y < Threshold.Y);
}