// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	AimOffsetBlendSpace.cpp: AimOffsetBlendSpace functionality
=============================================================================*/ 

#include "EnginePrivate.h"
#include "Animation/BlendSpaceBase.h"
#include "Animation/AimOffsetBlendSpace.h"
#include "Animation/AnimSequence.h"

UAimOffsetBlendSpace::UAimOffsetBlendSpace(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bRotationBlendInMeshSpace = true;
}

bool UAimOffsetBlendSpace::IsValidAdditive()  const
{
	return IsValidAdditiveInternal(AAT_RotationOffsetMeshSpace);
}

bool UAimOffsetBlendSpace::ValidateSampleInput(FBlendSample& BlendSample, int32 OriginalIndex) const
{
	// we only accept rotation offset additive. Otherwise it's invalid
	if (BlendSample.Animation && !BlendSample.Animation->IsValidAdditive())
	{
		return false;
	}

	return Super::ValidateSampleInput(BlendSample, OriginalIndex);
}
