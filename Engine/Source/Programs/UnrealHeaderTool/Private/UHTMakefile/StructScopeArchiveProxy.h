// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ScopeArchiveProxy.h"

class FArchive;
class FUHTMakefile;
class FStructScope;

/* See UHTMakefile.h for overview how makefiles work. */
struct FStructScopeArchiveProxy : public FScopeArchiveProxy
{
	FStructScopeArchiveProxy(const FUHTMakefile& UHTMakefile, const FStructScope* StructScope);
	FStructScopeArchiveProxy() { }

	static void AddReferencedNames(const FStructScope* StructScope, FUHTMakefile& UHTMakefile);

	friend FArchive& operator<<(FArchive& Ar, FStructScopeArchiveProxy& StructScopeArchiveProxy);
	FStructScope* CreateStructScope(const FUHTMakefile& UHTMakefile) const;
	void Resolve(FStructScope* StructScope, const FUHTMakefile& UHTMakefile) const;

	FSerializeIndex StructIndex;
};
