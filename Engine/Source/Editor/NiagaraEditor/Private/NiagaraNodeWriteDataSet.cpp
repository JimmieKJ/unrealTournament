// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "NiagaraEditorPrivatePCH.h"
#include "BlueprintGraphDefinitions.h"
#include "GraphEditorSettings.h"

#include "NiagaraNodeWriteDataSet.h"
#include "EdGraphSchema_Niagara.h"

#define LOCTEXT_NAMESPACE "NiagaraNodeWriteDataSet"

UNiagaraNodeWriteDataSet::UNiagaraNodeWriteDataSet(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
}

void UNiagaraNodeWriteDataSet::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	if (PropertyChangedEvent.Property != nullptr)
	{
		ReallocatePins();
	}
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

void UNiagaraNodeWriteDataSet::AllocateDefaultPins()
{
	const UEdGraphSchema_Niagara* Schema = GetDefault<UEdGraphSchema_Niagara>();

	if (DataSet.Type == ENiagaraDataSetType::Event)
	{
		UEdGraphPin* Pin = CreatePin(EGPD_Input, Schema->PC_Vector, TEXT(""), NULL, false, false, TEXT("Valid"));//TODO - CHANGE TO INT / BOOL / SCALAR
		Pin->bDefaultValueIsIgnored = true;
	}
	
	for (const FNiagaraVariableInfo& Var : Variables)
	{
		switch (Var.Type)
		{
		case ENiagaraDataType::Scalar:
		{
			CreatePin(EGPD_Input, Schema->PC_Float, TEXT(""), NULL, false, false, Var.Name.ToString());
		}
			break;
		case ENiagaraDataType::Vector:
		{
			CreatePin(EGPD_Input, Schema->PC_Vector, TEXT(""), NULL, false, false, Var.Name.ToString());
		}
			break;
		case ENiagaraDataType::Matrix:
		{
			CreatePin(EGPD_Input, Schema->PC_Matrix, TEXT(""), NULL, false, false, Var.Name.ToString());
		}
			break;
		};
	}
}

FText UNiagaraNodeWriteDataSet::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return FText::Format(LOCTEXT("NiagaraDataSetWriteFormat", "{0} Write"), FText::FromName(DataSet.Name));
}

FLinearColor UNiagaraNodeWriteDataSet::GetNodeTitleColor() const
{
	check(DataSet.Type == ENiagaraDataSetType::Event);//Implement other datasets
	return CastChecked<UEdGraphSchema_Niagara>(GetSchema())->NodeTitleColor_Event;
}

void UNiagaraNodeWriteDataSet::Compile(class INiagaraCompiler* Compiler, TArray<FNiagaraNodeResult>& Outputs)
{
	bool bError = false;

	if (DataSet.Type == ENiagaraDataSetType::Event)
	{
		//Compile the Emit pin.
		TNiagaraExprPtr EmitExpression = Compiler->CompilePin(Pins[0]);

		if (EmitExpression.IsValid())
		{
			//Test the Valid pin result against 0. Maybe just require a direct connection of 0 or 0xFFFFFFFF?
			FNiagaraVariableInfo Zero(TEXT("0.0, 0.0, 0.0, 0.0"), ENiagaraDataType::Vector);
			TNiagaraExprPtr ZeroContantExpression = Compiler->GetInternalConstant(Zero, FVector4(0.0f, 0.0f, 0.0f, 0.0f));
			TArray<TNiagaraExprPtr> ConditonInputs;
			ConditonInputs.Add(EmitExpression);
			ConditonInputs.Add(ZeroContantExpression);
			check(ZeroContantExpression.IsValid());
			TArray<TNiagaraExprPtr> ConditionOpOutputs;
			INiagaraCompiler::GreaterThan(Compiler, ConditonInputs, ConditionOpOutputs);
			TNiagaraExprPtr ValidExpr = ConditionOpOutputs[0];

			TArray<TNiagaraExprPtr> InputExpressions;
			for (int32 i = 0; i < Variables.Num(); ++i)
			{
				const FNiagaraVariableInfo& Var = Variables[i];
				UEdGraphPin* Pin = Pins[i + 1];//Pin[0] is emit
				check(Pin->Direction == EGPD_Input);

				TNiagaraExprPtr Result = Compiler->CompilePin(Pin);
				if (!Result.IsValid())
				{
					bError = true;
					Compiler->Error(FText::Format(LOCTEXT("DataSetWriteErrorFormat", "Error writing variable {0} to dataset {1}"), FText::FromName(DataSet.Name), Pin->PinFriendlyName), this, Pin);
				}

				InputExpressions.Add(Result);
			}

			//Gets the index to write to. 
			TNiagaraExprPtr IndexExpression = Compiler->AcquireSharedDataIndex(DataSet, true, ValidExpr);

			if (!bError)
			{
				check(Variables.Num() == InputExpressions.Num());
				for (int32 i = 0; i < Variables.Num(); ++i)
				{
					Outputs.Add(FNiagaraNodeResult(Compiler->SharedDataWrite(DataSet, Variables[i], IndexExpression, InputExpressions[i]), Pins[i + 1]));//Pin[0] is Emit
				}
			}
		}
	}
	else
	{
		check(false);//IMPLEMENT OTHER DATA SETS.
	}
}

#undef LOCTEXT_NAMESPACE





