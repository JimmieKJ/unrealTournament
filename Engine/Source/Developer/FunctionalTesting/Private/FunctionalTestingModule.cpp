// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "FunctionalTestingPrivatePCH.h"
#include "ModuleInterface.h"

void FFunctionalTestingModule::StartupModule() 
{
	Manager = MakeShareable(new FFuncTestManager());
}

void FFunctionalTestingModule::ShutdownModule() 
{
	Manager = NULL;
}

IMPLEMENT_MODULE( FFunctionalTestingModule, FunctionalTesting );
