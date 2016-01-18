// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "ModuleInterface.h"
#include "ICookingStats.h"
#include "CookingStatsInterface.h"

namespace CookingStatsConstants
{
	const FName ModuleName("CookingStats");
}

/**
* Asset registry module
*/
class FCookingStatsModule
	//: public IModuleInterface
	: public ICookingStatsModule
{

public:

	/** Called right after the module DLL has been loaded and the module object has been created */
	virtual void StartupModule() override;

	/** Called before the module is unloaded, right before the module object is destroyed. */
	virtual void ShutdownModule() override;

	/** Gets the asset registry singleton */
	virtual ICookingStats& Get() const;


private:
	ICookingStats* CookingStats;
};


