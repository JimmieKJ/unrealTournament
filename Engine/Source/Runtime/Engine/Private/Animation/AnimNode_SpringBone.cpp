// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "Animation/BoneControllers/AnimNode_SpringBone.h"

/////////////////////////////////////////////////////
// FAnimNode_SpringBone

FAnimNode_SpringBone::FAnimNode_SpringBone()
	: bLimitDisplacement(false)
	, MaxDisplacement(0.0f)
	, SpringStiffness(50.0f)
	, SpringDamping(4.0f)
	, ErrorResetThresh(256.0f)
	, bNoZSpring(false)
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

void FAnimNode_SpringBone::Update(const FAnimationUpdateContext& Context)
{
	FAnimNode_SkeletalControlBase::Update(Context);

	RemainingTime += Context.GetDeltaTime();

	const USkeletalMeshComponent* SkelComp = Context.AnimInstance->GetSkelMeshComponent();
	const UWorld* World = SkelComp->GetWorld();
	check(World->GetWorldSettings());
	// Fixed step simulation at 120hz
	FixedTimeStep = (1.f / 120.f) * World->GetWorldSettings()->GetEffectiveTimeDilation();
}

void FAnimNode_SpringBone::GatherDebugData(FNodeDebugData& DebugData)
{
	const float ActualAlpha = AlphaScaleBias.ApplyTo(Alpha);

	//MDW_TODO Add more output info?
	FString DebugLine = DebugData.GetNodeName(this);
	DebugLine += FString::Printf(TEXT("(Alpha: %.1f%% RemainingTime: %.3f)"), ActualAlpha*100.f, RemainingTime);

	DebugData.AddDebugItem(DebugLine);
	ComponentPose.GatherDebugData(DebugData);
}

void FAnimNode_SpringBone::EvaluateBoneTransforms(USkeletalMeshComponent* SkelComp, const FBoneContainer& RequiredBones, FA2CSPose& MeshBases, TArray<FBoneTransform>& OutBoneTransforms)
{
	check(OutBoneTransforms.Num() == 0);

	// Location of our bone in world space
	FTransform SpaceBase = MeshBases.GetComponentSpaceTransform(SpringBone.BoneIndex);
	FTransform  BoneTransformInWorldSpace = (SkelComp != NULL) ? SpaceBase * SkelComp->GetComponentToWorld() : SpaceBase;

	FVector const TargetPos = BoneTransformInWorldSpace.GetLocation();

	AActor* SkelOwner = (SkelComp != NULL) ? SkelComp->GetOwner() : NULL;
	if ((SkelComp != NULL) && (SkelComp->AttachParent != NULL) && (SkelOwner == NULL))
	{
		SkelOwner = SkelComp->AttachParent->GetOwner();
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

		// Force z to be correct if desired
		if (bNoZSpring)
		{
			BoneLocation.Z = TargetPos.Z;
		}

		// If desired, limit error
		if (bLimitDisplacement)
		{
			FVector CurrentDisp = BoneLocation - TargetPos;
			// Too far away - project back onto sphere around target.
			if (CurrentDisp.Size() > MaxDisplacement)
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

	// Output new transform for current bone.
	OutBoneTransforms.Add( FBoneTransform(SpringBone.BoneIndex, OutBoneTM) );
}


bool FAnimNode_SpringBone::IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones) 
{
	return (SpringBone.IsValid(RequiredBones));
}

void FAnimNode_SpringBone::InitializeBoneReferences(const FBoneContainer& RequiredBones) 
{
	SpringBone.Initialize(RequiredBones);
}
