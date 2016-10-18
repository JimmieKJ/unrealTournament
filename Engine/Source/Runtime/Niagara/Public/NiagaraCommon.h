// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraCommon.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogNiagara, Log, Verbose);

UENUM()
enum class ENiagaraDataType : uint8
{
	Scalar,
	Vector,
	Matrix,
};


enum class ENiagaraType : uint8
{
	Byte,
	Word,
	DWord,
	Float 
};

enum ENiagaraSimTarget
{
	CPUSim,
	GPUComputeSim
};


enum ENiagaraTickState
{
	NTS_Running,
	NTS_Suspended,
	NTS_Dieing,
	NTS_Dead
};


uint32 GetNiagaraDataElementCount(ENiagaraDataType Type);
uint32 GetNiagaraBytesPerElement(ENiagaraDataType DataType, ENiagaraType Type);



USTRUCT()
struct FNiagaraVariableInfo
{
	GENERATED_USTRUCT_BODY()

	FNiagaraVariableInfo()
	: Name(NAME_None)
	, Type(ENiagaraDataType::Vector)
	{}

	FNiagaraVariableInfo(FName InName, ENiagaraDataType InType)
	: Name(InName)
	, Type(InType)
	{}

	UPROPERTY(EditAnywhere, Category = "Variable")
	FName Name;

	UPROPERTY(EditAnywhere, Category = "Variable")
	TEnumAsByte<ENiagaraDataType> Type;
	
	FORCEINLINE bool operator==(const FNiagaraVariableInfo& Other)const
	{
		return Name == Other.Name && Type == Other.Type;
	}

	FORCEINLINE bool operator!=(const FNiagaraVariableInfo& Other)const
	{
		return !(*this == Other);
	}
};

FORCEINLINE FArchive& operator<<(FArchive& Ar, FNiagaraVariableInfo& VarInfo)
{
	Ar << VarInfo.Name << VarInfo.Type;
	return Ar;
}

FORCEINLINE uint32 GetTypeHash(const FNiagaraVariableInfo& Var)
{
	return HashCombine(GetTypeHash(Var.Name), (uint32)(Var.Type.GetValue()));
}


UENUM()
enum class ENiagaraDataSetType : uint8
{
	ParticleData,
	Shared,
	Event,
};

USTRUCT()
struct FNiagaraDataSetID
{
	GENERATED_USTRUCT_BODY()

	FNiagaraDataSetID()
	: Name(NAME_None)
	, Type(ENiagaraDataSetType::Event)
	{}

	FNiagaraDataSetID(FName InName, ENiagaraDataSetType InType)
		: Name(InName)
		, Type(InType)
	{}

	UPROPERTY(EditAnywhere, Category = "Data Set")
	FName Name;

	UPROPERTY()
	TEnumAsByte<ENiagaraDataSetType> Type;

	FORCEINLINE bool operator==(const FNiagaraDataSetID& Other)const
	{
		return Name == Other.Name && Type == Other.Type;
	}

	FORCEINLINE bool operator!=(const FNiagaraDataSetID& Other)const
	{
		return !(*this == Other);
	}

	//Some special case data sets.
	static FNiagaraDataSetID DeathEvent;
	static FNiagaraDataSetID SpawnEvent;
	static FNiagaraDataSetID CollisionEvent;
};

FORCEINLINE FArchive& operator<<(FArchive& Ar, FNiagaraDataSetID& VarInfo)
{
	Ar << VarInfo.Name << VarInfo.Type;
	return Ar;
}

FORCEINLINE uint32 GetTypeHash(const FNiagaraDataSetID& Var)
{
	return HashCombine(GetTypeHash(Var.Name), (uint32)(Var.Type.GetValue()));
}
