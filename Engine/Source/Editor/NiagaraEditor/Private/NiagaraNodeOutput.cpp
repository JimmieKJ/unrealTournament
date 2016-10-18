// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "NiagaraEditorPrivatePCH.h"
#include "BlueprintGraphDefinitions.h"
#include "GraphEditorSettings.h"

#include "NiagaraNodeOutput.h"
#include "EdGraphSchema_Niagara.h"

#define LOCTEXT_NAMESPACE "NiagaraNodeOutput"

UNiagaraNodeOutput::UNiagaraNodeOutput(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
}

void UNiagaraNodeOutput::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	if (PropertyChangedEvent.Property != nullptr)
	{
		ReallocatePins();
	}
	Super::PostEditChangeProperty(PropertyChangedEvent);
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

	bool bError = false;
	for (int32 i = 0; i < Outputs.Num(); ++i)
	{
		const FNiagaraVariableInfo& Out = Outputs[i];
		UEdGraphPin* Pin = Pins[i];
		check(Pin->Direction == EGPD_Input);

		TNiagaraExprPtr Result = Compiler->CompilePin(Pin);
		if (!Result.IsValid())
		{
			bError = true;
			Compiler->Error(FText::Format(LOCTEXT("OutputErrorFormat", "Error compiling attriubte {0}"), Pin->PinFriendlyName), this, Pin);
		}

		InputExpressions.Add(Result);
	}

	if (!bError)
	{
		check(Outputs.Num() == InputExpressions.Num());
		for (int32 i = 0; i < Outputs.Num(); ++i)
		{
			OutputExpressions.Add(FNiagaraNodeResult(Compiler->Output(Outputs[i], InputExpressions[i]), Pins[i]));
		}
	}
}

#undef LOCTEXT_NAMESPACE