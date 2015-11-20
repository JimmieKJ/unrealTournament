// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "LocalizationPrivatePCH.h"
#include "LocalizationSettings.h"
#include "LocalizationTargetTypes.h"
#include "ISourceControlModule.h"

ULocalizationSettings::ULocalizationSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, EngineTargetSet(ObjectInitializer.CreateDefaultSubobject<ULocalizationTargetSet>(this, TEXT("EngineLocalizationTargetSet")))
	, GameTargetSet(ObjectInitializer.CreateDefaultSubobject<ULocalizationTargetSet>(this, TEXT("ProjectLocalizationTargetSet")))
{	
}

#if WITH_EDITOR
void ULocalizationSettings::PostInitProperties()
{
	Super::PostInitProperties();

	// Create and initialize objects for details model from backing config properties.
	if (EngineTargetSet)
	{
		EngineTargetSet->TargetObjects.Empty(EngineTargetsSettings.Num());
		for (const auto& TargetSettings : EngineTargetsSettings)
		{
			ULocalizationTarget* const TargetObject = NewObject<ULocalizationTarget>(EngineTargetSet);
			TargetObject->Settings = TargetSettings;
			TargetObject->UpdateStatusFromConflictReport();
			TargetObject->UpdateWordCountsFromCSV();
			EngineTargetSet->TargetObjects.Add(TargetObject);
		}
	}

	// Create and initialize objects for details model from backing config properties.
	if (GameTargetSet)
	{
		GameTargetSet->TargetObjects.Empty(EngineTargetsSettings.Num());
		for (const auto& TargetSettings : GameTargetsSettings)
		{
			ULocalizationTarget* const TargetObject = NewObject<ULocalizationTarget>(GameTargetSet);
			TargetObject->Settings = TargetSettings;
			TargetObject->UpdateStatusFromConflictReport();
			TargetObject->UpdateWordCountsFromCSV();
			GameTargetSet->TargetObjects.Add(TargetObject);
		}
	}
}

void ULocalizationSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// Copy settings back.
	if (EngineTargetSet)
	{
		EngineTargetsSettings.Empty(EngineTargetSet->TargetObjects.Num());
		for (const auto& TargetObject : EngineTargetSet->TargetObjects)
		{
			EngineTargetsSettings.Add(TargetObject ? TargetObject->Settings : FLocalizationTargetSettings());
		}
	}

	// Copy settings back.
	if (GameTargetSet)
	{
		GameTargetsSettings.Empty(GameTargetSet->TargetObjects.Num());
		for (const auto& TargetObject : GameTargetSet->TargetObjects)
		{
			GameTargetsSettings.Add(TargetObject ? TargetObject->Settings : FLocalizationTargetSettings());
		}
	}

	UpdateDefaultConfigFile();
}
#endif

ULocalizationTargetSet* ULocalizationSettings::GetEngineTargetSet()
{
	ULocalizationSettings* LocalizationSettings = GetMutableDefault<ULocalizationSettings>();
	check(LocalizationSettings);
	return LocalizationSettings->EngineTargetSet;
}

ULocalizationTargetSet* ULocalizationSettings::GetGameTargetSet()
{
	ULocalizationSettings* LocalizationSettings = GetMutableDefault<ULocalizationSettings>();
	check(LocalizationSettings);
	return LocalizationSettings->GameTargetSet;
}