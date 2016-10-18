// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.


#include "AssetRegistryPCH.h"
#include "AssetRegistryModule.h"
#include "AssetRegistryConsoleCommands.h"

IMPLEMENT_MODULE( FAssetRegistryModule, AssetRegistry );

void FAssetRegistryModule::StartupModule()
{
	AssetRegistry = new FAssetRegistry();
	ConsoleCommands = new FAssetRegistryConsoleCommands(*this);
}


void FAssetRegistryModule::ShutdownModule()
{
	if ( AssetRegistry )
	{
		delete AssetRegistry;
		AssetRegistry = NULL;
	}

	if ( ConsoleCommands )
	{
		delete ConsoleCommands;
		ConsoleCommands = NULL;
	}
}

IAssetRegistry& FAssetRegistryModule::Get() const
{
	check(AssetRegistry);
	return *AssetRegistry;
}
