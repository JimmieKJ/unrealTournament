// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "AnimGraphRuntimePrivatePCH.h"
#include "BoneControllers/AnimNode_SpringBone.h"
#include "Animation/AnimInstanceProxy.h"

/////////////////////////////////////////////////////
// FAnimNode_SpringBone

FAnimNode_SpringBone::FAnimNode_SpringBone()
	: bLimitDisplacement(false)
	, MaxDisplacement(0.0f)
	, SpringStiffness(50.0f)
	, SpringDamping(4.0f)
	, ErrorResetThresh(256.0f)
	, bNoZSpring_DEPRECATED(false)
	, bTranslateX(true)
	, bTranslateY(true)
	, bTranslateZ(true)
	, bRotateX(false)
	, bRotateY(false)
	, bRotateZ(false)
	, RemainingTime(0.f)
	, bHadValidStrength(false)
	, BoneLocation(FVector::ZeroVector)
	, BoneVelocity(FVector::ZeroVector)
{
}

void FAnimNode_SpringBone::Initialize(const FAnimationInitializeContext& Context)
{
	FAnimNode_SkeletalControlBase::Initialize(Context);

	RemainingTime = 0.0f;
}

void FAnimNode_SpringBone::CacheBones(const FAnimationCacheBonesContext& Context) 
{
	FAnimNode_SkeletalControlBase::CacheBones(Context);
}

void FAnimNode_SpringBone::UpdateInternal(const FAnimationUpdateContext& Context)
{
	FAnimNode_SkeletalControlBase::UpdateInternal(Context);

	RemainingTime += Context.GetDeltaTime();

	// Fixed step simulation at 120hz
	FixedTimeStep = (1.f / 120.f) * TimeDilation;
}

void FAnimNode_SpringBone::GatherDebugData(FNodeDebugData& DebugData)
{
	const float ActualBiasedAlpha = AlphaScaleBias.ApplyTo(Alpha);

	//MDW_TODO Add more output info?
	FString DebugLine = DebugData.GetNodeName(this);
	DebugLine += FString::Printf(TEXT("(Alpha: %.1f%% RemainingTime: %.3f)"), ActualBiasedAlpha*100.f, RemainingTime);

	DebugData.AddDebugItem(DebugLine);
	ComponentPose.GatherDebugData(DebugData);
}

FORCEINLINE void CopyToVectorByFlags(FVector& DestVec, const FVector& SrcVec, bool bX, bool bY, bool bZ)
{
	if (bX)
	{
		DestVec.X = SrcVec.X;
	}
	if (bY)
	{
		DestVec.Y = SrcVec.Y;
	}
	if (bZ)
	{
		DestVec.Z = SrcVec.Z;
	}
}

void FAnimNode_SpringBone::EvaluateBoneTransforms(USkeletalMeshComponent* SkelComp, FCSPose<FCompactPose>& MeshBases, TArray<FBoneTransform>& OutBoneTransforms)
{
	check(SkelComp);
	check(OutBoneTransforms.Num() == 0);

	const bool bNoOffset = !bTranslateX && !bTranslateY && !bTranslateZ;
	if (bNoOffset)
	{
		return;
	}

	// Location of our bone in world space
	const FBoneContainer& BoneContainer = MeshBases.GetPose().GetBoneContainer();

	const FCompactPoseBoneIndex SpringBoneIndex = SpringBone.GetCompactPoseIndex(BoneContainer);
	const FTransform& SpaceBase = MeshBases.GetComponentSpaceTransform(SpringBoneIndex);
	FTransform BoneTransformInWorldSpace = SpaceBase * SkelComp->GetComponentToWorld();

	FVector const TargetPos = BoneTransformInWorldSpace.GetLocation();

	AActor* SkelOwner = SkelComp->GetOwner();
	if (SkelComp->GetAttachParent() != NULL && (SkelOwner == NULL))
	{
		SkelOwner = SkelComp->GetAttachParent()->GetOwner();
	}

	// Init values first time
	if (RemainingTime == 0.0f)
	{
		BoneLocation = TargetPos;
		BoneVelocity = FVector::ZeroVector;
	}
		
	while (RemainingTime > FixedTimeStep)
	{
		// Update location of our base by how much our base moved this frame.
		FVector const BaseTranslation = SkelOwner ? (SkelOwner->GetVelocity() * FixedTimeStep) : FVector::ZeroVector;
		BoneLocation += BaseTranslation;

		// Reinit values if outside reset threshold
		if (((TargetPos - BoneLocation).SizeSquared() > (ErrorResetThresh*ErrorResetThresh)))
		{
			BoneLocation = TargetPos;
			BoneVelocity = FVector::ZeroVector;
		}

		// Calculate error vector.
		FVector const Error = (TargetPos - BoneLocation);
		FVector const DampingForce = SpringDamping * BoneVelocity;
		FVector const SpringForce = SpringStiffness * Error;

		// Calculate force based on error and vel
		FVector const Acceleration = SpringForce - DampingForce;

		// Integrate velocity
		// Make sure damping with variable frame rate actually dampens velocity. Otherwise Spring will go nuts.
		float const CutOffDampingValue = 1.f/FixedTimeStep;
		if (SpringDamping > CutOffDampingValue)
		{
			float const SafetyScale = CutOffDampingValue / SpringDamping;
			BoneVelocity += SafetyScale * (Acceleration * FixedTimeStep);
		}
		else
		{
			BoneVelocity += (Acceleration * FixedTimeStep);
		}

		// Clamp velocity to something sane (|dX/dt| <= ErrorResetThresh)
		float const BoneVelocityMagnitude = BoneVelocity.Size();
		if (BoneVelocityMagnitude * FixedTimeStep > ErrorResetThresh)
		{
			BoneVelocity *= (ErrorResetThresh / (BoneVelocityMagnitude * FixedTimeStep));
		}

		// Integrate position
		FVector const OldBoneLocation = BoneLocation;
		FVector const DeltaMove = (BoneVelocity * FixedTimeStep);
		BoneLocation += DeltaMove;

		// Filter out spring translation based on our filter properties
		CopyToVectorByFlags(BoneLocation, TargetPos, !bTranslateX, !bTranslateY, !bTranslateZ);


		// If desired, limit error
		if (bLimitDisplacement)
		{
			FVector CurrentDisp = BoneLocation - TargetPos;
			// Too far away - project back onto sphere around target.
			if (CurrentDisp.SizeSquared() > FMath::Square(MaxDisplacement))
			{
				FVector DispDir = CurrentDisp.GetSafeNormal();
				BoneLocation = TargetPos + (MaxDisplacement * DispDir);
			}
		}

		// Update velocity to reflect post processing done to bone location.
		BoneVelocity = (BoneLocation - OldBoneLocation) / FixedTimeStep;

		check( !BoneLocation.ContainsNaN() );
		check( !BoneVelocity.ContainsNaN() );

		RemainingTime -= FixedTimeStep;
	}

	// Now convert back into component space and output - rotation is unchanged.
	FTransform OutBoneTM = SpaceBase;
	OutBoneTM.SetLocation( SkelComp->GetComponentToWorld().InverseTransformPosition(BoneLocation) );

	const bool bUseRotation = bRotateX || bRotateY || bRotateZ;
	if (bUseRotation)
	{
		FCompactPoseBoneIndex ParentBoneIndex = MeshBases.GetPose().GetParentBoneIndex(SpringBoneIndex);
		const FTransform& ParentSpaceBase = MeshBases.GetComponentSpaceTransform(ParentBoneIndex);

		FVector ParentToTarget = (TargetPos - ParentSpaceBase.GetLocation()).GetSafeNormal();
		FVector ParentToCurrent = (BoneLocation - ParentSpaceBase.GetLocation()).GetSafeNormal();

		FQuat AdditionalRotation = FQuat::FindBetweenNormals(ParentToTarget, ParentToCurrent);

		// Filter rotation based on our filter properties
		FVector EularRot = AdditionalRotation.Euler();
		CopyToVectorByFlags(EularRot, FVector::ZeroVector, !bRotateX, !bRotateY, !bRotateZ);

		OutBoneTM.SetRotation(FQuat::MakeFromEuler(EularRot) * OutBoneTM.GetRotation());
	}

	// Output new transform for current bone.
	OutBoneTransforms.Add(FBoneTransform(SpringBoneIndex, OutBoneTM));
}


bool FAnimNode_SpringBone::IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones) 
{
	return (SpringBone.IsValid(RequiredBones));
}

void FAnimNode_SpringBone::InitializeBoneReferences(const FBoneContainer& RequiredBones) 
{
	SpringBone.Initialize(RequiredBones);
}

void FAnimNode_SpringBone::PreUpdate(const UAnimInstance* InAnimInstance)
{
	const USkeletalMeshComponent* SkelComp = InAnimInstance->GetSkelMeshComponent();
	const UWorld* World = SkelComp->GetWorld();
	check(World->GetWorldSettings());
	TimeDilation = World->GetWorldSettings()->GetEffectiveTimeDilation();
}