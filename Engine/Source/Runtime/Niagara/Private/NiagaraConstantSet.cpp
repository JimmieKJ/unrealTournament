// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "NiagaraPrivate.h"
#include "NiagaraConstantSet.h"
#include "NiagaraSimulation.h"


//////////////////////////////////////////////////////////////////////////

void FNiagaraConstants::Empty()
{
	ScalarConstants.Empty();
	VectorConstants.Empty();
	MatrixConstants.Empty();
}

void FNiagaraConstants::GetScalarConstant(int32 Index, float& OutValue, FNiagaraVariableInfo& OutInfo)
{
	OutInfo = FNiagaraVariableInfo(ScalarConstants[Index].Name, ENiagaraDataType::Scalar);
	OutValue = ScalarConstants[Index].Value;
}

void FNiagaraConstants::GetVectorConstant(int32 Index, FVector4& OutValue, FNiagaraVariableInfo& OutInfo)
{
	OutInfo = FNiagaraVariableInfo(VectorConstants[Index].Name, ENiagaraDataType::Vector);
	OutValue = VectorConstants[Index].Value;
}

void FNiagaraConstants::GetMatrixConstant(int32 Index, FMatrix& OutValue, FNiagaraVariableInfo& OutInfo)
{
	OutInfo = FNiagaraVariableInfo(MatrixConstants[Index].Name, ENiagaraDataType::Matrix);
	OutValue = MatrixConstants[Index].Value;
}

void FNiagaraConstants::Init(UNiagaraEmitterProperties* EmitterProps, FNiagaraEmitterScriptProperties* ScriptProps)
{
	check(ScriptProps);
	check(EmitterProps);

	//Copy the script constants, retaining any altered values.
	const FNiagaraConstants& ScriptConstants = ScriptProps->Script->ConstantData.GetExternalConstants();

	TArray<FNiagaraConstant_Float> NewScalarConstants = ScriptConstants.ScalarConstants;
	for (auto& Override : ScalarConstants)
	{
		int32 Idx = NewScalarConstants.IndexOfByPredicate([&](const FNiagaraConstant_Float& C){ return Override.Name == C.Name; });

		if (Idx != INDEX_NONE)
			NewScalarConstants[Idx].Value = Override.Value;
	}
	ScalarConstants = NewScalarConstants;

	TArray<FNiagaraConstant_Vector> NewVectorConstants = ScriptConstants.VectorConstants;
	for (auto& Override : VectorConstants)
	{
		int32 Idx = NewVectorConstants.IndexOfByPredicate([&](const FNiagaraConstant_Vector& C){ return Override.Name == C.Name; });

		if (Idx != INDEX_NONE)
			NewVectorConstants[Idx].Value = Override.Value;
	}
	VectorConstants = NewVectorConstants;

	TArray<FNiagaraConstant_Matrix> NewMatrixConstants = ScriptConstants.MatrixConstants;
	for (auto& Override : MatrixConstants)
	{
		int32 Idx = NewMatrixConstants.IndexOfByPredicate([&](const FNiagaraConstant_Matrix& C){ return Override.Name == C.Name; });

		if (Idx != INDEX_NONE)
			NewMatrixConstants[Idx].Value = Override.Value;

	}
	MatrixConstants = NewMatrixConstants;
}

void FNiagaraConstants::AppendToConstantsTable(TArray<FVector4>& ConstantsTable)const
{
	int32 Idx = ConstantsTable.Num();
	int32 NewIdx = 0;
	ConstantsTable.AddUninitialized(ScalarTableSize() + VectorTableSize() + MatrixTableSize());
	for (const FNiagaraConstant_Float& Sc : ScalarConstants)
	{
		ConstantsTable[Idx++] = FVector4(Sc.Value, Sc.Value, Sc.Value, Sc.Value);
	}

	for (const FNiagaraConstant_Vector& Vc : VectorConstants)
	{
		ConstantsTable[Idx++] = Vc.Value;
	}
	for (const FNiagaraConstant_Matrix& C : MatrixConstants)
	{
		const FMatrix& Mc = C.Value;
		ConstantsTable[Idx] = FVector4(Mc.M[0][0], Mc.M[0][1], Mc.M[0][2], Mc.M[0][3]);
		ConstantsTable[Idx + 1] = FVector4(Mc.M[1][0], Mc.M[1][1], Mc.M[1][2], Mc.M[1][3]);
		ConstantsTable[Idx + 2] = FVector4(Mc.M[2][0], Mc.M[2][1], Mc.M[2][2], Mc.M[2][3]);
		ConstantsTable[Idx + 3] = FVector4(Mc.M[3][0], Mc.M[3][1], Mc.M[3][2], Mc.M[3][3]);
		Idx += 4;
	}
}



void FNiagaraConstants::AppendToConstantsTable(TArray<FVector4>& ConstantsTable, const FNiagaraConstants& Externals)const
{
	int32 Idx = ConstantsTable.Num();
	ConstantsTable.AddUninitialized(ScalarTableSize() + VectorTableSize() + MatrixTableSize());
	for (int32 i = 0; i < ScalarConstants.Num(); ++i)
	{
		const float* Scalar = Externals.FindScalar(ScalarConstants[i].Name);
		float Value = Scalar ? *Scalar : ScalarConstants[i].Value;
		ConstantsTable[Idx++] = FVector4(Value, Value, Value, Value);
	}

	for (int32 i = 0; i < VectorConstants.Num(); ++i)
	{
		const FVector4* Vector = Externals.FindVector(VectorConstants[i].Name);
		ConstantsTable[Idx++] = Vector ? *Vector : VectorConstants[i].Value;
	}

	for (int32 i = 0; i < MatrixConstants.Num(); ++i)
	{
		const FMatrix* Mat = Externals.FindMatrix(MatrixConstants[i].Name);
		FMatrix Mc = Mat ? *Mat : MatrixConstants[i].Value;
		ConstantsTable[Idx] = FVector4(Mc.M[0][0], Mc.M[0][1], Mc.M[0][2], Mc.M[0][3]);
		ConstantsTable[Idx + 1] = FVector4(Mc.M[1][0], Mc.M[1][1], Mc.M[1][2], Mc.M[1][3]);
		ConstantsTable[Idx + 2] = FVector4(Mc.M[2][0], Mc.M[2][1], Mc.M[2][2], Mc.M[2][3]);
		ConstantsTable[Idx + 3] = FVector4(Mc.M[3][0], Mc.M[3][1], Mc.M[3][2], Mc.M[3][3]);
	}
}


void FNiagaraConstants::SetOrAdd(const FNiagaraVariableInfo& Constant, float Value)
{
	check(Constant.Type == ENiagaraDataType::Scalar);
	FNiagaraConstant_Float* Const = ScalarConstants.FindByPredicate([&](const FNiagaraConstant_Float& C){ return Constant.Name == C.Name; });
	if (Const)
	{
		Const->Value = Value;
	}
	else
	{
		ScalarConstants.Add(FNiagaraConstant_Float(Constant.Name, Value));
	}
}

void FNiagaraConstants::SetOrAdd(const FNiagaraVariableInfo& Constant, const FVector4& Value)
{
	check(Constant.Type == ENiagaraDataType::Vector);
	FNiagaraConstant_Vector* Const = VectorConstants.FindByPredicate([&](const FNiagaraConstant_Vector& C){ return Constant.Name == C.Name; });
	if (Const)
	{
		Const->Value = Value;
	}
	else
	{
		VectorConstants.Add(FNiagaraConstant_Vector(Constant.Name, Value));
	}
}

void FNiagaraConstants::SetOrAdd(const FNiagaraVariableInfo& Constant, const FMatrix& Value)
{
	check(Constant.Type == ENiagaraDataType::Matrix);
	FNiagaraConstant_Matrix* Const = MatrixConstants.FindByPredicate([&](const FNiagaraConstant_Matrix& C){ return Constant.Name == C.Name; });
	if (Const)
	{
		Const->Value = Value;
	}
	else
	{
		MatrixConstants.Add(FNiagaraConstant_Matrix(Constant.Name, Value));
	}
}

void FNiagaraConstants::SetOrAdd(FName Name, float Value)
{
	SetOrAdd(FNiagaraVariableInfo(Name, ENiagaraDataType::Scalar), Value);
}

void FNiagaraConstants::SetOrAdd(FName Name, const FVector4& Value)
{
	SetOrAdd(FNiagaraVariableInfo(Name, ENiagaraDataType::Vector), Value);
}

void FNiagaraConstants::SetOrAdd(FName Name, const FMatrix& Value)
{
	SetOrAdd(FNiagaraVariableInfo(Name, ENiagaraDataType::Matrix), Value);
}

float* FNiagaraConstants::FindScalar(FName Name)
{
	auto* C = ScalarConstants.FindByPredicate([&](const FNiagaraConstant_Float& Constant){ return Constant.Name == Name; });
	return C ? &C->Value : nullptr;
}
FVector4* FNiagaraConstants::FindVector(FName Name)
{
	auto* C = VectorConstants.FindByPredicate([&](const FNiagaraConstant_Vector& Constant){ return Constant.Name == Name; });
	return C ? &C->Value : nullptr;
}
FMatrix* FNiagaraConstants::FindMatrix(FName Name)
{
	auto* C = MatrixConstants.FindByPredicate([&](const FNiagaraConstant_Matrix& Constant){ return Constant.Name == Name; });
	return C ? &C->Value : nullptr;
}




const float*  FNiagaraConstants::FindScalar(FName Name) const
{
	auto* C = ScalarConstants.FindByPredicate([&](const FNiagaraConstant_Float& Constant) { return Constant.Name == Name; });
	return C ? &C->Value : nullptr;
}

const FVector4* FNiagaraConstants::FindVector(FName Name) const
{
	auto* C = VectorConstants.FindByPredicate([&](const FNiagaraConstant_Vector& Constant) { return Constant.Name == Name; });
	return C ? &C->Value : nullptr;
}

const FMatrix* FNiagaraConstants::FindMatrix(FName Name) const
{
	auto* C = MatrixConstants.FindByPredicate([&](const FNiagaraConstant_Matrix& Constant) { return Constant.Name == Name; });
	return C ? &C->Value : nullptr;
}

//////////////////////////////////////////////////////////////////////////
