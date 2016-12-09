// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MetadataArchiveProxy.h"

class UPackage;
class FArchive;
class FUHTMakefile;

/* See UHTMakefile.h for overview how makefiles work. */
struct FPackageArchiveProxy : public FObjectBaseArchiveProxy
{
	FPackageArchiveProxy() { }
	FPackageArchiveProxy(FUHTMakefile& UHTMakefile, UPackage* Package);

	uint32 PackageFlags;
	FMetadataArchiveProxy MetadataArchiveProxy;

	static void AddReferencedNames(UPackage* Package, FUHTMakefile& UHTMakefile);

	friend FArchive& operator<<(FArchive& Ar, FPackageArchiveProxy& PackageArchiveProxy);
	void Resolve(UPackage* Package, const FUHTMakefile& UHTMakefile) const;
	UPackage* CreatePackage(const FUHTMakefile& UHTMakefile);
	void PostConstruct(UPackage* Package) const;
};
