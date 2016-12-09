// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Class.h"

class FUHTMakefile;
class FArchive;

/* See UHTMakefile.h for overview how makefiles work. */
struct FRepRecordArchiveProxy
{
	FRepRecordArchiveProxy() { }
	FRepRecordArchiveProxy(const FUHTMakefile& UHTMakefile, const FRepRecord& RepRecord);

	int32 PropertyIndex;
	int32 Index;

	FRepRecord CreateRepRecord(const FUHTMakefile& UHTMakefile) const;

	friend FArchive& operator<<(FArchive& Ar, FRepRecordArchiveProxy& RepRecordArchiveProxy);

	void Resolve(FRepRecord& RepRecord, const FUHTMakefile& UHTMakefile) const;
};
