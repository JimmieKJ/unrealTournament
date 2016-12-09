// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "AnimGraphNode_PoseDriver.h"
#include "Kismet2/CompilerResultsLog.h"
#include "AnimNodeEditModes.h"

#define LOCTEXT_NAMESPACE "A3Nodes"

UAnimGraphNode_PoseDriver::UAnimGraphNode_PoseDriver(const FObjectInitializer& ObjectInitializer)
	:Super(ObjectInitializer)
{

}

FText UAnimGraphNode_PoseDriver::GetTooltipText() const
{
	return LOCTEXT("UAnimGraphNode_PoseDriver_ToolTip", "Drive parameters base on a bones distance from a set of defined poses.");
}

FText UAnimGraphNode_PoseDriver::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("PoseDriver", "Pose Driver");
}

FText UAnimGraphNode_PoseDriver::GetMenuCategory() const
{
	return LOCTEXT("PoseAssetCategory_Label", "Poses");
}


void UAnimGraphNode_PoseDriver::ValidateAnimNodeDuringCompilation(USkeleton* ForSkeleton, FCompilerResultsLog& MessageLog)
{
	if (ForSkeleton->GetReferenceSkeleton().FindBoneIndex(Node.SourceBone.BoneName) == INDEX_NONE)
	{
		MessageLog.Warning(*LOCTEXT("NoSourceBone", "@@ - You must pick a source bone as the Driver joint").ToString(), this);
	}

	Super::ValidateAnimNodeDuringCompilation(ForSkeleton, MessageLog);
}

FEditorModeID UAnimGraphNode_PoseDriver::GetEditorMode() const
{
	return AnimNodeEditModes::PoseDriver;
}

#undef LOCTEXT_NAMESPACE
