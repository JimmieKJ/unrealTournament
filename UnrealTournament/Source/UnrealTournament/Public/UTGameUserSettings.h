// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
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

	virtual void SetEmoteIndex1(int32 NewIndex) { EmoteIndex1 = NewIndex; }
	virtual void SetEmoteIndex2(int32 NewIndex) { EmoteIndex2 = NewIndex; }
	virtual void SetEmoteIndex3(int32 NewIndex) { EmoteIndex3 = NewIndex; }

	virtual int32 GetEmoteIndex1() { return EmoteIndex1; }
	virtual int32 GetEmoteIndex2() { return EmoteIndex2; }
	virtual int32 GetEmoteIndex3() { return EmoteIndex3; }

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

	UPROPERTY(config)
	int32 EmoteIndex1;

	UPROPERTY(config)
	int32 EmoteIndex2;

	UPROPERTY(config)
	int32 EmoteIndex3;
};
