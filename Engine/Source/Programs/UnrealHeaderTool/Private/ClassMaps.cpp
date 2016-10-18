// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealHeaderTool.h"

#include "ClassMaps.h"
#include "UnrealSourceFile.h"
#include "UnrealTypeDefinitionInfo.h"
#include "UHTMakefile/UHTMakefile.h"

TMap<FString, TSharedRef<FUnrealSourceFile> > GUnrealSourceFilesMap;
TMap<UField*, TSharedRef<FUnrealTypeDefinitionInfo> > GTypeDefinitionInfoMap;
TMap<UClass*, FString> GClassStrippedHeaderTextMap;
TMap<UClass*, FString> GClassHeaderNameWithNoPathMap;
TSet<UClass*> GPublicClassSet;
TSet<FUnrealSourceFile*> GPublicSourceFileSet;
TSet<FUnrealSourceFile*> GExportedSourceFiles;
TMap<UProperty*, FString> GArrayDimensions;
TMap<UPackage*,  const FManifestModule*> GPackageToManifestModuleMap;
TMap<UField*, uint32> GGeneratedCodeCRCs;
TMap<UEnum*,  EPropertyType> GEnumUnderlyingTypes;
TMap<FName, TSharedRef<FClassDeclarationMetaData> > GClassDeclarations;
TSet<UProperty*> GUnsizedProperties;

TSharedRef<FUnrealTypeDefinitionInfo> AddTypeDefinition(FUHTMakefile& UHTMakefile, FUnrealSourceFile* SourceFile, UField* Field, int32 Line)
{
	FUnrealTypeDefinitionInfo* UnrealTypeDefinitionInfo = new FUnrealTypeDefinitionInfo(*SourceFile, Line);
	UHTMakefile.AddUnrealTypeDefinitionInfo(SourceFile, UnrealTypeDefinitionInfo);

	TSharedRef<FUnrealTypeDefinitionInfo> DefinitionInfo = MakeShareable(UnrealTypeDefinitionInfo);
	UHTMakefile.AddTypeDefinitionInfoMapEntry(SourceFile, Field, UnrealTypeDefinitionInfo);
	GTypeDefinitionInfoMap.Add(Field, DefinitionInfo);

	return DefinitionInfo;
}
