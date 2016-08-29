// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "EnginePrivate.h"
#include "AnimCompressionDerivedData.h"
#include "AnimationUtils.h"
#include "AnimEncoding.h"
#include "Animation/AnimCompress.h"
#include "Animation/AnimCompress_Automatic.h"
#include "AnimationCompression.h"

#if WITH_EDITOR

DECLARE_CYCLE_STAT(TEXT("Anim Compression (Derived Data)"), STAT_AnimCompressionDerivedData, STATGROUP_Anim);

FDerivedDataAnimationCompression::FDerivedDataAnimationCompression(UAnimSequence* InAnimSequence, TSharedPtr<FAnimCompressContext> InCompressContext, bool bInDoCompressionInPlace)
	: OriginalAnimSequence(InAnimSequence)
	, DuplicateSequence(nullptr)
	, CompressContext(InCompressContext)
	, bDoCompressionInPlace(bInDoCompressionInPlace)
{
	check(InAnimSequence != nullptr && InAnimSequence->GetSkeleton() != nullptr);
	InAnimSequence->AddToRoot(); //Keep this around until we are finished
}

FDerivedDataAnimationCompression::~FDerivedDataAnimationCompression()
{
	OriginalAnimSequence->RemoveFromRoot();
	if (DuplicateSequence)
	{
		DuplicateSequence->RemoveFromRoot();
	}
}

FString FDerivedDataAnimationCompression::GetPluginSpecificCacheKeySuffix() const
{
	enum { UE_ANIMCOMPRESSION_DERIVEDDATA_VER = 1 };

	const bool bCanBakeAdditive = OriginalAnimSequence->CanBakeAdditive();
	UAnimSequence* AdditiveBase = OriginalAnimSequence->RefPoseSeq;

	//Make up our content key consisting of:
	//	* Our plugin verison
	//	* Global animation compression version
	//	* Our raw data GUID
	//	* Our skeleton GUID: If our skeleton changes our compressed data may now be stale
	//	* Baked Additive Flag
	//	* Additive ref pose GUID or hardcoded string if not available
	//	* Compression Settings

	uint8 AdditiveSettings = bCanBakeAdditive ? (OriginalAnimSequence->RefPoseType << 4) + OriginalAnimSequence->AdditiveAnimType : 0;

	char AdditiveType = bCanBakeAdditive ? NibbleToTChar(OriginalAnimSequence->AdditiveAnimType) : '0';
	char RefType = bCanBakeAdditive ? NibbleToTChar(OriginalAnimSequence->RefPoseType) : '0';

	FString Ret = FString::Printf(TEXT("%i_%i_%s%s_%c%c%i_%s_%s"),
		(int32)UE_ANIMCOMPRESSION_DERIVEDDATA_VER,
		(int32)CURRENT_ANIMATION_ENCODING_PACKAGE_VERSION,
		*OriginalAnimSequence->RawDataGuid.ToString(),
		*OriginalAnimSequence->GetSkeleton()->GetGuid().ToString(),
		AdditiveType,
		RefType,
		OriginalAnimSequence->RefFrameIndex,
		(bCanBakeAdditive && AdditiveBase) ? *AdditiveBase->RawDataGuid.ToString() : TEXT("NoAdditiveBase"),
		*OriginalAnimSequence->CompressionScheme->MakeDDCKey()
		);

	return Ret;

}

struct FScopedAnimSequenceRawDataCache
{
	UAnimSequence* SrcAnim;
	TArray<FRawAnimSequenceTrack> RawAnimationData;
	TArray<FName> AnimationTrackNames;
	TArray<FTrackToSkeletonMap> TrackToSkeletonMapTable;
	FRawCurveTracks RawCurveData;
	bool bWasEmpty;

	FScopedAnimSequenceRawDataCache() : SrcAnim(nullptr), bWasEmpty(false){}
	~FScopedAnimSequenceRawDataCache()
	{
		if (SrcAnim)
		{
			RestoreTo(SrcAnim);
		}
	}

	void InitFrom(UAnimSequence* Src)
	{
#if WITH_EDITORONLY_DATA
		check(!SrcAnim);
		SrcAnim = Src;
		RawAnimationData = Src->RawAnimationData;
		bWasEmpty = RawAnimationData.Num() == 0;
		AnimationTrackNames = Src->AnimationTrackNames;
		TrackToSkeletonMapTable = Src->TrackToSkeletonMapTable;
		RawCurveData = Src->RawCurveData;
#endif
	}

	void RestoreTo(UAnimSequence* Src)
	{
#if WITH_EDITORONLY_DATA
		Src->RawAnimationData = MoveTemp(RawAnimationData);
		check(bWasEmpty || Src->RawAnimationData.Num() > 0);
		Src->AnimationTrackNames = MoveTemp(AnimationTrackNames);
		Src->TrackToSkeletonMapTable = MoveTemp(TrackToSkeletonMapTable);
		Src->RawCurveData = RawCurveData;
#endif
	}

};

bool FDerivedDataAnimationCompression::Build( TArray<uint8>& OutData )
{
	SCOPE_CYCLE_COUNTER(STAT_AnimCompressionDerivedData);
	UE_LOG(LogAnimationCompression, Log, TEXT("Building Anim DDC data for %s"), *OriginalAnimSequence->GetFullName());
	check(OriginalAnimSequence != NULL);

	UAnimSequence* AnimToOperateOn;

	if (!bDoCompressionInPlace)
	{
		DuplicateSequence = DuplicateObject<UAnimSequence>(OriginalAnimSequence, GetTransientPackage());
		DuplicateSequence->AddToRoot();
		AnimToOperateOn = DuplicateSequence;
	}
	else
	{
		AnimToOperateOn = OriginalAnimSequence;
	}

	bool bCompressionSuccessful = false;
	{
		FScopedAnimSequenceRawDataCache RawDataCache;
		if (AnimToOperateOn->CanBakeAdditive())
		{
			if (bDoCompressionInPlace)
			{
				RawDataCache.InitFrom(AnimToOperateOn);
			}

			AnimToOperateOn->BakeOutAdditiveIntoRawData();
		}

		AnimToOperateOn->CompressedTrackToSkeletonMapTable = AnimToOperateOn->TrackToSkeletonMapTable;
		AnimToOperateOn->CompressedCurveData = AnimToOperateOn->RawCurveData; //Curves don't actually get compressed, but could have additives baked in

#ifdef DO_CHECK
		FString CompressionName = AnimToOperateOn->CompressionScheme->GetFullName();
		const TCHAR* AAC = CompressContext.Get()->bAllowAlternateCompressor ? TEXT("true") : TEXT("false");
		const TCHAR* OutputStr = CompressContext.Get()->bOutput ? TEXT("true") : TEXT("false");
#endif

		AnimToOperateOn->CompressedByteStream.Reset();
		AnimToOperateOn->CompressedTrackOffsets.Reset();
		FAnimationUtils::CompressAnimSequence(AnimToOperateOn, *CompressContext.Get());
		bCompressionSuccessful = AnimToOperateOn->IsCompressedDataValid();

		ensureMsgf(bCompressionSuccessful, TEXT("Anim Compression failed for Sequence '%s' with compression scheme '%s': compressed data empty\n\tAnimIndex: %i\n\tMaxAnim:%i\n\tAllowAltCompressor:%s\n\tOutput:%s"), 
											*AnimToOperateOn->GetFullName(), 
											*CompressionName,
											CompressContext.Get()->AnimIndex,
											CompressContext.Get()->MaxAnimations,
											AAC,
											OutputStr);

		AnimToOperateOn->TemporaryAdditiveBaseAnimationData.Empty();
	}

	//Our compression scheme may change so copy the new one back
	if (OriginalAnimSequence != AnimToOperateOn)
	{
		OriginalAnimSequence->CompressionScheme = static_cast<UAnimCompress*>(StaticDuplicateObject(AnimToOperateOn->CompressionScheme, OriginalAnimSequence));
	}

	if (bCompressionSuccessful)
	{
		FMemoryWriter Ar(OutData, true);
		AnimToOperateOn->SerializeCompressedData(Ar, true); //Save out compressed
	}

	return bCompressionSuccessful;
}
#endif	//WITH_EDITOR
