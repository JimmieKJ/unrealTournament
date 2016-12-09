// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Templates/ScopedPointer.h"
#include "ClassMaps.h"
#include "MultipleInheritanceBaseClassArchiveProxy.h"
#include "PropertyDataArchiveProxy.h"
#include "UniquePtr.h"

class FArchive;
class FUHTMakefile;
class FClassMetaData;

/* See UHTMakefile.h for overview how makefiles work. */
struct FClassMetaDataArchiveProxy
{
	FClassMetaDataArchiveProxy(const FUHTMakefile& UHTMakefile, const FClassMetaData* ClassMetaData);
	FClassMetaDataArchiveProxy() { }

	static void AddReferencedNames(const FClassMetaData* FClassMetaData, FUHTMakefile& UHTMakefile);

	friend FArchive& operator<<(FArchive& Ar, FClassMetaDataArchiveProxy& ClassMetaDataArchiveProxy);
	TUniquePtr<FClassMetaData> CreateClassMetaData() const;
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
