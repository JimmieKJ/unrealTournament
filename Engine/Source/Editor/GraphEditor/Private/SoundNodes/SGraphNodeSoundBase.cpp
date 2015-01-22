// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "GraphEditorCommon.h"
#include "SoundDefinitions.h"
#include "SGraphNodeSoundBase.h"
//#include "IDocumentation.h"
#include "ScopedTransaction.h"

/////////////////////////////////////////////////////
// SGraphNodeSoundBase

void SGraphNodeSoundBase::Construct(const FArguments& InArgs, class USoundCueGraphNode* InNode)
{
	this->GraphNode = InNode;
	this->SoundNode = InNode;

	this->SetCursor(EMouseCursor::CardinalCross);

	this->UpdateGraphNode();
}

void SGraphNodeSoundBase::CreateOutputSideAddButton(TSharedPtr<SVerticalBox> OutputBox)
{
	TSharedRef<SWidget> AddPinButton = AddPinButtonContent(
		NSLOCTEXT("SoundNode", "SoundNodeAddPinButton", "Add input"),
		NSLOCTEXT("SoundNode", "SoundNodeAddPinButton_Tooltip", "Adds an input to the sound node")
	);

	FMargin AddPinPadding = Settings->GetOutputPinPadding();
	AddPinPadding.Top += 6.0f;

	OutputBox->AddSlot()
	.AutoHeight()
	.VAlign(VAlign_Center)
	.Padding(AddPinPadding)
	[
		AddPinButton
	];
}

EVisibility SGraphNodeSoundBase::IsAddPinButtonVisible() const
{
	EVisibility Visibility = SGraphNode::IsAddPinButtonVisible();
	if (Visibility == EVisibility::Visible)
	{
		if (!SoundNode->CanAddInputPin())
		{
			Visibility = EVisibility::Collapsed;
		}
	}
	return Visibility;
}

FReply SGraphNodeSoundBase::OnAddPin()
{
	SoundNode->AddInputPin();

	return FReply::Handled();
}
