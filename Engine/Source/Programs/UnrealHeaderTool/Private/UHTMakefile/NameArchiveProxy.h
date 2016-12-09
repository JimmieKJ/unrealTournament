// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/OutputDevice.h"

class FArchive;
class FUHTMakefile;

/* See UHTMakefile.h for overview how makefiles work. */
struct FNameArchiveProxy
{
	FNameArchiveProxy() { }
	FNameArchiveProxy(const FUHTMakefile& UHTMakefile, FName Name);

	FName CreateName(const FUHTMakefile& UHTMakefile) const;

	friend FArchive& operator<<(FArchive& Ar, FNameArchiveProxy& NameArchiveProxy);

	int32 NameMapIndex;
	int32 Number;
};
