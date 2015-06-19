// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealHeaderTool.h"

#include "ClassMaps.h"
#include "UnrealSourceFile.h"
#include "UnrealTypeDefinitionInfo.h"

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

TSharedRef<FUnrealTypeDefinitionInfo> AddTypeDefinition(FUnrealSourceFile& SourceFile, UField* Field, int32 Line)
{
	TSharedRef<FUnrealTypeDefinitionInfo> DefinitionInfo = MakeShareable(new FUnrealTypeDefinitionInfo(SourceFile, Line));

	GTypeDefinitionInfoMap.Add(Field, DefinitionInfo);

	return DefinitionInfo;
}