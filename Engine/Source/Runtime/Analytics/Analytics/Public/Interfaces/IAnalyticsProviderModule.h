// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AnalyticsProviderConfigurationDelegate.h"
#include "ModuleManager.h"

class IAnalyticsProvider;

/** Generic interface for an analytics provider. Other modules can define more and register them with this module. */
class IAnalyticsProviderModule : public IModuleInterface
{
public:
	virtual TSharedPtr<IAnalyticsProvider> CreateAnalyticsProvider(const FAnalyticsProviderConfigurationDelegate& GetConfigValue) const = 0;
};
