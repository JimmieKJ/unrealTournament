// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraCommon.h"
#include "NiagaraConstantSet.generated.h"


USTRUCT()
struct FNiagaraConstantMap
{
	GENERATED_USTRUCT_BODY()

private:
	TMap<FNiagaraVariableInfo, float> ScalarConstants;
	TMap<FNiagaraVariableInfo, FVector4> VectorConstants;
	TMap<FNiagaraVariableInfo, FMatrix> MatrixConstants;
	TMap<FNiagaraVariableInfo, class FNiagaraDataObject*> DataConstants;

public:

	void Empty()
	{
		ScalarConstants.Empty();
		VectorConstants.Empty();
		MatrixConstants.Empty();
	}

	void SetOrAdd(FNiagaraVariableInfo ID, float Sc)
	{
		check(ID.Type == ENiagaraDataType::Scalar);
		ScalarConstants.FindOrAdd(ID) = Sc;
	}

	void SetOrAdd(FNiagaraVariableInfo ID, const FVector4& Vc)
	{
		check(ID.Type == ENiagaraDataType::Vector);
		VectorConstants.FindOrAdd(ID) = Vc;
	}

	void SetOrAdd(FNiagaraVariableInfo ID, const FMatrix& Mc)
	{
		check(ID.Type == ENiagaraDataType::Matrix);
		MatrixConstants.FindOrAdd(ID) = Mc;
	}

	void SetOrAdd(FNiagaraVariableInfo ID, FNiagaraDataObject* Cc)
	{
		check(ID.Type == ENiagaraDataType::Curve);
		DataConstants.FindOrAdd(ID) = Cc;
	}

	void SetOrAdd(FName Name, float Sc)
	{
		ScalarConstants.FindOrAdd(FNiagaraVariableInfo(Name, ENiagaraDataType::Scalar)) = Sc;
	}

	void SetOrAdd(FName Name, const FVector4& Vc)
	{
		VectorConstants.FindOrAdd(FNiagaraVariableInfo(Name, ENiagaraDataType::Vector)) = Vc;
	}

	void SetOrAdd(FName Name, const FMatrix& Mc)
	{
		MatrixConstants.FindOrAdd(FNiagaraVariableInfo(Name, ENiagaraDataType::Matrix)) = Mc;
	}

	void SetOrAdd(FName Name, class FNiagaraDataObject* Cc)
	{
		DataConstants.FindOrAdd(FNiagaraVariableInfo(Name, ENiagaraDataType::Curve)) = Cc;
	}


	float* FindScalar(FNiagaraVariableInfo ID){ return ScalarConstants.Find(ID); }
	FVector4* FindVector(FNiagaraVariableInfo ID){ return VectorConstants.Find(ID); }
	FMatrix* FindMatrix(FNiagaraVariableInfo ID){ return MatrixConstants.Find(ID); }
	const float* FindScalar(FNiagaraVariableInfo ID)const { return ScalarConstants.Find(ID); }
	const FVector4* FindVector(FNiagaraVariableInfo ID)const { return VectorConstants.Find(ID); }
	const FMatrix* FindMatrix(FNiagaraVariableInfo ID)const { return MatrixConstants.Find(ID); }
	FNiagaraDataObject* FindDataObj(FNiagaraVariableInfo ID) const
	{
		FNiagaraDataObject * const *Ptr = DataConstants.Find(ID);
		if (Ptr)
		{
			return *Ptr;
		}

		return nullptr; 
	}
	float* FindScalar(FName Name) { return ScalarConstants.Find(FNiagaraVariableInfo(Name,ENiagaraDataType::Scalar)); }
	FVector4* FindVector(FName Name) { return VectorConstants.Find(FNiagaraVariableInfo(Name, ENiagaraDataType::Vector)); }
	FMatrix* FindMatrix(FName Name) { return MatrixConstants.Find(FNiagaraVariableInfo(Name, ENiagaraDataType::Matrix)); }
	FNiagaraDataObject* FindDataObj(FName Name)const { return FindDataObj(FNiagaraVariableInfo(Name, ENiagaraDataType::Curve)); }

	void Merge(FNiagaraConstantMap &InMap)
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

	const TMap<FNiagaraVariableInfo, class FNiagaraDataObject*> &GetDataConstants()	const
	{ 
		return DataConstants; 
	}


	virtual bool Serialize(FArchive &Ar)
	{
		// TODO: can't serialize the data object constants at the moment; need to figure that out
		Ar << ScalarConstants << VectorConstants << MatrixConstants;
		return true;
	}
};




USTRUCT()
struct FNiagaraConstants
{
	GENERATED_USTRUCT_BODY()

private:

	UPROPERTY()
	TArray<FNiagaraVariableInfo> ScalarConstantsInfo;
	UPROPERTY()
	TArray<FNiagaraVariableInfo> VectorConstantsInfo;
	UPROPERTY()
	TArray<FNiagaraVariableInfo> MatrixConstantsInfo;
	//UPROPERTY()
	TArray<FNiagaraVariableInfo> DataObjectConstantsInfo;

	UPROPERTY()
	TArray<float> ScalarConstants;
	UPROPERTY()
	TArray<FVector4> VectorConstants;
	UPROPERTY()
	TArray<FMatrix> MatrixConstants;
	//UPROPERTY()
	TArray<FNiagaraDataObject*> DataObjectConstants;

public:

	void Empty()
	{
		ScalarConstantsInfo.Empty();
		VectorConstantsInfo.Empty();
		MatrixConstantsInfo.Empty();
		DataObjectConstantsInfo.Empty();
		ScalarConstants.Empty();
		VectorConstants.Empty();
		MatrixConstants.Empty();
		DataObjectConstants.Empty();
	}

	void GetScalarConstant(int32 Index, float& OutValue, FNiagaraVariableInfo& OutInfo)const
	{
		OutInfo = ScalarConstantsInfo[Index]; 
		OutValue = ScalarConstants[Index];
	}
	void GetVectorConstant(int32 Index, FVector4& OutValue, FNiagaraVariableInfo& OutInfo)const
	{
		OutInfo = VectorConstantsInfo[Index];
		OutValue = VectorConstants[Index];
	}
	void GetMatrixConstant(int32 Index, FMatrix& OutValue, FNiagaraVariableInfo& OutInfo)const
	{
		OutInfo = MatrixConstantsInfo[Index];
		OutValue = MatrixConstants[Index];
	}
	void GetDataObjectConstant(int32 Index, FNiagaraDataObject*& OutValue, FNiagaraVariableInfo& OutInfo)const
	{
		OutInfo = DataObjectConstantsInfo[Index];
		OutValue = DataObjectConstants[Index];
	}

	const int32 GetNumScalarConstants()const{ return ScalarConstantsInfo.Num(); }
	const int32 GetNumVectorConstants()const{ return VectorConstantsInfo.Num(); }
	const int32 GetNumMatrixConstants()const{ return MatrixConstantsInfo.Num(); }
	const int32 GetNumDataObjectConstants()const{ return DataObjectConstantsInfo.Num(); }

	/** Fills the entire constants set into the constant table. */
	void AppendToConstantsTable(TArray<FVector4>& ConstantsTable)const
	{
		int32 Idx = ConstantsTable.Num();
		int32 NewIdx = 0;
		ConstantsTable.AddUninitialized(ScalarTableSize() + VectorTableSize() + MatrixTableSize());
		for (float Sc : ScalarConstants)
		{
			ConstantsTable[Idx + (NewIdx / 4)][NewIdx % 4] = Sc;
			++NewIdx;
			Idx = NewIdx % 4 == 0 ? Idx + 1 : Idx;
		}

		Idx += FMath::Min(1, NewIdx % 4);//Move to the next table entry if needed.
		for (const FVector4& Vc : VectorConstants)
		{
			ConstantsTable[Idx++] = Vc;
		}
		for (const FMatrix& Mc : MatrixConstants)
		{
			ConstantsTable[Idx] = FVector4(Mc.M[0][0], Mc.M[0][1], Mc.M[0][2], Mc.M[0][3]);
			ConstantsTable[Idx + 1] = FVector4(Mc.M[1][0], Mc.M[1][1], Mc.M[1][2], Mc.M[1][3]);
			ConstantsTable[Idx + 2] = FVector4(Mc.M[2][0], Mc.M[2][1], Mc.M[2][2], Mc.M[2][3]);
			ConstantsTable[Idx + 3] = FVector4(Mc.M[3][0], Mc.M[3][1], Mc.M[3][2], Mc.M[3][3]);
			Idx += 4;
		}
	}

	void AppendBufferConstants(TArray<class FNiagaraDataObject*> &Objs) const
	{
		Objs.Append(DataObjectConstants);
	}

	/** 
	Fills only selected constants into the table. In the order they appear in the array of passed names not the order they appear in the set. 
	Checks the passed map for entires to superceed the default values in the set.
	*/
	void AppendToConstantsTable(TArray<FVector4>& ConstantsTable, const FNiagaraConstantMap& Externals)const
	{
		int32 Idx = ConstantsTable.Num();
		int32 NewIdx = 0;
		ConstantsTable.AddUninitialized(ScalarTableSize() + VectorTableSize() + MatrixTableSize());
		for (int32 i = 0; i < ScalarConstants.Num(); ++i)
		{
			const float* Scalar = Externals.FindScalar(ScalarConstantsInfo[i]);
			ConstantsTable[Idx + (NewIdx / 4)][NewIdx % 4] = Scalar ? *Scalar : ScalarConstants[i];
			++NewIdx;
			Idx = NewIdx % 4 == 0 ? Idx + 1 : Idx;
		}

		Idx += FMath::Min(1, NewIdx % 4);//Move to the next table entry if needed.

		for (int32 i = 0; i < VectorConstants.Num(); ++i)
		{
			const FVector4* Vector = Externals.FindVector(VectorConstantsInfo[i]);
			ConstantsTable[Idx++] = Vector ? *Vector : VectorConstants[i];
		}

		for (int32 i = 0; i < MatrixConstants.Num(); ++i)
		{
			const FMatrix* Mat = Externals.FindMatrix(MatrixConstantsInfo[i]);
			FMatrix Mc = Mat ? *Mat : MatrixConstants[i];
			ConstantsTable[Idx] = FVector4(Mc.M[0][0], Mc.M[0][1], Mc.M[0][2], Mc.M[0][3]);
			ConstantsTable[Idx + 1] = FVector4(Mc.M[1][0], Mc.M[1][1], Mc.M[1][2], Mc.M[1][3]);
			ConstantsTable[Idx + 2] = FVector4(Mc.M[2][0], Mc.M[2][1], Mc.M[2][2], Mc.M[2][3]);
			ConstantsTable[Idx + 3] = FVector4(Mc.M[3][0], Mc.M[3][1], Mc.M[3][2], Mc.M[3][3]);
		}
	}

	void AppendExternalBufferConstants(TArray<class FNiagaraDataObject*> &DataObjs, const FNiagaraConstantMap& Externals) const
	{
		for (int32 i = 0; i < DataObjectConstantsInfo.Num(); ++i)
		{
			FNiagaraVariableInfo CcID = DataObjectConstantsInfo[i];
			FNiagaraDataObject* Data = Externals.FindDataObj(CcID);
			DataObjs.Add(Data);
		}
	}

	void SetOrAdd(const FNiagaraVariableInfo& Constant, float Value)
	{
		int32 Idx = ScalarConstantsInfo.Find(Constant);
		if (Idx == INDEX_NONE)
		{
			ScalarConstantsInfo.Add(Constant);
			ScalarConstants.Add(Value);
		}
		else
		{
			ScalarConstants[Idx] = Value;
		}
	}

	void SetOrAdd(const FNiagaraVariableInfo& Constant, const FVector4& Value)
	{
		int32 Idx = VectorConstantsInfo.Find(Constant);
		if (Idx == INDEX_NONE)
		{
			VectorConstantsInfo.Add(Constant);
			VectorConstants.Add(Value);
		}
		else
		{
			VectorConstants[Idx] = Value;
		}
	}

	void SetOrAdd(const FNiagaraVariableInfo& Constant, const FMatrix& Value)
	{
		int32 Idx = MatrixConstantsInfo.Find(Constant);
		if (Idx == INDEX_NONE)
		{
			MatrixConstantsInfo.Add(Constant);
			MatrixConstants.Add(Value);
		}
		else
		{
			MatrixConstants[Idx] = Value;
		}
	}

	void SetOrAdd(const FNiagaraVariableInfo& Constant, FNiagaraDataObject* Value)
	{
		int32 Idx = DataObjectConstantsInfo.Find(Constant);
		if (Idx == INDEX_NONE)
		{
			DataObjectConstantsInfo.Add(Constant);
			DataObjectConstants.Add(Value);
		}
		else
		{
			DataObjectConstants[Idx] = Value;
		}
	}

	FORCEINLINE int32 NumScalars()const { return ScalarConstants.Num(); }
	FORCEINLINE int32 NumVectors()const { return VectorConstants.Num(); }
	FORCEINLINE int32 NumMatrices()const { return MatrixConstants.Num(); }
	FORCEINLINE int32 ScalarTableSize()const { return(NumScalars() / 4) + FMath::Min(1,NumScalars() % 4); }
	FORCEINLINE int32 VectorTableSize()const { return(NumVectors()); }
	FORCEINLINE int32 MatrixTableSize()const { return(NumMatrices() * 4); }

	/**	Returns the number of vectors these constants would use in a constants table. */
	FORCEINLINE int32 GetTableSize()const { return ScalarTableSize() + VectorTableSize() + MatrixTableSize(); }

	/** Return the first constant used for this constant in a table. */
	FORCEINLINE int32 GetTableIndex_Scalar(const FNiagaraVariableInfo& Constant)const
	{ 
		int Idx = ScalarConstantsInfo.Find(Constant);
		if (Idx != INDEX_NONE)
		{
			return Idx / 4;
		}
		return INDEX_NONE;
	}

	FORCEINLINE int32 GetTableIndex_Vector(const FNiagaraVariableInfo& Constant)const
	{ 
		int Idx = VectorConstantsInfo.Find(Constant);
		if (Idx != INDEX_NONE)
		{
			return ScalarTableSize() + Idx;
		}
		return INDEX_NONE;
	}

	FORCEINLINE int32 GetTableIndex_Matrix(const FNiagaraVariableInfo& Constant)const
	{ 
		int Idx = MatrixConstantsInfo.Find(Constant);
		if (Idx != INDEX_NONE)
		{
			return ScalarTableSize() + VectorTableSize() + Idx * 4;
		}
		return INDEX_NONE;
	}

	FORCEINLINE int32 GetTableIndex_DataObj(const FNiagaraVariableInfo& Constant)const
	{ 
		return DataObjectConstantsInfo.Find(Constant); 
	}

	/** Return the absolute index of the passed scalar. Can be used to get the component index also. */
	FORCEINLINE int32 GetAbsoluteIndex_Scalar(const FNiagaraVariableInfo& Constant)const{ return ScalarConstantsInfo.Find(Constant); }
};

