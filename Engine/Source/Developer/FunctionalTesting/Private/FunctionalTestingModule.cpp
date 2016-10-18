// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "FunctionalTestingPrivatePCH.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "FunctionalTesting"

void FFunctionalTestingModule::StartupModule() 
{
	Manager = MakeShareable(new FFuncTestManager());
}

void FFunctionalTestingModule::ShutdownModule() 
{
	Manager.Reset();
}

IMPLEMENT_MODULE( FFunctionalTestingModule, FunctionalTesting );

#undef LOCTEXT_NAMESPACE
