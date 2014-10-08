// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTGameUserSettings.h"

namespace EUTGameUserSettingsVersion
{
	enum Type
	{
		UT_GAMEUSERSETTINGS_VERSION = 1
	};
}


UUTGameUserSettings::UUTGameUserSettings(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
	SoundClassVolumes[EUTSoundClass::Master] = 1.0f;
	SoundClassVolumes[EUTSoundClass::Music] = 1.0f;
	SoundClassVolumes[EUTSoundClass::SFX] = 1.0f;
	SoundClassVolumes[EUTSoundClass::Voice] = 1.0f;
}

bool UUTGameUserSettings::IsVersionValid()
{
	return (Super::IsVersionValid() && (UTVersion == EUTGameUserSettingsVersion::UT_GAMEUSERSETTINGS_VERSION));
}

void UUTGameUserSettings::UpdateVersion()
{
	Super::UpdateVersion();
	UTVersion = EUTGameUserSettingsVersion::UT_GAMEUSERSETTINGS_VERSION;
}


void UUTGameUserSettings::SetToDefaults()
{
	Super::SetToDefaults();
	SoundClassVolumes[EUTSoundClass::Master] = 1.0f;
	SoundClassVolumes[EUTSoundClass::Music] = 1.0f;
	SoundClassVolumes[EUTSoundClass::SFX] = 1.0f;
	SoundClassVolumes[EUTSoundClass::Voice] = 1.0f; 
	FullscreenMode = EWindowMode::Fullscreen;
}

void UUTGameUserSettings::ApplySettings(bool bCheckForCommandLineOverrides)
{
	Super::ApplySettings(bCheckForCommandLineOverrides);

	for (int32 i = 0; i < ARRAY_COUNT(SoundClassVolumes); i++)
	{
		SetSoundClassVolume(EUTSoundClass::Type(i), SoundClassVolumes[i]);
	}
}

void UUTGameUserSettings::SetSoundClassVolume(EUTSoundClass::Type Category, float NewValue)
{
	if (Category < ARRAY_COUNT(SoundClassVolumes))
	{
		SoundClassVolumes[Category] = NewValue;
		UUTAudioSettings* AudioSettings = UUTAudioSettings::StaticClass()->GetDefaultObject<UUTAudioSettings>();
		if (AudioSettings)
		{
			AudioSettings->SetSoundClassVolume(Category, NewValue);
		}
	}
}
float UUTGameUserSettings::GetSoundClassVolume(EUTSoundClass::Type Category)
{
	return (Category < ARRAY_COUNT(SoundClassVolumes)) ? SoundClassVolumes[Category] : 0.0f;
}

FString UUTGameUserSettings::GetEpicID()
{
	return EpicIDLogin;
}
void UUTGameUserSettings::SetEpicID(FString NewID)
{
	EpicIDLogin = NewID;
}

FString UUTGameUserSettings::GetEpicAuth()
{
	return EpicIDAuthToken;
}

void UUTGameUserSettings::SetEpicAuth(FString NewAuth)
{
	EpicIDAuthToken = NewAuth;
}

float UUTGameUserSettings::GetServerBrowserColumnWidth(int32 ColumnIndex)
{
	return (ColumnIndex >= 0 && ColumnIndex < 9) ? ServerBrowserColumnWidths[ColumnIndex] : 0;
}
void UUTGameUserSettings::SetServerBrowserColumnWidth(int32 ColumnIndex, float NewWidth)
{
	if (ColumnIndex >=0 && ColumnIndex < 9)
	{
		ServerBrowserColumnWidths[ColumnIndex] = NewWidth;	
	}
}

float UUTGameUserSettings::GetServerBrowserRulesColumnWidth(int32 ColumnIndex)
{
	return (ColumnIndex >= 0 && ColumnIndex < 2) ? ServerBrowserRulesColumnWidths[ColumnIndex] : 0;
}

void UUTGameUserSettings::SetServerBrowserRulesColumnWidth(int32 ColumnIndex, float NewWidth)
{
	if (ColumnIndex >=0 && ColumnIndex < 2)
	{
		ServerBrowserRulesColumnWidths[ColumnIndex] = NewWidth;	
	}
}

float UUTGameUserSettings::GetServerBrowserPlayersColumnWidth(int32 ColumnIndex)
{
	return (ColumnIndex >= 0 && ColumnIndex < 2) ? ServerBrowserPlayerColumnWidths[ColumnIndex] : 0;
}
void UUTGameUserSettings::SetServerBrowserPlayersColumnWidth(int32 ColumnIndex, float NewWidth)
{
	if (ColumnIndex >=0 && ColumnIndex < 2)
	{
		ServerBrowserPlayerColumnWidths[ColumnIndex] = NewWidth;	
	}
}

float UUTGameUserSettings::GetServerBrowserSplitterPositions(int32 SplitterIndex)
{
	return (SplitterIndex >= 0 && SplitterIndex < 4) ? ServerBrowserSplitterPositions[SplitterIndex] : 0;
}
void UUTGameUserSettings::SetServerBrowserSplitterPositions(int32 SplitterIndex, float NewWidth)
{
	if (SplitterIndex >=0 && SplitterIndex < 4)
	{
		ServerBrowserSplitterPositions[SplitterIndex] = NewWidth;	
	}
}

