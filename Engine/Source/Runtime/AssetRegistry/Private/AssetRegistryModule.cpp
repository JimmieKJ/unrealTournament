// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#include "AssetRegistryPCH.h"
#include "AssetRegistryModule.h"
#include "AssetRegistryConsoleCommands.h"

#include "Developer/MessageLog/Public/MessageLogModule.h"

#define LOCTEXT_NAMESPACE "AssetRegistry"

IMPLEMENT_MODULE( FAssetRegistryModule, AssetRegistry );

void FAssetRegistryModule::StartupModule()
{
	AssetRegistry = new FAssetRegistry();
	ConsoleCommands = new FAssetRegistryConsoleCommands(*this);

#if WITH_UNREAL_DEVELOPER_TOOLS
	// create a message log for the asset registry to use
	FMessageLogModule& MessageLogModule = FModuleManager::LoadModuleChecked<FMessageLogModule>("MessageLog");
	FMessageLogInitializationOptions InitOptions;
	InitOptions.bShowPages = true;
	MessageLogModule.RegisterLogListing("AssetRegistry", LOCTEXT("AssetRegistryLogLabel", "Asset Registry"), InitOptions);
#endif
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

#if WITH_UNREAL_DEVELOPER_TOOLS
	if( FModuleManager::Get().IsModuleLoaded("MessageLog") )
	{
		// unregister message log
		FMessageLogModule& MessageLogModule = FModuleManager::GetModuleChecked<FMessageLogModule>("MessageLog");
		MessageLogModule.UnregisterLogListing("AssetRegistry");
	}
#endif
}

IAssetRegistry& FAssetRegistryModule::Get() const
{
	check(AssetRegistry);
	return *AssetRegistry;
}

#undef LOCTEXT_NAMESPACE