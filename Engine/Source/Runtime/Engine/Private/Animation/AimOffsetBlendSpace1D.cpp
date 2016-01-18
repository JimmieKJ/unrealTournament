// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	AimOffsetBlendSpace1D.cpp: AimOffsetBlendSpace functionality
=============================================================================*/ 

#include "EnginePrivate.h"
#include "Animation/BlendSpaceBase.h"
#include "Animation/AimOffsetBlendSpace1D.h"
#include "Animation/AnimSequence.h"

UAimOffsetBlendSpace1D::UAimOffsetBlendSpace1D(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bRotationBlendInMeshSpace = true;
}

bool UAimOffsetBlendSpace1D::IsValidAdditive()  const
{
	return IsValidAdditiveInternal(AAT_RotationOffsetMeshSpace);
}

bool UAimOffsetBlendSpace1D::ValidateSampleInput(FBlendSample& BlendSample, int32 OriginalIndex) const
{
	// we only accept rotation offset additive. Otherwise it's invalid
	if (BlendSample.Animation && !BlendSample.Animation->IsValidAdditive())
	{
		return false;
	}

	return Super::ValidateSampleInput(BlendSample, OriginalIndex);
}
