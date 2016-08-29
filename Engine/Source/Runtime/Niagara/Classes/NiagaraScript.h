// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

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
	/** The attribute is passed through without double buffering */
	PassThrough,
};

USTRUCT()
struct FNiagaraDataSetProperties
{
	GENERATED_BODY()
	
	UPROPERTY(VisibleAnywhere, Category = "Data Set")
	FNiagaraDataSetID ID;

	UPROPERTY()
	TArray<FNiagaraVariableInfo> Variables;
};

/** Struct containt usage information about a script. Things such as whether it reads attribute data, reads or writes events data etc.*/
USTRUCT()
struct FNiagaraScriptUsageInfo
{
	GENERATED_BODY()
	
	FNiagaraScriptUsageInfo()
	: bReadsAttriubteData(false)
	{}

	/** If true, this script reads attribute data. */
	UPROPERTY()
	bool bReadsAttriubteData;
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

	/** Information about the events this script receives and which variables are accessed. */
	UPROPERTY()
	TArray<FNiagaraDataSetProperties> EventReceivers;

	/** Information about the events this script generates and which variables are written. */
	UPROPERTY()
	TArray<FNiagaraDataSetProperties> EventGenerators;

	/** Contains various usage information for this script. */
	UPROPERTY()
	FNiagaraScriptUsageInfo Usage;

#if WITH_EDITORONLY_DATA
	/** 'Source' data/graphs for this script */
	UPROPERTY()
	class UNiagaraScriptSourceBase*	Source;
#endif
};