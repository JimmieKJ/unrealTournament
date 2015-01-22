// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/**
 * Bitwise animation compression only; performs no key reduction.
 *
 */

#pragma once
#include "Animation/AnimCompress.h"
#include "AnimCompress_BitwiseCompressOnly.generated.h"

UCLASS(MinimalAPI)
class UAnimCompress_BitwiseCompressOnly : public UAnimCompress
{
	GENERATED_UCLASS_BODY()


protected:
	// Begin UAnimCompress Interface
	virtual void DoReduction(class UAnimSequence* AnimSeq, const TArray<class FBoneData>& BoneData) override;
	// Begin UAnimCompress Interface
};



