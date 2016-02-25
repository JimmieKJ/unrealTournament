// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "IBuildPatchServicesModule.h"

class BUILDPATCHSERVICES_API FBuildPatchServices
{
public:

	static bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded(ModuleName);
	}

	static IBuildPatchServicesModule& Get()
	{
		return FModuleManager::Get().GetModuleChecked<IBuildPatchServicesModule>(ModuleName);
	}

	static void Set(const FName& Value)
	{
		if(IsAvailable())
		{
			Shutdown();
		}
		ModuleName = Value;
		FModuleManager::Get().LoadModuleChecked<IBuildPatchServicesModule>(ModuleName);
	}

	static void Shutdown()
	{
		if(IsAvailable())
		{
			FModuleManager::Get().UnloadModule(ModuleName);
		}
	}

private:

	static FName ModuleName;
};