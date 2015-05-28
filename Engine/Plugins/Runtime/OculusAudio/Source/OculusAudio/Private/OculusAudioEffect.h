// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "XAudio2Device.h"
#include "AudioEffect.h"
#include "Runtime/Windows/XAudio2/Private/XAudio2Support.h"
#include "Engine.h"
#include "AudioDevice.h"

#include "AllowWindowsPlatformTypes.h"
#include <xapobase.h>
#include <xapofx.h>
#include <xaudio2fx.h>
#include "HideWindowsPlatformTypes.h"

#define AUDIO_HRTF_EFFECT_CLASS_ID __declspec( uuid( "{8E67E588-FFF5-4860-A323-5E89B325D5EF}" ) )

class FAudioHRTFEffectParameters
{
public:
	FAudioHRTFEffectParameters()
		: EmitterPosition(0.0f)
		, SpatializationAlgorithm(SPATIALIZATION_TYPE_FAST)
	{}

	FAudioHRTFEffectParameters(const FVector& InEmitterPosition)
		: EmitterPosition(InEmitterPosition)
		, SpatializationAlgorithm(SPATIALIZATION_TYPE_FAST)
	{}

	FAudioHRTFEffectParameters(const FVector& InEmitterPosition, ESpatializationEffectType InSpatializationAlgorithm)
		: EmitterPosition(InEmitterPosition)
		, SpatializationAlgorithm(InSpatializationAlgorithm)
	{}

	FVector						EmitterPosition;
	ESpatializationEffectType	SpatializationAlgorithm;
};



class AUDIO_HRTF_EFFECT_CLASS_ID FXAudio2HRTFEffect : public CXAPOParametersBase
{
public:
	FXAudio2HRTFEffect(uint32 InVoiceId, FAudioDevice* InAudioDevice);
	STDMETHOD(LockForProcess)(UINT32 InputLockedParameterCount, const XAPO_LOCKFORPROCESS_BUFFER_PARAMETERS *pInputLockedParameters, UINT32 OutputLockedParameterCount, const XAPO_LOCKFORPROCESS_BUFFER_PARAMETERS *pOutputLockedParameters);
	STDMETHOD_(void, Process)(UINT32 InputProcessParameterCount, const XAPO_PROCESS_BUFFER_PARAMETERS *pInputProcessParameters, UINT32 OutputProcessParameterCount, XAPO_PROCESS_BUFFER_PARAMETERS *pOutputProcessParameters, BOOL IsEnabled);

private:

	FAudioHRTFEffectParameters				HRTFEffectParameters[3];
	FVector									EmitterPosition;
	WAVEFORMATEX							WaveFormat;

	/* Which voice index this effect is attached to. */
	uint32									InputFrameCount;
	static XAPO_REGISTRATION_PROPERTIES		Registration;
	uint32									InputChannels;
	uint32									OutputChannels;
	uint32									VoiceId;
	FAudioDevice*							AudioDevice;
	bool									bPassThrough;
};

