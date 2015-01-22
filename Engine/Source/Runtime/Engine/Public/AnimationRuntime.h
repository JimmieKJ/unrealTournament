// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	AnimationRuntime.h: Skeletal mesh animation utilities
	Should only contain functions needed for runtime animation, no tools.
=============================================================================*/ 

#pragma once
#include "Animation/AnimSequenceBase.h"

struct FInputBlendPose;
struct FA2CSPose;
struct FA2Pose;
struct FAnimTrack;
struct FPerBoneBlendWeight;
class UBlendSpaceBase;
class UAnimSequenceBase;
typedef TArray<FTransform> FTransformArrayA2;
/** In AnimationRunTime Library, we extract animation data based on Skeleton hierarchy, not ref pose hierarchy. 
	Ref pose will need to be re-mapped later **/

class ENGINE_API FAnimationRuntime
{
public:
	static void NormalizeRotations(const FBoneContainer& RequiredBones, /*inout*/ FTransformArrayA2& Atoms);
	static void NormalizeRotations(FTransformArrayA2 & Atoms);

	static void InitializeTransform(const FBoneContainer& RequiredBones, /*inout*/ FTransformArrayA2& Atoms);
#if DO_GUARD_SLOW
	static bool ContainsNaN(TArray<FBoneIndexType> & RequiredBoneIndices, FA2Pose & Pose);
#endif
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
	static void BlendPosesTogether(
		int32 NumPoses,
		const FTransformArrayA2** SourcePoses,
		const float* SourceWeights,
		const FBoneContainer& RequiredBones,
		/*out*/ FTransformArrayA2& ResultAtoms);

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
	static void BlendPosesTogether(
		int32 NumPoses,
		const TArray<FTransformArrayA2>& SourcePoses,
		const TArray<float>&	SourceWeights,
		const FBoneContainer& RequiredBones,
		/*out*/ FTransformArrayA2& ResultAtoms);

	/**
	 * Blends together a set of poses, each with a given weight.
	 * This function is for BlendSpace per bone blending. BlendSampleDataCache contains the weight information
	 *
	 * This blends in local space
	 *
	 * @param	ResultAtoms		Output array of relative bone transforms.
	 * @param	RequiredBones	Indices of bones that we want to return. Note that bones not in this array will not be modified, so are not safe to access! 
	 *							This array must be in strictly increasing order.
	 */
	static void BlendPosesTogetherPerBone(
		int32 NumPoses,
		const TArray<FTransformArrayA2>& SourcePoses,
		const UBlendSpaceBase* BlendSpace, 
		const TArray<FBlendSampleData>& BlendSampleDataCache, 
		const FBoneContainer& RequiredBones,
		/*out*/ FTransformArrayA2& ResultAtoms);

	/**
	 * Blends together a set of poses, each with a given weight.
	 * This function is for BlendSpace per bone blending. BlendSampleDataCache contains the weight information
	 *
	 * This blends rotation in mesh space, so performance intensive. 
	 *
	 * @param	ResultAtoms		Output array of relative bone transforms.
	 * @param	RequiredBones	Indices of bones that we want to return. Note that bones not in this array will not be modified, so are not safe to access! 
	 *							This array must be in strictly increasing order.
	 */
	static void BlendPosesTogetherPerBoneInMeshSpace(
		int32 NumPoses,
		TArray<FTransformArrayA2>& SourcePoses,
		const UBlendSpaceBase* BlendSpace, 
		const TArray<FBlendSampleData>& BlendSampleDataCache, 
		const FBoneContainer& RequiredBones,
		/*out*/ FTransformArrayA2& ResultAtoms);

	/** 
	 * Blend Poses per bone weights : The BasePoses + BlendPoses(SourceIndex) * Blend Weights(BoneIndex)
	 * Please note BlendWeights are array, so you can define per bone base 
	 * This supports multi per bone blending, but only one pose as blend at a time per track
	 * PerBoneBlendWeights.Num() == Atoms.Num()
	 *
	 * I had multiple debates about having PerBoneBlendWeights array(for memory reasons),  
	 * but it so much simplifies multiple purpose for this function instead of searching bonenames or 
	 * having multiple bone names with multiple weights, and filtering through which one is correct one
	 * I assume all those things should be determined before coming here and this only cares about weights
	 **/
	static void BlendPosesPerBoneFilter(struct FA2Pose& BasePose, const TArray<struct FA2Pose>& BlendPoses, struct FA2Pose& OutPose, TArray<FPerBoneBlendWeight>& BoneBlendWeights, bool bMeshSpaceRotationBlending, const FBoneContainer& RequiredBones, USkeleton* Skeleton);

	static void UpdateDesiredBoneWeight(const TArray<FPerBoneBlendWeight>& SrcBoneBlendWeights, TArray<FPerBoneBlendWeight>& TargetBoneBlendWeights, const TArray<float>& BlendWeights, const FBoneContainer& RequiredBones, USkeleton* Skeleton);

	static void CreateMaskWeights(int32 NumOfBones, 
			TArray<FPerBoneBlendWeight> & BoneBlendWeights, 
			const TArray<FInputBlendPose>	&BlendFilters, 
			const FBoneContainer& RequiredBones, 
			const USkeleton* Skeleton);

	static void CombineWithAdditiveAnimations(
		int32 NumAdditivePoses,
		const FTransformArrayA2** SourceAdditivePoses,
		const float* SourceAdditiveWeights,
		const FBoneContainer& RequiredBones,
		/*inout*/ FTransformArrayA2& Atoms);

	static void GetPoseFromSequence(
		const UAnimSequenceBase* Sequence,
		const FBoneContainer& RequiredBones,
		/*out*/ FTransformArrayA2& ResultAtoms,
		const FAnimExtractContext& ExtractionContext);

	static void GetPoseFromAnimTrack(
		const FAnimTrack& Track,
		const FBoneContainer& RequiredBones,
		/*out*/ FTransformArrayA2& ResultAtoms,
		const FAnimExtractContext& ExtractionContext);

	/** Fill ref pose **/
	static void FillWithRefPose(TArray<FTransform> & OutAtoms, const FBoneContainer& RequiredBones);

#if WITH_EDITOR
	/** fill with retarget base ref pose but this isn't used during run-time, so it always copies all of them */
	static void FillWithRetargetBaseRefPose( TArray<FTransform> & OutAtoms, const USkeletalMesh* Mesh, const FBoneContainer& RequiredBones );
#endif

	/** Convert LocalTransforms into MeshSpaceTransforms over RequiredBones. */
	static void ConvertPoseToMeshSpace(const TArray<FTransform> & LocalTransforms, TArray<FTransform> & MeshSpaceTransforms, const FBoneContainer& RequiredBones);

	/** Convert TargetPose into an AdditivePose, by doing TargetPose = TargetPose - BasePose over RequiredBones. */
	static void ConvertPoseToAdditive(FTransformArrayA2 & TargetPose, const FTransformArrayA2 & BasePose, const FBoneContainer& RequiredBones);

	/** Convert LocalPose into MeshSpaceRotations over RequiredBones. */
	static void ConvertPoseToMeshRotation(FTransformArrayA2 & LocalPose, const FBoneContainer& RequiredBones);

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
	static void BlendPosesAccumulate(
		const FTransformArrayA2& BlendPoses,
		const float BlendWeight, 
		const FBoneContainer& RequiredBones, 
		/*inout*/ FTransformArrayA2& ResultAtoms);

	/**
	 * Blends SourcePose + BlendPose*BlendWeight = ResultAtoms. As it states, this accumulates to the SourcePose
	 *
	 * Right now this came from BlendPosesTogether, light weight blend, no slerp or anything. 
	 * 
	 * @param	ResultAtoms		Output array of relative bone transforms.
	 * @param	RequiredBones	Indices of bones that we want to return. Note that bones not in this array will not be modified, so are not safe to access! 
	 *							This array must be in strictly increasing order.
	 */
	static void BlendAdditivePose(
		const FTransformArrayA2& SourcePoses, 
		const FTransformArrayA2& AdditiveBlendPoses, 
		const float BlendWeight, 
		const FBoneContainer& RequiredBones, 
		/*out*/ FTransformArrayA2& ResultAtoms);

	/** Lerp for BoneTransforms. Stores results in A. Performs A = Lerp(A, B, Alpha);
	 * @param A : In/Out transform array.
	 * @param B : B In transform array.
	 * @param Alpha : Alpha.
	 * @param RequiredBonesArray : Array of bone indices.
	 */
	static void LerpBoneTransforms(TArray<FTransform> & A, const TArray<FTransform> & B, float Alpha, const TArray<FBoneIndexType> & RequiredBonesArray);

	/** 
	 * Get Pose From BlendSpace
	 */
	static void GetPoseFromBlendSpace(
		UBlendSpaceBase * BlendSpace,
		TArray<FBlendSampleData>& BlendSampleDataCache,
		FBoneContainer & RequiredBones,
		/*out*/ FTransformArrayA2& ResultAtoms);

	/** 
	 * Advance CurrentTime to CurrentTime + MoveDelta. 
	 * It will handle wrapping if bAllowLooping is true
	 *
	 * return ETypeAdvanceAnim type
	 */
	static enum ETypeAdvanceAnim AdvanceTime(const bool & bAllowLooping, const float& MoveDelta, float& InOutTime, const float& EndTime);

	static void TickBlendWeight(float DeltaTime, float DesiredWeight, float& Weight, float& BlendTime);
	/** 
	 * Apply Weight to the Transform 
	 * Atoms = Weight * Atoms at the end
	 */
	static void ApplyWeightToTransform(const FBoneContainer& RequiredBones, /*inout*/ FTransformArrayA2& Atoms, float Weight);

	/** 
	 * Get Key Indices (start/end with alpha from start) with input parameter Time, NumFrames
	 * from % from StartKeyIndex, meaning (CurrentKeyIndex(float)-StartKeyIndex)/(EndKeyIndex-StartKeyIndex)
	 * by this Start-End, it will be between 0-(NumFrames-1), not number of Pos/Rot key tracks 
	 **/
	static void GetKeyIndicesFromTime(int32& OutKeyIndex1, int32& OutKeyIndex2, float& OutAlpha, const float Time, const int32 NumFrames, const float SequenceLength);

	/** 
	 *	Utility for taking an array of bone indices and ensuring that all parents are present 
	 *	(ie. all bones between those in the array and the root are present). 
	 *	Note that this must ensure the invariant that parent occur before children in BoneIndices.
	 */
	static void EnsureParentsPresent(TArray<FBoneIndexType>& BoneIndices, USkeletalMesh* SkelMesh);

	static void ExcludeBonesWithNoParents(const TArray<int32> & BoneIndices, const FReferenceSkeleton& RefSkeleton, TArray<int32> & FilteredRequiredBones);

	/** Convert a ComponentSpace FTransform to given BoneSpace. */
	static void ConvertCSTransformToBoneSpace
	(
		USkeletalMeshComponent * SkelComp,  
		FA2CSPose & MeshBases, 
		/*inout*/ FTransform& CSBoneTM, 
		int32 BoneIndex, 
		uint8 Space
	);

	/** Convert a BoneSpace FTransform to ComponentSpace. */
	static void ConvertBoneSpaceTransformToCS
	(
		USkeletalMeshComponent * SkelComp,  
		FA2CSPose & MeshBases, 
		/*inout*/ FTransform& BoneSpaceTM, 
		int32 BoneIndex, 
		uint8 Space
	);

	// FA2Pose/FA2CSPose Interfaces for template functions
	static FTransform GetSpaceTransform(FA2Pose& Pose, int32 Index);
	static FTransform GetSpaceTransform(FA2CSPose& Pose, int32 Index);
	static void SetSpaceTransform(FA2Pose& Pose, int32 Index, FTransform& NewTransform);
	static void SetSpaceTransform(FA2CSPose& Pose, int32 Index, FTransform& NewTransform);
	// space bases
#if WITH_EDITOR
	static void FillUpSpaceBases(const FReferenceSkeleton & RefSkeleton, const TArray<FTransform> &LocalAtoms, TArray<FTransform> &SpaceBases);
	static void FillUpSpaceBasesRefPose(const USkeleton* Skeleton, TArray<FTransform> &SpaceBaseRefPose);
	static void FillUpSpaceBasesRetargetBasePose(const USkeleton* Skeleton, TArray<FTransform> &SpaceBaseRefPose);
#endif

private:
	/** 
	* Blend Poses per bone weights : The BasePoses + BlendPoses(SourceIndex) * Blend Weights(BoneIndex)
	* Please note BlendWeights are array, so you can define per bone base 
	* This supports multi per bone blending, but only one pose as blend at a time per track
	* PerBoneBlendWeights.Num() == Atoms.Num()
	*
	* @note : This blends rotation in mesh space, but translation in local space
	*
	* I had multiple debates about having PerBoneBlendWeights array(for memory reasons),  
	* but it so much simplifies multiple purpose for this function instead of searching bonenames or 
	* having multiple bone names with multiple weights, and filtering through which one is correct one
	* I assume all those things should be determined before coming here and this only cares about weights
	**/
	static void BlendMeshPosesPerBoneWeights( 
			struct FA2Pose& BasePose, 
			TArray<struct FA2Pose>& BlendPoses, 
			const TArray<FPerBoneBlendWeight> & BoneBlendWeights, 
			const FBoneContainer& RequiredBones, 
			USkeleton* Skeleton,
			/*out*/ FA2Pose& OutPose);

	/** 
	* Blend Poses per bone weights : The BasePoses + BlendPoses(SourceIndex) * Blend Weights(BoneIndex)
	* Please note BlendWeights are array, so you can define per bone base 
	* This supports multi per bone blending, but only one pose as blend at a time per track
	* PerBoneBlendWeights.Num() == Atoms.Num()
	*
	* @note : This blends all in local space
	* 
	* I had multiple debates about having PerBoneBlendWeights array(for memory reasons),  
	* but it so much simplifies multiple purpose for this function instead of searching bonenames or 
	* having multiple bone names with multiple weights, and filtering through which one is correct one
	* I assume all those things should be determined before coming here and this only cares about weights
	**/
	static void BlendLocalPosesPerBoneWeights( 
			FA2Pose& BasePose, 
			TArray<FA2Pose>& BlendPoses, 
			const TArray<FPerBoneBlendWeight> & BoneBlendWeights, 
			const FBoneContainer& RequiredBones, 
			/*out*/ FA2Pose& OutPose);

};

