// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "AnimationRuntime.h"
#include "Animation/BoneControllers/AnimNode_WheelHandler.h"
#include "GameFramework/WheeledVehicle.h"
#include "Vehicles/WheeledVehicleMovementComponent.h"
#include "Vehicles/VehicleWheel.h"

/////////////////////////////////////////////////////
// FAnimNode_WheelHandler

FAnimNode_WheelHandler::FAnimNode_WheelHandler()
{
}

void FAnimNode_WheelHandler::GatherDebugData(FNodeDebugData& DebugData)
{
	FString DebugLine = DebugData.GetNodeName(this);

	DebugLine += "(";
	AddDebugNodeData(DebugLine);
	DebugLine += ")";

	DebugData.AddDebugItem(DebugLine);
	for(const auto & WheelSim : WheelSimulators)
	{
		if (WheelSim.BoneReference.BoneIndex != INDEX_NONE)
		{
			DebugLine = FString::Printf(TEXT(" [Wheel Index : %d] Bone: %s , Rotation Offset : %s, Location Offset : %s"), 
				WheelSim.WheelIndex, *WheelSim.BoneReference.BoneName.ToString(), *WheelSim.RotOffset.ToString(), *WheelSim.LocOffset.ToString());
		}
		else
		{
			DebugLine = FString::Printf(TEXT(" [Wheel Index : %d] Bone: %s (invalid bone)"),
					WheelSim.WheelIndex, *WheelSim.BoneReference.BoneName.ToString());
		}

		DebugData.AddDebugItem(DebugLine);
	}

	ComponentPose.GatherDebugData(DebugData);
}

void FAnimNode_WheelHandler::EvaluateBoneTransforms(USkeletalMeshComponent* SkelComp, const FBoneContainer& RequiredBones, FA2CSPose& MeshBases, TArray<FBoneTransform>& OutBoneTransforms)
{
	check(OutBoneTransforms.Num() == 0);

	for(const auto & WheelSim : WheelSimulators)
	{
		if(WheelSim.BoneReference.IsValid(RequiredBones))
		{
			// the way we apply transform is same as FMatrix or FTransform
			// we apply scale first, and rotation, and translation
			// if you'd like to translate first, you'll need two nodes that first node does translate and second nodes to rotate. 
			FTransform NewBoneTM = MeshBases.GetComponentSpaceTransform(WheelSim.BoneReference.BoneIndex);

			FAnimationRuntime::ConvertCSTransformToBoneSpace(SkelComp, MeshBases, NewBoneTM, WheelSim.BoneReference.BoneIndex, BCS_ComponentSpace);
			
			// Apply rotation offset
			const FQuat BoneQuat(WheelSim.RotOffset);
			NewBoneTM.SetRotation(BoneQuat * NewBoneTM.GetRotation());

			// Apply loc offset
			NewBoneTM.AddToTranslation(WheelSim.LocOffset);

			// Convert back to Component Space.
			FAnimationRuntime::ConvertBoneSpaceTransformToCS(SkelComp, MeshBases, NewBoneTM, WheelSim.BoneReference.BoneIndex, BCS_ComponentSpace);

			// add back to it
			OutBoneTransforms.Add( FBoneTransform(WheelSim.BoneReference.BoneIndex, NewBoneTM) );
		}
	}
}

bool FAnimNode_WheelHandler::IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones) 
{
	// if both bones are valid
	for(const auto & WheelSim : WheelSimulators)
	{
		// if one of them is valid
		if (WheelSim.BoneReference.IsValid(RequiredBones) == true)
		{
			return true;
		}
	}

	return false;
}

void FAnimNode_WheelHandler::InitializeBoneReferences(const FBoneContainer& RequiredBones) 
{
	for (auto & WheelSim : WheelSimulators)
	{
		WheelSim.BoneReference.Initialize(RequiredBones);
	}

	// sort by bone indices
	WheelSimulators.Sort([](FWheelSimulator L, FWheelSimulator R) { return L.BoneReference.BoneIndex < R.BoneReference.BoneIndex; });
}

void FAnimNode_WheelHandler::Update(const FAnimationUpdateContext& Context)
{
	if(VehicleSimComponent)
	{
		for(auto & WheelSim : WheelSimulators)
		{
			if(VehicleSimComponent->Wheels.IsValidIndex(WheelSim.WheelIndex))
			{
				UVehicleWheel* Wheel = VehicleSimComponent->Wheels[WheelSim.WheelIndex];
				if(Wheel != nullptr)
				{
					WheelSim.RotOffset.Pitch	= Wheel->GetRotationAngle();
					WheelSim.RotOffset.Yaw		= Wheel->GetSteerAngle();
					WheelSim.RotOffset.Roll		= 0.f;

					WheelSim.LocOffset.X		= 0.f;
					WheelSim.LocOffset.Y		= 0.f;
					WheelSim.LocOffset.Z		= Wheel->GetSuspensionOffset();
				}
			}
		}
	}

	FAnimNode_SkeletalControlBase::Update(Context);
}

void FAnimNode_WheelHandler::Initialize(const FAnimationInitializeContext& Context)
{
	// TODO: only check vehicle anim instance
	// UVehicleAnimInstance
	AWheeledVehicle * Vehicle = Cast<AWheeledVehicle> ((CastChecked<USkeletalMeshComponent> (Context.AnimInstance->GetOuter()))->GetOwner());
	;
	// we only support vehicle for this class
	if(Vehicle != nullptr)
	{
		VehicleSimComponent = Vehicle->GetVehicleMovementComponent();

		int32 NumOfwheels = VehicleSimComponent->WheelSetups.Num();
		if(NumOfwheels > 0)
		{
			WheelSimulators.Empty(NumOfwheels);
			WheelSimulators.AddZeroed(NumOfwheels);
			// now add wheel data
			for(int32 WheelIndex = 0; WheelIndex<WheelSimulators.Num(); ++WheelIndex)
			{
				FWheelSimulator & WheelSim = WheelSimulators[WheelIndex];
				const FWheelSetup& WheelSetup = VehicleSimComponent->WheelSetups[WheelIndex];

				// set data
				WheelSim.WheelIndex = WheelIndex;
				WheelSim.BoneReference.BoneName = WheelSetup.BoneName;
				WheelSim.LocOffset = FVector::ZeroVector;
				WheelSim.RotOffset = FRotator::ZeroRotator;
			}
		}
	}

	FAnimNode_SkeletalControlBase::Initialize(Context);
}