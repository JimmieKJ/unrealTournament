// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "Modules/ModuleManager.h"

class FMobilePatchingUtilsModule : public FDefaultModuleImpl
{
public:
	virtual void StartupModule() override
	{
	}

	virtual void ShutdownModule() override
	{
	}
};

IMPLEMENT_MODULE( FMobilePatchingUtilsModule, MobilePatchingUtils )
