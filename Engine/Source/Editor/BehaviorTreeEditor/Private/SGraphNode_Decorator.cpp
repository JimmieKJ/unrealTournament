// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "BehaviorTreeEditorPrivatePCH.h"
#include "SGraphNode_Decorator.h"

void SGraphNode_Decorator::Construct(const FArguments& InArgs, UBehaviorTreeDecoratorGraphNode_Decorator* InNode)
{
	this->GraphNode = InNode;

	this->SetCursor(EMouseCursor::CardinalCross);

	this->UpdateGraphNode();
}

FString SGraphNode_Decorator::GetNodeComment() const
{
	const UBehaviorTreeDecoratorGraphNode_Decorator* MyGraphNode = Cast<const UBehaviorTreeDecoratorGraphNode_Decorator>(GetNodeObj());
	const UBTDecorator* MyBTNode = MyGraphNode ? Cast<const UBTDecorator>(MyGraphNode->NodeInstance) : NULL;

	if (MyBTNode)
	{
		return MyBTNode->GetNodeName();
	}

	return SGraphNode::GetNodeComment();
}
