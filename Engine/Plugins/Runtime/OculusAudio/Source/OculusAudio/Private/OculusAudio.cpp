// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "OculusAudio.h"
#include "OVR_Audio.h"
#include "XAudio2Device.h"
#include "AudioEffect.h"
#include "XAudio2Effects.h"
#include "Engine.h"
#include "OculusAudioEffect.h"
#include "Runtime/Windows/XAudio2/Private/XAudio2Support.h"

//---------------------------------------------------
// Oculus Audio Plugin Implementation
//---------------------------------------------------

class FHrtfSpatializationAlgorithm : public IAudioSpatializationAlgorithm
{
public:
	FHrtfSpatializationAlgorithm(class FAudioDevice * AudioDevice)
		: bOvrContextInitialized(false)
		, AudioDevice(AudioDevice)
		, OvrAudioContextFast(0)
#if OCULUS_HQ_ENABLED
		, OvrAudioContextHighQuality(0)
#endif
	{
	}

	~FHrtfSpatializationAlgorithm()
	{
		for (int32 i = 0; i < HRTFEffects.Num(); ++i)
		{
			HRTFEffects[i].Reset();
		}

		HRTFEffects.Empty();

		// Destroy the contexts if we created them
		if (bOvrContextInitialized)
		{
			if (OvrAudioContextFast != nullptr)
			{
				ovrAudio_DestroyContext(OvrAudioContextFast);
			}

#if OCULUS_HQ_ENABLED
			if (OvrAudioContextHighQuality != nullptr)
			{
				ovrAudio_DestroyContext(OvrAudioContextHighQuality);
			}
#endif
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
		ContextConfig.acc_Provider = ovrAudioSpatializationProvider_OVR_Fast;
		ContextConfig.acc_MaxNumSources = MaxNumSources;
		ContextConfig.acc_SampleRate = SampleRate;
		ContextConfig.acc_BufferLength = BufferLength;

		check(OvrAudioContextFast == nullptr);
		// Create the OVR Audio Context with a given quality
		if (!ovrAudio_CreateContext(&OvrAudioContextFast, &ContextConfig))
		{
			const char* ErrorString = ovrAudio_GetLastError(OvrAudioContextFast);
			UE_LOG(LogAudio, Warning, TEXT("Failed to create a Fast OVR Audio context: %s"), ErrorString);
			return;
		}

		// Now initialize the high quality algorithm context
#if OCULUS_HQ_ENABLED
		ContextConfig.acc_Provider = ovrAudioSpatializationProvider_OVR_HQ;
		check(OvrAudioContextHighQuality == nullptr);
		// Create the OVR Audio Context with a given quality
		if (!ovrAudio_CreateContext(&OvrAudioContextHighQuality, &ContextConfig))
		{
			const char* ErrorString = ovrAudio_GetLastError(OvrAudioContextHighQuality);
			UE_LOG(LogAudio, Warning, TEXT("Failed to create a High Quality OVR Audio context: %s"), ErrorString);
			return;
		}
#endif // #if OCULUS_HQ_ENABLED

		bOvrContextInitialized = true;
	}

	void ProcessSpatializationForVoice(ESpatializationEffectType Type, uint32 VoiceIndex, float* InSamples, float* OutSamples, const FVector& Position) override
	{
		if (Type == SPATIALIZATION_TYPE_FAST)
		{
			ProcessAudio(OvrAudioContextFast, VoiceIndex, InSamples, OutSamples, Position);
		}
#if OCULUS_HQ_ENABLED
		else
		{
			check(Type == SPATIALIZATION_TYPE_HIGH_QUALITY);
			ProcessAudio(OvrAudioContextHighQuality, VoiceIndex, InSamples, OutSamples, Position);
		}
#endif // #if OCULUS_HQ_ENABLED
	}

	virtual bool CreateSpatializationEffect(uint32 VoiceId) override
	{
		// If an effect for this voice has already been created, then leave
		if (VoiceId < (uint32)HRTFEffects.Num())
		{
			return false;
		}

		TSharedPtr<FXAudio2HRTFEffect> NewHRTFEffect = TSharedPtr<FXAudio2HRTFEffect>(new FXAudio2HRTFEffect(VoiceId, AudioDevice));
		NewHRTFEffect->Initialize(nullptr, 0);
		HRTFEffects.Add(NewHRTFEffect);
		return true;
	}

	void* GetSpatializationEffect(uint32 VoiceId) override
	{
		if (VoiceId < (uint32)HRTFEffects.Num())
		{
			TSharedPtr<FXAudio2HRTFEffect> Effect = HRTFEffects[VoiceId];
			if (Effect.IsValid())
			{
				return (void*)Effect.Get();
			}
		}
		return nullptr;
	}

	void SetSpatializationParameters(uint32 VoiceId, const FVector& EmitterPosition, ESpatializationEffectType AlgorithmType) override
	{
		if (VoiceId < (uint32)HRTFEffects.Num())
		{
			TSharedPtr<FXAudio2HRTFEffect> Effect = HRTFEffects[VoiceId];
			if (Effect.IsValid())
			{
				FAudioHRTFEffectParameters Params(EmitterPosition);
				Effect->SetParameters((const void *)&Params, sizeof(Params));
			}
		}
	}

private:

	void ProcessAudio(ovrAudioContext AudioContext, uint32 VoiceIndex, float* InSamples, float* OutSamples, const FVector& Position)
	{
		if (!ovrAudio_SetAudioSourceAttenuationMode(AudioContext, VoiceIndex, ovrAudioSourceAttenuationMode_None, 1.0f))
		{
			const char* ErrorString = ovrAudio_GetLastError(AudioContext);
			UE_LOG(LogAudio, Warning, TEXT("Failed to set the OVR source attenuation mode for voice index %d: %s"), VoiceIndex, ErrorString);
			return;
		}

		// Translate the input position to OVR coordinates
		FVector OvrPosition = ToOVRVector(Position);

		// Set the source position to current audio position
		if (!ovrAudio_SetAudioSourcePos(AudioContext, VoiceIndex, OvrPosition.X, OvrPosition.Y, OvrPosition.Z))
		{
			const char* ErrorString = ovrAudio_GetLastError(AudioContext);
			UE_LOG(LogAudio, Warning, TEXT("Failed to set the OVR source position (%.4f, %.4f, %.4f) for voice %d: %s"), OvrPosition.X, OvrPosition.Y, OvrPosition.Z, VoiceIndex, ErrorString);
			return;
		}

		// Perform the processing
		if (!ovrAudio_SpatializeMonoSourceInterleaved(AudioContext, VoiceIndex, OutSamples, InSamples))
		{
			const char* ErrorString = ovrAudio_GetLastError(AudioContext);
			UE_LOG(LogAudio, Warning, TEXT("Failed to OVR spatialize for for voice %d: %s"), VoiceIndex, ErrorString);
			return;
		}
	}

	/** Helper function to convert from UE coords to OVR coords. */
	FORCEINLINE FVector ToOVRVector(const FVector& InVec) const
	{
		return FVector(float(InVec.Y), float(InVec.Z), float(-InVec.X));
	}

	/* Whether or not the OVR audio context is initialized. We defer initialization until the first audio callback.*/
	bool bOvrContextInitialized;

	/** Ptr to parent audio device */
	FAudioDevice* AudioDevice;

	/** The OVR Audio context initialized to "Fast" algorithm. */
	ovrAudioContext OvrAudioContextFast;

#if OCULUS_HQ_ENABLED
	/** The OVR Audio context initialized to "High Quality" algorithm. */
	ovrAudioContext OvrAudioContextHighQuality;
#endif

	/** Xaudio2 effects for the oculus plugin */
	TArray<TSharedPtr<FXAudio2HRTFEffect>> HRTFEffects;

};


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
			if (!ovrAudio_Initialize())
			{
				UE_LOG(LogAudio, Error, TEXT("Failed to initialize OVR Audio system"));
				return;
			}

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
		// Better have more than 0 instances here
		check(FOculusAudioPlugin::NumInstances > 0);

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
		return new FHrtfSpatializationAlgorithm(AudioDevice);
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
