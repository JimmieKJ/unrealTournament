// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Niagara/NiagaraScriptConstantData.h"

#include "NiagaraScript.generated.h"

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
	TArray<FName> Attributes;

#if WITH_EDITORONLY_DATA
	/** 'Source' data/graphs for this script */
	UPROPERTY()
	class UNiagaraScriptSourceBase*	Source;
#endif
};