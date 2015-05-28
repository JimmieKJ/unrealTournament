// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Sound/AudioVolume.h"
#include "Sound/SoundAttenuation.h"
#include "Components/AudioComponent.h"

/**
 *	Struct used for gathering the final parameters to apply to a wave instance
 */
struct FSoundParseParameters
{
	// A collection of 
	FNotifyBufferFinishedHooks NotifyBufferFinishedHooks;

	// The Sound Class to use the settings of
	class USoundClass* SoundClass;
	
	// The transform of the sound (scale is not used)
	FTransform Transform;

	// The speed that the sound is moving relative to the listener
	FVector Velocity;
	
	float Volume;
	float VolumeMultiplier;

	float Pitch;
	float HighFrequencyGain;

	// How far in to the sound the
	float StartTime;

	// At what distance from the source of the sound should spatialization begin
	float OmniRadius;

	// Which spatialization algorithm to use
	ESoundSpatializationAlgorithm SpatializationAlgorithm;

	// Whether the sound should be spatialized
	uint32 bUseSpatialization:1;

	// Whether the sound should be seamlessly looped
	uint32 bLooping:1;

	FSoundParseParameters()
		: SoundClass(NULL)
		, Velocity(ForceInit)
		, Volume(1.f)
		, VolumeMultiplier(1.f)
		, Pitch(1.f)
		, HighFrequencyGain(1.f)
		, StartTime(-1.f)
		, OmniRadius(0.0f)
		, SpatializationAlgorithm(SPATIALIZATION_Default)
		, bUseSpatialization(false)
		, bLooping(false)
	{
	}
};

struct ENGINE_API FActiveSound
{
public:

	FActiveSound();
	~FActiveSound();

	class USoundBase* Sound;
	TWeakObjectPtr<class UWorld> World;
	TWeakObjectPtr<class UAudioComponent> AudioComponent;

	/** Optional SoundClass to override Sound */
	USoundClass* SoundClassOverride;

	/** whether we were occluded the last time we checked */
	uint32 bOccluded:1;

	/** Is this sound allowed to be spatialized? */
	uint32 bAllowSpatialization:1;

	/** Does this sound have attenuation settings specified */
	uint32 bHasAttenuationSettings:1;

	/** Whether the wave instances should remain active if they're dropped by the prioritization code. Useful for e.g. vehicle sounds that shouldn't cut out. */
	uint32 bShouldRemainActiveIfDropped:1;

	/** Is the audio component currently fading out */
	uint32 bFadingOut:1;

	/** Whether the current component has finished playing */
	uint32 bFinished:1;

	/** If true, the decision on whether to apply the radio filter has been made. */
	uint32 bRadioFilterSelected:1;

	/** If true, this sound will not be stopped when flushing the audio device. */
	uint32 bApplyRadioFilter:1;

	/** If true, the AudioComponent will be notified when a Wave is started to handle subtitles */
	uint32 bHandleSubtitles:1;

	/** Whether the Location of the component is well defined */
	uint32 bLocationDefined:1;

	/** If true, this sound will not be stopped when flushing the audio device. */
	uint32 bIgnoreForFlushing:1;

	/** Whether audio effects are applied */
	uint32 bEQFilterApplied:1;

	/** Whether to artificially prioritize the component to play */
	uint32 bAlwaysPlay:1;

	/** Whether or not this sound plays when the game is paused in the UI */
	uint32 bIsUISound:1;

	/** Whether or not this audio component is a music clip */
	uint32 bIsMusic:1;

	/** Whether or not the audio component should be excluded from reverb EQ processing */
	uint32 bReverb:1;

	/** Whether or not this sound class forces sounds to the center channel */
	uint32 bCenterChannelOnly:1;

	/** Whether we have queried for the interior settings at least once */
	uint32 bGotInteriorSettings:1;

#if !NO_LOGGING
	/** For debugging purposes, output to the log once that a looping sound has been orphaned */
	uint32 bWarnedAboutOrphanedLooping:1;
#endif

	uint8 UserIndex;

	float PlaybackTime;
	float RequestedStartTime;

	float CurrentAdjustVolumeMultiplier;
	float TargetAdjustVolumeMultiplier;
	float TargetAdjustVolumeStopTime;

	float VolumeMultiplier;
	float PitchMultiplier;
	float HighFrequencyGainMultiplier;

	float SubtitlePriority;

	/** frequency with which to check for occlusion from its closest listener */
	float OcclusionCheckInterval;

	/** last time we checked for occlusion */
	float LastOcclusionCheckTime;

	FTransform Transform;

	/** location last time playback was updated */
	FVector LastLocation;

	FAttenuationSettings AttenuationSettings;

	/** cache what volume settings we had last time so we don't have to search again if we didn't move */
	FInteriorSettings LastInteriorSettings;

	class AAudioVolume* LastAudioVolume;

	// To remember where the volumes are interpolating to and from
	double LastUpdateTime; 
	float SourceInteriorVolume;
	float SourceInteriorLPF;
	float CurrentInteriorVolume;
	float CurrentInteriorLPF;

	TMap<UPTRINT, struct FWaveInstance*> WaveInstances;

	TMap<UPTRINT,uint32> SoundNodeOffsetMap;
	TArray<uint8> SoundNodeData;

	TArray<FAudioComponentParam> InstanceParameters;

	// Updates the wave instances to be played.
	void UpdateWaveInstances( FAudioDevice* AudioDevice, TArray<FWaveInstance*> &WaveInstances, const float DeltaTime );

	void Stop(FAudioDevice* AudioDevice);

	/** 
	 * Find an existing waveinstance attached to this audio component (if any)
	 */
	FWaveInstance* FindWaveInstance(const UPTRINT WaveInstanceHash);

	/** 
	 * Check whether to apply the radio filter
	 */
	void ApplyRadioFilter( FAudioDevice* AudioDevice, const struct FSoundParseParameters& ParseParams );

	/** Sets a float instance parameter for the ActiveSound */
	void SetFloatParameter(const FName InName, const float InFloat);

	/** Sets a wave instance parameter for the ActiveSound */
	void SetWaveParameter(const FName InName, class USoundWave* InWave);

	/** Sets a boolean instance parameter for the ActiveSound */
	void SetBoolParameter(const FName InName, const bool InBool);
	
	/** Sets an integer instance parameter for the ActiveSound */
	void SetIntParameter(const FName InName, const int32 InInt);

	/**
	 * Try and find an Instance Parameter with the given name and if we find it return the float value.
	 * @return true if float for parameter was found, otherwise false
	 */
	bool GetFloatParameter(const FName InName, float& OutFloat) const;

	/**
	 *Try and find an Instance Parameter with the given name and if we find it return the USoundWave value.
	 * @return true if USoundWave for parameter was found, otherwise false
	 */
	bool GetWaveParameter(const FName InName, USoundWave*& OutWave) const;

	/**
	 *Try and find an Instance Parameter with the given name and if we find it return the boolean value.
	 * @return true if boolean for parameter was found, otherwise false
	 */
	bool GetBoolParameter(const FName InName, bool& OutBool) const;
	
	/**
	 *Try and find an Instance Parameter with the given name and if we find it return the integer value.
	 * @return true if boolean for parameter was found, otherwise false
	 */
	int32 GetIntParameter(const FName InName, int32& OutInt) const;

	void CollectAttenuationShapesForVisualization(TMultiMap<EAttenuationShape::Type, FAttenuationSettings::AttenuationShapeDetails>& ShapeDetailsMap) const;

	/**
	 * Friend archive function used for serialization.
	 */
	friend FArchive& operator<<( FArchive& Ar, FActiveSound* ActiveSound );

	void AddReferencedObjects( FReferenceCollector& Collector );

	/**
	 * Get the sound class to apply on this sound instance
	 */
	USoundClass* GetSoundClass() const;

	/* Determines which listener is the closest to the sound */
	int32 FindClosestListener( const TArray<struct FListener>& InListeners ) const;

private:
	void UpdateAdjustVolumeMultiplier(const float DeltaTime);

	/** if OcclusionCheckInterval > 0.0, checks if the sound has become (un)occluded during playback
	 * and calls eventOcclusionChanged() if so
	 * primarily used for gameplay-relevant ambient sounds
	 * CurrentLocation is the location of this component that will be used for playback
	 * @param ListenerLocation location of the closest listener to the sound
	 */
	void CheckOcclusion(const FVector ListenerLocation, const FVector SoundLocation);

	/** Apply the interior settings to the ambient sound as appropriate */
	void HandleInteriorVolumes( const FListener& Listener, struct FSoundParseParameters& ParseParams );

};