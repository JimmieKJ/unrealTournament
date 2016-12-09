// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Array.h"

class FArchive;
class FUHTMakefile;
class FPropertyData;

/* See UHTMakefile.h for overview how makefiles work. */
struct FPropertyDataArchiveProxy
{
	FPropertyDataArchiveProxy(const FUHTMakefile& UHTMakefile, const FPropertyData* PropertyData);
	FPropertyDataArchiveProxy() { }

	static void AddReferencedNames(const FPropertyData* PropertyData, FUHTMakefile& UHTMakefile);

	friend FArchive& operator<<(FArchive& Ar, FPropertyDataArchiveProxy& PropertyDataArchiveProxy);
	FPropertyData* CreatePropertyData(const FUHTMakefile& UHTMakefile) const;
	void Resolve(FPropertyData* PropertyData, FUHTMakefile& UHTMakefile);

	TArray<int32> EntriesIndexes;
};
