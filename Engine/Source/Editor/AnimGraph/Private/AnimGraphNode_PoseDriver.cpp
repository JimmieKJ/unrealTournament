// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "AnimGraphPrivatePCH.h"
#include "AnimGraphNode_PoseDriver.h"
#include "CompilerResultsLog.h"
#include "PropertyEditing.h"
#include "AnimationCustomVersion.h"
#include "ContentBrowserModule.h"
#include "Animation/PoseAsset.h"

#define LOCTEXT_NAMESPACE "A3Nodes"

UAnimGraphNode_PoseDriver::UAnimGraphNode_PoseDriver(const FObjectInitializer& ObjectInitializer)
	:Super(ObjectInitializer)
{

}

FText UAnimGraphNode_PoseDriver::GetTooltipText() const
{
	return LOCTEXT("UAnimGraphNode_PoseDriver_ToolTip", "Interpolate parameters between defined orientation poses.");
}

FText UAnimGraphNode_PoseDriver::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return GetControllerDescription();
}

FText UAnimGraphNode_PoseDriver::GetControllerDescription() const
{
	return LOCTEXT("PoseDriver", "Orientation Driver");
}

void UAnimGraphNode_PoseDriver::Draw(FPrimitiveDrawInterface* PDI, USkeletalMeshComponent* SkelMeshComp) const
{

}

void UAnimGraphNode_PoseDriver::DrawCanvas(FViewport& InViewport, FSceneView& View, FCanvas& Canvas, USkeletalMeshComponent * PreviewSkelMeshComp) const
{
	// Get the node we are debugging
	FAnimNode_PoseDriver* DebugNode = (FAnimNode_PoseDriver*)FindDebugAnimNode(PreviewSkelMeshComp);

	if (DebugNode == nullptr)
	{
		return;
	}

	const int MarginX = 60;
	const int XDelta = 10;

	int PosY = 60;
	const int YDelta = 20;

	// Display bone we are watching
	FText InterpItemText = FText::Format(LOCTEXT("BoneFormat", "Bone: {0}"), FText::FromName(DebugNode->SourceBone.BoneName));
	FCanvasTextItem InterpItem(FVector2D(MarginX, PosY), InterpItemText, GEngine->GetSmallFont(), FLinearColor::White);
	Canvas.DrawItem(InterpItem);
	PosY += YDelta;

	int32 PoseIndex = 0;
	for (const FPoseDriverPoseInfo& PoseInfo : DebugNode->PoseInfos)
	{
		FString PoseItemString = FString::Printf(TEXT("%s : %f"), *PoseInfo.PoseName.ToString(), PoseInfo.PoseWeight);
		FCanvasTextItem PoseItem(FVector2D(MarginX + XDelta, PosY), FText::FromString(PoseItemString), GEngine->GetSmallFont(), FLinearColor::White);
		Canvas.DrawItem(PoseItem);
		PosY += YDelta;

		PoseIndex++;
	}

	PosY += YDelta;

	// Parameters header
	FCanvasTextItem ParamsHeaderItem(FVector2D(MarginX, PosY), LOCTEXT("Parameters", "Parameters"), GEngine->GetSmallFont(), FLinearColor::White);
	Canvas.DrawItem(ParamsHeaderItem);
	PosY += YDelta;

	// Iterate over Parameters
	for (const FPoseDriverParam& Param : DebugNode->ResultParamSet.Params)
	{
		FString ParamItemString = FString::Printf(TEXT("%s : %f"), *Param.ParamInfo.Name.ToString(), Param.ParamValue);
		FCanvasTextItem ParamItem(FVector2D(MarginX + XDelta, PosY), FText::FromString(ParamItemString), GEngine->GetSmallFont(), FLinearColor::White);
		Canvas.DrawItem(ParamItem);
		PosY += YDelta;
	}
}


void UAnimGraphNode_PoseDriver::ValidateAnimNodeDuringCompilation(USkeleton* ForSkeleton, FCompilerResultsLog& MessageLog)
{
	if (ForSkeleton->GetReferenceSkeleton().FindBoneIndex(Node.SourceBone.BoneName) == INDEX_NONE)
	{
		MessageLog.Warning(*LOCTEXT("NoSourceBone", "@@ - You must pick a source bone as the Driver joint").ToString(), this);
	}

	Super::ValidateAnimNodeDuringCompilation(ForSkeleton, MessageLog);
}

void UAnimGraphNode_PoseDriver::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{

}


#undef LOCTEXT_NAMESPACE
