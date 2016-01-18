// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Sound/AudioVolume.h"
#include "Sound/SoundConcurrency.h"
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

	// The priority of sound, which is the product of the component priority and the USoundBased priority
	float Priority;

	float Pitch;

	// How far in to the sound the
	float StartTime;

	// At what distance from the source of the sound should spatialization begin
	float OmniRadius;

	// The distance between left and right channels when spatializing stereo assets
	float StereoSpread;

	// Which spatialization algorithm to use
	ESoundSpatializationAlgorithm SpatializationAlgorithm;

	// The lowpass filter frequency to apply (if enabled)
	float LowPassFilterFrequency;

	// The lowpass filter frequency to apply due to distance attenuation
	float AttenuationFilterFrequency;

	// The lowpass filter to apply if the sound is occluded
	float OcclusionFilterFrequency;

	// The lowpass filter to apply if the sound is inside an ambient zone
	float AmbientZoneFilterFrequency;

	// Whether the sound should be spatialized
	uint32 bUseSpatialization:1;

	// Whether the sound should be seamlessly looped
	uint32 bLooping:1;
	
	// Whether we have enabled low-pass filtering of this sound
	uint32 bEnableLowPassFilter : 1;

	// Whether this sound is occluded
	uint32 bIsOccluded : 1;

	FSoundParseParameters()
		: SoundClass(NULL)
		, Velocity(ForceInit)
		, Volume(1.f)
		, VolumeMultiplier(1.f)
		, Pitch(1.f)
		, StartTime(-1.f)
		, OmniRadius(0.0f)
		, StereoSpread(0.0f)
		, SpatializationAlgorithm(SPATIALIZATION_Default)
		, LowPassFilterFrequency(MAX_FILTER_FREQUENCY)
		, AttenuationFilterFrequency(MAX_FILTER_FREQUENCY)
		, OcclusionFilterFrequency(MAX_FILTER_FREQUENCY)
		, AmbientZoneFilterFrequency(MAX_FILTER_FREQUENCY)
		, bUseSpatialization(false)
		, bLooping(false)
		, bEnableLowPassFilter(false)
		, bIsOccluded(false)
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

private:
	TWeakObjectPtr<class UAudioComponent> AudioComponent;
	UPTRINT AudioComponentIndex;

public:

	bool IsAudioComponentValid() const { return AudioComponent.IsValid(); }
	UAudioComponent* GetAudioComponent() const { return AudioComponent.Get(); }
	UPTRINT GetAudioComponentIndex() const { return AudioComponentIndex; }
	void SetAudioComponent(UAudioComponent* Component)
	{
		AudioComponent = Component;
		AudioComponentIndex = (UPTRINT)Component;
	}

	void SetAudioDevice(FAudioDevice* InAudioDevice)
	{
		AudioDevice = InAudioDevice;
	}

	FAudioDevice* AudioDevice;

	/** The group of active concurrent sounds that this sound is playing in. */
	FConcurrencyGroupID ConcurrencyGroupID;

	/** Optional USoundConcurrency to override sound */
	USoundConcurrency* ConcurrencySettings;

	/** Optional SoundClass to override Sound */
	USoundClass* SoundClassOverride;

	/** whether we were occluded the last time we checked */
	uint32 bIsOccluded:1;

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

	/** Whether or not to stop this active sound due to max concurrency */
	uint32 bShouldStopDueToMaxConcurrency:1;

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

#if !(NO_LOGGING || UE_BUILD_SHIPPING || UE_BUILD_TEST)
	/** For debugging purposes, output to the log once that a looping sound has been orphaned */
	uint32 bWarnedAboutOrphanedLooping:1;
#endif

	/** Whether or not we have a low-pass filter enabled on this active sound. */
	uint32 bEnableLowPassFilter : 1;

	/** Whether or not we have occlusion checks enabled on this active sound. */
	uint32 bEnableOcclusionChecks : 1;

	/** Whether or not we use complex occlusion traces. */
	uint32 bUseComplexOcclusionChecks : 1;

	/** Whether or not to use an async trace for occlusion */
	uint32 bOcclusionAsyncTrace : 1;

	uint8 UserIndex;

	float PlaybackTime;
	float RequestedStartTime;

	float CurrentAdjustVolumeMultiplier;
	float TargetAdjustVolumeMultiplier;
	float TargetAdjustVolumeStopTime;

	float VolumeMultiplier;
	float PitchMultiplier;

	/** The low-pass filter frequency to apply if bEnableLowPassFilter is true. */
	float LowPassFilterFrequency;

	/** The occlusion lowpass filter frequency to apply if bEnableOcclusionChecks is true. This will override LowPassFilterFrequency if the sound is occluded. */
	float OcclusionLowPassFilterFrequency;

	/** The time it takes to interpolate from occluded to unoccluded filtering */
	float OcclusionInterpolationTime;

	/** The amount of volume attenuation to apply to sounds which are occluded */
	float OcclusionVolumeAttenuation;

	/** The interpolated parameter for the low-pass frequency due to occlusion */
	FDynamicParameter CurrentOcclusionFilterFrequency;

	FDynamicParameter CurrentOcclusionVolumeAttenuation;

	/** A volume scale to apply to a sound based on the concurrency count of the active sound when it started */
	float ConcurrencyVolumeScale;

	float SubtitlePriority;

	/** The product of the component priority and the USoundBase priority */
	float Priority;

	/** Frequency with which to check for occlusion from its closest listener */
	// The volume used to determine concurrency resolution for "quietest" active sound
	float VolumeConcurrency;

	/** Frequency with which to check for occlusion from its closest listener */
	/** The time in seconds with which to check for occlusion from its closest listener */
	float OcclusionCheckInterval;

	/** Last time we checked for occlusion */
	float LastOcclusionCheckTime;

	/** The max distance this sound will be audible. */
	float MaxDistance;

	FTransform Transform;

	/** Location last time playback was updated */
	FVector LastLocation;

	FAttenuationSettings AttenuationSettings;

	/** Cache what volume settings we had last time so we don't have to search again if we didn't move */
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

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	FName DebugOriginalSoundName;
#endif

	// Updates the wave instances to be played.
	void UpdateWaveInstances( TArray<FWaveInstance*> &OutWaveInstances, const float DeltaTime );

	/** 
	 * Find an existing waveinstance attached to this audio component (if any)
	 */
	FWaveInstance* FindWaveInstance(const UPTRINT WaveInstanceHash);

	/** 
	 * Check whether to apply the radio filter
	 */
	void ApplyRadioFilter(const struct FSoundParseParameters& ParseParams );

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
	
	/** Returns the unique ID of the active sound's owner if it exists. Returns 0 if the sound doesn't have an owner. */
	uint32 TryGetOwnerID() const;

	/** Gets the sound concurrency to apply on this active sound instance */
	const FSoundConcurrencySettings* GetSoundConcurrencySettingsToApply() const;
	void OcclusionTraceDone(const FTraceHandle& TraceHandle, FTraceDatum& TraceDatum);

	/** Returns the sound concurrency object ID if it exists. If it doesn't exist, returns 0. */
	uint32 GetSoundConcurrencyObjectID() const;

	/** Applies the active sound's attenuation settings to the input parse params using the given listener */
	void ApplyAttenuation(FSoundParseParameters& ParseParams, const FListener& Listener);

	/** Whether or not this active sound is out of range of its max distance relative to the closest listener */
	bool IsOutOfRange() const;

private:
	
	/** Whether or not this sound is audible */
	bool bIsAudible;

	/** This is a friend so the audio device can call Stop() on the active sound. */
	friend class FAudioDevice;

	/** Stops the active sound. Can only be called from the owning audio device. */
	void Stop();

	void UpdateAdjustVolumeMultiplier(const float DeltaTime);

	/** if OcclusionCheckInterval > 0.0, checks if the sound has become (un)occluded during playback
	 * and calls eventOcclusionChanged() if so
	 * primarily used for gameplay-relevant ambient sounds
	 * CurrentLocation is the location of this component that will be used for playback
	 * @param ListenerLocation location of the closest listener to the sound
	 */
	void CheckOcclusion(const FVector ListenerLocation, const FVector SoundLocation);
	
	void SetIsOccluded(bool bInOccluded);

	FTraceDelegate OcclusionTraceDelegate;

	/** Apply the interior settings to the ambient sound as appropriate */
	void HandleInteriorVolumes( const FListener& Listener, struct FSoundParseParameters& ParseParams );

	/** Struct used to cache listener attenuation vector math results */
	struct FAttenuationListenerData
	{
		FVector ListenerToSoundDir;
		float AttenuationDistance;
		float ListenerToSoundDistance;
		bool bDataComputed;

		FAttenuationListenerData()
			: ListenerToSoundDir(FVector::ZeroVector)
			, AttenuationDistance(0.0f)
			, ListenerToSoundDistance(0.0f)
			, bDataComputed(false)
		{}
	};

	/** Performs listener-based calculations for spatializing and attenuating the active sound */
	void GetAttenuationListenerData(FAttenuationListenerData& Data, const FSoundParseParameters& ParseParams, const FListener& Listener) const;
};