// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#include "EnginePrivate.h"
#include "AnimationUtils.h"
#include "Animation/AnimCompress_Automatic.h"
#include "Animation/AnimationSettings.h"

UAnimCompress_Automatic::UAnimCompress_Automatic(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Description = TEXT("Automatic");
	UAnimationSettings* AnimationSettings = UAnimationSettings::Get();
	MaxEndEffectorError = AnimationSettings->AlternativeCompressionThreshold;
	bTryFixedBitwiseCompression = AnimationSettings->bTryFixedBitwiseCompression;
	bTryPerTrackBitwiseCompression = AnimationSettings->bTryPerTrackBitwiseCompression;
	bTryLinearKeyRemovalCompression = AnimationSettings->bTryLinearKeyRemovalCompression;
	bTryIntervalKeyRemoval = AnimationSettings->bTryIntervalKeyRemoval;
	bRunCurrentDefaultCompressor = AnimationSettings->bFirstRecompressUsingCurrentOrDefault;
	bAutoReplaceIfExistingErrorTooGreat = AnimationSettings->bForceBelowThreshold;
	bRaiseMaxErrorToExisting = AnimationSettings->bRaiseMaxErrorToExisting;
}


void UAnimCompress_Automatic::DoReduction(UAnimSequence* AnimSeq, const TArray<FBoneData>& BoneData)
{
#if WITH_EDITORONLY_DATA
	FAnimationUtils::CompressAnimSequenceExplicit(
		AnimSeq,
		MaxEndEffectorError,
		false, // bOutput
		bRunCurrentDefaultCompressor,
		bAutoReplaceIfExistingErrorTooGreat,
		bRaiseMaxErrorToExisting,
		bTryFixedBitwiseCompression,
		bTryPerTrackBitwiseCompression,
		bTryLinearKeyRemovalCompression,
		bTryIntervalKeyRemoval);
	AnimSeq->CompressionScheme = static_cast<UAnimCompress*>( StaticDuplicateObject( AnimSeq->CompressionScheme, AnimSeq) );
#endif // WITH_EDITORONLY_DATA
}
