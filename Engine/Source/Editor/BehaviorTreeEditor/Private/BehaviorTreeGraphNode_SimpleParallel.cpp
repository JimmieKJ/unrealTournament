// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "BehaviorTreeEditorPrivatePCH.h"
#include "Internationalization.h" // for LOCTEXT()

#define LOCTEXT_NAMESPACE "BehaviorTreeEditor"

UBehaviorTreeGraphNode_SimpleParallel::UBehaviorTreeGraphNode_SimpleParallel(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
}

void UBehaviorTreeGraphNode_SimpleParallel::AllocateDefaultPins()
{
	CreatePin(EGPD_Input, UBehaviorTreeEditorTypes::PinCategory_MultipleNodes, TEXT(""), NULL, false, false, TEXT("In"));
	
	CreatePin(EGPD_Output, UBehaviorTreeEditorTypes::PinCategory_SingleTask, TEXT(""), NULL, false, false, TEXT("Task"));
	CreatePin(EGPD_Output, UBehaviorTreeEditorTypes::PinCategory_SingleNode, TEXT(""), NULL, false, false, TEXT("Out"));
}

void UBehaviorTreeGraphNode_SimpleParallel::GetPinHoverText(const UEdGraphPin& Pin, FString& HoverTextOut) const
{
	ensure(Pin.GetOwningNode() == this);
	if (Pin.Direction == EGPD_Output)
	{
		if (Pin.PinType.PinCategory == UBehaviorTreeEditorTypes::PinCategory_SingleTask)
		{
			HoverTextOut = LOCTEXT("PinHoverParallelMain","Main task of parrallel node").ToString();
		}
		else
		{
			HoverTextOut = LOCTEXT("PinHoverParallelBackground","Nodes running in the background, while main task is active").ToString();
		}
	}
}

#undef LOCTEXT_NAMESPACE
