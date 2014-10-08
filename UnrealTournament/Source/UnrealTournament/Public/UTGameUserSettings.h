// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTAudioSettings.h"
#include "UTGameUserSettings.generated.h"

UCLASS()
class UUTGameUserSettings : public UGameUserSettings
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
	float ServerBrowserColumnWidths[9];

	UPROPERTY(config)
	float ServerBrowserRulesColumnWidths[2];

	UPROPERTY(config)
	float ServerBrowserPlayerColumnWidths[2];

	UPROPERTY(config)
	float ServerBrowserSplitterPositions[4];
};
