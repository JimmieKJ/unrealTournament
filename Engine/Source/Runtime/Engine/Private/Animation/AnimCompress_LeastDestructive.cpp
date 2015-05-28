// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	AnimCompress_LeastDestructive.cpp: Uses the Bitwise compressor with really light settings
=============================================================================*/ 

#include "EnginePrivate.h"
#include "Animation/AnimCompress_LeastDestructive.h"
#include "Animation/AnimCompress_BitwiseCompressOnly.h"
#include "AnimationUtils.h"
#include "AnimEncoding.h"

UAnimCompress_LeastDestructive::UAnimCompress_LeastDestructive(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Description = TEXT("Least Destructive");
	TranslationCompressionFormat = ACF_None;
	RotationCompressionFormat = ACF_None;
}


void UAnimCompress_LeastDestructive::DoReduction(UAnimSequence* AnimSeq, const TArray<FBoneData>& BoneData)
{
	UAnimCompress* BitwiseCompressor = NewObject<UAnimCompress_BitwiseCompressOnly>();
	BitwiseCompressor->RotationCompressionFormat = ACF_Float96NoW;
	BitwiseCompressor->TranslationCompressionFormat = ACF_None;
	BitwiseCompressor->Reduce(AnimSeq, false);
}
