// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "AbcImportSettings.h"
#include "UObject/Class.h"

UAbcImportSettings::UAbcImportSettings(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	ImportType = EAlembicImportType::StaticMesh;
	bReimport = false;
}

UAbcImportSettings* UAbcImportSettings::Get()
{	
	// This is a singleton, use default object
	UAbcImportSettings* DefaultSettings = GetMutableDefault<UAbcImportSettings>();	
	return DefaultSettings;
}
