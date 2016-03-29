// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "UHTMakefile/PropertyDataArchiveProxy.h"
#include "UHTMakefile/TokenArchiveProxy.h"

class FUHTMakefile;
class FStructData;

/* See UHTMakefile.h for overview how makefiles work. */
struct FStructDataArchiveProxy
{
	FStructDataArchiveProxy(const FUHTMakefile& UHTMakefile, const FStructData* StructData);
	FStructDataArchiveProxy() { }

	static void AddReferencedNames(const FStructData* StructData, FUHTMakefile& UHTMakefile);

	friend FArchive& operator<<(FArchive& Ar, FStructDataArchiveProxy& StructDataArchiveProxy);
	FStructData* CreateStructData(FUHTMakefile& UHTMakefile);

	FTokenArchiveProxy StructData;
	FPropertyDataArchiveProxy StructPropertyData;
};
