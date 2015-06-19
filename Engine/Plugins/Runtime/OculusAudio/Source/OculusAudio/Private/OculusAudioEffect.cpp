// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "OculusAudio.h"
#include "OVR_Audio.h"
#include "AudioDevice.h"
#include "XAudio2Device.h"
#include "AudioEffect.h"
#include "XAudio2Effects.h"
#include "Engine.h"
#include "OculusAudioEffect.h"

#include "AllowWindowsPlatformTypes.h"
#include <xapobase.h>
#include <xapofx.h>
#include <xaudio2fx.h>
#include "HideWindowsPlatformTypes.h"

FXAudio2HRTFEffect::FXAudio2HRTFEffect(uint32 InVoiceId, FAudioDevice* InAudioDevice)
	: CXAPOParametersBase(&Registration, (uint8*)HRTFEffectParameters, sizeof(FAudioHRTFEffectParameters), false)
	, VoiceId(InVoiceId)
	, AudioDevice(InAudioDevice)
	, bPassThrough(false)
{
	FAudioHRTFEffectParameters DefaultParameters;
	SetParameters(&DefaultParameters, sizeof(DefaultParameters));
}

HRESULT FXAudio2HRTFEffect::LockForProcess(UINT32 InputLockedParameterCount, const XAPO_LOCKFORPROCESS_BUFFER_PARAMETERS *pInputLockedParameters, UINT32 OutputLockedParameterCount, const XAPO_LOCKFORPROCESS_BUFFER_PARAMETERS *pOutputLockedParameters)
{
	// Try to lock the process on the base class before attempting to do any initialization here. 
	HRESULT Result = CXAPOParametersBase::LockForProcess(InputLockedParameterCount, pInputLockedParameters, OutputLockedParameterCount, pOutputLockedParameters);

	// Process couldn't be locked. ABORT! 
	if (FAILED(Result))
	{
		return Result;
	}

	// For user XAPOs, the frame count is locked to MaxFrameCount
	InputFrameCount = pInputLockedParameters->MaxFrameCount;

	// Make sure that our incoming channels is 1 and outgoing is 2
	InputChannels = pInputLockedParameters[0].pFormat->nChannels;
	OutputChannels = pOutputLockedParameters[0].pFormat->nChannels;

	// Make sure this effect is set up correctly to have 1 input channel and 2 output channels
	check(InputChannels == 1);
	check(OutputChannels == 2);

	// Store the wave format locally on this effect to use in the Process() function.
	FMemory::Memcpy(&WaveFormat, pInputLockedParameters[0].pFormat, sizeof(WAVEFORMATEX));
	return S_OK;
}

void FXAudio2HRTFEffect::Process(UINT32 InputProcessParameterCount, const XAPO_PROCESS_BUFFER_PARAMETERS *pInputProcessParameters, UINT32 OutputProcessParameterCount, XAPO_PROCESS_BUFFER_PARAMETERS *pOutputProcessParameters, BOOL IsEnabled)
{
	// Verify several condition based on the registration 
	// properties we used to create the class. 
	if (!IsLocked())
		return;

	check(InputProcessParameterCount == 1);
	check(OutputProcessParameterCount == 1);

	// Make sure our input and output buffers are *not* the same. This is not an in-process plugin
	check(pInputProcessParameters[0].pBuffer != pOutputProcessParameters[0].pBuffer);

	// Don't do anything if we aren't enabled or there is no output volume
	if (!IsEnabled || (WaveFormat.nChannels != 1))
	{
		return;
	}

	// This locks the parameters until the EndProcess() call at the bottom. 
	FAudioHRTFEffectParameters* CurrentParameters = (FAudioHRTFEffectParameters*)BeginProcess();

	// We set the emitter position and the current algorithm from outside the XAPO plugin
	FVector & EmitterPosition = CurrentParameters->EmitterPosition;
	ESpatializationEffectType SpatializationAlgorithm = CurrentParameters->SpatializationAlgorithm;

	// We better not have default set at this point because default means "no HRTF processing". We also
	// better thave the plugin because this XAPO code is inside the plugin
	check(SpatializationAlgorithm != SPATIALIZATION_TYPE_DEFAULT);
	check(AudioDevice->SpatializationPlugin != nullptr);

	XAPO_BUFFER_FLAGS InFlags = pInputProcessParameters[0].BufferFlags;
	XAPO_BUFFER_FLAGS OutFlags = pOutputProcessParameters[0].BufferFlags;

	switch (InFlags)
	{
	case XAPO_BUFFER_VALID:
	{
		// Get the input/output samples
		float* InputSamples = (float*)pInputProcessParameters[0].pBuffer;
		float* OutputSamples = (float *)pOutputProcessParameters[0].pBuffer;

		// If we're in pass through mode, we're not going to the oculus SDK, but instead, we're going to
		// just split the mono stream into a stereo stream.
		if (bPassThrough)
		{
			uint32 SampleIndex = 0;
			for (uint32 FrameIndex = 0; FrameIndex < InputFrameCount; ++FrameIndex)
			{
				// Scale by 0.5f to preserve power when splitting into stereo
				float InputSample = InputSamples[FrameIndex];
				OutputSamples[SampleIndex++] = 0.5f * InputSample;
				OutputSamples[SampleIndex++] = 0.5f * InputSample;
			}
		}
		else
		{
			check(AudioDevice->SpatializeProcessor);

			// Check if for OVR audio context initialization. We do this here because there apparently isn't a way to get
			// the effect buffer size until the actual callback. 
			if (!AudioDevice->SpatializeProcessor->IsSpatializationEffectInitialized())
			{
				AudioDevice->SpatializeProcessor->InitializeSpatializationEffect(InputFrameCount);
			}

			// Spatialize the audio stream with the current algorithm
			AudioDevice->SpatializeProcessor->ProcessSpatializationForVoice(
				SpatializationAlgorithm, 
				VoiceId, 
				InputSamples, 
				OutputSamples, 
				EmitterPosition
			);
		}
		pOutputProcessParameters[0].BufferFlags = XAPO_BUFFER_VALID;
		pOutputProcessParameters[0].ValidFrameCount = InputFrameCount;
	}
	break;

	case XAPO_BUFFER_SILENT:
	{
		pOutputProcessParameters[0].BufferFlags = XAPO_BUFFER_SILENT;
		pOutputProcessParameters[0].ValidFrameCount = InputFrameCount;
	}
	break;
	}

	EndProcess();
}

/** Define the registration properties for the radio effect. */
XAPO_REGISTRATION_PROPERTIES FXAudio2HRTFEffect::Registration =
{
	__uuidof(FXAudio2HRTFEffect),										// clsid
	TEXT("FXAudio2HRTFEffect"),											// Friendly Name
	TEXT("Copyright 1998-2015 Epic Games, Inc. All Rights Reserved."),	// Registration string length
	1, 0,																// Major/Minor Version
	XAPO_FLAG_FRAMERATE_MUST_MATCH |									// Flags: note, this is not supporting in-place processing
	XAPO_FLAG_BITSPERSAMPLE_MUST_MATCH |
	XAPO_FLAG_BUFFERCOUNT_MUST_MATCH,
	1, 1, 1, 1															// all buffer counts are the same
};
