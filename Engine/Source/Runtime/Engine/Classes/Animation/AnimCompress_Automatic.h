// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/**
 * Animation compression algorithm that is just a shell for trying the range of other compression schemes and pikcing the
 * smallest result within a configurable error threshold.
 *
 */

#pragma once
#include "Animation/AnimCompress.h"
#include "AnimCompress_Automatic.generated.h"

UCLASS(hidecategories=AnimationCompressionAlgorithm)
class UAnimCompress_Automatic : public UAnimCompress
{
	GENERATED_UCLASS_BODY()

	/** Maximum amount of error that a compression technique can introduce in an end effector */
	UPROPERTY(EditAnywhere, Category=AnimationCompressionAlgorithm_Automatic)
	float MaxEndEffectorError;

	UPROPERTY(EditAnywhere, Category=AnimationCompressionAlgorithm_Automatic)
	uint32 bTryFixedBitwiseCompression:1;

	UPROPERTY(EditAnywhere, Category=AnimationCompressionAlgorithm_Automatic)
	uint32 bTryPerTrackBitwiseCompression:1;

	UPROPERTY(EditAnywhere, Category=AnimationCompressionAlgorithm_Automatic)
	uint32 bTryLinearKeyRemovalCompression:1;

	UPROPERTY(EditAnywhere, Category=AnimationCompressionAlgorithm_Automatic)
	uint32 bTryIntervalKeyRemoval:1;

	UPROPERTY(EditAnywhere, Category=AnimationCompressionAlgorithm_Automatic)
	uint32 bRunCurrentDefaultCompressor:1;

	UPROPERTY(EditAnywhere, Category=AnimationCompressionAlgorithm_Automatic)
	uint32 bAutoReplaceIfExistingErrorTooGreat:1;

	UPROPERTY(EditAnywhere, Category=AnimationCompressionAlgorithm_Automatic)
	uint32 bRaiseMaxErrorToExisting:1;


protected:
	// Begin UAnimCompress Interface
	virtual void DoReduction(class UAnimSequence* AnimSeq, const TArray<class FBoneData>& BoneData) override;
	// Begin UAnimCompress Interface
};



