// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "AnimGraphRuntimePrivatePCH.h"
#include "AnimNodes/AnimNode_PoseHandler.h"
#include "Animation/AnimInstanceProxy.h"

/////////////////////////////////////////////////////
// FAnimPoseByNameNode

void FAnimNode_PoseHandler::Initialize(const FAnimationInitializeContext& Context)
{
	FAnimNode_AssetPlayerBase::Initialize(Context);

	UpdatePoseAssetProperty(Context.AnimInstanceProxy);
}

void FAnimNode_PoseHandler::CacheBones(const FAnimationCacheBonesContext& Context) 
{
	FAnimNode_AssetPlayerBase::CacheBones(Context);

	BoneBlendWeights.Reset();
	// this has to update bone blending weight
	if (CurrentPoseAsset.IsValid())
	{
		const TArray<FName> TrackNames = CurrentPoseAsset.Get()->GetTrackNames();
		const FBoneContainer& BoneContainer = Context.AnimInstanceProxy->GetRequiredBones();
		const TArray<FBoneIndexType>& RequiredBoneIndices = BoneContainer.GetBoneIndicesArray();
		BoneBlendWeights.AddZeroed(RequiredBoneIndices.Num());

		for (const auto& TrackName : TrackNames)
		{
			int32 MeshBoneIndex = BoneContainer.GetPoseBoneIndexForBoneName(TrackName);
			if (MeshBoneIndex != INDEX_NONE)
			{
				BoneBlendWeights[MeshBoneIndex] = 1.f;
			}
		}
	}
}

void FAnimNode_PoseHandler::UpdateAssetPlayer(const FAnimationUpdateContext& Context)
{
	EvaluateGraphExposedInputs.Execute(Context);

	// update pose asset if it's not valid
	if (CurrentPoseAsset.IsValid() == false || CurrentPoseAsset.Get() != PoseAsset)
	{
		UpdatePoseAssetProperty(Context.AnimInstanceProxy);
	}
}

void FAnimNode_PoseHandler::OverrideAsset(UAnimationAsset* NewAsset)
{
	if(UPoseAsset* NewPoseAsset = Cast<UPoseAsset>(NewAsset))
	{
		PoseAsset = NewPoseAsset;
	}
}

void FAnimNode_PoseHandler::GatherDebugData(FNodeDebugData& DebugData)
{
	FString DebugLine = DebugData.GetNodeName(this);
	
	DebugLine += FString::Printf(TEXT("('%s')"), *GetNameSafe(PoseAsset));
	DebugData.AddDebugItem(DebugLine, true);
}

void FAnimNode_PoseHandler::UpdatePoseAssetProperty(struct FAnimInstanceProxy* InstanceProxy)
{
	CurrentPoseAsset = PoseAsset;

	if (CurrentPoseAsset.IsValid())
	{
		const TArray<FSmartName>& PoseNames = CurrentPoseAsset->GetPoseNames();
		const int32 TotalPoseNum = PoseNames.Num();
		if (TotalPoseNum > 0)
		{
			PoseExtractContext.PoseCurves.Reset(TotalPoseNum);
			PoseUIDList.Reset(TotalPoseNum);

			if (TotalPoseNum > 0)
			{
				PoseExtractContext.PoseCurves.AddZeroed(TotalPoseNum);
				for (const auto& PoseName : PoseNames)
				{
					PoseUIDList.Add(PoseName.UID);
				}
			}

			CacheBones(FAnimationCacheBonesContext(InstanceProxy));
		}
	}
	else
	{
		PoseUIDList.Reset();
		PoseExtractContext.PoseCurves.Reset();
		BoneBlendWeights.Reset();
	}
}