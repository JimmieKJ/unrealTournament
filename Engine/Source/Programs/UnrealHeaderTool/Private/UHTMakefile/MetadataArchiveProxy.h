// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Map.h"
#include "ObjectBaseArchiveProxy.h"

class UMetaData;
class FArchive;
class FUHTMakefile;

/* See UHTMakefile.h for overview how makefiles work. */
struct FMetadataArchiveProxy : public FObjectBaseArchiveProxy
{
	FMetadataArchiveProxy() { }
	FMetadataArchiveProxy(FUHTMakefile& UHTMakefile, UMetaData* Metadata);

	friend FArchive& operator<<(FArchive& Ar, FMetadataArchiveProxy& MetadataArchiveProxy);

	UMetaData* CreateMetadata(const FUHTMakefile& UHTMakefile) const;
	void Resolve(UMetaData* Metadata, const FUHTMakefile& UHTMakefile) const;

	TArray<TPair<FNameArchiveProxy, FNameArchiveProxy>> KeyRedirectMap;
	TArray<TPair<FSerializeIndex, TArray<TPair<FNameArchiveProxy, FString>>>> ObjectMetaDataMapProxy;

	static void AddReferencedNames(UMetaData* Metadata, FUHTMakefile& UHTMakefile);

	static void AddStaticallyReferencedNames(FUHTMakefile& UHTMakefile);
};
