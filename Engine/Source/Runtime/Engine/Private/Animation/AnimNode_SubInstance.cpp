// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "Components/SkeletalMeshComponent.h"
#include "AnimInstanceProxy.h"
#include "Animation/AnimNode_SubInstance.h"
#include "Animation/AnimNode_SubInput.h"

FAnimNode_SubInstance::FAnimNode_SubInstance()
	: InstanceClass(nullptr)
	, InstanceToRun(nullptr)
{

}

void FAnimNode_SubInstance::Initialize(const FAnimationInitializeContext& Context)
{
	FAnimNode_Base::Initialize(Context);

	InPose.Initialize(Context);

	if(InstanceToRun)
	{
		InstanceToRun->InitializeAnimation();
	}
}

void FAnimNode_SubInstance::ValidateInstance()
{
	if(!InstanceToRun)
	{
		// Need an instance to run
		if(*InstanceClass)
		{
			InstanceToRun = NewObject<UAnimInstance>(GetTransientPackage(), InstanceClass);
		}
		else
		{
			UE_LOG(LogAnimation, Warning, TEXT("FAnimNode_SubInstance: Attempting to use a subgraph node without setting an instance class."));
		}
	}
}

void FAnimNode_SubInstance::CacheBones(const FAnimationCacheBonesContext& Context)
{

}

void FAnimNode_SubInstance::Update(const FAnimationUpdateContext& Context)
{
	InPose.Update(Context);
	EvaluateGraphExposedInputs.Execute(Context);

	if(InstanceToRun)
	{
		FAnimInstanceProxy& Proxy = InstanceToRun->GetProxyOnAnyThread<FAnimInstanceProxy>();

		// Only update if we've not had a single-threaded update already
		if(InstanceToRun->bNeedsUpdate)
		{
			Proxy.UpdateAnimation();
		}

		check(InstanceProperties.Num() == SubInstanceProperties.Num());
		for(int32 PropIdx = 0; PropIdx < InstanceProperties.Num(); ++PropIdx)
		{
			UProperty* CallerProperty = InstanceProperties[PropIdx];
			UProperty* SubProperty = SubInstanceProperties[PropIdx];

			check(CallerProperty && SubProperty);

			uint8* SrcPtr = CallerProperty->ContainerPtrToValuePtr<uint8>(Context.AnimInstanceProxy->GetAnimInstanceObject());
			uint8* DestPtr = SubProperty->ContainerPtrToValuePtr<uint8>(InstanceToRun);

			CallerProperty->CopyCompleteValue(DestPtr, SrcPtr);
		}
	}
}

void FAnimNode_SubInstance::Evaluate(FPoseContext& Output)
{
	if(InstanceToRun)
	{
		InPose.Evaluate(Output);

		FAnimInstanceProxy& Proxy = InstanceToRun->GetProxyOnAnyThread<FAnimInstanceProxy>();
		FAnimNode_SubInput* InputNode = Proxy.SubInstanceInputNode;

		if(InputNode)
		{
			InputNode->InputPose.CopyBonesFrom(Output.Pose);
			InputNode->InputCurve.CopyFrom(Output.Curve);
		}

		InstanceToRun->ParallelEvaluateAnimation(false, nullptr, BoneTransforms, BlendedCurve);

		for(const FCompactPoseBoneIndex BoneIndex : Output.Pose.ForEachBoneIndex())
		{
			FMeshPoseBoneIndex MeshBoneIndex = Output.Pose.GetBoneContainer().MakeMeshPoseIndex(BoneIndex);
			Output.Pose[BoneIndex] = BoneTransforms[MeshBoneIndex.GetInt()];
		}

		Output.Curve.CopyFrom(BlendedCurve);
	}
	else
	{
		Output.ResetToRefPose();
	}

}

void FAnimNode_SubInstance::GatherDebugData(FNodeDebugData& DebugData)
{
	// Add our entry
	FString DebugLine = DebugData.GetNodeName(this);

	// Gather data from the sub instance
	if(InstanceToRun)
	{
		FAnimInstanceProxy& Proxy = InstanceToRun->GetProxyOnAnyThread<FAnimInstanceProxy>();
		Proxy.GatherDebugData(DebugData);
	}

	// Pass to next
	InPose.GatherDebugData(DebugData);
}

bool FAnimNode_SubInstance::HasPreUpdate() const
{
	return true;
}

void FAnimNode_SubInstance::PreUpdate(const UAnimInstance* InAnimInstance)
{
	if(USkeletalMeshComponent* SkelComp = InAnimInstance->GetSkelMeshComponent())
	{
		USkeletalMesh* SkelMesh = SkelComp->SkeletalMesh;

		const int32 NumTransforms = SkelComp->GetComponentSpaceTransforms().Num();
		BoneTransforms.Empty(NumTransforms);
		BoneTransforms.AddZeroed(NumTransforms);
	}
}

void FAnimNode_SubInstance::RootInitialize(const FAnimInstanceProxy* InProxy)
{
	if(*InstanceClass)
	{
		USkeletalMeshComponent* MeshComp = InProxy->GetSkelMeshComponent();
		check(MeshComp);

		// Full reinit, kill old instances
		if(InstanceToRun)
		{
			MeshComp->SubInstances.Remove(InstanceToRun);
			InstanceToRun->MarkPendingKill();
			InstanceToRun = nullptr;
		}

		// Need an instance to run
		InstanceToRun = NewObject<UAnimInstance>(MeshComp, InstanceClass);

		MeshComp->SubInstances.Add(InstanceToRun);

		// Build property lists
		InstanceProperties.Empty();
		SubInstanceProperties.Empty();

		check(SourcePropertyNames.Num() == DestPropertyNames.Num());

		for(int32 Idx = 0; Idx < SourcePropertyNames.Num(); ++Idx)
		{
			FName& SourceName = SourcePropertyNames[Idx];
			FName& DestName = DestPropertyNames[Idx];

			UClass* SourceClass = IAnimClassInterface::GetActualAnimClass(InProxy->GetAnimClassInterface());

			UProperty* SourceProperty = FindField<UProperty>(SourceClass, SourceName);
			UProperty* DestProperty = FindField<UProperty>(*InstanceClass, DestName);

			check(SourceProperty && DestProperty);

			InstanceProperties.Add(SourceProperty);
			SubInstanceProperties.Add(DestProperty);
		}
		
	}
	else if(InstanceToRun)
	{
		// We have an instance but no instance class
		TeardownInstance();
	}
}

void FAnimNode_SubInstance::TeardownInstance()
{
	InstanceToRun->UninitializeAnimation();
	InstanceToRun = nullptr;
}

