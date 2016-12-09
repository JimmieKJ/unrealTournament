// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "FileScopeArchiveProxy.h"
#include "UnrealHeaderTool.h"
#include "Scope.h"
#include "UHTMakefile.h"

FArchive& operator<<(FArchive& Ar, FFileScopeArchiveProxy& FileScopeArchiveProxy)
{
	Ar << static_cast<FScopeArchiveProxy&>(FileScopeArchiveProxy);
	Ar << FileScopeArchiveProxy.SourceFileIndex;
	Ar << FileScopeArchiveProxy.Name;
	Ar << FileScopeArchiveProxy.IncludedScopesIndexes;

	return Ar;
}

FFileScopeArchiveProxy::FFileScopeArchiveProxy(const FUHTMakefile& UHTMakefile, const FFileScope* FileScope)
	: FScopeArchiveProxy(UHTMakefile, FileScope)
{
	SourceFileIndex = UHTMakefile.GetUnrealSourceFileIndex(FileScope->GetSourceFile());
	Name = FNameArchiveProxy(UHTMakefile, FileScope->GetName());
	IncludedScopesIndexes.Empty(FileScope->GetIncludedScopes().Num());

	for (const FFileScope* IncludedScope : FileScope->GetIncludedScopes())
	{
		IncludedScopesIndexes.Add(UHTMakefile.GetFileScopeIndex(IncludedScope));
	}
}

void FFileScopeArchiveProxy::AddReferencedNames(const FFileScope* FileScope, FUHTMakefile& UHTMakefile)
{
	FScopeArchiveProxy::AddReferencedNames(FileScope, UHTMakefile);
	UHTMakefile.AddName(FileScope->Name);
}

FFileScope*  FFileScopeArchiveProxy::CreateFileScope(const FUHTMakefile& UHTMakefile) const
{
	return new FFileScope(Name.CreateName(UHTMakefile), nullptr);
}

void FFileScopeArchiveProxy::PostConstruct(FFileScope* FileScope) const
{

}

void FFileScopeArchiveProxy::Resolve(FFileScope* FileScope, const FUHTMakefile& UHTMakefile) const
{
	FScopeArchiveProxy::Resolve(FileScope, UHTMakefile);
	FileScope->SetSourceFile(UHTMakefile.GetUnrealSourceFileByIndex(SourceFileIndex));
	for (int32 Index : IncludedScopesIndexes)
	{
		FileScope->IncludeScope(UHTMakefile.GetFileScopeByIndex(Index));
	}
}
