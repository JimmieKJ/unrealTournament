// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

struct FTokenData;
class FArchive;
class FUHTMakefile;

/* See UHTMakefile.h for overview how makefiles work. */
struct FTokenDataArchiveProxy
{
	FTokenDataArchiveProxy(const FUHTMakefile& UHTMakefile, const FTokenData* TokenData);
	FTokenDataArchiveProxy() { }

	static void AddReferencedNames(const FTokenData* TokenData, FUHTMakefile& UHTMakefile);

	friend FArchive& operator<<(FArchive& Ar, FTokenDataArchiveProxy& TokenDataArchiveProxy);
	FTokenData* CreateTokenData(const FUHTMakefile& UHTMakefile) const;
	void Resolve(FTokenData& TokenData, const FUHTMakefile& UHTMakefile) const;

	int32 TokenIndex;
};
