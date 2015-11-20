// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraCommon.h"
#include "VectorVMDataObject.h"
#include "NiagaraConstantSet.generated.h"

struct FNiagaraConstants;

/** Runtime storage for emitter overridden constants. Only now a USTRUCT still to support BC */
USTRUCT()
struct FNiagaraConstantMap
{
	GENERATED_USTRUCT_BODY()
private:
	TMap<FNiagaraVariableInfo, float> ScalarConstants;
	TMap<FNiagaraVariableInfo, FVector4> VectorConstants;
	TMap<FNiagaraVariableInfo, FMatrix> MatrixConstants;
	TMap<FNiagaraVariableInfo, class UNiagaraDataObject*> DataConstants;

public:
	FNiagaraConstantMap()
	{
	}

	void Empty();

	void SetOrAdd(FNiagaraVariableInfo ID, float Sc);
	void SetOrAdd(FNiagaraVariableInfo ID, const FVector4& Vc);
	void SetOrAdd(FNiagaraVariableInfo ID, const FMatrix& Mc);
	void SetOrAdd(FNiagaraVariableInfo ID, UNiagaraDataObject* Cc);
	void SetOrAdd(FName Name, float Sc);
	void SetOrAdd(FName Name, const FVector4& Vc);
	void SetOrAdd(FName Name, const FMatrix& Mc);
	void SetOrAdd(FName Name, class UNiagaraDataObject* Cc);

	float* FindScalar(FNiagaraVariableInfo ID);
	FVector4* FindVector(FNiagaraVariableInfo ID);
	FMatrix* FindMatrix(FNiagaraVariableInfo ID);
	UNiagaraDataObject* FindDataObj(FNiagaraVariableInfo ID);
	const float* FindScalar(FNiagaraVariableInfo ID)const;
	const FVector4* FindVector(FNiagaraVariableInfo ID)const;
	const FMatrix* FindMatrix(FNiagaraVariableInfo ID)const;
	UNiagaraDataObject* FindDataObj(FNiagaraVariableInfo ID) const;
	float* FindScalar(FName Name);
	FVector4* FindVector(FName Name);
	FMatrix* FindMatrix(FName Name);
	UNiagaraDataObject* FindDataObj(FName Name);
	const float* FindScalar(FName Name)const;
	const FVector4* FindVector(FName Name)const;
	const FMatrix* FindMatrix(FName Name)const;
	UNiagaraDataObject* FindDataObj(FName Name)const;

	void Merge(FNiagaraConstants &InConstants);
	void Merge(FNiagaraConstantMap &InMap);

	const TMap<FNiagaraVariableInfo, float> &GetScalarConstants()	const;
	const TMap<FNiagaraVariableInfo, FVector4> &GetVectorConstants()	const;
	const TMap<FNiagaraVariableInfo, FMatrix> &GetMatrixConstants()	const;
	const TMap<FNiagaraVariableInfo, class UNiagaraDataObject*> &GetDataConstants()	const;


	virtual bool Serialize(FArchive &Ar)
	{
		// TODO: can't serialize the data object constants at the moment; need to figure that out
		Ar << ScalarConstants << VectorConstants << MatrixConstants;
		if (Ar.UE4Ver() >= VER_UE4_NIAGARA_DATA_OBJECT_DEV_UI_FIX)
		{
			Ar << DataConstants;
		}
		return true;
	}
};

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
struct FNiagaraConstants_Float : public FNiagaraConstantBase
{
	GENERATED_USTRUCT_BODY()

	FNiagaraConstants_Float()
	: Value(0.0f)
	{}

	FNiagaraConstants_Float(FName InName, float InValue)
	: FNiagaraConstantBase(InName)
	, Value(InValue)
	{}

	UPROPERTY(EditAnywhere, Category = "Constant")
	float Value;
};

USTRUCT()
struct FNiagaraConstants_Vector : public FNiagaraConstantBase
{
	GENERATED_USTRUCT_BODY()

	FNiagaraConstants_Vector()
	: Value(FVector4(1.0f, 1.0f, 1.0f, 1.0f))
	{}

	FNiagaraConstants_Vector(FName InName, const FVector4& InValue)
	: FNiagaraConstantBase(InName)
	, Value(InValue)
	{}

	UPROPERTY(EditAnywhere, Category = "Constant")
	FVector4 Value;
};

USTRUCT()
struct FNiagaraConstants_Matrix : public FNiagaraConstantBase
{
	GENERATED_USTRUCT_BODY()

	FNiagaraConstants_Matrix()
	: Value(FMatrix::Identity)
	{}

	FNiagaraConstants_Matrix(FName InName, const FMatrix& InValue)
	: FNiagaraConstantBase(InName)
	, Value(InValue)
	{}

	UPROPERTY(EditAnywhere, Category = "Constant")
	FMatrix Value;
};

USTRUCT()
struct FNiagaraConstants_DataObject : public FNiagaraConstantBase
{
	GENERATED_USTRUCT_BODY()

	FNiagaraConstants_DataObject()
	: Value(nullptr)
	{}

	FNiagaraConstants_DataObject(FName InName, UNiagaraDataObject* InValue)
	: FNiagaraConstantBase(InName)
	, Value(InValue)
	{}

	UPROPERTY(Instanced, VisibleAnywhere, Category = "Constant")
	UNiagaraDataObject* Value;
};

//Dummy struct used to serialize in the old layout of FNiagaraConstants
//This should be removed once everyone has recompiled and saved their scripts.
USTRUCT()
struct FDeprecatedNiagaraConstants
{
	GENERATED_USTRUCT_BODY()

	//DEPRECATED PROPERTIES. REMOVE SOON!
	UPROPERTY()
	TArray<FNiagaraVariableInfo> ScalarConstantsInfo_DEPRECATED;
	UPROPERTY()
	TArray<FNiagaraVariableInfo> VectorConstantsInfo_DEPRECATED;
	UPROPERTY()
	TArray<FNiagaraVariableInfo> MatrixConstantsInfo_DEPRECATED;
	//UPROPERTY()
	TArray<FNiagaraVariableInfo> DataObjectConstantsInfo_DEPRECATED;
	UPROPERTY()
	TArray<float> ScalarConstants_DEPRECATED;
	UPROPERTY()
	TArray<FVector4> VectorConstants_DEPRECATED;
	UPROPERTY()
	TArray<FMatrix> MatrixConstants_DEPRECATED;
	//UPROPERTY()
	TArray<void*> DataObjectConstants_DEPRECATED;
};

USTRUCT()
struct FNiagaraConstants
{
	GENERATED_USTRUCT_BODY()
	
public:

	UPROPERTY(EditAnywhere, EditFixedSize, Category = "Constant")
	TArray<FNiagaraConstants_Float> ScalarConstants;
	UPROPERTY(EditAnywhere, EditFixedSize, Category = "Constant")
	TArray<FNiagaraConstants_Vector> VectorConstants;
	UPROPERTY(EditAnywhere, EditFixedSize, Category = "Constant")
	TArray<FNiagaraConstants_Matrix> MatrixConstants;
	UPROPERTY(EditAnywhere, EditFixedSize, Category = "Constant")
	TArray<FNiagaraConstants_DataObject> DataObjectConstants;

	NIAGARA_API void Empty();
	NIAGARA_API void GetScalarConstant(int32 Index, float& OutValue, FNiagaraVariableInfo& OutInfo);
	NIAGARA_API void GetVectorConstant(int32 Index, FVector4& OutValue, FNiagaraVariableInfo& OutInfo);
	NIAGARA_API void GetMatrixConstant(int32 Index, FMatrix& OutValue, FNiagaraVariableInfo& OutInfo);
	NIAGARA_API void GetDataObjectConstant(int32 Index, UNiagaraDataObject*& OutValue, FNiagaraVariableInfo& OutInfo);

	const int32 GetNumScalarConstants()const{ return ScalarConstants.Num(); }
	const int32 GetNumVectorConstants()const{ return VectorConstants.Num(); }
	const int32 GetNumMatrixConstants()const{ return MatrixConstants.Num(); }
	const int32 GetNumDataObjectConstants()const{ return DataObjectConstants.Num(); }

	NIAGARA_API void Init(class UNiagaraEmitterProperties* EmitterProps, struct FNiagaraEmitterScriptProperties* ScriptProps);

	/**
	Copies constants in from the script as usual but allows overrides from old data in an FNiagaraConstantMap.
	Only used for BC. DO NOT USE.
	*/
	NIAGARA_API void InitFromOldMap(FNiagaraConstantMap& OldMap);

	/** Fills the entire constants set into the constant table. */
	void AppendToConstantsTable(TArray<FVector4>& ConstantsTable)const;

	void AppendBufferConstants(TArray<class UNiagaraDataObject*> &Objs) const;

	/** 
	Fills only selected constants into the table. In the order they appear in the array of passed names not the order they appear in the set. 
	Checks the passed map for entires to superceed the default values in the set.
	*/
	void AppendToConstantsTable(TArray<FVector4>& ConstantsTable, const FNiagaraConstantMap& Externals)const;
	void AppendExternalBufferConstants(TArray<class UNiagaraDataObject*> &DataObjs, const FNiagaraConstantMap& Externals) const;

	NIAGARA_API void SetOrAdd(const FNiagaraVariableInfo& Constant, float Value);
	NIAGARA_API void SetOrAdd(const FNiagaraVariableInfo& Constant, const FVector4& Value);
	NIAGARA_API void SetOrAdd(const FNiagaraVariableInfo& Constant, const FMatrix& Value);
	NIAGARA_API void SetOrAdd(const FNiagaraVariableInfo& Constant, UNiagaraDataObject* Value);
	NIAGARA_API void SetOrAdd(FName Name, float Value);
	NIAGARA_API void SetOrAdd(FName Name, const FVector4& Value);
	NIAGARA_API void SetOrAdd(FName Name, const FMatrix& Value);
	NIAGARA_API void SetOrAdd(FName Name, UNiagaraDataObject* Value);

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
	NIAGARA_API UNiagaraDataObject* FindDataObj(FName Name);

	/** Return the first constant used for this constant in a table. */
	FORCEINLINE int32 GetTableIndex_Scalar(const FNiagaraVariableInfo& Constant)const
	{ 
		int Idx = ScalarConstants.IndexOfByPredicate([&](const FNiagaraConstants_Float& V){ return Constant.Name == V.Name; });
		if (Idx != INDEX_NONE)
		{
			return Idx;
		}
		return INDEX_NONE;
	}

	FORCEINLINE int32 GetTableIndex_Vector(const FNiagaraVariableInfo& Constant)const
	{ 
		int Idx = VectorConstants.IndexOfByPredicate([&](const FNiagaraConstants_Vector& V){ return Constant.Name == V.Name; });
		if (Idx != INDEX_NONE)
		{
			return ScalarTableSize() + Idx;
		}
		return INDEX_NONE;
	}

	FORCEINLINE int32 GetTableIndex_Matrix(const FNiagaraVariableInfo& Constant)const
	{ 
		int Idx = MatrixConstants.IndexOfByPredicate([&](const FNiagaraConstants_Matrix& V){ return Constant.Name == V.Name; });
		if (Idx != INDEX_NONE)
		{
			return ScalarTableSize() + VectorTableSize() + Idx * 4;
		}
		return INDEX_NONE;
	}

	FORCEINLINE int32 GetTableIndex_DataObj(const FNiagaraVariableInfo& Constant)const
	{ 
		return DataObjectConstants.IndexOfByPredicate([&](const FNiagaraConstants_DataObject& V){ return Constant.Name == V.Name; });
	}
};
