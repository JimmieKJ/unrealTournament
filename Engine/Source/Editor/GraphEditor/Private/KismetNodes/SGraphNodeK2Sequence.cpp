// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "KismetNodes/SGraphNodeK2Sequence.h"
#include "EdGraph/EdGraph.h"
#include "Widgets/SBoxPanel.h"
#include "GraphEditorSettings.h"
#include "K2Node_CommutativeAssociativeBinaryOperator.h"
#include "K2Node_DoOnceMultiInput.h"
#include "K2Node_ExecutionSequence.h"
#include "K2Node_MakeArray.h"


void SGraphNodeK2Sequence::Construct( const FArguments& InArgs, UK2Node* InNode )
{
	GraphNode = InNode;

	SetCursor( EMouseCursor::CardinalCross );

	UpdateGraphNode();
}

void SGraphNodeK2Sequence::CreateOutputSideAddButton(TSharedPtr<SVerticalBox> OutputBox)
{
	TSharedRef<SWidget> AddPinButton = AddPinButtonContent(
		NSLOCTEXT("SequencerNode", "SequencerNodeAddPinButton", "Add pin"),
		NSLOCTEXT("SequencerNode", "SequencerNodeAddPinButton_ToolTip", "Add new pin"));

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

FReply SGraphNodeK2Sequence::OnAddPin()
{
	if (UK2Node_ExecutionSequence* SequenceNode = Cast<UK2Node_ExecutionSequence>(GraphNode))
	{
		SequenceNode->AddPinToExecutionNode();
		UpdateGraphNode();
		GraphNode->GetGraph()->NotifyGraphChanged();
	}
	else if (UK2Node_MakeArray* MakeArrayNode = Cast<UK2Node_MakeArray>(GraphNode))
	{
		MakeArrayNode->AddInputPin();
		UpdateGraphNode();
		GraphNode->GetGraph()->NotifyGraphChanged();
	}
	else if (UK2Node_CommutativeAssociativeBinaryOperator* OperatorNode = Cast<UK2Node_CommutativeAssociativeBinaryOperator>(GraphNode))
	{
		OperatorNode->AddInputPin();
		UpdateGraphNode();
		GraphNode->GetGraph()->NotifyGraphChanged();
	}
	else if (UK2Node_DoOnceMultiInput* DoOnceMultiNode = Cast<UK2Node_DoOnceMultiInput>(GraphNode))
	{
		DoOnceMultiNode->AddInputPin();
		UpdateGraphNode();
		GraphNode->GetGraph()->NotifyGraphChanged();
	}

	return FReply::Handled();
}
