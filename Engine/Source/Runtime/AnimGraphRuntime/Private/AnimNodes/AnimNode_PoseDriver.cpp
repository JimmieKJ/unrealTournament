// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "AnimNodes/AnimNode_PoseDriver.h"
#include "AnimationRuntime.h"

#include "Animation/AnimInstanceProxy.h"


FAnimNode_PoseDriver::FAnimNode_PoseDriver()
{
	RadialScaling = 0.25f;
	bIncludeRefPoseAsNeutralPose = true;
	Type = EPoseDriverType::SwingOnly;
}

void FAnimNode_PoseDriver::Initialize(const FAnimationInitializeContext& Context)
{
	FAnimNode_PoseHandler::Initialize(Context);

	SourcePose.Initialize(Context);
}

void FAnimNode_PoseDriver::CacheBones(const FAnimationCacheBonesContext& Context)
{
	FAnimNode_PoseHandler::CacheBones(Context);
	// Init pose input
	SourcePose.CacheBones(Context);
	// Init bone ref
	SourceBone.Initialize(Context.AnimInstanceProxy->GetRequiredBones());
}

void FAnimNode_PoseDriver::UpdateAssetPlayer(const FAnimationUpdateContext& Context)
{
	FAnimNode_PoseHandler::UpdateAssetPlayer(Context);
	SourcePose.Update(Context);
}

void FAnimNode_PoseDriver::GatherDebugData(FNodeDebugData& DebugData)
{
	FAnimNode_PoseHandler::GatherDebugData(DebugData);
	SourcePose.GatherDebugData(DebugData.BranchFlow(1.f));
}

void FAnimNode_PoseDriver::OnPoseAssetChange()
{
	FAnimNode_PoseHandler::OnPoseAssetChange();
	bCachedPoseInfoUpToDate = false;
}

FVector FAnimNode_PoseDriver::GetTwistAxisVector()
{
	switch (TwistAxis)
	{
	case BA_X:
	default:
		return FVector(1.f, 0.f, 0.f);
	case BA_Y:
		return FVector(0.f, 1.f, 0.f);
	case BA_Z:
		return FVector(0.f, 0.f, 1.f);
	}
}

float FAnimNode_PoseDriver::FindDistanceBetweenPoses(const FTransform& A, const FTransform& B)
{
	if (Type == EPoseDriverType::SwingAndTwist)
	{
		const FQuat AQuat = A.GetRotation();
		const FQuat BQuat = B.GetRotation();
		return AQuat.AngularDistance(BQuat);
	}
	else if(Type == EPoseDriverType::SwingOnly)
	{
		FVector TwistVector = GetTwistAxisVector();
		FVector VecA = A.TransformVectorNoScale(TwistVector);
		FVector VecB = B.TransformVectorNoScale(TwistVector);

		const float Dot = FVector::DotProduct(VecA, VecB);
		return FMath::Acos(Dot);
	}
	else if (Type == EPoseDriverType::Translation)
	{
		float DistSquared = (A.GetTranslation() - B.GetTranslation()).SizeSquared();
		if (DistSquared > SMALL_NUMBER)
		{
			return FMath::Sqrt(DistSquared);
		}
		else
		{
			return 0.f;
		}
	}
	else
	{
		ensureMsgf(false, TEXT("Unknown EPoseDriverType"));
		return 0.f;
	}
}

void FAnimNode_PoseDriver::UpdateCachedPoseInfo(const FTransform& RefTM)
{
	if (CurrentPoseAsset.IsValid())
	{
		int32 NumPoses = CurrentPoseAsset.Get()->GetNumPoses();
		int32 NumPoseInfos = NumPoses;
		if (bIncludeRefPoseAsNeutralPose)
		{
			NumPoseInfos++;
		}

		PoseInfos.Empty();
		PoseInfos.AddZeroed(NumPoseInfos);

		// Get track index in PoseAsset for bone of interest
		const int32 TrackIndex = CurrentPoseAsset.Get()->GetTrackIndexByName(SourceBone.BoneName);
		if (TrackIndex != INDEX_NONE)
		{
			TArray<FSmartName> PoseNames = CurrentPoseAsset.Get()->GetPoseNames();

			// Cache quat for source bone for each pose
			for (int32 PoseIdx = 0; PoseIdx < NumPoses; PoseIdx++)
			{
				FPoseDriverPoseInfo& PoseInfo = PoseInfos[PoseIdx];

				FTransform BoneTransform;
				bool bFound = CurrentPoseAsset.Get()->GetLocalPoseForTrack(PoseIdx, TrackIndex, BoneTransform);
				check(bFound);

				PoseInfo.PoseTM = BoneTransform;
				PoseInfo.PoseName = PoseNames[PoseIdx].DisplayName;

			}

			// If we want to include ref pose, add that to end
			if (bIncludeRefPoseAsNeutralPose)
			{
				FPoseDriverPoseInfo& PoseInfo = PoseInfos.Last();

				static FName RefPoseName = FName(TEXT("RefPose"));
				PoseInfo.PoseTM = RefTM;
				PoseInfo.PoseName = RefPoseName;
			}

			// Calculate largest distance between poses
			if (NumPoseInfos == 1)
			{
				// If only one pose, set to 180 degrees
				PoseInfos[0].NearestPoseDist = PI;
			}
			else
			{
				// Iterate over poses
				for (int32 PoseIdx = 0; PoseIdx < NumPoseInfos; PoseIdx++)
				{
					FPoseDriverPoseInfo& PoseInfo = PoseInfos[PoseIdx];
					PoseInfo.NearestPoseDist = BIG_NUMBER; // init to large value

					for (int32 OtherPoseIdx = 0; OtherPoseIdx < NumPoseInfos; OtherPoseIdx++)
					{
						if (OtherPoseIdx != PoseIdx) // If not ourself..
						{
							// Get distance between poses
							FPoseDriverPoseInfo& OtherPoseInfo = PoseInfos[OtherPoseIdx];
							float Dist = FindDistanceBetweenPoses(PoseInfo.PoseTM, OtherPoseInfo.PoseTM);
							PoseInfo.NearestPoseDist = FMath::Min(Dist, PoseInfo.NearestPoseDist);
						}
					}

					// Avoid zero dist if poses are all on top of each other
					PoseInfo.NearestPoseDist = FMath::Max(PoseInfo.NearestPoseDist, KINDA_SMALL_NUMBER);
				}
			}
		}
	}

	bCachedPoseInfoUpToDate = true;
}

void FAnimNode_PoseDriver::Evaluate(FPoseContext& Output)
{
	FPoseContext SourceData(Output);
	SourcePose.Evaluate(SourceData);

	// Get the index of the source bone
	const FBoneContainer& BoneContainer = SourceData.Pose.GetBoneContainer();
	const FCompactPoseBoneIndex SourceCompactIndex = SourceBone.GetCompactPoseIndex(BoneContainer);

	// Do nothing if no PoseAsset, or bone is not found/LOD-ed out
	if (!CurrentPoseAsset.IsValid() || SourceCompactIndex == INDEX_NONE || !Output.AnimInstanceProxy->IsSkeletonCompatible(CurrentPoseAsset->GetSkeleton()))
	{
		Output = SourceData;
		return;
	}

	// Get transform of source bone
	SourceBoneTM = SourceData.Pose[SourceCompactIndex];

	// Ensure cached info is up to date
	if (!bCachedPoseInfoUpToDate)
	{
		const FTransform& SourceRefPoseBoneTM = BoneContainer.GetRefPoseTransform(SourceCompactIndex);

		UpdateCachedPoseInfo(SourceRefPoseBoneTM);
	}

	float TotalWeight = 0.f; // Keep track of total weight generated, to renormalize at the end

	// Iterate over each pose, adding its contribution
	for (int32 PoseInfoIdx = 0; PoseInfoIdx < PoseInfos.Num(); PoseInfoIdx++)
	{
		FPoseDriverPoseInfo& PoseInfo = PoseInfos[PoseInfoIdx];

		// Find distance
		PoseInfo.PoseDistance = FindDistanceBetweenPoses(SourceBoneTM, PoseInfo.PoseTM);

		// Evaluate radial basis function to find weight
		float GaussVarianceSqr = FMath::Square(RadialScaling * PoseInfo.NearestPoseDist);
		PoseInfo.PoseWeight = FMath::Exp(-(PoseInfo.PoseDistance * PoseInfo.PoseDistance) / GaussVarianceSqr);

		// Add weight to total
		TotalWeight += PoseInfo.PoseWeight;
	}

	// Only normalize and apply if we got some kind of weight
	if (TotalWeight > KINDA_SMALL_NUMBER)
	{
		// Normalize pose weights by rescaling by sum of weights
		const float WeightScale = 1.f / TotalWeight;
		for (FPoseDriverPoseInfo& PoseInfo : PoseInfos)
		{
			PoseInfo.PoseWeight *= WeightScale;
		}

		FPoseContext CurrentPose(Output);
		check(PoseExtractContext.PoseCurves.Num() == PoseUIDList.Num());
		for (int32 PoseIdx = 0; PoseIdx < PoseUIDList.Num(); ++PoseIdx)
		{
			PoseExtractContext.PoseCurves[PoseIdx] = PoseInfos[PoseIdx].PoseWeight;
		}

		if (CurrentPoseAsset.Get()->GetAnimationPose(CurrentPose.Pose, CurrentPose.Curve, PoseExtractContext))
		{
			// blend by weight
			if (CurrentPoseAsset->IsValidAdditive())
			{
				// Don't want to modify SourceBone, set additive offset to identity
				CurrentPose.Pose[SourceCompactIndex] = FTransform::Identity;

				Output = SourceData;
				FAnimationRuntime::AccumulateAdditivePose(Output.Pose, CurrentPose.Pose, Output.Curve, CurrentPose.Curve, 1.f, EAdditiveAnimationType::AAT_LocalSpaceBase);
			}
			else
			{
				// Don't want to modify SourceBone, set weight to zero
				BoneBlendWeights[SourceCompactIndex.GetInt()] = 0.f;

				FAnimationRuntime::BlendTwoPosesTogetherPerBone(SourceData.Pose, CurrentPose.Pose, SourceData.Curve, CurrentPose.Curve, BoneBlendWeights, Output.Pose, Output.Curve);
			}
		}
	}
	// No poses activated, just pass through
	else
	{
		Output = SourceData;
	}
}
