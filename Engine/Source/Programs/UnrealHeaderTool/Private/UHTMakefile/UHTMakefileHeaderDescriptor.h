// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

class FUnrealSourceFile;

/* See UHTMakefile.h for overview how makefiles work. */
class FUHTMakefileHeaderDescriptor
{
public:
	FUHTMakefileHeaderDescriptor(FUHTMakefile* InUHTMakefile = nullptr);

	void AddPrerequesites(TArray<FUnrealSourceFile*>& Prerequesities);
	void CreateObjects(EUHTMakefileLoadingPhase UHTMakefileLoadingPhase);
	void ResolveObjects(EUHTMakefileLoadingPhase UHTMakefileLoadingPhase);
	friend FArchive& operator<<(FArchive& Ar, FUHTMakefileHeaderDescriptor& UHTMakefileHeaderDescriptor);

	void SetMakefile(FUHTMakefile* InUHTMakefile)
	{
		UHTMakefile = InUHTMakefile;
	}

	FUHTMakefile* GetMakefile()
	{
		return UHTMakefile;
	}

	void AddEntry(int32 Index)
	{
		check(LoadingPhase != EUHTMakefileLoadingPhase::Max);
		InitializeObjectIndexes[+LoadingPhase].Add(Index);
	}

	void StopPreloading()
	{
		LoadingPhase = EUHTMakefileLoadingPhase::Max;
	}

	bool IsPreloading()
	{
		return LoadingPhase == EUHTMakefileLoadingPhase::Preload;
	}

	void StartLoading();

	void StopLoading()
	{
		LoadingPhase = EUHTMakefileLoadingPhase::Max;
	}

	bool IsLoading()
	{
		return LoadingPhase == EUHTMakefileLoadingPhase::Load;
	}

	void StartExporting()
	{
		LoadingPhase = EUHTMakefileLoadingPhase::Export;
	}

	void StopExporting()
	{
		LoadingPhase = EUHTMakefileLoadingPhase::Max;
	}

	bool IsExporting()
	{
		return LoadingPhase == EUHTMakefileLoadingPhase::Export;
	}

	FUnrealSourceFile* GetSourceFile() const
	{
		return SourceFile;
	}

	void SetSourceFile(FUnrealSourceFile* InSourceFile)
	{
		SourceFile = InSourceFile;
	}

	void Reset();

private:
	TArray<int32> PrerequisiteIndexes;
	TArray<TArray<int32>> InitializeObjectIndexes;
	FUHTMakefile* UHTMakefile;

	FUnrealSourceFile* SourceFile;
	EUHTMakefileLoadingPhase LoadingPhase;
	bool bCreatedObjects[(uint8)EUHTMakefileLoadingPhase::Max];
	bool bResolvedObjects[(uint8)EUHTMakefileLoadingPhase::Max];
};
