// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	SoundClassGraphNode.cpp
=============================================================================*/

#include "UnrealEd.h"
#include "Sound/SoundClass.h"

#define LOCTEXT_NAMESPACE "SoundClassGraphNode"

/////////////////////////////////////////////////////
// USoundClassGraphNode

USoundClassGraphNode::USoundClassGraphNode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, ChildPin(NULL)
	, ParentPin(NULL)
{
}

bool USoundClassGraphNode::CheckRepresentsSoundClass()
{
	for (int32 ChildIndex = 0; ChildIndex < ChildPin->LinkedTo.Num(); ChildIndex++)
	{
		USoundClassGraphNode* ChildNode = CastChecked<USoundClassGraphNode>(ChildPin->LinkedTo[ChildIndex]->GetOwningNode());
		if (!SoundClass->ChildClasses.Contains(ChildNode->SoundClass))
		{
			return false;
		}
	}

	for (int32 ChildIndex = 0; ChildIndex < SoundClass->ChildClasses.Num(); ChildIndex++)
	{
		bool bFoundChild = false;
		for (int32 NodeChildIndex = 0; NodeChildIndex < ChildPin->LinkedTo.Num(); NodeChildIndex++)
		{
			USoundClassGraphNode* ChildNode = CastChecked<USoundClassGraphNode>(ChildPin->LinkedTo[NodeChildIndex]->GetOwningNode());
			if (ChildNode->SoundClass == SoundClass->ChildClasses[ChildIndex])
			{
				bFoundChild = true;
				break;
			}
		}

		if (!bFoundChild)
		{
			return false;
		}
	}

	return true;
}

FLinearColor USoundClassGraphNode::GetNodeTitleColor() const
{
	if (SoundClass && SoundClass->PassiveSoundMixModifiers.Num() > 0)
	{
		return FLinearColor::Green;
	}
	else
	{
		return Super::GetNodeTitleColor();
	}
}

void USoundClassGraphNode::AllocateDefaultPins()
{
	check(Pins.Num() == 0);

	ChildPin = CreatePin(EGPD_Output, TEXT("SoundClass"), TEXT(""), NULL, /*bIsArray=*/ false, /*bIsReference=*/ false, LOCTEXT("SoundClassChildren", "Children").ToString());
	ParentPin = CreatePin(EGPD_Input, TEXT("SoundClass"), TEXT(""), NULL, /*bIsArray=*/ false, /*bIsReference=*/ false, TEXT(""));
}

void USoundClassGraphNode::AutowireNewNode(UEdGraphPin* FromPin)
{
	if (FromPin != NULL)
	{
		const USoundClassGraphSchema* Schema = CastChecked<USoundClassGraphSchema>(GetSchema());

		if (FromPin->Direction == EGPD_Input)
		{
			Schema->TryCreateConnection(FromPin, ChildPin);
		}
		else
		{
			Schema->TryCreateConnection(FromPin, ParentPin);
		}
	}
}

bool USoundClassGraphNode::CanCreateUnderSpecifiedSchema(const UEdGraphSchema* Schema) const
{
	return Schema->IsA(USoundClassGraphSchema::StaticClass());
}

FText USoundClassGraphNode::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	if (SoundClass)
	{
		return FText::FromString(SoundClass->GetName());
	}
	else
	{
		return Super::GetNodeTitle(TitleType);
	}
}

bool USoundClassGraphNode::CanUserDeleteNode() const
{
	USoundClassGraph* SoundClassGraph = CastChecked<USoundClassGraph>(GetGraph());

	// Cannot remove the root node from the graph
	return SoundClass != SoundClassGraph->GetRootSoundClass();
}

#undef LOCTEXT_NAMESPACE
