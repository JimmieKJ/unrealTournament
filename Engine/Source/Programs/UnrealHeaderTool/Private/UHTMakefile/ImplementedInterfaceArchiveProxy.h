// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Class.h"
#include "MakefileHelpers.h"

class FArchive;
class FUHTMakefile;

/* See UHTMakefile.h for overview how makefiles work. */
struct FImplementedInterfaceArchiveProxy
{
	FImplementedInterfaceArchiveProxy() { }
	FImplementedInterfaceArchiveProxy(FUHTMakefile& UHTMakefile, const FImplementedInterface& ImplementedInterface);

	FImplementedInterface CreateImplementedInterface(const FUHTMakefile& UHTMakefile) const;
	void Resolve(FImplementedInterface& ImplementedInterface, const FUHTMakefile& UHTMakefile) const;

	friend FArchive& operator<<(FArchive& Ar, FImplementedInterfaceArchiveProxy& ImplementedInterfaceArchiveProxy);

	FSerializeIndex ClassIndex;
	int32 PointerOffset;
	bool bImplementedByK2;
};
