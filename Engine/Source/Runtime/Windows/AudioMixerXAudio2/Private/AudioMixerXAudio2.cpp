// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/**
	Concrete implementation of FAudioDevice for XAudio2

	See https://msdn.microsoft.com/en-us/library/windows/desktop/hh405049%28v=vs.85%29.aspx
*/

#include "AudioMixer.h"
#include "AudioMixerDevice.h"
#include "AudioMixerPlatformXAudio2.h"


class FAudioMixerModuleXAudio2 : public IAudioDeviceModule
{
public:
	virtual FAudioDevice* CreateAudioDevice() override
	{
		return new Audio::FMixerDevice(new Audio::FMixerPlatformXAudio2());
	}
};

IMPLEMENT_MODULE(FAudioMixerModuleXAudio2, AudioMixerXAudio2);


