// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AnalyticsPrivatePCH.h"
#include "Interfaces/IAnalyticsProviderModule.h"
#include "Interfaces/IAnalyticsProvider.h"

DEFINE_LOG_CATEGORY_STATIC(LogAnalytics, Display, All);

IMPLEMENT_MODULE( FAnalytics, Analytics );

FAnalytics::FAnalytics()
{
}

FAnalytics::~FAnalytics()
{
}


TSharedPtr<IAnalyticsProvider> FAnalytics::CreateAnalyticsProvider( const FName& ProviderModuleName, const FProviderConfigurationDelegate& GetConfigValue )
{
	// Check if we can successfully load the module.
	if (ProviderModuleName != NAME_None)
	{
		IAnalyticsProviderModule* Module = FModuleManager::Get().LoadModulePtr<IAnalyticsProviderModule>(ProviderModuleName);
		if (Module != NULL)
		{
			UE_LOG(LogAnalytics, Log, TEXT("Creating configured Analytics provider %s"), *ProviderModuleName.ToString());
			return Module->CreateAnalyticsProvider(GetConfigValue);
		}
		else
		{
			UE_LOG(LogAnalytics, Warning, TEXT("Failed to find Analytics provider named %s."), *ProviderModuleName.ToString());
		}
	}
	else
	{
		UE_LOG(LogAnalytics, Warning, TEXT("CreateAnalyticsProvider called with a module name of None."));
	}
	return NULL;
}

FAnalytics::BuildType FAnalytics::GetBuildType() const
{
	// Detect release packaging
	// @todo - find some way to decide if we're really release mode
	bool bIsRelease = false;
#if UE_BUILD_SHIPPING 
	bIsRelease = true;
#endif
	if (bIsRelease) return Release;

	return FParse::Param(FCommandLine::Get(), TEXT("DEBUGANALYTICS")) 
		? Debug
		: FParse::Param(FCommandLine::Get(), TEXT("TESTANALYTICS"))
			? Test
			: FParse::Param(FCommandLine::Get(), TEXT("RELEASEANALYTICS"))
				? Release
				: Development;
}

FString FAnalytics::GetConfigValueFromIni( const FString& IniName, const FString& SectionName, const FString& KeyName, bool bIsRequired )
{
	FString Result;
	if (!GConfig->GetString(*SectionName, *KeyName, Result, IniName) && bIsRequired)
	{
		UE_LOG(LogAnalytics, Verbose, TEXT("Analytics missing Key %s from %s[%s]."), *KeyName, *IniName, *SectionName);
	}
	return Result;
}

void FAnalytics::StartupModule()
{
}

void FAnalytics::ShutdownModule()
{
}
