// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "StructArchiveProxy.h"
#include "RepRecordArchiveProxy.h"
#include "ImplementedInterfaceArchiveProxy.h"

class FUHTMakefile;
class UClass;
class FArchive;

/* See UHTMakefile.h for overview how makefiles work. */
struct FClassArchiveProxy : public FStructArchiveProxy
{
	FClassArchiveProxy() { }
	FClassArchiveProxy(FUHTMakefile& UHTMakefile, const UClass* Class);

	UClass* CreateClass(const FUHTMakefile& UHTMakefile) const;
	void PostConstruct(UClass* Class) const;
	void Resolve(UClass* Class, const FUHTMakefile& UHTMakefile) const;

	friend FArchive& operator<<(FArchive& Ar, FClassArchiveProxy& ClassArchiveProxy);

	static void AddReferencedNames(const UClass* Class, FUHTMakefile& UHTMakefile);

	int32 ClassUnique;
	uint32 ClassFlags;
	EClassCastFlags ClassCastFlags;
	FSerializeIndex ClassWithinIndex;
	FSerializeIndex ClassGeneratedByIndex;
	FNameArchiveProxy ClassConfigName;
	bool bCooked;
	TArray<FRepRecordArchiveProxy> ClassReps;
	TArray<FSerializeIndex> NetFields;
	TArray<TPair<FNameArchiveProxy, int32>> FuncMap;
	TArray<FImplementedInterfaceArchiveProxy> Interfaces;
	TArray<FSerializeIndex> ConvertedSubobjectsFromBPGC;
};
