// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ModuleManager.h"
#include "ICollectionManager.h"

class FCollectionManagerModule : public IModuleInterface
{
public:
	virtual void StartupModule();
	virtual void ShutdownModule();

	virtual ICollectionManager& Get() const;

private:
	ICollectionManager* CollectionManager;
	class FCollectionManagerConsoleCommands* ConsoleCommands;
};
