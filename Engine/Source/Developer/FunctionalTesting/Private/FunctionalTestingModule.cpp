// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "FunctionalTestingPrivatePCH.h"
#include "ModuleInterface.h"



#define LOCTEXT_NAMESPACE "FunctionalTesting"

void FFunctionalTestingModule::StartupModule() 
{
	FMessageLogModule& MessageLogModule = FModuleManager::LoadModuleChecked<FMessageLogModule>("MessageLog");
	FMessageLogInitializationOptions InitOptions;
	InitOptions.bShowFilters = true;
	InitOptions.bShowPages = true;
	MessageLogModule.RegisterLogListing("FunctionalTestingLog", LOCTEXT("FunctionalTestingLog", "Functional Testing Log"), InitOptions );
	Manager = MakeShareable(new FFuncTestManager());
}

void FFunctionalTestingModule::ShutdownModule() 
{
	if( FModuleManager::Get().IsModuleLoaded( "MessageLog" ) )
	{
		FMessageLogModule& MessageLogModule = FModuleManager::GetModuleChecked<FMessageLogModule>("MessageLog");
		MessageLogModule.UnregisterLogListing("FunctionalTestingLog");
	}
	Manager = NULL;
}

IMPLEMENT_MODULE( FFunctionalTestingModule, FunctionalTesting );

#undef LOCTEXT_NAMESPACE