// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UHTMakefileModuleDescriptor.h"
#include "UnrealHeaderTool.h"
#include "UHTMakefile.h"
#include "ClassMaps.h"

void FUHTMakefileModuleDescriptor::SetPackageIndex(int32 Index)
{
	PackageIndex = Index;
	UHTMakefile->AddObject(ESerializedObjectType::EPackage, Index);
}

void FUHTMakefileModuleDescriptor::LoadModuleData(const FManifestModule& ManifestModule, EUHTMakefileLoadingPhase UHTMakefileLoadingPhase)
{
	if (UHTMakefileLoadingPhase == EUHTMakefileLoadingPhase::Preload)
	{
		UHTMakefile->CreatePackage(GetPackageIndex());
	}

	for (int32 Index : HeadersOrder[+UHTMakefileLoadingPhase])
	{
		if (UHTMakefileLoadingPhase == EUHTMakefileLoadingPhase::Preload)
		{
			HeaderDescriptors[Index].SetMakefile(UHTMakefile);
		}
		HeaderDescriptors[Index].CreateObjects(UHTMakefileLoadingPhase);
	}

	if (UHTMakefileLoadingPhase == EUHTMakefileLoadingPhase::Load)
	{
		UHTMakefile->ResolvePackage(PackageIndex);
	}

	for (int32 Index : HeadersOrder[+UHTMakefileLoadingPhase])
	{
		HeaderDescriptors[Index].ResolveObjects(UHTMakefileLoadingPhase);
	}

	if (UHTMakefileLoadingPhase == EUHTMakefileLoadingPhase::Preload)
	{
		GPackageToManifestModuleMap.Add(UHTMakefile->GetPackageByIndex(GetPackageIndex()), &ManifestModule);
	}
}

void FUHTMakefileModuleDescriptor::AddToHeaderOrder(FUnrealSourceFile* SourceFile)
{
	if (SourceFile->GetPackage() != Package)
	{
		return;
	}
	int32 SourceFileIndex = UHTMakefile->GetUnrealSourceFileIndex(SourceFile);
	TArray<int32>& CurrentHeadersOrder = HeadersOrder[+LoadingPhase];
	checkSlow(!CurrentHeadersOrder.Contains(SourceFileIndex));
	CurrentHeadersOrder.Add(SourceFileIndex);
}

void FUHTMakefileModuleDescriptor::Reset()
{
	PackageIndex = INDEX_NONE;
	HeadersOrder.Reset();
	HeadersOrder.AddDefaulted(+EUHTMakefileLoadingPhase::Max);
	LoadingPhase = EUHTMakefileLoadingPhase::Preload;
	UHTMakefile = nullptr;
	Package = nullptr;


	for (auto& Kvp : HeaderDescriptors)
	{
		Kvp.Value.Reset();
	}

	HeaderDescriptors.Empty();
}

FArchive& operator<<(FArchive& Ar, FUHTMakefileModuleDescriptor& UHTMakefileModuleDescriptor)
{
	Ar << UHTMakefileModuleDescriptor.PackageIndex;
	Ar << UHTMakefileModuleDescriptor.HeaderDescriptors;
	Ar << UHTMakefileModuleDescriptor.HeadersOrder;

	return Ar;
}
