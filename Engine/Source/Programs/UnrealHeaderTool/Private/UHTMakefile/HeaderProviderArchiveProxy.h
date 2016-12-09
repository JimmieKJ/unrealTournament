// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/UnrealString.h"

class FArchive;
class FUHTMakefile;
class FHeaderProvider;

/* See UHTMakefile.h for overview how makefiles work. */
struct FHeaderProviderArchiveProxy
{
	FHeaderProvider CreateHeaderProvider() const;
	void ResolveHeaderProvider(FHeaderProvider& HeaderProvider, const FUHTMakefile& UHTMakefile) const;

	friend FArchive& operator<<(FArchive& Ar, FHeaderProviderArchiveProxy& HeaderProviderArchiveProxy);
	uint8 Type;
	FString Id;
	int32 CacheIndex;
	bool bAutoInclude;
};
