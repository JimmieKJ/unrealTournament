// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "NiagaraEditorPrivatePCH.h"
#include "BlueprintGraphDefinitions.h"
#include "GraphEditorSettings.h"

#include "NiagaraNodeInput.h"
#include "EdGraphSchema_Niagara.h"

UNiagaraNodeInput::UNiagaraNodeInput(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
, Input(NAME_None, ENiagaraDataType::Vector)
, FloatDefault(1.0f)
, VectorDefault(1.0f, 1.0f, 1.0f, 1.0f)
, MatrixDefault(FMatrix::Identity)
, bCanBeExposed(true)
, bExposeWhenConstant(true)
{
}

void UNiagaraNodeInput::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	if (PropertyChangedEvent.Property != nullptr)
	{
		ReallocatePins();
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

void UNiagaraNodeInput::AllocateDefaultPins()
{
	const UEdGraphSchema_Niagara* Schema = GetDefault<UEdGraphSchema_Niagara>();
	switch (Input.Type)
	{
		case ENiagaraDataType::Scalar:
		{
			CreatePin(EGPD_Output, Schema->PC_Float, TEXT(""), NULL, false, false, TEXT("Input"));
		}
			break;
		case ENiagaraDataType::Vector:
		{
			CreatePin(EGPD_Output, Schema->PC_Vector, TEXT(""), NULL, false, false, TEXT("Input"));
		}
			break;
		case ENiagaraDataType::Matrix:
		{
			CreatePin(EGPD_Output, Schema->PC_Matrix, TEXT(""), NULL, false, false, TEXT("Input"));
		}
			break;
	};
}

FText UNiagaraNodeInput::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return FText::FromName(Input.Name);
}

FLinearColor UNiagaraNodeInput::GetNodeTitleColor() const
{
	const UEdGraphSchema_Niagara* Schema = CastChecked<UEdGraphSchema_Niagara>(GetSchema());
	if (IsConstant())
	{
		return Schema->IsSystemConstant(Input) ? 
			CastChecked<UEdGraphSchema_Niagara>(GetSchema())->NodeTitleColor_SystemConstant :
			CastChecked<UEdGraphSchema_Niagara>(GetSchema())->NodeTitleColor_Constant ;
	}
	else
	{
		return CastChecked<UEdGraphSchema_Niagara>(GetSchema())->NodeTitleColor_Attribute;
	}
}

void UNiagaraNodeInput::AutowireNewNode(UEdGraphPin* FromPin)
{
	if (FromPin != nullptr)
	{
		const UEdGraphSchema_Niagara* Schema = CastChecked<UEdGraphSchema_Niagara>(GetSchema());
		check(Schema);
		if (Input.Name == NAME_None)
		{
			Input.Name = FName(*FromPin->PinName);
			Input.Type = Schema->GetPinDataType(FromPin);
			ReallocatePins();
		}
		check(Pins.Num() == 1 && Pins[0] != NULL);
		
		if (GetSchema()->TryCreateConnection(FromPin, Pins[0]))
		{
			FromPin->GetOwningNode()->NodeConnectionListChanged();
		}
	}
}

void UNiagaraNodeInput::Compile(class INiagaraCompiler* Compiler, TArray<FNiagaraNodeResult>& Outputs)
{
	//Input nodes in scripts being used as functions by other scripts will actually never get this far.
	//The compiler will find them and replace them with direct links between the pins going in and the nodes connected to the corresponding input.

	const UEdGraphSchema_Niagara* Schema = CastChecked<UEdGraphSchema_Niagara>(GetSchema());
	if (IsConstant())
	{
		if (IsExposedConstant() || Schema->IsSystemConstant(Input))
		{
			//treat this input as an external constant.
			switch (Input.Type)
			{
			case ENiagaraDataType::Scalar:	Outputs.Add(FNiagaraNodeResult(Compiler->GetExternalConstant(Input, FloatDefault), Pins[0]));	break;
			case ENiagaraDataType::Vector:	Outputs.Add(FNiagaraNodeResult(Compiler->GetExternalConstant(Input, VectorDefault), Pins[0]));	break;
			case ENiagaraDataType::Matrix:	Outputs.Add(FNiagaraNodeResult(Compiler->GetExternalConstant(Input, MatrixDefault), Pins[0]));	break;
			}
		}
		else
		{
			//treat this input as an internal constant.
			switch (Input.Type)
			{
			case ENiagaraDataType::Scalar:	Outputs.Add(FNiagaraNodeResult(Compiler->GetInternalConstant(Input, FloatDefault), Pins[0]));	break;
			case ENiagaraDataType::Vector:	Outputs.Add(FNiagaraNodeResult(Compiler->GetInternalConstant(Input, VectorDefault), Pins[0]));	break;
			case ENiagaraDataType::Matrix:	Outputs.Add(FNiagaraNodeResult(Compiler->GetInternalConstant(Input, MatrixDefault), Pins[0]));	break;
			}
		}
	}
	else
	{
		//This input is an attribute.
		Outputs.Add(FNiagaraNodeResult(Compiler->GetAttribute(Input), Pins[0]));
	}
}

bool UNiagaraNodeInput::IsConstant()const
{
	//This input is a constant if it does not match an attribute for the current script.
	return GetNiagaraGraph()->GetAttributeIndex(Input) == INDEX_NONE;
}

bool UNiagaraNodeInput::IsExposedConstant()const
{
	return bExposeWhenConstant && IsConstant();
}