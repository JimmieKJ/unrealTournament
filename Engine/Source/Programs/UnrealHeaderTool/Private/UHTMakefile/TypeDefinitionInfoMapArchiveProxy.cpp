// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealHeaderTool.h"
#include "UHTMakefile/UHTMakefile.h"
#include "UHTMakefile/TypeDefinitionInfoMapArchiveProxy.h"
#include "ClassMaps.h"


FTypeDefinitionInfoMapArchiveProxy::FTypeDefinitionInfoMapArchiveProxy(FUHTMakefile& UHTMakefile, TArray<TPair<UField*, FUnrealTypeDefinitionInfo*>>& TypeDefinitionInfoPairs)
{
	for (auto& Kvp : TypeDefinitionInfoPairs)
	{
		UField* Field = Kvp.Key;

		FSerializeIndex FieldIndex = UHTMakefile.GetFieldIndex(Field);
		int32 UnrealTypeDefinitionInfoIndex = UHTMakefile.GetUnrealTypeDefinitionInfoIndex(Kvp.Value);

		TypeDefinitionInfoIndexes.Add(TPairInitializer<FSerializeIndex, int32>(FieldIndex, UnrealTypeDefinitionInfoIndex));
	}
}

void FTypeDefinitionInfoMapArchiveProxy::ResolveIndex(FUHTMakefile& UHTMakefile, int32 Index)
{
	auto& Kvp = TypeDefinitionInfoIndexes[Index];
	UField* Field = UHTMakefile.GetFieldByIndex(Kvp.Key);
	FUnrealTypeDefinitionInfo* UnrealTypeDefinitionInfo = UHTMakefile.GetUnrealTypeDefinitionInfoByIndex(Kvp.Value);
	TPair<UField*, FUnrealTypeDefinitionInfo*>* Entry = UHTMakefile.GetTypeDefinitionInfoMapEntryByIndex(Index);
	Entry->Key = Field;
	Entry->Value = UnrealTypeDefinitionInfo;
	GTypeDefinitionInfoMap.Add(Field, MakeShareable(UnrealTypeDefinitionInfo));
}

void FTypeDefinitionInfoMapArchiveProxy::ResolveClassIndex(FUHTMakefile& UHTMakefile, int32 Index)
{
	auto& Kvp = TypeDefinitionInfoIndexes[Index];
	UClass* Class = UHTMakefile.GetClassByIndex(Kvp.Key);
	FUnrealTypeDefinitionInfo* UnrealTypeDefinitionInfo = UHTMakefile.GetUnrealTypeDefinitionInfoByIndex(Kvp.Value);
	TPair<UField*, FUnrealTypeDefinitionInfo*>* Entry = UHTMakefile.GetTypeDefinitionInfoMapEntryByIndex(Index);
	Entry->Key = Class;
	Entry->Value = UnrealTypeDefinitionInfo;
	GTypeDefinitionInfoMap.Add(Class, MakeShareable(UnrealTypeDefinitionInfo));
}
