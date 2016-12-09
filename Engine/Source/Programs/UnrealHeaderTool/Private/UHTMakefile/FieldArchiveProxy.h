// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ObjectBaseArchiveProxy.h"

class UField;
class FArchive;
class FUHTMakefile;

/* See UHTMakefile.h for overview how makefiles work. */
struct FFieldArchiveProxy : public FObjectBaseArchiveProxy
{
	FFieldArchiveProxy() { }
	FFieldArchiveProxy(FUHTMakefile& UHTMakefile, const UField* Field);

	void Resolve(UField* Field, const FUHTMakefile& UHTMakefile) const;
	static void AddReferencedNames(const UField* Field, FUHTMakefile& UHTMakefile);

	friend FArchive& operator<<(FArchive& Ar, FFieldArchiveProxy& FieldArchiveProxy);

	FSerializeIndex NextIndex;
};
