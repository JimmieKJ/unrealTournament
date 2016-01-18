// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "LootTablesPCH.h"

DEFINE_LOG_CATEGORY(LogLootTables);

/**
 * Module
 */
class FLootTablesModule
	: public IModuleInterface
{
public:

	// IModuleInterface interface

	virtual void StartupModule( ) override
	{
	}

	virtual void ShutdownModule( ) override
	{
	}
};

IMPLEMENT_MODULE(FLootTablesModule, LootTables);

