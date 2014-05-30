// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTGameUserSettings.generated.h"

/** Defines the current state of the game. */


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

protected:
	UPROPERTY(config)
	FString PlayerName;

	UPROPERTY(config)
	uint32 UTVersion;

};




