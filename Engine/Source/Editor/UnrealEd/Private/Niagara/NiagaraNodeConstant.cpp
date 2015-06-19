// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealEd.h"
#include "INiagaraCompiler.h"

#define LOCTEXT_NAMESPACE "NiagaraNodeConst"

UDEPRECATED_NiagaraNodeConstant::UDEPRECATED_NiagaraNodeConstant(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ConstName = NAME_None;
	DataType = ENiagaraDataType::Vector;
	bNeedsDefault = true;
	bExposeToEffectEditor = false;
}

void UDEPRECATED_NiagaraNodeConstant::AllocateDefaultPins()
{
	const UEdGraphSchema_Niagara* Schema = GetDefault<UEdGraphSchema_Niagara>();

	if (DataType == ENiagaraDataType::Scalar)
	{
		if (bNeedsDefault)
		{
			CreatePin(EGPD_Input, Schema->PC_Float, TEXT(""), NULL, false, false, LOCTEXT("FloatPinName", "Default").ToString(), true);
		}
		CreatePin(EGPD_Output, Schema->PC_Vector, TEXT(""), NULL, false, false, ConstName.ToString());
	}
	else if (DataType == ENiagaraDataType::Vector)
	{
		if (bNeedsDefault)
		{
			CreatePin(EGPD_Input, Schema->PC_Vector, TEXT(""), NULL, false, false, LOCTEXT("VectorPinName", "Default").ToString(), true);
		}
		CreatePin(EGPD_Output, Schema->PC_Vector, TEXT(""), NULL, false, false, ConstName.ToString());
	}
	else if (DataType == ENiagaraDataType::Matrix)
	{
		if (bNeedsDefault)
		{
			CreatePin(EGPD_Input, Schema->PC_Vector, TEXT(""), NULL, false, false, LOCTEXT("MatrixPinName0", "Default Row0").ToString(), true);
			CreatePin(EGPD_Input, Schema->PC_Vector, TEXT(""), NULL, false, false, LOCTEXT("MatrixPinName1", "Default Row1").ToString(), true);
			CreatePin(EGPD_Input, Schema->PC_Vector, TEXT(""), NULL, false, false, LOCTEXT("MatrixPinName2", "Default Row2").ToString(), true);
			CreatePin(EGPD_Input, Schema->PC_Vector, TEXT(""), NULL, false, false, LOCTEXT("MatrixPinName3", "Default Row3").ToString(), true);
		}
		CreatePin(EGPD_Output, Schema->PC_Matrix, TEXT(""), NULL, false, false, ConstName.ToString());
	}
	else if (DataType == ENiagaraDataType::Curve)
	{
		//CreatePin(EGPD_Input, Schema->PC_Vector, TEXT(""), NULL, false, false, LOCTEXT("FloatPinName", "Time").ToString(), true);
		CreatePin(EGPD_Output, Schema->PC_Curve, TEXT(""), NULL, false, false, ConstName.ToString());
	}
}

FText UDEPRECATED_NiagaraNodeConstant::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	if (ConstName != NAME_None)
		return FText::FromName(ConstName);
	else
		return LOCTEXT("DefaultTitle", "External Constant");
}

FLinearColor UDEPRECATED_NiagaraNodeConstant::GetNodeTitleColor() const
{
	const UEdGraphSchema_Niagara* Schema = GetDefault<UEdGraphSchema_Niagara>();

	if (Pins.Num() > 0 && Pins[0] != NULL)
	{
		return Schema->GetPinTypeColor(Pins[0]->PinType);
	}
	else
	{
		return Super::GetNodeTitleColor();
	}
}

void UDEPRECATED_NiagaraNodeConstant::Compile(class INiagaraCompiler* Compiler, TArray<FNiagaraNodeResult>& Outputs)
{
	//UE_LOG(LogNiagara, Fatal, TEXT("UDEPRECATED_NiagaraNodeConstant is deprecated. All instance of it should have been replaced in UNiagaraGraph::PostLoad()"));
	check(0);

	return;

	//TODO REMOVE THIS NODE ENTIRELY
// 	TArray<int32> OutputExpressions;
// 	if (DataType == ENiagaraDataType::Scalar)
// 	{
// 		float Default = 1.0f;
// 		UEdGraphPin* OutPin;
// 		if (bNeedsDefault)
// 		{
// 			CastChecked<UEdGraphSchema_Niagara>(GetSchema())->GetPinDefaultValue(Pins[0], Default);
// 			OutPin = Pins[1];
// 		}
// 		else
// 		{
// 			OutPin = Pins[0];
// 		}
// 		Outputs.Add(FNiagaraNodeResult(Compiler->GetExternalConstant(ConstName, Default), OutPin));
// 	}
// 	else if (DataType == ENiagaraDataType::Vector)
// 	{
// 		FVector4 Default = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
// 		UEdGraphPin* OutPin;
// 		if (bNeedsDefault)
// 		{
// 			CastChecked<UEdGraphSchema_Niagara>(GetSchema())->GetPinDefaultValue(Pins[0], Default);
// 			OutPin = Pins[1];
// 		}
// 		else
// 		{
// 			OutPin = Pins[0];
// 		}
// 		Outputs.Add(FNiagaraNodeResult(Compiler->GetExternalConstant(ConstName, Default), OutPin));
// 	}
// 	else if (DataType == ENiagaraDataType::Matrix)
// 	{
// 		FVector4 Default0 = FVector4(1.0f, 0.0f, 0.0f, 0.0f);
// 		FVector4 Default1 = FVector4(0.0f, 1.0f, 0.0f, 0.0f);
// 		FVector4 Default2 = FVector4(0.0f, 0.0f, 1.0f, 0.0f);
// 		FVector4 Default3 = FVector4(0.0f, 0.0f, 0.0f, 1.0f);
// 
// 		UEdGraphPin* OutPin;
// 		if (bNeedsDefault)
// 		{
// 			CastChecked<UEdGraphSchema_Niagara>(GetSchema())->GetPinDefaultValue(Pins[0], Default0);
// 			CastChecked<UEdGraphSchema_Niagara>(GetSchema())->GetPinDefaultValue(Pins[1], Default1);
// 			CastChecked<UEdGraphSchema_Niagara>(GetSchema())->GetPinDefaultValue(Pins[2], Default2);
// 			CastChecked<UEdGraphSchema_Niagara>(GetSchema())->GetPinDefaultValue(Pins[3], Default3);
// 			OutPin = Pins[4];
// 		}
// 		else
// 		{
// 			OutPin = Pins[0];
// 		}
// 		FMatrix Default;
// 		Default.M[0][0] = Default0[0]; Default.M[0][1] = Default0[1]; Default.M[0][2] = Default0[2]; Default.M[0][3] = Default0[3];
// 		Default.M[1][0] = Default1[0]; Default.M[1][1] = Default1[1]; Default.M[1][2] = Default1[2]; Default.M[1][3] = Default1[3];
// 		Default.M[2][0] = Default2[0]; Default.M[2][1] = Default2[1]; Default.M[2][2] = Default2[2]; Default.M[2][3] = Default2[3];
// 		Default.M[3][0] = Default3[0]; Default.M[3][1] = Default3[1]; Default.M[3][2] = Default3[2]; Default.M[3][3] = Default3[3];
// 		Outputs.Add(FNiagaraNodeResult(Compiler->GetExternalConstant(ConstName, Default), OutPin));
// 	}
//	else if (DataType == ENiagaraDataType::Curve)
//	{
//		UEdGraphPin* OutPin = Pins[0];
//		Outputs.Add(FNiagaraNodeResult(Compiler->GetExternalCurveConstant(ConstName), OutPin));
//	}
}

void UDEPRECATED_NiagaraNodeConstant::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	static FName DataTypePropertyName(TEXT("DataType"));
	if (PropertyChangedEvent.Property && PropertyChangedEvent.Property->GetFName() == DataTypePropertyName)
	{
		for (auto Pin : Pins)
		{
			Pin->BreakAllPinLinks();
		}
		Pins.Empty();
		AllocateDefaultPins();
	}
}
#undef LOCTEXT_NAMESPACE