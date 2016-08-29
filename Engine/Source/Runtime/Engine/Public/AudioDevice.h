// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "Sound/AudioVolume.h"
#include "Sound/SoundMix.h"
#include "AudioDeviceManager.h"

/**
 * Forward declares
 */

class FAudioEffectsManager;
class ICompressedAudioInfo;
class IAudioSpatializationPlugin;
class IAudioSpatializationAlgorithm;
class UReverbEffect;
class FViewportClient;

/** 
 * Debug state of the audio system
 */
enum EDebugState
{
	// No debug sounds
	DEBUGSTATE_None,
	// No reverb sounds
	DEBUGSTATE_IsolateDryAudio,
	// Only reverb sounds
	DEBUGSTATE_IsolateReverb,
	// Force LPF on all sources
	DEBUGSTATE_TestLPF,
	// Bleed stereo sounds fully to the rear speakers
	DEBUGSTATE_TestStereoBleed,
	// Bleed all sounds to the LFE speaker
	DEBUGSTATE_TestLFEBleed,
	// Disable any LPF filter effects
	DEBUGSTATE_DisableLPF,
	// Disable any radio filter effects
	DEBUGSTATE_DisableRadio,
	DEBUGSTATE_MAX,
};

/**
 * Current state of a SoundMix
 */
namespace ESoundMixState
{
	enum Type
	{
		// Waiting to fade in
		Inactive,
		// Fading in
		FadingIn,
		// Fully active
		Active,
		// Fading out
		FadingOut,
		// Time elapsed, just about to be removed
		AwaitingRemoval,
	};
}

namespace ESortedActiveWaveGetType
{
	enum Type
	{
		FullUpdate,
		PausedUpdate,
		QueryOnly,
	};
}

namespace ERequestedAudioStats
{
	static const uint8 SoundWaves = 0x1;
	static const uint8 SoundCues = 0x2;
	static const uint8 Sounds = 0x4;
	static const uint8 DebugSounds = 0x8;
	static const uint8 LongSoundNames = 0x01;
};

/** 
 * Defines the properties of the listener
 */
struct FListener
{
	FTransform Transform;
	FVector Velocity;

	struct FInteriorSettings InteriorSettings;

	/** The ID of the volume the listener resides in */
	uint32 AudioVolumeID;

	/** The ID of the world the listener resides in */
	uint32 WorldID;

	/** The times of interior volumes fading in and out */
	double InteriorStartTime;
	double InteriorEndTime;
	double ExteriorEndTime;
	double InteriorLPFEndTime;
	double ExteriorLPFEndTime;
	float InteriorVolumeInterp;
	float InteriorLPFInterp;
	float ExteriorVolumeInterp;
	float ExteriorLPFInterp;

	FVector GetUp() const		{ return Transform.GetUnitAxis(EAxis::Z); }
	FVector GetFront() const	{ return Transform.GetUnitAxis(EAxis::Y); }
	FVector GetRight() const	{ return Transform.GetUnitAxis(EAxis::X); }

	/**
	 * Works out the interp value between source and end
	 */
	float Interpolate(const double EndTime);

	/**
	 * Gets the current state of the interior settings for the listener
	 */
	void UpdateCurrentInteriorSettings();

	/** 
	 * Apply the interior settings to ambient sounds
	 */
	void ApplyInteriorSettings(uint32 AudioVolumeID, const FInteriorSettings& Settings);

	FListener()
		: Transform(FTransform::Identity)
		, Velocity(ForceInit)
		, AudioVolumeID(0)
		, InteriorStartTime(0.0)
		, InteriorEndTime(0.0)
		, ExteriorEndTime(0.0)
		, InteriorLPFEndTime(0.0)
		, ExteriorLPFEndTime(0.0)
		, InteriorVolumeInterp(0.f)
		, InteriorLPFInterp(0.f)
		, ExteriorVolumeInterp(0.f)
		, ExteriorLPFInterp(0.f)
	{
	}
};

/** 
 * Structure for collating info about sound classes
 */
struct FAudioClassInfo
{
	int32 NumResident;
	int32 SizeResident;
	int32 NumRealTime;
	int32 SizeRealTime;

	FAudioClassInfo()
		: NumResident(0)
		, SizeResident(0)
		, NumRealTime(0)
		, SizeRealTime(0)
	{
	}
};

struct FSoundMixState
{
	bool IsBaseSoundMix;
	uint32 ActiveRefCount;
	uint32 PassiveRefCount;
	double StartTime;
	double FadeInStartTime;
	double FadeInEndTime;
	double FadeOutStartTime;
	double EndTime;
	float InterpValue;
	ESoundMixState::Type CurrentState;
};

struct FSoundMixClassOverride
{
	FSoundClassAdjuster SoundClassAdjustor;
	FDynamicParameter VolumeOverride;
	FDynamicParameter PitchOverride;
	float FadeInTime;
	uint8 bOverrideApplied : 1;
	uint8 bOverrideChanged : 1;
	uint8 bIsClearing : 1;
	uint8 bIsCleared : 1;

	FSoundMixClassOverride()
		: VolumeOverride(1.0f)
		, PitchOverride(1.0f)
		, FadeInTime(0.0f)
		, bOverrideApplied(false)
		, bOverrideChanged(false)
		, bIsClearing(false)
		, bIsCleared(false)
	{
	}
};

typedef TMap<USoundClass*, FSoundMixClassOverride> FSoundMixClassOverrideMap;

struct FActivatedReverb
{
	FReverbSettings ReverbSettings;
	float Priority;

	FActivatedReverb()
		: Priority(0.f)
	{
	}
};

/** Struct used to cache listener attenuation vector math results */
struct FAttenuationListenerData
{
	FVector ListenerToSoundDir;
	FTransform ListenerTransform;
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

struct FAttenuationFocusData
{
	float FocusFactor;
	float DistanceScale;
	float PriorityScale;
	float VolumeScale;

	FAttenuationFocusData()
		: FocusFactor(1.0f)
		, DistanceScale(1.0f)
		, PriorityScale(1.0f)
		, VolumeScale(1.0f)
	{}
};

/*
* Setting for global focus scaling
*/
struct FGlobalFocusSettings
{
	float FocusAzimuthScale;
	float NonFocusAzimuthScale;
	float FocusDistanceScale;
	float NonFocusDistanceScale;
	float FocusVolumeScale;
	float NonFocusVolumeScale;
	float FocusPriorityScale;
	float NonFocusPriorityScale;

	FGlobalFocusSettings()
		: FocusAzimuthScale(1.0f)
		, NonFocusAzimuthScale(1.0f)
		, FocusDistanceScale(1.0f)
		, NonFocusDistanceScale(1.0f)
		, FocusVolumeScale(1.0f)
		, NonFocusVolumeScale(1.0f)
		, FocusPriorityScale(1.0f)
		, NonFocusPriorityScale(1.0f)
	{}
};

#if !UE_BUILD_SHIPPING
struct FAudioStats
{
	FAudioStats()
		: bStale(true)
	{
	}

	struct FStatWaveInstanceInfo
	{
		FString Description;
		float ActualVolume;
		int32 InstanceIndex;
	};

	struct FStatSoundInfo
	{
		FString SoundName;
		FName SoundClassName;
		float Distance;
		uint32 AudioComponentID;
		FTransform Transform;
		TArray<FStatWaveInstanceInfo> WaveInstanceInfos;
		TMultiMap<EAttenuationShape::Type, FAttenuationSettings::AttenuationShapeDetails> ShapeDetailsMap;
	};

	uint8 bStale:1;
	FVector ListenerLocation;
	TArray<FStatSoundInfo> StatSoundInfos;

};
#endif

class ENGINE_API FAudioDevice : public FExec
{
public:

	//Begin FExec Interface
	virtual bool Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar = *GLog) override;
	//End FExec Interface

#if !UE_BUILD_SHIPPING
private:
	/**
	 * Exec command handlers
	 */
	bool HandleDumpSoundInfoCommand(const TCHAR* Cmd, FOutputDevice& Ar);
	/**
	 * Lists all the loaded sounds and their memory footprint
	 */
	bool HandleListSoundsCommand(const TCHAR* Cmd, FOutputDevice& Ar);
	/**
	 * Lists all the playing waveinstances and their associated source
	 */
	bool HandleListWavesCommand(const TCHAR* Cmd, FOutputDevice& Ar);
	/**
	 * Lists a summary of loaded sound collated by class
	 */
	bool HandleListSoundClassesCommand(const TCHAR* Cmd, FOutputDevice& Ar);
	/**
	 * shows sound class hierarchy
	 */
	bool HandleShowSoundClassHierarchyCommand(const TCHAR* Cmd, FOutputDevice& Ar);
	bool HandleListSoundClassVolumesCommand(const TCHAR* Cmd, FOutputDevice& Ar);
	bool HandleListAudioComponentsCommand(const TCHAR* Cmd, FOutputDevice& Ar);
	bool HandleListSoundDurationsCommand(const TCHAR* Cmd, FOutputDevice& Ar);
	bool HandleSoundTemplateInfoCommand(const TCHAR* Cmd, FOutputDevice& Ar);
	bool HandlePlaySoundCueCommand(const TCHAR* Cmd, FOutputDevice& Ar);
	bool HandlePlaySoundWaveCommand(const TCHAR* Cmd, FOutputDevice& Ar);
	bool HandleSetBaseSoundMixCommand(const TCHAR* Cmd, FOutputDevice& Ar);
	bool HandleIsolateDryAudioCommand(const TCHAR* Cmd, FOutputDevice& Ar);
	bool HandleIsolateReverbCommand(const TCHAR* Cmd, FOutputDevice& Ar);
	bool HandleTestLPFCommand(const TCHAR* Cmd, FOutputDevice& Ar);
	bool HandleTestStereoBleedCommand(const TCHAR* Cmd, FOutputDevice& Ar);
	bool HandleTestLFEBleedCommand(const TCHAR* Cmd, FOutputDevice& Ar);
	bool HandleDisableLPFCommand(const TCHAR* Cmd, FOutputDevice& Ar);
	bool HandleDisableRadioCommand(const TCHAR* Cmd, FOutputDevice& Ar);
	bool HandleEnableRadioCommand(const TCHAR* Cmd, FOutputDevice& Ar);
	bool HandleResetSoundStateCommand(const TCHAR* Cmd, FOutputDevice& Ar);
	bool HandleToggleSpatializationExtensionCommand(const TCHAR* Cmd, FOutputDevice& Ar);
	bool HandleEnableHRTFForAllCommand(const TCHAR* Cmd, FOutputDevice& Ar);
	bool HandleSoloCommand(const TCHAR* Cmd, FOutputDevice& Ar);
	bool HandleClearSoloCommand(const TCHAR* Cmd, FOutputDevice& Ar);
	bool HandlePlayAllPIEAudioCommand(const TCHAR* Cmd, FOutputDevice& Ar);
	bool HandleAudio3dVisualizeCommand(const TCHAR* Cmd, FOutputDevice& Ar);
	bool HandleAudioMemoryInfo(const TCHAR* Cmd, FOutputDevice& Ar);
	bool HandleAudioSoloSoundClass(const TCHAR* Cmd, FOutputDevice& Ar);
	bool HandleAudioSoloSoundWave(const TCHAR* Cmd, FOutputDevice& Ar);
	bool HandleAudioSoloSoundCue(const TCHAR* Cmd, FOutputDevice& Ar);

	/**
	* Lists a summary of loaded sound collated by class
	*/
	void ShowSoundClassHierarchy(FOutputDevice& Ar, USoundClass* SoundClass = nullptr, int32 Indent = 0) const;

	/** 
	* Gets a summary of loaded sound collated by class
	*/
	void GetSoundClassInfo(TMap<FName, FAudioClassInfo>& AudioClassInfos);
#endif

public:

	/**
	 * Constructor
	 */
	FAudioDevice();

	virtual ~FAudioDevice()
	{
	}

	/** Returns an array of available audio devices names for the platform */
	virtual void GetAudioDeviceList(TArray<FString>& OutAudioDeviceNames) const
	{
	}

	/**
	 * Basic initialisation of the platform agnostic layer of the audio system
	 */
	bool Init(int32 InMaxChannels);

	/**
	 * Tears down the audio device
	 */
	void Teardown();

	/**
	 * The audio system's main "Tick" function
	 */
	void Update(bool bGameTicking);

	/**
	 * Suspend/resume all sounds (global pause for device suspend/resume, etc.)
	 *
	 * @param bGameTicking Whether the game is still ticking at the time of suspend
	 */
	void Suspend(bool bGameTicking);

	/**
	 * Counts the bytes for the structures used in this class
	 */
	virtual void CountBytes(FArchive& Ar);

	/**
	 * Track references to UObjects
	 */
	void AddReferencedObjects(FReferenceCollector& Collector);

	/**
	 * Iterate over the active AudioComponents for wave instances that could be playing.
	 *
	 * @return Index of first wave instance that can have a source attached
	 */
	int32 GetSortedActiveWaveInstances(TArray<FWaveInstance*>& WaveInstances, const ESortedActiveWaveGetType::Type GetType);

	/**
	 * Stop all the audio components and sources attached to the world. nullptr world means all components.
	 */
	void Flush(UWorld* WorldToFlush, bool bClearActivatedReverb = true);

	/**
	 * Stop any playing sounds that are using a particular SoundWave
	 *
	 * @param SoundWave					Resource to stop any sounds that are using it
	 * @param[out] StoppedComponents	List of Audio Components that were stopped
	 */
	void StopSoundsUsingResource(USoundWave* SoundWave, TArray<UAudioComponent*>* StoppedComponents = nullptr);

#if WITH_EDITOR
	/** Deals with anything audio related that should happen when PIE starts */
	void OnBeginPIE(const bool bIsSimulating);

	/** Deals with anything audio related that should happen when PIE ends */
	void OnEndPIE(const bool bIsSimulating);
#endif

	/**
	 * Precaches the passed in sound node wave object.
	 *
	 * @param	SoundWave		Resource to be precached.
	 * @param	bSynchronous	If true, this function will block until a vorbis decompression is complete
	 * @param	bTrackMemory	If true, the audio mem stats will be updated
	 */
	virtual void Precache(USoundWave* SoundWave, bool bSynchronous = false, bool bTrackMemory=true);

	/**
	 * Precaches all existing sounds. Called when audio setup is complete
	 */
	virtual void PrecacheStartupSounds();

	/** 
	 * Sets the maximum number of channels dynamically. Can't raise the cap over the initial value but can lower it 
	 */
	virtual void SetMaxChannels(int32 InMaxChannels);

	/**
	* Stops any sound sources which are using the given buffer.
	*
	* @param	FSoundBuffer	Buffer to check against
	*/
	void StopSourcesUsingBuffer(FSoundBuffer * SoundBuffer);

	/**
	 * Stops all game sounds (and possibly UI) sounds
	 *
	 * @param bShouldStopUISounds If true, this function will stop UI sounds as well
	 */
	virtual void StopAllSounds(bool bShouldStopUISounds = false);

	/**
	 * Sets the details about the listener
	 * @param   ListenerIndex		The index of the listener
	 * @param   ListenerTransform   The listener's world transform
	 * @param   DeltaSeconds		The amount of time over which velocity should be calculated.  If 0, then velocity will not be calculated.
	 */
	void SetListener(UWorld* World, int32 InListenerIndex, const FTransform& ListenerTransform, float InDeltaSeconds);

	const TArray<FListener>& GetListeners() const { check(IsInAudioThread()); return Listeners; }

	/** 
	 * Returns the currently applied reverb effect if there is one.
	 */
	UReverbEffect* GetCurrentReverbEffect() const
	{
		check(IsInGameThread());
		return CurrentReverbEffect;
	}

	/**
	 * Creates an audio component to handle playing a sound.
	 * Plays a sound at the given location without creating an audio component.
	 * @param   Sound				The USoundBase to play at the location.
	 * @param   World				The world this sound is playing in.
	 * @param   AActor				The optional actor with which to play the sound on.
	 * @param   Play				Whether or not to automatically call play on the audio component after it is created.
	 * @param	bStopWhenOwnerDestroyed Whether or not to automatically stop the audio component if its owner is destroyed.
	 * @param	Location			The sound's location.
	 * @param	AttenuationSettings	The sound's attenuation settings to use. Will default to the USoundBase's AttenuationSettings if not specified.
	 * @param	USoundConcurrency	The sound's sound concurrency settings to use. Will use the USoundBase's USoundConcurrency if not specified.
	 * @return	The created audio component if the function successfully created one or a nullptr if not successful. Note: if audio is disabled or if there were no hardware audio devices available, this will return nullptr.
	 */
	static UAudioComponent* CreateComponent(USoundBase* Sound, UWorld* World, AActor*  AActor = nullptr, bool Play = true, bool bStopWhenOwnerDestroyed = false, const FVector* Location = nullptr, USoundAttenuation* AttenuationSettings = nullptr, USoundConcurrency* ConcurrencySettings = nullptr);

	/** 
	 * Plays a sound at the given location without creating an audio component.
	 * @param   Sound				The USoundBase to play at the location.
	 * @param   World				The world this sound is playing in.
	 * @param   VolumeMultiplier	The volume multiplier to set on the sound.
	 * @param   PitchMultiplier		The pitch multiplier to set on the sound.
	 * @param	StartTime			The initial time offset for the sound.
	 * @param	Location			The sound's position.
	 * @param	Rotation			The sound's rotation.
	 * @param	AttenuationSettings	The sound's attenuation settings to use (optional). Will default to the USoundBase's AttenuationSettings if not specified.
	 * @param	USoundConcurrency	The sound's sound concurrency settings to use (optional). Will use the USoundBase's USoundConcurrency if not specified.
	 * @param	Params				An optional list of audio component params to immediately apply to a sound.
	 */
	void PlaySoundAtLocation(USoundBase* Sound, UWorld* World, float VolumeMultiplier, float PitchMultiplier, float StartTime, const FVector& Location, const FRotator& Rotation, USoundAttenuation* AttenuationSettings = nullptr, USoundConcurrency* ConcurrencySettings = nullptr, const TArray<FAudioComponentParam>* Params = nullptr);

	/**
	 * Adds an active sound to the audio device
	 */
	void AddNewActiveSound(const FActiveSound& ActiveSound);

	/**
	 * Removes the active sound for the specified audio component
	 */
	void StopActiveSound(uint64 AudioComponentID);

	/**
	* Stops the active sound
	*/
	void StopActiveSound(FActiveSound* ActiveSound);

	/**
	* Finds the active sound for the specified audio component ID
	*/
	FActiveSound* FindActiveSound(uint64 AudioComponentID);

	/**
	 * Removes an active sound from the active sounds array
	 */
	void RemoveActiveSound(FActiveSound* ActiveSound);

	void AddAudioVolumeProxy(const FAudioVolumeProxy& Proxy);
	void RemoveAudioVolumeProxy(uint32 AudioVolumeID);
	void UpdateAudioVolumeProxy(const FAudioVolumeProxy& Proxy);

	struct FAudioVolumeSettings
	{
		uint32 AudioVolumeID;
		float Priority;
		FReverbSettings ReverbSettings;
		FInteriorSettings InteriorSettings;
	};

	void GetAudioVolumeSettings(const uint32 WorldID, const FVector& Location, FAudioVolumeSettings& OutSettings) const;

public:

	void SetDefaultAudioSettings(UWorld* World, const FReverbSettings& DefaultReverbSettings, const FInteriorSettings& DefaultInteriorSettings);

	/** 
	 * Gets the current audio debug state
	 */
	EDebugState GetMixDebugState() const { return((EDebugState)DebugState); }

	void SetMixDebugState(EDebugState DebugState);

	/**
	 * Set up the sound class hierarchy
	 */
	void InitSoundClasses();

protected:
	/**
	 * Set up the initial sound sources
	 * Allows us to initialize sound source early on, allowing for render callback hookups for iOS Audio.
	 */
	void InitSoundSources();

public:
	/**
	 * Registers a sound class with the audio device
	 *
	 * @param	SoundClassName	name of sound class to retrieve
	 * @return	sound class properties if it exists
	 */
	void RegisterSoundClass(USoundClass* InSoundClass);

	/**
	* Unregisters a sound class
	*/
	void UnregisterSoundClass(USoundClass* SoundClass);

	/**
	* Gets the current properties of a sound class, if the sound class hasn't been registered, then it returns nullptr
	*
	* @param	SoundClassName	name of sound class to retrieve
	* @return	sound class properties if it exists
	*/
	FSoundClassProperties* GetSoundClassCurrentProperties(USoundClass* InSoundClass);

	/**
	* Checks to see if a coordinate is within a distance of any listener
	*/
	bool LocationIsAudible(const FVector& Location, const float MaxDistance) const;

	/**
	* Checks to see if a coordinate is within a distance of the given listener
	*/
	static bool LocationIsAudible(const FVector& Location, const FTransform& ListenerTransform, const float MaxDistance);

	/**
	 * Sets the Sound Mix that should be active by default
	 */
	void SetDefaultBaseSoundMix(USoundMix* SoundMix);

	/**
	 * Removes a sound mix - called when SoundMix is unloaded
	 */
	void RemoveSoundMix(USoundMix* SoundMix);

	/** 
	 * Resets all interpolating values to defaults.
	 */
	void ResetInterpolation();

	/** Enables or Disables the radio effect. */
	void EnableRadioEffect(bool bEnable = false);

	friend class FAudioEffectsManager;
	/**
	 * Sets a new sound mix and applies it to all appropriate sound classes
	 */
	void SetBaseSoundMix(USoundMix* SoundMix);

	/**
	 * Push a SoundMix onto the Audio Device's list.
	 *
	 * @param SoundMix The SoundMix to push.
	 * @param bIsPassive Whether this is a passive push from a playing sound.
	 */
	void PushSoundMixModifier(USoundMix* SoundMix, bool bIsPassive = false);

	/** 
	 * Sets a sound class override in the given sound mix.
	 */
	void SetSoundMixClassOverride(USoundMix* InSoundMix, USoundClass* InSoundClass, float Volume, float Pitch, float FadeInTime, bool bApplyToChildren);

	/**
	* Clears a sound class override in the given sound mix.
	*/
	void ClearSoundMixClassOverride(USoundMix* InSoundMix, USoundClass* InSoundClass, float FadeOutTime);

	/**
	 * Pop a SoundMix from the Audio Device's list.
	 *
	 * @param SoundMix The SoundMix to pop.
	 * @param bIsPassive Whether this is a passive pop from a sound finishing.
	 */
	void PopSoundMixModifier(USoundMix* SoundMix, bool bIsPassive = false);

	/**
	 * Clear the effect of one SoundMix completely.
	 *
	 * @param SoundMix The SoundMix to clear.
	 */
	void ClearSoundMixModifier(USoundMix* SoundMix);

	/**
	 * Clear the effect of all SoundMix modifiers.
	 */
	void ClearSoundMixModifiers();

	/** Activates a Reverb Effect without the need for a volume
	 * @param ReverbEffect Reverb Effect to use
	 * @param TagName Tag to associate with Reverb Effect
	 * @param Priority Priority of the Reverb Effect
	 * @param Volume Volume level of Reverb Effect
	 * @param FadeTime Time before Reverb Effect is fully active
	 */
	void ActivateReverbEffect(UReverbEffect* ReverbEffect, FName TagName, float Priority, float Volume, float FadeTime);
	
	/**
	 * Deactivates a Reverb Effect not applied by a volume
	 *
	 * @param TagName Tag associated with Reverb Effect to remove
	 */
	void DeactivateReverbEffect(FName TagName);

	virtual FName GetRuntimeFormat(USoundWave* SoundWave) = 0;

	/** Whether this SoundWave has an associated info class to decompress it */
	virtual bool HasCompressedAudioInfoClass(USoundWave* SoundWave) { return false; }

	/** Whether this device supports realtime decompression of sound waves (i.e. DTYPE_RealTime) */
	virtual bool SupportsRealtimeDecompression() const
	{ 
		return false;
	}

	/** Creates a Compressed audio info class suitable for decompressing this SoundWave */
	virtual ICompressedAudioInfo* CreateCompressedAudioInfo(USoundWave* SoundWave) { return nullptr; }

	/**
	 * Check for errors and output a human readable string
	 */
	virtual bool ValidateAPICall(const TCHAR* Function, uint32 ErrorCode)
	{
		return true;
	}

	const TArray<FActiveSound*>& GetActiveSounds() const { return ActiveSounds; }

	/* When the set of Audio volumes have changed invalidate the cached values of active sounds */
	void InvalidateCachedInteriorVolumes() const;

	/** Suspend any context related objects */
	virtual void SuspendContext() {}
	
	/** Resume any context related objects */
	virtual void ResumeContext() {}

	/** Check if any background music or sound is playing through the audio device */
	virtual bool IsExernalBackgroundSoundActive() { return false; }

	/** Whether or not HRTF spatialization is enabled for all. */
	bool IsHRTFEnabledForAll() const;

	bool IsAudioDeviceMuted() const;

	void SetDeviceMuted(bool bMuted);

	/** Computes and returns some geometry related to the listener and the given sound transform. */
	void GetAttenuationListenerData(FAttenuationListenerData& OutListenerData, const FTransform& SoundTransform, const FAttenuationSettings& AttenuationSettings, const FTransform* InListenerTransform = nullptr) const;

	/** Returns the focus factor of a sound based on its position and listener data. */
	float GetFocusFactor(FAttenuationListenerData& OutListenerData, const USoundBase* Sound, const FTransform& SoundTransform, const FAttenuationSettings& AttenuationSettings, const FTransform* InListenerTransform = nullptr) const;

	/** Gets the max distance and focus factor of a sound. */
	void GetMaxDistanceAndFocusFactor(USoundBase* Sound, const UWorld* World, const FVector& Location, const FAttenuationSettings* AttenuationSettingsToApply, float& OutMaxDistance, float& OutFocusFactor);

	/**
	* Checks if the given sound would be audible.
	* @param Sound					The sound to check if it would be audible
	* @param World					The world the sound is playing in
	* @param Location				The location the sound is playing in the world
	* @param AttenuationSettings	The (optional) attenuation settings the sound is using
	* @param MaxDistance			The computed max distance of the sound.
	* @param FocusFactor			The focus factor of the sound.
	* @param Returns true if the sound is audible, false otherwise.
	*/
	bool SoundIsAudible(USoundBase* Sound, const UWorld* World, const FVector& Location, const FAttenuationSettings* AttenuationSettingsToApply, float MaxDistance, float FocusFactor);

	/** Returns the index of the listener closest to the given sound transform */
	static int32 FindClosestListenerIndex(const FTransform& SoundTransform, const TArray<FListener>& InListeners);
	int32 FindClosestListenerIndex(const FTransform& SoundTransform) const;

protected:
	friend class FSoundSource;

	/**
	 * Handle pausing/unpausing of sources when entering or leaving pause mode, or global pause (like device suspend)
	 */
	void HandlePause(bool bGameTicking, bool bGlobalPause = false);

	/**
	 * Stop sources that need to be stopped, and touch the ones that need to be kept alive
	 * Stop sounds that are too low in priority to be played
	 */
	void StopSources(TArray<FWaveInstance*>& WaveInstances, int32 FirstActiveIndex);

	/**
	 * Start and/or update any sources that have a high enough priority to play
	 */
	void StartSources(TArray<FWaveInstance*>& WaveInstances, int32 FirstActiveIndex, bool bGameTicking);

	/**
	 * Sets the 'pause' state of sounds which are always loaded.
	 *
	 * @param	bPaused			Pause sounds if true, play paused sounds if false.
	 */
	virtual void PauseAlwaysLoadedSounds(bool bPaused)
	{
	}

private:

	/**
	 * Parses the sound classes and propagates multiplicative properties down the tree.
	 */
	void ParseSoundClasses();


	/**
	 * Set the mix for altering sound class properties
	 *
	 * @param NewMix The SoundMix to apply
	 * @param SoundMixState The State associated with this SoundMix
	 */
	bool ApplySoundMix(USoundMix* NewMix, FSoundMixState* SoundMixState);

	/**
	 * Updates the state of a sound mix if it is pushed more than once.
	 *
	 * @param SoundMix The SoundMix we are updating
	 * @param SoundMixState The State associated with this SoundMix
	 */
	void UpdateSoundMix(USoundMix* SoundMix, FSoundMixState* SoundMixState);

	/**
	 * Updates list of SoundMixes that are applied passively, pushing and popping those that change
	 *
	 * @param WaveInstances Sorted list of active wave instances
	 * @param FirstActiveIndex Index of first wave instance that will be played.
	 */
	void UpdatePassiveSoundMixModifiers(TArray<FWaveInstance*>& WaveInstances, int32 FirstActiveIndex);

	/**
	 * Attempt to clear the effect of a particular SoundMix
	 *
	 * @param SoundMix The SoundMix we're attempting to clear
	 * @param SoundMixState The current state of this SoundMix
	 *
	 * @return Whether this SoundMix could be cleared (only true when both ref counts are zero).
	 */
	bool TryClearingSoundMix(USoundMix* SoundMix, FSoundMixState* SoundMixState);

	/**
	 * Attempt to remove this SoundMix's EQ effect - it may not currently be active
	 *
	 * @param SoundMix The SoundMix we're attempting to clear
	 *
	 * @return Whether the effect of this SoundMix was cleared
	 */
	bool TryClearingEQSoundMix(USoundMix* SoundMix);

	/**
	 * Find the SoundMix with the next highest EQ priority to the one passed in
	 *
	 * @param SoundMix The highest priority SoundMix, which will be ignored
	 *
	 * @return The next highest priority SoundMix or nullptr if one cannot be found
	 */
	USoundMix* FindNextHighestEQPrioritySoundMix(USoundMix* IgnoredSoundMix);

	/**
	 * Clear the effect of a SoundMix completely - only called after checking it's safe to
	 */
	void ClearSoundMix(USoundMix* SoundMix);

	/**
	 * Sets the sound class adjusters from a SoundMix.
	 *
	 * @param SoundMix		The SoundMix to apply adjusters from
	 * @param InterpValue	Proportion of adjuster to apply
	 * @param DeltaTime 	The current frame delta time. Used to interpolate sound class adjusters.
	 */
	void ApplyClassAdjusters(USoundMix* SoundMix, float InterpValue, float DeltaTime);

	/**
	* Construct the CurrentSoundClassProperties map
	* @param DeltaTime The current frame delta. Used to interpolate sound class adjustments.
	*
	* This contains the original sound class properties propagated properly, and all adjustments due to the sound mixes
	*/
	void UpdateSoundClassProperties(float DeltaTime);

	/**
	 * Recursively apply an adjuster to the passed in sound class and all children of the sound class
	 * 
	 * @param InAdjuster		The adjuster to apply
	 * @param InSoundClassName	The name of the sound class to apply the adjuster to.  Also applies to all children of this class
	 */
	void RecursiveApplyAdjuster(const FSoundClassAdjuster& InAdjuster, USoundClass* InSoundClass);

	/**
	 * Takes an adjuster value and modifies it by the proportion that is currently in effect
	 */
	float InterpolateAdjuster(const float Adjuster, const float InterpValue) const
	{
		return Adjuster * InterpValue + 1.0f - InterpValue;
	}

public:

	/**
	 * Platform dependent call to init effect data on a sound source
	 */
	void* InitEffect(FSoundSource* Source);

	/**
	 * Platform dependent call to update the sound output with new parameters
	 * The audio system's main "Tick" function
	 */
	void* UpdateEffect(FSoundSource* Source);

	/**
	 * Platform dependent call to destroy any effect related data
	 */
	void DestroyEffect(FSoundSource* Source);

	/**
	 * Return the pointer to the sound effects handler
	 */
	FAudioEffectsManager* GetEffects()
	{
		check(IsInAudioThread());
		return Effects;
	}

private:
	/**
	 * Internal helper function used by ParseSoundClasses to traverse the tree.
	 *
	 * @param CurrentClass			Subtree to deal with
	 * @param ParentProperties		Propagated properties of parent node
	 */
	void RecurseIntoSoundClasses(USoundClass* CurrentClass, FSoundClassProperties& ParentProperties);

	/**
	 * Find the current highest priority reverb after a change to the list of active ones.
	 */
	void UpdateHighestPriorityReverb();

	void SendUpdateResultsToGameThread(int32 FirstActiveIndex);

public:

// If we make FAudioDevice not be subclassable, then all the functions following would move to IAudioDeviceModule

	/** Starts up any platform specific hardware/APIs */
	virtual bool InitializeHardware()
	{
		return true;
	}

	/** Shuts down any platform specific hardware/APIs */
	virtual void TeardownHardware()
	{
	}

	/** Lets the platform any tick actions */
	virtual void UpdateHardware()
	{
	}

	/** Creates a new platform specific sound source */
	virtual FAudioEffectsManager* CreateEffectsManager();

	/** Creates a new platform specific sound source */
	virtual FSoundSource* CreateSoundSource() = 0;

	/** Low pass filter OneOverQ value */
	float GetLowPassFilterResonance() const;

	/** Wether or not the spatialization plugin is enabled. */
	bool IsSpatializationPluginEnabled() const
	{
		return bSpatializationExtensionEnabled;
	}

	void AddSoundToStop(struct FActiveSound* SoundToStop);

	/**   
	* Gets the direction of the given position vector transformed relative to listener.   
	* @param Position				Input position vector to transform relative to listener
	* @param OutDistance			Optional output of distance from position to listener
	* @return The input position relative to the listener.
	*/
	FVector GetListenerTransformedDirection(const FVector& Position, float* OutDistance);

private:
	/** Processes the set of pending sounds that need to be stopped */ 
	void ProcessingPendingActiveSoundStops(bool bForceDelete = false);

public:

	/** Query if the editor is in VR Preview for the current play world. Returns false for non-editor builds */
	static bool CanUseVRAudioDevice();

#if !UE_BUILD_SHIPPING
	void RenderStatReverb(UWorld* World, FViewport* Viewport, FCanvas* Canvas, int32 X, int32& Y, const FVector* ViewLocation, const FRotator* ViewRotation) const;

	void UpdateSoundShowFlags(const uint8 OldSoundShowFlags, const uint8 NewSoundShowFlags);
	void UpdateRequestedStat(const uint8 InRequestedStat);
	void ResolveDesiredStats(FViewportClient* ViewportClient);

	FAudioStats& GetAudioStats()
	{
		check(IsInGameThread());
		return AudioStats;
	}
#endif

	bool AreStartupSoundsPreCached() const { return bStartupSoundsPreCached; }

	float GetTransientMasterVolume() const { check(IsInAudioThread()); return TransientMasterVolume; }
	void SetTransientMasterVolume(float TransientMasterVolume);

	FSoundSource* GetSoundSource(FWaveInstance* WaveInstance) const;

	const FGlobalFocusSettings& GetGlobalFocusSettings() const;
	void SetGlobalFocusSettings(const FGlobalFocusSettings& NewFocusSettings);

	const FDynamicParameter& GetGlobalPitchScale() const { check(IsInAudioThread()); return GlobalPitchScale; }
	void SetGlobalPitchModulation(float PitchScale, float TimeSec);

	float GetPlatformAudioHeadroom() const { check(IsInAudioThread()); return PlatformAudioHeadroom; }
	void SetPlatformAudioHeadroom(float PlatformHeadRoom);

	DEPRECATED(4.13, "Direct access of SoundClasses is no longer allowed. Instead you should use the SoundMixClassOverride system")
	const TMap<USoundClass*, FSoundClassProperties>& GetSoundClassPropertyMap() const
	{
		check(IsInAudioThread());
		return SoundClasses;
	}

public:

	/** The maximum number of concurrent audible sounds */
	int32 MaxChannels;

	/** The sample rate of the audio device */
	int32 SampleRate;

	/** The amount of memory to reserve for always resident sounds */
	int32 CommonAudioPoolSize;

	/** Pointer to permanent memory allocation stack. */
	void* CommonAudioPool;

	/** Available size in permanent memory stack */
	int32 CommonAudioPoolFreeBytes;

	/** The handle for this audio device used in the audio device manager. */
	uint32 DeviceHandle;

	/** Audio spatialization plugin. */
	IAudioSpatializationPlugin* SpatializationPlugin;

	/** Audio spatialization algorithm (derived from a plugin). */
	IAudioSpatializationAlgorithm* SpatializeProcessor;

private:
	// Audio thread representation of listeners
	TArray<FListener> Listeners;

	// Game thread cache of listener transforms
	TArray<FTransform> ListenerTransforms;

	uint64 CurrentTick;

	/** An AudioComponent to play test sounds on */
	TWeakObjectPtr<UAudioComponent> TestAudioComponent;

	/** The debug state of the audio device */
	TEnumAsByte<enum EDebugState> DebugState;

	/** transient master volume multiplier that can be modified at runtime without affecting user settings automatically reset to 1.0 on level change */
	float TransientMasterVolume;

	/** Global dynamic pitch scale parameter */
	FDynamicParameter GlobalPitchScale;

	/** The global focus settings */
	FGlobalFocusSettings GlobalFocusSettings;
	FGlobalFocusSettings GlobalFocusSettings_GameThread;
	
	/** Timestamp of the last update */
	double LastUpdateTime;

	/** Next resource ID to assign out to a wave/buffer */
	int32 NextResourceID;

	/** Set of sources used to play sounds (platform will subclass these) */
protected:
	TArray<FSoundSource*> Sources;
	TArray<FSoundSource*> FreeSources;

private:
	TMap<FWaveInstance*, FSoundSource*>	WaveInstanceSourceMap;

	/** Current properties of all sound classes */
	TMap<USoundClass*, FSoundClassProperties>	SoundClasses;

	/** The Base SoundMix that's currently active */
	USoundMix* BaseSoundMix;

	/** The Base SoundMix that should be applied by default */
	USoundMix* DefaultBaseSoundMix;

	/** Map of sound mixes currently affecting audio properties */
	TMap<USoundMix*, FSoundMixState> SoundMixModifiers;

	/** Map of sound mix sound class overrides. Will override any sound class effects for any sound mixes */
	TMap<USoundMix*, FSoundMixClassOverrideMap> SoundMixClassEffectOverrides;

protected:
	/** Interface to audio effects processing */
	FAudioEffectsManager* Effects;

private:
	UReverbEffect* CurrentReverbEffect;

	/** A volume headroom to apply to specific platforms to achieve better platform consistency. */
	float PlatformAudioHeadroom;

	/** Reverb Effects activated without volumes - Game Thread owned */
	TMap<FName, FActivatedReverb> ActivatedReverbs;

	/** The activated reverb that currently has the highest priority - Audio Thread owned */
	FActivatedReverb HighestPriorityActivatedReverb;

	/** Gamethread representation of whether HRTF is enabled for all 3d sounds. (not bitpacked to avoid thread issues) */
	bool bHRTFEnabledForAll_OnGameThread:1;

	uint8 bGameWasTicking:1;

public:
	/* HACK: Temporarily disable audio caching.  This will be done better by changing the decompression pool size in the future */
	uint8 bDisableAudioCaching:1;

private:
	/* True once the startup sounds have been precached */
	uint8 bStartupSoundsPreCached:1;

	/** Whether or not the spatialization plugin is enabled. */
	uint8 bSpatializationExtensionEnabled:1;

	/** Whether HRTF is enabled for all 3d sounds. */
	uint8 bHRTFEnabledForAll:1;

	/** Whether the audio device is active (current audio device in-focus in PIE) */
	uint8 bIsDeviceMuted:1;

	/** Whether the audio device has been initialized */
	uint8 bIsInitialized:1;

	/** Whether the value in HighestPriorityActivatedReverb should be used - Audio Thread owned */
	uint8 bHasActivatedReverb:1;

#if !UE_BUILD_SHIPPING
	uint8 RequestedAudioStats;

	FAudioStats AudioStats;
#endif

	TArray<FActiveSound*> ActiveSounds;
	TArray<FWaveInstance*> ActiveWaveInstances;

	/** Cached copy of sound class adjusters array. Cached to avoid allocating every frame. */
	TArray<FSoundClassAdjuster> SoundClassAdjustersCopy;

	/** Set of sounds which will be stopped next audio frame update */
	TSet<FActiveSound*> PendingSoundsToStop;

	/** A set of sounds which need to be deleted but weren't able to be deleted due to pending async operations */
	TArray<FActiveSound*> PendingSoundsToDelete;

	TMap<uint64, FActiveSound*> AudioComponentIDToActiveSoundMap;

	TMap<uint32, FAudioVolumeProxy> AudioVolumeProxies;

	TMap<uint32, TPair<FReverbSettings,FInteriorSettings>> WorldIDToDefaultAudioVolumeSettingsMap;

	/** List of passive SoundMixes active last frame */
	TArray<USoundMix*> PrevPassiveSoundMixModifiers;

	friend class FSoundConcurrencyManager;
	FSoundConcurrencyManager ConcurrencyManager;

	/** Inverse listener transformation, used for spatialization */
	FMatrix InverseListenerTransform;
};


/**
 * Interface for audio device modules
 */

/** Defines the interface of a module implementing an audio device and associated classes. */
class IAudioDeviceModule : public IModuleInterface
{
public:

	/** Creates a new instance of the audio device implemented by the module. */
	virtual FAudioDevice* CreateAudioDevice() = 0;
};


