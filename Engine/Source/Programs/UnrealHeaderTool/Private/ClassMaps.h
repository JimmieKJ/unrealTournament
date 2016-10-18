// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UniqueObj.h"
#include "ParserClass.h"
#include "Scope.h"
#include "HeaderProvider.h"

#include "UnrealSourceFile.h"
#include "ClassDeclarationMetaData.h"

struct FManifestModule;
class FUnrealSourceFile;
class FUnrealTypeDefinitionInfo;

extern TMap<FString, TSharedRef<FUnrealSourceFile> > GUnrealSourceFilesMap;
extern TMap<UField*, TSharedRef<FUnrealTypeDefinitionInfo> > GTypeDefinitionInfoMap;
extern TMap<UClass*, FString> GClassStrippedHeaderTextMap;
extern TMap<UClass*, FString> GClassHeaderNameWithNoPathMap;
extern TSet<FUnrealSourceFile*> GExportedSourceFiles;
extern TSet<UClass*> GPublicClassSet;
extern TSet<FUnrealSourceFile*> GPublicSourceFileSet;
extern TMap<UProperty*, FString> GArrayDimensions;
extern TMap<UPackage*,  const FManifestModule*> GPackageToManifestModuleMap;
extern TMap<UField*, uint32> GGeneratedCodeCRCs;
extern TMap<UEnum*,  EPropertyType> GEnumUnderlyingTypes;
extern TMap<FName, TSharedRef<FClassDeclarationMetaData> > GClassDeclarations;
extern TSet<UProperty*> GUnsizedProperties;

/** Types access specifiers. */
enum EAccessSpecifier
{
	ACCESS_NotAnAccessSpecifier = 0,
	ACCESS_Public,
	ACCESS_Private,
	ACCESS_Protected,
	ACCESS_Num,
};

inline FArchive& operator<<(FArchive& Ar, EAccessSpecifier& ObjectType)
{
	if (Ar.IsLoading())
	{
		int32 Value;
		Ar << Value;
		ObjectType = EAccessSpecifier(Value);
	}
	else if (Ar.IsSaving())
	{
		int32 Value = (int32)ObjectType;
		Ar << Value;
	}

	return Ar;
}

/**
 * Add type definition info to global map.
 *
 * @param UHTMakefile Makefile to which data is saved.
 * @param SourceFile SourceFile in which type was defined.
 * @param Field Defined type.
 * @param Line Line on which the type was defined.
 *
 * @returns Type definition info.
 */
TSharedRef<FUnrealTypeDefinitionInfo> AddTypeDefinition(FUHTMakefile& UHTMakefile, FUnrealSourceFile* SourceFile, UField* Field, int32 Line);