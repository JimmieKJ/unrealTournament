// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "UHTMakefile/StructArchiveProxy.h"

/* See UHTMakefile.h for overview how makefiles work. */
struct FScriptStructArchiveProxy : public FStructArchiveProxy
{
	FScriptStructArchiveProxy() { }
	FScriptStructArchiveProxy(FUHTMakefile& UHTMakefile, const UScriptStruct* ScriptStruct);

	UScriptStruct* CreateScriptStruct(const FUHTMakefile& UHTMakefile) const;

	void PostConstruct(UScriptStruct* ScriptStruct) const;

	void Resolve(UScriptStruct* Struct, const FUHTMakefile& UHTMakefile) const;

	friend FArchive& operator<<(FArchive& Ar, FScriptStructArchiveProxy& StructArchiveProxy);
	static void AddReferencedNames(UScriptStruct* ScriptStruct, FUHTMakefile& UHTMakefile)
	{
		FStructArchiveProxy::AddReferencedNames(ScriptStruct, UHTMakefile);
	}

	uint32 StructFlagsUInt32;
	int32 StructMacroDeclaredLineNumber;
	bool bCppStructOpsFromBaseClass;
	bool bPrepareCppStructOpsCompleted;
};
