// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealHeaderTool.h"
#include "UHTMakefile/UHTMakefileHeaderDescriptor.h"
#include "UHTMakefile/UHTMakefile.h"
#include "ClassMaps.h"

FUHTMakefileHeaderDescriptor::FUHTMakefileHeaderDescriptor(FUHTMakefile* InUHTMakefile /*= nullptr*/)
	: UHTMakefile(InUHTMakefile)
	, SourceFile(nullptr)
	, LoadingPhase(EUHTMakefileLoadingPhase::Preload)
{
	InitializeObjectIndexes.AddDefaulted(+EUHTMakefileLoadingPhase::Max);
	for (uint8 i = 0; i < +EUHTMakefileLoadingPhase::Max; ++i)
	{
		bCreatedObjects[i] = bResolvedObjects[i] = false;
	}
}

void FUHTMakefileHeaderDescriptor::AddPrerequesites(TArray<FUnrealSourceFile*>& Prerequisites)
{
	PrerequisiteIndexes.Reserve(PrerequisiteIndexes.Num() + Prerequisites.Num());
	for (FUnrealSourceFile* UnrealSourceFile : Prerequisites)
	{
		PrerequisiteIndexes.Add(UHTMakefile->GetUnrealSourceFileIndex(UnrealSourceFile));
	}
}

void FUHTMakefileHeaderDescriptor::CreateObjects(EUHTMakefileLoadingPhase UHTMakefileLoadingPhase)
{
	if (bCreatedObjects[+UHTMakefileLoadingPhase])
	{
		return;
	}
	bCreatedObjects[+UHTMakefileLoadingPhase] = true;

	if (UHTMakefileLoadingPhase == EUHTMakefileLoadingPhase::Load)
	{
		// Handle prerequisites first.
		for (int32 PrerequesiteIndex : PrerequisiteIndexes)
		{
			UHTMakefile->GetHeaderDescriptorBySourceFileIndex(PrerequesiteIndex).CreateObjects(UHTMakefileLoadingPhase);
		}
	}

	// Create own objects
	for (int32 ObjectIndex : InitializeObjectIndexes[+UHTMakefileLoadingPhase])
	{
		UHTMakefile->CreateObjectAtInitOrderIndex(ObjectIndex, UHTMakefileLoadingPhase);
	}
}

void FUHTMakefileHeaderDescriptor::ResolveObjects(EUHTMakefileLoadingPhase UHTMakefileLoadingPhase)
{
	if (UHTMakefileLoadingPhase == EUHTMakefileLoadingPhase::Preload)
	{
		// Resolve own objects
		for (int32 ObjectIndex : InitializeObjectIndexes[+EUHTMakefileLoadingPhase::Preload])
		{
			UHTMakefile->ResolveTypeFullyCreatedDuringPreload(ObjectIndex);
		}

		return;
	}

	if (bResolvedObjects[+UHTMakefileLoadingPhase])
	{
		return;
	}

	bResolvedObjects[+UHTMakefileLoadingPhase] = true;

	if (UHTMakefileLoadingPhase == EUHTMakefileLoadingPhase::Load)
	{
		// Handle prerequisites first.
		for (int32 PrerequesityIndex : PrerequisiteIndexes)
		{
			UHTMakefile->GetHeaderDescriptor(UHTMakefile->GetUnrealSourceFileByIndex(PrerequesityIndex)).ResolveObjects(UHTMakefileLoadingPhase);
		}

		for (int32 ObjectIndex : InitializeObjectIndexes[+EUHTMakefileLoadingPhase::Preload])
		{
			UHTMakefile->ResolveObjectAtInitOrderIndex(ObjectIndex, EUHTMakefileLoadingPhase::Preload);
		}
	}

	// Resolve own objects
	for (int32 ObjectIndex : InitializeObjectIndexes[+UHTMakefileLoadingPhase])
	{
		UHTMakefile->ResolveObjectAtInitOrderIndex(ObjectIndex, UHTMakefileLoadingPhase);
	}
}

void FUHTMakefileHeaderDescriptor::StartLoading()
{
	LoadingPhase = EUHTMakefileLoadingPhase::Load;
}

void FUHTMakefileHeaderDescriptor::Reset()
{
	PrerequisiteIndexes.Empty();
	InitializeObjectIndexes.Empty();
	UHTMakefile = nullptr;
	SourceFile = nullptr;
	LoadingPhase = EUHTMakefileLoadingPhase::Preload;
	FMemory::Memzero(bCreatedObjects, sizeof(bCreatedObjects));
	FMemory::Memzero(bResolvedObjects, sizeof(bResolvedObjects));
}

FArchive& operator<<(FArchive& Ar, FUHTMakefileHeaderDescriptor& UHTMakefileHeaderDescriptor)
{
	Ar << UHTMakefileHeaderDescriptor.PrerequisiteIndexes;
	Ar << UHTMakefileHeaderDescriptor.InitializeObjectIndexes;

	return Ar;
}
