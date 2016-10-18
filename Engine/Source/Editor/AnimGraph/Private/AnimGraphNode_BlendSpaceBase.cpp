// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "AnimGraphPrivatePCH.h"
#include "AnimationGraphSchema.h"
#include "AnimGraphNode_BlendSpaceBase.h"
#include "AnimGraphNode_BlendSpacePlayer.h"
#include "AnimGraphNode_RotationOffsetBlendSpace.h"
#include "Animation/AimOffsetBlendSpace.h"
#include "Animation/AimOffsetBlendSpace1D.h"
#include "GraphEditorActions.h"

#define LOCTEXT_NAMESPACE "AnimGraphNode_BlendSpaceBase"

/////////////////////////////////////////////////////

// Action to add a sequence player node to the graph
struct FNewBlendSpacePlayerAction : public FEdGraphSchemaAction_K2NewNode
{
	FNewBlendSpacePlayerAction(class UBlendSpaceBase* BlendSpace)
		: FEdGraphSchemaAction_K2NewNode()
	{
		check(BlendSpace);

		const bool bIsAimOffset = BlendSpace->IsA(UAimOffsetBlendSpace::StaticClass()) || BlendSpace->IsA(UAimOffsetBlendSpace1D::StaticClass());
		FString NewTooltipDescription;
		if (bIsAimOffset)
		{
			UAnimGraphNode_RotationOffsetBlendSpace* Template = NewObject<UAnimGraphNode_RotationOffsetBlendSpace>();
			Template->Node.BlendSpace = BlendSpace;
			NodeTemplate = Template;
			NewTooltipDescription = TEXT("Evaluates an aim offset at a particular coordinate to produce a pose");
		}
		else
		{
			UAnimGraphNode_BlendSpacePlayer* Template = NewObject<UAnimGraphNode_BlendSpacePlayer>();
			Template->Node.BlendSpace = BlendSpace;
			NodeTemplate = Template;
			NewTooltipDescription = TEXT("Evaluates a blend space at a particular coordinate to produce a pose");
		}

		FText NewMenuDescription = NodeTemplate->GetNodeTitle(ENodeTitleType::ListView);

		UpdateSearchData(NewMenuDescription, NewTooltipDescription, LOCTEXT("Animation", "Animations"), FText::FromString(BlendSpace->GetPathName()));
	}
};

/////////////////////////////////////////////////////
// UAnimGraphNode_BlendSpaceBase

UAnimGraphNode_BlendSpaceBase::UAnimGraphNode_BlendSpaceBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

FLinearColor UAnimGraphNode_BlendSpaceBase::GetNodeTitleColor() const
{
	return FLinearColor(0.2f, 0.8f, 0.2f);
}

void UAnimGraphNode_BlendSpaceBase::CustomizePinData(UEdGraphPin* Pin, FName SourcePropertyName, int32 ArrayIndex) const
{
	UBlendSpaceBase * BlendSpace = GetBlendSpace();

	if (BlendSpace != NULL)
	{
		if (SourcePropertyName == TEXT("X"))
		{
			Pin->PinFriendlyName = FText::FromString(BlendSpace->GetBlendParameter(0).DisplayName);
		}
		else if (SourcePropertyName == TEXT("Y"))
		{
			Pin->PinFriendlyName = FText::FromString(BlendSpace->GetBlendParameter(1).DisplayName);
			Pin->bHidden = (BlendSpace->NumOfDimension == 1) ? 1 : 0;
		}
		else if (SourcePropertyName == TEXT("Z"))
		{
			Pin->PinFriendlyName = FText::FromString(BlendSpace->GetBlendParameter(2).DisplayName);
		}
	}
}


void UAnimGraphNode_BlendSpaceBase::PreloadRequiredAssets()
{
	PreloadObject(GetBlendSpace());

	Super::PreloadRequiredAssets();
}

void UAnimGraphNode_BlendSpaceBase::PostProcessPinName(const UEdGraphPin* Pin, FString& DisplayName) const
{
	if(Pin->Direction == EGPD_Input)
	{
		UBlendSpaceBase * BlendSpace = GetBlendSpace();

		if(BlendSpace != NULL)
		{
			if(Pin->PinName == TEXT("X"))
			{
				DisplayName = BlendSpace->GetBlendParameter(0).DisplayName;
			}
			else if(Pin->PinName == TEXT("Y"))
			{
				DisplayName = BlendSpace->GetBlendParameter(1).DisplayName;
			}
			else if(Pin->PinName == TEXT("Z"))
			{
				DisplayName = BlendSpace->GetBlendParameter(2).DisplayName;
			}
		}
	}

	Super::PostProcessPinName(Pin, DisplayName);
}

FText UAnimGraphNode_BlendSpaceBase::GetMenuCategory() const
{
	return LOCTEXT("BlendSpaceCategory_Label", "BlendSpaces");
}
#undef LOCTEXT_NAMESPACE