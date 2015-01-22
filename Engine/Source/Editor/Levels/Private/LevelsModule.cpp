// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#include "LevelsPrivatePCH.h"

#include "ModuleManager.h"

IMPLEMENT_MODULE( FLevelsModule, Levels );

void FLevelsModule::StartupModule()
{
	FLevelsViewCommands::Register();

	// register the editor mode
	FEditorModeRegistry::Get().RegisterMode<FEdModeLevel>(FBuiltinEditorModes::EM_Level);
}


void FLevelsModule::ShutdownModule()
{
	FLevelsViewCommands::Unregister();

	// unregister the editor mode
	FEditorModeRegistry::Get().UnregisterMode(FBuiltinEditorModes::EM_Level);
}


TSharedRef< SWidget > FLevelsModule::CreateLevelBrowser()
{
	return SNew( SLevelBrowser );
}


ULevelBrowserSettings::ULevelBrowserSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{ }
