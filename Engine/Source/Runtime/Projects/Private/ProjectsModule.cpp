// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ProjectsModule.cpp: Implements the FProjectsModule class.
=============================================================================*/

#include "ProjectsPrivatePCH.h"


/**
 * Implements the Projects module.
 */
class FProjectsModule
	: public IModuleInterface
{
public:

	virtual void StartupModule( ) override { }

	virtual void ShutdownModule( ) override { }

	virtual bool SupportsDynamicReloading( ) override
	{
		return true;
	}
};


IMPLEMENT_MODULE(FProjectsModule, Projects);
