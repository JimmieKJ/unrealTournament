// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#include "UnrealHeaderTool.h"
#include "UHTMakefile/UHTMakefile.h"
#include "UHTMakefile/NameLookupCPPArchiveProxy.h"

FNameLookupCPPArchiveProxy::FNameLookupCPPArchiveProxy(const FUHTMakefile& UHTMakefile, const FNameLookupCPP* InNameLookupCPP)
{
	// TODO: This really probably shouldn't be a local variable and should be populating the member variable instead
	TArray<TPair<FSerializeIndex, FString>> StructNameMapLocal;
	StructNameMapLocal.Empty(InNameLookupCPP->StructNameMap.Num());
	for (auto& Kvp : InNameLookupCPP->StructNameMap)
	{
		StructNameMapLocal.Add(TPairInitializer<FSerializeIndex, FString>(UHTMakefile.GetStructIndex(Kvp.Key), Kvp.Value));
	}
	// TODO: This really probably shouldn't be a local variable and should be populating the member variable instead
	TArray<FString> InterfaceAllocationsLocal;
	InterfaceAllocationsLocal.Empty(InNameLookupCPP->InterfaceAllocations.Num());
	for (TCHAR* InterfaceAllocation : InNameLookupCPP->InterfaceAllocations)
	{
		InterfaceAllocationsLocal.Add(InterfaceAllocation);
	}
}

FArchive& operator<<(FArchive& Ar, FNameLookupCPPArchiveProxy& NameLookupCPPArchiveProxy)
{
	Ar << NameLookupCPPArchiveProxy.InterfaceAllocations;
	Ar << NameLookupCPPArchiveProxy.StructNameMap;

	return Ar;
}

void FNameLookupCPPArchiveProxy::Resolve(FNameLookupCPP* InNameLookupCPP, FUHTMakefile& UHTMakefile)
{

}
