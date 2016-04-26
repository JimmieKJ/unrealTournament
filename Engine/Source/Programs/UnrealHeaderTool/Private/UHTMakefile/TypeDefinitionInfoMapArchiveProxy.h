// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "UnrealTypeDefinitionInfo.h"

class FUnrealTypeDefinitionInfo;

/* See UHTMakefile.h for overview how makefiles work. */
struct FTypeDefinitionInfoMapArchiveProxy
{
	FTypeDefinitionInfoMapArchiveProxy(FUHTMakefile& UHTMakefile, TArray<TPair<UField*, FUnrealTypeDefinitionInfo*>>& TypeDefinitionInfoPairs);
	FTypeDefinitionInfoMapArchiveProxy() { }

	TArray<TPair<FSerializeIndex, int32>> TypeDefinitionInfoIndexes;

	friend FArchive& operator<<(FArchive& Ar, FTypeDefinitionInfoMapArchiveProxy& FileScopeArchiveProxy)
	{
		Ar << FileScopeArchiveProxy.TypeDefinitionInfoIndexes;

		return Ar;
	}

	void ResolveIndex(FUHTMakefile& UHTMakefile, int32 Index);
	void ResolveClassIndex(FUHTMakefile& UHTMakefile, int32 Index);
};