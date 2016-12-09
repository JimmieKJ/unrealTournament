// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "ScriptStructArchiveProxy.h"
#include "UnrealHeaderTool.h"
#include "UObject/Package.h"
#include "UHTMakefile.h"

FScriptStructArchiveProxy::FScriptStructArchiveProxy(FUHTMakefile& UHTMakefile, const UScriptStruct* ScriptStruct)
	: FStructArchiveProxy(UHTMakefile, ScriptStruct)
{
	StructFlagsUInt32 = (uint32)ScriptStruct->StructFlags;
	StructMacroDeclaredLineNumber = ScriptStruct->StructMacroDeclaredLineNumber;
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
	bool bRemovedCppStructOpsFromBaseClass = false;
	Ar << bRemovedCppStructOpsFromBaseClass;
	Ar << ScriptStructArchiveProxy.bPrepareCppStructOpsCompleted;

	return Ar;
}
