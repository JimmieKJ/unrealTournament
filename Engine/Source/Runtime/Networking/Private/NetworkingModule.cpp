// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "NetworkingPrivatePCH.h"
#include "ModuleManager.h"


IMPLEMENT_MODULE(FNetworkingModule, Networking);


/* IModuleInterface interface
 *****************************************************************************/

void FNetworkingModule::StartupModule( )
{
	FIPv4Endpoint::Initialize();
}


void FNetworkingModule::ShutdownModule( )
{
}


bool FNetworkingModule::SupportsDynamicReloading( )
{
	return false;
}
