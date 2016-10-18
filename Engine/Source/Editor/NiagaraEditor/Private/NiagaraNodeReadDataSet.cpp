// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "NiagaraEditorPrivatePCH.h"
#include "BlueprintGraphDefinitions.h"
#include "GraphEditorSettings.h"

#include "NiagaraNodeReadDataSet.h"
#include "EdGraphSchema_Niagara.h"

#define LOCTEXT_NAMESPACE "NiagaraNodeDataSetRead"

UNiagaraNodeReadDataSet::UNiagaraNodeReadDataSet(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
}

void UNiagaraNodeReadDataSet::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	if (PropertyChangedEvent.Property != nullptr)
	{
		ReallocatePins();
	}
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

void UNiagaraNodeReadDataSet::AllocateDefaultPins()
{
	const UEdGraphSchema_Niagara* Schema = GetDefault<UEdGraphSchema_Niagara>();

	if (DataSet.Type == ENiagaraDataSetType::Event)
	{
		//Probably need this for all data set types tbh!
		UEdGraphPin* Pin = CreatePin(EGPD_Output, Schema->PC_Vector, TEXT(""), NULL, false, false, TEXT("Valid"));//TODO - CHANGE TO INT / BOOL / SCALAR
		Pin->bDefaultValueIsIgnored = true;
	}

	for (const FNiagaraVariableInfo& Var : Variables)
	{
		switch (Var.Type)
		{
		case ENiagaraDataType::Scalar:
		{
			CreatePin(EGPD_Output, Schema->PC_Float, TEXT(""), NULL, false, false, Var.Name.ToString());
		}
			break;
		case ENiagaraDataType::Vector:
		{
			CreatePin(EGPD_Output, Schema->PC_Vector, TEXT(""), NULL, false, false, Var.Name.ToString());
		}
			break;
		case ENiagaraDataType::Matrix:
		{
			CreatePin(EGPD_Output, Schema->PC_Matrix, TEXT(""), NULL, false, false, Var.Name.ToString());
		}
			break;
		};
	}
}

FText UNiagaraNodeReadDataSet::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return FText::Format(LOCTEXT("NiagaraDataSetReadFormat", "{0} Read"), FText::FromName(DataSet.Name));
}

FLinearColor UNiagaraNodeReadDataSet::GetNodeTitleColor() const
{
	check(DataSet.Type == ENiagaraDataSetType::Event);//Implement other datasets
	return CastChecked<UEdGraphSchema_Niagara>(GetSchema())->NodeTitleColor_Event;
}

void UNiagaraNodeReadDataSet::Compile(class INiagaraCompiler* Compiler, TArray<FNiagaraNodeResult>& Outputs)
{
	if(DataSet.Type == ENiagaraDataSetType::Event)
	{
		TNiagaraExprPtr IndexExpr = Compiler->ConsumeSharedDataIndex(DataSet, true);//Where do we decide wrapping?
		check(Variables.Num() == Pins.Num() - 1);//We should have a pin for each variable + 1 for the Valid pin.
		Outputs.Add(FNiagaraNodeResult(Compiler->SharedDataIndexIsValid(DataSet, IndexExpr), Pins[0]));
		for (int32 i = 0; i < Variables.Num(); ++i)
		{
			check(Variables[i].Type == ENiagaraDataType::Vector);//Only support vectors currently.
			Outputs.Add(FNiagaraNodeResult(Compiler->SharedDataRead(DataSet, Variables[i], IndexExpr), Pins[i+1]));
		}
	}
	else
	{
		Compiler->Error(LOCTEXT("UnImplDataSetTypeError", "This data set type hasn't been implemented."), this, nullptr);
	}
	
}

#undef LOCTEXT_NAMESPACE



