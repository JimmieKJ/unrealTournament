// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "SequenceRecorderPrivatePCH.h"
#include "ActorRecordingSettings.h"
#include "Runtime/Core/Public/Features/IModularFeatures.h"

FActorRecordingSettings::FActorRecordingSettings()
{
	TArray<IMovieSceneSectionRecorderFactory*> ModularFeatures = IModularFeatures::Get().GetModularFeatureImplementations<IMovieSceneSectionRecorderFactory>("MovieSceneSectionRecorderFactory");
	for (IMovieSceneSectionRecorderFactory* Factory : ModularFeatures)
	{
		UObject* SettingsObject = Factory->CreateSettingsObject();
		if (SettingsObject)
		{
			Settings.Add(SettingsObject);
		}
	}
}