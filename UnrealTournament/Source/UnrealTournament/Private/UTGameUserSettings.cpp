// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTGameUserSettings.h"

namespace EUTGameUserSettingsVersion
{
	enum Type
	{
		UT_GAMEUSERSETTINGS_VERSION = 1
	};
}


UUTGameUserSettings::UUTGameUserSettings(const class FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SoundClassVolumes[EUTSoundClass::Master] = 1.0f;
	SoundClassVolumes[EUTSoundClass::Music] = 1.0f;
	SoundClassVolumes[EUTSoundClass::SFX] = 1.0f;
	SoundClassVolumes[EUTSoundClass::Voice] = 1.0f;
	InitialBenchmarkState = -1;
	bBenchmarkInProgress=false;
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
	ScreenPercentage = 100;
	InitialBenchmarkState = -1;
}

void UUTGameUserSettings::ApplySettings(bool bCheckForCommandLineOverrides)
{
	Super::ApplySettings(bCheckForCommandLineOverrides);

	for (int32 i = 0; i < ARRAY_COUNT(SoundClassVolumes); i++)
	{
		SetSoundClassVolume(EUTSoundClass::Type(i), SoundClassVolumes[i]);
	}

	SetAAMode(AAMode);
	SetScreenPercentage(ScreenPercentage);
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
	return (ColumnIndex >= 0 && ColumnIndex < 10) ? ServerBrowserColumnWidths[ColumnIndex] : 0;
}
void UUTGameUserSettings::SetServerBrowserColumnWidth(int32 ColumnIndex, float NewWidth)
{
	if (ColumnIndex >=0 && ColumnIndex < 10)
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

int32 UUTGameUserSettings::GetAAMode()
{
	return AAMode;
}

void UUTGameUserSettings::SetAAMode(int32 NewAAMode)
{
	AAMode = NewAAMode;
	auto AAModeCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.PostProcessAAQuality"));
	AAModeCVar->Set(AAMode, ECVF_SetByGameSetting);
}

int32 UUTGameUserSettings::ConvertAAScalabilityQualityToAAMode(int32 AAScalabilityQuality)
{
	const int32 AAScalabilityQualityToModeLookup[] = { 0, 2, 3, 4 };
	int32 AAQuality = FMath::Clamp(AAScalabilityQuality, 0, 3);

	return AAScalabilityQualityToModeLookup[AAQuality];
}

int32 UUTGameUserSettings::GetScreenPercentage()
{
	return ScreenPercentage;
}

void UUTGameUserSettings::SetScreenPercentage(int32 NewScreenPercentage)
{
	ScreenPercentage = NewScreenPercentage;
	auto ScreenPercentageCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ScreenPercentage"));
	ScreenPercentageCVar->Set(ScreenPercentage, ECVF_SetByGameSetting);
}

#if !UE_SERVER
void UUTGameUserSettings::BenchmarkDetailSettingsIfNeeded(UUTLocalPlayer* LocalPlayer)
{
	if (ensure(LocalPlayer))
	{
		if (InitialBenchmarkState == -1)
		{
			InitialBenchmarkState = 0;
			BenchmarkDetailSettings(LocalPlayer, true);
		}
		else if (InitialBenchmarkState == 0)
		{
			// Benchmark was previously run but didn't complete. 
			// Perhaps prompt the user?
			SetBenchmarkFallbackValues();
		}
		else
		{
			//  Must be successfully completed
			ensure(InitialBenchmarkState == 1);
		}
	}
}

void UUTGameUserSettings::BenchmarkDetailSettings(UUTLocalPlayer* LocalPlayer, bool bSaveSettingsOnceDetected)
{
	if (ensure(LocalPlayer))
	{
		AutoDetectingSettingsDialog = LocalPlayer->ShowMessage(
			NSLOCTEXT("UTLocalPlayer", "BenchmarkingTitle", "Finding Settings"),
			NSLOCTEXT("UTLocalPlayer", "BenchmarkingText", "Automatically finding the settings that will give you the best experience.\n\nThis could take some time..."),
			0
			);

		// allow for the dialog to be displayed before running the synth benchmark
		FTimerHandle UnusedHandle;
		LocalPlayer->GetWorld()->GetTimerManager().SetTimer(UnusedHandle, FTimerDelegate::CreateUObject(this, &UUTGameUserSettings::RunSynthBenchmark, bSaveSettingsOnceDetected), 0.5f, false);
		bBenchmarkInProgress = true;
	}
}

void UUTGameUserSettings::RunSynthBenchmark(bool bSaveSettingsOnceDetected)
{
	Scalability::FQualityLevels DetectedLevels = Scalability::BenchmarkQualityLevels();

	if (bSaveSettingsOnceDetected)
	{
		ScalabilityQuality = DetectedLevels;
		Scalability::SetQualityLevels(ScalabilityQuality);
		Scalability::SaveState(GGameUserSettingsIni);

		// Set specific features in game settings. These values are controlled by scalability initially but can be overwritten in the settings screen.
		int32 AAMode = UUTGameUserSettings::ConvertAAScalabilityQualityToAAMode(DetectedLevels.AntiAliasingQuality);
		SetAAMode(AAMode);
		SetScreenPercentage(DetectedLevels.ResolutionQuality);

		// Mark initial benchmark state as being complete. Even if this benchmark wasn't triggered by the initial run system, it
		// is still valid to disable any future benchmarking now.
		InitialBenchmarkState = 1;

		SaveSettings();
	}

	SettingsAutodetectedEvent.Broadcast(DetectedLevels);
	bBenchmarkInProgress = false;
	if (AutoDetectingSettingsDialog.IsValid())
	{
		AutoDetectingSettingsDialog->OnDialogClosed();
		GEngine->GameViewport->RemoveViewportWidgetContent(AutoDetectingSettingsDialog.ToSharedRef());
	}
}
#endif // !UE_SERVER