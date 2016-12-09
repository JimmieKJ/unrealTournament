// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PropertyDataArchiveProxy.h"
#include "TokenArchiveProxy.h"

class FArchive;
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
