// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "CompilerMetadataManagerArchiveProxy.h"
#include "UnrealHeaderTool.h"
#include "UHTMakefile.h"

FCompilerMetadataManagerArchiveProxy::FCompilerMetadataManagerArchiveProxy(const FUHTMakefile& UHTMakefile, TArray<TPair<UStruct*, FClassMetaData*>>& CompilerMetadataManager)
{
	for (auto& Kvp : CompilerMetadataManager)
	{
		Array.Add(TPairInitializer<FSerializeIndex, FClassMetaDataArchiveProxy>(UHTMakefile.GetStructIndex(Kvp.Key), FClassMetaDataArchiveProxy(UHTMakefile, Kvp.Value)));
	}
}

void FCompilerMetadataManagerArchiveProxy::AddReferencedNames(const FCompilerMetadataManager* CompilerMetadataManager, FUHTMakefile& UHTMakefile)
{
	for (auto& Kvp : *CompilerMetadataManager)
	{
		FClassMetaDataArchiveProxy::AddReferencedNames(Kvp.Value.Get(), UHTMakefile);
	}
}

void FCompilerMetadataManagerArchiveProxy::Resolve(int32 Index, FCompilerMetadataManager& CompilerMetadataManager, FUHTMakefile& UHTMakefile)
{
	auto& Kvp = Array[Index];
	UStruct* Struct = UHTMakefile.GetStructByIndex(Kvp.Key);
	FClassMetaData* ClassMetaData = GScriptHelper.AddClassData(Struct, UHTMakefile, nullptr);
	Kvp.Value.PostConstruct(ClassMetaData);
	Kvp.Value.Resolve(ClassMetaData, UHTMakefile);
	UHTMakefile.CreateGScriptHelperEntry(Struct, ClassMetaData);
}

FArchive& operator<<(FArchive& Ar, FCompilerMetadataManagerArchiveProxy& CompilerMetadataManagerArchiveProxy)
{
	Ar << CompilerMetadataManagerArchiveProxy.Array;
	return Ar;
}
