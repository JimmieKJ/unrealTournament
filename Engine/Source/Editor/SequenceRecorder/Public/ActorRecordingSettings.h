// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ActorRecordingSettings.generated.h"

USTRUCT()
struct FActorRecordingSettings
{
	GENERATED_BODY()

	FActorRecordingSettings();

	template <typename SettingsType>
	SettingsType* GetSettingsObject() const
	{
		for (UObject* SettingsObject : Settings)
		{
			if (SettingsType* TypedSettingsObject = Cast<SettingsType>(SettingsObject))
			{
				return TypedSettingsObject;
			}
		}

		return nullptr;
	}

private:
	/** External settings objects for recorders that supply them. Displayed via a details customization  */
	UPROPERTY(EditAnywhere, Category = "Actor Recording")
	TArray<UObject*> Settings;
};