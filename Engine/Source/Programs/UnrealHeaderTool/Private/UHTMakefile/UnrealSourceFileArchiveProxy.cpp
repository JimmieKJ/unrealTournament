// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealHeaderTool.h"
#include "UHTMakefile/UHTMakefile.h"
#include "UHTMakefile/UnrealSourceFileArchiveProxy.h"
#include "UnrealSourceFile.h"

FUnrealSourceFileArchiveProxy::FUnrealSourceFileArchiveProxy(FUHTMakefile& UHTMakefile, FUnrealSourceFile* UnrealSourceFile)
{
	ScopeIndex = UHTMakefile.GetFileScopeIndex(&UnrealSourceFile->GetScope().Get());
	Filename = UnrealSourceFile->GetFilename();
	
	UPackage* Package = UnrealSourceFile->GetPackage();
	PackageIndex = UHTMakefile.GetPackageIndex(Package);

	GeneratedFileName = UnrealSourceFile->GetGeneratedFilename();
	ModuleRelativePath = UnrealSourceFile->GetModuleRelativePath();
	IncludePath = UnrealSourceFile->GetIncludePath();

	Includes.Empty(UnrealSourceFile->GetIncludes().Num());

	for (FHeaderProvider& HeaderProvider : UnrealSourceFile->GetIncludes())
	{
		if (HeaderProvider.GetId().EndsWith(TEXT(".h")))
		{
			FHeaderProviderArchiveProxy HeaderProviderArchiveProxy;
			HeaderProviderArchiveProxy.Type = static_cast<uint8>(HeaderProvider.GetType());
			HeaderProviderArchiveProxy.Id = HeaderProvider.GetId();
			HeaderProviderArchiveProxy.CacheIndex = UHTMakefile.GetUnrealSourceFileIndex(HeaderProvider.Resolve());
			HeaderProviderArchiveProxy.bAutoInclude = HeaderProvider.IsAutoInclude();

			Includes.Add(HeaderProviderArchiveProxy);
		}
	}

	DefinedClassesWithParsingInfo.Empty(UnrealSourceFile->GetDefinedClasses().Num());

	for (auto& Kvp : UnrealSourceFile->GetDefinedClassesWithParsingInfo())
	{
		DefinedClassesWithParsingInfo.Add(TPairInitializer<FSerializeIndex, FSimplifiedParsingClassInfo>(UHTMakefile.GetClassIndex(Kvp.Key), Kvp.Value));
	}

	GeneratedCodeVersionsArchiveProxy.Empty(UnrealSourceFile->GetGeneratedCodeVersions().Num());

	for (auto& Kvp : UnrealSourceFile->GetGeneratedCodeVersions())
	{
		FSerializeIndex StructIndex = UHTMakefile.GetStructIndex(Kvp.Key);
		uint8 GeneratedCodeVersion = static_cast<uint8>(Kvp.Value);

		GeneratedCodeVersionsArchiveProxy.Add(TPairInitializer<FSerializeIndex, uint8>(StructIndex, GeneratedCodeVersion));
	}
}

FArchive& operator<<(FArchive& Ar, FUnrealSourceFileArchiveProxy& UnrealSourceFileArchiveProxy)
{
	Ar << UnrealSourceFileArchiveProxy.ScopeIndex;
	Ar << UnrealSourceFileArchiveProxy.Filename;
	Ar << UnrealSourceFileArchiveProxy.PackageIndex;
	Ar << UnrealSourceFileArchiveProxy.GeneratedFileName;
	Ar << UnrealSourceFileArchiveProxy.ModuleRelativePath;
	Ar << UnrealSourceFileArchiveProxy.IncludePath;
	Ar << UnrealSourceFileArchiveProxy.Includes;
	Ar << UnrealSourceFileArchiveProxy.DefinedClassesWithParsingInfo;
	Ar << UnrealSourceFileArchiveProxy.GeneratedCodeVersionsArchiveProxy;

	return Ar;
}

FUnrealSourceFile* FUnrealSourceFileArchiveProxy::CreateUnrealSourceFile(FUHTMakefile& UHTMakefile)
{
	UPackage* Package = UHTMakefile.GetPackageByIndex(PackageIndex);
	FUnrealSourceFile* UnrealSourceFile = new FUnrealSourceFile(Package, CopyTemp(Filename), FString());
	UnrealSourceFile->SetGeneratedFilename(GeneratedFileName);
	UnrealSourceFile->SetModuleRelativePath(ModuleRelativePath);
	UnrealSourceFile->SetIncludePath(IncludePath);
	GeneratedCodeVersionsArchiveProxy.Empty(UnrealSourceFile->GetGeneratedCodeVersions().Num());

	UHTMakefile.AddFileScope(&UnrealSourceFile->GetScope().Get());

	return UnrealSourceFile;
}

void FUnrealSourceFileArchiveProxy::Resolve(FUnrealSourceFile* UnrealSourceFile, const FUHTMakefile& UHTMakefile)
{
	UnrealSourceFile->SetScope(UHTMakefile.GetFileScopeByIndex(ScopeIndex));
	UnrealSourceFile->GetIncludes().Empty(Includes.Num());
	for (const FHeaderProviderArchiveProxy& Include : Includes)
	{
		FHeaderProvider HeaderProvider = FHeaderProvider(static_cast<EHeaderProviderSourceType>(Include.Type), Include.Id, Include.bAutoInclude);
		HeaderProvider.SetCache(UHTMakefile.GetUnrealSourceFileByIndex(Include.CacheIndex));
		UnrealSourceFile->GetIncludes().Add(HeaderProvider);
	}

	for (const auto& Kvp : DefinedClassesWithParsingInfo)
	{
		UClass* Class = UHTMakefile.GetClassByIndex(Kvp.Key);
		FSimplifiedParsingClassInfo SimplifiedClassParsingInfo = Kvp.Value;
		UnrealSourceFile->AddDefinedClass(Class, SimplifiedClassParsingInfo);
	}

	UnrealSourceFile->GetGeneratedCodeVersions().Empty(GeneratedCodeVersionsArchiveProxy.Num());
	for (const auto& Kvp : GeneratedCodeVersionsArchiveProxy)
	{
		EGeneratedCodeVersion GeneratedCodeVersion = static_cast<EGeneratedCodeVersion>(Kvp.Value);
		UStruct* Struct = UHTMakefile.GetStructByIndex(Kvp.Key);
		UnrealSourceFile->GetGeneratedCodeVersions().Add(Struct, GeneratedCodeVersion);
	}

	UHTMakefile.ResolveFileScope(ScopeIndex);
}