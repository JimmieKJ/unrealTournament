// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Niagara/NiagaraCommon.h"
#include "NiagaraConstantSet.generated.h"


USTRUCT()
struct FNiagaraConstantMap
{
	GENERATED_USTRUCT_BODY()

private:
	TMap<FName, float> ScalarConstants;
	TMap<FName, FVector4> VectorConstants;
	TMap<FName, FMatrix> MatrixConstants;

public:

	void SetOrAdd(FName Name, float Sc)
	{
		ScalarConstants.FindOrAdd(Name) = Sc;
	}

	void SetOrAdd(FName Name, const FVector4& Vc)
	{
		VectorConstants.FindOrAdd(Name) = Vc;
	}

	void SetOrAdd(FName Name, const FMatrix& Mc)
	{
		MatrixConstants.FindOrAdd(Name) = Mc;
	}

	float* FindScalar(FName InName){ return ScalarConstants.Find(InName); }
	FVector4* FindVector(FName InName){ return VectorConstants.Find(InName); }
	FMatrix* FindMatrix(FName InName){ return MatrixConstants.Find(InName); }
	const float* FindScalar(FName InName)const { return ScalarConstants.Find(InName); }
	const FVector4* FindVector(FName InName)const { return VectorConstants.Find(InName); }
	const FMatrix* FindMatrix(FName InName)const { return MatrixConstants.Find(InName); }

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
	}


	virtual bool Serialize(FArchive &Ar)
	{
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
	TArray<float> ScalarConstants;
	UPROPERTY()
	TArray<FName> ScalarNames;

	UPROPERTY()
	TArray<FVector4> VectorConstants;
	UPROPERTY()
	TArray<FName> VectorNames;

	UPROPERTY()
	TArray<FMatrix> MatrixConstants;
	UPROPERTY()
	TArray<FName> MatrixNames;

public:

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
		for (FVector4 Vc : VectorConstants)
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

	/** 
	Fills only selected constants into the table. In the order they appear in the array of passed names not the order they appear in the set. 
	Checks the passed map for entires to superceed the default values in the set.
	*/
	void AppendToConstantsTable(TArray<FVector4>& ConstantsTable, const FNiagaraConstantMap& Externals)const
	{
		int32 Idx = ConstantsTable.Num();
		int32 NewIdx = 0;
		ConstantsTable.AddUninitialized(ScalarTableSize() + VectorTableSize() + MatrixTableSize());
		for (int32 i = 0; i < ScalarNames.Num(); ++i)
		{
			FName ScName = ScalarNames[i];
			const float* Scalar = Externals.FindScalar(ScName);
			ConstantsTable[Idx + (NewIdx / 4)][NewIdx % 4] = Scalar ? *Scalar : ScalarConstants[i];
			++NewIdx;
			Idx = NewIdx % 4 == 0 ? Idx + 1 : Idx;
		}

		Idx += FMath::Min(1, NewIdx % 4);//Move to the next table entry if needed.

		for (int32 i = 0; i < VectorNames.Num(); ++i)
		{
			FName VcName = VectorNames[i];
			const FVector4* Vector = Externals.FindVector(VcName);
			ConstantsTable[Idx++] = Vector ? *Vector : VectorConstants[i];
		}

		for (int32 i = 0; i < MatrixNames.Num(); ++i)
		{
			FName McName = MatrixNames[i];
			const FMatrix* Mat = Externals.FindMatrix(McName);
			FMatrix Mc = Mat ? *Mat : MatrixConstants[i];
			ConstantsTable[Idx] = FVector4(Mc.M[0][0], Mc.M[0][1], Mc.M[0][2], Mc.M[0][3]);
			ConstantsTable[Idx + 1] = FVector4(Mc.M[1][0], Mc.M[1][1], Mc.M[1][2], Mc.M[1][3]);
			ConstantsTable[Idx + 2] = FVector4(Mc.M[2][0], Mc.M[2][1], Mc.M[2][2], Mc.M[2][3]);
			ConstantsTable[Idx + 3] = FVector4(Mc.M[3][0], Mc.M[3][1], Mc.M[3][2], Mc.M[3][3]);
		}
	}

	void SetOrAdd(FName Name, float Sc)
	{
		int32 Idx = ScalarNames.Find(Name);
		if (Idx == INDEX_NONE)
		{
			ScalarNames.Add(Name);
			ScalarConstants.Add(Sc);
		}
		else
		{
			ScalarConstants[Idx] = Sc;
		}

	}

	void SetOrAdd(FName Name, const FVector4& Vc)
	{
		int32 Idx = VectorNames.Find(Name);
		if (Idx == INDEX_NONE)
		{
			VectorNames.Add(Name);
			VectorConstants.Add(Vc);
		}
		else
		{
			VectorConstants[Idx] = Vc;
		}
	}

	void SetOrAdd(FName Name, const FMatrix& Mc)
	{
		int32 Idx = MatrixNames.Find(Name);
		if (Idx == INDEX_NONE)
		{
			MatrixNames.Add(Name);
			MatrixConstants.Add(Mc);
		}
		else
		{
			MatrixConstants[Idx] = Mc;
		}
	}

	FORCEINLINE int32 NumScalars()const { return ScalarNames.Num(); }
	FORCEINLINE int32 NumVectors()const { return VectorNames.Num(); }
	FORCEINLINE int32 NumMatrices()const { return MatrixNames.Num(); }
	FORCEINLINE int32 ScalarTableSize()const { return(NumScalars() / 4) + FMath::Min(1,NumScalars() % 4); }
	FORCEINLINE int32 VectorTableSize()const { return(NumVectors()); }
	FORCEINLINE int32 MatrixTableSize()const { return(NumMatrices() * 4); }

	/**	Returns the number of vectors these constants would use in a constants table. */
	FORCEINLINE int32 GetTableSize()const { return ScalarTableSize() + VectorTableSize() + MatrixTableSize(); }

	/** Return the first constant used for this constant in a table. */
	FORCEINLINE int32 GetTableIndex_Scalar(FName InName)const{ return ScalarNames.Find(InName) / 4; }
	FORCEINLINE int32 GetTableIndex_Vector(FName InName)const{ return ScalarTableSize() + VectorNames.Find(InName); }
	FORCEINLINE int32 GetTableIndex_Matrix(FName InName)const{ return ScalarTableSize() + VectorTableSize() + MatrixNames.Find(InName) * 4; }

	/** Return the absolute index of the passed scalar. Can be used to get the component index also. */
	FORCEINLINE int32 GetAbsoluteIndex_Scalar(FName InName)const{ return ScalarNames.Find(InName); }
};

