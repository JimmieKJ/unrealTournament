// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	AnimationUE4.cpp: Animation runtime utilities
=============================================================================*/ 

#include "EnginePrivate.h"
#include "BlueprintUtilities.h"
#include "AnimEncoding.h"
#include "AnimationRuntime.h"
#include "AnimationUtils.h"
#include "Animation/AnimData/BoneMaskFilter.h"
#include "Animation/BlendSpaceBase.h"
#include "Animation/AnimCompositeBase.h"
#include "Animation/AnimInstance.h"

DEFINE_LOG_CATEGORY(LogAnimation);
DEFINE_LOG_CATEGORY(LogRootMotion);

/////////////////////////////////////////////////////////
// Templated Transform Blend Functionality

namespace ETransformBlendMode
{
	enum Type
	{
		Overwrite,
		Accumulate
	};
}

template<int32>
void BlendTransform(const FTransform& Source, FTransform& Dest, const float BlendWeight)
{
	check(false); /// should never call this
}

template<>
void BlendTransform<ETransformBlendMode::Overwrite>(const FTransform& Source, FTransform& Dest, const float BlendWeight)
{
	const ScalarRegister VBlendWeight(BlendWeight);
	Dest = Source * VBlendWeight;
}

template<>
void BlendTransform<ETransformBlendMode::Accumulate>(const FTransform& Source, FTransform& Dest, const float BlendWeight)
{
	const ScalarRegister VBlendWeight(BlendWeight);
	Dest.AccumulateWithShortestRotation(Source, VBlendWeight);
}

//////////////////////////////////////////////////////////////////////////

void FAnimationRuntime::NormalizeRotations(const FBoneContainer& RequiredBones, /*inout*/ FTransformArrayA2 & Atoms)
{
	check( Atoms.Num() == RequiredBones.GetNumBones() );
	const TArray<FBoneIndexType> & RequiredBoneIndices = RequiredBones.GetBoneIndicesArray();
	for (int32 j = 0; j < RequiredBoneIndices.Num(); ++j)
	{
		const int32 BoneIndex = RequiredBoneIndices[j];
		Atoms[BoneIndex].NormalizeRotation();
	}
}

void FAnimationRuntime::NormalizeRotations(FTransformArrayA2 & Atoms)
{
	for (int32 BoneIndex = 0; BoneIndex < Atoms.Num(); BoneIndex++)
	{
		Atoms[BoneIndex].NormalizeRotation();
	}
}

void FAnimationRuntime::InitializeTransform(const FBoneContainer& RequiredBones, /*inout*/ FTransformArrayA2 & Atoms)
{
	check( Atoms.Num() == RequiredBones.GetNumBones() );
	const TArray<FBoneIndexType> & RequiredBoneIndices = RequiredBones.GetBoneIndicesArray();
	for (int32 j = 0; j < RequiredBoneIndices.Num(); ++j)
	{
		const int32 BoneIndex = RequiredBoneIndices[j];
		Atoms[BoneIndex].SetIdentity();
	}
}

template <int32 TRANSFORM_BLEND_MODE>
FORCEINLINE void BlendPose(const FTransformArrayA2& SourcePoses, FTransformArrayA2& ResultAtoms, const TArray<FBoneIndexType> & RequiredBoneIndices, const float BlendWeight)
{
	for (int32 i = 0; i < RequiredBoneIndices.Num(); ++i)
	{
		const int32 BoneIndex = RequiredBoneIndices[i];
		BlendTransform<TRANSFORM_BLEND_MODE>(SourcePoses[BoneIndex], ResultAtoms[BoneIndex], BlendWeight);
	}
}

/**
 * Blends together a set of poses, each with a given weight.
 * This function is lightweight, it does not cull out nearly zero weights or check to make sure weights sum to 1.0, the caller should take care of that if needed.
 *
 * The blend is done by taking a weighted sum of each atom, and re-normalizing the quaternion part at the end, not using SLERP.
 * This allows n-way blends, and makes the code much faster, though the angular velocity will not be constant across the blend.
 *
 * @param	ResultAtoms		Output array of relative bone transforms.
 * @param	RequiredBones	Indices of bones that we want to return. Note that bones not in this array will not be modified, so are not safe to access! 
 *							This array must be in strictly increasing order.
 */
void FAnimationRuntime::BlendPosesTogether(int32 NumPoses, const FTransformArrayA2** SourcePoses, const float* SourceWeights, const FBoneContainer& RequiredBones, /*out*/ FTransformArrayA2& ResultAtoms)
{
	check(NumPoses > 0);
	// verification process so that it doesn't crash
	const TArray<FBoneIndexType> & RequiredBoneIndices = RequiredBones.GetBoneIndicesArray();
	if (ensure(SourcePoses && ResultAtoms.Num() >= RequiredBoneIndices.Num()))
	{
		// debug purpose for now, but this can cause 0 bone transform, so we'd like to catch it 
		float WeightSum = SourceWeights[0];
		BlendPose<ETransformBlendMode::Overwrite>(*SourcePoses[0], ResultAtoms, RequiredBoneIndices, SourceWeights[0]);

		for (int32 i = 1; i < NumPoses; ++i)
		{
			WeightSum += SourceWeights[i];

			BlendPose<ETransformBlendMode::Accumulate>(*SourcePoses[i], ResultAtoms, RequiredBoneIndices, SourceWeights[i]);
		}

		ensure (WeightSum != 0.f);

		// Ensure that all of the resulting rotations are normalized
		if( NumPoses > 1 )
		{
			FAnimationRuntime::NormalizeRotations(RequiredBones, ResultAtoms);
		}
	}
}

/**
 * Blends together a set of poses, each with a given weight.
 * This function is lightweight, it does not cull out nearly zero weights or check to make sure weights sum to 1.0, the caller should take care of that if needed.
 *
 * The blend is done by taking a weighted sum of each atom, and re-normalizing the quaternion part at the end, not using SLERP.
 * This allows n-way blends, and makes the code much faster, though the angular velocity will not be constant across the blend.
 *
 * @param	ResultAtoms		Output array of relative bone transforms.
 * @param	RequiredBones	Indices of bones that we want to return. Note that bones not in this array will not be modified, so are not safe to access! 
 *							This array must be in strictly increasing order.
 */
void FAnimationRuntime::BlendPosesTogether(int32 NumPoses, const TArray<FTransformArrayA2>& SourcePoses, const TArray<float>&	SourceWeights, const FBoneContainer& RequiredBones, /*out*/ FTransformArrayA2& ResultAtoms)
{
	check(NumPoses > 0);

	const TArray<FBoneIndexType> & RequiredBoneIndices = RequiredBones.GetBoneIndicesArray();

	BlendPose<ETransformBlendMode::Overwrite>(SourcePoses[0], ResultAtoms, RequiredBoneIndices, SourceWeights[0]);

	for (int32 i = 1; i < NumPoses; ++i)
	{
		BlendPose<ETransformBlendMode::Accumulate>(SourcePoses[i], ResultAtoms, RequiredBoneIndices, SourceWeights[i]);
	}

	// Ensure that all of the resulting rotations are normalized
	if( NumPoses > 1 )
	{
		FAnimationRuntime::NormalizeRotations(RequiredBones, ResultAtoms);
	}
}

template <int32 TRANSFORM_BLEND_MODE>
void BlendPosePerBone(const TArray<FBoneIndexType> &RequiredBoneIndices, const TArray<int32>& PerBoneIndices, const FBlendSampleData& BlendSampleDataCache, FTransformArrayA2 &ResultAtoms, const FTransformArrayA2& SourceAtoms)
{
	const float BlendWeight = BlendSampleDataCache.GetWeight();
	TArray<float> PerBoneBlends;
	for (int32 i = 0; i < BlendSampleDataCache.PerBoneBlendData.Num(); ++i)
	{
		PerBoneBlends.Add(FMath::Clamp<float>(BlendSampleDataCache.PerBoneBlendData[i], 0.f, 1.f));
	}

	for (int32 i = 0; i < RequiredBoneIndices.Num(); ++i)
	{
		const int32 BoneIndex = RequiredBoneIndices[i];
		int32 PerBoneIndex = PerBoneIndices[i];
		if (PerBoneIndex == INDEX_NONE || !BlendSampleDataCache.PerBoneBlendData.IsValidIndex(PerBoneIndex))
		{
			BlendTransform<TRANSFORM_BLEND_MODE>(SourceAtoms[BoneIndex], ResultAtoms[BoneIndex], BlendWeight);
		}
		else
		{
			BlendTransform<TRANSFORM_BLEND_MODE>(SourceAtoms[BoneIndex], ResultAtoms[BoneIndex], PerBoneBlends[PerBoneIndex]);
		}
	}
}

/**
 * Blends together a set of poses, each with a given weight.
 * This function is lightweight, it does not cull out nearly zero weights or check to make sure weights sum to 1.0, the caller should take care of that if needed.
 *
 * The blend is done by taking a weighted sum of each atom, and re-normalizing the quaternion part at the end, not using SLERP.
 * This allows n-way blends, and makes the code much faster, though the angular velocity will not be constant across the blend.
 *
 * @param	ResultAtoms		Output array of relative bone transforms.
 * @param	RequiredBones	Indices of bones that we want to return. Note that bones not in this array will not be modified, so are not safe to access! 
 *							This array must be in strictly increasing order.
 */
void FAnimationRuntime::BlendPosesTogetherPerBone(int32 NumPoses, const TArray<FTransformArrayA2>& SourcePoses, const UBlendSpaceBase* BlendSpace, const TArray<FBlendSampleData>& BlendSampleDataCache, const FBoneContainer& RequiredBones, /*out*/ FTransformArrayA2& ResultAtoms)
{
	check(NumPoses > 0);

	const TArray<FBoneIndexType> & RequiredBoneIndices = RequiredBones.GetBoneIndicesArray();

	TArray<int32> PerBoneIndices;
	PerBoneIndices.AddUninitialized(ResultAtoms.Num());
	for (int BoneIndex = 0; BoneIndex < PerBoneIndices.Num(); ++BoneIndex)
	{
		PerBoneIndices[BoneIndex] = BlendSpace->GetPerBoneInterpolationIndex(RequiredBoneIndices[BoneIndex], RequiredBones);
	}

	BlendPosePerBone<ETransformBlendMode::Overwrite>(RequiredBoneIndices, PerBoneIndices, BlendSampleDataCache[0], ResultAtoms, SourcePoses[0]);

	for (int32 i = 1; i < NumPoses; ++i)
	{
		BlendPosePerBone<ETransformBlendMode::Accumulate>(RequiredBoneIndices, PerBoneIndices, BlendSampleDataCache[i], ResultAtoms, SourcePoses[i]);
	}

	// Ensure that all of the resulting rotations are normalized
	FAnimationRuntime::NormalizeRotations(RequiredBones, ResultAtoms);
}

void FAnimationRuntime::BlendPosesTogetherPerBoneInMeshSpace(int32 NumPoses, TArray<FTransformArrayA2>& SourcePoses, const UBlendSpaceBase* BlendSpace, const TArray<FBlendSampleData>& BlendSampleDataCache, const FBoneContainer& RequiredBones, /*out*/ FTransformArrayA2& ResultAtoms)
{
	FQuat NewRotation;
	USkeleton* Skeleton = BlendSpace->GetSkeleton();
	
	const TArray<FBoneIndexType> & RequiredBoneIndices = RequiredBones.GetBoneIndicesArray();
	// all this is going to do is to convert SourcePoses.Rotation to be mesh space, and then once it goes through BlendPosesTogetherPerBone, convert back to local
	for (int32 Pose=0; Pose<NumPoses; ++Pose)
	{
		for (int32 I=0; I<RequiredBoneIndices.Num(); ++I)
		{
			int32 const & BoneIndex = RequiredBoneIndices[I];
			const int32 ParentIndex = RequiredBones.GetParentBoneIndex(BoneIndex);
			if ( ParentIndex!=INDEX_NONE )
			{
				NewRotation = SourcePoses[Pose][ParentIndex].GetRotation()*SourcePoses[Pose][BoneIndex].GetRotation();
				NewRotation.Normalize();
			}
			else
			{
				NewRotation = SourcePoses[Pose][BoneIndex].GetRotation();
			}

			// now copy back to SourcePoses
			SourcePoses[Pose][BoneIndex].SetRotation(NewRotation);
		}
	}

	// now we have mesh space rotation, call BlendPosesTogetherPerBone
	BlendPosesTogetherPerBone(NumPoses, SourcePoses, BlendSpace, BlendSampleDataCache, RequiredBones, ResultAtoms);

	// now result atoms has the output with mesh space roatation. Convert back to local space, start from back
	for (int32 I=RequiredBoneIndices.Num()-1; I>=0; --I)
	{
		int32 const & BoneIndex = RequiredBoneIndices[I];
		const int32 ParentIndex = RequiredBones.GetParentBoneIndex(BoneIndex);
		if ( ParentIndex!=INDEX_NONE )
		{
			FQuat LocalBlendQuat = ResultAtoms[ParentIndex].GetRotation().Inverse()*ResultAtoms[BoneIndex].GetRotation();
			ResultAtoms[BoneIndex].SetRotation(LocalBlendQuat);
			ResultAtoms[BoneIndex].NormalizeRotation();
		}
	}
}
/**
 * Accumulates BlendPoses to ResultAtoms with BlendWeight. 
 * ResultAtoms += BlendPose*BlendWeight.
 * Result is NOT Normalized, as this is expected to be called several times with each 'BlendPoses' transform array to be accumulated.
 * 
 * @param	BlendPoses		Array to BoneTransforms to scale and accumulate.
 * @param	BlendWeight		Weight to apply to 'BlendPoses'
 * @param	RequiredBones	Indices of bones to process, in strictly increasing order.
 * @param	ResultAtoms		Array of BoneTransforms to accumulate to.
 */
void FAnimationRuntime::BlendPosesAccumulate(const FTransformArrayA2& BlendPoses, const float BlendWeight, const FBoneContainer& RequiredBones, /*inout*/ FTransformArrayA2& ResultAtoms)
{
	BlendPose<ETransformBlendMode::Accumulate>(BlendPoses, ResultAtoms, RequiredBones.GetBoneIndicesArray(), BlendWeight);
}

void FAnimationRuntime::LerpBoneTransforms(TArray<FTransform> & A, const TArray<FTransform> & B, float Alpha, const TArray<FBoneIndexType> & RequiredBonesArray)
{
	if( Alpha >= (1.f - ZERO_ANIMWEIGHT_THRESH) )
	{
		A = B;
	}
	else if( Alpha > ZERO_ANIMWEIGHT_THRESH )
	{
		FTransform* ATransformData = A.GetData(); 
		const FTransform* BTransformData = B.GetData();
		const ScalarRegister VAlpha(Alpha);
		const ScalarRegister VOneMinusAlpha(1.f - Alpha);

		for (int32 Index=0; Index<RequiredBonesArray.Num(); Index++)
		{
			const int32& BoneIndex = RequiredBonesArray[Index];
			FTransform* TA = ATransformData + BoneIndex;
			const FTransform* TB = BTransformData + BoneIndex;

			*TA *= VOneMinusAlpha;
			TA->AccumulateWithShortestRotation(*TB, VAlpha);
			TA->NormalizeRotation();

// 			TA->BlendWith(*TB, Alpha);
		}
	}
}

void FAnimationRuntime::BlendAdditivePose(const FTransformArrayA2& SourcePoses, const FTransformArrayA2& AdditiveBlendPoses, const float BlendWeight, const FBoneContainer& RequiredBones, /*out*/ FTransformArrayA2& ResultAtoms)
{
	const ScalarRegister VBlendWeight(BlendWeight);
	// Subsequent poses need to be blended in
	const TArray<FBoneIndexType> & RequiredBoneIndices = RequiredBones.GetBoneIndicesArray();
	for (int32 j = 0; j < RequiredBoneIndices.Num(); ++j)
	{
		const int32 BoneIndex = RequiredBoneIndices[j];
		FTransform Additive = AdditiveBlendPoses[BoneIndex];
		ResultAtoms[BoneIndex] = SourcePoses[BoneIndex];
		FTransform::BlendFromIdentityAndAccumulate(ResultAtoms[BoneIndex], Additive, VBlendWeight);
	}

	// Ensure that all of the resulting rotations are normalized
	FAnimationRuntime::NormalizeRotations(RequiredBones, ResultAtoms);
}

void FAnimationRuntime::CombineWithAdditiveAnimations(int32 NumAdditivePoses, const FTransformArrayA2** SourceAdditivePoses, const float* SourceAdditiveWeights, const FBoneContainer& RequiredBones, /*inout*/ FTransformArrayA2& Atoms)
{
	const TArray<FBoneIndexType> & RequiredBoneIndices = RequiredBones.GetBoneIndicesArray();
	for (int32 PoseIndex = 0; PoseIndex < NumAdditivePoses; ++PoseIndex)
	{
		const ScalarRegister VBlendWeight(SourceAdditiveWeights[PoseIndex]);
		const FTransformArrayA2& SourceAtoms = *SourceAdditivePoses[PoseIndex];

		for (int32 j = 0; j < RequiredBoneIndices.Num(); ++j)
		{
			const int32 BoneIndex = RequiredBoneIndices[j];
			FTransform SourceAtom = SourceAtoms[BoneIndex];
			FTransform::BlendFromIdentityAndAccumulate(Atoms[BoneIndex], SourceAtom, VBlendWeight);
		}
	}
}

void FAnimationRuntime::GetPoseFromSequence(
	const UAnimSequenceBase* InSequence,
	const FBoneContainer& RequiredBones,
	/*out*/ FTransformArrayA2 & ResultAtoms,
	const FAnimExtractContext& ExtractionContext)
{
	//@TODO: ANIM: ANIMCOMPOSITE: Some of the code in here cares about UAnimSequence members and won't work on something derived from a different branch of UAnimSequenceBase
	const UAnimSequence* Sequence = Cast<const UAnimSequence>(InSequence);

	if( !Sequence )
	{
#if 0
		UE_LOG(LogAnimation, Log, TEXT("FAnimationRuntime::GetPoseFromSequence - %s - No animation data!"), *GetFName());
#endif
		FAnimationRuntime::FillWithRefPose(ResultAtoms, RequiredBones);
		return;
	}

	Sequence->GetAnimationPose(ResultAtoms, RequiredBones, ExtractionContext);

	// Check that all bone atoms coming from animation are normalized
#if DO_CHECK && WITH_EDITORONLY_DATA
	const TArray<FBoneIndexType> & RequiredBoneIndices = RequiredBones.GetBoneIndicesArray();
	for (int32 i = 0; i < RequiredBoneIndices.Num(); ++i)
	{
		const int32 BoneIndex = RequiredBoneIndices[i];
		check( ResultAtoms[BoneIndex].IsRotationNormalized() );
	}
#endif
}

void FAnimationRuntime::GetPoseFromAnimTrack(
	const FAnimTrack& Track,
	const FBoneContainer& RequiredBones,
	/*out*/ FTransformArrayA2& ResultAtoms, 
	const FAnimExtractContext& ExtractionContext)
{
	const int32 NumBones = RequiredBones.GetNumBones();

	TArray<FTransformArrayA2> SourcePoses;
	TArray<float> SourceWeights;
	float TotalWeight = 0.f;

	float CurrentTime = FMath::Clamp(ExtractionContext.CurrentTime, 0.f, Track.GetLength());

	// first get all the montage instance weight this slot node has
	for (int32 I=0; I<Track.AnimSegments.Num(); ++I)
	{
		const FAnimSegment& AnimSegment = Track.AnimSegments[I];

		float PositionInAnim = 0.f;
		float Weight = 0.f;
		UAnimSequenceBase* AnimRef = AnimSegment.GetAnimationData(CurrentTime, PositionInAnim, Weight);
		// make this to be 1 function
		if( AnimRef && (Weight > ZERO_ANIMWEIGHT_THRESH) )
		{
			// todo anim: hack - until we fix animcomposite
			UAnimSequence * Sequence = Cast<UAnimSequence>(AnimRef);
			if( Sequence )
			{
				const int32 NewIndex = SourceWeights.AddUninitialized(1);
				SourcePoses.AddZeroed(1);

				SourcePoses[NewIndex].AddUninitialized(NumBones);
				SourceWeights[NewIndex] = Weight;
				TotalWeight += Weight;

				// Copy passed in Extraction Context, but override position and looping parameters.
				FAnimExtractContext SequenceExtractionContext(ExtractionContext);
				SequenceExtractionContext.CurrentTime = PositionInAnim;
				SequenceExtractionContext.bExtractRootMotion &= Sequence->bEnableRootMotion;
				FAnimationRuntime::GetPoseFromSequence(Sequence, RequiredBones, SourcePoses[NewIndex], SequenceExtractionContext);
			}
		}
	}

	if( SourcePoses.Num() == 0 )
	{
		FAnimationRuntime::FillWithRefPose(ResultAtoms, RequiredBones);
	}
	else if( SourcePoses.Num() == 1 )
	{
		ResultAtoms = SourcePoses[0];
	}
	else
	{
		// If we have SourcePoses.Num() > 0, then we will have a non zero weight.
		check(TotalWeight >= ZERO_ANIMWEIGHT_THRESH);
		for (int32 I=0; I<SourceWeights.Num(); ++I)
		{
			// normalize I
			SourceWeights[I] /= TotalWeight;
		}
		FAnimationRuntime::BlendPosesTogether(SourcePoses.Num(), SourcePoses, SourceWeights, RequiredBones, ResultAtoms);
	}
}

void FAnimationRuntime::ConvertPoseToAdditive(FTransformArrayA2 & TargetPose, const FTransformArrayA2 & BasePose, const FBoneContainer& RequiredBones)
{
	const TArray<FBoneIndexType> & RequiredBoneIndices = RequiredBones.GetBoneIndicesArray();
	const int32 NumRequiredBones = RequiredBoneIndices.Num();

	for(int32 Index=0; Index<NumRequiredBones; Index++)
	{
		const int32 BoneIndex = RequiredBoneIndices[Index];

		FTransform& TargetTransform = TargetPose[BoneIndex];
		const FTransform& BaseTransform = BasePose[BoneIndex];

		TargetTransform.SetRotation( TargetTransform.GetRotation() * BaseTransform.GetRotation().Inverse() );
		TargetTransform.SetTranslation( TargetTransform.GetTranslation() - BaseTransform.GetTranslation() );
		TargetTransform.SetScale3D( TargetTransform.GetScale3D() * BaseTransform.GetSafeScaleReciprocal( BaseTransform.GetScale3D() ) );
		TargetTransform.NormalizeRotation();
	}
}

void FAnimationRuntime::ConvertPoseToMeshRotation(FTransformArrayA2 & LocalPose, const FBoneContainer& RequiredBones)
{
	const TArray<FBoneIndexType> & RequiredBoneIndices = RequiredBones.GetBoneIndicesArray();
	const int32 NumRequiredBones = RequiredBoneIndices.Num();

	// only the root bone doesn't have a parent. So skip it to save a branch in the iteration.
	check( RequiredBoneIndices[0] == 0 );

	// Convert all rotations to mesh space
	for (int32 Index=1; Index<NumRequiredBones; Index++)
	{
		const int32 BoneIndex = RequiredBoneIndices[Index];
		const int32 ParentIndex = RequiredBones.GetParentBoneIndex(BoneIndex);

		FTransform& LocalTransform = LocalPose[BoneIndex];
		const FTransform& LocalParentTransform = LocalPose[ParentIndex];

		LocalTransform.SetRotation( LocalParentTransform.GetRotation() * LocalTransform.GetRotation() );
	}
}


/** 
 * return ETypeAdvanceAnim type
 */
ETypeAdvanceAnim FAnimationRuntime::AdvanceTime(const bool & bAllowLooping, const float& MoveDelta, float& InOutTime, const float& EndTime)
{
	InOutTime += MoveDelta;

	if( InOutTime < 0.f || InOutTime > EndTime )
	{
		if( bAllowLooping )
		{
			if( EndTime != 0.f )
			{
				InOutTime	= FMath::Fmod(InOutTime, EndTime);
				// Fmod doesn't give result that falls into (0, EndTime), but one that falls into (-EndTime, EndTime). Negative values need to be handled in custom way
				if( InOutTime < 0.f )
				{
					InOutTime += EndTime;
				}
			}

			// it has been looped
			return ETAA_Looped;
		}
		else 
		{
			// If not, snap time to end of sequence and stop playing.
			InOutTime = FMath::Clamp(InOutTime, 0.f, EndTime);
			return ETAA_Finished;
		}
	}

	return ETAA_Default;
}

/** 
 * Scale transforms by Weight.
 * Result is obviously NOT normalized.
 */
void FAnimationRuntime::ApplyWeightToTransform(const FBoneContainer& RequiredBones, /*inout*/ FTransformArrayA2& Atoms, float Weight)
{
	const TArray<FBoneIndexType> & RequiredBoneIndices = RequiredBones.GetBoneIndicesArray();
	ScalarRegister MultWeight(Weight);
	for (int32 j = 0; j < RequiredBoneIndices.Num(); ++j)
	{
		const int32 BoneIndex = RequiredBoneIndices[j];
		Atoms[BoneIndex] *= MultWeight;
	}			
}

void FAnimationRuntime::GetPoseFromBlendSpace(
	UBlendSpaceBase * BlendSpace,
	TArray<FBlendSampleData>& BlendSampleDataCache,
	FBoneContainer & RequiredBones,
	/*out*/ FTransformArrayA2& ResultAtoms)
{
	// if not valid parameters
	if (BlendSpace == NULL || BlendSampleDataCache.Num() == 0)
	{
		FillWithRefPose(ResultAtoms, RequiredBones);
		return;
	}

	const int32 NumPoses = BlendSampleDataCache.Num();

	TArray<FTransformArrayA2> ChildrenTransform;
	ChildrenTransform.AddZeroed(NumPoses);

	TArray<float>				ChildrenWeights;
	ChildrenWeights.AddZeroed(NumPoses);

	const int32 NumBones = RequiredBones.GetNumBones();
	for (auto ChIter=ChildrenTransform.CreateIterator(); ChIter; ++ChIter)
	{
		(*ChIter).AddUninitialized(NumBones);
	}

	const TArray<struct FBlendSample> & SampleData = BlendSpace->GetBlendSamples();

	// get all child atoms we interested in
	for (int32 I=0; I<BlendSampleDataCache.Num(); ++I)
	{
		if ( SampleData.IsValidIndex(BlendSampleDataCache[I].SampleDataIndex) )
		{
			const FBlendSample& Sample = SampleData[BlendSampleDataCache[I].SampleDataIndex];
			ChildrenWeights[I] = BlendSampleDataCache[I].GetWeight();

			if (Sample.Animation)
			{
				const float Time = FMath::Clamp<float>(BlendSampleDataCache[I].Time, 0.f, Sample.Animation->SequenceLength);

				// first one always fills up the source one
				FAnimationRuntime::GetPoseFromSequence(Sample.Animation, RequiredBones, ChildrenTransform[I], FAnimExtractContext(Time, true));
			}
			else
			{
				FAnimationRuntime::FillWithRefPose(ChildrenTransform[I], RequiredBones);
			}
		}
		else
		{
			FAnimationRuntime::FillWithRefPose(ChildrenTransform[I], RequiredBones);
		}
	}

	if ( BlendSpace->PerBoneBlend.Num() > 0 )
	{
		if ( BlendSpace->IsValidAdditive() )
		{
			FAnimationRuntime::BlendPosesTogetherPerBone(NumPoses, ChildrenTransform, BlendSpace, BlendSampleDataCache, RequiredBones, ResultAtoms);
		}
		else
		{
			if ( BlendSpace->bRotationBlendInMeshSpace )
			{
				FAnimationRuntime::BlendPosesTogetherPerBoneInMeshSpace(NumPoses, ChildrenTransform, BlendSpace, BlendSampleDataCache, RequiredBones, ResultAtoms);
			}
			else
			{
				FAnimationRuntime::BlendPosesTogetherPerBone(NumPoses, ChildrenTransform, BlendSpace, BlendSampleDataCache, RequiredBones, ResultAtoms);
			}
		}
	}
	else
	{
		FAnimationRuntime::BlendPosesTogether(NumPoses, ChildrenTransform, ChildrenWeights, RequiredBones, ResultAtoms);
	}

	// Once all the accumulation and blending has been done, normalize rotations.
	FAnimationRuntime::NormalizeRotations(RequiredBones, ResultAtoms);
}


/* from % from OutKeyIndex1, meaning (CurrentKeyIndex(float)-OutKeyIndex1)/(OutKeyIndex2-OutKeyIndex1) */
void FAnimationRuntime::GetKeyIndicesFromTime(int32& OutKeyIndex1, int32& OutKeyIndex2, float& OutAlpha, const float Time, const int32 NumFrames, const float SequenceLength)
{
	// Check for 1-frame, before-first-frame and after-last-frame cases.
	if( Time <= 0.f || NumFrames == 1 )
	{
		OutKeyIndex1 = 0;
		OutKeyIndex2 = 0;
		OutAlpha = 0.f;
		return;
	}

	const int32 LastIndex		= NumFrames - 1;
	if( Time >= SequenceLength )
	{
		OutKeyIndex1 = LastIndex;
		OutKeyIndex2 = (OutKeyIndex1 + 1) % (NumFrames);
		OutAlpha = 0.f;
		return;
	}

	// This assumes that all keys are equally spaced (ie. won't work if we have dropped unimportant frames etc).
	const int32 NumKeys = NumFrames - 1;
	const float KeyPos = ((float)NumKeys * Time) / SequenceLength;

	// Find the integer part (ensuring within range) and that gives us the 'starting' key index.
	const int32 KeyIndex1 = FMath::Clamp<int32>( FMath::FloorToInt(KeyPos), 0, NumFrames-1 );  // @todo should be changed to FMath::TruncToInt

	// The alpha (fractional part) is then just the remainder.
	const float Alpha = KeyPos - (float)KeyIndex1;

	int32 KeyIndex2 = KeyIndex1 + 1;

	// If we have gone over the end, do different things in case of looping
	if( KeyIndex2 == NumFrames )
	{
		KeyIndex2 = KeyIndex1;
	}

	OutKeyIndex1 = KeyIndex1;
	OutKeyIndex2 = KeyIndex2;
	OutAlpha = Alpha;
}

void FAnimationRuntime::FillWithRefPose(TArray<FTransform> & OutAtoms, const FBoneContainer& RequiredBones)
{
	// Copy Target Asset's ref pose.
	OutAtoms = RequiredBones.GetRefPoseArray();

	// If retargeting is disabled, copy ref pose from Skeleton, rather than mesh.
	// this is only used in editor and for debugging.
	if( RequiredBones.GetDisableRetargeting() )
	{
		checkSlow( RequiredBones.IsValid() );
		// Only do this if we have a mesh. otherwise we're not retargeting animations.
		if( RequiredBones.GetSkeletalMeshAsset() )
		{
			TArray<int32> const & PoseToSkeletonBoneIndexArray = RequiredBones.GetPoseToSkeletonBoneIndexArray();
			TArray<FBoneIndexType> const & RequireBonesIndexArray = RequiredBones.GetBoneIndicesArray();
			TArray<FTransform> const & SkeletonRefPose = RequiredBones.GetSkeletonAsset()->GetRefLocalPoses();

			for (int32 ArrayIndex = 0; ArrayIndex<RequireBonesIndexArray.Num(); ArrayIndex++)
			{
				int32 const & PoseBoneIndex = RequireBonesIndexArray[ArrayIndex];
				int32 const & SkeletonBoneIndex = PoseToSkeletonBoneIndexArray[PoseBoneIndex];

				// Pose bone index should always exist in Skeleton
				checkSlow(SkeletonBoneIndex != INDEX_NONE);
				OutAtoms[PoseBoneIndex] = SkeletonRefPose[SkeletonBoneIndex];
			}
		}
	}
}

#if WITH_EDITOR
void FAnimationRuntime::FillWithRetargetBaseRefPose( TArray<FTransform> & OutAtoms, const USkeletalMesh* Mesh, const FBoneContainer& RequiredBones )
{
	// Copy Target Asset's ref pose.
	if (Mesh)
	{
		const int32 NumBones = RequiredBones.GetNumBones();
		const TArray<FBoneIndexType> & BoneIndices = RequiredBones.GetBoneIndicesArray();
		OutAtoms.Empty(NumBones);
		OutAtoms.AddUninitialized(NumBones);
		for (int32 Idx = 0; Idx < BoneIndices.Num(); ++Idx)
		{
			const int32 PoseIdx = BoneIndices[Idx];
			OutAtoms[PoseIdx] = Mesh->RetargetBasePose[PoseIdx];
		}
	}
	else
	{
		OutAtoms.Empty();
	}
}
#endif // WITH_EDITOR

void FAnimationRuntime::ConvertPoseToMeshSpace(const TArray<FTransform> & LocalTransforms, TArray<FTransform> & MeshSpaceTransforms, const FBoneContainer& RequiredBones)
{
	const int32 NumBones = RequiredBones.GetNumBones();

	// right now all this does is to convert to SpaceBases
	check( NumBones == LocalTransforms.Num() );
	check( NumBones == MeshSpaceTransforms.Num() );

	const FTransform* LocalTransformsData = LocalTransforms.GetData(); 
	FTransform* SpaceBasesData = MeshSpaceTransforms.GetData();
	const TArray<FBoneIndexType> & RequiredBoneIndexArray = RequiredBones.GetBoneIndicesArray();

	// First bone is always root bone, and it doesn't have a parent.
	{
		check( RequiredBoneIndexArray[0] == 0 );
		MeshSpaceTransforms[0] = LocalTransforms[0];
	}

	const int32 NumRequiredBones = RequiredBoneIndexArray.Num();
	for(int32 i=1; i<NumRequiredBones; i++)
	{
		const int32 BoneIndex = RequiredBoneIndexArray[i];
		FPlatformMisc::Prefetch(SpaceBasesData + BoneIndex);

		// For all bones below the root, final component-space transform is relative transform * component-space transform of parent.
		const int32 ParentIndex = RequiredBones.GetParentBoneIndex(BoneIndex);
		FPlatformMisc::Prefetch(SpaceBasesData + ParentIndex);

		FTransform::Multiply(SpaceBasesData + BoneIndex, LocalTransformsData + BoneIndex, SpaceBasesData + ParentIndex);

		checkSlow( MeshSpaceTransforms[BoneIndex].IsRotationNormalized() );
		checkSlow( !MeshSpaceTransforms[BoneIndex].ContainsNaN() );
	}
}

/** 
 *	Utility for taking an array of bone indices and ensuring that all parents are present 
 *	(ie. all bones between those in the array and the root are present). 
 *	Note that this must ensure the invariant that parent occur before children in BoneIndices.
 */
void FAnimationRuntime::EnsureParentsPresent(TArray<FBoneIndexType> & BoneIndices, USkeletalMesh * SkelMesh )
{
	const int32 NumBones = SkelMesh->RefSkeleton.GetNum();
	// Iterate through existing array.
	int32 i=0;
	while( i<BoneIndices.Num() )
	{
		const int32 BoneIndex = BoneIndices[i];

		// For the root bone, just move on.
		if( BoneIndex > 0 )
		{
#if	!(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			// Warn if we're getting bad data.
			// Bones are matched as int32, and a non found bone will be set to INDEX_NONE == -1
			// This should never happen, so if it does, something is wrong!
			if( BoneIndex >= NumBones )
			{
				UE_LOG(LogAnimation, Log, TEXT("FAnimationRuntime::EnsureParentsPresent, BoneIndex >= SkelMesh->RefSkeleton.GetNum()."));
				i++;
				continue;
			}
#endif
			const int32 ParentIndex = SkelMesh->RefSkeleton.GetParentIndex(BoneIndex);

			// If we do not have this parent in the array, we add it in this location, and leave 'i' where it is.
			// This can happen if somebody removes bones in the physics asset, then it will try add back in, and in the process, 
			// parent can be missing
			if( !BoneIndices.Contains(ParentIndex) )
			{
				BoneIndices.InsertUninitialized(i);
				BoneIndices[i] = ParentIndex;
			}
			// If parent was in array, just move on.
			else
			{
				i++;
			}
		}
		else
		{
			i++;
		}
	}
}

void FAnimationRuntime::ExcludeBonesWithNoParents(const TArray<int32> & BoneIndices, const FReferenceSkeleton& RefSkeleton, TArray<int32> & FilteredRequiredBones)
{
	// Filter list, we only want bones that have their parents present in this array.
	FilteredRequiredBones.Empty(BoneIndices.Num());

	for (int32 Index=0; Index<BoneIndices.Num(); Index++)
	{
		const int32& BoneIndex = BoneIndices[Index];
		// Always add root bone.
		if( BoneIndex == 0 )
		{
			FilteredRequiredBones.Add(BoneIndex);
		}
		else
		{
			const int32 ParentBoneIndex = RefSkeleton.GetParentIndex(BoneIndex);
			if( FilteredRequiredBones.Contains(ParentBoneIndex) )
			{
				FilteredRequiredBones.Add(BoneIndex);
			}
			else
			{
				UE_LOG(LogAnimation, Warning, TEXT("ExcludeBonesWithNoParents: Filtering out bone (%s) since parent (%s) is missing"), 
					*RefSkeleton.GetBoneName(BoneIndex).ToString(), *RefSkeleton.GetBoneName(ParentBoneIndex).ToString());
			}
		}
	}
}

void FAnimationRuntime::BlendMeshPosesPerBoneWeights( 
		struct FA2Pose& BasePose, 
		TArray<struct FA2Pose>& BlendPoses, 
		const TArray<FPerBoneBlendWeight> & BoneBlendWeights, 
		const FBoneContainer& RequiredBones, 
		USkeleton* Skeleton,
		/*out*/ FA2Pose& OutPose)
{
	check ( BasePose.Bones.Num() == BoneBlendWeights.Num() );

	float BlendWeight;

	TArray<FQuat> SourceRotations;
	TArray<FQuat> BlendRotations;
	TArray<FQuat> TargetRotations;
	SourceRotations.AddUninitialized(BasePose.Bones.Num());
	BlendRotations.AddUninitialized(BasePose.Bones.Num());
	TargetRotations.AddUninitialized(BasePose.Bones.Num());
	
	const TArray<FBoneIndexType> & RequiredBoneIndices = RequiredBones.GetBoneIndicesArray();
	for (int32 I=0; I<RequiredBoneIndices.Num(); ++I)
	{
		int32 const & BoneIndex = RequiredBoneIndices[I];
		int32 const & PoseIndex = BoneBlendWeights[BoneIndex].SourceIndex;
		const int32 ParentIndex = RequiredBones.GetParentBoneIndex(BoneIndex);

		FQuat SrcRotationInMesh;
		FQuat TargetRotationInMesh;

		if (ParentIndex!=INDEX_NONE)
		{
			SrcRotationInMesh = SourceRotations[ParentIndex]*BasePose.Bones[BoneIndex].GetRotation();
			TargetRotationInMesh = TargetRotations[ParentIndex]*BlendPoses[PoseIndex].Bones[BoneIndex].GetRotation();
		}
		else
		{
			SrcRotationInMesh = BasePose.Bones[BoneIndex].GetRotation();
			TargetRotationInMesh = BlendPoses[PoseIndex].Bones[BoneIndex].GetRotation();
		}
	
		// update mesh based rotations
		SourceRotations[BoneIndex] = SrcRotationInMesh;
		TargetRotations[BoneIndex] = TargetRotationInMesh;

		// now update outer
		FTransform BaseAtom = BasePose.Bones[BoneIndex];
		FTransform TargetAtom = BlendPoses[PoseIndex].Bones[BoneIndex];
		FTransform BlendAtom;

		BlendWeight = FMath::Clamp(BoneBlendWeights[BoneIndex].BlendWeight, 0.f, 1.f);
		if ( BlendWeight < ZERO_ANIMWEIGHT_THRESH )
		{
			BlendAtom = BaseAtom;
			BlendRotations[BoneIndex] = SourceRotations[BoneIndex];
		}
		else if ( (1.0 - BlendWeight) < ZERO_ANIMWEIGHT_THRESH )
		{
			BlendAtom = TargetAtom;
			BlendRotations[BoneIndex] = TargetRotations[BoneIndex];
		}
		else // we want blend here
		{
			BlendAtom = BaseAtom;
			BlendAtom.BlendWith(TargetAtom, BlendWeight);

			// blend rotation in mesh space
			BlendRotations[BoneIndex] = FQuat::FastLerp(SourceRotations[BoneIndex], TargetRotations[BoneIndex], BlendWeight);
			
			// Fast lerp produces un-normalized quaternions, re-normalize.
			BlendRotations[BoneIndex].Normalize();
		}

		OutPose.Bones[BoneIndex] = BlendAtom;
		if (ParentIndex!=INDEX_NONE)
		{
			FQuat LocalBlendQuat = BlendRotations[ParentIndex].Inverse() * BlendRotations[BoneIndex];
			
			// local -> mesh -> local transformations can cause loss of precision for long bone chains, we have to normalize rotation there.
			LocalBlendQuat.Normalize();
			OutPose.Bones[BoneIndex].SetRotation(LocalBlendQuat);
		}
	}
}

void FAnimationRuntime::BlendLocalPosesPerBoneWeights( 
	FA2Pose& BasePose, 
	TArray<FA2Pose>& BlendPoses, 
	const TArray<FPerBoneBlendWeight> & BoneBlendWeights, 
	const FBoneContainer& RequiredBones, 
	/*out*/ FA2Pose& OutPose)
{
	check ( BasePose.Bones.Num() == BoneBlendWeights.Num() );

	float BlendWeight;

	const TArray<FBoneIndexType> & RequiredBoneIndices = RequiredBones.GetBoneIndicesArray();
	for (int32 I=0; I<RequiredBoneIndices.Num(); ++I)
	{
		int32 const & BoneIndex = RequiredBoneIndices[I];
		int32 const & PoseIndex = BoneBlendWeights[BoneIndex].SourceIndex;
		FTransform BaseAtom = GetSpaceTransform(BasePose, BoneIndex);

		BlendWeight = FMath::Clamp(BoneBlendWeights[BoneIndex].BlendWeight, 0.f, 1.f);
		if ( BlendWeight < ZERO_ANIMWEIGHT_THRESH )
		{
			SetSpaceTransform(OutPose, BoneIndex, BaseAtom);
		}
		else if ( (1.0 - BlendWeight) < ZERO_ANIMWEIGHT_THRESH )
		{
			FTransform BlendAtom = GetSpaceTransform(BlendPoses[PoseIndex], BoneIndex);
			SetSpaceTransform(OutPose, BoneIndex, BlendAtom);
		}
		else // we want blend here
		{
			FTransform BlendAtom = GetSpaceTransform(BasePose, BoneIndex);
			FTransform TargetAtom = GetSpaceTransform(BlendPoses[PoseIndex], BoneIndex);
			BlendAtom.BlendWith(TargetAtom, BlendWeight);
			SetSpaceTransform(OutPose, BoneIndex, BlendAtom);
		}
	}
}

void FAnimationRuntime::UpdateDesiredBoneWeight(const TArray<FPerBoneBlendWeight>& SrcBoneBlendWeights, TArray<FPerBoneBlendWeight>& TargetBoneBlendWeights, const TArray<float>& BlendWeights, const FBoneContainer& RequiredBones, USkeleton* Skeleton)
{
	// in the future, cache this outside
	ensure (TargetBoneBlendWeights.Num() == SrcBoneBlendWeights.Num());

	FMemory::Memset(TargetBoneBlendWeights.GetData(), 0, TargetBoneBlendWeights.Num() * sizeof(FPerBoneBlendWeight));

	// go through skeleton tree requiredboneindices
	const TArray<FBoneIndexType> & RequiredBoneIndices = RequiredBones.GetBoneIndicesArray();
	for (int32 I=0; I<RequiredBoneIndices.Num(); ++I)
	{
		int32 BoneIndex = RequiredBoneIndices[I];
		int32 PoseIndex = SrcBoneBlendWeights[BoneIndex].SourceIndex;
		float TargetBlendWeight = BlendWeights[PoseIndex]*SrcBoneBlendWeights[BoneIndex].BlendWeight;
		// if relevant, otherwise all initialized as zero
		if (TargetBlendWeight > ZERO_ANIMWEIGHT_THRESH)
		{
			TargetBoneBlendWeights[BoneIndex].SourceIndex = PoseIndex;
			TargetBoneBlendWeights[BoneIndex].BlendWeight = TargetBlendWeight;
		}
	}
}

void FAnimationRuntime::BlendPosesPerBoneFilter(struct FA2Pose& BasePose, const TArray<struct FA2Pose>& BlendPoses, struct FA2Pose& OutPose, TArray<FPerBoneBlendWeight>& BoneBlendWeights, bool bMeshSpaceRotationBlending, const FBoneContainer& RequiredBones, USkeleton* Skeleton)
{
	ensure (OutPose.Bones.Num() == BasePose.Bones.Num());
	if (BlendPoses.Num() != 0)
	{
		TArray<FA2Pose>& NewBlendPoses = (TArray<FA2Pose>&)BlendPoses;
		if (bMeshSpaceRotationBlending)
		{
			BlendMeshPosesPerBoneWeights(BasePose, NewBlendPoses, BoneBlendWeights, RequiredBones, Skeleton, OutPose);
		}
		else
		{
			BlendLocalPosesPerBoneWeights(BasePose, NewBlendPoses, BoneBlendWeights, RequiredBones, OutPose);
		}
	}
	else // if no blendpose, outpose = basepose
	{
		OutPose = BasePose;
	}
}

void FAnimationRuntime::CreateMaskWeights(int32 NumOfBones, TArray<FPerBoneBlendWeight> & BoneBlendWeights, const TArray<FInputBlendPose>	&BlendFilters, const FBoneContainer& RequiredBones, const USkeleton* Skeleton)
{
	if ( Skeleton )
	{
		BoneBlendWeights.Empty(NumOfBones);
		BoneBlendWeights.AddZeroed(NumOfBones);

		const TArray<FBoneIndexType> & RequiredBoneIndices = RequiredBones.GetBoneIndicesArray();
		// base mask bone
		for (int32 PoseIndex=0; PoseIndex<BlendFilters.Num(); ++PoseIndex)
		{
			const FInputBlendPose& BlendPose = BlendFilters[PoseIndex];

			for (int32 BranchIndex=0; BranchIndex<BlendPose.BranchFilters.Num(); ++BranchIndex)
			{
				const FBranchFilter& BranchFilter = BlendPose.BranchFilters[BranchIndex];
				int32 MaskBoneIndex = RequiredBones.GetPoseBoneIndexForBoneName(BranchFilter.BoneName);

				// how much weight increase Per depth
				float MaxWeight = (BranchFilter.BlendDepth > 0) ? 1.f : -1.f;
				float IncreaseWeightPerDepth = (BranchFilter.BlendDepth != 0) ? (1.f/((float)BranchFilter.BlendDepth)) : 1.f;
	
				// go through skeleton tree requiredboneindices
				for (int32 I=0; I<RequiredBoneIndices.Num(); ++I)
				{
					int32 BoneIndex = RequiredBoneIndices[I];
					int32 Depth = RequiredBones.GetDepthBetweenBones(BoneIndex, MaskBoneIndex);

					// if Depth == -1, it's not a child
					if( Depth != -1 )
					{
						// when you write to buffer, you'll need to match with BasePoses BoneIndex
						FPerBoneBlendWeight& BoneBlendWeight = BoneBlendWeights[BoneIndex];

						BoneBlendWeight.SourceIndex = PoseIndex;
						float BlendIncrease = IncreaseWeightPerDepth * (float)(Depth + 1);
						BoneBlendWeight.BlendWeight = FMath::Clamp<float>(BoneBlendWeight.BlendWeight + BlendIncrease, 0.f, 1.f);
					}
				}
			}
		}
	}
}

/** Convert a ComponentSpace FTransform to given BoneSpace. */
void FAnimationRuntime::ConvertCSTransformToBoneSpace
(
	USkeletalMeshComponent * SkelComp,  
	FA2CSPose & MeshBases, 
	/**inout*/ FTransform& CSBoneTM, 
	int32 BoneIndex, 
	uint8 Space
)
{
	switch( Space )
	{
		case BCS_WorldSpace : 
			// world space, so component space * component to world
			CSBoneTM *= SkelComp->ComponentToWorld;
			break;

		case BCS_ComponentSpace :
			// Component Space, no change.
			break;

		case BCS_ParentBoneSpace :
			{
				const int32 ParentIndex = MeshBases.GetParentBoneIndex(BoneIndex);
				if (ParentIndex != INDEX_NONE)
				{
					const FTransform ParentTM = MeshBases.GetComponentSpaceTransform(ParentIndex);
					CSBoneTM.SetToRelativeTransform(ParentTM);
				}
			}
			break;

		case BCS_BoneSpace :
			{
				const FTransform BoneTM = MeshBases.GetComponentSpaceTransform(BoneIndex);
				CSBoneTM.SetToRelativeTransform(BoneTM);
			}
			break;

		default:
			UE_LOG(LogAnimation, Warning, TEXT("ConvertCSTransformToBoneSpace: Unknown BoneSpace %d  for Mesh: %s"), Space, *GetNameSafe(SkelComp->SkeletalMesh));
			break;
	}
}

/** Convert a BoneSpace FTransform to ComponentSpace. */
void FAnimationRuntime::ConvertBoneSpaceTransformToCS
(
	USkeletalMeshComponent * SkelComp,  
	FA2CSPose & MeshBases, 
	/*inout*/ FTransform& BoneSpaceTM, 
	int32 BoneIndex, 
	uint8 Space
)
{
	switch( Space )
	{
		case BCS_WorldSpace : 
			BoneSpaceTM.SetToRelativeTransform(SkelComp->ComponentToWorld);
			break;

		case BCS_ComponentSpace :
			// Component Space, no change.
			break;

		case BCS_ParentBoneSpace :
			if( BoneIndex != INDEX_NONE )
			{
				const int32 ParentIndex = MeshBases.GetParentBoneIndex(BoneIndex);
				if( ParentIndex != INDEX_NONE )
				{
					const FTransform ParentTM = MeshBases.GetComponentSpaceTransform(ParentIndex);
					BoneSpaceTM *= ParentTM;
				}
			}
			break;

		case BCS_BoneSpace :
			if( BoneIndex != INDEX_NONE )
			{
				const FTransform BoneTM = MeshBases.GetComponentSpaceTransform(BoneIndex);
				BoneSpaceTM *= BoneTM;
			}
			break;

		default:
			UE_LOG(LogAnimation, Warning, TEXT("ConvertBoneSpaceTransformToCS: Unknown BoneSpace %d  for Mesh: %s"), Space, *GetNameSafe(SkelComp->SkeletalMesh));
			break;
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
/// Pose conversion functions
////////////////////////////////////////////////////////////////////////////////////////////////////

FTransform FAnimationRuntime::GetSpaceTransform(FA2Pose& Pose, int32 Index)
{
	return Pose.Bones[Index];
}

FTransform FAnimationRuntime::GetSpaceTransform(FA2CSPose& Pose, int32 Index)
{
	return Pose.GetComponentSpaceTransform(Index);
}

void FAnimationRuntime::SetSpaceTransform(FA2Pose& Pose, int32 Index, FTransform& NewTransform)
{
	Pose.Bones[Index] = NewTransform;
}

void FAnimationRuntime::SetSpaceTransform(FA2CSPose& Pose, int32 Index, FTransform& NewTransform)
{
	Pose.SetComponentSpaceTransform(Index, NewTransform);
}

void FAnimationRuntime::TickBlendWeight(float DeltaTime, float DesiredWeight, float& Weight, float& BlendTime)
{
	// if it's not same, we'll need to update weight
	if (DesiredWeight != Weight)
	{
		if (BlendTime == 0.f)
		{
			// no blending, just go
			Weight = DesiredWeight;
		}
		else
		{
			float WeightChangePerTime = (DesiredWeight-Weight)/BlendTime;
			Weight += WeightChangePerTime*DeltaTime;

			// going up or down, changes where to clamp to 
			if (WeightChangePerTime > 0.f)
			{
				Weight = FMath::Clamp<float>(Weight, 0.f, DesiredWeight);
			}
			else // if going down
			{
				Weight = FMath::Clamp<float>(Weight, DesiredWeight, 1.f);
			}

			BlendTime-=DeltaTime;
		}
	}
}

#if DO_GUARD_SLOW
// use checkSlow to use this function for debugging
bool FAnimationRuntime::ContainsNaN(TArray<FBoneIndexType> & RequiredBoneIndices, FA2Pose & Pose) 
{
	for (int32 Iter = 0; Iter < RequiredBoneIndices.Num(); ++Iter)
	{
		const int32 BoneIndex = RequiredBoneIndices[Iter];
		if (Pose.Bones[BoneIndex].ContainsNaN())
		{
			return true;
		}
	}

	return false;
}
#endif

#if WITH_EDITOR
void FAnimationRuntime::FillUpSpaceBasesRefPose(const USkeleton* Skeleton, TArray<FTransform> &SpaceBaseRefPose)
{
	check(Skeleton);

	const TArray<FTransform> & ReferencePose = Skeleton->GetReferenceSkeleton().GetRefBonePose();
	SpaceBaseRefPose.Empty(ReferencePose.Num());
	SpaceBaseRefPose.AddUninitialized(ReferencePose.Num());

	// initialize to identity since some of them don't have tracks
	for(int Index=0; Index <SpaceBaseRefPose.Num(); ++Index)
	{
		int32 ParentIndex = Skeleton->GetReferenceSkeleton().GetParentIndex(Index);
		if(ParentIndex != INDEX_NONE)
		{
			SpaceBaseRefPose[Index] = ReferencePose[Index] * SpaceBaseRefPose[ParentIndex];
		}
		else
		{
			SpaceBaseRefPose[Index] = ReferencePose[Index];
		}
	}
}

void FAnimationRuntime::FillUpSpaceBasesRetargetBasePose(const USkeleton* Skeleton, TArray<FTransform> &SpaceBaseRefPose)
{
	check(Skeleton);

	// @Todo fixme: this has to get preview mesh instead of skeleton
	
	const USkeletalMesh* PreviewMesh = Skeleton->GetPreviewMesh();
	if (PreviewMesh)
	{
		const TArray<FTransform> & ReferencePose = PreviewMesh->RetargetBasePose;
		SpaceBaseRefPose.Empty(ReferencePose.Num());
		SpaceBaseRefPose.AddUninitialized(ReferencePose.Num());

		// initialize to identity since some of them don't have tracks
		for(int Index=0; Index <SpaceBaseRefPose.Num(); ++Index)
		{
			int32 ParentIndex = PreviewMesh->RefSkeleton.GetParentIndex(Index);
			if(ParentIndex != INDEX_NONE)
			{
				SpaceBaseRefPose[Index] = ReferencePose[Index] * SpaceBaseRefPose[ParentIndex];
			}
			else
			{
				SpaceBaseRefPose[Index] = ReferencePose[Index];
			}
		}
	}
	else
	{
		FAnimationRuntime::FillUpSpaceBasesRefPose(Skeleton, SpaceBaseRefPose);
	}
}
#endif // WITH_EDITOR

/////////////////////////////////////////////////////////////////////////////////////////
// FA2CSPose
/////////////////////////////////////////////////////////////////////////////////////////

/** constructor - needs LocalPoses **/
void FA2CSPose::AllocateLocalPoses(const FBoneContainer& InBoneContainer, const FA2Pose & LocalPose)
{
	AllocateLocalPoses(InBoneContainer, LocalPose.Bones);
}

void FA2CSPose::AllocateLocalPoses(const FBoneContainer& InBoneContainer, const FTransformArrayA2 & LocalBones)
{
	check( InBoneContainer.IsValid() );
	BoneContainer = &InBoneContainer;

	Bones = LocalBones;
	ComponentSpaceFlags.Init(0, Bones.Num());

	// root is same, so set root first
	check(ComponentSpaceFlags.Num() > 0);
	ComponentSpaceFlags[0] = 1;
}

bool FA2CSPose::IsValid() const
{
	return (BoneContainer && BoneContainer->IsValid());
}

int32 FA2CSPose::GetParentBoneIndex(const int32& BoneIndex) const
{
	checkSlow( IsValid() );
	return BoneContainer->GetParentBoneIndex(BoneIndex);
}

/** Do not access Bones array directly but via this 
 * This will fill up gradually mesh space bases 
 */
FTransform FA2CSPose::GetComponentSpaceTransform(int32 BoneIndex)
{
	check(Bones.IsValidIndex(BoneIndex));

	// if not evaluate, calculate it
	if( ComponentSpaceFlags[BoneIndex] == 0 )
	{
		CalculateComponentSpaceTransform(BoneIndex);
	}

	return Bones[BoneIndex];
}

void FA2CSPose::SetComponentSpaceTransform(int32 BoneIndex, const FTransform& NewTransform)
{
	check (Bones.IsValidIndex(BoneIndex));

	// this one forcefully sets component space transform
	Bones[BoneIndex] = NewTransform;
	ComponentSpaceFlags[BoneIndex] = 1;
}

/**
 * Convert Bone to Local Space.
 */
void FA2CSPose::ConvertBoneToLocalSpace(int32 BoneIndex)
{
	checkSlow( IsValid() );

	// If BoneTransform is in Component Space, then convert it.
	// Never convert Root to Local Space.
	if( BoneIndex > 0 && ComponentSpaceFlags[BoneIndex] == 1 )
	{
		const int32 ParentIndex = BoneContainer->GetParentBoneIndex(BoneIndex);

		// Verify that our Parent is also in Component Space. That should always be the case.
		check( ComponentSpaceFlags[ParentIndex] == 1 );

		// Convert to local space.
		Bones[BoneIndex].SetToRelativeTransform( Bones[ParentIndex] );
		ComponentSpaceFlags[BoneIndex] = 0;
	}
}

/** 
 * Do not access Bones array directly but via this 
 * This will fill up gradually mesh space bases 
 */
FTransform FA2CSPose::GetLocalSpaceTransform(int32 BoneIndex)
{
	check( Bones.IsValidIndex(BoneIndex) );
	checkSlow( IsValid() );

	// if evaluated, calculate it
	if( ComponentSpaceFlags[BoneIndex] )
	{
		const int32 ParentIndex = BoneContainer->GetParentBoneIndex(BoneIndex);
		if (ParentIndex != INDEX_NONE)
		{
			const FTransform ParentTransform = GetComponentSpaceTransform(ParentIndex);
			const FTransform& BoneTransform = Bones[BoneIndex];
			// calculate local space
			return BoneTransform.GetRelativeTransform(ParentTransform);
		}
	}

	return Bones[BoneIndex];
}

void FA2CSPose::SetLocalSpaceTransform(int32 BoneIndex, const FTransform& NewTransform)
{
	check (Bones.IsValidIndex(BoneIndex));

	// this one forcefully sets component space transform
	Bones[BoneIndex] = NewTransform;
	ComponentSpaceFlags[BoneIndex] = 0;
}

/** Calculate all transform till parent **/
void FA2CSPose::CalculateComponentSpaceTransform(int32 BoneIndex)
{
	check( ComponentSpaceFlags[BoneIndex] == 0 );
	checkSlow( IsValid() );

	// root is already verified, so root should not come here
	// check AllocateLocalPoses
	const int32 ParentIndex = BoneContainer->GetParentBoneIndex(BoneIndex);

	// if Parent already has been calculated, use it
	if( ComponentSpaceFlags[ParentIndex] == 0 )
	{
		// if Parent hasn't been calculated, also calculate parents
		CalculateComponentSpaceTransform(ParentIndex);
	}

	// current Bones(Index) should contain LocalPoses.
	Bones[BoneIndex] = Bones[BoneIndex] * Bones[ParentIndex];
	Bones[BoneIndex].NormalizeRotation();
	ComponentSpaceFlags[BoneIndex] = 1;
}

void FA2CSPose::ConvertToLocalPoses(FA2Pose & LocalPoses)  const
{
	checkSlow(IsValid());
	LocalPoses.Bones = Bones;

	// now we need to convert back to local bases
	// only convert back that has been converted to mesh base
	// if it was local base, and if it hasn't been modified
	// that is still okay even if parent is changed, 
	// that doesn't mean this local has to change
	// go from child to parent since I need parent inverse to go back to local
	// root is same, so no need to do Index == 0
	for(int32 BoneIndex=ComponentSpaceFlags.Num()-1; BoneIndex>0; --BoneIndex)
	{
		// root is already verified, so root should not come here
		// check AllocateLocalPoses
		const int32 ParentIndex = BoneContainer->GetParentBoneIndex(BoneIndex);

		// convert back 
		if( ComponentSpaceFlags[BoneIndex] )
		{
			LocalPoses.Bones[BoneIndex].SetToRelativeTransform( LocalPoses.Bones[ParentIndex] );
			LocalPoses.Bones[BoneIndex].NormalizeRotation();
		}
	}
}

/** 
 * Set a bunch of Component Space Bone Transforms.
 * Do this safely by insuring that Parents are already in Component Space,
 * and any Component Space children are converted back to Local Space before hand.
 */
void FA2CSPose::SafeSetCSBoneTransforms(const TArray<struct FBoneTransform> & BoneTransforms)
{
	checkSlow( IsValid() );

	// Bone Mask to keep track of which bones have to be converted to local space.
	// This is basically BoneTransforms's children.
	TArray<uint8> BoneMask;
	BoneMask.AddZeroed( Bones.Num() );

	// First build our BoneMask
	for (int32 Index=0; Index<BoneTransforms.Num(); Index++)	
	{
		const int32 BoneIndex = BoneTransforms[Index].BoneIndex;
		// Mark those bones in Mesh Pose as being required to be in Local Space.
		BoneMask[BoneIndex] = 1;
	}

	// First Flag children
	const TArray<FBoneIndexType> & RequiredBoneIndices = BoneContainer->GetBoneIndicesArray();
	for(int32 Index=0; Index<RequiredBoneIndices.Num(); ++Index)
	{
		const int32 BoneIndex = RequiredBoneIndices[Index];
		const int32 ParentIndex = BoneContainer->GetParentBoneIndex(BoneIndex);
		// Propagate our BoneMask to children.
		if( ParentIndex != INDEX_NONE )
		{
			BoneMask[BoneIndex] |= BoneMask[ParentIndex];
		}
	}

	// now iterate from children to parent to calculate back to local space
	// Sadly this has to iterate from back because you'll need parent to be component space
	// if you do this from parent to child, you'll see parent turned to local before children applies it
	int32 BoneTransformIndex = BoneTransforms.Num() - 1;
	for(int32 Index=RequiredBoneIndices.Num()-1; Index>=0; --Index)
	{
		const int32 BoneIndex = RequiredBoneIndices[Index];
		// If this bone has to be converted to Local Space...
		if( BoneMask[BoneIndex] != 0 )
		{
			// If this is one of the original BoneTransforms list bones
			// Then we don't actually want to convert this one to local space, since we're going to overwrite it.
			// So skip it.
			if( BoneTransformIndex >=0 && BoneIndex == BoneTransforms[BoneTransformIndex].BoneIndex )
			{
				BoneTransformIndex--;
			}
			// If this is a children bone then we want it to be in local space!
			else
			{
				// .. If it is not currently in Local Space, then convert it.
				ConvertBoneToLocalSpace(BoneIndex);
			}
		}
	}

	// Finally copy our Component Space transforms
	for (int32 Index=0; Index<BoneTransforms.Num(); Index++)	
	{
		const int32 BoneIndex = BoneTransforms[Index].BoneIndex;
		
		// Make sure our BoneTransforms were in Component Space in the first place, before we overwrite them
		// Only check their parent to do minimal work needed.
		const int32 ParentBoneIndex = BoneContainer->GetParentBoneIndex(BoneIndex);
		if( ParentBoneIndex != INDEX_NONE && ComponentSpaceFlags[ParentBoneIndex] == 0 )
		{
			CalculateComponentSpaceTransform(ParentBoneIndex);
		}

		// Set new Component Space transform.
		SetComponentSpaceTransform( BoneIndex, BoneTransforms[Index].Transform );
	}
}

/** 
 * Blends Component Space transforms to MeshPose in Local Space. 
 * Used by SkelControls to apply their transforms.
 *
 * The tricky bit is that SkelControls deliver their transforms in Component Space,
 * But the blending is done in Local Space. Also we need to refresh any Children they have
 * that has been previously converted to Component Space.
 */
void FA2CSPose::LocalBlendCSBoneTransforms
(
	const TArray<struct FBoneTransform>& BoneTransforms,
	float Alpha
)
{
	// if Alpha is small enough, skip
	if( Alpha < ZERO_ANIMWEIGHT_THRESH )
	{
		return;
	}

#if DO_CHECK
	{
		int32 LastIndex = INDEX_NONE;
		// Make sure bones are sorted in "Parents before Children" order.
		for (int32 I=0; I<BoneTransforms.Num(); ++I)
		{
			check(BoneTransforms[I].BoneIndex != INDEX_NONE);
			check(BoneTransforms[I].BoneIndex >= LastIndex);
			LastIndex = BoneTransforms[I].BoneIndex;
		}
	}
#endif

	// If we are not doing any blending, do a faster path.
	// Set transforms directly in Component Space. But still refresh children.
	if( Alpha >= 1.f - ZERO_ANIMWEIGHT_THRESH )
	{
		SafeSetCSBoneTransforms(BoneTransforms);
	}
	// Slower blending path.
	else
	{
		// Bone Mask to keep track of which bones have to be converted to local space.
		// This is basically BoneTransforms bones and their children.
		TArray<uint8> BoneMask;
		BoneMask.AddZeroed( Bones.Num() );

		TArray<struct FBoneTransform> LocalBoneTransforms;
		LocalBoneTransforms.SetNumUninitialized( BoneTransforms.Num() );

		const TArray<FBoneIndexType> & RequiredBoneIndices = BoneContainer->GetBoneIndicesArray();
		// First, convert BoneTransforms to local space for blending.
		for (int32 Index=0; Index<BoneTransforms.Num(); Index++)	
		{
			const int32 BoneIndex = BoneTransforms[Index].BoneIndex;
			const int32 ParentIndex = BoneContainer->GetParentBoneIndex(BoneIndex);
			if( ParentIndex != INDEX_NONE )
			{
				// if BoneTransforms(modified by controllers) contains ParentIndex, it should use that as ParentTransform, not the one from input
				int32 LocalParentIndex = INDEX_NONE;
				for ( int32 LocalIndex = 0; LocalIndex<BoneTransforms.Num(); ++LocalIndex )
				{
					if (ParentIndex == BoneTransforms[LocalIndex].BoneIndex)
					{
						LocalParentIndex = LocalIndex;
						break;
					}
				}

				// saves Parent Transform
				FTransform ParentTransform;
				if ( LocalParentIndex == INDEX_NONE )
				{
					ParentTransform = GetComponentSpaceTransform(ParentIndex);
				}
				else
				{
					ParentTransform = BoneTransforms[LocalParentIndex].Transform;
				}

				LocalBoneTransforms[Index].Transform = BoneTransforms[Index].Transform.GetRelativeTransform(ParentTransform);
				LocalBoneTransforms[Index].BoneIndex = BoneIndex;

				// Mark those bones in Mesh Pose as being required to be in Local Space.
				BoneMask[BoneIndex] = 1;
			}
			else
			{
				// when root is entered as to modify, we don't need to adjust parent index, just clear it
				LocalBoneTransforms[Index].Transform =  BoneTransforms[Index].Transform;
				LocalBoneTransforms[Index].BoneIndex = BoneIndex;

				BoneMask[BoneIndex] = 1;
			}
		}

		// Then, convert MeshPose Bones from BoneTransforms list, and their children, to local space if they are not already.
		for(int32 Index=0; Index<RequiredBoneIndices.Num(); ++Index)
		{
			const int32 BoneIndex = RequiredBoneIndices[Index];
			const int32 ParentIndex = BoneContainer->GetParentBoneIndex(BoneIndex);
			// Propagate our BoneMask to children.
			if( ParentIndex != INDEX_NONE )
			{
				BoneMask[BoneIndex] |= BoneMask[ParentIndex];
			}
		}

		for(int32 Index=RequiredBoneIndices.Num()-1; Index>=0; --Index)		
		{
			const int32 BoneIndex = RequiredBoneIndices[Index];
			// If this bone has to be converted to Local Space...
			if( BoneMask[BoneIndex] != 0 )
			{
				// .. And is not currently in Local Space, then convert it.
				ConvertBoneToLocalSpace(BoneIndex);
			}
		}

		// Lastly, do the blending in local space.
		for (int32 Index=0; Index<LocalBoneTransforms.Num(); Index++)	
		{
			const int32 BoneIndex = LocalBoneTransforms[Index].BoneIndex;
			// Make sure this transform is in local space, because we are writing a local space one to it.
			// If we are not in local space, this could mean trouble for our children.
			check( (ComponentSpaceFlags[BoneIndex] == 0) || (BoneIndex == 0) );

			// No need to normalize rotation since BlendWith() does it.
			Bones[BoneIndex].BlendWith(LocalBoneTransforms[Index].Transform, Alpha);
		}
	}
}

