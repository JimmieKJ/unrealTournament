// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AssetToolsPrivatePCH.h"
#include "AssetToolsModule.h"
#include "AssetToolsConsoleCommands.h"

IMPLEMENT_MODULE( FAssetToolsModule, AssetTools );
DEFINE_LOG_CATEGORY(LogAssetTools);

void FAssetToolsModule::StartupModule()
{
	AssetTools = new FAssetTools();
	ConsoleCommands = new FAssetToolsConsoleCommands(*this);
}

void FAssetToolsModule::ShutdownModule()
{
	if (AssetTools != NULL)
	{
		delete AssetTools;
		AssetTools = NULL;
	}

	if (ConsoleCommands != NULL)
	{
		delete ConsoleCommands;
		ConsoleCommands = NULL;
	}
}

IAssetTools& FAssetToolsModule::Get() const
{
	check(AssetTools);
	return *AssetTools;
}
