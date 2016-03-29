// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "PaperTiledImporterPrivatePCH.h"

//////////////////////////////////////////////////////////////////////////
// FPaperTiledImporterModule

class FPaperTiledImporterModule : public FDefaultModuleImpl
{
public:
	virtual void StartupModule() override
	{
	}

	virtual void ShutdownModule() override
	{
	}
};

//////////////////////////////////////////////////////////////////////////

IMPLEMENT_MODULE(FPaperTiledImporterModule, PaperTiledImporter);
DEFINE_LOG_CATEGORY(LogPaperTiledImporter);
