// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AlembicLibraryPublicPCH.h"

#include "EditorFramework/AssetImportData.h"
#include "AbcAssetImportData.generated.h"

/**
* Base class for import data and options used when importing any asset from Alembic
*/
UCLASS(config = EditorPerProjectUserSettings, HideCategories = Object, abstract)
class ALEMBICLIBRARY_API UAbcAssetImportData : public UAssetImportData
{
	GENERATED_UCLASS_BODY()
};
