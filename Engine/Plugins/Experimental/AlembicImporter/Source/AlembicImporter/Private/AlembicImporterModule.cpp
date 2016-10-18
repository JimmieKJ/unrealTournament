// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "AlembicImporterPrivatePCH.h"
#include "AlembicLibraryModule.h"
#include "AlembicImporterModule.h"

class FAlembicImporterModule : public IAlembicImporterModuleInterface
{
	virtual void StartupModule() override
	{
		FModuleManager::LoadModuleChecked< IAlembicLibraryModule >("AlembicLibrary");
	}
};

IMPLEMENT_MODULE(FAlembicImporterModule, AlembicImporter);