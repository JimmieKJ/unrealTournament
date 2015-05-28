// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AnimGraphPrivatePCH.h"
#include "AnimGraphNode_LookAt.h"

/////////////////////////////////////////////////////
// UAnimGraphNode_LookAt

#define LOCTEXT_NAMESPACE "AnimGraph_LookAt"

UAnimGraphNode_LookAt::UAnimGraphNode_LookAt(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

FText UAnimGraphNode_LookAt::GetControllerDescription() const
{
	return LOCTEXT("LookAtNode", "Look At");
}

FText UAnimGraphNode_LookAt::GetTooltipText() const
{
	return LOCTEXT("AnimGraphNode_LookAt_Tooltip", "This node allow a bone to trace or follow another bone");
}

FText UAnimGraphNode_LookAt::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	if ((TitleType == ENodeTitleType::ListView || TitleType == ENodeTitleType::MenuTitle) && (Node.BoneToModify.BoneName == NAME_None))
	{
		return GetControllerDescription();
	}
	// @TODO: the bone can be altered in the property editor, so we have to 
	//        choose to mark this dirty when that happens for this to properly work
	else //if (!CachedNodeTitles.IsTitleCached(TitleType, this))
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("ControllerDescription"), GetControllerDescription());
		Args.Add(TEXT("BoneName"), FText::FromName(Node.BoneToModify.BoneName));

		// FText::Format() is slow, so we cache this to save on performance
		if (TitleType == ENodeTitleType::ListView || TitleType == ENodeTitleType::MenuTitle)
		{
			CachedNodeTitles.SetCachedTitle(TitleType, FText::Format(LOCTEXT("AnimGraphNode_LookAt_ListTitle", "{ControllerDescription} - Bone: {BoneName}"), Args), this);
		}
		else
		{
			CachedNodeTitles.SetCachedTitle(TitleType, FText::Format(LOCTEXT("AnimGraphNode_LookAt_Title", "{ControllerDescription}\nBone: {BoneName}"), Args), this);
		}
	}
	return CachedNodeTitles[TitleType];		
}

void UAnimGraphNode_LookAt::Draw(FPrimitiveDrawInterface* PDI, USkeletalMeshComponent* SkelMeshComp) const
{
	// this is not accurate debugging
	// since i can't access the transient data of run-time instance, I have to calculate data from other way
	// this technically means a frame delay, but it would be sufficient for debugging purpose. 
	FVector BoneLocation = SkelMeshComp->GetSocketLocation(Node.BoneToModify.BoneName);
	FVector TargetLocation = (Node.LookAtBone.BoneName != NAME_None)? SkelMeshComp->GetSocketLocation(Node.LookAtBone.BoneName) : Node.LookAtLocation;

	DrawWireStar(PDI, TargetLocation, 5.f, FLinearColor(1, 0, 0), SDPG_Foreground);
	DrawDashedLine(PDI, BoneLocation, TargetLocation, FLinearColor(1, 1, 0), 2.f, SDPG_Foreground);
}

#undef LOCTEXT_NAMESPACE
