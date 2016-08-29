// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "Runtime/Analytics/AnalyticsET/Private/AnalyticsETPrivatePCH.h"

#include "IAnalyticsProviderET.h"
#include "Runtime/Analytics/AnalyticsET/Public/AnalyticsET.h"
#include "ModuleManager.h"
#include "Http.h"

IMPLEMENT_MODULE( FAnalyticsET, AnalyticsET );
DEFINE_LOG_CATEGORY(LogAnalytics);

void FAnalyticsET::StartupModule()
{
	// Make sure http is loaded so that we can flush events during module shutdown
	FModuleManager::LoadModuleChecked<FHttpModule>("HTTP");
}

void FAnalyticsET::ShutdownModule()
{
}

TSharedPtr<IAnalyticsProvider> FAnalyticsET::CreateAnalyticsProvider(const FAnalyticsProviderConfigurationDelegate& GetConfigValue) const
{
	if (GetConfigValue.IsBound())
	{
		Config ConfigValues;
		ConfigValues.APIKeyET = GetConfigValue.Execute(Config::GetKeyNameForAPIKey(), true);
		ConfigValues.APIServerET = GetConfigValue.Execute(Config::GetKeyNameForAPIServer(), true);
		ConfigValues.AppVersionET = GetConfigValue.Execute(Config::GetKeyNameForAppVersion(), false);
		ConfigValues.UseLegacyProtocol = FCString::ToBool(*GetConfigValue.Execute(Config::GetKeyNameForUseLegacyProtocol(), false));
		if (!ConfigValues.UseLegacyProtocol)
		{
			ConfigValues.AppEnvironment = GetConfigValue.Execute(Config::GetKeyNameForAppEnvironment(), true);
			ConfigValues.UploadType = GetConfigValue.Execute(Config::GetKeyNameForUploadType(), true);
		}
		return CreateAnalyticsProvider(ConfigValues);
	}
	else
	{
		UE_LOG(LogAnalytics, Warning, TEXT("CreateAnalyticsProvider called with an unbound delegate"));
	}
	return NULL;
}

