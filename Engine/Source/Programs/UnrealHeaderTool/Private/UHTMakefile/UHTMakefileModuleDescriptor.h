// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "UHTMakefile/UHTMakefileHeaderDescriptor.h"

class FUHTMakefile;
struct FManifestModule;

/* See UHTMakefile.h for overview how makefiles work. */
class FUHTMakefileModuleDescriptor
{
public:
	FUHTMakefileModuleDescriptor(FUHTMakefile* InUHTMakefile = nullptr)
		: LoadingPhase(EUHTMakefileLoadingPhase::Preload)
		, UHTMakefile(InUHTMakefile)
		, Package(nullptr)
		
	{
		HeadersOrder.AddDefaulted(+EUHTMakefileLoadingPhase::Max);
	}

	void StopPreloading()
	{
		LoadingPhase = EUHTMakefileLoadingPhase::Max;

		for (auto& Kvp : HeaderDescriptors)
		{
			Kvp.Value.StopPreloading();
		}
	}

	void StartLoading()
	{
		LoadingPhase = EUHTMakefileLoadingPhase::Load;

		for (auto& Kvp : HeaderDescriptors)
		{
			Kvp.Value.StartLoading();
		}
	}

	void StopLoading()
	{
		LoadingPhase = EUHTMakefileLoadingPhase::Max;

		for (auto& Kvp : HeaderDescriptors)
		{
			Kvp.Value.StopLoading();
		}
	}

	void StartExporting()
	{
		LoadingPhase = EUHTMakefileLoadingPhase::Export;

		for (auto& Kvp : HeaderDescriptors)
		{
			Kvp.Value.StartExporting();
		}
	}

	void StopExporting()
	{
		LoadingPhase = EUHTMakefileLoadingPhase::Max;

		for (auto& Kvp : HeaderDescriptors)
		{
			Kvp.Value.StopExporting();
		}
	}

	void SetPackageIndex(int32 Index);

	int32 GetPackageIndex() const
	{
		return PackageIndex;
	}

	bool HasHeaderDescriptor(int32 Index)
	{
		return HeaderDescriptors.Contains(Index);
	}

	FUHTMakefileHeaderDescriptor& GetHeaderDescriptor(int32 Index)
	{
		FUHTMakefileHeaderDescriptor& HeaderDescriptor = HeaderDescriptors.FindOrAdd(Index);
		HeaderDescriptor.SetMakefile(UHTMakefile);
		return HeaderDescriptor;
	}
	void LoadModuleData(const FManifestModule& ManifestModule, EUHTMakefileLoadingPhase UHTMakefileLoadingPhase);
	void SetMakefile(FUHTMakefile* InUHTMakefile)
	{
		UHTMakefile = InUHTMakefile;
	}

	UPackage* GetPackage() const
	{
		return Package;
	}

	void SetPackage(UPackage* InPackage)
	{
		Package = InPackage;
	}

	void AddToHeaderOrder(FUnrealSourceFile* SourceFile);
	void Reset();
private:
	friend FArchive& operator<<(FArchive& Ar, FUHTMakefileModuleDescriptor& UHTMakefileModuleDescriptor);
	/** Index of this module's package in UHTMakefile Packages array. */
	int32 PackageIndex;
	TMap<int32, FUHTMakefileHeaderDescriptor> HeaderDescriptors;
	TArray<TArray<int32>> HeadersOrder;

	EUHTMakefileLoadingPhase LoadingPhase;
	FUHTMakefile* UHTMakefile;
	UPackage* Package;
};
