// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "DetailCustomizationsPrivatePCH.h"
#include "AudioSettingsDetails.h"
#include "Runtime/Engine/Classes/Sound/AudioSettings.h"

TSharedRef<IDetailCustomization> FAudioSettingsDetails::MakeInstance()
{
	return MakeShareable(new FAudioSettingsDetails);
}

void FAudioSettingsDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	if (!GetDefault<UEditorExperimentalSettings>()->bShowAudioStreamingOptions)
	{
		DetailBuilder.HideProperty(GET_MEMBER_NAME_CHECKED(UAudioSettings, MaximumConcurrentStreams));
	}
}
