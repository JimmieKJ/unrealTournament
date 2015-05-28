// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ModuleInterface.h"
#include "ModuleManager.h"
#include "Runtime/Core/Public/Features/IModularFeatures.h"

// Whether or not we are enabling the "High Quality" Version of the spatialization algorithm
#define OCULUS_HQ_ENABLED (0) // Note: We are disabling the HQ version since the "FAST" quality seems good.

enum ESpatializationEffectType
{
	SPATIALIZATION_TYPE_DEFAULT,
	SPATIALIZATION_TYPE_FAST,
#if OCULUS_HQ_ENABLED
	SPATIALIZATION_TYPE_HIGH_QUALITY,
#endif // #if OCULUS_HQ_ENABLED
	SPATIALIZATION_TYPE_COUNT
};

/**
* IAudioSpatializationAlgorithm
*
* This class represents instances of a plugin that will process spatialization for a stream of audio.
* Currently used to process a mono-stream through an HRTF spatialization algorithm into a stereo stream.
* This algorithm contains an audio effect assigned to every VoiceId (playing sound instance). It assumes
* the effect is updated in the audio engine update loop with new position information.
*
*/
class IAudioSpatializationAlgorithm
{
public:
	/** Virtual destructor */
	virtual ~IAudioSpatializationAlgorithm()
	{
	}

	/** Uses the given HRTF algorithm to spatialize a mono audio stream. */
	virtual void ProcessSpatializationForVoice(ESpatializationEffectType Type, uint32 VoiceIndex, float* InSamples, float* OutSamples, const FVector& Position)
	{

	}
	/** Sets the spatialization effect parameters. */

	virtual void SetSpatializationParameters(uint32 VoiceId, const FVector& EmitterPosition, ESpatializationEffectType AlgorithmType)
	{
	}

	/** Returns whether or not the spatialization effect has been initialized */
	virtual bool IsSpatializationEffectInitialized() const
	{
		return false;
	}

	/** Initializes the spatialization effect with the given buffer length. */
	virtual void InitializeSpatializationEffect(uint32 BufferLength)
	{
	}

	/** Creates an audio spatialization effect. */
	virtual bool CreateSpatializationEffect(uint32 VoiceId)
	{
		return true;
	}

	/**	Returns the spatialization effect for the given voice id. */
	virtual void* GetSpatializationEffect(uint32 VoiceId)
	{
		return nullptr;
	}
};

/**
* The public interface of the MotionControlsModule
*/
class IAudioSpatializationPlugin : public IModuleInterface, public IModularFeature
{
public:
	// IModularFeature
	static FName GetModularFeatureName()
	{
		static FName AudioExtFeatureName = FName(TEXT("AudioSpatialization"));
		return AudioExtFeatureName;
	}

	// IModuleInterface
	virtual void StartupModule() override
	{
		IModularFeatures::Get().RegisterModularFeature(GetModularFeatureName(), this);
	}

	/**
	* Singleton-like access to IAudioExtensionPlugin
	*
	* @return Returns IAudioExtensionPlugin singleton instance, loading the module on demand if needed
	*/
	static inline IAudioSpatializationPlugin& Get()
	{
		return FModuleManager::LoadModuleChecked< IAudioSpatializationPlugin >("AudioSpatialization");
	}

	/**
	* Checks to see if this module is loaded and ready.  It is only valid to call Get() if IsAvailable() returns true.
	*
	* @return True if the module is loaded and ready to use
	*/
	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded("AudioSpatialization");
	}

	/**
	* Initializes the spatialization plugin.
	*
	*/
	virtual void Initialize()
	{
	}

	/**
	* Shuts down the spatialization plugin.
	*
	*/
	virtual void Shutdown()
	{

	}

	/**
	* Returns a new spatialization algorithm instance
	*
	* @return A new spatialization algorithm instance
	*/
	virtual IAudioSpatializationAlgorithm* GetNewSpatializationAlgorithm(class FAudioDevice* AudioDevice)
	{
		return nullptr;
	}
};