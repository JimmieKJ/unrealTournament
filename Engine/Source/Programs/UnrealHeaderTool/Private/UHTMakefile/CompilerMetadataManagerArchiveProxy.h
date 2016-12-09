// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ClassMetaDataArchiveProxy.h"

class UStruct;
class FClassMetaData;
class FArchive;
class FCompilerMetadataManager;
class FUHTMakefile;

/* See UHTMakefile.h for overview how makefiles work. */
struct FCompilerMetadataManagerArchiveProxy
{
	FCompilerMetadataManagerArchiveProxy(const FUHTMakefile& UHTMakefile, TArray<TPair<UStruct*, FClassMetaData*>>& CompilerMetadataManager);
	FCompilerMetadataManagerArchiveProxy() { }

	static void AddReferencedNames(const FCompilerMetadataManager* CompilerMetadataManager, FUHTMakefile& UHTMakefile);

	friend FArchive& operator<<(FArchive& Ar, FCompilerMetadataManagerArchiveProxy& CompilerMetadataManagerArchiveProxy);
	void Resolve(int32 Index, FCompilerMetadataManager& CompilerMetadataManager, FUHTMakefile& UHTMakefile);

	TArray<TPair<FSerializeIndex, FClassMetaDataArchiveProxy>> Array;
};
