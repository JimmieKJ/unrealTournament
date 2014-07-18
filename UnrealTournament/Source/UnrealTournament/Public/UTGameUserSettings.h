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
	virtual void ApplySettings();
	virtual void UpdateVersion();
	virtual bool IsVersionValid();

	virtual FString GetPlayerName();
	virtual void SetPlayerName(FString NewPlayerName);

	virtual void SetSoundClassVolume(EUTSoundClass::Type Category, float NewValue);
	virtual float GetSoundClassVolume(EUTSoundClass::Type Category);

protected:
	UPROPERTY(config)
	FString PlayerName;

	UPROPERTY(config)
	uint32 UTVersion;

	UPROPERTY(config)
	float SoundClassVolumes[EUTSoundClass::MAX];
};
