// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ModuleManager.h"

class INUTUnrealEngine4 : public IModuleInterface
{
public:
	static inline INUTUnrealEngine4& Get()
	{
		return FModuleManager::LoadModuleChecked<INUTUnrealEngine4>("NUTUnrealEngine4");
	}

	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded("NUTUnrealEngine4");
	}
};

