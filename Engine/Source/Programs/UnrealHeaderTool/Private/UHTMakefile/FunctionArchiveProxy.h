// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Class.h"
#include "StructArchiveProxy.h"

class FUHTMakefile;
class UFunction;
class FArchive;
class UDelegateFunction;

/* See UHTMakefile.h for overview how makefiles work. */
struct FFunctionArchiveProxy : public FStructArchiveProxy
{
	FFunctionArchiveProxy() { }
	FFunctionArchiveProxy(FUHTMakefile& UHTMakefile, const UFunction* Function);

	UFunction* CreateFunction(const FUHTMakefile& UHTMakefile) const;
	void PostConstruct(UFunction* Function) const;
	void Resolve(UFunction* Function, const FUHTMakefile& UHTMakefile) const;

	friend FArchive& operator<<(FArchive& Ar, FFunctionArchiveProxy& FunctionArchiveProxy);

	static void AddReferencedNames(const UFunction* Function, FUHTMakefile& UHTMakefile)
	{
		FStructArchiveProxy::AddReferencedNames(Function, UHTMakefile);
	}

	uint32 FunctionFlags;
	uint16 RepOffset;
	uint8 NumParms;
	uint16 ParmsSize;
	uint16 ReturnValueOffset;
	uint16 RPCId;
	uint16 RPCResponseId;
	int32 FirstPropertyToInitIndex;
	int32 EventGraphFunctionIndex;
	int32 EventGraphCallOffset;
};

struct FDelegateFunctionArchiveProxy : public FFunctionArchiveProxy
{
	FDelegateFunctionArchiveProxy() { }
	FDelegateFunctionArchiveProxy(FUHTMakefile& UHTMakefile, const UDelegateFunction* DelegateFunction);
	UDelegateFunction* CreateDelegateFunction(const FUHTMakefile& UHTMakefile) const;
	void PostConstruct(UDelegateFunction* DelegateFunction) const;
	static void AddReferencedNames(const UDelegateFunction* DelegateFunction, FUHTMakefile& UHTMakefile)
	{
		FFunctionArchiveProxy::AddReferencedNames(DelegateFunction, UHTMakefile);
	}
};

