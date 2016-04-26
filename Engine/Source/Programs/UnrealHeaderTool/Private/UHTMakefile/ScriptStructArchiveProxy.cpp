// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealHeaderTool.h"
#include "UHTMakefile/UHTMakefile.h"
#include "UHTMakefile/StructArchiveProxy.h"

FScriptStructArchiveProxy::FScriptStructArchiveProxy(FUHTMakefile& UHTMakefile, const UScriptStruct* ScriptStruct)
	: FStructArchiveProxy(UHTMakefile, ScriptStruct)
{
	StructFlagsUInt32 = (uint32)ScriptStruct->StructFlags;
	StructMacroDeclaredLineNumber = ScriptStruct->StructMacroDeclaredLineNumber;
	bCppStructOpsFromBaseClass = ScriptStruct->bCppStructOpsFromBaseClass;
	bPrepareCppStructOpsCompleted = ScriptStruct->bPrepareCppStructOpsCompleted;
}

UScriptStruct* FScriptStructArchiveProxy::CreateScriptStruct(const FUHTMakefile& UHTMakefile) const
{
	UObject* Outer = Cast<UPackage>(UHTMakefile.GetObjectByIndex(OuterIndex));
	UScriptStruct* SuperStruct = UHTMakefile.GetScriptStructByIndex(SuperStructIndex);
	FName StructName = Name.CreateName(UHTMakefile);
	UScriptStruct* ScriptStruct = new (EC_InternalUseOnlyConstructor, Outer, Name.CreateName(UHTMakefile), (EObjectFlags)ObjectFlagsUint32) UScriptStruct(FObjectInitializer(), SuperStruct);
	PostConstruct(ScriptStruct);


	return ScriptStruct;
}

void FScriptStructArchiveProxy::PostConstruct(UScriptStruct* ScriptStruct) const
{
	ScriptStruct->StructFlags = (EStructFlags)StructFlagsUInt32;
	ScriptStruct->StructMacroDeclaredLineNumber = StructMacroDeclaredLineNumber;
	ScriptStruct->bCppStructOpsFromBaseClass = bCppStructOpsFromBaseClass;
	ScriptStruct->bPrepareCppStructOpsCompleted = bPrepareCppStructOpsCompleted;
}

void FScriptStructArchiveProxy::Resolve(UScriptStruct* ScriptStruct, const FUHTMakefile& UHTMakefile) const
{
	FStructArchiveProxy::Resolve(ScriptStruct, UHTMakefile);
}

FArchive& operator<<(FArchive& Ar, FScriptStructArchiveProxy& ScriptStructArchiveProxy)
{
	Ar << static_cast<FStructArchiveProxy&>(ScriptStructArchiveProxy);
	Ar << ScriptStructArchiveProxy.StructFlagsUInt32;
	Ar << ScriptStructArchiveProxy.StructMacroDeclaredLineNumber;
	Ar << ScriptStructArchiveProxy.bCppStructOpsFromBaseClass;
	Ar << ScriptStructArchiveProxy.bPrepareCppStructOpsCompleted;

	return Ar;
}
