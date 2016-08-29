// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraCommon.h"
#include "NiagaraConstantSet.generated.h"

struct FNiagaraConstants;


USTRUCT()
struct FNiagaraConstantBase
{
	GENERATED_USTRUCT_BODY()

	FNiagaraConstantBase()
	: Name(NAME_None)
	{}

	FNiagaraConstantBase(FName InName)
	: Name(InName)
	{}

	UPROPERTY(VisibleAnywhere, Category = "Constant")
	FName Name;
};

USTRUCT()
struct FNiagaraConstant_Float : public FNiagaraConstantBase
{
	GENERATED_USTRUCT_BODY()

	FNiagaraConstant_Float()
	: Value(0.0f)
	{}

	FNiagaraConstant_Float(FName InName, float InValue)
	: FNiagaraConstantBase(InName)
	, Value(InValue)
	{}

	UPROPERTY(EditAnywhere, Category = "Constant")
	float Value;
};

USTRUCT()
struct FNiagaraConstant_Vector : public FNiagaraConstantBase
{
	GENERATED_USTRUCT_BODY()

	FNiagaraConstant_Vector()
	: Value(FVector4(1.0f, 1.0f, 1.0f, 1.0f))
	{}

	FNiagaraConstant_Vector(FName InName, const FVector4& InValue)
	: FNiagaraConstantBase(InName)
	, Value(InValue)
	{}

	UPROPERTY(EditAnywhere, Category = "Constant")
	FVector4 Value;
};

USTRUCT()
struct FNiagaraConstant_Matrix : public FNiagaraConstantBase
{
	GENERATED_USTRUCT_BODY()

	FNiagaraConstant_Matrix()
	: Value(FMatrix::Identity)
	{}

	FNiagaraConstant_Matrix(FName InName, const FMatrix& InValue)
	: FNiagaraConstantBase(InName)
	, Value(InValue)
	{}

	UPROPERTY(EditAnywhere, Category = "Constant")
	FMatrix Value;
};

USTRUCT()
struct FNiagaraConstants
{
	GENERATED_USTRUCT_BODY()
	
public:

	UPROPERTY(EditAnywhere, EditFixedSize, Category = "Constant")
	TArray<FNiagaraConstant_Float> ScalarConstants;
	UPROPERTY(EditAnywhere, EditFixedSize, Category = "Constant")
	TArray<FNiagaraConstant_Vector> VectorConstants;
	UPROPERTY(EditAnywhere, EditFixedSize, Category = "Constant")
	TArray<FNiagaraConstant_Matrix> MatrixConstants;

	NIAGARA_API void Empty();
	NIAGARA_API void GetScalarConstant(int32 Index, float& OutValue, FNiagaraVariableInfo& OutInfo);
	NIAGARA_API void GetVectorConstant(int32 Index, FVector4& OutValue, FNiagaraVariableInfo& OutInfo);
	NIAGARA_API void GetMatrixConstant(int32 Index, FMatrix& OutValue, FNiagaraVariableInfo& OutInfo);

	const int32 GetNumScalarConstants()const{ return ScalarConstants.Num(); }
	const int32 GetNumVectorConstants()const{ return VectorConstants.Num(); }
	const int32 GetNumMatrixConstants()const{ return MatrixConstants.Num(); }

	NIAGARA_API void Init(class UNiagaraEmitterProperties* EmitterProps, struct FNiagaraEmitterScriptProperties* ScriptProps);

	/** Fills the entire constants set into the constant table. */
	void AppendToConstantsTable(TArray<FVector4>& ConstantsTable)const;

	/** 
	Fills only selected constants into the table. In the order they appear in the array of passed names not the order they appear in the set. 
	Checks the passed map for entries to supersede the default values in the set.
	*/
	void AppendToConstantsTable(TArray<FVector4>& ConstantsTable, const FNiagaraConstants& Externals)const;


	NIAGARA_API void SetOrAdd(const FNiagaraVariableInfo& Constant, float Value);
	NIAGARA_API void SetOrAdd(const FNiagaraVariableInfo& Constant, const FVector4& Value);
	NIAGARA_API void SetOrAdd(const FNiagaraVariableInfo& Constant, const FMatrix& Value);
	NIAGARA_API void SetOrAdd(FName Name, float Value);
	NIAGARA_API void SetOrAdd(FName Name, const FVector4& Value);
	NIAGARA_API void SetOrAdd(FName Name, const FMatrix& Value);

	FORCEINLINE int32 NumScalars()const { return ScalarConstants.Num(); }
	FORCEINLINE int32 NumVectors()const { return VectorConstants.Num(); }
	FORCEINLINE int32 NumMatrices()const { return MatrixConstants.Num(); }
	FORCEINLINE int32 ScalarTableSize()const { return(NumScalars()); }
	FORCEINLINE int32 VectorTableSize()const { return(NumVectors()); }
	FORCEINLINE int32 MatrixTableSize()const { return(NumMatrices() * 4); }

	/**	Returns the number of vectors these constants would use in a constants table. */
	FORCEINLINE int32 GetTableSize()const { return ScalarTableSize() + VectorTableSize() + MatrixTableSize(); }
	
	NIAGARA_API float* FindScalar(FName Name);
	NIAGARA_API FVector4* FindVector(FName Name);
	NIAGARA_API FMatrix* FindMatrix(FName Name);

	NIAGARA_API const float* FindScalar(FName Name) const;
	NIAGARA_API const FVector4* FindVector(FName Name) const;
	NIAGARA_API const FMatrix* FindMatrix(FName Name) const;


	/** Return the first constant used for this constant in a table. */
	FORCEINLINE int32 GetTableIndex_Scalar(const FNiagaraVariableInfo& Constant)const
	{ 
		int Idx = ScalarConstants.IndexOfByPredicate([&](const FNiagaraConstant_Float& V){ return Constant.Name == V.Name; });
		if (Idx != INDEX_NONE)
		{
			return Idx;
		}
		return INDEX_NONE;
	}

	FORCEINLINE int32 GetTableIndex_Vector(const FNiagaraVariableInfo& Constant)const
	{ 
		int Idx = VectorConstants.IndexOfByPredicate([&](const FNiagaraConstant_Vector& V){ return Constant.Name == V.Name; });
		if (Idx != INDEX_NONE)
		{
			return ScalarTableSize() + Idx;
		}
		return INDEX_NONE;
	}

	FORCEINLINE int32 GetTableIndex_Matrix(const FNiagaraVariableInfo& Constant)const
	{ 
		int Idx = MatrixConstants.IndexOfByPredicate([&](const FNiagaraConstant_Matrix& V){ return Constant.Name == V.Name; });
		if (Idx != INDEX_NONE)
		{
			return ScalarTableSize() + VectorTableSize() + Idx * 4;
		}
		return INDEX_NONE;
	}


	void Merge(FNiagaraConstants &InConstants)
	{
		for (FNiagaraConstant_Float& C : InConstants.ScalarConstants)
		{
			SetOrAdd(FNiagaraVariableInfo(C.Name, ENiagaraDataType::Scalar), C.Value);
		}
		for (FNiagaraConstant_Vector& C : InConstants.VectorConstants)
		{
			SetOrAdd(FNiagaraVariableInfo(C.Name, ENiagaraDataType::Vector), C.Value);
		}
		for (FNiagaraConstant_Matrix& C : InConstants.MatrixConstants)
		{
			SetOrAdd(FNiagaraVariableInfo(C.Name, ENiagaraDataType::Matrix), C.Value);
		}
	}
};
