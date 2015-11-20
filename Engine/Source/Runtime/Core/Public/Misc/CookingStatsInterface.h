// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ModuleManager.h"
#include "Core.h"

/**
* CookingStats module interface
*/
class ICookingStatsModule : public IModuleInterface
{
public:
	/**
	 * Tries to gets a pointer to the active CookingStatsModule implementation. 
	 */
	static inline ICookingStatsModule * GetPtr()
	{
		static FName NAME_CookingStats("CookingStats");
		auto Module = FModuleManager::GetModulePtr<ICookingStatsModule>(NAME_CookingStats);
		if (Module == nullptr)
		{
			Module = &FModuleManager::LoadModuleChecked<ICookingStatsModule>(NAME_CookingStats);
		}
		return reinterpret_cast<ICookingStatsModule*>(Module);
	}

};

