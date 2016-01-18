// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "LocalizationPrivatePCH.h"
#include "LocalizationModule.h"
#include "LocalizationTargetTypes.h"
#include "LocalizationSettings.h"

ULocalizationTarget* FLocalizationModule::GetLocalizationTargetByName(FString TargetName, bool bIsEngineTarget)
{
	if (bIsEngineTarget)
	{
		ULocalizationTargetSet* EngineTargetSet = ULocalizationSettings::GetEngineTargetSet();
		for (ULocalizationTarget* Target : EngineTargetSet->TargetObjects)
		{
			if (Target->Settings.Name == TargetName)
			{
				return Target;
			}
		}
	}
	else
	{
		ULocalizationTargetSet* EngineTargetSet = ULocalizationSettings::GetEngineTargetSet();
		for (ULocalizationTarget* Target : EngineTargetSet->TargetObjects)
		{
			if (Target->Settings.Name == TargetName)
			{
				return Target;
			}
		}
	}

	return nullptr;
}


IMPLEMENT_MODULE(FLocalizationModule, Localization);