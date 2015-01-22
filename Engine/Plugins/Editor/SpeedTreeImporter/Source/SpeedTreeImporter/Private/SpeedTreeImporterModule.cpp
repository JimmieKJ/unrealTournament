// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SpeedTreeImporterPrivatePCH.h"
#include "ModuleManager.h"

/**
 * SpeedTreeImporter module implementation (private)
 */
class FSpeedTreeImporterModule : public ISpeedTreeImporter
{
public:
	virtual void StartupModule() override
	{
	}


	virtual void ShutdownModule() override
	{
	}

};

IMPLEMENT_MODULE(FSpeedTreeImporterModule, SpeedTreeImporter);
