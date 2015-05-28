// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AnimGraphPrivatePCH.h"
#include "AnimGraphNode_BoneDrivenController.h"

#define LOCTEXT_NAMESPACE "A3Nodes"

UAnimGraphNode_BoneDrivenController::UAnimGraphNode_BoneDrivenController(const FObjectInitializer& ObjectInitializer)
	:Super(ObjectInitializer)
{

}

FText UAnimGraphNode_BoneDrivenController::GetTooltipText() const
{
	return LOCTEXT("UAnimGraphNode_BoneDrivenController_ToolTip", "Drives the transform of a bone using the transform of another");
}

FText UAnimGraphNode_BoneDrivenController::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	if ((Node.SourceBone.BoneName == NAME_None) && (Node.TargetBone.BoneName == NAME_None) && (TitleType == ENodeTitleType::ListView || TitleType == ENodeTitleType::MenuTitle))
	{
		return GetControllerDescription();
	}
	// @TODO: the bone can be altered in the property editor, so we have to 
	//        choose to mark this dirty when that happens for this to properly work
	else //if (!CachedNodeTitles.IsTitleCached(TitleType, this))
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("ControllerDesc"), GetControllerDescription());
		Args.Add(TEXT("SourceBone"), FText::FromName(Node.SourceBone.BoneName));
		Args.Add(TEXT("TargetBone"), FText::FromName(Node.TargetBone.BoneName));

		if (TitleType == ENodeTitleType::ListView || TitleType == ENodeTitleType::MenuTitle)
		{
			Args.Add(TEXT("Delim"), FText::FromString(TEXT(" - ")));
		}
		else
		{
			Args.Add(TEXT("Delim"), FText::FromString(TEXT("\n")));
		}

		// FText::Format() is slow, so we cache this to save on performance
		CachedNodeTitles.SetCachedTitle(TitleType, FText::Format(LOCTEXT("AnimGraphNode_BoneDrivenController_Title", "{ControllerDesc}{Delim}Driving Bone: {SourceBone}{Delim}Driven Bone: {TargetBone}"), Args), this);
	}	
	return CachedNodeTitles[TitleType];
}

FText UAnimGraphNode_BoneDrivenController::GetControllerDescription() const
{
	return LOCTEXT("BoneDrivenController", "Bone Driven Controller");
}

void UAnimGraphNode_BoneDrivenController::Draw(FPrimitiveDrawInterface* PDI, USkeletalMeshComponent* SkelMeshComp) const
{	
	static const float ArrowHeadWidth = 5.0f;
	static const float ArrowHeadHeight = 8.0f;

	int32 SourceIdx = SkelMeshComp->GetBoneIndex(Node.SourceBone.BoneName);
	int32 TargetIdx = SkelMeshComp->GetBoneIndex(Node.TargetBone.BoneName);

	if(SourceIdx != INDEX_NONE && TargetIdx != INDEX_NONE)
	{
		FTransform SourceTM = SkelMeshComp->GetSpaceBases()[SourceIdx] * SkelMeshComp->ComponentToWorld;
		FTransform TargetTM = SkelMeshComp->GetSpaceBases()[TargetIdx] * SkelMeshComp->ComponentToWorld;

		PDI->DrawLine(TargetTM.GetLocation(), SourceTM.GetLocation(), FLinearColor(0.0f, 0.0f, 1.0f), SDPG_Foreground, 0.5f);

		FVector ToTarget = TargetTM.GetTranslation() - SourceTM.GetTranslation();
		FVector UnitToTarget = ToTarget;
		UnitToTarget.Normalize();
		FVector Midpoint = SourceTM.GetTranslation() + 0.5f * ToTarget + 0.5f * UnitToTarget * ArrowHeadHeight;

		FVector YAxis;
		FVector ZAxis;
		UnitToTarget.FindBestAxisVectors(YAxis, ZAxis);
		FMatrix ArrowMatrix(UnitToTarget, YAxis, ZAxis, Midpoint);

		DrawConnectedArrow(PDI, ArrowMatrix, FLinearColor(0.0f, 1.0f, 0.0), ArrowHeadHeight, ArrowHeadWidth, SDPG_Foreground);

		PDI->DrawPoint(SourceTM.GetTranslation(), FLinearColor(0.8f, 0.8f, 0.2f), 5.0f, SDPG_Foreground);
		PDI->DrawPoint(SourceTM.GetTranslation() + ToTarget, FLinearColor(0.8f, 0.8f, 0.2f), 5.0f, SDPG_Foreground);
	}
}

#undef LOCTEXT_NAMESPACE
