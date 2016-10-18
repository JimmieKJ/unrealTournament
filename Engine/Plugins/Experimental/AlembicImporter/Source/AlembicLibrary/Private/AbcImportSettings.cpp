// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "AlembicLibraryPublicPCH.h"
#include "AbcImportSettings.h"

UAbcImportSettings::UAbcImportSettings(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	ImportType = EAlembicImportType::StaticMesh;
}

UAbcImportSettings* UAbcImportSettings::Get()
{	
	// This is a singleton, use default object
	UAbcImportSettings* DefaultSettings = GetMutableDefault<UAbcImportSettings>();	
	return DefaultSettings;
}
