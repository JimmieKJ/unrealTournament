// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "FlurryEditorPrivatePCH.h"

#include "FlurryEditor.h"
#include "FlurrySettings.h"
#include "Analytics.h"

IMPLEMENT_MODULE( FFlurryEditorModule, FlurryEditorModule );

#define LOCTEXT_NAMESPACE "Flurry"

void FFlurryEditorModule::StartupModule()
{
}

void FFlurryEditorModule::ShutdownModule()
{
}

UFlurrySettings::UFlurrySettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SettingsDisplayName = LOCTEXT("SettingsDisplayName", "Flurry");
	SettingsTooltip = LOCTEXT("SettingsTooltip", "Flurry analytics configuration settings");
}

void UFlurrySettings::ReadConfigSettings()
{
	FString ReadApiKey = FAnalytics::Get().GetConfigValueFromIni(GetIniName(), GetReleaseIniSection(), TEXT("FlurryApiKey"), true);
	ReleaseApiKey = ReadApiKey;

	ReadApiKey = FAnalytics::Get().GetConfigValueFromIni(GetIniName(), GetDebugIniSection(), TEXT("FlurryApiKey"), true);
	if (ReadApiKey.Len() == 0)
	{
		ReadApiKey = ReleaseApiKey;
	}
	DebugApiKey = ReadApiKey;

	ReadApiKey = FAnalytics::Get().GetConfigValueFromIni(GetIniName(), GetTestIniSection(), TEXT("FlurryApiKey"), true);
	if (ReadApiKey.Len() == 0)
	{
		ReadApiKey = ReleaseApiKey;
	}
	TestApiKey = ReadApiKey;

	ReadApiKey = FAnalytics::Get().GetConfigValueFromIni(GetIniName(), GetDevelopmentIniSection(), TEXT("FlurryApiKey"), true);
	if (ReadApiKey.Len() == 0)
	{
		ReadApiKey = ReleaseApiKey;
	}
	DevelopmentApiKey = ReadApiKey;
}

void UFlurrySettings::WriteConfigSettings()
{
	FAnalytics::Get().WriteConfigValueToIni(GetIniName(), GetReleaseIniSection(), TEXT("FlurryApiKey"), ReleaseApiKey);
	FAnalytics::Get().WriteConfigValueToIni(GetIniName(), GetTestIniSection(), TEXT("FlurryApiKey"), TestApiKey);
	FAnalytics::Get().WriteConfigValueToIni(GetIniName(), GetDebugIniSection(), TEXT("FlurryApiKey"), DebugApiKey);
	FAnalytics::Get().WriteConfigValueToIni(GetIniName(), GetDevelopmentIniSection(), TEXT("FlurryApiKey"), DevelopmentApiKey);
}

#undef LOCTEXT_NAMESPACE
