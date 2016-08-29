// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "AnimGraphPrivatePCH.h"
#include "AnimGraphNode_Fabrik.h"
#include "Animation/AnimInstance.h"

/////////////////////////////////////////////////////
// UAnimGraphNode_Fabrik 

#define LOCTEXT_NAMESPACE "A3Nodes"

UAnimGraphNode_Fabrik::UAnimGraphNode_Fabrik(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
}

FText UAnimGraphNode_Fabrik::GetControllerDescription() const
{
	return LOCTEXT("Fabrik", "FABRIK");
}

void UAnimGraphNode_Fabrik::Draw(FPrimitiveDrawInterface* PDI, USkeletalMeshComponent * PreviewSkelMeshComp) const
{
	if(PreviewSkelMeshComp)
	{
		if(FAnimNode_Fabrik* ActiveNode = GetActiveInstanceNode<FAnimNode_Fabrik>(PreviewSkelMeshComp->GetAnimInstance()))
		{
			ActiveNode->ConditionalDebugDraw(PDI, PreviewSkelMeshComp);
		}
	}
}

FText UAnimGraphNode_Fabrik::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return GetControllerDescription();
}

FVector UAnimGraphNode_Fabrik::GetWidgetLocation(const USkeletalMeshComponent* SkelComp, FAnimNode_SkeletalControlBase* AnimNode)
{
	FName& BoneName = Node.EffectorTransformBone.BoneName;
	FVector Location = Node.EffectorTransform.GetLocation();
	EBoneControlSpace Space = Node.EffectorTransformSpace;
	FVector WidgetLoc = ConvertWidgetLocation(SkelComp, AnimNode->ForwardedPose, BoneName, Location, Space);
	return WidgetLoc;
}

int32 UAnimGraphNode_Fabrik::GetWidgetMode(const USkeletalMeshComponent* SkelComp)
{
	uint32 BoneIndex = SkelComp->GetBoneIndex(Node.EffectorTransformBone.BoneName);

	if (BoneIndex != INDEX_NONE)
	{
		return FWidget::WM_Translate;
	}

	return FWidget::WM_None;
}

FName UAnimGraphNode_Fabrik::FindSelectedBone()
{
	return Node.EffectorTransformBone.BoneName;
}

void UAnimGraphNode_Fabrik::DoTranslation(const USkeletalMeshComponent* SkelComp, FVector& Drag, FAnimNode_Base* InOutAnimNode)
{
	FAnimNode_Fabrik* Fabrik = static_cast<FAnimNode_Fabrik*>(InOutAnimNode);

	if (Fabrik)
	{
		FVector Offset = ConvertCSVectorToBoneSpace(SkelComp, Drag, Fabrik->ForwardedPose, Node.EffectorTransformBone.BoneName, Node.EffectorTransformSpace);

		Fabrik->EffectorTransform.AddToTranslation(Offset);
		// copy same value to internal node for data consistency
		Node.EffectorTransform.SetTranslation(Fabrik->EffectorTransform.GetTranslation());
	}
}

void UAnimGraphNode_Fabrik::CopyNodeDataTo(FAnimNode_Base* AnimNode)
{
	FAnimNode_Fabrik* Fabrik = static_cast<FAnimNode_Fabrik*>(AnimNode);

	// copies Pin values from the internal node to get data which are not compiled yet
	Fabrik->EffectorTransform = Node.EffectorTransform;
}

void UAnimGraphNode_Fabrik::CopyNodeDataFrom(const FAnimNode_Base* InNewAnimNode)
{
	const FAnimNode_Fabrik* Fabrik = static_cast<const FAnimNode_Fabrik*>(InNewAnimNode);

	// copies Pin data from updated values
	Node.EffectorTransform = Fabrik->EffectorTransform;
}

#undef LOCTEXT_NAMESPACE