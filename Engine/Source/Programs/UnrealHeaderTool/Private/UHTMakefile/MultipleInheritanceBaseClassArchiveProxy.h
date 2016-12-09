// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MakefileHelpers.h"

class FArchive;
class FUHTMakefile;
struct FMultipleInheritanceBaseClass;

/* See UHTMakefile.h for overview how makefiles work. */
struct FMultipleInheritanceBaseClassArchiveProxy
{
	FMultipleInheritanceBaseClassArchiveProxy(const FUHTMakefile& UHTMakefile, const FMultipleInheritanceBaseClass* MultipleInheritanceBaseClass);
	FMultipleInheritanceBaseClassArchiveProxy() { }

	static void AddReferencedNames(const FMultipleInheritanceBaseClass* MultipleInheritanceBaseClass, FUHTMakefile& UHTMakefile);

	friend FArchive& operator<<(FArchive& Ar, FMultipleInheritanceBaseClassArchiveProxy& MultipleInheritanceBaseClassArchiveProxy);
	void Resolve(FMultipleInheritanceBaseClass* MultipleInheritanceBaseClass, const FUHTMakefile& UHTMakefile) const;
	FMultipleInheritanceBaseClass* CreateMultipleInheritanceBaseClass() const;

	FString ClassName;
	FSerializeIndex InterfaceClassIndex;
};
