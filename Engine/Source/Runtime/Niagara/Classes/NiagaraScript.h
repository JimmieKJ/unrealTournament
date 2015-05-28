// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraScriptConstantData.h"

#include "NiagaraScript.generated.h"

#define NIAGARA_INVALID_MEMORY (0xBA)

/** Defines what will happen to unused attributes when a script is run. */
UENUM()
enum class EUnusedAttributeBehaviour : uint8
{
	/** The previous value of the attribute is copied across. */
	Copy,
	/** The attribute is set to zero. */
	Zero,
	/** The attribute is untouched. */
	None,
	/** The memory for the attribute is set to NIAGARA_INVALID_MEMORY. */
	MarkInvalid, 
};

/** Runtime script for a Niagara system */
UCLASS(MinimalAPI)
class UNiagaraScript : public UObject
{
	GENERATED_UCLASS_BODY()

	/** Byte code to execute for this system */
	UPROPERTY()
	TArray<uint8> ByteCode;

	/** All the data for using constants in the script. */
	UPROPERTY()
	FNiagaraScriptConstantData ConstantData;

	/** Attributes used by this script. */
	UPROPERTY()
 	TArray<FNiagaraVariableInfo> Attributes;

#if WITH_EDITORONLY_DATA
	/** 'Source' data/graphs for this script */
	UPROPERTY()
	class UNiagaraScriptSourceBase*	Source;
#endif
};