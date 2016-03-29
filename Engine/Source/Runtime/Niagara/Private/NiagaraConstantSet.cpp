// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "NiagaraPrivate.h"
#include "NiagaraConstantSet.h"
#include "NiagaraSimulation.h"

//////////////////////////////////////////////////////////////////////////
// FNiagaraConstantMap

template<>
struct TStructOpsTypeTraits<FNiagaraConstantMap> : public TStructOpsTypeTraitsBase
{
	enum
	{
		WithSerializer = true
	};
};
IMPLEMENT_STRUCT(NiagaraConstantMap);

void FNiagaraConstantMap::Empty()
{
	ScalarConstants.Empty();
	VectorConstants.Empty();
	MatrixConstants.Empty();
}

void FNiagaraConstantMap::SetOrAdd(FNiagaraVariableInfo ID, float Sc)
{
	check(ID.Type == ENiagaraDataType::Scalar);
	ScalarConstants.FindOrAdd(ID) = Sc;
}

void FNiagaraConstantMap::SetOrAdd(FNiagaraVariableInfo ID, const FVector4& Vc)
{
	check(ID.Type == ENiagaraDataType::Vector);
	VectorConstants.FindOrAdd(ID) = Vc;
}

void FNiagaraConstantMap::SetOrAdd(FNiagaraVariableInfo ID, const FMatrix& Mc)
{
	check(ID.Type == ENiagaraDataType::Matrix);
	MatrixConstants.FindOrAdd(ID) = Mc;
}

void FNiagaraConstantMap::SetOrAdd(FNiagaraVariableInfo ID, UNiagaraDataObject* Cc)
{
	check(ID.Type == ENiagaraDataType::Curve);
	DataConstants.FindOrAdd(ID) = Cc;
}

void FNiagaraConstantMap::SetOrAdd(FName Name, float Sc)
{
	ScalarConstants.FindOrAdd(FNiagaraVariableInfo(Name, ENiagaraDataType::Scalar)) = Sc;
}

void FNiagaraConstantMap::SetOrAdd(FName Name, const FVector4& Vc)
{
	VectorConstants.FindOrAdd(FNiagaraVariableInfo(Name, ENiagaraDataType::Vector)) = Vc;
}

void FNiagaraConstantMap::SetOrAdd(FName Name, const FMatrix& Mc)
{
	MatrixConstants.FindOrAdd(FNiagaraVariableInfo(Name, ENiagaraDataType::Matrix)) = Mc;
}

void FNiagaraConstantMap::SetOrAdd(FName Name, class UNiagaraDataObject* Cc)
{
	DataConstants.FindOrAdd(FNiagaraVariableInfo(Name, ENiagaraDataType::Curve)) = Cc;
}

float* FNiagaraConstantMap::FindScalar(FNiagaraVariableInfo ID){ return ScalarConstants.Find(ID); }
FVector4* FNiagaraConstantMap::FindVector(FNiagaraVariableInfo ID){ return VectorConstants.Find(ID); }
FMatrix* FNiagaraConstantMap::FindMatrix(FNiagaraVariableInfo ID){ return MatrixConstants.Find(ID); }
UNiagaraDataObject* FNiagaraConstantMap::FindDataObj(FNiagaraVariableInfo ID)
{
	UNiagaraDataObject * const *Ptr = DataConstants.Find(ID);
	if (Ptr)
	{
		return *Ptr;
	}

	return nullptr;
}
const float* FNiagaraConstantMap::FindScalar(FNiagaraVariableInfo ID)const { return ScalarConstants.Find(ID); }
const FVector4* FNiagaraConstantMap::FindVector(FNiagaraVariableInfo ID)const { return VectorConstants.Find(ID); }
const FMatrix* FNiagaraConstantMap::FindMatrix(FNiagaraVariableInfo ID)const { return MatrixConstants.Find(ID); }
UNiagaraDataObject* FNiagaraConstantMap::FindDataObj(FNiagaraVariableInfo ID) const
{
	UNiagaraDataObject * const *Ptr = DataConstants.Find(ID);
	if (Ptr)
	{
		return *Ptr;
	}

	return nullptr;
}
float* FNiagaraConstantMap::FindScalar(FName Name) { return ScalarConstants.Find(FNiagaraVariableInfo(Name, ENiagaraDataType::Scalar)); }
FVector4* FNiagaraConstantMap::FindVector(FName Name) { return VectorConstants.Find(FNiagaraVariableInfo(Name, ENiagaraDataType::Vector)); }
FMatrix* FNiagaraConstantMap::FindMatrix(FName Name) { return MatrixConstants.Find(FNiagaraVariableInfo(Name, ENiagaraDataType::Matrix)); }
UNiagaraDataObject* FNiagaraConstantMap::FindDataObj(FName Name){ return FindDataObj(FNiagaraVariableInfo(Name, ENiagaraDataType::Curve)); }

const float* FNiagaraConstantMap::FindScalar(FName Name)const  { return ScalarConstants.Find(FNiagaraVariableInfo(Name, ENiagaraDataType::Scalar)); }
const FVector4* FNiagaraConstantMap::FindVector(FName Name)const  { return VectorConstants.Find(FNiagaraVariableInfo(Name, ENiagaraDataType::Vector)); }
const FMatrix* FNiagaraConstantMap::FindMatrix(FName Name)const  { return MatrixConstants.Find(FNiagaraVariableInfo(Name, ENiagaraDataType::Matrix)); }
UNiagaraDataObject* FNiagaraConstantMap::FindDataObj(FName Name)const { return FindDataObj(FNiagaraVariableInfo(Name, ENiagaraDataType::Curve)); }

void FNiagaraConstantMap::Merge(FNiagaraConstants &InConstants)
{
	for (FNiagaraConstants_Float& C : InConstants.ScalarConstants)
	{
		SetOrAdd(FNiagaraVariableInfo(C.Name, ENiagaraDataType::Scalar), C.Value);
	}
	for (FNiagaraConstants_Vector& C : InConstants.VectorConstants)
	{
		SetOrAdd(FNiagaraVariableInfo(C.Name, ENiagaraDataType::Vector), C.Value);
	}
	for (FNiagaraConstants_Matrix& C : InConstants.MatrixConstants)
	{
		SetOrAdd(FNiagaraVariableInfo(C.Name, ENiagaraDataType::Matrix), C.Value);
	}
	for (FNiagaraConstants_DataObject& C : InConstants.DataObjectConstants)
	{
		SetOrAdd(FNiagaraVariableInfo(C.Name, ENiagaraDataType::Curve), C.Value);
	}
}

void FNiagaraConstantMap::Merge(FNiagaraConstantMap &InMap)
{
	for (auto MapIt = InMap.ScalarConstants.CreateIterator(); MapIt; ++MapIt)
	{
		SetOrAdd(MapIt.Key(), MapIt.Value());
	}

	for (auto MapIt = InMap.VectorConstants.CreateIterator(); MapIt; ++MapIt)
	{
		SetOrAdd(MapIt.Key(), MapIt.Value());
	}

	for (auto MapIt = InMap.MatrixConstants.CreateIterator(); MapIt; ++MapIt)
	{
		SetOrAdd(MapIt.Key(), MapIt.Value());
	}

	for (auto MapIt = InMap.DataConstants.CreateIterator(); MapIt; ++MapIt)
	{
		SetOrAdd(MapIt.Key(), MapIt.Value());
	}
}

const TMap<FNiagaraVariableInfo, float> &FNiagaraConstantMap::GetScalarConstants()	const
{
	return ScalarConstants;
}

const TMap<FNiagaraVariableInfo, FVector4> &FNiagaraConstantMap::GetVectorConstants()	const
{
	return VectorConstants;
}

const TMap<FNiagaraVariableInfo, FMatrix> &FNiagaraConstantMap::GetMatrixConstants()	const
{
	return MatrixConstants;
}

const TMap<FNiagaraVariableInfo, class UNiagaraDataObject*> &FNiagaraConstantMap::GetDataConstants()	const
{
	return DataConstants;
}

//////////////////////////////////////////////////////////////////////////

void FNiagaraConstants::Empty()
{
	ScalarConstants.Empty();
	VectorConstants.Empty();
	MatrixConstants.Empty();
	DataObjectConstants.Empty();
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

void FNiagaraConstants::GetDataObjectConstant(int32 Index, UNiagaraDataObject*& OutValue, FNiagaraVariableInfo& OutInfo)
{
	OutInfo = FNiagaraVariableInfo(DataObjectConstants[Index].Name, ENiagaraDataType::Curve);
	OutValue = DataObjectConstants[Index].Value;
}

void FNiagaraConstants::Init(UNiagaraEmitterProperties* EmitterProps, FNiagaraEmitterScriptProperties* ScriptProps)
{
	check(ScriptProps);
	check(EmitterProps);

	//Copy the script constants, retaining any altered values.
	const FNiagaraConstants& ScriptConstants = ScriptProps->Script->ConstantData.GetExternalConstants();

	TArray<FNiagaraConstants_Float> NewScalarConstants = ScriptConstants.ScalarConstants;
	for (auto& Override : ScalarConstants)
	{
		int32 Idx = NewScalarConstants.IndexOfByPredicate([&](const FNiagaraConstants_Float& C){ return Override.Name == C.Name; });

		if (Idx != INDEX_NONE)
			NewScalarConstants[Idx].Value = Override.Value;
	}
	ScalarConstants = NewScalarConstants;

	TArray<FNiagaraConstants_Vector> NewVectorConstants = ScriptConstants.VectorConstants;
	for (auto& Override : VectorConstants)
	{
		int32 Idx = NewVectorConstants.IndexOfByPredicate([&](const FNiagaraConstants_Vector& C){ return Override.Name == C.Name; });

		if (Idx != INDEX_NONE)
			NewVectorConstants[Idx].Value = Override.Value;
	}
	VectorConstants = NewVectorConstants;

	TArray<FNiagaraConstants_Matrix> NewMatrixConstants = ScriptConstants.MatrixConstants;
	for (auto& Override : MatrixConstants)
	{
		int32 Idx = NewMatrixConstants.IndexOfByPredicate([&](const FNiagaraConstants_Matrix& C){ return Override.Name == C.Name; });

		if (Idx != INDEX_NONE)
			NewMatrixConstants[Idx].Value = Override.Value;

	}
	MatrixConstants = NewMatrixConstants;

	const TArray<FNiagaraConstants_DataObject>& ScriptDataObjectConstants = ScriptConstants.DataObjectConstants;
	TArray<FNiagaraConstants_DataObject> OldDataObjectConstants = DataObjectConstants;
	DataObjectConstants.Empty();
	for (const FNiagaraConstants_DataObject& ScriptConst : ScriptDataObjectConstants)
	{
		int32 OverrideIdx = OldDataObjectConstants.IndexOfByPredicate([&](const FNiagaraConstants_DataObject& C){ return ScriptConst.Name == C.Name && ScriptConst.Value && C.Value &&ScriptConst.Value->GetClass() == C.Value->GetClass(); });
		
		int32 AddIndex = DataObjectConstants.AddZeroed();
		DataObjectConstants[AddIndex].Name = ScriptDataObjectConstants[AddIndex].Name;
		
		if (OverrideIdx != INDEX_NONE)
		{
			//If this constant already has data in the effect, use that.
			DataObjectConstants[AddIndex].Value = OldDataObjectConstants[OverrideIdx].Value;
		}		
		else
		{
			//Otherwise, duplicate the data from the script.
			if (ScriptConst.Value)
				DataObjectConstants[AddIndex].Value = CastChecked<UNiagaraDataObject>(StaticDuplicateObject(ScriptConst.Value, EmitterProps));
		}
	}
}

/** 
Copies constants in from the script as usual but allows overrides from old data in an FNiagaraConstantMap.
Only used for BC. DO NOT USE.
*/
void FNiagaraConstants::InitFromOldMap(FNiagaraConstantMap& OldMap)
{
	const TMap<FNiagaraVariableInfo, float>& OldScalarConsts = OldMap.GetScalarConstants();
	const TMap<FNiagaraVariableInfo, FVector4>& OldVectorConsts = OldMap.GetVectorConstants();
	const TMap<FNiagaraVariableInfo, FMatrix>& OldMatrixConsts = OldMap.GetMatrixConstants();
	const TMap<FNiagaraVariableInfo, UNiagaraDataObject*>& OldDataObjectConstants = OldMap.GetDataConstants();

	for (const TPair<FNiagaraVariableInfo, float> Pair : OldScalarConsts)
	{
		ScalarConstants.Add(FNiagaraConstants_Float(Pair.Key.Name, Pair.Value));
	}
	for (const TPair<FNiagaraVariableInfo, FVector4>& Pair : OldVectorConsts)
	{
		VectorConstants.Add(FNiagaraConstants_Vector(Pair.Key.Name, Pair.Value));
	}
	for (const TPair<FNiagaraVariableInfo, FMatrix>& Pair : OldMatrixConsts)
	{
		MatrixConstants.Add(FNiagaraConstants_Matrix(Pair.Key.Name, Pair.Value));
	}
	for (const TPair<FNiagaraVariableInfo, UNiagaraDataObject*>& Pair : OldDataObjectConstants)
	{
		DataObjectConstants.Add(FNiagaraConstants_DataObject(Pair.Key.Name, Pair.Value));
	}
}

void FNiagaraConstants::AppendToConstantsTable(TArray<FVector4>& ConstantsTable)const
{
	int32 Idx = ConstantsTable.Num();
	int32 NewIdx = 0;
	ConstantsTable.AddUninitialized(ScalarTableSize() + VectorTableSize() + MatrixTableSize());
	for (const FNiagaraConstants_Float& Sc : ScalarConstants)
	{
		ConstantsTable[Idx++] = FVector4(Sc.Value, Sc.Value, Sc.Value, Sc.Value);
	}

	for (const FNiagaraConstants_Vector& Vc : VectorConstants)
	{
		ConstantsTable[Idx++] = Vc.Value;
	}
	for (const FNiagaraConstants_Matrix& C : MatrixConstants)
	{
		const FMatrix& Mc = C.Value;
		ConstantsTable[Idx] = FVector4(Mc.M[0][0], Mc.M[0][1], Mc.M[0][2], Mc.M[0][3]);
		ConstantsTable[Idx + 1] = FVector4(Mc.M[1][0], Mc.M[1][1], Mc.M[1][2], Mc.M[1][3]);
		ConstantsTable[Idx + 2] = FVector4(Mc.M[2][0], Mc.M[2][1], Mc.M[2][2], Mc.M[2][3]);
		ConstantsTable[Idx + 3] = FVector4(Mc.M[3][0], Mc.M[3][1], Mc.M[3][2], Mc.M[3][3]);
		Idx += 4;
	}
}

void FNiagaraConstants::AppendBufferConstants(TArray<class UNiagaraDataObject*> &Objs) const
{
	for (const FNiagaraConstants_DataObject& C : DataObjectConstants)
	{
		Objs.Add(C.Value);
	}
}

void FNiagaraConstants::AppendToConstantsTable(TArray<FVector4>& ConstantsTable, const FNiagaraConstantMap& Externals)const
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

void FNiagaraConstants::AppendExternalBufferConstants(TArray<class UNiagaraDataObject*> &DataObjs, const FNiagaraConstantMap& Externals) const
{
	for (int32 i = 0; i < DataObjectConstants.Num(); ++i)
	{
		UNiagaraDataObject* Data = Externals.FindDataObj(DataObjectConstants[i].Name);
		DataObjs.Add(Data);
	}
}

void FNiagaraConstants::SetOrAdd(const FNiagaraVariableInfo& Constant, float Value)
{
	check(Constant.Type == ENiagaraDataType::Scalar);
	FNiagaraConstants_Float* Const = ScalarConstants.FindByPredicate([&](const FNiagaraConstants_Float& C){ return Constant.Name == C.Name; });
	if (Const)
	{
		Const->Value = Value;
	}
	else
	{
		ScalarConstants.Add(FNiagaraConstants_Float(Constant.Name, Value));
	}
}

void FNiagaraConstants::SetOrAdd(const FNiagaraVariableInfo& Constant, const FVector4& Value)
{
	check(Constant.Type == ENiagaraDataType::Vector);
	FNiagaraConstants_Vector* Const = VectorConstants.FindByPredicate([&](const FNiagaraConstants_Vector& C){ return Constant.Name == C.Name; });
	if (Const)
	{
		Const->Value = Value;
	}
	else
	{
		VectorConstants.Add(FNiagaraConstants_Vector(Constant.Name, Value));
	}
}

void FNiagaraConstants::SetOrAdd(const FNiagaraVariableInfo& Constant, const FMatrix& Value)
{
	check(Constant.Type == ENiagaraDataType::Matrix);
	FNiagaraConstants_Matrix* Const = MatrixConstants.FindByPredicate([&](const FNiagaraConstants_Matrix& C){ return Constant.Name == C.Name; });
	if (Const)
	{
		Const->Value = Value;
	}
	else
	{
		MatrixConstants.Add(FNiagaraConstants_Matrix(Constant.Name, Value));
	}
}

void FNiagaraConstants::SetOrAdd(const FNiagaraVariableInfo& Constant, UNiagaraDataObject* Value)
{
	check(Constant.Type == ENiagaraDataType::Curve);
	FNiagaraConstants_DataObject* Const = DataObjectConstants.FindByPredicate([&](const FNiagaraConstants_DataObject& C){ return Constant.Name == C.Name; });
	if (Const)
	{
		Const->Value = Value;
	}
	else
	{
		DataObjectConstants.Add(FNiagaraConstants_DataObject(Constant.Name, Value));
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

void FNiagaraConstants::SetOrAdd(FName Name, UNiagaraDataObject* Value)
{
	SetOrAdd(FNiagaraVariableInfo(Name, ENiagaraDataType::Curve), Value);
}

float* FNiagaraConstants::FindScalar(FName Name)
{
	auto* C = ScalarConstants.FindByPredicate([&](const FNiagaraConstants_Float& Constant){ return Constant.Name == Name; });
	return C ? &C->Value : nullptr;
}
FVector4* FNiagaraConstants::FindVector(FName Name)
{
	auto* C = VectorConstants.FindByPredicate([&](const FNiagaraConstants_Vector& Constant){ return Constant.Name == Name; });
	return C ? &C->Value : nullptr;
}
FMatrix* FNiagaraConstants::FindMatrix(FName Name)
{
	auto* C = MatrixConstants.FindByPredicate([&](const FNiagaraConstants_Matrix& Constant){ return Constant.Name == Name; });
	return C ? &C->Value : nullptr;
}
UNiagaraDataObject* FNiagaraConstants::FindDataObj(FName Name)
{
	auto* C = DataObjectConstants.FindByPredicate([&](const FNiagaraConstants_DataObject& Constant){ return Constant.Name == Name; });
	return C ? C->Value : nullptr;
}

//////////////////////////////////////////////////////////////////////////
