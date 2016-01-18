// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LevelSequenceProjectSettings.generated.h"

// Settings for the level sequences
UCLASS(config=EditorPerProjectUserSettings)
class ULevelSequenceProjectSettings : public UObject
{
	GENERATED_BODY()

public:
	ULevelSequenceProjectSettings();

	/** The default start time for new level sequences, in seconds. */
	UPROPERTY(config, EditAnywhere, Category=Timeline, meta=(Units=s))
	float DefaultStartTime;

	/** The default duration for new level sequences in seconds. */
	UPROPERTY(config, EditAnywhere, Category=Timeline, meta=(ClampMin=0.00001f, Units=s))
	float DefaultDuration;

};
