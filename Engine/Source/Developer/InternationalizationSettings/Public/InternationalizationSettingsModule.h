// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ModuleManager.h"

class IInternationalizationSettingsModule : public IModuleInterface
{
public:
	static inline IInternationalizationSettingsModule& Get()
	{
		return FModuleManager::LoadModuleChecked< IInternationalizationSettingsModule >( "InternationalizationSettingsModule" );
	}

	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded( "InternationalizationSettingsModule" );
	}
};

