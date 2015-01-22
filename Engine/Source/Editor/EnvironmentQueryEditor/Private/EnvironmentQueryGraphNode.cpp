// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnvironmentQueryEditorPrivatePCH.h"
#include "ScopedTransaction.h"

UEnvironmentQueryGraphNode::UEnvironmentQueryGraphNode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, NodeInstance(NULL)
	, EnvQueryNodeClass(NULL)
{
}

void UEnvironmentQueryGraphNode::PrepareForCopying()
{
	if (NodeInstance)
	{
		// Temporarily take ownership of the node instance, so that it is not deleted when cutting
		NodeInstance->Rename(NULL, this, REN_DontCreateRedirectors | REN_DoNotDirty );
	}
}

void UEnvironmentQueryGraphNode::PostEditImport()
{
	ResetNodeOwner();
}

void UEnvironmentQueryGraphNode::PostCopyNode()
{
	ResetNodeOwner();
}

void UEnvironmentQueryGraphNode::ResetNodeOwner()
{
	if (NodeInstance)
	{
		UEnvQuery* Query = Cast<UEnvQuery>(GetEnvironmentQueryGraph()->GetOuter());
		NodeInstance->Rename(NULL, Query, REN_DontCreateRedirectors | REN_DoNotDirty);
	}
}

FText UEnvironmentQueryGraphNode::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return FText::GetEmpty();
}

FText UEnvironmentQueryGraphNode::GetDescription() const
{
	return FText::GetEmpty();
}

UEdGraphPin* UEnvironmentQueryGraphNode::GetInputPin(int32 InputIndex) const
{
	check(InputIndex >= 0);

	for (int32 PinIndex = 0, FoundInputs = 0; PinIndex < Pins.Num(); PinIndex++)
	{
		if (Pins[PinIndex]->Direction == EGPD_Input)
		{
			if (InputIndex == FoundInputs)
			{
				return Pins[PinIndex];
			}
			else
			{
				FoundInputs++;
			}
		}
	}

	return NULL;
}

UEdGraphPin* UEnvironmentQueryGraphNode::GetOutputPin(int32 InputIndex) const
{
	check(InputIndex >= 0);

	for (int32 PinIndex = 0, FoundInputs = 0; PinIndex < Pins.Num(); PinIndex++)
	{
		if (Pins[PinIndex]->Direction == EGPD_Output)
		{
			if (InputIndex == FoundInputs)
			{
				return Pins[PinIndex];
			}
			else
			{
				FoundInputs++;
			}
		}
	}

	return NULL;
}

void UEnvironmentQueryGraphNode::AutowireNewNode(UEdGraphPin* FromPin)
{
	Super::AutowireNewNode(FromPin);

	if (FromPin != NULL)
	{
		if (GetSchema()->TryCreateConnection(FromPin, GetInputPin()))
		{
			FromPin->GetOwningNode()->NodeConnectionListChanged();
		}
	}
}


UEnvironmentQueryGraph* UEnvironmentQueryGraphNode::GetEnvironmentQueryGraph()
{
	return CastChecked<UEnvironmentQueryGraph>(GetGraph());
}

void UEnvironmentQueryGraphNode::NodeConnectionListChanged()
{
	GetEnvironmentQueryGraph()->UpdateAsset();
}

bool UEnvironmentQueryGraphNode::CanCreateUnderSpecifiedSchema(const UEdGraphSchema* DesiredSchema) const
{
	return DesiredSchema->GetClass()->IsChildOf(UEdGraphSchema_EnvironmentQuery::StaticClass());
}
