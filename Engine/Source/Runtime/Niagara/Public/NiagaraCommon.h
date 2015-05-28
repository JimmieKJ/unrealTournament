// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraCommon.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogNiagara, Log, Verbose);

UENUM()
enum class ENiagaraDataType : uint8
{
	Scalar,
	Vector,
	Matrix,
	Curve,
};

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
