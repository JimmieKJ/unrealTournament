// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ModuleInterface.h"

class ILocalizationServiceProvider;
class ULocalizationTarget;

/**
 * Interface for localization dashboard module.
 */
class ILocalizationDashboardModule
	: public IModuleInterface
{
public:
	/** Virtual destructor. */
	virtual ~ILocalizationDashboardModule() { }

	/**
	 * Shows the localization dashboard UI.
	 */
	virtual void Show() = 0;

	virtual const TArray<ILocalizationServiceProvider*>& GetLocalizationServiceProviders() const = 0;
	virtual ILocalizationServiceProvider* GetCurrentLocalizationServiceProvider() const = 0;

	static ILocalizationDashboardModule& Get()
	{
		return FModuleManager::LoadModuleChecked<ILocalizationDashboardModule>("LocalizationDashboard");
	}
};
