// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealEd.h"
#include "BlueprintGraphDefinitions.h"
#include "GraphEditorSettings.h"

UNiagaraNodeOutput::UNiagaraNodeOutput(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
}

void UNiagaraNodeOutput::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	ReallocatePins();
}

void UNiagaraNodeOutput::ReallocatePins()
{
	Modify();

	// Break any links to 'orphan' pins
	for (int32 PinIndex = 0; PinIndex < Pins.Num(); ++PinIndex)
	{
		UEdGraphPin* Pin = Pins[PinIndex];
		TArray<class UEdGraphPin*>& LinkedToRef = Pin->LinkedTo;
		for (int32 LinkIdx = 0; LinkIdx < LinkedToRef.Num(); LinkIdx++)
		{
			UEdGraphPin* OtherPin = LinkedToRef[LinkIdx];
			// If we are linked to a pin that its owner doesn't know about, break that link
			if (!OtherPin->GetOwningNode()->Pins.Contains(OtherPin))
			{
				Pin->LinkedTo.Remove(OtherPin);
			}
		}
	}

	// Store the old Input and Output pins
	TArray<UEdGraphPin*> OldInputPins;
	TArray<UEdGraphPin*> OldOutputPins;
	GetInputPins(OldInputPins);
	GetOutputPins(OldOutputPins);

	// Move the existing pins to a saved array
	TArray<UEdGraphPin*> OldPins(Pins);
	Pins.Empty();

	// Recreate the new pins
	AllocateDefaultPins();

	// Get new Input and Output pins
	TArray<UEdGraphPin*> NewInputPins;
	TArray<UEdGraphPin*> NewOutputPins;
	GetInputPins(NewInputPins);
	GetOutputPins(NewOutputPins);

	for (int32 PinIndex = 0; PinIndex < OldInputPins.Num(); PinIndex++)
	{
		if (PinIndex < NewInputPins.Num())
		{
			NewInputPins[PinIndex]->CopyPersistentDataFromOldPin(*OldInputPins[PinIndex]);
		}
	}

	for (int32 PinIndex = 0; PinIndex < OldOutputPins.Num(); PinIndex++)
	{
		if (PinIndex < NewOutputPins.Num())
		{
			NewOutputPins[PinIndex]->CopyPersistentDataFromOldPin(*OldOutputPins[PinIndex]);
		}
	}

	OldInputPins.Empty();
	OldOutputPins.Empty();

	// Throw away the original pins
	for (int32 OldPinIndex = 0; OldPinIndex < OldPins.Num(); ++OldPinIndex)
	{
		UEdGraphPin* OldPin = OldPins[OldPinIndex];
		OldPin->Modify();
		OldPin->BreakAllPinLinks();

#if 0
		UEdGraphNode::ReturnPinToPool(OldPin);
#else
		OldPin->Rename(NULL, GetTransientPackage(), REN_None);
		OldPin->RemoveFromRoot();
		OldPin->MarkPendingKill();
#endif
	}
	OldPins.Empty();

	GetGraph()->NotifyGraphChanged();
}

void UNiagaraNodeOutput::AllocateDefaultPins()
{
	const UEdGraphSchema_Niagara* Schema = GetDefault<UEdGraphSchema_Niagara>();
	for (const FNiagaraVariableInfo& Output : Outputs)
	{
		switch (Output.Type)
		{
		case ENiagaraDataType::Scalar:
		{
			CreatePin(EGPD_Input, Schema->PC_Float, TEXT(""), NULL, false, false, Output.Name.ToString());
		}
			break;
		case ENiagaraDataType::Vector:
		{
			CreatePin(EGPD_Input, Schema->PC_Vector, TEXT(""), NULL, false, false, Output.Name.ToString());
		}
			break;
		case ENiagaraDataType::Matrix:
		{
			CreatePin(EGPD_Input, Schema->PC_Matrix, TEXT(""), NULL, false, false, Output.Name.ToString());
		}
			break;
		case ENiagaraDataType::Curve:
		{
			 CreatePin(EGPD_Input, Schema->PC_Curve, TEXT(""), NULL, false, false, Output.Name.ToString());
		}
			break;
		};
	}
}

FText UNiagaraNodeOutput::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return NSLOCTEXT("NiagaraNode", "Output", "Output");
}

FLinearColor UNiagaraNodeOutput::GetNodeTitleColor() const
{
	return CastChecked<UEdGraphSchema_Niagara>(GetSchema())->NodeTitleColor_Attribute;
}

void UNiagaraNodeOutput::Compile(class INiagaraCompiler* Compiler, TArray<FNiagaraNodeResult>& OutputExpressions)
{
	TArray<TNiagaraExprPtr> InputExpressions;

	for (int32 i = 0; i < Outputs.Num(); ++i)
	{
		const FNiagaraVariableInfo& Out = Outputs[i];
		UEdGraphPin* Pin = Pins[i];
		check(Pin->Direction == EGPD_Input);

		TNiagaraExprPtr Result = Compiler->CompilePin(Pin);
		if (!Result.IsValid())
		{
			//The pin was not connected so pass through the initial value of the attribute.
			Result = Compiler->GetAttribute(Out);
		}

		InputExpressions.Add(Result);
	}

	check(Outputs.Num() == InputExpressions.Num());
	for (int32 i = 0; i < Outputs.Num(); ++i)
	{
		OutputExpressions.Add(FNiagaraNodeResult(Compiler->Output(Outputs[i], InputExpressions[i]), Pins[i]));
	}
}