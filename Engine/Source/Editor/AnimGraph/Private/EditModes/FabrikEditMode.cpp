// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "EditModes/FabrikEditMode.h"
#include "AnimGraphNode_Fabrik.h"
#include "IPersonaPreviewScene.h"
#include "Animation/DebugSkelMeshComponent.h"

void FFabrikEditMode::EnterMode(class UAnimGraphNode_Base* InEditorNode, struct FAnimNode_Base* InRuntimeNode)
{
	RuntimeNode = static_cast<FAnimNode_Fabrik*>(InRuntimeNode);
	GraphNode = CastChecked<UAnimGraphNode_Fabrik>(InEditorNode);

	FAnimNodeEditMode::EnterMode(InEditorNode, InRuntimeNode);
}

void FFabrikEditMode::ExitMode()
{
	RuntimeNode = nullptr;
	GraphNode = nullptr;

	FAnimNodeEditMode::ExitMode();
}

FVector FFabrikEditMode::GetWidgetLocation() const
{
	USkeletalMeshComponent* SkelComp = GetAnimPreviewScene().GetPreviewMeshComponent();

	FName& BoneName = GraphNode->Node.EffectorTransformBone.BoneName;
	FVector Location = GraphNode->Node.EffectorTransform.GetLocation();
	EBoneControlSpace Space = GraphNode->Node.EffectorTransformSpace;
	FVector WidgetLoc = ConvertWidgetLocation(SkelComp, RuntimeNode->ForwardedPose, BoneName, Location, Space);
	return WidgetLoc;
}

FWidget::EWidgetMode FFabrikEditMode::GetWidgetMode() const
{
	USkeletalMeshComponent* SkelComp = GetAnimPreviewScene().GetPreviewMeshComponent();
	int32 BoneIndex = SkelComp->GetBoneIndex(GraphNode->Node.EffectorTransformBone.BoneName);

	if (BoneIndex != INDEX_NONE)
	{
		return FWidget::WM_Translate;
	}

	return FWidget::WM_None;
}

FName FFabrikEditMode::GetSelectedBone() const
{
	return GraphNode->Node.EffectorTransformBone.BoneName;
}

void FFabrikEditMode::DoTranslation(FVector& InTranslation)
{
	USkeletalMeshComponent* SkelComp = GetAnimPreviewScene().GetPreviewMeshComponent();
	FVector Offset = ConvertCSVectorToBoneSpace(SkelComp, InTranslation, RuntimeNode->ForwardedPose, GraphNode->Node.EffectorTransformBone.BoneName, GraphNode->Node.EffectorTransformSpace);

	RuntimeNode->EffectorTransform.AddToTranslation(Offset);
	GraphNode->Node.EffectorTransform.SetTranslation(RuntimeNode->EffectorTransform.GetTranslation());
}
