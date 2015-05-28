// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealEd.h"
#include "BlueprintGraphDefinitions.h"
#include "GraphEditorSettings.h"
#include "NiagaraScript.h"

void UNiagaraNodeFunctionCall::PostLoad()
{
	Super::PostLoad();

	if (FunctionScript)
	{
		FunctionScript->ConditionalPostLoad();
	}
}

void UNiagaraNodeFunctionCall::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	ReallocatePins();
}

void UNiagaraNodeFunctionCall::ReallocatePins()
{
	if (!FunctionScript)
		return;

	const UEdGraphSchema_Niagara* Schema = GetDefault<UEdGraphSchema_Niagara>();
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
	
	if (UNiagaraScriptSource* Source = Cast<UNiagaraScriptSource>(FunctionScript->Source))
	{
		if (UNiagaraGraph* Graph = Cast<UNiagaraGraph>(Source->NodeGraph))
		{
			if (UNiagaraNodeOutput* OutNode = Graph->FindOutputNode())
			{
				//Add the input pins.
				TArray<UNiagaraNodeInput*> InputNodes;
				Graph->FindInputNodes(InputNodes);

				for (UNiagaraNodeInput* InNode : InputNodes)
				{
					//Don't expose system constants as inputs.
					if (Schema->IsSystemConstant(InNode->Input))
						continue;

					check(InNode);
					switch (InNode->Input.Type)
					{
					case ENiagaraDataType::Scalar:
					{
						UEdGraphPin* NewPin = CreatePin(EGPD_Input, Schema->PC_Float, TEXT(""), NULL, false, false, InNode->Input.Name.ToString());
						NewPin->bDefaultValueIsIgnored = true;
					}
						break;
					case ENiagaraDataType::Vector:
					{
						UEdGraphPin* NewPin = CreatePin(EGPD_Input, Schema->PC_Vector, TEXT(""), NULL, false, false, InNode->Input.Name.ToString());
						NewPin->bDefaultValueIsIgnored = true;
					}
						break;
					case ENiagaraDataType::Matrix:
					{
						UEdGraphPin* NewPin = CreatePin(EGPD_Input, Schema->PC_Matrix, TEXT(""), NULL, false, false, InNode->Input.Name.ToString());
						NewPin->bDefaultValueIsIgnored = true;
					}
						break;
					};
				}

				//Add the output pins.
				for (FNiagaraVariableInfo& Output : OutNode->Outputs)
				{
					switch (Output.Type)
					{
					case ENiagaraDataType::Scalar:
					{						
						UEdGraphPin* NewPin = CreatePin(EGPD_Output, Schema->PC_Float, TEXT(""), NULL, false, false, Output.Name.ToString());
						NewPin->bDefaultValueIsIgnored = true;
					}
						break;
					case ENiagaraDataType::Vector:
					{						
						UEdGraphPin* NewPin = CreatePin(EGPD_Output, Schema->PC_Vector, TEXT(""), NULL, false, false, Output.Name.ToString());
						NewPin->bDefaultValueIsIgnored = true;
					}
						break;
					case ENiagaraDataType::Matrix:
					{
						UEdGraphPin* NewPin = CreatePin(EGPD_Output, Schema->PC_Matrix, TEXT(""), NULL, false, false, Output.Name.ToString());
						NewPin->bDefaultValueIsIgnored = true;
					}
						break;
					};
				}		
			}
		}
	}

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

void UNiagaraNodeFunctionCall::AllocateDefaultPins()
{
	ReallocatePins();
}

FText UNiagaraNodeFunctionCall::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return FunctionScript ? FText::FromString(FunctionScript->GetName()) : Super::GetNodeTitle(TitleType);
}

FText UNiagaraNodeFunctionCall::GetTooltipText()const
{
	return FunctionScript ? FText::FromString(FunctionScript->GetName()) : FText::FromString(TEXT("Funtion Call"));
}

FLinearColor UNiagaraNodeFunctionCall::GetNodeTitleColor() const
{
	return UEdGraphSchema_Niagara::NodeTitleColor_FunctionCall;
}

void UNiagaraNodeFunctionCall::Compile(class INiagaraCompiler* Compiler, TArray<FNiagaraNodeResult>& Outputs)
{
	//We don't actually compile function call nodes(yet). They are merged into the source graph. Effectively inlining vs branching.
	//When we support branching in the VM we can probably look at branching funtion calls.
	check(false);
}