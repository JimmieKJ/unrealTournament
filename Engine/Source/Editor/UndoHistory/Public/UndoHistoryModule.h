// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IUndoHistoryModule.h"

static const FName UndoHistoryTabName("UndoHistory");

/**
* Implements the UndoHistory module.
*/
class FUndoHistoryModule : public IUndoHistoryModule
{
public:

	// Begin IModuleInterface interface

	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	virtual bool SupportsDynamicReloading() override;

	// End IModuleInterface interface

	static void ExecuteOpenUndoHistory()
	{
		FGlobalTabmanager::Get()->InvokeTab(UndoHistoryTabName);
	}

private:
	// Handles creating the project settings tab.
	TSharedRef<SDockTab> HandleSpawnSettingsTab(const FSpawnTabArgs& SpawnTabArgs);
};