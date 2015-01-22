// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "InternationalizationSettingsModulePrivatePCH.h"

class FInternationalizationSettingsModule : public IInternationalizationSettingsModule
{
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

IMPLEMENT_MODULE( FInternationalizationSettingsModule, InternationalizationSettingsModule )

void FInternationalizationSettingsModule::StartupModule()
{
}

void FInternationalizationSettingsModule::ShutdownModule()
{
}