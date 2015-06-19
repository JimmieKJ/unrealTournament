// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	AnimSequence.cpp: Skeletal mesh animation functions.
=============================================================================*/ 

#include "EnginePrivate.h"
#include "AnimationCompression.h"
#include "AnimEncoding.h"
#include "AnimationUtils.h"
#include "AnimationRuntime.h"
#include "TargetPlatform.h"
#include "Animation/AnimCompress.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "Animation/Rig.h"
#include "EditorFramework/AssetImportData.h"

#define USE_SLERP 0

/////////////////////////////////////////////////////
// FRawAnimSequenceTrackNativeDeprecated

//@deprecated with VER_REPLACED_LAZY_ARRAY_WITH_UNTYPED_BULK_DATA
struct FRawAnimSequenceTrackNativeDeprecated
{
	TArray<FVector> PosKeys;
	TArray<FQuat> RotKeys;
	friend FArchive& operator<<(FArchive& Ar, FRawAnimSequenceTrackNativeDeprecated& T)
	{
		return	Ar << T.PosKeys << T.RotKeys;
	}
};

/////////////////////////////////////////////////////
// FCurveTrack

/** Returns true if valid curve weight exists in the array*/
bool FCurveTrack::IsValidCurveTrack()
{
	bool bValid = false;

	if ( CurveName != NAME_None )
	{
		for (int32 I=0; I<CurveWeights.Num(); ++I)
		{
			// it has valid weight
			if (CurveWeights[I]>KINDA_SMALL_NUMBER)
			{
				bValid = true;
				break;
			}
		}
	}

	return bValid;
}

/** This is very simple cut to 1 key method if all is same since I see so many redundant same value in every frame 
 *  Eventually this can get more complicated 
 *  Will return true if compressed to 1. Return false otherwise
 **/
bool FCurveTrack::CompressCurveWeights()
{
	// if always 1, no reason to do this
	if (CurveWeights.Num() > 1)
	{
		bool bCompress = true;
		// first weight
		float FirstWeight = CurveWeights[0];

		for (int32 I=1; I<CurveWeights.Num(); ++I)
		{
			// see if my key is same as previous
			if (fabs(FirstWeight - CurveWeights[I]) > SMALL_NUMBER)
			{
				// if not same, just get out, you don't like to compress this to 1 key
				bCompress = false;
				break;
			}
		} 

		if (bCompress)
		{
			CurveWeights.Empty();
			CurveWeights.Add(FirstWeight);
			CurveWeights.Shrink();
		}

		return bCompress;
	}

	// nothing changed
	return false;
}

/////////////////////////////////////////////////////////////

// since we want this change for hot fix, I can't change header file, 
// next time move this to the header
float GetIntervalPerKey(int32 NumFrames, float SequenceLength) 
{
	return (NumFrames > 1) ? (SequenceLength / (NumFrames-1)) : MINIMUM_ANIMATION_LENGTH;
}

/////////////////////////////////////////////////////
// UAnimSequence

UAnimSequence::UAnimSequence(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, bEnableRootMotion(false)
	, RootMotionRootLock(ERootMotionRootLock::RefPose)
	, bRootMotionSettingsCopiedFromMontage(false)
{

	RateScale = 1.0;
}

SIZE_T UAnimSequence::GetResourceSize(EResourceSizeMode::Type Mode)
{
	const int32 ResourceSize = CompressedTrackOffsets.Num() == 0 ? GetApproxRawSize() : GetApproxCompressedSize();
	return ResourceSize;
}

void UAnimSequence::GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const
{
#if WITH_EDITORONLY_DATA
	if (AssetImportData)
	{
		OutTags.Add( FAssetRegistryTag(SourceFileTagName(), AssetImportData->SourceFilePath, FAssetRegistryTag::TT_Hidden) );
	}
#endif

	Super::GetAssetRegistryTags(OutTags);
}

int32 UAnimSequence::GetApproxRawSize() const
{
	int32 Total = sizeof(FRawAnimSequenceTrack) * RawAnimationData.Num();
	for (int32 i=0;i<RawAnimationData.Num();++i)
	{
		const FRawAnimSequenceTrack& RawTrack = RawAnimationData[i];
		Total +=
			sizeof( FVector ) * RawTrack.PosKeys.Num() +
			sizeof( FQuat ) * RawTrack.RotKeys.Num() + 
			sizeof( FVector ) * RawTrack.ScaleKeys.Num(); 
	}
	return Total;
}


int32 UAnimSequence::GetApproxReducedSize() const
{
	int32 Total =
		sizeof(FTranslationTrack) * TranslationData.Num() +
		sizeof(FRotationTrack) * RotationData.Num() + 
		sizeof(FScaleTrack) * ScaleData.Num();

	for (int32 i=0;i<TranslationData.Num();++i)
	{
		const FTranslationTrack& TranslationTrack = TranslationData[i];
		Total +=
			sizeof( FVector ) * TranslationTrack.PosKeys.Num() +
			sizeof( float ) * TranslationTrack.Times.Num();
	}

	for (int32 i=0;i<RotationData.Num();++i)
	{
		const FRotationTrack& RotationTrack = RotationData[i];
		Total +=
			sizeof( FQuat ) * RotationTrack.RotKeys.Num() +
			sizeof( float ) * RotationTrack.Times.Num();
	}

	for (int32 i=0;i<ScaleData.Num();++i)
	{
		const FScaleTrack& ScaleTrack = ScaleData[i];
		Total +=
			sizeof( FVector ) * ScaleTrack.ScaleKeys.Num() +
			sizeof( float ) * ScaleTrack.Times.Num();
	}
	return Total;
}



int32 UAnimSequence::GetApproxCompressedSize() const
{
	const int32 Total = sizeof(int32)*CompressedTrackOffsets.Num() + CompressedByteStream.Num() + CompressedScaleOffsets.GetMemorySize();
	return Total;
}

/**
 * Deserializes old compressed track formats from the specified archive.
 */
static void LoadOldCompressedTrack(FArchive& Ar, FCompressedTrack& Dst, int32 ByteStreamStride)
{
	// Serialize from the archive to a buffer.
	int32 NumBytes = 0;
	Ar << NumBytes;

	TArray<uint8> SerializedData;
	SerializedData.Empty( NumBytes );
	SerializedData.AddUninitialized( NumBytes );
	Ar.Serialize( SerializedData.GetData(), NumBytes );

	// Serialize the key times.
	Ar << Dst.Times;

	// Serialize mins and ranges.
	Ar << Dst.Mins[0] << Dst.Mins[1] << Dst.Mins[2];
	Ar << Dst.Ranges[0] << Dst.Ranges[1] << Dst.Ranges[2];
}

void UAnimSequence::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	FStripDataFlags StripFlags( Ar );
	if( !StripFlags.IsEditorDataStripped() )
	{
		Ar << RawAnimationData;
#if WITH_EDITORONLY_DATA
		if (!Ar.IsCooking())
		{
			if (Ar.UE4Ver() >= VER_UE4_ANIMATION_ADD_TRACKCURVES)
			{
				Ar << SourceRawAnimationData;
			}
		}
#endif // WITH_EDITORONLY_DATA
	}

	if ( Ar.IsLoading() )
	{
		// Serialize the compressed byte stream from the archive to the buffer.
		int32 NumBytes;
		Ar << NumBytes;

		TArray<uint8> SerializedData;
		SerializedData.Empty( NumBytes );
		SerializedData.AddUninitialized( NumBytes );
		Ar.Serialize( SerializedData.GetData(), NumBytes );

		// Swap the buffer into the byte stream.
		FMemoryReader MemoryReader( SerializedData, true );
		MemoryReader.SetByteSwapping( Ar.ForceByteSwapping() );

		// we must know the proper codecs to use
		AnimationFormat_SetInterfaceLinks(*this);

		// and then use the codecs to byte swap
		check( RotationCodec != NULL );
		((AnimEncoding*)RotationCodec)->ByteSwapIn(*this, MemoryReader);
	}
	else if( Ar.IsSaving() || Ar.IsCountingMemory() )
	{
		// Swap the byte stream into a buffer.
		TArray<uint8> SerializedData;

		// we must know the proper codecs to use
		AnimationFormat_SetInterfaceLinks(*this);

		// and then use the codecs to byte swap
		check( RotationCodec != NULL );
		((AnimEncoding*)RotationCodec)->ByteSwapOut(*this, SerializedData, Ar.ForceByteSwapping());

		// Make sure the entire byte stream was serialized.
		//check( CompressedByteStream.Num() == SerializedData.Num() );

		// Serialize the buffer to archive.
		int32 Num = SerializedData.Num();
		Ar << Num;
		Ar.Serialize( SerializedData.GetData(), SerializedData.Num() );

		// Count compressed data.
		Ar.CountBytes( SerializedData.Num(), SerializedData.Num() );
	}

#if WITH_EDITORONLY_DATA
	// SourceFilePath and SourceFileTimestamp were moved into a subobject
	if ( Ar.IsLoading() && Ar.UE4Ver() < VER_UE4_ADDED_FBX_ASSET_IMPORT_DATA )
	{
		if ( AssetImportData == NULL )
		{
			AssetImportData = NewObject<UAssetImportData>(this);
		}
		
		AssetImportData->SourceFilePath = SourceFilePath_DEPRECATED;
		AssetImportData->SourceFileTimestamp = SourceFileTimestamp_DEPRECATED;
		SourceFilePath_DEPRECATED = TEXT("");
		SourceFileTimestamp_DEPRECATED = TEXT("");
	}
#endif // WITH_EDITORONLY_DATA
}

int32 UAnimSequence::GetNumberOfTracks() const
{
	// this shows how many tracks are used and stored in this animation
	return TrackToSkeletonMapTable.Num();
}

#if WITH_EDITOR
bool UAnimSequence::IsValidToPlay() const
{
	// make sure sequence length is valid and raw animation data exists, and compressed
	return ( SequenceLength > 0.f && RawAnimationData.Num() > 0 && CompressedTrackOffsets.Num() > 0 );
}
#endif

void UAnimSequence::PreSave()
{
#if WITH_EDITOR
	// we have to bake it if it's not baked
	if (DoesNeedRebake())
	{
		BakeTrackCurvesToRawAnimation();
	}

	// make sure if it does contain transform curvesm it contains source data
	check (!DoesContainTransformCurves() || SourceRawAnimationData.Num() != 0);
#endif

	Super::PreSave();
}

void UAnimSequence::PostLoad()
{
#if WITH_EDITOR
	// I have to do this first thing in here
	// so that remove all NaNs before even being read
	if(GetLinkerUE4Version() < VER_UE4_ANIMATION_REMOVE_NANS)
	{
		RemoveNaNTracks();
	}
#endif // WITH_EDITOR

	Super::PostLoad();

	// Ensure notifies are sorted.
	SortNotifies();

	// No animation data is found. Warn - this should check before we check CompressedTrackOffsets size
	// Otherwise, we'll see empty data set crashing game due to no CompressedTrackOffsets
	// You can't check RawAnimationData size since it gets removed during cooking
	if ( NumFrames == 0 )
	{
		UE_LOG(LogAnimation, Warning, TEXT("No animation data exists for sequence %s (%s)"), *GetName(), (GetOuter() ? *GetOuter()->GetFullName() : *GetFullName()) );
#if WITH_EDITOR
		if( GIsEditor )
		{
			UE_LOG(LogAnimation, Warning, TEXT("Removing bad AnimSequence (%s) from %s."), *GetName(), (GetOuter() ? *GetOuter()->GetFullName() : *GetFullName()) );
		}
#endif
	}
	// @remove temp hack for fixing length
	// @todo need to fix importer/editing feature
	else if ( SequenceLength == 0.f )
	{
		ensure(NumFrames == 1);
		SequenceLength = MINIMUM_ANIMATION_LENGTH;
	}
	// Raw data exists, but missing compress animation data
	else if( CompressedTrackOffsets.Num() == 0 && RawAnimationData.Num() > 0)
	{
		if (!FPlatformProperties::HasEditorOnlyData())
		{
			// Never compress on consoles.
			UE_LOG(LogAnimation, Fatal, TEXT("No animation compression exists for sequence %s (%s)"), *GetName(), (GetOuter() ? *GetOuter()->GetFullName() : *GetFullName()) );
		}
		else
		{
			UE_LOG(LogAnimation, Warning, TEXT("No animation compression exists for sequence %s (%s)"), *GetName(), (GetOuter() ? *GetOuter()->GetFullName() : *GetFullName()) );
			// No animation compression, recompress using default settings.
			FAnimationUtils::CompressAnimSequence(this, false, false);
		}
	}

	static bool ForcedRecompressionSetting = FAnimationUtils::GetForcedRecompressionSetting();

	// Recompress the animation if it was encoded with an old package set
	// or we are being forced to do so
	if (EncodingPkgVersion != CURRENT_ANIMATION_ENCODING_PACKAGE_VERSION ||
		ForcedRecompressionSetting)
	{
		if (FPlatformProperties::RequiresCookedData())
		{
			if (EncodingPkgVersion != CURRENT_ANIMATION_ENCODING_PACKAGE_VERSION)
			{
				// Never compress on platforms that don't support it.
				UE_LOG(LogAnimation, Fatal, TEXT("Animation compression method out of date for sequence %s"), *GetName() );
				CompressedTrackOffsets.Empty(0);
				CompressedByteStream.Empty(0);
				CompressedScaleOffsets.Empty(0);
			}
		}
		else
		{
			FAnimationUtils::CompressAnimSequence(this, true, false);
		}
	}

	// If we're in the game and compressed animation data exists, whack the raw data.
	if( IsRunningGame() )
	{
		if( RawAnimationData.Num() > 0  && CompressedTrackOffsets.Num() > 0 )
		{
#if 0//@todo.Cooker/Package...
			// Don't do this on consoles; raw animation data should have been stripped during cook!
			UE_LOG(LogAnimation, Fatal, TEXT("Cooker did not strip raw animation from sequence %s"), *GetName() );
#else
			// Remove raw animation data.
			for ( int32 TrackIndex = 0 ; TrackIndex < RawAnimationData.Num() ; ++TrackIndex )
			{
				FRawAnimSequenceTrack& RawTrack = RawAnimationData[TrackIndex];
				RawTrack.PosKeys.Empty();
				RawTrack.RotKeys.Empty();
				RawTrack.ScaleKeys.Empty();
			}
			
			RawAnimationData.Empty();
#endif
		}
	}

#if WITH_EDITORONLY_DATA
	bWasCompressedWithoutTranslations = false; //@todoanim: @fixmelh : AnimRotationOnly - GetAnimSet()->bAnimRotationOnly;
#endif // WITH_EDITORONLY_DATA

	// setup the Codec interfaces
	AnimationFormat_SetInterfaceLinks(*this);

#if WITH_EDITOR
	// save track data for the case
	VerifyTrackMap();
#endif

	if( IsRunningGame() )
	{
		// this probably will not show newly created animations in PIE but will show them in the game once they have been saved off
		INC_DWORD_STAT_BY( STAT_AnimationMemory, GetResourceSize(EResourceSizeMode::Exclusive) );
	}

	{
		LOG_SCOPE_VERBOSITY_OVERRIDE(LogAnimation, ELogVerbosity::Warning);
 		// convert animnotifies
 		for (int32 I=0; I<Notifies.Num(); ++I)
 		{
 			if (Notifies[I].Notify!=NULL)
 			{
				FString Label = Notifies[I].Notify->GetClass()->GetName();
				Label = Label.Replace(TEXT("AnimNotify_"), TEXT(""), ESearchCase::CaseSensitive);
				Notifies[I].NotifyName = FName(*Label);
 			}
 		}
	}

	for(FAnimNotifyEvent& Notify : Notifies)
	{
		if(Notify.DisplayTime_DEPRECATED != 0.0f)
		{
			Notify.Clear();
			Notify.LinkSequence(this, Notify.DisplayTime_DEPRECATED);
		}
		else
		{
			Notify.LinkSequence(this, Notify.GetTime());
		}
	
		if(Notify.Duration != 0.0f)
		{
			Notify.EndLink.LinkSequence(this, Notify.GetTime() + Notify.Duration);
		}
	}

#if WITH_EDITOR
	if (USkeleton * CurrentSkeleton = GetSkeleton())
	{
		// Get the name mapping object for curves
		FSmartNameMapping* NameMapping = CurrentSkeleton->SmartNames.GetContainer(USkeleton::AnimTrackCurveMappingName);

		TArray<FTransformCurve*> UnlinkedCurves;
		for(FTransformCurve& Curve : RawCurveData.TransformCurves)
		{
			if(!NameMapping->Exists(Curve.LastObservedName))
			{
				// The skeleton doesn't know our name. Use the last observed name that was saved with the
				// curve to create a new name. This can happen if a user saves an animation but not a skeleton
				// either when updating the assets or editing the curves within.
				UnlinkedCurves.Add(&Curve);
			}
		}

		for(FTransformCurve* Curve : UnlinkedCurves)
		{
			NameMapping->AddOrFindName(Curve->LastObservedName, Curve->CurveUid);
		}
	}
#endif // WITH_EDITOR
}

#if WITH_EDITOR
void UAnimSequence::VerifyTrackMap(USkeleton* MySkeleton)
{
	USkeleton* UseSkeleton = (MySkeleton)? MySkeleton: GetSkeleton();

	if(AnimationTrackNames.Num() != TrackToSkeletonMapTable.Num() && UseSkeleton!=NULL)
	{
		UE_LOG(LogAnimation, Warning, TEXT("RESAVE ANIMATION NEEDED(%s): Fixing track names."), *GetName());

		const TArray<FBoneNode>& BoneTree = UseSkeleton->GetBoneTree();
		AnimationTrackNames.Empty();
		AnimationTrackNames.AddUninitialized(TrackToSkeletonMapTable.Num());
		for(int32 I=0; I<TrackToSkeletonMapTable.Num(); ++I)
		{
			const FTrackToSkeletonMap& TrackMap = TrackToSkeletonMapTable[I];
			AnimationTrackNames[I] = UseSkeleton->GetReferenceSkeleton().GetBoneName(TrackMap.BoneTreeIndex);
		}
	}
	else if (UseSkeleton != NULL)
	{
		int32 NumTracks = AnimationTrackNames.Num();
		int32 NumSkeletonBone = UseSkeleton->GetReferenceSkeleton().GetNum();

		bool bNeedsFixing = false;
		// verify all tracks are still valid
		for(int32 TrackIndex=0; TrackIndex<NumTracks; TrackIndex++)
		{
			int32 SkeletonBoneIndex = TrackToSkeletonMapTable[TrackIndex].BoneTreeIndex;
			// invalid index found
			if (NumSkeletonBone <= SkeletonBoneIndex)
			{
				// if one is invalid, fix up for all.
				// you don't know what index got messed up
				bNeedsFixing = true;
				break;
			}
		}

		if(bNeedsFixing)
		{
			UE_LOG(LogAnimation, Warning, TEXT("RESAVE ANIMATION NEEDED(%s): Fixing track index."), *GetName());

			const TArray<FBoneNode>& BoneTree = UseSkeleton->GetBoneTree();
			for(int32 I=NumTracks-1; I>=0; --I)
			{
				int32 BoneTreeIndex = UseSkeleton->GetReferenceSkeleton().FindBoneIndex(AnimationTrackNames[I]);
				if (BoneTreeIndex == INDEX_NONE)
				{
					RemoveTrack(I);
				}
				else
				{
					TrackToSkeletonMapTable[I].BoneTreeIndex = BoneTreeIndex;
				}
			}
		}
	}
}

#endif // WITH_EDITOR
void UAnimSequence::BeginDestroy()
{
	Super::BeginDestroy();

	// clear any active codec links
	RotationCodec = NULL;
	TranslationCodec = NULL;

	if( IsRunningGame() )
	{
		DEC_DWORD_STAT_BY( STAT_AnimationMemory, GetResourceSize(EResourceSizeMode::Exclusive) );
	}
}

#if WITH_EDITOR
void UAnimSequence::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if(!IsTemplate())
	{
		// Make sure package is marked dirty when doing stuff like adding/removing notifies
		MarkPackageDirty();
	}

	if (AdditiveAnimType != AAT_None)
	{
		if (RefPoseType == ABPT_None)
		{
			// slate will take care of change
			RefPoseType = ABPT_RefPose;
		}
	}

	if (RefPoseSeq != NULL)
	{
		if (RefPoseSeq->GetSkeleton() != GetSkeleton()) // @todo this may require to be changed when hierarchy of skeletons is introduced
		{
			RefPoseSeq = NULL;
		}
	}

	// @Todo fix me: This is temporary fix to make sure they always have compressed data
	if (CompressedTrackOffsets.Num() == 0 && RawAnimationData.Num() > 0)
	{
		PostProcessSequence();
	}
}

void UAnimSequence::PostDuplicate(bool bDuplicateForPIE)
{
	// if transform curve exists, mark as bake
	if (DoesContainTransformCurves())
	{
		bNeedsRebake = true;
	}

	Super::PostDuplicate(bDuplicateForPIE);
}
#endif // WITH_EDITOR

// @todo DB: Optimize!
template<typename TimeArray>
static int32 FindKeyIndex(float Time, const TimeArray& Times)
{
	int32 FoundIndex = 0;
	for ( int32 Index = 0 ; Index < Times.Num() ; ++Index )
	{
		const float KeyTime = Times(Index);
		if ( Time >= KeyTime )
		{
			FoundIndex = Index;
		}
		else
		{
			break;
		}
	}
	return FoundIndex;
}

void UAnimSequence::GetBoneTransform(FTransform& OutAtom, int32 TrackIndex, float Time, bool bUseRawData) const
{
	// If the caller didn't request that raw animation data be used . . .
	if ( !bUseRawData )
	{
		if ( CompressedTrackOffsets.Num() > 0 )
		{
			AnimationFormat_GetBoneAtom( OutAtom, *this, TrackIndex, Time );
			return;
		}
	}

	ExtractBoneTransform(RawAnimationData, OutAtom, TrackIndex, Time);
}

void UAnimSequence::ExtractBoneTransform(const TArray<struct FRawAnimSequenceTrack>& InRawAnimationData, FTransform& OutAtom, int32 TrackIndex, float Time) const
{
	// Bail out if the animation data doesn't exists (e.g. was stripped by the cooker).
	if(InRawAnimationData.Num() == 0)
	{
		UE_LOG(LogAnimation, Log, TEXT("UAnimSequence::GetBoneTransform : No anim data in AnimSequence!"));
		OutAtom.SetIdentity();
		return;
	}

	const FRawAnimSequenceTrack& RawTrack = InRawAnimationData[TrackIndex];

	// Bail out (with rather wacky data) if data is empty for some reason.
	if(RawTrack.PosKeys.Num() == 0 || RawTrack.RotKeys.Num() == 0)
	{
		UE_LOG(LogAnimation, Log, TEXT("UAnimSequence::GetBoneTransform : No anim data in AnimSequence!"));
		OutAtom.SetIdentity();
		return;
	}

	int32 KeyIndex1, KeyIndex2;
	float Alpha;
	FAnimationRuntime::GetKeyIndicesFromTime(KeyIndex1, KeyIndex2, Alpha, Time, NumFrames, SequenceLength);
	// @Todo fix me: this change is not good, it has lots of branches. But we'd like to save memory for not saving scale if no scale change exists
	const bool bHasScaleKey = (RawTrack.ScaleKeys.Num() > 0);
	static const FVector DefaultScale3D = FVector(1.f);

	if(Alpha <= 0.f)
	{
		const int32 PosKeyIndex1 = FMath::Min(KeyIndex1, RawTrack.PosKeys.Num()-1);
		const int32 RotKeyIndex1 = FMath::Min(KeyIndex1, RawTrack.RotKeys.Num()-1);
		if(bHasScaleKey)
		{
			const int32 ScaleKeyIndex1 = FMath::Min(KeyIndex1, RawTrack.ScaleKeys.Num()-1);
			OutAtom = FTransform(RawTrack.RotKeys[RotKeyIndex1], RawTrack.PosKeys[PosKeyIndex1], RawTrack.ScaleKeys[ScaleKeyIndex1]);
		}
		else
		{
			OutAtom = FTransform(RawTrack.RotKeys[RotKeyIndex1], RawTrack.PosKeys[PosKeyIndex1], DefaultScale3D);
		}
		return;
	}
	else if(Alpha >= 1.f)
	{
		const int32 PosKeyIndex2 = FMath::Min(KeyIndex2, RawTrack.PosKeys.Num()-1);
		const int32 RotKeyIndex2 = FMath::Min(KeyIndex2, RawTrack.RotKeys.Num()-1);
		if(bHasScaleKey)
		{
			const int32 ScaleKeyIndex2 = FMath::Min(KeyIndex2, RawTrack.ScaleKeys.Num()-1);
			OutAtom = FTransform(RawTrack.RotKeys[RotKeyIndex2], RawTrack.PosKeys[PosKeyIndex2], RawTrack.ScaleKeys[ScaleKeyIndex2]);
		}
		else
		{
			OutAtom = FTransform(RawTrack.RotKeys[RotKeyIndex2], RawTrack.PosKeys[PosKeyIndex2], DefaultScale3D);
		}
		return;
	}

	const int32 PosKeyIndex1 = FMath::Min(KeyIndex1, RawTrack.PosKeys.Num()-1);
	const int32 RotKeyIndex1 = FMath::Min(KeyIndex1, RawTrack.RotKeys.Num()-1);

	const int32 PosKeyIndex2 = FMath::Min(KeyIndex2, RawTrack.PosKeys.Num()-1);
	const int32 RotKeyIndex2 = FMath::Min(KeyIndex2, RawTrack.RotKeys.Num()-1);

	FTransform KeyAtom1, KeyAtom2;

	if(bHasScaleKey)
	{
		const int32 ScaleKeyIndex1 = FMath::Min(KeyIndex1, RawTrack.ScaleKeys.Num()-1);
		const int32 ScaleKeyIndex2 = FMath::Min(KeyIndex2, RawTrack.ScaleKeys.Num()-1);

		KeyAtom1 = FTransform(RawTrack.RotKeys[RotKeyIndex1], RawTrack.PosKeys[PosKeyIndex1], RawTrack.ScaleKeys[ScaleKeyIndex1]);
		KeyAtom2 = FTransform(RawTrack.RotKeys[RotKeyIndex2], RawTrack.PosKeys[PosKeyIndex2], RawTrack.ScaleKeys[ScaleKeyIndex2]);
	}
	else
	{
		KeyAtom1 = FTransform(RawTrack.RotKeys[RotKeyIndex1], RawTrack.PosKeys[PosKeyIndex1], DefaultScale3D);
		KeyAtom2 = FTransform(RawTrack.RotKeys[RotKeyIndex2], RawTrack.PosKeys[PosKeyIndex2], DefaultScale3D);
	}

	// 	UE_LOG(LogAnimation, Log, TEXT(" *  *  *  Position. PosKeyIndex1: %3d, PosKeyIndex2: %3d, Alpha: %f"), PosKeyIndex1, PosKeyIndex2, Alpha);
	// 	UE_LOG(LogAnimation, Log, TEXT(" *  *  *  Rotation. RotKeyIndex1: %3d, RotKeyIndex2: %3d, Alpha: %f"), RotKeyIndex1, RotKeyIndex2, Alpha);

	OutAtom.Blend(KeyAtom1, KeyAtom2, Alpha);
	OutAtom.NormalizeRotation();
}

void UAnimSequence::OnAssetPlayerTickedInternal(FAnimAssetTickContext &Context, const float PreviousTime, const float MoveDelta, const FAnimTickRecord &Instance, class UAnimInstance* InstanceOwner) const
{
	Super::OnAssetPlayerTickedInternal(Context, PreviousTime, MoveDelta, Instance, InstanceOwner);

	if (bEnableRootMotion)
	{
		Context.RootMotionMovementParams.Accumulate(ExtractRootMotion(PreviousTime, MoveDelta, Instance.bLooping));
	}
}

FTransform UAnimSequence::ExtractRootTrackTransform(float Pos, const FBoneContainer * RequiredBones) const
{
	// we assume root is in first data if available = SkeletonIndex == 0 && BoneTreeIndex == 0)
	if ((TrackToSkeletonMapTable.Num() > 0) && (TrackToSkeletonMapTable[0].BoneTreeIndex == 0) )
	{
		// if we do have root data, then return root data
		FTransform RootTransform;
		GetBoneTransform(RootTransform, 0, Pos, false );
		return RootTransform;
	}

	// Fallback to root bone from reference skeleton.
	if( RequiredBones )
	{
		const FReferenceSkeleton& RefSkeleton = RequiredBones->GetReferenceSkeleton();
		if( RefSkeleton.GetNum() > 0 )
		{
			return RefSkeleton.GetRefBonePose()[0];
		}
	}

	USkeleton * MySkeleton = GetSkeleton();
	// If we don't have a RequiredBones array, get root bone from default skeleton.
	if( !RequiredBones &&  MySkeleton )
	{
		const FReferenceSkeleton& RefSkeleton = MySkeleton->GetReferenceSkeleton();
		if( RefSkeleton.GetNum() > 0 )
		{
			return RefSkeleton.GetRefBonePose()[0];
		}
	}

	// Otherwise, use identity.
	return FTransform::Identity;
}

FTransform UAnimSequence::ExtractRootMotion(float StartTime, float DeltaTime, bool bAllowLooping) const
{
	FRootMotionMovementParams RootMotionParams;

	if (DeltaTime != 0.f)
	{
		bool const bPlayingBackwards = (DeltaTime < 0.f);

		float PreviousPosition = StartTime;
		float CurrentPosition = StartTime;
		float DesiredDeltaMove = DeltaTime;

		do
		{
			// Disable looping here. Advance to desired position, or beginning / end of animation 
			const ETypeAdvanceAnim AdvanceType = FAnimationRuntime::AdvanceTime(false, DesiredDeltaMove, CurrentPosition, SequenceLength);

			// Verify position assumptions
			check(bPlayingBackwards ? (CurrentPosition <= PreviousPosition) : (CurrentPosition >= PreviousPosition));

			RootMotionParams.Accumulate(ExtractRootMotionFromRange(PreviousPosition, CurrentPosition));

			// If we've hit the end of the animation, and we're allowed to loop, keep going.
			if ((AdvanceType == ETAA_Finished) && bAllowLooping)
			{
				const float ActualDeltaMove = (CurrentPosition - PreviousPosition);
				DesiredDeltaMove -= ActualDeltaMove;

				PreviousPosition = bPlayingBackwards ? SequenceLength : 0.f;
				CurrentPosition = PreviousPosition;
			}
			else
			{
				break;
			}
		} while (true);
	}

	return RootMotionParams.RootMotionTransform;
}

FTransform UAnimSequence::ExtractRootMotionFromRange(float StartTrackPosition, float EndTrackPosition) const
{
	FTransform InitialTransform = ExtractRootTrackTransform(0.f, NULL);
	FTransform StartTransform = ExtractRootTrackTransform(StartTrackPosition, NULL);
	FTransform EndTransform = ExtractRootTrackTransform(EndTrackPosition, NULL);

	// Transform to Component Space Rotation (inverse root transform from first frame)
	const FTransform RootToComponentRot = FTransform(InitialTransform.GetRotation().Inverse());
	StartTransform = RootToComponentRot * StartTransform;
	EndTransform = RootToComponentRot * EndTransform;

	return EndTransform.GetRelativeTransform(StartTransform);
}

void UAnimSequence::GetAnimationPose(FTransformArrayA2& OutAtoms, const FBoneContainer& RequiredBones, const FAnimExtractContext& ExtractionContext) const
{
	// @todo anim: if compressed and baked in the future, we don't have to do this 
	if( IsValidAdditive() ) 
	{
		if (AdditiveAnimType == AAT_LocalSpaceBase)
		{
			GetBonePose_Additive(OutAtoms, RequiredBones, ExtractionContext);
		}
		else if ( AdditiveAnimType == AAT_RotationOffsetMeshSpace )
		{
			GetBonePose_AdditiveMeshRotationOnly(OutAtoms, RequiredBones, ExtractionContext);
		}
	}
	else
	{
		GetBonePose(OutAtoms, RequiredBones, ExtractionContext);
	}
}

void UAnimSequence::ResetRootBoneForRootMotion(FTransformArrayA2& BoneTransforms, const FBoneContainer& RequiredBones, ERootMotionRootLock::Type InRootMotionRootLock) const
{
	switch (InRootMotionRootLock)
	{
		case ERootMotionRootLock::AnimFirstFrame: BoneTransforms[0] = ExtractRootTrackTransform(0.f, &RequiredBones); break;
		case ERootMotionRootLock::Zero: BoneTransforms[0] = FTransform::Identity; break;
		default:
		case ERootMotionRootLock::RefPose: BoneTransforms[0] = RequiredBones.GetRefPoseArray()[0]; break;
	}
}

void UAnimSequence::GetBonePose(FTransformArrayA2& OutAtoms, const FBoneContainer& RequiredBones, const FAnimExtractContext& ExtractionContext) const
{
	USkeleton* MySkeleton = GetSkeleton();
	if (!MySkeleton)
	{
		FAnimationRuntime::FillWithRefPose(OutAtoms, RequiredBones);
		return;
	}

	// if retargeting is disabled, we initialize pose with 'Retargeting Source' ref pose.
	bool const bDisableRetargeting = RequiredBones.GetDisableRetargeting();
	if (bDisableRetargeting)
	{
		TArray<FTransform> const& AuthoredOnRefSkeleton = MySkeleton->GetRefLocalPoses(RetargetSource);
		TArray<FBoneIndexType> const& RequireBonesIndexArray = RequiredBones.GetBoneIndicesArray();
		TArray<int32> const& PoseToSkeletonBoneIndexArray = RequiredBones.GetPoseToSkeletonBoneIndexArray();
	
		// Allocate output and make sure it's the right size.
		int32 const NumPoseBones = RequiredBones.GetNumBones();
		OutAtoms.Empty(NumPoseBones);
		OutAtoms.AddUninitialized(NumPoseBones);

		int32 const NumRequiredBones = RequireBonesIndexArray.Num();
		for (int32 ArrayIndex = 0; ArrayIndex < NumRequiredBones; ArrayIndex++)
		{
			int32 const& PoseBoneIndex = RequireBonesIndexArray[ArrayIndex];
			int32 const& SkeletonBoneIndex = PoseToSkeletonBoneIndexArray[PoseBoneIndex];

			// Pose bone index should always exist in Skeleton
			checkSlow(SkeletonBoneIndex != INDEX_NONE);
			OutAtoms[PoseBoneIndex] = AuthoredOnRefSkeleton[SkeletonBoneIndex];
		}
	}
	else
	{
		// initialize with ref-pose
		FAnimationRuntime::FillWithRefPose(OutAtoms, RequiredBones);
	}

	int32 const NumTracks = GetNumberOfTracks();
	if (NumTracks == 0)
	{
		return;
	}

#if WITH_EDITOR
	// this happens only with editor data
	// Slower path for disable retargeting, that's only used in editor and for debugging.
	if (bDisableRetargeting || RequiredBones.ShouldUseRawData() || RequiredBones.ShouldUseSourceData())
	{
		TArray<int32> const& SkeletonToPoseBoneIndexArray = RequiredBones.GetSkeletonToPoseBoneIndexArray();
		const TArray<FRawAnimSequenceTrack>* AnimationData = NULL;
		
		if (RequiredBones.ShouldUseSourceData() && SourceRawAnimationData.Num() > 0)
		{
			AnimationData = &SourceRawAnimationData;
		}
		else
		{
			AnimationData = &RawAnimationData;
		}

		for(int32 TrackIndex=0; TrackIndex<NumTracks; TrackIndex++)
		{
			const int32 SkeletonBoneIndex = GetSkeletonIndexFromTrackIndex(TrackIndex);
			// not sure it's safe to assume that SkeletonBoneIndex can never be INDEX_NONE
			if( (SkeletonBoneIndex != INDEX_NONE) && (SkeletonBoneIndex < MAX_BONES) )
			{
				const int32 PoseBoneIndex = SkeletonToPoseBoneIndexArray[SkeletonBoneIndex];
				if(PoseBoneIndex != INDEX_NONE)
				{
					// extract animation
					ExtractBoneTransform(*AnimationData, OutAtoms[PoseBoneIndex], TrackIndex, ExtractionContext.CurrentTime);

					if(!bDisableRetargeting)
					{
						RetargetBoneTransform(OutAtoms[PoseBoneIndex], SkeletonBoneIndex, PoseBoneIndex, RequiredBones);
					}
				}
			}
		}

		if( ExtractionContext.bExtractRootMotion && bEnableRootMotion )
		{
			ResetRootBoneForRootMotion(OutAtoms, RequiredBones, RootMotionRootLock);
		}
		return;
	}
#endif // WITH_EDITOR

	TArray<FBoneNode> const& BoneTree = MySkeleton->GetBoneTree();
	TArray<int32> const& SkeletonToPoseBoneIndexArray = RequiredBones.GetSkeletonToPoseBoneIndexArray();

	//@TODO:@ANIMATION: These should be memstack allocated - very heavy
	BoneTrackArray RotationScalePairs;
	BoneTrackArray TranslationPairs;
	BoneTrackArray AnimScaleRetargetingPairs;
	BoneTrackArray AnimRelativeRetargetingPairs;

	// build a list of desired bones
	RotationScalePairs.Empty(NumTracks);
	TranslationPairs.Empty(NumTracks);
	AnimScaleRetargetingPairs.Empty(NumTracks);
	AnimRelativeRetargetingPairs.Empty(NumTracks);

	// Optimization: assuming first index is root bone. That should always be the case in Skeletons.
	checkSlow( (SkeletonToPoseBoneIndexArray[0] == 0) );
	// this is not guaranteed for AnimSequences though... If Root is not animated, Track will not exist.
	const bool bFirstTrackIsRootBone = (GetSkeletonIndexFromTrackIndex(0) == 0);

	// Handle root bone separately if it is track 0. so we start w/ Index 1.
	for(int32 TrackIndex=(bFirstTrackIsRootBone ? 1 : 0); TrackIndex<NumTracks; TrackIndex++)
	{
		const int32 SkeletonBoneIndex = GetSkeletonIndexFromTrackIndex(TrackIndex);
		// not sure it's safe to assume that SkeletonBoneIndex can never be INDEX_NONE
		if( SkeletonBoneIndex != INDEX_NONE )
		{
			const int32 PoseBoneIndex = SkeletonToPoseBoneIndexArray[SkeletonBoneIndex];
			if(PoseBoneIndex != INDEX_NONE)
			{
				checkSlow(PoseBoneIndex < RequiredBones.GetNumBones());
				RotationScalePairs.Add(BoneTrackPair(PoseBoneIndex, TrackIndex));

				// Skip extracting translation component for EBoneTranslationRetargetingMode::Skeleton.
				switch(BoneTree[SkeletonBoneIndex].TranslationRetargetingMode)
				{
				case EBoneTranslationRetargetingMode::Animation:
				TranslationPairs.Add(BoneTrackPair(PoseBoneIndex, TrackIndex));
				break;
				case EBoneTranslationRetargetingMode::AnimationScaled:
				TranslationPairs.Add(BoneTrackPair(PoseBoneIndex, TrackIndex));
				AnimScaleRetargetingPairs.Add(BoneTrackPair(PoseBoneIndex, SkeletonBoneIndex));
				break;
				case EBoneTranslationRetargetingMode::AnimationRelative:
				TranslationPairs.Add(BoneTrackPair(PoseBoneIndex, TrackIndex));
				AnimRelativeRetargetingPairs.Add(BoneTrackPair(PoseBoneIndex, SkeletonBoneIndex));
				break;
				}
			}
		}
	}

	// Handle Root Bone separately
	if( bFirstTrackIsRootBone )
	{
		const int32 TrackIndex = 0;
		FTransform& RootAtom = OutAtoms[0];

		AnimationFormat_GetBoneAtom(	
			RootAtom,
			*this,
			TrackIndex,
			ExtractionContext.CurrentTime);

		// @laurent - we should look into splitting rotation and translation tracks, so we don't have to process translation twice.
		RetargetBoneTransform(RootAtom, 0, 0, RequiredBones);
	}

	if (RotationScalePairs.Num() > 0)
	{
		// get the remaining bone atoms
		AnimationFormat_GetAnimationPose(
			*((FTransformArray*)&OutAtoms), //@TODO:@ANIMATION: Nasty hack
			RotationScalePairs,
			TranslationPairs,
			RotationScalePairs,
			*this,
			ExtractionContext.CurrentTime);
	}

	// Once pose has been extracted, snap root bone back to first frame if we are extracting root motion.
	if( ExtractionContext.bExtractRootMotion && bEnableRootMotion)
	{
		ResetRootBoneForRootMotion(OutAtoms, RequiredBones, RootMotionRootLock);
	}

	// Anim Scale Retargeting
	int32 const NumBonesToScaleRetarget = AnimScaleRetargetingPairs.Num();
	if (NumBonesToScaleRetarget > 0)
	{
		TArray<FTransform> const& AuthoredOnRefSkeleton = MySkeleton->GetRefLocalPoses(RetargetSource);
		TArray<FTransform> const& PlayingOnRefSkeleton = RequiredBones.GetRefPoseArray();

		for (int32 Index = 0; Index<NumBonesToScaleRetarget; Index++)
		{
			BoneTrackPair const& BonePair = AnimScaleRetargetingPairs[Index];
			int32 const& PoseBoneIndex = BonePair.AtomIndex;
			int32 const& SkeletonBoneIndex = BonePair.TrackIndex; 

			// @todo - precache that in FBoneContainer when we have SkeletonIndex->TrackIndex mapping. So we can just apply scale right away.
			float const SourceTranslationLength = AuthoredOnRefSkeleton[SkeletonBoneIndex].GetTranslation().Size();
			if (FMath::Abs(SourceTranslationLength) > KINDA_SMALL_NUMBER)
			{
				float const TargetTranslationLength = PlayingOnRefSkeleton[PoseBoneIndex].GetTranslation().Size();
				OutAtoms[PoseBoneIndex].ScaleTranslation(TargetTranslationLength / SourceTranslationLength);
			}
		}
	}

	// Anim Relative Retargeting
	int32 const NumBonesToRelativeRetarget = AnimRelativeRetargetingPairs.Num();
	if (NumBonesToRelativeRetarget > 0)
	{
		TArray<FTransform> const& AuthoredOnRefSkeleton = MySkeleton->GetRefLocalPoses(RetargetSource);
		TArray<FTransform> const& PlayingOnRefSkeleton = RequiredBones.GetRefPoseArray();

		for (int32 Index = 0; Index < NumBonesToRelativeRetarget; Index++)
		{
			BoneTrackPair const& BonePair = AnimRelativeRetargetingPairs[Index];
			int32 const& PoseBoneIndex = BonePair.AtomIndex;
			int32 const& SkeletonBoneIndex = BonePair.TrackIndex;

			// Apply the retargeting as if it were an additive difference between the current skeleton and the retarget skeleton. 
			OutAtoms[PoseBoneIndex].SetRotation(OutAtoms[PoseBoneIndex].GetRotation() * AuthoredOnRefSkeleton[SkeletonBoneIndex].GetRotation().Inverse() * PlayingOnRefSkeleton[PoseBoneIndex].GetRotation() );
			OutAtoms[PoseBoneIndex].SetTranslation(OutAtoms[PoseBoneIndex].GetTranslation() + (PlayingOnRefSkeleton[PoseBoneIndex].GetTranslation() - AuthoredOnRefSkeleton[SkeletonBoneIndex].GetTranslation()));
			OutAtoms[PoseBoneIndex].SetScale3D(OutAtoms[PoseBoneIndex].GetScale3D() * (PlayingOnRefSkeleton[PoseBoneIndex].GetScale3D() * AuthoredOnRefSkeleton[SkeletonBoneIndex].GetSafeScaleReciprocal(AuthoredOnRefSkeleton[SkeletonBoneIndex].GetScale3D())));
			OutAtoms[PoseBoneIndex].NormalizeRotation();
		}
	}
}

void UAnimSequence::GetBonePose_Additive(FTransformArrayA2& OutAtoms, const FBoneContainer& RequiredBones, const FAnimExtractContext& ExtractionContext) const
{
	if( !IsValidAdditive() )
	{
		FAnimationRuntime::InitializeTransform(RequiredBones, OutAtoms);
		return;
	}

	// Extract target pose
	GetBonePose(OutAtoms, RequiredBones, ExtractionContext);

	// Extract base pose
	FTransformArrayA2 BasePoseAtoms;
	GetAdditiveBasePose(BasePoseAtoms, RequiredBones, ExtractionContext);

	// Create Additive animation
	FAnimationRuntime::ConvertPoseToAdditive(OutAtoms, BasePoseAtoms, RequiredBones);
}

void UAnimSequence::GetAdditiveBasePose(FTransformArrayA2& OutAtoms, const FBoneContainer& RequiredBones, const FAnimExtractContext& ExtractionContext) const
{
	switch (RefPoseType)
	{
		// use whole animation as a base pose. Need BasePoseSeq.
	case ABPT_AnimScaled:	
		{
			// normalize time to fit base seq
			const float Fraction = FMath::Clamp<float>(ExtractionContext.CurrentTime / SequenceLength, 0.f, 1.f);
			const float BasePoseTime = RefPoseSeq->SequenceLength * Fraction;

			FAnimExtractContext BasePoseExtractionContext(ExtractionContext);
			BasePoseExtractionContext.CurrentTime = BasePoseTime;
			RefPoseSeq->GetBonePose(OutAtoms, RequiredBones, BasePoseExtractionContext);
			break;
		}
		// use animation as a base pose. Need BasePoseSeq and RefFrameIndex (will clamp if outside).
	case ABPT_AnimFrame:		
		{
			const float Fraction = (RefPoseSeq->NumFrames > 0) ? FMath::Clamp<float>((float)RefFrameIndex/(float)RefPoseSeq->NumFrames, 0.f, 1.f) : 0.f;
			const float BasePoseTime = RefPoseSeq->SequenceLength * Fraction;

			FAnimExtractContext BasePoseExtractionContext(ExtractionContext);
			BasePoseExtractionContext.CurrentTime = BasePoseTime;
			RefPoseSeq->GetBonePose(OutAtoms, RequiredBones, BasePoseExtractionContext);
			break;
		}
		// use ref pose of Skeleton as base
	case ABPT_RefPose:		
	default:
		FAnimationRuntime::FillWithRefPose(OutAtoms, RequiredBones);
		break;
	}
}

void UAnimSequence::GetBonePose_AdditiveMeshRotationOnly(FTransformArrayA2& OutAtoms, const FBoneContainer& RequiredBones, const FAnimExtractContext& ExtractionContext) const
{
	if( !IsValidAdditive() )
	{
		// since this is additive, need to initialize to identity
		FAnimationRuntime::InitializeTransform(RequiredBones, OutAtoms);
		return;
	}

	// Get target pose
	GetBonePose(OutAtoms, RequiredBones, ExtractionContext);

	// get base pose
	FTransformArrayA2 BasePoseAtoms;
	GetAdditiveBasePose(BasePoseAtoms, RequiredBones, ExtractionContext);

	// Convert them to mesh rotation.
	FAnimationRuntime::ConvertPoseToMeshRotation(OutAtoms, RequiredBones);
	FAnimationRuntime::ConvertPoseToMeshRotation(BasePoseAtoms, RequiredBones);

	// Turn into Additive
	FAnimationRuntime::ConvertPoseToAdditive(OutAtoms, BasePoseAtoms, RequiredBones);
}

void UAnimSequence::RetargetBoneTransform(FTransform& BoneTransform, const int32& SkeletonBoneIndex, const int32& PoseBoneIndex, const FBoneContainer& RequiredBones) const
{
	const USkeleton* MySkeleton = GetSkeleton();
	const TArray<FBoneNode>& BoneTree = MySkeleton->GetBoneTree();
	if( BoneTree[SkeletonBoneIndex].TranslationRetargetingMode == EBoneTranslationRetargetingMode::Skeleton )
	{
		const TArray<FTransform>& TargetRefPoseArray = RequiredBones.GetRefPoseArray();
		BoneTransform.SetTranslation( TargetRefPoseArray[PoseBoneIndex].GetTranslation() );
	}
	else if( BoneTree[SkeletonBoneIndex].TranslationRetargetingMode == EBoneTranslationRetargetingMode::AnimationScaled )
	{
		// @todo - precache that in FBoneContainer when we have SkeletonIndex->TrackIndex mapping. So we can just apply scale right away.
		const TArray<FTransform>& SkeletonRefPoseArray = GetSkeleton()->GetRefLocalPoses(RetargetSource);
		const float SourceTranslationLength = SkeletonRefPoseArray[SkeletonBoneIndex].GetTranslation().Size();
		if( FMath::Abs(SourceTranslationLength) > KINDA_SMALL_NUMBER )
		{
			const TArray<FTransform>& TargetRefPoseArray = RequiredBones.GetRefPoseArray();
			const float TargetTranslationLength = TargetRefPoseArray[PoseBoneIndex].GetTranslation().Size();
			BoneTransform.ScaleTranslation(TargetTranslationLength / SourceTranslationLength);
		}
	}
	else if (BoneTree[SkeletonBoneIndex].TranslationRetargetingMode == EBoneTranslationRetargetingMode::AnimationRelative)
	{
		const TArray<FTransform>& AuthoredOnRefSkeleton = GetSkeleton()->GetRefLocalPoses(RetargetSource);
		const TArray<FTransform>& PlayingOnRefSkeleton = RequiredBones.GetRefPoseArray();

		// Apply the retargeting as if it were an additive difference between the current skeleton and the retarget skeleton. 
		BoneTransform.SetRotation(BoneTransform.GetRotation() * AuthoredOnRefSkeleton[SkeletonBoneIndex].GetRotation().Inverse() * PlayingOnRefSkeleton[PoseBoneIndex].GetRotation());
		BoneTransform.SetTranslation(BoneTransform.GetTranslation() + (PlayingOnRefSkeleton[PoseBoneIndex].GetTranslation() - AuthoredOnRefSkeleton[SkeletonBoneIndex].GetTranslation()));
		BoneTransform.SetScale3D(BoneTransform.GetScale3D() * (PlayingOnRefSkeleton[PoseBoneIndex].GetScale3D() * AuthoredOnRefSkeleton[SkeletonBoneIndex].GetSafeScaleReciprocal(AuthoredOnRefSkeleton[SkeletonBoneIndex].GetScale3D())));
		BoneTransform.NormalizeRotation();
	}
}

#if WITH_EDITOR
/** Utility function to crop data from a RawAnimSequenceTrack */
static int32 CropRawTrack(FRawAnimSequenceTrack& RawTrack, int32 StartKey, int32 NumKeys, int32 TotalNumOfFrames)
{
	check(RawTrack.PosKeys.Num() == 1 || RawTrack.PosKeys.Num() == TotalNumOfFrames);
	check(RawTrack.RotKeys.Num() == 1 || RawTrack.RotKeys.Num() == TotalNumOfFrames);
	// scale key can be empty
	check(RawTrack.ScaleKeys.Num() == 0 || RawTrack.ScaleKeys.Num() == 1 || RawTrack.ScaleKeys.Num() == TotalNumOfFrames);

	if( RawTrack.PosKeys.Num() > 1 )
	{
		RawTrack.PosKeys.RemoveAt(StartKey, NumKeys);
		check(RawTrack.PosKeys.Num() > 0);
		RawTrack.PosKeys.Shrink();
	}

	if( RawTrack.RotKeys.Num() > 1 )
	{
		RawTrack.RotKeys.RemoveAt(StartKey, NumKeys);
		check(RawTrack.RotKeys.Num() > 0);
		RawTrack.RotKeys.Shrink();
	}

	if( RawTrack.ScaleKeys.Num() > 1 )
	{
		RawTrack.ScaleKeys.RemoveAt(StartKey, NumKeys);
		check(RawTrack.ScaleKeys.Num() > 0);
		RawTrack.ScaleKeys.Shrink();
	}

	// Update NumFrames below to reflect actual number of keys.
	return FMath::Max<int32>( RawTrack.PosKeys.Num(), FMath::Max<int32>(RawTrack.RotKeys.Num(), RawTrack.ScaleKeys.Num()) );
}

void UAnimSequence::ResizeSequence(float NewLength, int32 NewNumFrames)
{
	NumFrames = NewNumFrames;
	// Update sequence length to match new number of frames.
	SequenceLength = NewLength;

	// resize curves
	RawCurveData.Resize(NewLength);
}

bool UAnimSequence::InsertFramesToRawAnimData( int32 StartFrame, int32 EndFrame, int32 CopyFrame)
{
	// make sure the copyframe is valid and start frame is valid
	int32 NumFramesToInsert = EndFrame-StartFrame;
	if ((CopyFrame>=0 && CopyFrame<NumFrames) && (StartFrame >= 0 && StartFrame <=NumFrames) && NumFramesToInsert > 0)
	{
		for (auto& RawData : RawAnimationData)
		{
			if (RawData.PosKeys.Num() > 1 && RawData.PosKeys.IsValidIndex(CopyFrame))
			{
				auto Source = RawData.PosKeys[CopyFrame];
				RawData.PosKeys.InsertZeroed(StartFrame, NumFramesToInsert);
				for (int32 Index=StartFrame; Index<EndFrame; ++Index)
				{
					RawData.PosKeys[Index] = Source;
				}
			}

			if(RawData.RotKeys.Num() > 1 && RawData.RotKeys.IsValidIndex(CopyFrame))
			{
				auto Source = RawData.RotKeys[CopyFrame];
				RawData.RotKeys.InsertZeroed(StartFrame, NumFramesToInsert);
				for(int32 Index=StartFrame; Index<EndFrame; ++Index)
				{
					RawData.RotKeys[Index] = Source;
				}
			}

			if(RawData.ScaleKeys.Num() > 1 && RawData.ScaleKeys.IsValidIndex(CopyFrame))
			{
				auto Source = RawData.ScaleKeys[CopyFrame];
				RawData.ScaleKeys.InsertZeroed(StartFrame, NumFramesToInsert);

				for(int32 Index=StartFrame; Index<EndFrame; ++Index)
				{
					RawData.ScaleKeys[Index] = Source;
				}
			}
		}

		float const FrameTime = SequenceLength / ((float)NumFrames);

		int32 NewNumFrames = NumFrames + NumFramesToInsert;
		ResizeSequence((float)NewNumFrames * FrameTime, NewNumFrames);

		UE_LOG(LogAnimation, Log, TEXT("\tSequenceLength: %f, NumFrames: %d"), SequenceLength, NumFrames);

		MarkPackageDirty();

		return true;
	}

	return false;
}

bool UAnimSequence::CropRawAnimData( float CurrentTime, bool bFromStart )
{
	// Length of one frame.
	float const FrameTime = SequenceLength / ((float)NumFrames);
	// Save Total Number of Frames before crop
	int32 TotalNumOfFrames = NumFrames;

	// if current frame is 1, do not try crop. There is nothing to crop
	if ( NumFrames <= 1 )
	{
		return false;
	}
	
	// If you're end or beginning, you can't cut all nor nothing. 
	// Avoiding ambiguous situation what exactly we would like to cut 
	// Below it clamps range to 1, TotalNumOfFrames-1
	// causing if you were in below position, it will still crop 1 frame. 
	// To be clearer, it seems better if we reject those inputs. 
	// If you're a bit before/after, we assume that you'd like to crop
	if ( CurrentTime == 0.f || CurrentTime == SequenceLength )
	{
		return false;
	}

	// Find the right key to cut at.
	// This assumes that all keys are equally spaced (ie. won't work if we have dropped unimportant frames etc).
	// The reason I'm changing to TotalNumOfFrames is CT/SL = KeyIndexWithFraction/TotalNumOfFrames
	// To play TotalNumOfFrames, it takes SequenceLength. Each key will take SequenceLength/TotalNumOfFrames
	float const KeyIndexWithFraction = (CurrentTime * (float)(TotalNumOfFrames)) / SequenceLength;
	int32 KeyIndex = bFromStart ? FMath::FloorToInt(KeyIndexWithFraction) : FMath::CeilToInt(KeyIndexWithFraction);
	// Ensure KeyIndex is in range.
	KeyIndex = FMath::Clamp<int32>(KeyIndex, 1, TotalNumOfFrames-1); 
	// determine which keys need to be removed.
	int32 const StartKey = bFromStart ? 0 : KeyIndex;
	int32 const NumKeys = bFromStart ? KeyIndex : TotalNumOfFrames - KeyIndex ;

	// Recalculate NumFrames
	NumFrames = TotalNumOfFrames - NumKeys;

	UE_LOG(LogAnimation, Log, TEXT("UAnimSequence::CropRawAnimData %s - CurrentTime: %f, bFromStart: %d, TotalNumOfFrames: %d, KeyIndex: %d, StartKey: %d, NumKeys: %d"), *GetName(), CurrentTime, bFromStart, TotalNumOfFrames, KeyIndex, StartKey, NumKeys);

	// Iterate over tracks removing keys from each one.
	for(int32 i=0; i<RawAnimationData.Num(); i++)
	{
		// Update NumFrames below to reflect actual number of keys while we crop the anim data
		CropRawTrack(RawAnimationData[i], StartKey, NumKeys, TotalNumOfFrames);
	}

	// Double check that everything is fine
	for(int32 i=0; i<RawAnimationData.Num(); i++)
	{
		FRawAnimSequenceTrack& RawTrack = RawAnimationData[i];
		check(RawTrack.PosKeys.Num() == 1 || RawTrack.PosKeys.Num() == NumFrames);
		check(RawTrack.RotKeys.Num() == 1 || RawTrack.RotKeys.Num() == NumFrames);
	}

	// Crop curve data
	{
		float CroppedStartTime = (bFromStart)? (NumKeys + StartKey)*FrameTime : 0.f;
		float CroppedEndTime = (bFromStart)? SequenceLength : (StartKey-1)*FrameTime;
		
		// make sure it doesn't go negative
		CroppedStartTime = FMath::Max<float>(CroppedStartTime, 0.f);
		CroppedEndTime = FMath::Max<float>(CroppedEndTime, 0.f);

		for ( auto CurveIter = RawCurveData.FloatCurves.CreateIterator(); CurveIter; ++CurveIter )
		{
			FFloatCurve& Curve = *CurveIter;
			// fix up curve before deleting
			// add these two values to keep the previous curve shape
			// it's possible I'm adding same values in the same location
			if (bFromStart)
			{
				// start from beginning, just add at the start time
				float EvaluationTime = CroppedStartTime; 
				float Value = Curve.FloatCurve.Eval(EvaluationTime);
				Curve.FloatCurve.AddKey(EvaluationTime, Value);
			}
			else
			{
				float EvaluationTime = CroppedEndTime; 
				float Value = Curve.FloatCurve.Eval(EvaluationTime);
				Curve.FloatCurve.AddKey(EvaluationTime, Value);
			}

			// now delete anything outside of range
			TArray<FKeyHandle> KeysToDelete;
			for (auto It(Curve.FloatCurve.GetKeyHandleIterator()); It; ++It)
			{
				FKeyHandle KeyHandle = It.Key();
				float KeyTime = Curve.FloatCurve.GetKeyTime(KeyHandle);
				// if outside of range, just delete it. 
				if ( KeyTime<CroppedStartTime || KeyTime>CroppedEndTime)
				{
					KeysToDelete.Add(KeyHandle);
				}
				// if starting from beginning, 
				// fix this up to apply delta
				else if (bFromStart)
				{
					Curve.FloatCurve.SetKeyTime(KeyHandle, KeyTime - CroppedStartTime);
				}
			}
			for (int32 i = 0; i < KeysToDelete.Num(); ++i)
			{
				Curve.FloatCurve.DeleteKey(KeysToDelete[i]);
			}
		}
	}

	// Update sequence length to match new number of frames.
	ResizeSequence((float)NumFrames * FrameTime, NumFrames);

	UE_LOG(LogAnimation, Log, TEXT("\tSequenceLength: %f, NumFrames: %d"), SequenceLength, NumFrames);

	MarkPackageDirty();
	return true;
}

bool UAnimSequence::CompressRawAnimSequenceTrack(FRawAnimSequenceTrack& RawTrack, float MaxPosDiff, float MaxAngleDiff)
{
	bool bRemovedKeys = false;

	// First part is to make sure we have valid input
	bool const bPosTrackIsValid = (RawTrack.PosKeys.Num() == 1 || RawTrack.PosKeys.Num() == NumFrames);
	if( !bPosTrackIsValid )
	{
		UE_LOG(LogAnimation, Warning, TEXT("Found non valid position track for %s, %d frames, instead of %d. Chopping!"), *GetName(), RawTrack.PosKeys.Num(), NumFrames);
		bRemovedKeys = true;
		RawTrack.PosKeys.RemoveAt(1, RawTrack.PosKeys.Num()- 1);
		RawTrack.PosKeys.Shrink();
		check( RawTrack.PosKeys.Num() == 1);
	}

	bool const bRotTrackIsValid = (RawTrack.RotKeys.Num() == 1 || RawTrack.RotKeys.Num() == NumFrames);
	if( !bRotTrackIsValid )
	{
		UE_LOG(LogAnimation, Warning, TEXT("Found non valid rotation track for %s, %d frames, instead of %d. Chopping!"), *GetName(), RawTrack.RotKeys.Num(), NumFrames);
		bRemovedKeys = true;
		RawTrack.RotKeys.RemoveAt(1, RawTrack.RotKeys.Num()- 1);
		RawTrack.RotKeys.Shrink();
		check( RawTrack.RotKeys.Num() == 1);
	}

	// scale keys can be empty, and that is valid 
	bool const bScaleTrackIsValid = (RawTrack.ScaleKeys.Num() == 0 || RawTrack.ScaleKeys.Num() == 1 || RawTrack.ScaleKeys.Num() == NumFrames);
	if( !bScaleTrackIsValid )
	{
		UE_LOG(LogAnimation, Warning, TEXT("Found non valid Scaleation track for %s, %d frames, instead of %d. Chopping!"), *GetName(), RawTrack.ScaleKeys.Num(), NumFrames);
		bRemovedKeys = true;
		RawTrack.ScaleKeys.RemoveAt(1, RawTrack.ScaleKeys.Num()- 1);
		RawTrack.ScaleKeys.Shrink();
		check( RawTrack.ScaleKeys.Num() == 1);
	}

	// Second part is actual compression.

	// Check variation of position keys
	if( (RawTrack.PosKeys.Num() > 1) && (MaxPosDiff >= 0.0f) )
	{
		FVector FirstPos = RawTrack.PosKeys[0];
		bool bFramesIdentical = true;
		for(int32 j=1; j<RawTrack.PosKeys.Num() && bFramesIdentical; j++)
		{
			if( (FirstPos - RawTrack.PosKeys[j]).Size() > MaxPosDiff )
			{
				bFramesIdentical = false;
			}
		}

		// If all keys are the same, remove all but first frame
		if( bFramesIdentical )
		{
			bRemovedKeys = true;
			RawTrack.PosKeys.RemoveAt(1, RawTrack.PosKeys.Num()- 1);
			RawTrack.PosKeys.Shrink();
			check( RawTrack.PosKeys.Num() == 1);
		}
	}

	// Check variation of rotational keys
	if( (RawTrack.RotKeys.Num() > 1) && (MaxAngleDiff >= 0.0f) )
	{
		FQuat FirstRot = RawTrack.RotKeys[0];
		bool bFramesIdentical = true;
		for(int32 j=1; j<RawTrack.RotKeys.Num() && bFramesIdentical; j++)
		{
			if( FQuat::Error(FirstRot, RawTrack.RotKeys[j]) > MaxAngleDiff )
			{
				bFramesIdentical = false;
			}
		}

		// If all keys are the same, remove all but first frame
		if( bFramesIdentical )
		{
			bRemovedKeys = true;
			RawTrack.RotKeys.RemoveAt(1, RawTrack.RotKeys.Num()- 1);
			RawTrack.RotKeys.Shrink();
			check( RawTrack.RotKeys.Num() == 1);
		}			
	}
	
	float MaxScaleDiff = 0.0001f;

	// Check variation of Scaleition keys
	if( (RawTrack.ScaleKeys.Num() > 1) && (MaxScaleDiff >= 0.0f) )
	{
		FVector FirstScale = RawTrack.ScaleKeys[0];
		bool bFramesIdentical = true;
		for(int32 j=1; j<RawTrack.ScaleKeys.Num() && bFramesIdentical; j++)
		{
			if( (FirstScale - RawTrack.ScaleKeys[j]).Size() > MaxScaleDiff )
			{
				bFramesIdentical = false;
			}
		}

		// If all keys are the same, remove all but first frame
		if( bFramesIdentical )
		{
			bRemovedKeys = true;
			RawTrack.ScaleKeys.RemoveAt(1, RawTrack.ScaleKeys.Num()- 1);
			RawTrack.ScaleKeys.Shrink();
			check( RawTrack.ScaleKeys.Num() == 1);
		}
	}

	return bRemovedKeys;
}

bool UAnimSequence::CompressRawAnimData(float MaxPosDiff, float MaxAngleDiff)
{
	bool bRemovedKeys = false;
#if WITH_EDITORONLY_DATA

	// This removes trivial keys, and this has to happen before the removing tracks
	for(int32 TrackIndex=0; TrackIndex<RawAnimationData.Num(); TrackIndex++)
	{
		bRemovedKeys |= CompressRawAnimSequenceTrack(RawAnimationData[TrackIndex], MaxPosDiff, MaxAngleDiff);
	}

	const USkeleton* MySkeleton = GetSkeleton();

	if (MySkeleton)
	{
		bool bCompressScaleKeys = false;
		// go through remove keys if not needed
		for ( int32 TrackIndex=0; TrackIndex<RawAnimationData.Num(); TrackIndex++ )
		{
			FRawAnimSequenceTrack const& RawData = RawAnimationData[TrackIndex];
			if ( RawData.ScaleKeys.Num() > 0 )
			{
				// if scale key exists, see if we can just empty it
				if((RawData.ScaleKeys.Num() > 1) || (RawData.ScaleKeys[0].Equals(FVector(1.f)) == false))
				{
					bCompressScaleKeys = true;
					break;
				}
			}
		}

		// if we don't have scale, we should delete all scale keys
		// if you have one track that has scale, we still should support scale, so compress scale
		if (!bCompressScaleKeys)
		{
			// then remove all scale keys
			for ( int32 TrackIndex=0; TrackIndex<RawAnimationData.Num(); TrackIndex++ )
			{
				FRawAnimSequenceTrack& RawData = RawAnimationData[TrackIndex];
				RawData.ScaleKeys.Empty();
			}
		}
	}

	CompressedTrackOffsets.Empty();
	CompressedScaleOffsets.Empty();
#endif
	return bRemovedKeys;
}

bool UAnimSequence::CompressRawAnimData()
{
	const float MaxPosDiff = 0.0001f;
	const float MaxAngleDiff = 0.0003f;
	return CompressRawAnimData(MaxPosDiff, MaxAngleDiff);
}

/** 
 * Flip Rotation W for the RawTrack
 */
void FlipRotationW(FRawAnimSequenceTrack& RawTrack)
{
	int32 TotalNumOfRotKey = RawTrack.RotKeys.Num();

	for (int32 I=0; I<TotalNumOfRotKey; ++I)
	{
		FQuat& RotKey = RawTrack.RotKeys[I];
		RotKey.W *= -1.f;
	}
}


void UAnimSequence::FlipRotationWForNonRoot(USkeletalMesh * SkelMesh)
{
	if (!GetSkeleton())
	{
		return;
	}

	// Now add additive animation to destination.
	for(int32 TrackIdx=0; TrackIdx<TrackToSkeletonMapTable.Num(); TrackIdx++)
	{
		// Figure out which bone this track is mapped to
		const int32 BoneIndex = TrackToSkeletonMapTable[TrackIdx].BoneTreeIndex;
		if ( BoneIndex > 0 )
		{
			FlipRotationW( RawAnimationData[TrackIdx] );

		}
	}

	// Apply compression
	FAnimationUtils::CompressAnimSequence(this, false, false);
}
#endif 

void UAnimSequence::RecycleAnimSequence()
{
#if WITH_EDITORONLY_DATA
	// Clear RawAnimData
	RawAnimationData.Empty();
	TrackToSkeletonMapTable.Empty();

#endif // WITH_EDITORONLY_DATA
}
bool UAnimSequence::CopyAnimSequenceProperties(UAnimSequence* SourceAnimSeq, UAnimSequence* DestAnimSeq, bool bSkipCopyingNotifies)
{
#if WITH_EDITORONLY_DATA
	// Copy parameters
	DestAnimSeq->SequenceLength				= SourceAnimSeq->SequenceLength;
	DestAnimSeq->NumFrames					= SourceAnimSeq->NumFrames;
	DestAnimSeq->RateScale					= SourceAnimSeq->RateScale;
	DestAnimSeq->bDoNotOverrideCompression	= SourceAnimSeq->bDoNotOverrideCompression;

	// Copy Compression Settings
	DestAnimSeq->CompressionScheme				= static_cast<UAnimCompress*>( StaticDuplicateObject( SourceAnimSeq->CompressionScheme, DestAnimSeq, TEXT("None"), ~RF_RootSet ) );
	DestAnimSeq->TranslationCompressionFormat	= SourceAnimSeq->TranslationCompressionFormat;
	DestAnimSeq->RotationCompressionFormat		= SourceAnimSeq->RotationCompressionFormat;
	DestAnimSeq->AdditiveAnimType				= SourceAnimSeq->AdditiveAnimType;
	DestAnimSeq->RefPoseType					= SourceAnimSeq->RefPoseType;
	DestAnimSeq->RefPoseSeq						= SourceAnimSeq->RefPoseSeq;
	DestAnimSeq->RefFrameIndex					= SourceAnimSeq->RefFrameIndex;

	if( !bSkipCopyingNotifies )
	{
		// Copy Metadata information
		CopyNotifies(SourceAnimSeq, DestAnimSeq);
	}

	DestAnimSeq->MarkPackageDirty();

	// Copy Curve Data
	DestAnimSeq->RawCurveData = SourceAnimSeq->RawCurveData;
#endif // WITH_EDITORONLY_DATA

	return true;
}


bool UAnimSequence::CopyNotifies(UAnimSequence* SourceAnimSeq, UAnimSequence* DestAnimSeq)
{
#if WITH_EDITOR
	// Abort if source == destination.
	if( SourceAnimSeq == DestAnimSeq )
	{
		return true;
	}

	// If the destination sequence is shorter than the source sequence, we'll be dropping notifies that
	// occur at later times than the dest sequence is long.  Give the user a chance to abort if we
	// find any notifies that won't be copied over.
	if( DestAnimSeq->SequenceLength < SourceAnimSeq->SequenceLength )
	{
		for(int32 NotifyIndex=0; NotifyIndex<SourceAnimSeq->Notifies.Num(); ++NotifyIndex)
		{
			// If a notify is found which occurs off the end of the destination sequence, prompt the user to continue.
			const FAnimNotifyEvent& SrcNotifyEvent = SourceAnimSeq->Notifies[NotifyIndex];
			if( SrcNotifyEvent.DisplayTime_DEPRECATED > DestAnimSeq->SequenceLength )
			{
				const bool bProceed = EAppReturnType::Yes == FMessageDialog::Open( EAppMsgType::YesNo, NSLOCTEXT("UnrealEd", "SomeNotifiesWillNotBeCopiedQ", "Some notifies will not be copied because the destination sequence is not long enough.  Proceed?") );
				if( !bProceed )
				{
					return false;
				}
				else
				{
					break;
				}
			}
		}
	}

	// If the destination sequence contains any notifies, ask the user if they'd like
	// to delete the existing notifies before copying over from the source sequence.
	if( DestAnimSeq->Notifies.Num() > 0 )
	{
		const bool bDeleteExistingNotifies = EAppReturnType::Yes == FMessageDialog::Open(EAppMsgType::YesNo, FText::Format(
			NSLOCTEXT("UnrealEd", "DestSeqAlreadyContainsNotifiesMergeQ", "The destination sequence already contains {0} notifies.  Delete these before copying?"), FText::AsNumber(DestAnimSeq->Notifies.Num())) );
		if( bDeleteExistingNotifies )
		{
			DestAnimSeq->Notifies.Empty();
			DestAnimSeq->MarkPackageDirty();
		}
	}

	// Do the copy.
	TArray<int32> NewNotifyIndices;
	int32 NumNotifiesThatWereNotCopied = 0;

	for(int32 NotifyIndex=0; NotifyIndex<SourceAnimSeq->Notifies.Num(); ++NotifyIndex)
	{
		const FAnimNotifyEvent& SrcNotifyEvent = SourceAnimSeq->Notifies[NotifyIndex];

		// Skip notifies which occur at times later than the destination sequence is long.
		if( SrcNotifyEvent.DisplayTime_DEPRECATED > DestAnimSeq->SequenceLength )
		{
			continue;
		}

		// Do a linear-search through existing notifies to determine where
		// to insert the new notify.
		int32 NewNotifyIndex = 0;
		while( NewNotifyIndex < DestAnimSeq->Notifies.Num()
			&& DestAnimSeq->Notifies[NewNotifyIndex].DisplayTime_DEPRECATED <= SrcNotifyEvent.DisplayTime_DEPRECATED )
		{
			++NewNotifyIndex;
		}

		// Track the location of the new notify.
		NewNotifyIndices.Add(NewNotifyIndex);

		// Create a new empty on in the array.
		DestAnimSeq->Notifies.InsertZeroed(NewNotifyIndex);

		// Copy time and comment.
		FAnimNotifyEvent& Notify = DestAnimSeq->Notifies[NewNotifyIndex];
		Notify.DisplayTime_DEPRECATED = SrcNotifyEvent.DisplayTime_DEPRECATED;
		Notify.TriggerTimeOffset = GetTriggerTimeOffsetForType( DestAnimSeq->CalculateOffsetForNotify(Notify.DisplayTime_DEPRECATED));
		Notify.NotifyName = SrcNotifyEvent.NotifyName;
		Notify.Duration = SrcNotifyEvent.Duration;

		// Copy the notify itself, and point the new one at it.
		if( SrcNotifyEvent.Notify )
		{
			DestAnimSeq->Notifies[NewNotifyIndex].Notify = static_cast<UAnimNotify*>( StaticDuplicateObject(SrcNotifyEvent.Notify, DestAnimSeq, TEXT("None"), ~RF_RootSet ) );
		}
		else
		{
			DestAnimSeq->Notifies[NewNotifyIndex].Notify = NULL;
		}

		if( SrcNotifyEvent.NotifyStateClass )
		{
			DestAnimSeq->Notifies[NewNotifyIndex].NotifyStateClass = static_cast<UAnimNotifyState*>( StaticDuplicateObject(SrcNotifyEvent.NotifyStateClass, DestAnimSeq, TEXT("None"), ~RF_RootSet ) );
		}
		else
		{
			DestAnimSeq->Notifies[NewNotifyIndex].NotifyStateClass = NULL;
		}

		// Make sure editor knows we've changed something.
		DestAnimSeq->MarkPackageDirty();
	}

	// Inform the user if some notifies weren't copied.
	if( SourceAnimSeq->Notifies.Num() > NewNotifyIndices.Num() )
	{
		FMessageDialog::Open( EAppMsgType::Ok, FText::Format(
			NSLOCTEXT("UnrealEd", "SomeNotifiesWereNotCopiedF", "Because the destination sequence was shorter, {0} notifies were not copied."), FText::AsNumber(SourceAnimSeq->Notifies.Num() - NewNotifyIndices.Num())) );
	}
#endif // WITH_EDITOR

	return true;
}

bool UAnimSequence::IsValidAdditive() const		
{ 
	if (AdditiveAnimType != AAT_None)
	{
		switch (RefPoseType)
		{
		case ABPT_RefPose:
			return true;
		case ABPT_AnimScaled:
			return (RefPoseSeq != NULL);
		case ABPT_AnimFrame:
			return (RefPoseSeq != NULL) && (RefFrameIndex >= 0);
		default:
			return false;
		}
	}

	return false;
}

#if WITH_EDITOR

int32 FindMeshBoneIndexFromBoneName(USkeleton * Skeleton, const FName &BoneName)
{
	USkeletalMesh * PreviewMesh = Skeleton->GetPreviewMesh();
	const int32& SkeletonBoneIndex = Skeleton->GetReferenceSkeleton().FindBoneIndex(BoneName);

	int32 BoneIndex = INDEX_NONE;

	if(SkeletonBoneIndex != INDEX_NONE)
	{
		BoneIndex = Skeleton->GetMeshBoneIndexFromSkeletonBoneIndex(PreviewMesh, SkeletonBoneIndex);
	}

	return BoneIndex;
}
void FillUpTransformBasedOnRig(USkeleton* Skeleton, TArray<FTransform>& NodeSpaceBases, TArray<FTransform> &Rotations, TArray<FTransform>& Translations, TArray<bool>& TranslationParentFlags)
{
	TArray<FTransform> SpaceBases;
	FAnimationRuntime::FillUpSpaceBasesRetargetBasePose(Skeleton, SpaceBases);

	const URig* Rig = Skeleton->GetRig();

	if (Rig)
	{
		// this one has to collect all Nodes in Rig data
		// since we're comparing two of them together. 
		int32 NodeNum = Rig->GetNodeNum();

		if (NodeNum > 0)
		{
			NodeSpaceBases.Empty(NodeNum);
			NodeSpaceBases.AddUninitialized(NodeNum);

			Rotations.Empty(NodeNum);
			Rotations.AddUninitialized(NodeNum);

			Translations.Empty(NodeNum);
			Translations.AddUninitialized(NodeNum);

			TranslationParentFlags.Empty(Translations.Num());
			TranslationParentFlags.AddZeroed(Translations.Num());

			const USkeletalMesh* PreviewMesh = Skeleton->GetPreviewMesh();

			for (int32 Index = 0; Index < NodeNum; ++Index)
			{
				const FName NodeName = Rig->GetNodeName(Index);
				const FName& BoneName = Skeleton->GetRigBoneMapping(NodeName);
				const int32& BoneIndex = FindMeshBoneIndexFromBoneName(Skeleton, BoneName);

				if (BoneIndex == INDEX_NONE)
				{
					// add identity
					NodeSpaceBases[Index].SetIdentity();
					Rotations[Index].SetIdentity();
					Translations[Index].SetIdentity();
				}
				else
				{
					// initialize with SpaceBases - assuming World Based
					NodeSpaceBases[Index] = SpaceBases[BoneIndex];
					Rotations[Index] = SpaceBases[BoneIndex];
					Translations[Index] = SpaceBases[BoneIndex];

					const FTransformBase* TransformBase = Rig->GetTransformBaseByNodeName(NodeName);

					if (TransformBase != NULL)
					{
						// orientation constraint			
						const auto& RotConstraint = TransformBase->Constraints[EControlConstraint::Type::Orientation];

						if (RotConstraint.TransformConstraints.Num() > 0)
						{
							const FName& ParentBoneName = Skeleton->GetRigBoneMapping(RotConstraint.TransformConstraints[0].ParentSpace);
							const int32& ParentBoneIndex = FindMeshBoneIndexFromBoneName(Skeleton, ParentBoneName);

							if (ParentBoneIndex != INDEX_NONE)
							{
								Rotations[Index] = SpaceBases[BoneIndex].GetRelativeTransform(SpaceBases[ParentBoneIndex]);
							}
						}

						// translation constraint
						const auto& TransConstraint = TransformBase->Constraints[EControlConstraint::Type::Translation];

						if (TransConstraint.TransformConstraints.Num() > 0)
						{
							const FName& ParentBoneName = Skeleton->GetRigBoneMapping(TransConstraint.TransformConstraints[0].ParentSpace);
							const int32& ParentBoneIndex = FindMeshBoneIndexFromBoneName(Skeleton, ParentBoneName);

							if (ParentBoneIndex != INDEX_NONE)
							{
								// I think translation has to include rotation, otherwise it won't work
								Translations[Index] = SpaceBases[BoneIndex].GetRelativeTransform(SpaceBases[ParentBoneIndex]);
								TranslationParentFlags[Index] = true;
							}
						}
					}
				}
			}
		}
	}
}

int32 FindValidTransformParentTrack(const URig* Rig, int32 NodeIndex, bool bTranslate, const TArray<FName>& ValidNodeNames)
{
	int32 ParentIndex = Rig->FindTransformParentNode(NodeIndex, bTranslate);

	// verify if it exists in ValidNodeNames
	if (ParentIndex != INDEX_NONE)
	{
		FName NodeName = Rig->GetNodeName(ParentIndex);

		return ValidNodeNames.Find(NodeName);
	}

	return INDEX_NONE;
}


void UAnimSequence::RemapTracksToNewSkeleton( USkeleton* NewSkeleton, bool bConvertSpaces )
{
	// this is not cheap, so make sure it only happens in editor

	// @Todo : currently additive will work fine since we don't bake anything except when we extract
	// but in the future if we bake this can be problem
	if (bConvertSpaces)
	{
		USkeleton* OldSkeleton = GetSkeleton();

		// first check if both has same rig, if so, we'll retarget using it
		if (OldSkeleton && OldSkeleton->GetRig() != NULL && NewSkeleton->GetRig() == OldSkeleton->GetRig())
		{
			const URig* Rig = OldSkeleton->GetRig();

			// we'll have to save the relative space bases transform from old ref pose to new refpose
			TArray<FTransform> RelativeToNewSpaceBases;
			// save the ratio of translation change
			TArray<float> OldToNewTranslationRatio;
			// create relative transform in component space between old skeleton and new skeleton
			{
				// first calculate component space ref pose to get the relative transform between
				// two ref poses. It is very important update ref pose before getting here. 
				TArray<FTransform> NewRotations, NewSpaceBases, OldRotations;
				TArray<FTransform> NewTranslations, OldSpaceBases, OldTranslations;
				TArray<bool> NewTranslationParentFlags, OldTranslationParentFlags;
				// get the spacebases transform
				FillUpTransformBasedOnRig(NewSkeleton, NewSpaceBases, NewRotations, NewTranslations, NewTranslationParentFlags);
				FillUpTransformBasedOnRig(OldSkeleton, OldSpaceBases, OldRotations, OldTranslations, OldTranslationParentFlags);

				// now we'd like to get the relative transform from old to new ref pose in component space
				// PK2*K2 = PK1*K1*theta where theta => P1*R1*theta = P2*R2 
				// where	P1 - parent transform in component space for original skeleton
				//			R1 - local space of the current bone for original skeleton
				//			P2 - parent transform in component space for new skeleton
				//			R2 - local space of the current bone for new skeleton
				// what we're looking for is theta, so that we can apply that to animated transform
				// this has to have all of nodes since comparing two skeletons, that might have different configuration
				int32 NumNodes = Rig->GetNodeNum();
				// saves the theta data per node
				RelativeToNewSpaceBases.AddUninitialized(NumNodes);
				// saves the translation conversion datao
				OldToNewTranslationRatio.AddUninitialized(NumNodes);

				const TArray<FNode>& Nodes = Rig->GetNodes();
				// calculate the relative transform to new skeleton
				// so that we can apply the delta in component space
				for (int32 NodeIndex = 0; NodeIndex < NumNodes; ++NodeIndex)
				{
					// theta (RelativeToNewTransform) = (P1*R1)^(-1) * P2*R2 where theta => P1*R1*theta = P2*R2
					RelativeToNewSpaceBases[NodeIndex] = NewSpaceBases[NodeIndex].GetRelativeTransform(OldSpaceBases[NodeIndex]); 

					// also savees the translation difference between old to new
					FVector OldTranslation = OldTranslations[NodeIndex].GetTranslation();
					FVector NewTranslation = NewTranslations[NodeIndex].GetTranslation();

					if (NewTranslationParentFlags[NodeIndex] == OldTranslationParentFlags[NodeIndex])
					{
						// only do this if parent status matches, otherwise, you'll have invalid state 
						// where one is based on shoulder, where the other is missing the shoulder node
						float OldTranslationSize = OldTranslation.Size();
						float NewTranslationSize = NewTranslation.Size();

						OldToNewTranslationRatio[NodeIndex] = (FMath::IsNearlyZero(OldTranslationSize)) ? 1.f/*do not touch new translation size*/ : NewTranslationSize / OldTranslationSize;
					}
					else
					{
						OldToNewTranslationRatio[NodeIndex] = 1.f; // set to be 1, we don't know what it is
					}

					UE_LOG(LogAnimation, Verbose, TEXT("Retargeting (%s : %d) : OldtoNewTranslationRatio (%0.2f), Relative Transform (%s)"), *Nodes[NodeIndex].Name.ToString(), NodeIndex, 
						OldToNewTranslationRatio[NodeIndex], *RelativeToNewSpaceBases[NodeIndex].ToString());
					UE_LOG(LogAnimation, Verbose, TEXT("\tOldSpaceBase(%s), NewSpaceBase(%s)"), *OldSpaceBases[NodeIndex].ToString(), *NewSpaceBases[NodeIndex].ToString());
				}
			}

			FAnimSequenceTrackContainer RiggingAnimationData;

			// now convert animation data to rig data
			ConvertAnimationDataToRiggingData(RiggingAnimationData);

			// here we have to watch out the index
			// The RiggingAnimationData will contain only the nodes that are mapped to source skeleton
			// and here we convert everything that is in RiggingAnimationData which means based on source data
			// when mapped back to new skeleton, it will discard results that are not mapped to target skeleton
			
			TArray<FName> SrcValidNodeNames;
			int32 SrcNumTracks = OldSkeleton->GetMappedValidNodes(SrcValidNodeNames);

			// now convert to space bases animation 
			TArray< TArray<FTransform> > ComponentSpaceAnimations, ConvertedLocalSpaceAnimations, ConvertedSpaceAnimations;
			ComponentSpaceAnimations.AddZeroed(SrcNumTracks);
			ConvertedSpaceAnimations.AddZeroed(SrcNumTracks);
			ConvertedLocalSpaceAnimations.AddZeroed(SrcNumTracks);

			int32 NumKeys = NumFrames;
			float Interval = GetIntervalPerKey(NumFrames, SequenceLength);

			// allocate arrays
			for(int32 SrcTrackIndex=0; SrcTrackIndex<SrcNumTracks; ++SrcTrackIndex)
			{
				ComponentSpaceAnimations[SrcTrackIndex].AddUninitialized(NumKeys);
				ConvertedLocalSpaceAnimations[SrcTrackIndex].AddUninitialized(NumKeys);
				ConvertedSpaceAnimations[SrcTrackIndex].AddUninitialized(NumKeys);
			}

			for (int32 SrcTrackIndex=0; SrcTrackIndex<SrcNumTracks; ++SrcTrackIndex)		
			{
				int32 NodeIndex = Rig->FindNode(SrcValidNodeNames[SrcTrackIndex]);
				check (NodeIndex != INDEX_NONE);
				auto& RawAnimation = RiggingAnimationData.AnimationTracks[SrcTrackIndex];

				// find rotation parent node
				int32 RotParentTrackIndex = FindValidTransformParentTrack(Rig, NodeIndex, false, SrcValidNodeNames);
				int32 TransParentTrackIndex = FindValidTransformParentTrack(Rig, NodeIndex, true, SrcValidNodeNames);
				// fill up keys - calculate PK1 * K1
				for(int32 Key=0; Key<NumKeys; ++Key)
				{
					FTransform AnimatedLocalKey;
					ExtractBoneTransform(RiggingAnimationData.AnimationTracks, AnimatedLocalKey, SrcTrackIndex, Interval*Key);

					AnimatedLocalKey.ScaleTranslation(OldToNewTranslationRatio[NodeIndex]);

					if(RotParentTrackIndex != INDEX_NONE)
					{
						FQuat ComponentSpaceRotation = ComponentSpaceAnimations[RotParentTrackIndex][Key].GetRotation() * AnimatedLocalKey.GetRotation();
						ComponentSpaceAnimations[SrcTrackIndex][Key].SetRotation(ComponentSpaceRotation);
					}
					else
					{
						ComponentSpaceAnimations[SrcTrackIndex][Key].SetRotation(AnimatedLocalKey.GetRotation());
					}

					if (TransParentTrackIndex != INDEX_NONE)
					{
						FVector ComponentSpaceTranslation = ComponentSpaceAnimations[TransParentTrackIndex][Key].TransformPosition(AnimatedLocalKey.GetTranslation());
						ComponentSpaceAnimations[SrcTrackIndex][Key].SetTranslation(ComponentSpaceTranslation);
						ComponentSpaceAnimations[SrcTrackIndex][Key].SetScale3D(AnimatedLocalKey.GetScale3D());
					}
					else
					{
						ComponentSpaceAnimations[SrcTrackIndex][Key].SetTranslation(AnimatedLocalKey.GetTranslation());
						ComponentSpaceAnimations[SrcTrackIndex][Key].SetScale3D(AnimatedLocalKey.GetScale3D());
					}
				}
			}

			// now animation is converted to component space
			TArray<struct FRawAnimSequenceTrack> NewRawAnimationData = RiggingAnimationData.AnimationTracks;
			for (int32 SrcTrackIndex=0; SrcTrackIndex<SrcNumTracks; ++SrcTrackIndex)
			{
				int32 NodeIndex = Rig->FindNode(SrcValidNodeNames[SrcTrackIndex]);
				// find rotation parent node
				int32 RotParentTrackIndex = FindValidTransformParentTrack(Rig, NodeIndex, false, SrcValidNodeNames);
				int32 TransParentTrackIndex = FindValidTransformParentTrack(Rig, NodeIndex, true, SrcValidNodeNames);

				// clear translation;
				RelativeToNewSpaceBases[NodeIndex].SetTranslation(FVector::ZeroVector);

				for(int32 Key=0; Key<NumKeys; ++Key)
				{
					// now convert to the new space and save to local spaces
					ConvertedSpaceAnimations[SrcTrackIndex][Key] = RelativeToNewSpaceBases[NodeIndex] * ComponentSpaceAnimations[SrcTrackIndex][Key];

					if(RotParentTrackIndex != INDEX_NONE)
					{
						FQuat LocalRotation = ConvertedSpaceAnimations[RotParentTrackIndex][Key].GetRotation().Inverse() * ConvertedSpaceAnimations[SrcTrackIndex][Key].GetRotation();
						ConvertedLocalSpaceAnimations[SrcTrackIndex][Key].SetRotation(LocalRotation);
					}
					else
					{
						ConvertedLocalSpaceAnimations[SrcTrackIndex][Key].SetRotation(ConvertedSpaceAnimations[SrcTrackIndex][Key].GetRotation());
					}

					if(TransParentTrackIndex != INDEX_NONE)
					{
						FVector LocalTranslation = ConvertedSpaceAnimations[SrcTrackIndex][Key].GetRelativeTransform(ConvertedSpaceAnimations[TransParentTrackIndex][Key]).GetTranslation();
						ConvertedLocalSpaceAnimations[SrcTrackIndex][Key].SetTranslation(LocalTranslation);
						ConvertedLocalSpaceAnimations[SrcTrackIndex][Key].SetScale3D(ConvertedSpaceAnimations[SrcTrackIndex][Key].GetScale3D());
					}
					else
					{
						ConvertedLocalSpaceAnimations[SrcTrackIndex][Key].SetTranslation(ConvertedSpaceAnimations[SrcTrackIndex][Key].GetTranslation());
						ConvertedLocalSpaceAnimations[SrcTrackIndex][Key].SetScale3D(ConvertedSpaceAnimations[SrcTrackIndex][Key].GetScale3D());
					}
				}

				auto& RawAnimation = NewRawAnimationData[SrcTrackIndex];
				RawAnimation.PosKeys.Empty(NumKeys);
				RawAnimation.PosKeys.AddUninitialized(NumKeys);
				RawAnimation.RotKeys.Empty(NumKeys);
				RawAnimation.RotKeys.AddUninitialized(NumKeys);
				RawAnimation.ScaleKeys.Empty(NumKeys);
				RawAnimation.ScaleKeys.AddUninitialized(NumKeys);

				for(int32 Key=0; Key<NumKeys; ++Key)
				{
					RawAnimation.PosKeys[Key] = ConvertedLocalSpaceAnimations[SrcTrackIndex][Key].GetLocation();
					RawAnimation.RotKeys[Key] = ConvertedLocalSpaceAnimations[SrcTrackIndex][Key].GetRotation();
					RawAnimation.ScaleKeys[Key] = ConvertedLocalSpaceAnimations[SrcTrackIndex][Key].GetScale3D();

					// normalize rotation
					RawAnimation.RotKeys[Key].Normalize();
				}
			}

			RiggingAnimationData.AnimationTracks = MoveTemp(NewRawAnimationData);
			RiggingAnimationData.TrackNames = MoveTemp(SrcValidNodeNames);

			// set new skeleton
			SetSkeleton(NewSkeleton);

			// convert back to animated data with new skeleton
			ConvertRiggingDataToAnimationData(RiggingAnimationData);
		}
		// @todo end rig testing
		// @IMPORTANT: now otherwise this will try to do bone to bone mapping
		else if(OldSkeleton)
		{
			// this only replaces the primary one, it doesn't replace old ones
			TArray<struct FTrackToSkeletonMap> NewTrackToSkeletonMapTable;
			NewTrackToSkeletonMapTable.Empty(AnimationTrackNames.Num());
			NewTrackToSkeletonMapTable.AddUninitialized(AnimationTrackNames.Num());
			for (int32 Track = 0; Track < AnimationTrackNames.Num(); ++Track)
			{
				int32 BoneIndex = NewSkeleton->GetReferenceSkeleton().FindBoneIndex(AnimationTrackNames[Track]);
				NewTrackToSkeletonMapTable[Track].BoneTreeIndex = BoneIndex;
			}

			// now I have all NewTrack To Skeleton Map Table
			// I'll need to compare with old tracks and copy over if SkeletonIndex == 0
			// if SkeletonIndex != 0, we need to see if we can 
			for (int32 TableId = 0; TableId < NewTrackToSkeletonMapTable.Num(); ++TableId)
			{
				if (ensure(TrackToSkeletonMapTable.IsValidIndex(TableId)))
				{
					if (NewTrackToSkeletonMapTable[TableId].BoneTreeIndex != INDEX_NONE)
					{
						TrackToSkeletonMapTable[TableId].BoneTreeIndex = NewTrackToSkeletonMapTable[TableId].BoneTreeIndex;
					}
					else
					{
						// if not found, delete the track data
						RemoveTrack(TableId);
						NewTrackToSkeletonMapTable.RemoveAt(TableId);
						--TableId;
					}
				}
			}

			if (TrackToSkeletonMapTable.Num() == 0)
			{
				// no bones to retarget
				// return with error
				//@todo fail message
			}
			// make sure you do update reference pose before coming here
			
			// first calculate component space ref pose to get the relative transform between
			// two ref poses. It is very important update ref pose before getting here. 
			TArray<FTransform> NewSpaceBaseRefPose, OldSpaceBaseRefPose, RelativeToNewTransform;
			// get the spacebases transform
			FAnimationRuntime::FillUpSpaceBasesRefPose(NewSkeleton, NewSpaceBaseRefPose);
			FAnimationRuntime::FillUpSpaceBasesRefPose(OldSkeleton, OldSpaceBaseRefPose);

			const TArray<FTransform>& OldRefPose = OldSkeleton->GetReferenceSkeleton().GetRefBonePose();
			const TArray<FTransform>& NewRefPose = NewSkeleton->GetReferenceSkeleton().GetRefBonePose();

			// now we'd like to get the relative transform from old to new ref pose in component space
			// PK2*K2 = PK1*K1*theta where theta => P1*R1*theta = P2*R2 
			// where	P1 - parent transform in component space for original skeleton
			//			R1 - local space of the current bone for original skeleton
			//			P2 - parent transform in component space for new skeleton
			//			R2 - local space of the current bone for new skeleton
			// what we're looking for is theta, so that we can apply that to animated transform
			int32 NumBones = NewSpaceBaseRefPose.Num();
			// saves the theta data per bone
			RelativeToNewTransform.AddUninitialized(NumBones);
			TArray<float> OldToNewTranslationRatio;
			// saves the translation conversion data
			OldToNewTranslationRatio.AddUninitialized(NumBones);

			// calculate the relative transform to new skeleton
			// so that we can apply the delta in component space
			for(int32 BoneIndex=0; BoneIndex<NumBones; ++BoneIndex)
			{
				// first find bone name of the idnex
				FName BoneName = NewSkeleton->GetReferenceSkeleton().GetRefBoneInfo()[BoneIndex].Name;
				// find it in old index
				int32 OldBoneIndex = OldSkeleton->GetReferenceSkeleton().FindBoneIndex(BoneName);

				// get old bone index
				if(OldBoneIndex != INDEX_NONE)
				{
					// theta (RelativeToNewTransform) = (P1*R1)^(-1) * P2*R2 where theta => P1*R1*theta = P2*R2
					RelativeToNewTransform[BoneIndex] = NewSpaceBaseRefPose[BoneIndex].GetRelativeTransform(OldSpaceBaseRefPose[OldBoneIndex]);

					// also savees the translation difference between old to new
					FVector OldTranslation = OldRefPose[OldBoneIndex].GetTranslation();
					FVector NewTranslation = NewRefPose[BoneIndex].GetTranslation();

					float OldTranslationSize = OldTranslation.Size();
					float NewTranslationSize = NewTranslation.Size();
					OldToNewTranslationRatio[BoneIndex] = (FMath::IsNearlyZero(OldTranslationSize))? 1.f/*do not touch new translation size*/ : NewTranslationSize/OldTranslationSize;
				}
				else
				{
					RelativeToNewTransform[BoneIndex].SetIdentity();
				}
			}

			// 2d array of animated time [boneindex][time key]
			TArray< TArray<FTransform> > AnimatedSpaceBases, ConvertedLocalSpaces, ConvertedSpaceBases;
			AnimatedSpaceBases.AddZeroed(NumBones);
			ConvertedLocalSpaces.AddZeroed(NumBones);
			ConvertedSpaceBases.AddZeroed(NumBones);

			int32 NumKeys = NumFrames;
			float Interval = GetIntervalPerKey(NumFrames, SequenceLength);

			// allocate arrays
			for(int32 BoneIndex=0; BoneIndex<NumBones; ++BoneIndex)
			{
				AnimatedSpaceBases[BoneIndex].AddUninitialized(NumKeys);
				ConvertedLocalSpaces[BoneIndex].AddUninitialized(NumKeys);
				ConvertedSpaceBases[BoneIndex].AddUninitialized(NumKeys);
			}

			// now calculating old animated space bases
			// this one calculates aniamted space per bones and per key
			for(int32 BoneIndex=0; BoneIndex<NumBones; ++BoneIndex)
			{
				FName BoneName = NewSkeleton->GetReferenceSkeleton().GetBoneName(BoneIndex);
				int32 OldBoneIndex = OldSkeleton->GetReferenceSkeleton().FindBoneIndex(BoneName);
				int32 TrackIndex = AnimationTrackNames.Find(BoneName);
				int32 ParentBoneIndex = NewSkeleton->GetReferenceSkeleton().GetParentIndex(BoneIndex);

				if(TrackIndex != INDEX_NONE)
				{
					auto& RawAnimation = RawAnimationData[TrackIndex];
					// fill up keys - calculate PK1 * K1
					for(int32 Key=0; Key<NumKeys; ++Key)
					{
						FTransform AnimatedLocalKey;
						ExtractBoneTransform(RawAnimationData, AnimatedLocalKey, TrackIndex, Interval*Key);

						// note that we apply scale in the animated space
						// at this point, you should have scaled version of animated skeleton
						AnimatedLocalKey.ScaleTranslation(OldToNewTranslationRatio[BoneIndex]);

						if(ParentBoneIndex != INDEX_NONE)
						{
							AnimatedSpaceBases[BoneIndex][Key] = AnimatedLocalKey * AnimatedSpaceBases[ParentBoneIndex][Key];
						}
						else
						{
							AnimatedSpaceBases[BoneIndex][Key] = AnimatedLocalKey;
						}
					}
				}
				else
				{
					// get local spaces from refpose and use that to fill it up
					FTransform LocalTransform = (OldBoneIndex != INDEX_NONE)? OldSkeleton->GetReferenceSkeleton().GetRefBonePose()[OldBoneIndex] : FTransform::Identity;

					for(int32 Key=0; Key<NumKeys; ++Key)
					{
						if(ParentBoneIndex != INDEX_NONE)
						{
							AnimatedSpaceBases[BoneIndex][Key] = LocalTransform * AnimatedSpaceBases[ParentBoneIndex][Key];
						}
						else
						{
							AnimatedSpaceBases[BoneIndex][Key] = LocalTransform;
						}
					}
				}
			}

			// now apply the theta back to the animated space bases
			TArray<struct FRawAnimSequenceTrack> NewRawAnimationData = RawAnimationData;
			for(int32 BoneIndex=0; BoneIndex<NumBones; ++BoneIndex)
			{
				FName BoneName = NewSkeleton->GetReferenceSkeleton().GetBoneName(BoneIndex);
				int32 TrackIndex = AnimationTrackNames.Find(BoneName);
				int32 ParentBoneIndex = NewSkeleton->GetReferenceSkeleton().GetParentIndex(BoneIndex);

				for(int32 Key=0; Key<NumKeys; ++Key)
				{
					// thus PK2 & K2 =  PK1 * K1 * theta where theta = (P1*R1)^(-1) * P2*R2
					// where PK2	: parent transform in component space of animated key for new skeleton
					//		 K2		: local transform of animated key for new skeleton
					//		 PK1	: parent transform in component space of animated key for old skeleton
					//		 K1		: local transform of animated key for old skeleton
					FTransform SpaceBase;
					// we don't just apply it because translation is sensitive
					// we don't like to apply relative transform to tranlsation directly
					// rotation and scale we can, but translation we'd like to use scaled translation instead of transformed location
					// as their relative translation can be different
					SpaceBase.SetRotation(AnimatedSpaceBases[BoneIndex][Key].GetRotation() * RelativeToNewTransform[BoneIndex].GetRotation());
					SpaceBase.SetScale3D(AnimatedSpaceBases[BoneIndex][Key].GetScale3D() * RelativeToNewTransform[BoneIndex].GetScale3D());
					// use animated scaled translation directly
					SpaceBase.SetTranslation(AnimatedSpaceBases[BoneIndex][Key].GetTranslation());
					ConvertedSpaceBases[BoneIndex][Key] = SpaceBase;
					// now calculate local space for animation
					if(ParentBoneIndex != INDEX_NONE)
					{
						// K2 = PK2^(-1) * PK1 * K1 * (P1*R1)^(-1) * P2*R2
						ConvertedLocalSpaces[BoneIndex][Key] = SpaceBase.GetRelativeTransform(ConvertedSpaceBases[ParentBoneIndex][Key]);
					}
					else
					{
						ConvertedLocalSpaces[BoneIndex][Key] = SpaceBase;
					}
				}

				// now save back to animation data
				if(TrackIndex != INDEX_NONE)
				{
					auto& RawAnimation = NewRawAnimationData[TrackIndex];
					RawAnimation.PosKeys.Empty(NumKeys);
					RawAnimation.PosKeys.AddUninitialized(NumKeys);
					RawAnimation.RotKeys.Empty(NumKeys);
					RawAnimation.RotKeys.AddUninitialized(NumKeys);
					RawAnimation.ScaleKeys.Empty(NumKeys);
					RawAnimation.ScaleKeys.AddUninitialized(NumKeys);

					for(int32 Key=0; Key<NumKeys; ++Key)
					{
						RawAnimation.PosKeys[Key] = ConvertedLocalSpaces[BoneIndex][Key].GetLocation();
						RawAnimation.RotKeys[Key] = ConvertedLocalSpaces[BoneIndex][Key].GetRotation();
						RawAnimation.ScaleKeys[Key] = ConvertedLocalSpaces[BoneIndex][Key].GetScale3D();
					}
				}
			}
			RawAnimationData = NewRawAnimationData;
		}
		else
		{
			// this only replaces the primary one, it doesn't replace old ones
			TArray<struct FTrackToSkeletonMap> NewTrackToSkeletonMapTable;
			NewTrackToSkeletonMapTable.Empty(AnimationTrackNames.Num());
			NewTrackToSkeletonMapTable.AddUninitialized(AnimationTrackNames.Num());
			for (int32 Track = 0; Track < AnimationTrackNames.Num(); ++Track)
			{
				int32 BoneIndex = NewSkeleton->GetReferenceSkeleton().FindBoneIndex(AnimationTrackNames[Track]);
				NewTrackToSkeletonMapTable[Track].BoneTreeIndex = BoneIndex;
			}

			// now I have all NewTrack To Skeleton Map Table
			// I'll need to compare with old tracks and copy over if SkeletonIndex == 0
			// if SkeletonIndex != 0, we need to see if we can 
			for (int32 TableId = 0; TableId < NewTrackToSkeletonMapTable.Num(); ++TableId)
			{
				if (ensure(TrackToSkeletonMapTable.IsValidIndex(TableId)))
				{
					if (NewTrackToSkeletonMapTable[TableId].BoneTreeIndex != INDEX_NONE)
					{
						TrackToSkeletonMapTable[TableId].BoneTreeIndex = NewTrackToSkeletonMapTable[TableId].BoneTreeIndex;
					}
					else
					{
						// if not found, delete the track data
						RemoveTrack(TableId);
						NewTrackToSkeletonMapTable.RemoveAt(TableId);
						--TableId;
					}
				}
			}
		}

		// I have to set this here in order for compression
		// that has to happen outside of this after Skeleton changes
		SetSkeleton(NewSkeleton);
	}
	else
	{
		VerifyTrackMap(NewSkeleton);
	}

	PostProcessSequence();
}

void UAnimSequence::PostProcessSequence()
{
	// pre process before compress raw animation data

	// if scale is too small, zero it out. Cause it hard to retarget when compress
	// inverse scale is applied to translation, and causing translation to be huge to retarget, but
	// compression can't handle that much precision. 
	for (auto Iter = RawAnimationData.CreateIterator(); Iter; ++Iter)
	{
		FRawAnimSequenceTrack& RawAnim = (*Iter);

		for (auto ScaleIter = RawAnim.ScaleKeys.CreateIterator(); ScaleIter; ++ScaleIter)
		{
			FVector& Scale3D = *ScaleIter;
			if ( FMath::IsNearlyZero(Scale3D.X) )
			{
				Scale3D.X = 0.f;
			}
			if ( FMath::IsNearlyZero(Scale3D.Y) )
			{
				Scale3D.Y = 0.f;
			}
			if ( FMath::IsNearlyZero(Scale3D.Z) )
			{
				Scale3D.Z = 0.f;
			}
		}

		// make sure Rotation part is normalized before compress
		for(auto RotIter = RawAnim.RotKeys.CreateIterator(); RotIter; ++RotIter)
		{
			FQuat& Rotation = *RotIter;
			if( !Rotation.IsNormalized() )
			{
				Rotation.Normalize();
			}
		}
	}

	CompressRawAnimData();
	// Apply compression
	FAnimationUtils::CompressAnimSequence(this, false, false);
	// initialize notify track
	InitializeNotifyTrack();
	//Make sure we dont have any notifies off the end of the sequence
	ClampNotifiesAtEndOfSequence();
}

void UAnimSequence::RemoveNaNTracks()
{
	bool bRecompress = false;

	for( int32 TrackIndex=0; TrackIndex<RawAnimationData.Num(); ++TrackIndex )
	{
		const FRawAnimSequenceTrack& RawTrack = RawAnimationData[TrackIndex];

		bool bContainsNaN = false;
		for ( auto Key : RawTrack.PosKeys )
		{
			bContainsNaN |= Key.ContainsNaN();
		}

		if (!bContainsNaN)
		{
			for(auto Key : RawTrack.RotKeys)
			{
				bContainsNaN |= Key.ContainsNaN();
			}
		}

		if (!bContainsNaN)
		{
			for(auto Key : RawTrack.ScaleKeys)
			{
				bContainsNaN |= Key.ContainsNaN();
			}
		}

		if (bContainsNaN)
		{
			UE_LOG(LogAnimation, Warning, TEXT("Animation raw data contains NaNs - Removing the following track [%s Track (%s)]"), (GetOuter() ? *GetOuter()->GetFullName() : *GetFullName()), *AnimationTrackNames[TrackIndex].ToString());
			// remove this track
			RemoveTrack(TrackIndex);
			--TrackIndex;

			bRecompress = true;
		}
	}

	if (bRecompress)
	{
		FAnimationUtils::CompressAnimSequence(this, false, false);
	}
}


void UAnimSequence::RemoveTrack(int32 TrackIndex)
{
	if (RawAnimationData.IsValidIndex(TrackIndex))
	{
		RawAnimationData.RemoveAt(TrackIndex);
		AnimationTrackNames.RemoveAt(TrackIndex);
		TrackToSkeletonMapTable.RemoveAt(TrackIndex);
		// source raw animation only exists if edited
		if (SourceRawAnimationData.Num() > 0 )
		{
			SourceRawAnimationData.RemoveAt(TrackIndex);
		}

		check (RawAnimationData.Num() == AnimationTrackNames.Num() && AnimationTrackNames.Num() == TrackToSkeletonMapTable.Num() );
	}
}

int32 FindFirstChildTrack(const USkeleton* MySkeleton, const FReferenceSkeleton& RefSkeleton, const TArray<FName>& AnimationTrackNames, FName BoneName)
{
	const int32 BoneIndex = RefSkeleton.FindBoneIndex(BoneName);
	if(BoneIndex == INDEX_NONE)
	{
		// get out, nothing to do
		return INDEX_NONE;
	}

	// find children
	TArray<int32> Childs;
	if(MySkeleton->GetChildBones(BoneIndex, Childs) > 0)
	{
		// first look for direct children
		for(auto ChildIndex : Childs)
		{
			FName ChildBoneName = RefSkeleton.GetBoneName(ChildIndex);
			int32 ChildTrackIndex = AnimationTrackNames.Find(ChildBoneName);
			if(ChildTrackIndex != INDEX_NONE)
			{
				// found the new track
				return ChildTrackIndex;
			}
		}

		int32 BestGrandChildIndex = INDEX_NONE;
		// if you didn't find yet, now you have to go through all children
		for(auto ChildIndex : Childs)
		{
			FName ChildBoneName = RefSkeleton.GetBoneName(ChildIndex);
			// now I have to go through all childrewn and find who is earliest since I don't know which one might be the closest one
			int32 GrandChildIndex = FindFirstChildTrack(MySkeleton, RefSkeleton, AnimationTrackNames, ChildBoneName);
			if (GrandChildIndex != INDEX_NONE)
			{
				if (BestGrandChildIndex == INDEX_NONE)
				{
					BestGrandChildIndex = GrandChildIndex;
				}
				else if (BestGrandChildIndex > GrandChildIndex)
				{
					// best should be earlier track index
					BestGrandChildIndex = GrandChildIndex;
				}
			}
		}

		return BestGrandChildIndex;
	}
	else
	{
		// there is no child, just add at the end
		return AnimationTrackNames.Num();
	}
}

int32 UAnimSequence::InsertTrack(const FName& BoneName)
{
	// first verify if it doesn't exists, if it does, return
	int32 CurrentTrackIndex = AnimationTrackNames.Find(BoneName);
	if (CurrentTrackIndex != INDEX_NONE)
	{
		return CurrentTrackIndex;
	}

	USkeleton * MySkeleton = GetSkeleton();
	// should not call this if skeleton was empty
	if (ensure(MySkeleton) == false)
	{
		return INDEX_NONE;
	}

	const FReferenceSkeleton& RefSkeleton = MySkeleton->GetReferenceSkeleton();
	int32 NewTrackIndex = FindFirstChildTrack(MySkeleton, RefSkeleton, AnimationTrackNames, BoneName);
	int32 BoneIndex = RefSkeleton.FindBoneIndex(BoneName);
	if (NewTrackIndex != INDEX_NONE)
	{
		const TArray<FTransform>& RefPose = RefSkeleton.GetRefBonePose();

		FRawAnimSequenceTrack RawTrack;
		RawTrack.PosKeys.Add(RefPose[BoneIndex].GetTranslation());
		RawTrack.RotKeys.Add(RefPose[BoneIndex].GetRotation());
		RawTrack.ScaleKeys.Add(RefPose[BoneIndex].GetScale3D());

		// now insert to the track
		RawAnimationData.Insert(RawTrack, NewTrackIndex);
		AnimationTrackNames.Insert(BoneName, NewTrackIndex);
		SourceRawAnimationData.Insert(RawTrack, NewTrackIndex);

		RefreshTrackMapFromAnimTrackNames();

		check(RawAnimationData.Num() == AnimationTrackNames.Num() && AnimationTrackNames.Num() == TrackToSkeletonMapTable.Num());
	}

	return NewTrackIndex;
}

bool UAnimSequence::GetAllAnimationSequencesReferred(TArray<UAnimSequence*>& AnimationSequences)
{
	if (RefPoseSeq)
	{
		AnimationSequences.AddUnique(RefPoseSeq);
	}
	return AnimationSequences.Num() > 0;
}

void UAnimSequence::ReplaceReferredAnimations(const TMap<UAnimSequence*, UAnimSequence*>& ReplacementMap)
{
	if (RefPoseSeq)
	{
		UAnimSequence* const* ReplacementAsset = (UAnimSequence*const*)ReplacementMap.Find(RefPoseSeq);
		if (ReplacementAsset)
		{
			RefPoseSeq = *ReplacementAsset;
		}
	}
}

bool UAnimSequence::AddLoopingInterpolation()
{
	int32 NumTracks = AnimationTrackNames.Num();
	float Interval = GetIntervalPerKey(NumFrames, SequenceLength);

	if(NumFrames > 0)
	{
		// added one more key
		int32 NewNumKeys = NumFrames +1 ;

		// now I need to calculate back to new animation data
		for(int32 TrackIndex=0; TrackIndex<NumTracks; ++TrackIndex)
		{
			auto& RawAnimation = RawAnimationData[TrackIndex];
			if (RawAnimation.PosKeys.Num() > 1)
			{
				FVector FirstKey = RawAnimation.PosKeys[0];
				RawAnimation.PosKeys.Add(FirstKey);
			}

			if(RawAnimation.RotKeys.Num() > 1)
			{
				FQuat FirstKey = RawAnimation.RotKeys[0];
				RawAnimation.RotKeys.Add(FirstKey);
			}

			if(RawAnimation.ScaleKeys.Num() > 1)
			{
				FVector FirstKey = RawAnimation.ScaleKeys[0];
				RawAnimation.ScaleKeys.Add(FirstKey);
			}
		}

		SequenceLength += Interval;
		NumFrames = NewNumKeys;

		PostProcessSequence();
		return true;
	}

	return false;
}
int32 FindParentNodeIndex(URig* Rig, USkeleton* Skeleton, FName ParentNodeName)
{
	const int32& ParentNodeIndex = Rig->FindNode(ParentNodeName);
	const FName& ParentBoneName = Skeleton->GetRigBoneMapping(ParentNodeName);
	
	return Skeleton->GetReferenceSkeleton().FindBoneIndex(ParentBoneName);
}

int32 UAnimSequence::GetSpaceBasedAnimationData(TArray< TArray<FTransform> >& AnimationDataInComponentSpace, FAnimSequenceTrackContainer * RiggingAnimationData) const
{
	USkeleton* Skeleton = GetSkeleton();

	check(Skeleton);
	const FReferenceSkeleton& RefSkeleton = Skeleton->GetReferenceSkeleton();
	int32 NumBones = RefSkeleton.GetNum();

	AnimationDataInComponentSpace.Empty(NumBones);
	AnimationDataInComponentSpace.AddZeroed(NumBones);

	// 2d array of animated time [boneindex][time key]
	int32 NumKeys = NumFrames;
	float Interval = GetIntervalPerKey(NumFrames, SequenceLength);

	// allocate arrays
	for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
	{
		AnimationDataInComponentSpace[BoneIndex].AddUninitialized(NumKeys);
	}

	if (RiggingAnimationData)
	{
		const URig* Rig = Skeleton->GetRig();

		check(Rig);

		// to fix the issue where parent of rig doesn't correspond to parent of this skeleton
		// we do this in multiple iteration if needed. 
		// this flag will be used to evaluate all of them until done
		TArray<bool> BoneEvaluated;
		BoneEvaluated.AddZeroed(NumBones);

		bool bCompleted = false;
		do
		{
			for(int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
			{
				if ( !BoneEvaluated[BoneIndex] )
				{
					const FName& BoneName = RefSkeleton.GetBoneName(BoneIndex);
					const FName& NodeName = Skeleton->GetRigNodeNameFromBoneName(BoneName);
					const FTransformBase* TransformBase = Rig->GetTransformBaseByNodeName(NodeName);
					const int32 NodeIndex = RiggingAnimationData->TrackNames.Find(NodeName);
					if(NodeIndex != INDEX_NONE)
					{
						check(TransformBase);

						// now calculate the component space
						const TArray<FRigTransformConstraint>	& RotTransformConstraints = TransformBase->Constraints[EControlConstraint::Type::Orientation].TransformConstraints;

						FQuat ComponentRotation;
						FTransform ComponentTranslation;
						FVector ComponentScale = FVector(1.f);

						// rotation first
						// this is easy since we just make sure it's evaluated or not
						{
							const FName& ParentNodeName = RotTransformConstraints[0].ParentSpace;
							const FName& ParentBoneName = Skeleton->GetRigBoneMapping(ParentNodeName);
							const int32& ParentBoneIndex = RefSkeleton.FindBoneIndex(ParentBoneName);

							if(ParentBoneIndex != INDEX_NONE)
							{
								if (BoneEvaluated[ParentBoneIndex])
								{
									for(int32 Key = 0; Key < NumKeys; ++Key)
									{
										ComponentRotation = AnimationDataInComponentSpace[ParentBoneIndex][Key].GetRotation() * RiggingAnimationData->AnimationTracks[NodeIndex].RotKeys[Key];
										AnimationDataInComponentSpace[BoneIndex][Key].SetRotation(ComponentRotation);
									}

									BoneEvaluated[BoneIndex] = true;
								}
							}
							else
							{
								for(int32 Key = 0; Key < NumKeys; ++Key)
								{
									ComponentRotation = RiggingAnimationData->AnimationTracks[NodeIndex].RotKeys[Key];
									AnimationDataInComponentSpace[BoneIndex][Key].SetRotation(ComponentRotation);
								}

								BoneEvaluated[BoneIndex] = true;
							}
						}

						const TArray<FRigTransformConstraint>	& PosTransformConstraints = TransformBase->Constraints[EControlConstraint::Type::Translation].TransformConstraints;

						// now time to check translation
						// this is a bit more complicated
						// since we have to make sure if it's true to start with
						// did we succeed on getting rotation?
						if (BoneEvaluated[BoneIndex])
						{
							const FName& ParentNodeName = PosTransformConstraints[0].ParentSpace;
							const FName& ParentBoneName = Skeleton->GetRigBoneMapping(ParentNodeName);
							const int32& ParentBoneIndex = RefSkeleton.FindBoneIndex(ParentBoneName);

							if(ParentBoneIndex != INDEX_NONE)
							{
								// this has to be check
								if (BoneEvaluated[ParentBoneIndex])
								{
									for(int32 Key = 0; Key < NumKeys; ++Key)
									{
										ComponentTranslation = FTransform(RiggingAnimationData->AnimationTracks[NodeIndex].PosKeys[Key]) * AnimationDataInComponentSpace[ParentBoneIndex][Key];
										AnimationDataInComponentSpace[BoneIndex][Key].SetTranslation(ComponentTranslation.GetTranslation());
										AnimationDataInComponentSpace[BoneIndex][Key].SetScale3D(ComponentScale);
									}
								}
								else
								{
									// if we failed to get parent clear the flag
									// because if translation has been calculated, BoneEvaluated[BoneIndex] might be true
									BoneEvaluated[BoneIndex] = false;
								}
							}
							else
							{
								for(int32 Key = 0; Key < NumKeys; ++Key)
								{
									ComponentTranslation = FTransform(RiggingAnimationData->AnimationTracks[NodeIndex].PosKeys[Key]);
									AnimationDataInComponentSpace[BoneIndex][Key].SetTranslation(ComponentTranslation.GetTranslation());
									AnimationDataInComponentSpace[BoneIndex][Key].SetScale3D(ComponentScale);
								}
							}
						}
					}
					else
					{
						int32 ParentIndex = RefSkeleton.GetParentIndex(BoneIndex);
						const FTransform& LocalSpace = RefSkeleton.GetRefBonePose()[BoneIndex];
						if(ParentIndex != INDEX_NONE)
						{
							// if parent is evaluated, do it
							if (BoneEvaluated[ParentIndex])
							{
								for(int32 Key = 0; Key < NumKeys; ++Key)
								{
									AnimationDataInComponentSpace[BoneIndex][Key] = LocalSpace * AnimationDataInComponentSpace[ParentIndex][Key];
								}

								BoneEvaluated[BoneIndex] = true;
							}
						}
						else
						{
							BoneEvaluated[BoneIndex] = true;

							for(int32 Key = 0; Key < NumKeys; ++Key)
							{
								AnimationDataInComponentSpace[BoneIndex][Key] = LocalSpace;
							}
						}
					}
				}
			}

			bCompleted = true;
			// see if we can get out, brute force for now
			for(int32 BoneIndex = 0; BoneIndex < NumBones && bCompleted; ++BoneIndex)
			{
				bCompleted &= !!BoneEvaluated[BoneIndex];
			}
		} while (bCompleted == false);
	}
	else
	{
		// now calculating old animated space bases
		// this one calculates aniamted space per bones and per key
		for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
		{
			FName BoneName = Skeleton->GetReferenceSkeleton().GetBoneName(BoneIndex);
			int32 TrackIndex = AnimationTrackNames.Find(BoneName);
			int32 ParentBoneIndex = Skeleton->GetReferenceSkeleton().GetParentIndex(BoneIndex);

			if (TrackIndex != INDEX_NONE)
			{
				auto& RawAnimation = RawAnimationData[TrackIndex];
				// fill up keys - calculate PK1 * K1
				for (int32 Key = 0; Key < NumKeys; ++Key)
				{
					FTransform AnimatedLocalKey;
					ExtractBoneTransform(RawAnimationData, AnimatedLocalKey, TrackIndex, Interval*Key);

					if (ParentBoneIndex != INDEX_NONE)
					{
						AnimationDataInComponentSpace[BoneIndex][Key] = AnimatedLocalKey * AnimationDataInComponentSpace[ParentBoneIndex][Key];
					}
					else
					{
						AnimationDataInComponentSpace[BoneIndex][Key] = AnimatedLocalKey;
					}
				}
			}
			else
			{
				// get local spaces from refpose and use that to fill it up
				FTransform LocalTransform = Skeleton->GetReferenceSkeleton().GetRefBonePose()[BoneIndex];

				for (int32 Key = 0; Key < NumKeys; ++Key)
				{
					if (ParentBoneIndex != INDEX_NONE)
					{
						AnimationDataInComponentSpace[BoneIndex][Key] = LocalTransform * AnimationDataInComponentSpace[ParentBoneIndex][Key];
					}
					else
					{
						AnimationDataInComponentSpace[BoneIndex][Key] = LocalTransform;
					}
				}
			}
		}

	}

	return AnimationDataInComponentSpace.Num();
}

bool UAnimSequence::ConvertAnimationDataToRiggingData(FAnimSequenceTrackContainer& RiggingAnimationData)
{
	USkeleton* Skeleton = GetSkeleton();
	if (Skeleton && Skeleton->GetRig())
	{
		const URig* Rig = Skeleton->GetRig();
		TArray<FName> ValidNodeNames;
		int32 NumNodes = Skeleton->GetMappedValidNodes(ValidNodeNames);
		TArray< TArray<FTransform> > AnimationDataInComponentSpace;
		int32 NumBones = GetSpaceBasedAnimationData(AnimationDataInComponentSpace, NULL);

		if (NumBones > 0)
		{
			RiggingAnimationData.Initialize(ValidNodeNames);

			// first we copy all space bases back to it
			for (int32 NodeIndex = 0; NodeIndex < NumNodes; ++NodeIndex)
			{
				struct FRawAnimSequenceTrack& Track = RiggingAnimationData.AnimationTracks[NodeIndex];
				const FName& NodeName = ValidNodeNames[NodeIndex];
				const FName& BoneName = Skeleton->GetRigBoneMapping(NodeName);
				const int32& BoneIndex = Skeleton->GetReferenceSkeleton().FindBoneIndex(BoneName);

				if (ensure(BoneIndex != INDEX_NONE))
				{
					Track.PosKeys.Empty(NumFrames);
					Track.RotKeys.Empty(NumFrames);
					Track.ScaleKeys.Empty(NumFrames);
					Track.PosKeys.AddUninitialized(NumFrames);
					Track.RotKeys.AddUninitialized(NumFrames);
					Track.ScaleKeys.AddUninitialized(NumFrames);

					int32 RigConstraintIndex = Rig->FindTransformBaseByNodeName(NodeName);

					if (RigConstraintIndex != INDEX_NONE)
					{
						const auto* RigConstraint = Rig->GetTransformBase(RigConstraintIndex);

						// apply orientation - for now only one
						const TArray<FRigTransformConstraint>& RotationTransformConstraint = RigConstraint->Constraints[EControlConstraint::Type::Orientation].TransformConstraints;

						if (RotationTransformConstraint.Num() > 0)
						{
							const FName& ParentSpace = RotationTransformConstraint[0].ParentSpace;
							const FName& ParentBoneName = Skeleton->GetRigBoneMapping(ParentSpace);
							const int32& ParentBoneIndex = Skeleton->GetReferenceSkeleton().FindBoneIndex(ParentBoneName);
							if (ParentBoneIndex != INDEX_NONE)
							{
								// if no rig control, component space is used
								for (int32 KeyIndex = 0; KeyIndex < NumFrames; ++KeyIndex)
								{
									FTransform ParentTransform = AnimationDataInComponentSpace[ParentBoneIndex][KeyIndex];
									FTransform RelativeTransform = AnimationDataInComponentSpace[BoneIndex][KeyIndex].GetRelativeTransform(ParentTransform);
									Track.RotKeys[KeyIndex] = RelativeTransform.GetRotation();
								}
							}
							else
							{
								// if no rig control, component space is used
								for (int32 KeyIndex = 0; KeyIndex < NumFrames; ++KeyIndex)
								{
									Track.RotKeys[KeyIndex] = AnimationDataInComponentSpace[BoneIndex][KeyIndex].GetRotation();
								}
							}
						}
						else
						{
							// if no rig control, component space is used
							for (int32 KeyIndex = 0; KeyIndex < NumFrames; ++KeyIndex)
							{
								Track.RotKeys[KeyIndex] = AnimationDataInComponentSpace[BoneIndex][KeyIndex].GetRotation();
							}
						}

						// apply translation - for now only one
						const TArray<FRigTransformConstraint>& TranslationTransformConstraint = RigConstraint->Constraints[EControlConstraint::Type::Translation].TransformConstraints;

						if (TranslationTransformConstraint.Num() > 0)
						{
							const FName& ParentSpace = TranslationTransformConstraint[0].ParentSpace;
							const FName& ParentBoneName = Skeleton->GetRigBoneMapping(ParentSpace);
							const int32& ParentBoneIndex = Skeleton->GetReferenceSkeleton().FindBoneIndex(ParentBoneName);
							if (ParentBoneIndex != INDEX_NONE)
							{
								// if no rig control, component space is used
								for (int32 KeyIndex = 0; KeyIndex < NumFrames; ++KeyIndex)
								{
									FTransform ParentTransform = AnimationDataInComponentSpace[ParentBoneIndex][KeyIndex];
									FTransform RelativeTransform = AnimationDataInComponentSpace[BoneIndex][KeyIndex].GetRelativeTransform(ParentTransform);
									Track.PosKeys[KeyIndex] = RelativeTransform.GetTranslation();
									Track.ScaleKeys[KeyIndex] = RelativeTransform.GetScale3D();
								}
							}
							else
							{
								for (int32 KeyIndex = 0; KeyIndex < NumFrames; ++KeyIndex)
								{
									Track.PosKeys[KeyIndex] = AnimationDataInComponentSpace[BoneIndex][KeyIndex].GetTranslation();
									Track.ScaleKeys[KeyIndex] = AnimationDataInComponentSpace[BoneIndex][KeyIndex].GetScale3D();
								}
							}
						}
						else
						{
							for (int32 KeyIndex = 0; KeyIndex < NumFrames; ++KeyIndex)
							{
								Track.PosKeys[KeyIndex] = AnimationDataInComponentSpace[BoneIndex][KeyIndex].GetTranslation();
								Track.ScaleKeys[KeyIndex] = AnimationDataInComponentSpace[BoneIndex][KeyIndex].GetScale3D();
							}
						}
					}
					else
					{
						// if no rig control, component space is used
						for (int32 KeyIndex = 0; KeyIndex < NumFrames; ++KeyIndex)
						{
							Track.PosKeys[KeyIndex] = AnimationDataInComponentSpace[BoneIndex][KeyIndex].GetTranslation();
							Track.RotKeys[KeyIndex] = AnimationDataInComponentSpace[BoneIndex][KeyIndex].GetRotation();
							Track.ScaleKeys[KeyIndex] = AnimationDataInComponentSpace[BoneIndex][KeyIndex].GetScale3D();
						}
					}
				}
			}
		}

		return true;
	}

	return false;
}

bool UAnimSequence::ConvertRiggingDataToAnimationData(FAnimSequenceTrackContainer& RiggingAnimationData)
{
	if (RiggingAnimationData.GetNum() > 0)
	{
		TArray< TArray<FTransform> > AnimationDataInComponentSpace;
		int32 NumBones = GetSpaceBasedAnimationData(AnimationDataInComponentSpace, &RiggingAnimationData);

		USkeleton* Skeleton = GetSkeleton();
		TArray<FRawAnimSequenceTrack> OldAnimationData = RawAnimationData;
		TArray<FName> OldAnimationTrackNames = AnimationTrackNames;
		TArray<FName> ValidNodeNames;
		Skeleton->GetMappedValidNodes(ValidNodeNames);
		// remove from ValidNodeNames if it doesn't belong to AnimationTrackNames
		for (int32 NameIndex=0; NameIndex<ValidNodeNames.Num(); ++NameIndex)
		{
			if (RiggingAnimationData.TrackNames.Contains(ValidNodeNames[NameIndex]) == false)
			{
				ValidNodeNames.RemoveAt(NameIndex);
				--NameIndex;
			}
		}

		int32 ValidNumNodes = ValidNodeNames.Num();

		// get local spaces
		// add all tracks?
		AnimationTrackNames.Empty(ValidNumNodes);
		AnimationTrackNames.AddUninitialized(ValidNumNodes);
		RawAnimationData.Empty(ValidNumNodes);
		RawAnimationData.AddZeroed(ValidNumNodes);

		const FReferenceSkeleton& RefSkeleton = Skeleton->GetReferenceSkeleton();
		const URig* Rig = Skeleton->GetRig();
		for (int32 NodeIndex = 0; NodeIndex < ValidNumNodes; ++NodeIndex)
		{
			FName BoneName = Skeleton->GetRigBoneMapping(ValidNodeNames[NodeIndex]);
			int32 BoneIndex = RefSkeleton.FindBoneIndex(BoneName);

			if (BoneIndex != INDEX_NONE)
			{
				// add track names
				AnimationTrackNames[NodeIndex] = BoneName;

				// update bone trasfnrom
				FRawAnimSequenceTrack& Track = RawAnimationData[NodeIndex];

				Track.PosKeys.Empty();
				Track.RotKeys.Empty();
				Track.ScaleKeys.Empty();
				Track.PosKeys.AddUninitialized(NumFrames);
				Track.RotKeys.AddUninitialized(NumFrames);
				Track.ScaleKeys.AddUninitialized(NumFrames);

				const int32& ParentBoneIndex = RefSkeleton.GetParentIndex(BoneIndex);

				if(ParentBoneIndex != INDEX_NONE)
				{
					for(int32 KeyIndex = 0; KeyIndex < NumFrames; ++KeyIndex)
					{
						FTransform LocalTransform = AnimationDataInComponentSpace[BoneIndex][KeyIndex].GetRelativeTransform(AnimationDataInComponentSpace[ParentBoneIndex][KeyIndex]);

						Track.PosKeys[KeyIndex] = LocalTransform.GetTranslation();
						Track.RotKeys[KeyIndex] = LocalTransform.GetRotation();
						Track.ScaleKeys[KeyIndex] = LocalTransform.GetScale3D();
					}
				}
				else
				{
					for(int32 KeyIndex = 0; KeyIndex < NumFrames; ++KeyIndex)
					{
						FTransform LocalTransform = AnimationDataInComponentSpace[BoneIndex][KeyIndex];

						Track.PosKeys[KeyIndex] = LocalTransform.GetTranslation();
						Track.RotKeys[KeyIndex] = LocalTransform.GetRotation();
						Track.ScaleKeys[KeyIndex] = LocalTransform.GetScale3D();
					}
				}
			}
		}

		// recreate track map
		TrackToSkeletonMapTable.Empty(AnimationTrackNames.Num());
		TrackToSkeletonMapTable.AddUninitialized(AnimationTrackNames.Num());
		int32 TrackIdx = 0;
		for (auto TrackName : AnimationTrackNames)
		{
			TrackToSkeletonMapTable[TrackIdx++].BoneTreeIndex = Skeleton->GetReferenceSkeleton().FindBoneIndex(TrackName);
		}
		PostProcessSequence();

		return true;
	}

	return false;
}

void UAnimSequence::BakeTrackCurvesToRawAnimation()
{
	// now bake the curves to the RawAnimationData
	if(NumFrames == 0)
	{
		// fail error?
		return;
	}

	if (!DoesContainTransformCurves())
	{
		if (SourceRawAnimationData.Num() > 0)
		{
			// if curve doesn't exists, we just bring back Source to Raw, and clears Source
			RawAnimationData = MoveTemp(SourceRawAnimationData);
			PostProcessSequence();
		}
	}
	else
	{
		if(SourceRawAnimationData.Num() == 0)
		{
			// if source data is empty, this is first time
			// copies the data
			SourceRawAnimationData = RawAnimationData;
		}
		else
		{
			// we copy SourceRawAnimationData because we'd need to create additive on top of current one
			RawAnimationData = SourceRawAnimationData;
		}

		USkeleton * CurSkeleton = GetSkeleton();
		check(CurSkeleton);

		FSmartNameMapping* NameMapping = CurSkeleton->SmartNames.GetContainer(USkeleton::AnimTrackCurveMappingName);
		// if no mapping, that means there is no transform curves
		if(!NameMapping)
		{
			// if no name mapping is found but curve exists, we should verify curve namex
			VerifyCurveNames(CurSkeleton, USkeleton::AnimTrackCurveMappingName, RawCurveData.TransformCurves);
			NameMapping = CurSkeleton->SmartNames.GetContainer(USkeleton::AnimTrackCurveMappingName);
		}
		
		// since now I'm about to modify Scale Keys. I should add all of them here at least one key. 
		// if all turns out to be same, it will clear it up. 
		for (auto & RawTrack: RawAnimationData)
		{
			if (RawTrack.ScaleKeys.Num() == 0)
			{
				// at least add one
				static FVector ScaleConstantKey(1.f);
				RawTrack.ScaleKeys.Add(ScaleConstantKey);
			}
		}

		for(const auto& Curve : RawCurveData.TransformCurves)
		{
			// find curves first, and then see what is index of this curve
			FName BoneName;

			if(Curve.GetCurveTypeFlag(ACF_Disabled)== false &&
				ensure(NameMapping->GetName(Curve.CurveUid, BoneName)))
			{
				int32 TrackIndex = AnimationTrackNames.Find(BoneName);

				// the animation data doesn't have this track, so insert it
				if(TrackIndex == INDEX_NONE)
				{
					TrackIndex = InsertTrack(BoneName);
					// if it still didn't find, something went horribly wrong
					if(ensure(TrackIndex != INDEX_NONE) == false)
					{
						UE_LOG(LogAnimation, Warning, TEXT("Animation Baking : Error adding %s track."), *BoneName.ToString());
						// I can't do anything about it
						continue;
					}
				}

				// now modify data
				auto& RawTrack = RawAnimationData[TrackIndex];

				// since now we're editing keys, 
				// if 1 (which meant constant), just expands to # of frames
				if(RawTrack.PosKeys.Num() == 1)
				{
					FVector OneKey = RawTrack.PosKeys[0];
					RawTrack.PosKeys.Init(OneKey, NumFrames);
				}
				else
				{
					ensure(RawTrack.PosKeys.Num() == NumFrames);
				}

				if(RawTrack.RotKeys.Num() == 1)
				{
					FQuat OneKey = RawTrack.RotKeys[0];
					RawTrack.RotKeys.Init(OneKey, NumFrames);
				}
				else
				{
					ensure(RawTrack.RotKeys.Num() == NumFrames);
				}

				// although we don't allow edit of scale
				// it is important to consider scale when apply transform
				// so make sure this also is included
				if(RawTrack.ScaleKeys.Num() == 1)
				{
					FVector OneKey = RawTrack.ScaleKeys[0];
					RawTrack.ScaleKeys.Init(OneKey, NumFrames);
				}
				else
				{
					ensure(RawTrack.ScaleKeys.Num() == NumFrames);
				}

				// NumFrames can't be zero (filtered earlier)
				float Interval = GetIntervalPerKey(NumFrames, SequenceLength);

				// now we have all data ready to apply
				for(int32 KeyIndex=0; KeyIndex < NumFrames; ++KeyIndex)
				{
					// now evaluate
					FTransformCurve* TransformCurve = static_cast<FTransformCurve*>(RawCurveData.GetCurveData(Curve.CurveUid, FRawCurveTracks::TransformType));

					if(ensure(TransformCurve))
					{
						FTransform AdditiveTransform = TransformCurve->Evaluate(KeyIndex * Interval, 1.0);
						FTransform LocalTransform(RawTrack.RotKeys[KeyIndex], RawTrack.PosKeys[KeyIndex], RawTrack.ScaleKeys[KeyIndex]);
						//  						LocalTransform = LocalTransform * AdditiveTransform;
						//  						RawTrack.RotKeys[KeyIndex] = LocalTransform.GetRotation();
						//  						RawTrack.PosKeys[KeyIndex] = LocalTransform.GetTranslation();
						//  						RawTrack.ScaleKeys[KeyIndex] = LocalTransform.GetScale3D();

						RawTrack.RotKeys[KeyIndex] = LocalTransform.GetRotation() * AdditiveTransform.GetRotation();
						RawTrack.PosKeys[KeyIndex] = LocalTransform.TransformPosition(AdditiveTransform.GetTranslation());
						RawTrack.ScaleKeys[KeyIndex] = LocalTransform.GetScale3D() * AdditiveTransform.GetScale3D();
					}
					else
					{
						UE_LOG(LogAnimation, Warning, TEXT("Animation Baking : Missing Curve for %s."), *BoneName.ToString());
					}
				}
			}
		}

		PostProcessSequence();
	}

	bNeedsRebake = false;
}

bool UAnimSequence::DoesNeedRebake() const
{
	return (bNeedsRebake);
}

bool UAnimSequence::DoesContainTransformCurves() const
{
	return (RawCurveData.TransformCurves.Num() > 0);
}

void UAnimSequence::AddKeyToSequence(float Time, const FName& BoneName, const FTransform& AdditiveTransform)
{
	// find if this already exists, then just add curve data only
	FName CurveName = BoneName;
	USkeleton * CurrentSkeleton = GetSkeleton();
	check (CurrentSkeleton);

	FSmartNameMapping* NameMapping = CurrentSkeleton->SmartNames.GetContainer(USkeleton::AnimTrackCurveMappingName);
	if(NameMapping == NULL)
	{
		CurrentSkeleton->Modify(true);
		CurrentSkeleton->SmartNames.AddContainer(USkeleton::AnimTrackCurveMappingName);
		NameMapping = CurrentSkeleton->SmartNames.GetContainer(USkeleton::AnimTrackCurveMappingName);
		check(NameMapping);
	}

	FSmartNameMapping::UID NewUID;
	NameMapping->AddOrFindName(CurveName, NewUID);

	// add curve - this won't add duplicate curve
	RawCurveData.AddCurveData(NewUID, ACF_DriveTrack | ACF_Editable, FRawCurveTracks::TransformType);

	//Add this curve
	FTransformCurve* TransformCurve = static_cast<FTransformCurve*>(RawCurveData.GetCurveData(NewUID, FRawCurveTracks::TransformType));
	check(TransformCurve);

	TransformCurve->UpdateOrAddKey(AdditiveTransform, Time);

	bNeedsRebake = true;
}

void UAnimSequence::ResetAnimation()
{
	// clear everything. Making new animation, so need to reset all the things that belong here
	NumFrames = 0;
	SequenceLength = 0.f;
	RawAnimationData.Empty();
	SourceRawAnimationData.Empty();
	AnimationTrackNames.Empty();
	TrackToSkeletonMapTable.Empty();
	CompressedTrackOffsets.Empty();
	CompressedScaleOffsets.Empty();
	CompressedByteStream.Empty();

	Notifies.Empty();
	AnimNotifyTracks.Empty();
	RawCurveData.Empty();
	RateScale = 1.f;
}

void UAnimSequence::RefreshTrackMapFromAnimTrackNames()
{
	TrackToSkeletonMapTable.Empty();

	const USkeleton * MySkeleton = GetSkeleton();
	const FReferenceSkeleton& RefSkeleton = MySkeleton->GetReferenceSkeleton();
	const int32 NumBones = AnimationTrackNames.Num();
	TrackToSkeletonMapTable.AddUninitialized(NumBones);

	bool bNeedsFixing = false;
	const int32 NumTracks = AnimationTrackNames.Num();
	for(int32 I=NumTracks-1; I>=0; --I)
	{
		int32 BoneTreeIndex = RefSkeleton.FindBoneIndex(AnimationTrackNames[I]);
		if(BoneTreeIndex == INDEX_NONE)
		{
			RemoveTrack(I);
		}
		else
		{
			TrackToSkeletonMapTable[I].BoneTreeIndex = BoneTreeIndex;
		}
	}
}

bool UAnimSequence::CreateAnimation(USkeletalMesh * Mesh)
{
	// create animation from Mesh's ref pose
	if (Mesh)
	{
		ResetAnimation();

		const FReferenceSkeleton& RefSkeleton = Mesh->RefSkeleton;
		SequenceLength = MINIMUM_ANIMATION_LENGTH;
		NumFrames = 1;

		const int32 NumBones = RefSkeleton.GetNum();
		RawAnimationData.AddZeroed(NumBones);
		AnimationTrackNames.AddUninitialized(NumBones);

		const TArray<FTransform>& RefBonePose = RefSkeleton.GetRefBonePose();

		check (RefBonePose.Num() == NumBones);

		for (int32 BoneIndex=0; BoneIndex<NumBones; ++BoneIndex)
		{
			AnimationTrackNames[BoneIndex] = RefSkeleton.GetBoneName(BoneIndex);

			FRawAnimSequenceTrack& RawTrack = RawAnimationData[BoneIndex];

			RawTrack.PosKeys.Add(RefBonePose[BoneIndex].GetTranslation());
			RawTrack.RotKeys.Add(RefBonePose[BoneIndex].GetRotation());
			RawTrack.ScaleKeys.Add(RefBonePose[BoneIndex].GetScale3D());
		}

		// refresh TrackToskeletonMapIndex
		RefreshTrackMapFromAnimTrackNames();

		// should recreate track map
		PostProcessSequence();
		return true;
	}

	return false;
}

bool UAnimSequence::CreateAnimation(USkeletalMeshComponent * MeshComponent)
{
	if(MeshComponent && MeshComponent->SkeletalMesh)
	{
		USkeletalMesh * Mesh = MeshComponent->SkeletalMesh;

		ResetAnimation();

		const FReferenceSkeleton& RefSkeleton = Mesh->RefSkeleton;
		SequenceLength = MINIMUM_ANIMATION_LENGTH;
		NumFrames = 1;

		const int32 NumBones = RefSkeleton.GetNum();
		RawAnimationData.AddZeroed(NumBones);
		AnimationTrackNames.AddUninitialized(NumBones);

		const TArray<FTransform>& LocalAtoms = MeshComponent->LocalAtoms;

		check(LocalAtoms.Num() == NumBones);

		for(int32 BoneIndex=0; BoneIndex<NumBones; ++BoneIndex)
		{
			AnimationTrackNames[BoneIndex] = RefSkeleton.GetBoneName(BoneIndex);

			FRawAnimSequenceTrack& RawTrack = RawAnimationData[BoneIndex];

			RawTrack.PosKeys.Add(LocalAtoms[BoneIndex].GetTranslation());
			RawTrack.RotKeys.Add(LocalAtoms[BoneIndex].GetRotation());
			RawTrack.ScaleKeys.Add(LocalAtoms[BoneIndex].GetScale3D());
		}

		// refresh TrackToskeletonMapIndex
		RefreshTrackMapFromAnimTrackNames();

		// should recreate track map
		PostProcessSequence();
		return true;
	}

	return false;
}

bool UAnimSequence::CreateAnimation(UAnimSequence * Sequence)
{
	if(Sequence)
	{
		ResetAnimation();

		SequenceLength = Sequence->SequenceLength;
		NumFrames = Sequence->NumFrames;

		RawAnimationData = Sequence->RawAnimationData;
		AnimationTrackNames = Sequence->AnimationTrackNames;

		Notifies = Sequence->Notifies;
		AnimNotifyTracks = Sequence->AnimNotifyTracks;
		RawCurveData = Sequence->RawCurveData;

		if (DoesContainTransformCurves())
		{
			bNeedsRebake = true;
		}

		// refresh TrackToskeletonMapIndex
		RefreshTrackMapFromAnimTrackNames();

		// should recreate track map
		PostProcessSequence();
		return true;
	}
	
	return false;
}

#endif
/*-----------------------------------------------------------------------------
	AnimNotify& subclasses
-----------------------------------------------------------------------------*/




////////////
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

void GatherAnimSequenceStats(FOutputDevice& Ar)
{
	int32 AnimationKeyFormatNum[AKF_MAX];
	int32 TranslationCompressionFormatNum[ACF_MAX];
	int32 RotationCompressionFormatNum[ACF_MAX];
	int32 ScaleCompressionFormatNum[ACF_MAX];
	FMemory::Memzero( AnimationKeyFormatNum, AKF_MAX * sizeof(int32) );
	FMemory::Memzero( TranslationCompressionFormatNum, ACF_MAX * sizeof(int32) );
	FMemory::Memzero( RotationCompressionFormatNum, ACF_MAX * sizeof(int32) );
	FMemory::Memzero( ScaleCompressionFormatNum, ACF_MAX * sizeof(int32) );

	Ar.Logf( TEXT(" %60s, Frames,NTT,NRT, NT1,NR1, TotTrnKys,TotRotKys,Codec,ResBytes"), TEXT("Sequence Name") );
	int32 GlobalNumTransTracks = 0;
	int32 GlobalNumRotTracks = 0;
	int32 GlobalNumScaleTracks = 0;
	int32 GlobalNumTransTracksWithOneKey = 0;
	int32 GlobalNumRotTracksWithOneKey = 0;
	int32 GlobalNumScaleTracksWithOneKey = 0;
	int32 GlobalApproxCompressedSize = 0;
	int32 GlobalApproxKeyDataSize = 0;
	int32 GlobalNumTransKeys = 0;
	int32 GlobalNumRotKeys = 0;
	int32 GlobalNumScaleKeys = 0;

	for( TObjectIterator<UAnimSequence> It; It; ++It )
	{
		UAnimSequence* Seq = *It;

		int32 NumTransTracks = 0;
		int32 NumRotTracks = 0;
		int32 NumScaleTracks = 0;
		int32 TotalNumTransKeys = 0;
		int32 TotalNumRotKeys = 0;
		int32 TotalNumScaleKeys = 0;
		float TranslationKeySize = 0.0f;
		float RotationKeySize = 0.0f;
		float ScaleKeySize = 0.0f;
		int32 OverheadSize = 0;
		int32 NumTransTracksWithOneKey = 0;
		int32 NumRotTracksWithOneKey = 0;
		int32 NumScaleTracksWithOneKey = 0;

		AnimationFormat_GetStats(
			Seq, 
			NumTransTracks,
			NumRotTracks,
			NumScaleTracks,
			TotalNumTransKeys,
			TotalNumRotKeys,
			TotalNumScaleKeys,
			TranslationKeySize,
			RotationKeySize,
			ScaleKeySize, 
			OverheadSize,
			NumTransTracksWithOneKey,
			NumRotTracksWithOneKey,
			NumScaleTracksWithOneKey);

		GlobalNumTransTracks += NumTransTracks;
		GlobalNumRotTracks += NumRotTracks;
		GlobalNumScaleTracks += NumScaleTracks;
		GlobalNumTransTracksWithOneKey += NumTransTracksWithOneKey;
		GlobalNumRotTracksWithOneKey += NumRotTracksWithOneKey;
		GlobalNumScaleTracksWithOneKey += NumScaleTracksWithOneKey;

		GlobalApproxCompressedSize += Seq->GetApproxCompressedSize();
		GlobalApproxKeyDataSize += (int32)((TotalNumTransKeys * TranslationKeySize) + (TotalNumRotKeys * RotationKeySize) + (TotalNumScaleKeys * ScaleKeySize));

		GlobalNumTransKeys += TotalNumTransKeys;
		GlobalNumRotKeys += TotalNumRotKeys;
		GlobalNumScaleKeys += TotalNumScaleKeys;

		Ar.Logf(TEXT(" %60s, %3i, %3i,%3i,%3i, %3i,%3i,%3i, %10i,%10i,%10i, %s, %d"),
			*Seq->GetName(),
			Seq->NumFrames,
			NumTransTracks, NumRotTracks, NumScaleTracks,
			NumTransTracksWithOneKey, NumRotTracksWithOneKey, NumScaleTracksWithOneKey,
			TotalNumTransKeys, TotalNumRotKeys, TotalNumScaleKeys,
			*FAnimationUtils::GetAnimationKeyFormatString(static_cast<AnimationKeyFormat>(Seq->KeyEncodingFormat)),
			(int32)Seq->GetResourceSize(EResourceSizeMode::Exclusive) );
	}
	Ar.Logf( TEXT("======================================================================") );
	Ar.Logf( TEXT("Total Num Tracks: %i trans, %i rot, %i scale, %i trans1, %i rot1, %i scale1"), GlobalNumTransTracks, GlobalNumRotTracks, GlobalNumScaleTracks, GlobalNumTransTracksWithOneKey, GlobalNumRotTracksWithOneKey, GlobalNumScaleTracksWithOneKey  );
	Ar.Logf( TEXT("Total Num Keys: %i trans, %i rot, %i scale"), GlobalNumTransKeys, GlobalNumRotKeys, GlobalNumScaleKeys );

	Ar.Logf( TEXT("Approx Compressed Memory: %i bytes"), GlobalApproxCompressedSize);
	Ar.Logf( TEXT("Approx Key Data Memory: %i bytes"), GlobalApproxKeyDataSize);
}
#endif

