// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "InternationalizationSettingsModule.h"

class FInternationalizationSettingsModule : public IInternationalizationSettingsModule
{
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

IMPLEMENT_MODULE( FInternationalizationSettingsModule, InternationalizationSettings)

void FInternationalizationSettingsModule::StartupModule()
{
}

void FInternationalizationSettingsModule::ShutdownModule()
{
}
