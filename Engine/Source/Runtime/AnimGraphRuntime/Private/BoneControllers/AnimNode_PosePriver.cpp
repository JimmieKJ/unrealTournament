// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "AnimGraphRuntimePrivatePCH.h"
#include "BoneControllers/AnimNode_PoseDriver.h"
#include "Animation/PoseAsset.h"

#include "AnimInstanceProxy.h"

//////////////////////////////////////////////////////////////////////////

void FPoseDriverParamSet::AddParam(const FPoseDriverParam& InParam, float InScale)
{
	bool bFoundParam = false;

	// Look to see if this param already exists in this set
	for (FPoseDriverParam& Param : Params)
	{
		if (Param.ParamInfo.Name == InParam.ParamInfo.Name)
		{
			Param.ParamValue += (InParam.ParamValue * InScale);
			bFoundParam = true;
		}
	}

	// If not found, add new entry and set to supplied value
	if (!bFoundParam)
	{
		int32 NewParamIndex = Params.AddZeroed();
		FPoseDriverParam& Param = Params[NewParamIndex];
		Param.ParamInfo = InParam.ParamInfo;
		Param.ParamValue = (InParam.ParamValue * InScale);
	}
}

void FPoseDriverParamSet::AddParams(const TArray<FPoseDriverParam>& InParams, float InScale)
{
	for (const FPoseDriverParam& InParam : InParams)
	{
		AddParam(InParam, InScale);
	}
}

void FPoseDriverParamSet::ScaleAllParams(float InScale)
{
	for (FPoseDriverParam& Param : Params)
	{
		Param.ParamValue *= InScale;
	}
}

void FPoseDriverParamSet::ClearParams()
{
	Params.Empty();
}


//////////////////////////////////////////////////////////////////////////

FAnimNode_PoseDriver::FAnimNode_PoseDriver()
{
	RadialScaling = 0.25f;
	bIncludeRefPoseAsNeutralPose = true;
	Type = EPoseDriverType::SwingOnly;
}

void FAnimNode_PoseDriver::GatherDebugData(FNodeDebugData& DebugData)
{
	FString DebugLine = DebugData.GetNodeName(this);

	DebugData.AddDebugItem(DebugLine);

	ComponentPose.GatherDebugData(DebugData);
}

void FAnimNode_PoseDriver::EvaluateBoneTransforms(USkeletalMeshComponent* SkelComp, FCSPose<FCompactPose>& MeshBases, TArray<FBoneTransform>& OutCSBoneTransforms)
{

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

float FAnimNode_PoseDriver::FindDistanceBetweenPoses(const FQuat& A, const FQuat& B)
{
	if (Type == EPoseDriverType::SwingAndTwist)
	{
		return A.AngularDistance(B);
	}
	else
	{
		FVector TwistVector = GetTwistAxisVector();
		FVector VecA = A.RotateVector(TwistVector);
		FVector VecB = B.RotateVector(TwistVector);

		const float Dot = FVector::DotProduct(VecA, VecB);
		return FMath::Acos(Dot);
	}
}

void FAnimNode_PoseDriver::UpdateCachedPoseInfo(const FQuat& RefQuat)
{
	if (PoseSource != nullptr)
	{
		int32 NumPoses = PoseSource->GetNumPoses();
		int32 NumPoseInfos = NumPoses;
		if (bIncludeRefPoseAsNeutralPose)
		{
			NumPoseInfos++;
		}

		PoseInfos.Empty();
		PoseInfos.AddZeroed(NumPoseInfos);

		// Get track index in PoseAsset for bone of interest
		const int32 TrackIndex = PoseSource->GetTrackIndexByName(SourceBone.BoneName);
		if (TrackIndex != INDEX_NONE)
		{
			TArray<FSmartName> PoseNames = PoseSource->GetPoseNames();

			// Cache quat for source bone for each pose
			for (int32 PoseIdx = 0; PoseIdx < NumPoses; PoseIdx++)
			{
				FPoseDriverPoseInfo& PoseInfo = PoseInfos[PoseIdx];

				FTransform BoneTransform;
				bool bFound = PoseSource->GetLocalPoseForTrack(PoseIdx, TrackIndex, BoneTransform);
				check(bFound);

				PoseInfo.PoseQuat = BoneTransform.GetRotation();
				PoseInfo.PoseName = PoseNames[PoseIdx].DisplayName;
			}

			// If we want to include ref pose, add that to end
			if (bIncludeRefPoseAsNeutralPose)
			{
				FPoseDriverPoseInfo& PoseInfo = PoseInfos.Last();

				static FName RefPoseName = FName(TEXT("RefPose"));
				PoseInfo.PoseName = RefPoseName;
				PoseInfo.PoseQuat = RefQuat;
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
							float Dist = FindDistanceBetweenPoses(PoseInfo.PoseQuat, OtherPoseInfo.PoseQuat);
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

void FAnimNode_PoseDriver::EvaluateComponentSpaceInternal(FComponentSpacePoseContext& Context)
{
	// Do nothing if no PoseAsset, or Info doesn't match
	if (PoseSource == nullptr)
	{
		return;
	}

	const FBoneContainer& BoneContainer = Context.Pose.GetPose().GetBoneContainer();

	// Final set of driven params, from all interpolators
	ResultParamSet.ClearParams();

	// Get the current rotation of the source bone
	const FTransform SourceCurrentBoneTransform = Context.Pose.GetLocalSpaceTransform(SourceBone.GetCompactPoseIndex(BoneContainer));
	const FQuat BoneQuat = SourceCurrentBoneTransform.GetRotation();

	const TArray<FAnimCurveBase> PoseAssetCurveData = PoseSource->GetCurveData();

	// Array of 'weights' of current orientation to each pose
	float TotalWeight = 0.f;

	// Ensure cached info is up to date
	if (!bCachedPoseInfoUpToDate)
	{
		const FTransform& SourceRefPoseBoneTransform = BoneContainer.GetRefPoseArray()[SourceBone.BoneIndex];

		UpdateCachedPoseInfo(SourceRefPoseBoneTransform.GetRotation());
	}

	// Iterate over each pose, adding its contribution
	for (int32 PoseInfoIdx = 0; PoseInfoIdx < PoseInfos.Num(); PoseInfoIdx++)
	{
		FPoseDriverPoseInfo& PoseInfo = PoseInfos[PoseInfoIdx];

		// Find distance
		PoseInfo.PoseDistance = FindDistanceBetweenPoses(BoneQuat, PoseInfo.PoseQuat);

		// Evaluate radial basis function to find weight
		float GaussVarianceSqr = FMath::Square(RadialScaling * PoseInfo.NearestPoseDist);
		PoseInfo.PoseWeight = FMath::Exp(-(PoseInfo.PoseDistance * PoseInfo.PoseDistance) / GaussVarianceSqr);

		// Add weight to total
		TotalWeight += PoseInfo.PoseWeight;

		// Final pose info may be ref pose, will not exist in PoseSource
		if (PoseInfoIdx < PoseSource->GetNumPoses())
		{
			// Get param values for this pose
			TArray<float> PoseAssetCurveValues = PoseSource->GetCurveValues(PoseInfoIdx);
			check(PoseAssetCurveValues.Num() == PoseAssetCurveData.Num()); // Should match name array

			// Add param values to driver info for this pose
			for (int32 CurveIdx = 0; CurveIdx < PoseAssetCurveValues.Num(); CurveIdx++)
			{
				// Only both copying params with non-zero weights
				float ParamValue = PoseAssetCurveValues[CurveIdx];
				if (FMath::Abs(ParamValue) > KINDA_SMALL_NUMBER)
				{
					FPoseDriverParam NewParam;

					NewParam.ParamInfo.Name = PoseAssetCurveData[CurveIdx].Name.DisplayName;
					NewParam.ParamInfo.UID = PoseAssetCurveData[CurveIdx].Name.UID;
					NewParam.ParamInfo.Type.bMaterial = PoseAssetCurveData[CurveIdx].GetCurveTypeFlag(ACF_DriveMaterial);
					NewParam.ParamInfo.Type.bMorphtarget = PoseAssetCurveData[CurveIdx].GetCurveTypeFlag(ACF_DriveMorphTarget);
					NewParam.ParamInfo.Type.bEvent = false;

					NewParam.ParamValue = PoseAssetCurveValues[CurveIdx];

					ResultParamSet.AddParam(NewParam, PoseInfo.PoseWeight);
				}
			}
		}
	}

	// Only normalize and apply if we got some kind of weight
	if (TotalWeight > KINDA_SMALL_NUMBER)
	{
		// Normalize params by rescaling by sum of weights
		const float WeightScale = 1.f / TotalWeight;
		ResultParamSet.ScaleAllParams(WeightScale);

		// Also normalize each pose weight
		for (FPoseDriverPoseInfo& PoseInfo : PoseInfos)
		{
			PoseInfo.PoseWeight *= WeightScale;
		}

		//	Morph target and Material parameter curves
		USkeleton* Skeleton = Context.AnimInstanceProxy->GetSkeleton();

		for (FPoseDriverParam& Param : ResultParamSet.Params)
		{
			if (Param.ParamInfo.UID != FSmartNameMapping::MaxUID)
			{
				Context.Curve.Set(Param.ParamInfo.UID, Param.ParamValue, Param.ParamInfo.Type.GetAnimCurveFlags() | ACF_DriveAttribute);
			}
		}
	}
}

bool FAnimNode_PoseDriver::IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones)
{
	// return true if at least one bone ref is valid
	return SourceBone.IsValid(RequiredBones);
}

void FAnimNode_PoseDriver::InitializeBoneReferences(const FBoneContainer& RequiredBones)
{
	// Init bone ref
	SourceBone.Initialize(RequiredBones);
}
