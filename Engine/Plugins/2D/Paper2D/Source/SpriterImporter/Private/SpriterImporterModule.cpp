// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SpriterImporterPrivatePCH.h"

#define LOCTEXT_NAMESPACE "SpriterImporter"

DEFINE_LOG_CATEGORY(LogSpriterImporter);

//////////////////////////////////////////////////////////////////////////
// FSpriterImporterModule

class FSpriterImporterModule : public FDefaultModuleImpl
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

IMPLEMENT_MODULE(FSpriterImporterModule, SpriterImporter);

//////////////////////////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
