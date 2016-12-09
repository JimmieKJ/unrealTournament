// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "OculusAudio.h"
#include "OVR_Audio.h"
#include "XAudio2Device.h"
#include "AudioEffect.h"
#include "XAudio2Effects.h"
#include "OculusAudioEffect.h"
#include "Runtime/Windows/XAudio2/Private/XAudio2Support.h"
#include "Misc/Paths.h"

//---------------------------------------------------
// Oculus Audio Plugin Implementation
//---------------------------------------------------

static const TCHAR* GetOculusErrorString(ovrResult Result)
{
	switch (Result)
	{
		default:
		case ovrError_AudioUnknown:			return TEXT("Unknown Error");
		case ovrError_AudioInvalidParam:	return TEXT("Invalid Param");
		case ovrError_AudioBadSampleRate:	return TEXT("Bad Samplerate");
		case ovrError_AudioMissingDLL:		return TEXT("Missing DLL");
		case ovrError_AudioBadAlignment:	return TEXT("Pointers did not meet 16 byte alignment requirements");
		case ovrError_AudioUninitialized:	return TEXT("Function called before initialization");
		case ovrError_AudioHRTFInitFailure: return TEXT("HRTF Profider initialization failed");
		case ovrError_AudioBadVersion:		return TEXT("Bad audio version");
		case ovrError_AudioSRBegin:			return TEXT("Sample rate begin");
		case ovrError_AudioSREnd:			return TEXT("Sample rate end");
	}
}

#define OVR_AUDIO_CHECK(Result, Context)																\
	if (Result != ovrSuccess)																			\
	{																									\
		const TCHAR* ErrString = GetOculusErrorString(Result);											\
		UE_LOG(LogAudio, Error, TEXT("Oculus Audio SDK Error - %s: %s"), TEXT(Context), ErrString);	\
		return;																							\
	}

class FHrtfSpatializationAlgorithmXAudio2 : public IAudioSpatializationAlgorithm
{
public:
	FHrtfSpatializationAlgorithmXAudio2(class FAudioDevice * AudioDevice)
		: bOvrContextInitialized(false)
		, AudioDevice(AudioDevice)
		, OvrAudioContext(nullptr)
	{
		// XAudio2's buffer lengths are always 1 percent of sample rate
		uint32 BufferLength = AudioDevice->SampleRate / 100;
		InitializeSpatializationEffect(BufferLength);

		check(HRTFEffects.Num() == 0);
		check(Params.Num() == 0);

		for (int32 EffectIndex = 0; EffectIndex < (int32)AudioDevice->MaxChannels; ++EffectIndex)
		{
			FXAudio2HRTFEffect* NewHRTFEffect = new FXAudio2HRTFEffect(EffectIndex, AudioDevice);
			NewHRTFEffect->Initialize(nullptr, 0);
			HRTFEffects.Add(NewHRTFEffect);

			Params.Add(FSpatializationParams());
		}
	}

	~FHrtfSpatializationAlgorithmXAudio2()
	{
		// Release all the effects for the oculus spatialization effect
		for (FXAudio2HRTFEffect* Effect : HRTFEffects)
		{
			if (Effect)
			{
				delete Effect;
			}
		}
		HRTFEffects.Empty();

		// Destroy the contexts if we created them
		if (bOvrContextInitialized)
		{
			if (OvrAudioContext != nullptr)
			{
				ovrAudio_DestroyContext(OvrAudioContext);
				OvrAudioContext = nullptr;
			}

			bOvrContextInitialized = false;
		}
	}

	bool IsSpatializationEffectInitialized() const override
	{
		return bOvrContextInitialized;
	}

	void InitializeSpatializationEffect(uint32 BufferLength) override
	{
		if (bOvrContextInitialized)
		{
			return;
		}

		uint32 MaxNumSources = AudioDevice->MaxChannels;
		uint32 SampleRate = AudioDevice->SampleRate;

		ovrAudioContextConfiguration ContextConfig;
		ContextConfig.acc_Size = sizeof(ovrAudioContextConfiguration);

		// First initialize the Fast algorithm context
		ContextConfig.acc_Provider = ovrAudioSpatializationProvider_OVR_OculusHQ;
		ContextConfig.acc_MaxNumSources = MaxNumSources;
		ContextConfig.acc_SampleRate = SampleRate;
		ContextConfig.acc_BufferLength = BufferLength;

		check(OvrAudioContext == nullptr);
		// Create the OVR Audio Context with a given quality
		ovrResult Result = ovrAudio_CreateContext(&OvrAudioContext, &ContextConfig);
		OVR_AUDIO_CHECK(Result, "Failed to create simple context");

		// Now initialize the high quality algorithm context
		bOvrContextInitialized = true;
	}

	void ProcessSpatializationForVoice(uint32 VoiceIndex, float* InSamples, float* OutSamples, const FVector& Position) override
	{
		if (OvrAudioContext)
		{
			ProcessAudio(OvrAudioContext, VoiceIndex, InSamples, OutSamples, Position);
		}
	}

	virtual bool CreateSpatializationEffect(uint32 VoiceId) override
	{
		// If an effect for this voice has already been created, then leave
		if ((int32)VoiceId >= HRTFEffects.Num())
		{
			return false;
		}
		return true;
	}

	void* GetSpatializationEffect(uint32 VoiceId) override
	{
		if ((int32)VoiceId < HRTFEffects.Num())
		{
			return (void*)HRTFEffects[VoiceId];
		}
		return nullptr;
	}

	void SetSpatializationParameters(uint32 VoiceId, const FSpatializationParams& InParams) override
	{
		check((int32)VoiceId < Params.Num());

		FScopeLock ScopeLock(&ParamCriticalSection);
		Params[VoiceId] = InParams;
	}

	void GetSpatializationParameters(uint32 VoiceId, FSpatializationParams& OutParams) override
	{
		check((int32)VoiceId < Params.Num());

		FScopeLock ScopeLock(&ParamCriticalSection);
		OutParams = Params[VoiceId];
	}

private:

	void ProcessAudio(ovrAudioContext AudioContext, uint32 VoiceIndex, float* InSamples, float* OutSamples, const FVector& Position)
	{
		ovrResult Result = ovrAudio_SetAudioSourceAttenuationMode(AudioContext, VoiceIndex, ovrAudioSourceAttenuationMode_None, 1.0f);
		OVR_AUDIO_CHECK(Result, "Failed to set source attenuation mode");

		// Translate the input position to OVR coordinates
		FVector OvrPosition = ToOVRVector(Position);

		// Set the source position to current audio position
		Result = ovrAudio_SetAudioSourcePos(AudioContext, VoiceIndex, OvrPosition.X, OvrPosition.Y, OvrPosition.Z);
		OVR_AUDIO_CHECK(Result, "Failed to set audio source position");

		// Perform the processing
		uint32 Status;
		Result = ovrAudio_SpatializeMonoSourceInterleaved(AudioContext, VoiceIndex, ovrAudioSpatializationFlag_None, &Status, OutSamples, InSamples);
		OVR_AUDIO_CHECK(Result, "Failed to spatialize mono source interleaved");
	}

	/** Helper function to convert from UE coords to OVR coords. */
	FORCEINLINE FVector ToOVRVector(const FVector& InVec) const
	{
		return FVector(float(InVec.Y), float(-InVec.Z), float(-InVec.X));
	}

	/* Whether or not the OVR audio context is initialized. We defer initialization until the first audio callback.*/
	bool bOvrContextInitialized;

	/** Ptr to parent audio device */
	FAudioDevice* AudioDevice;

	/** The OVR Audio context initialized to "Fast" algorithm. */
	ovrAudioContext OvrAudioContext;

	/** Xaudio2 effects for the oculus plugin */
	TArray<class FXAudio2HRTFEffect*> HRTFEffects;
	TArray<FSpatializationParams> Params;

	FCriticalSection ParamCriticalSection;
};

/**
* FHrtfSpatializationAlgorithmAudioMixer
* Simple implementation of the IAudioSpatializationAlgorithm interface for use with audio mixer.
*/

class FHrtfSpatializationAlgorithmAudioMixer : public IAudioSpatializationAlgorithm
{
public:
	FHrtfSpatializationAlgorithmAudioMixer(class FAudioDevice * AudioDevice)
		: bOvrContextInitialized(false)
		, AudioDevice(AudioDevice)
		, OvrAudioContext(nullptr)
	{
		uint32 MaxNumSources = AudioDevice->MaxChannels;
		int32 SampleRate = (int32)AudioDevice->GetSampleRate();
		int32 BufferLength = AudioDevice->GetBufferLength();

		check(MaxNumSources > 0);
		check(SampleRate > 0);
		check(BufferLength > 0);

		Params.AddDefaulted(MaxNumSources);

		ovrAudioContextConfiguration ContextConfig;
		ContextConfig.acc_Size = sizeof(ovrAudioContextConfiguration);

		// First initialize the Fast algorithm context
		ContextConfig.acc_Provider = ovrAudioSpatializationProvider_OVR_OculusHQ;
		ContextConfig.acc_MaxNumSources = MaxNumSources;
		ContextConfig.acc_SampleRate = SampleRate;
		ContextConfig.acc_BufferLength = (uint32)BufferLength;

		check(OvrAudioContext == nullptr);
		// Create the OVR Audio Context with a given quality
		ovrResult Result = ovrAudio_CreateContext(&OvrAudioContext, &ContextConfig);
		OVR_AUDIO_CHECK(Result, "Failed to create simple context");

		// Now initialize the high quality algorithm context
		bOvrContextInitialized = true;
	}

	bool IsSpatializationEffectInitialized() const override
	{
		return bOvrContextInitialized;
	}

	void InitializeSpatializationEffect(uint32 BufferLength) override
	{
		if (bOvrContextInitialized)
		{
			return;
		}

		uint32 MaxNumSources = AudioDevice->MaxChannels;
		uint32 SampleRate = AudioDevice->SampleRate;

		ovrAudioContextConfiguration ContextConfig;
		ContextConfig.acc_Size = sizeof(ovrAudioContextConfiguration);

		// First initialize the Fast algorithm context
		ContextConfig.acc_Provider = ovrAudioSpatializationProvider_OVR_OculusHQ;
		ContextConfig.acc_MaxNumSources = MaxNumSources;
		ContextConfig.acc_SampleRate = SampleRate;
		ContextConfig.acc_BufferLength = BufferLength;

		check(OvrAudioContext == nullptr);
		// Create the OVR Audio Context with a given quality
		ovrResult Result = ovrAudio_CreateContext(&OvrAudioContext, &ContextConfig);
		OVR_AUDIO_CHECK(Result, "Failed to create simple context");

		for (int32 SourceId = 0; SourceId < (int32)MaxNumSources; ++SourceId)
		{
			Result = ovrAudio_SetAudioSourceAttenuationMode(OvrAudioContext, SourceId, ovrAudioSourceAttenuationMode_None, 1.0f);
			OVR_AUDIO_CHECK(Result, "Failed to set source attenuation mode");
		}

		// Now initialize the high quality algorithm context
		bOvrContextInitialized = true;
	}

	virtual void SetSpatializationParameters(uint32 VoiceId, const FSpatializationParams& InParams)
	{
		Params[VoiceId] = InParams;
	}

	void ProcessSpatializationForVoice(uint32 SourceId, float* InSamples, float* OutSamples) override
	{
		if (OvrAudioContext)
		{
			// Translate the input position to OVR coordinates
			FVector OvrPosition = ToOVRVector(Params[SourceId].EmitterPosition);

			// Set the source position to current audio position
			ovrResult Result = ovrAudio_SetAudioSourcePos(OvrAudioContext, SourceId, OvrPosition.X, OvrPosition.Y, OvrPosition.Z);
			OVR_AUDIO_CHECK(Result, "Failed to set audio source position");

			// Perform the processing
			uint32 Status;
			Result = ovrAudio_SpatializeMonoSourceInterleaved(OvrAudioContext, SourceId, ovrAudioSpatializationFlag_None, &Status, OutSamples, InSamples);
			OVR_AUDIO_CHECK(Result, "Failed to spatialize mono source interleaved");
		}
	}

private:

	/** Helper function to convert from UE coords to OVR coords. */
	FORCEINLINE FVector ToOVRVector(const FVector& InVec) const
	{
		return FVector(InVec.Y, -InVec.Z, -InVec.X);
	}

	/* Whether or not the OVR audio context is initialized. We defer initialization until the first audio callback.*/
	bool bOvrContextInitialized;

	TArray<FSpatializationParams> Params;

	/** Ptr to parent audio device */
	FAudioDevice* AudioDevice;

	/** The OVR Audio context initialized to "Fast" algorithm. */
	ovrAudioContext OvrAudioContext;
};

/**
* FOculusAudioPlugin
*/

class FOculusAudioPlugin : public IOculusAudioPlugin
{
public:
	FOculusAudioPlugin()
	{
	}

	void Initialize() override
	{
		if (FOculusAudioPlugin::NumInstances == 0)
		{
			if (!LoadModule())
			{
				UE_LOG(LogAudio, Error, TEXT("Failed to load OVR Audio dll"));
				return;
			}
		}

		FOculusAudioPlugin::NumInstances++;

		if (!FOculusAudioPlugin::bInitialized)
		{
			// Check the version number
			int32 MajorVersionNumber;
			int32 MinorVersionNumber;
			int32 PatchNumber;

			// Initialize the OVR Audio SDK before making any calls to ovrAudio
			ovrResult Result = ovrAudio_Initialize();
			OVR_AUDIO_CHECK(Result, "Failed to initialize OVR Audio system");

			const char* OvrVersionString = ovrAudio_GetVersion(&MajorVersionNumber, &MinorVersionNumber, &PatchNumber);
			if (MajorVersionNumber != OVR_AUDIO_MAJOR_VERSION || MinorVersionNumber != OVR_AUDIO_MINOR_VERSION)
			{
				UE_LOG(LogAudio, Warning, TEXT("Using mismatched OVR Audio SDK Versiont! %d.%d vs. %d.%d"), OVR_AUDIO_MAJOR_VERSION, OVR_AUDIO_MINOR_VERSION, MajorVersionNumber, MinorVersionNumber);
				return;
			}
			FOculusAudioPlugin::bInitialized = true;
		}
	}

	void Shutdown() override
	{
		if (FOculusAudioPlugin::NumInstances == 0)
		{
			// This means we failed to load the OVR Audio module during initialization and there's nothing to shutdown.
			return;
		}

		FOculusAudioPlugin::NumInstances--;

		if (FOculusAudioPlugin::NumInstances == 0)
		{
			// Shutdown OVR audio
			ovrAudio_Shutdown();

			ShutdownModule();

			FOculusAudioPlugin::bInitialized = false;
		}
	}

	IAudioSpatializationAlgorithm* GetNewSpatializationAlgorithm(class FAudioDevice* AudioDevice) override
	{
		// If we are using the audio mixer, use a new implementation
		if (AudioDevice->IsAudioMixerEnabled())
		{
			return new FHrtfSpatializationAlgorithmAudioMixer(AudioDevice);
		}

		// Otherwise, use the xaudio2 implementation
		return new FHrtfSpatializationAlgorithmXAudio2(AudioDevice);
	}

private:

	bool LoadModule()
	{
		if (!FOculusAudioPlugin::OculusAudioDllHandle)
		{
			FString Path = FPaths::EngineDir() / FString::Printf(TEXT("Binaries/ThirdParty/Oculus/Audio/Win64/"));
			FPlatformProcess::PushDllDirectory(*Path);
			FOculusAudioPlugin::OculusAudioDllHandle = FPlatformProcess::GetDllHandle(*(Path + "ovraudio64.dll"));
			FPlatformProcess::PopDllDirectory(*Path);
			return FOculusAudioPlugin::OculusAudioDllHandle != nullptr;
		}
		return true;
	}

	void ReleaseModule()
	{

		if (FOculusAudioPlugin::NumInstances == 0 && FOculusAudioPlugin::OculusAudioDllHandle)
		{
			FPlatformProcess::FreeDllHandle(OculusAudioDllHandle);
			FOculusAudioPlugin::OculusAudioDllHandle = nullptr;
		}
	}

	static void* OculusAudioDllHandle;
	static uint32 NumInstances;
	static bool bInitialized;
};

void* FOculusAudioPlugin::OculusAudioDllHandle = nullptr;
uint32 FOculusAudioPlugin::NumInstances = 0;
bool FOculusAudioPlugin::bInitialized = false;

IMPLEMENT_MODULE( FOculusAudioPlugin, OculusAudio )
