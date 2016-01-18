// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class FUHTMakefile;
struct FNameLookupCPP;

/* See UHTMakefile.h for overview how makefiles work. */
struct FNameLookupCPPArchiveProxy
{
	FNameLookupCPPArchiveProxy(const FUHTMakefile& UHTMakefile, const FNameLookupCPP* NameLookupCPP);
	FNameLookupCPPArchiveProxy() { }

	friend FArchive& operator<<(FArchive& Ar, FNameLookupCPPArchiveProxy& NameLookupCPPArchiveProxy);
	void Resolve(FNameLookupCPP* NameLookupCPP, FUHTMakefile& UHTMakefile);

	TArray<TPair<FSerializeIndex, FString>> StructNameMap;
	TArray<FString> InterfaceAllocations;
};
