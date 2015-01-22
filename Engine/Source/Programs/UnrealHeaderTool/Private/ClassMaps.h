// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UniqueObj.h"

/** Information about class source and generated headers */
struct FClassHeaderInfo
{
	FString SourceFilename;
	FString GeneratedFilename;
	bool bHasChanged;
	
	FClassHeaderInfo()
		: bHasChanged(false)
	{
	}

	explicit FClassHeaderInfo(const FString& InSourceFilename)
		: SourceFilename(InSourceFilename)
		, bHasChanged(false)
	{
	}
};

struct FManifestModule;

extern TMap<UClass*, FString>                   GClassStrippedHeaderTextMap;
extern TMap<UClass*, FString>                   GClassSourceFileMap;
extern TMap<UClass*, int32>                     GClassDeclarationLineNumber;
extern TMap<UClass*, FClassHeaderInfo>          GClassGeneratedFileMap;
extern TMap<UClass*, TUniqueObj<TArray<FName>>> GClassDependentOnMap;
extern TMap<UClass*, FString>                   GClassHeaderNameWithNoPathMap;
extern TMap<UClass*, FString>                   GClassModuleRelativePathMap;
extern TMap<UClass*, FString>                   GClassIncludePathMap;
extern TSet<UClass*>                            GExportedClasses;
extern TSet<UClass*>                            GPublicClassSet;
extern TMap<UProperty*, FString>                GArrayDimensions;
extern TMap<UPackage*,  const FManifestModule*> GPackageToManifestModuleMap;
extern TMap<UField*, uint32>                    GGeneratedCodeCRCs;
extern TMap<UEnum*,  EPropertyType>             GEnumUnderlyingTypes;
extern TSet<UClass*>                            GTemporaryClasses;


/** Types access specifiers. */
enum EAccessSpecifier
{
	ACCESS_NotAnAccessSpecifier = 0,
	ACCESS_Public,
	ACCESS_Private,
	ACCESS_Protected,
	ACCESS_Num,
};
