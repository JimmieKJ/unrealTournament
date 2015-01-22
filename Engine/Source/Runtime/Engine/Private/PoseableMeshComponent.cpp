// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PoseableMeshComponent.cpp: UPoseableMeshComponent methods.
=============================================================================*/

#include "EnginePrivate.h"
#include "AnimTree.h"
#include "Animation/AnimInstance.h"
#include "Components/PoseableMeshComponent.h"

UPoseableMeshComponent::UPoseableMeshComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

bool UPoseableMeshComponent::AllocateTransformData()
{
	// Allocate transforms if not present.
	if ( Super::AllocateTransformData() )
	{
		if( LocalAtoms.Num() != SkeletalMesh->RefSkeleton.GetNum() )
		{
			LocalAtoms = SkeletalMesh->RefSkeleton.GetRefBonePose();

			TArray<FBoneIndexType> RequiredBoneIndexArray;
			RequiredBoneIndexArray.AddUninitialized(LocalAtoms.Num());
			for(int32 BoneIndex = 0; BoneIndex < LocalAtoms.Num(); ++BoneIndex)
			{
				RequiredBoneIndexArray[BoneIndex] = BoneIndex;
			}

			RequiredBones.InitializeTo(RequiredBoneIndexArray, *SkeletalMesh);
		}

		FillSpaceBases();
		FlipEditableSpaceBases();

		return true;
	}

	LocalAtoms.Empty();

	return false;
}

void UPoseableMeshComponent::RefreshBoneTransforms(FActorComponentTickFunction* TickFunction)
{
	SCOPE_CYCLE_COUNTER(STAT_RefreshBoneTransforms);

	// Can't do anything without a SkeletalMesh
	if( !SkeletalMesh )
	{
		return;
	}

	// Do nothing more if no bones in skeleton.
	if( GetNumSpaceBases() == 0 )
	{
		return;
	}

	// We need the mesh space bone transforms now for renderer to get delta from ref pose:
	FillSpaceBases();
	FlipEditableSpaceBases();

	MarkRenderDynamicDataDirty();
}

void UPoseableMeshComponent::FillSpaceBases()
{
	SCOPE_CYCLE_COUNTER(STAT_SkelComposeTime);

	if( !SkeletalMesh )
	{
		return;
	}

	// right now all this does is to convert to SpaceBases
	check( SkeletalMesh->RefSkeleton.GetNum() == LocalAtoms.Num() );
	check( SkeletalMesh->RefSkeleton.GetNum() == GetNumSpaceBases());
	check( SkeletalMesh->RefSkeleton.GetNum() == BoneVisibilityStates.Num() );

	const int32 NumBones = LocalAtoms.Num();

#if (UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT)
	/** Keep track of which bones have been processed for fast look up */
	TArray<uint8> BoneProcessed;
	BoneProcessed.AddZeroed(NumBones);
#endif
	// Build in 3 passes.
	FTransform* LocalTransformsData = LocalAtoms.GetData(); 
	FTransform* SpaceBasesData = GetEditableSpaceBases().GetData();
	
	GetEditableSpaceBases()[0] = LocalAtoms[0];
#if (UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT)
	BoneProcessed[0] = 1;
#endif

	for(int32 BoneIndex=1; BoneIndex<LocalAtoms.Num(); BoneIndex++)
	{
		FPlatformMisc::Prefetch(SpaceBasesData + BoneIndex);

#if (UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT)
		// Mark bone as processed
		BoneProcessed[BoneIndex] = 1;
#endif
		// For all bones below the root, final component-space transform is relative transform * component-space transform of parent.
		const int32 ParentIndex = SkeletalMesh->RefSkeleton.GetParentIndex(BoneIndex);
		FPlatformMisc::Prefetch(SpaceBasesData + ParentIndex);

#if (UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT)
		// Check the precondition that Parents occur before Children in the RequiredBones array.
		checkSlow(BoneProcessed[ParentIndex] == 1);
#endif
		FTransform::Multiply(SpaceBasesData + BoneIndex, LocalTransformsData + BoneIndex, SpaceBasesData + ParentIndex);

		checkSlow(GetEditableSpaceBases()[BoneIndex].IsRotationNormalized());
		checkSlow(!GetEditableSpaceBases()[BoneIndex].ContainsNaN());
	}
	bNeedToFlipSpaceBaseBuffers = true;
}

void UPoseableMeshComponent::SetBoneTransformByName(FName BoneName, const FTransform& InTransform, EBoneSpaces::Type BoneSpace)
{
	if( !SkeletalMesh || !RequiredBones.IsValid() )
	{
		return;
	}

	int32 BoneIndex = GetBoneIndex(BoneName);
	if(BoneIndex >=0 && BoneIndex < LocalAtoms.Num())
	{
		LocalAtoms[BoneIndex] = InTransform;

		// If we haven't requested local space we need to transform the position passed in
		//if(BoneSpace != EBoneSpaces::LocalSpace)
		{
			if(BoneSpace == EBoneSpaces::WorldSpace)
			{
				LocalAtoms[BoneIndex].SetToRelativeTransform(GetComponentToWorld());
			}

			int32 ParentIndex = RequiredBones.GetParentBoneIndex(BoneIndex);
			if(ParentIndex >=0)
			{
				FA2CSPose CSPose;
				CSPose.AllocateLocalPoses(RequiredBones, LocalAtoms);

				LocalAtoms[BoneIndex].SetToRelativeTransform(CSPose.GetComponentSpaceTransform(ParentIndex));
			}

			// Need to send new state to render thread
			MarkRenderDynamicDataDirty();
		}
	}
}

void UPoseableMeshComponent::SetBoneLocationByName(FName BoneName, FVector InLocation, EBoneSpaces::Type BoneSpace)
{
	FTransform CurrentTransform = GetBoneTransformByName(BoneName, BoneSpace);
	CurrentTransform.SetLocation(InLocation);
	SetBoneTransformByName(BoneName, CurrentTransform, BoneSpace);
}

void UPoseableMeshComponent::SetBoneRotationByName(FName BoneName, FRotator InRotation, EBoneSpaces::Type BoneSpace)
{
	FTransform CurrentTransform = GetBoneTransformByName(BoneName, BoneSpace);
	CurrentTransform.SetRotation(FQuat(InRotation));
	SetBoneTransformByName(BoneName, CurrentTransform, BoneSpace);
}

void UPoseableMeshComponent::SetBoneScaleByName(FName BoneName, FVector InScale3D, EBoneSpaces::Type BoneSpace)
{
	FTransform CurrentTransform = GetBoneTransformByName(BoneName, BoneSpace);
	CurrentTransform.SetScale3D(InScale3D);
	SetBoneTransformByName(BoneName, CurrentTransform, BoneSpace);
}

FTransform UPoseableMeshComponent::GetBoneTransformByName(FName BoneName, EBoneSpaces::Type BoneSpace)
{
	if( !SkeletalMesh || !RequiredBones.IsValid() )
	{
		return FTransform();
	}

	int32 BoneIndex = GetBoneIndex(BoneName);
	if( BoneIndex == INDEX_NONE)
	{
		FString Message = FString::Printf(TEXT("Invalid Bone Name '%s'"), *BoneName.ToString());
		FFrame::KismetExecutionMessage(*Message, ELogVerbosity::Warning);
		return FTransform();
	}

	/*if(BoneSpace == EBoneSpaces::LocalSpace)
	{
		return LocalAtoms[i];
	}*/

	FA2CSPose CSPose;
	CSPose.AllocateLocalPoses(RequiredBones, LocalAtoms);

	if(BoneSpace == EBoneSpaces::ComponentSpace)
	{
		return CSPose.GetComponentSpaceTransform(BoneIndex);
	}
	else
	{
		return CSPose.GetComponentSpaceTransform(BoneIndex) * ComponentToWorld;
	}
}

FVector UPoseableMeshComponent::GetBoneLocationByName(FName BoneName, EBoneSpaces::Type BoneSpace)
{
	FTransform CurrentTransform = GetBoneTransformByName(BoneName, BoneSpace);
	return CurrentTransform.GetLocation();
}

FRotator UPoseableMeshComponent::GetBoneRotationByName(FName BoneName, EBoneSpaces::Type BoneSpace)
{
	FTransform CurrentTransform = GetBoneTransformByName(BoneName, BoneSpace);
	return FRotator(CurrentTransform.GetRotation());
}

FVector UPoseableMeshComponent::GetBoneScaleByName(FName BoneName, EBoneSpaces::Type BoneSpace)
{
	FTransform CurrentTransform = GetBoneTransformByName(BoneName, BoneSpace);
	return CurrentTransform.GetScale3D();
}

void UPoseableMeshComponent::ResetBoneTransformByName(FName BoneName)
{
	if( !SkeletalMesh )
	{
		return;
	}

	const int32 BoneIndex = GetBoneIndex(BoneName);
	if( BoneIndex != INDEX_NONE )
	{
		LocalAtoms[BoneIndex] = SkeletalMesh->RefSkeleton.GetRefBonePose()[BoneIndex];
	}
	else
	{
		FString Message = FString::Printf(TEXT("Invalid Bone Name '%s'"), *BoneName.ToString());
		FFrame::KismetExecutionMessage(*Message, ELogVerbosity::Warning);
	}
}