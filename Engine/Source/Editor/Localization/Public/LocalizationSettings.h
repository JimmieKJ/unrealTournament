// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Object.h"
#include "LocalizationTargetTypes.h"
#include "LocalizationSettings.generated.h"

// Class for loading/saving configuration settings and the details view objects needed for localization dashboard functionality.
UCLASS(Config=Editor, defaultconfig)
class LOCALIZATION_API ULocalizationSettings : public UObject
{
	GENERATED_BODY()

public:
	ULocalizationSettings(const FObjectInitializer& ObjectInitializer);

private:
	UPROPERTY()
	ULocalizationTargetSet* EngineTargetSet;

	UPROPERTY(config)
	TArray<FLocalizationTargetSettings> EngineTargetsSettings;

	UPROPERTY()
	ULocalizationTargetSet* GameTargetSet;

	UPROPERTY(config)
	TArray<FLocalizationTargetSettings> GameTargetsSettings;

public:
#if WITH_EDITOR
	virtual void PostInitProperties() override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	static ULocalizationTargetSet* GetEngineTargetSet();
	static ULocalizationTargetSet* GetGameTargetSet();
};