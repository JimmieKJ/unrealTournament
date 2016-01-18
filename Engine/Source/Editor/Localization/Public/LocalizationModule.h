// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "ModuleInterface.h"

class ULocalizationTarget;

class LOCALIZATION_API FLocalizationModule : public IModuleInterface
{
public:
	ULocalizationTarget* GetLocalizationTargetByName(FString TargetName, bool bIsEngineTarget);

	static FLocalizationModule& Get()
	{
		return FModuleManager::LoadModuleChecked<FLocalizationModule>("Localization");
	}
};