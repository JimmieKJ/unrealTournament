// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "NiagaraEditorPrivatePCH.h"
#include "BlueprintGraphDefinitions.h"
#include "GraphEditorSettings.h"
#include "NiagaraScript.h"
#include "NiagaraNodeFunctionCall.h"
#include "NiagaraNodeInput.h"
#include "NiagaraNodeOutput.h"
#include "EdGraphSchema_Niagara.h"

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
	if (PropertyChangedEvent.Property != nullptr)
	{
		ReallocatePins();
	}
	Super::PostEditChangeProperty(PropertyChangedEvent);

	GetGraph()->NotifyGraphChanged();
}

void UNiagaraNodeFunctionCall::AllocateDefaultPins()
{
	if (!FunctionScript)
		return;

	const UEdGraphSchema_Niagara* Schema = CastChecked<UEdGraphSchema_Niagara>(GetSchema());
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