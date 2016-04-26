// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealHeaderTool.h"
#include "UHTMakefile/UHTMakefile.h"
#include "UHTMakefile/ScopeArchiveProxy.h"
#include "Scope.h"

FArchive& operator<<(FArchive& Ar, FScopeArchiveProxy& ScopeArchiveProxy)
{
	Ar << ScopeArchiveProxy.ParentIndex;
	Ar << ScopeArchiveProxy.TypeMap;

	return Ar;
}

FScopeArchiveProxy::FScopeArchiveProxy(const FUHTMakefile& UHTMakefile, const FScope* Scope)
{
	ParentIndex = UHTMakefile.GetScopeIndex(Scope->GetParent());
	for (auto& Kvp : Scope->TypeMap)
	{
		TypeMap.Add(TPairInitializer<FNameArchiveProxy, FSerializeIndex>(FNameArchiveProxy(UHTMakefile, Kvp.Key), UHTMakefile.GetFieldIndex(Kvp.Value)));
	}
}

void FScopeArchiveProxy::AddReferencedNames(const FScope* Scope, FUHTMakefile& UHTMakefile)
{
	for (auto& Kvp : Scope->TypeMap)
	{
		UHTMakefile.AddName(Kvp.Key);
	}
}

void FScopeArchiveProxy::Resolve(FScope* Scope, const FUHTMakefile& UHTMakefile) const
{
	Scope->Parent = UHTMakefile.GetScopeByIndex(ParentIndex);
	for (auto& Kvp : TypeMap)
	{
		FName Name = Kvp.Key.CreateName(UHTMakefile);
		UField* Field = UHTMakefile.GetFieldByIndex(Kvp.Value);
		Scope->TypeMap.Add(Name, Field);
	}
}
