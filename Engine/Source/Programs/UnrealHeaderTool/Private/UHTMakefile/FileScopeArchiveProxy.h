// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ScopeArchiveProxy.h"

class FArchive;
class FUHTMakefile;
class FFileScope;

/* See UHTMakefile.h for overview how makefiles work. */
struct FFileScopeArchiveProxy : public FScopeArchiveProxy
{
	FFileScopeArchiveProxy(const FUHTMakefile& UHTMakefile, const FFileScope* FileScope);
	FFileScopeArchiveProxy() { }

	static void AddReferencedNames(const FFileScope* FileScope, FUHTMakefile& UHTMakefile);

	TArray<int32> IncludedScopesIndexes;
	int32 SourceFileIndex;
	FNameArchiveProxy Name;

	friend FArchive& operator<<(FArchive& Ar, FFileScopeArchiveProxy& FileScopeArchiveProxy);
	FFileScope* CreateFileScope(const FUHTMakefile& UHTMakefile) const;
	void PostConstruct(FFileScope* FileScope) const;
	void Resolve(FFileScope* FileScope, const FUHTMakefile& UHTMakefile) const;
};
