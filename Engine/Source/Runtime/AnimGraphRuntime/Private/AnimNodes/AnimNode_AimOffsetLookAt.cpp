// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "AnimNodes/AnimNode_AimOffsetLookAt.h"
#include "Animation/AnimInstanceProxy.h"
#include "AnimationRuntime.h"
#include "Animation/BlendSpaceBase.h"
#include "Engine/SkeletalMeshSocket.h"
#include "DrawDebugHelpers.h"
#include "EngineGlobals.h"
#include "Engine/Engine.h"

TAutoConsoleVariable<int32> CVarAimOffsetLookAtEnable(TEXT("a.AnimNode.AimOffsetLookAt.Enable"), 1, TEXT("Enable/Disable LookAt AimOffset"));
TAutoConsoleVariable<int32> CVarAimOffsetLookAtDebug(TEXT("a.AnimNode.AimOffsetLookAt.Debug"), 0, TEXT("Toggle LookAt AimOffset debug"));

/////////////////////////////////////////////////////
// FAnimNode_AimOffsetLookAt

void FAnimNode_AimOffsetLookAt::Initialize(const FAnimationInitializeContext& Context)
{
	FAnimNode_BlendSpacePlayer::Initialize(Context);
	BasePose.Initialize(Context);
}

void FAnimNode_AimOffsetLookAt::CacheBones(const FAnimationCacheBonesContext& Context)
{
	FAnimNode_BlendSpacePlayer::CacheBones(Context);
	BasePose.CacheBones(Context);
}

void FAnimNode_AimOffsetLookAt::UpdateAssetPlayer(const FAnimationUpdateContext& Context)
{
	EvaluateGraphExposedInputs.Execute(Context);

	bIsLODEnabled = IsLODEnabled(Context.AnimInstanceProxy, LODThreshold);

	// We don't support ticking and advancing time, because Inputs are determined during Evaluate.
	// it may be possible to advance time there (is it a problem with notifies?)
	// But typically AimOffsets contain single frame poses, so time doesn't matter.

// 	if (bIsLODEnabled)
// 	{
// 		FAnimNode_BlendSpacePlayer::UpdateAssetPlayer(Context);
// 	}

	BasePose.Update(Context);
}

void FAnimNode_AimOffsetLookAt::Evaluate(FPoseContext& Context)
{
	// Evaluate base pose
	BasePose.Evaluate(Context);

	if (bIsLODEnabled && FAnimWeight::IsRelevant(Alpha) && (CVarAimOffsetLookAtEnable.GetValueOnAnyThread() == 1))
	{
		UpdateFromLookAtTarget(Context);

		// Evaluate MeshSpaceRotation additive blendspace
		FPoseContext MeshSpaceRotationAdditivePoseContext(Context);
		FAnimNode_BlendSpacePlayer::Evaluate(MeshSpaceRotationAdditivePoseContext);

		// Accumulate poses together
		FAnimationRuntime::AccumulateMeshSpaceRotationAdditiveToLocalPose(Context.Pose, MeshSpaceRotationAdditivePoseContext.Pose, Context.Curve, MeshSpaceRotationAdditivePoseContext.Curve, Alpha);

		// Resulting rotations are not normalized, so normalize here.
		Context.Pose.NormalizeRotations();
	}
}

void FAnimNode_AimOffsetLookAt::UpdateFromLookAtTarget(FPoseContext& LocalPoseContext)
{
	const FBoneContainer& RequiredBones = LocalPoseContext.Pose.GetBoneContainer();
	if (RequiredBones.GetSkeletalMeshAsset())
	{
		const USkeletalMeshSocket* Socket = RequiredBones.GetSkeletalMeshAsset()->FindSocket(SourceSocketName);
		if (Socket)
		{
			const FTransform SocketLocalTransform = Socket->GetSocketLocalTransform();

			FBoneReference SocketBoneReference;
			SocketBoneReference.BoneName = Socket->BoneName;
			SocketBoneReference.Initialize(RequiredBones);

			if (SocketBoneReference.IsValid(RequiredBones))
			{
				const FCompactPoseBoneIndex SocketBoneIndex = SocketBoneReference.GetCompactPoseIndex(RequiredBones);

				FCSPose<FCompactPose> GlobalPose;
				GlobalPose.InitPose(LocalPoseContext.Pose);

				USkeletalMeshComponent* Component = LocalPoseContext.AnimInstanceProxy->GetSkelMeshComponent();
				AActor* Actor = Component ? Component->GetOwner() : nullptr;

				if (Component && Actor &&  BlendSpace)
				{
					const FTransform ActorTransform = Actor->GetTransform();

					const FTransform BoneTransform = GlobalPose.GetComponentSpaceTransform(SocketBoneIndex);
					const FTransform SocketWorldTransform = SocketLocalTransform * BoneTransform * Component->ComponentToWorld;

					// Convert Target to Actor Space
					const FTransform TargetWorldTransform(LookAtLocation);

					const FVector DirectionToTarget = ActorTransform.InverseTransformVectorNoScale(TargetWorldTransform.GetLocation() - SocketWorldTransform.GetLocation()).GetSafeNormal();
					const FVector CurrentDirection = ActorTransform.InverseTransformVectorNoScale(SocketWorldTransform.GetUnitAxis(EAxis::X));

					const FVector AxisX = FVector::ForwardVector;
					const FVector AxisY = FVector::RightVector;
					const FVector AxisZ = FVector::UpVector;

					const FVector2D CurrentCoords = FMath::GetAzimuthAndElevation(CurrentDirection, AxisX, AxisY, AxisZ);
					const FVector2D TargetCoords = FMath::GetAzimuthAndElevation(DirectionToTarget, AxisX, AxisY, AxisZ);
					const FVector BlendInput(
						FRotator::NormalizeAxis(FMath::RadiansToDegrees(TargetCoords.X - CurrentCoords.X)), 
						FRotator::NormalizeAxis(FMath::RadiansToDegrees(TargetCoords.Y - CurrentCoords.Y)),
						0.f);

					// Set X and Y, so ticking next frame is based on correct weights.
					X = BlendInput.X;
					Y = BlendInput.Y;

					// Generate BlendSampleDataCache from inputs.
					BlendSpace->GetSamplesFromBlendInput(BlendInput, BlendSampleDataCache);

#if ENABLE_DRAW_DEBUG
					if (CVarAimOffsetLookAtDebug.GetValueOnAnyThread() == 1)
					{
						DrawDebugLine(Component->GetWorld(), SocketWorldTransform.GetLocation(), TargetWorldTransform.GetLocation(), FColor::Green);
						DrawDebugLine(Component->GetWorld(), SocketWorldTransform.GetLocation(), SocketWorldTransform.GetLocation() + SocketWorldTransform.GetUnitAxis(EAxis::X) * (TargetWorldTransform.GetLocation() - SocketWorldTransform.GetLocation()).Size(), FColor::Red);
						DrawDebugCoordinateSystem(Component->GetWorld(), ActorTransform.GetLocation(), ActorTransform.GetRotation().Rotator(), 100.f);

						FString DebugString = FString::Printf(TEXT("Socket (X:%f, Y:%f), Target (X:%f, Y:%f), Result (X:%f, Y:%f)")
							, FMath::RadiansToDegrees(CurrentCoords.X)
							, FMath::RadiansToDegrees(CurrentCoords.Y)
							, FMath::RadiansToDegrees(TargetCoords.X)
							, FMath::RadiansToDegrees(TargetCoords.Y)
							, BlendInput.X
							, BlendInput.Y);
						GEngine->AddOnScreenDebugMessage(INDEX_NONE, 0.f, FColor::Red, DebugString, false);
					}
#endif // ENABLE_DRAW_DEBUG
				}
			}
		}
	}
}

void FAnimNode_AimOffsetLookAt::GatherDebugData(FNodeDebugData& DebugData)
{
	FString DebugLine = DebugData.GetNodeName(this);

	DebugLine += FString::Printf(TEXT("(Play Time: %.3f)"), InternalTimeAccumulator);
	DebugData.AddDebugItem(DebugLine);

	BasePose.GatherDebugData(DebugData);
}

FAnimNode_AimOffsetLookAt::FAnimNode_AimOffsetLookAt()
	: LODThreshold(INDEX_NONE)
	, Alpha(1.f)
{
}
