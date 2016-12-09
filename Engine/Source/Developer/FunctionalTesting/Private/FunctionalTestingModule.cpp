// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "FunctionalTestingModule.h"
#include "FuncTestManager.h"

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
