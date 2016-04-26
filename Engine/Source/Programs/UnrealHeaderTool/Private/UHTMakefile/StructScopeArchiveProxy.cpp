// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealHeaderTool.h"
#include "UHTMakefile/UHTMakefile.h"
#include "UHTMakefile/StructScopeArchiveProxy.h"
#include "Scope.h"

FArchive& operator<<(FArchive& Ar, FStructScopeArchiveProxy& StructScopeArchiveProxy)
{
	Ar << static_cast<FScopeArchiveProxy&>(StructScopeArchiveProxy);
	Ar << StructScopeArchiveProxy.StructIndex;

	return Ar;
}

FStructScopeArchiveProxy::FStructScopeArchiveProxy(const FUHTMakefile& UHTMakefile, const FStructScope* StructScope)
	: FScopeArchiveProxy(UHTMakefile, StructScope)
{
	StructIndex = UHTMakefile.GetStructIndex(StructScope->Struct);
}

void FStructScopeArchiveProxy::AddReferencedNames(const FStructScope* StructScope, FUHTMakefile& UHTMakefile)
{
	FScopeArchiveProxy::AddReferencedNames(StructScope, UHTMakefile);
}

FStructScope* FStructScopeArchiveProxy::CreateStructScope(const FUHTMakefile& UHTMakefile) const
{
	return new FStructScope(nullptr, nullptr);
}

void FStructScopeArchiveProxy::Resolve(FStructScope* StructScope, const FUHTMakefile& UHTMakefile) const
{
	FScopeArchiveProxy::Resolve(StructScope, UHTMakefile);
	StructScope->Struct = UHTMakefile.GetStructByIndex(StructIndex);

	FScope::ScopeMap.Add(StructScope->Struct, StructScope->HasBeenAlreadyMadeSharable() ? StructScope->AsShared() : MakeShareable(StructScope));
}
