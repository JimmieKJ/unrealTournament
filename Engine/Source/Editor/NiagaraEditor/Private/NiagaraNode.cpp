// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "NiagaraEditorPrivatePCH.h"
#include "NiagaraNode.h"
#include "EdGraphSchema_Niagara.h"

UNiagaraNode::UNiagaraNode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UNiagaraNode::ReallocatePins()
{
	Modify();

	// Move the existing pins to a saved array
	TArray<UEdGraphPin*> OldPins(Pins);
	Pins.Empty();

	// Recreate the new pins
	AllocateDefaultPins();

	// Copy the old pin data and remove it.
	for (int32 OldPinIndex = 0; OldPinIndex < OldPins.Num(); ++OldPinIndex)
	{
		UEdGraphPin* OldPin = OldPins[OldPinIndex];
		if (UEdGraphPin** MatchingNewPin = Pins.FindByPredicate([&](UEdGraphPin* Pin){ return Pin->Direction == OldPin->Direction && Pin->PinName == OldPin->PinName; }))
		{
			if (*MatchingNewPin && OldPin)
				(*MatchingNewPin)->CopyPersistentDataFromOldPin(*OldPin);
		}
		OldPin->Modify();
		OldPin->BreakAllPinLinks();
	}
	OldPins.Empty();

	GetGraph()->NotifyGraphChanged();
}

void UNiagaraNode::AutowireNewNode(UEdGraphPin* FromPin)
{
	Super::AutowireNewNode(FromPin);

	if (FromPin != nullptr)
	{
		const UEdGraphSchema_Niagara* Schema = CastChecked<UEdGraphSchema_Niagara>(GetSchema());
		check(Schema);
		
		ENiagaraDataType FromType = Schema->GetPinDataType(FromPin);

		//Find first of this nodes pins with the right type and direction.
		UEdGraphPin* FirstPinOfSameType = NULL;
		EEdGraphPinDirection DesiredDirection = FromPin->Direction == EGPD_Output ? EGPD_Input : EGPD_Output;
		for (UEdGraphPin* Pin : Pins)
		{
			ENiagaraDataType ToType = Schema->GetPinDataType(Pin);
			if (FromType == ToType && Pin->Direction == DesiredDirection)
			{
				FirstPinOfSameType = Pin;
				break;
			}			
		}

		if (FirstPinOfSameType && GetSchema()->TryCreateConnection(FromPin, FirstPinOfSameType))
		{
			FromPin->GetOwningNode()->NodeConnectionListChanged();
		}
	}
}

const UNiagaraGraph* UNiagaraNode::GetNiagaraGraph()const
{
	return CastChecked<UNiagaraGraph>(GetGraph());
}

UNiagaraGraph* UNiagaraNode::GetNiagaraGraph()
{
	return CastChecked<UNiagaraGraph>(GetGraph());
}

UNiagaraScriptSource* UNiagaraNode::GetSource()const
{
	return GetNiagaraGraph()->GetSource();
}

UEdGraphPin* UNiagaraNode::GetInputPin(int32 InputIndex) const
{
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

void UNiagaraNode::GetInputPins(TArray<class UEdGraphPin*>& OutInputPins) const
{
	OutInputPins.Empty();

	for (int32 PinIndex = 0; PinIndex < Pins.Num(); PinIndex++)
	{
		if (Pins[PinIndex]->Direction == EGPD_Input)
		{
			OutInputPins.Add(Pins[PinIndex]);
		}
	}
}

UEdGraphPin* UNiagaraNode::GetOutputPin(int32 OutputIndex) const
{
	for (int32 PinIndex = 0, FoundOutputs = 0; PinIndex < Pins.Num(); PinIndex++)
	{
		if (Pins[PinIndex]->Direction == EGPD_Output)
		{
			if (OutputIndex == FoundOutputs)
			{
				return Pins[PinIndex];
			}
			else
			{
				FoundOutputs++;
			}
		}
	}

	return NULL;
}

void UNiagaraNode::GetOutputPins(TArray<class UEdGraphPin*>& OutOutputPins) const
{
	OutOutputPins.Empty();

	for (int32 PinIndex = 0; PinIndex < Pins.Num(); PinIndex++)
	{
		if (Pins[PinIndex]->Direction == EGPD_Output)
		{
			OutOutputPins.Add(Pins[PinIndex]);
		}
	}
}