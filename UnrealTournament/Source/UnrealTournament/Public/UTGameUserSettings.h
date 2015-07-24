// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTAudioSettings.h"
#include "UTGameUserSettings.generated.h"

UCLASS()
class UNREALTOURNAMENT_API UUTGameUserSettings : public UGameUserSettings
{
	GENERATED_UCLASS_BODY()

public:
	virtual void SetToDefaults();
	virtual void ApplySettings(bool bCheckForCommandLineOverrides) override;
	virtual void UpdateVersion();
	virtual bool IsVersionValid();

	virtual void SetSoundClassVolume(EUTSoundClass::Type Category, float NewValue);
	virtual float GetSoundClassVolume(EUTSoundClass::Type Category);

	virtual FString GetEpicID();
	virtual void SetEpicID(FString NewID);

	virtual FString GetEpicAuth();
	virtual void SetEpicAuth(FString NewAuth);

	virtual float GetServerBrowserColumnWidth(int32 ColumnIndex);
	virtual void SetServerBrowserColumnWidth(int32 ColumnIndex, float NewWidth);

	virtual float GetServerBrowserRulesColumnWidth(int32 ColumnIndex);
	virtual void SetServerBrowserRulesColumnWidth(int32 ColumnIndex, float NewWidth);

	virtual float GetServerBrowserPlayersColumnWidth(int32 ColumnIndex);
	virtual void SetServerBrowserPlayersColumnWidth(int32 ColumnIndex, float NewWidth);

	virtual float GetServerBrowserSplitterPositions(int32 SplitterIndex);
	virtual void SetServerBrowserSplitterPositions(int32 SplitterIndex, float NewWidth);

	virtual int32 GetAAMode();
	virtual void SetAAMode(int32 NewAAMode);
	static int32 ConvertAAScalabilityQualityToAAMode(int32 AAScalabilityQuality);

	virtual int32 GetScreenPercentage();
	virtual void SetScreenPercentage(int32 NewScreenPercentage);
	
	virtual bool IsHRTFEnabled();
	virtual void SetHRTFEnabled(bool NewHRTFEnabled);

#if !UE_SERVER
	DECLARE_EVENT_OneParam(UUTGameUserSettings, FSettingsAutodetected, const Scalability::FQualityLevels& /*DetectedQuality*/);
	virtual FSettingsAutodetected& OnSettingsAutodetected() { return SettingsAutodetectedEvent; }

	void BenchmarkDetailSettingsIfNeeded(class UUTLocalPlayer* LocalPlayer);
	void BenchmarkDetailSettings(class UUTLocalPlayer* LocalPlayer, bool bSaveSettingsOnceDetected);
#endif // !UE_SERVER

protected:
	UPROPERTY(config)
	uint32 UTVersion;

	UPROPERTY(config)
	float SoundClassVolumes[EUTSoundClass::MAX];

	UPROPERTY(config)
	FString EpicIDLogin;

	UPROPERTY(config)
	FString EpicIDAuthToken;

	UPROPERTY(config)
	float ServerBrowserColumnWidths[10];

	UPROPERTY(config)
	float ServerBrowserRulesColumnWidths[2];

	UPROPERTY(config)
	float ServerBrowserPlayerColumnWidths[2];

	UPROPERTY(config)
	float ServerBrowserSplitterPositions[4];

	UPROPERTY(config)
	int32 AAMode;

	UPROPERTY(config)
	int32 ScreenPercentage;

	/**
	 * Current state of the initial benchmark
	 *  -1: Not run yet
	 *   0: Started, but not completed
	 *   1: Completed
	 */
	UPROPERTY(config)
	int32 InitialBenchmarkState;

	UPROPERTY(config)
	bool bHRTFEnabled;

public:
	UPROPERTY(config)
	bool bShouldSuppressLanWarning;

	bool bBenchmarkInProgress;

private:
#if !UE_SERVER
	void RunSynthBenchmark(bool bSaveSettingsOnceDetected);

	FSettingsAutodetected SettingsAutodetectedEvent;
	TSharedPtr<class SUWDialog> AutoDetectingSettingsDialog;

	TWeakObjectPtr<UUTLocalPlayer> BenchmarkingLocalPlayer;
#endif // !UE_SERVER
};
