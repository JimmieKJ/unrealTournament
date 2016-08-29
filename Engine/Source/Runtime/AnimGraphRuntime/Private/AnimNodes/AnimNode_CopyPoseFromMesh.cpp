// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "AnimGraphRuntimePrivatePCH.h"
#include "AnimNodes/AnimNode_CopyPoseFromMesh.h"
#include "Animation/AnimInstanceProxy.h"

/////////////////////////////////////////////////////
// FAnimNode_CopyPoseFromMesh

FAnimNode_CopyPoseFromMesh::FAnimNode_CopyPoseFromMesh()
{
}

void FAnimNode_CopyPoseFromMesh::Initialize(const FAnimationInitializeContext& Context)
{
	FAnimNode_Base::Initialize(Context);
	ReinitializeMeshComponent(Context.AnimInstanceProxy);
}

void FAnimNode_CopyPoseFromMesh::CacheBones(const FAnimationCacheBonesContext& Context)
{
}

void FAnimNode_CopyPoseFromMesh::Update(const FAnimationUpdateContext& Context)
{
	EvaluateGraphExposedInputs.Execute(Context);

	if (CurrentlyUsedSourceMeshComponent.IsValid() && CurrentlyUsedSourceMeshComponent.Get() != SourceMeshComponent)
	{
		ReinitializeMeshComponent(Context.AnimInstanceProxy);
	}
	else if (!CurrentlyUsedSourceMeshComponent.IsValid() && SourceMeshComponent)
	{
		ReinitializeMeshComponent(Context.AnimInstanceProxy);
	}
}

void FAnimNode_CopyPoseFromMesh::Evaluate(FPoseContext& Output)
{
	FCompactPose& OutPose = Output.Pose;
	OutPose.ResetToRefPose();

	if (CurrentlyUsedSourceMeshComponent.IsValid() && CurrentlyUsedSourceMeshComponent.Get()->SkeletalMesh)
	{
		const FBoneContainer& RequiredBones = OutPose.GetBoneContainer();
		for(FCompactPoseBoneIndex PoseBoneIndex : OutPose.ForEachBoneIndex())
		{
			const int32& SkeletonBoneIndex = RequiredBones.GetSkeletonIndex(PoseBoneIndex);
			const int32& MeshBoneIndex = RequiredBones.GetSkeletonToPoseBoneIndexArray()[SkeletonBoneIndex];
			const int32* Value = BoneMapToSource.Find(MeshBoneIndex);
			if(Value && *Value!=INDEX_NONE)
			{
				const int32 SourceBoneIndex = *Value;
				const int32 ParentIndex = CurrentlyUsedSourceMeshComponent.Get()->SkeletalMesh->RefSkeleton.GetParentIndex(SourceBoneIndex);
				const FCompactPoseBoneIndex MyParentIndex = RequiredBones.GetParentBoneIndex(PoseBoneIndex);
				// only apply if I also have parent, otherwise, it should apply the space bases
				if (ParentIndex != INDEX_NONE && MyParentIndex != INDEX_NONE)
				{
					const FTransform& ParentTransform = CurrentlyUsedSourceMeshComponent.Get()->GetComponentSpaceTransforms()[ParentIndex];
					const FTransform& ChildTransform = CurrentlyUsedSourceMeshComponent.Get()->GetComponentSpaceTransforms()[SourceBoneIndex];
					OutPose[PoseBoneIndex] = ChildTransform.GetRelativeTransform(ParentTransform);
				}
				else
				{
					OutPose[PoseBoneIndex] = CurrentlyUsedSourceMeshComponent.Get()->GetComponentSpaceTransforms()[SourceBoneIndex];
				}
			}
		}
	}
}

void FAnimNode_CopyPoseFromMesh::GatherDebugData(FNodeDebugData& DebugData)
{
}

void FAnimNode_CopyPoseFromMesh::ReinitializeMeshComponent(FAnimInstanceProxy* AnimInstanceProxy)
{
	CurrentlyUsedSourceMeshComponent = SourceMeshComponent;
	BoneMapToSource.Reset();
	if (SourceMeshComponent && SourceMeshComponent->SkeletalMesh && !SourceMeshComponent->IsPendingKill())
	{
		USkeletalMeshComponent* TargetMeshComponent = AnimInstanceProxy->GetSkelMeshComponent();
		if (TargetMeshComponent)
		{
			USkeletalMesh* SourceSkelMesh = SourceMeshComponent->SkeletalMesh;
			USkeletalMesh* TargetSkelMesh = TargetMeshComponent->SkeletalMesh;

			if (SourceSkelMesh == TargetSkelMesh)
			{
				for(int32 ComponentSpaceBoneId = 0; ComponentSpaceBoneId < SourceSkelMesh->RefSkeleton.GetNum(); ++ComponentSpaceBoneId)
				{
					BoneMapToSource.Add(ComponentSpaceBoneId, ComponentSpaceBoneId);
				}
			}
			else
			{
				for (int32 ComponentSpaceBoneId=0; ComponentSpaceBoneId<TargetSkelMesh->RefSkeleton.GetNum(); ++ComponentSpaceBoneId)
				{
					FName BoneName = TargetSkelMesh->RefSkeleton.GetBoneName(ComponentSpaceBoneId);
					BoneMapToSource.Add(ComponentSpaceBoneId, SourceSkelMesh->RefSkeleton.FindBoneIndex(BoneName));
				}
			}
		}
	}
}