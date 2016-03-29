// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "UHTMakefile/MultipleInheritanceBaseClassArchiveProxy.h"
#include "UHTMakefile/PropertyDataArchiveProxy.h"
#include "UHTMakefile/StructDataArchiveProxy.h"

class FUHTMakefile;
class FClassMetaData;

/* See UHTMakefile.h for overview how makefiles work. */
struct FClassMetaDataArchiveProxy
{
	FClassMetaDataArchiveProxy(const FUHTMakefile& UHTMakefile, const FClassMetaData* ClassMetaData);
	FClassMetaDataArchiveProxy() { }

	static void AddReferencedNames(const FClassMetaData* FClassMetaData, FUHTMakefile& UHTMakefile);

	friend FArchive& operator<<(FArchive& Ar, FClassMetaDataArchiveProxy& ClassMetaDataArchiveProxy);
	TScopedPointer<FClassMetaData> CreateClassMetaData() const;
	void PostConstruct(FClassMetaData* ClassMetaData) const;
	void Resolve(FClassMetaData* ClassMetaData, FUHTMakefile& UHTMakefile);

	FPropertyDataArchiveProxy GlobalPropertyData;
	TArray<FMultipleInheritanceBaseClassArchiveProxy> MultipleInheritanceParents;
	bool bContainsDelegates;
	int32 PrologLine;
	int32 GeneratedBodyLine;
	int32 InterfaceGeneratedBodyLine;
	bool bConstructorDeclared;
	bool bDefaultConstructorDeclared;
	bool bObjectInitializerConstructorDeclared;
	bool bCustomVTableHelperConstructorDeclared;
	EAccessSpecifier GeneratedBodyMacroAccessSpecifier;
};
