// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "Features/IModularFeature.h"
#include "Features/IModularFeatures.h"

/**
* FSpatializationParams
* Struct for retrieving parameters needed for computing 3d spatialization.
*/
struct FSpatializationParams
{
	/** The listener position (is likely at the origin). */
	FVector ListenerPosition;

	/** The listener orientation. */
	FVector ListenerOrientation;

	/** The emitter position relative to listener. */
	FVector EmitterPosition;

	/** The emitter world position. */
	FVector EmitterWorldPosition;

	/** The left channel position. */
	FVector LeftChannelPosition;

	/** The right channel position. */
	FVector RightChannelPosition;

	/** The distance between listener and emitter. */
	float Distance;

	/** The normalized omni radius, or the radius that will blend a sound to non-3d */
	float NormalizedOmniRadius;

	FSpatializationParams()
		: ListenerPosition(FVector::ZeroVector)
		, ListenerOrientation(FVector::ZeroVector)
		, EmitterPosition(FVector::ZeroVector)
		, EmitterWorldPosition(FVector::ZeroVector)
		, LeftChannelPosition(FVector::ZeroVector)
		, RightChannelPosition(FVector::ZeroVector)
		, Distance(0.0f)
	{}
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
	virtual void ProcessSpatializationForVoice(uint32 VoiceIndex, float* InSamples, float* OutSamples, const FVector& Position)
	{
	}

	/** Uses the given HRTF algorithm to spatialize a mono audio stream, assumes the parameters have already been set before processing. */
	virtual void ProcessSpatializationForVoice(uint32 VoiceIndex, float* InSamples, float* OutSamples)
	{
	}

	/** Sets the spatialization effect parameters. */
	virtual void SetSpatializationParameters(uint32 VoiceId, const FSpatializationParams& Params)
	{
	}

	/** Gets the spatialization effect parameters. */
	virtual void GetSpatializationParameters(uint32 VoiceId, FSpatializationParams& OutParams)
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
