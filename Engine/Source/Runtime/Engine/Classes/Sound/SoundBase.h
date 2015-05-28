// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

/** 
 * The base class for a playable sound object 
 */

#include "SoundBase.generated.h"

struct FActiveSound;
struct FSoundParseParameters;
struct FWaveInstance;
struct FAttenuationSettings; 
class USoundClass;
class USoundAttenuation;


UENUM()
namespace EMaxConcurrentResolutionRule
{
	enum Type
	{
		PreventNew					UMETA(ToolTip = "When Max Concurrent sounds are active do not start a new sound."),
		StopOldest					UMETA(ToolTip = "When Max Concurrent sounds are active stop the oldest and start a new one."),
		StopFarthestThenPreventNew	UMETA(ToolTip = "When Max Concurrent sounds are active stop the furthest sound.  If all sounds are the same distance then do not start a new sound."),
		StopFarthestThenOldest		UMETA(ToolTip = "When Max Concurrent sounds are active stop the furthest sound.  If all sounds are the same distance then stop the oldest.")
	};
}

UCLASS(config=Engine, hidecategories=Object, abstract, editinlinenew, BlueprintType)
class ENGINE_API USoundBase : public UObject
{
	GENERATED_UCLASS_BODY()

protected:
	/** Sound class this sound belongs to */
	UPROPERTY(EditAnywhere, Category=Sound, meta=(DisplayName = "Sound Class"))
	USoundClass* SoundClassObject;

public:
	/** When "stat sounds -debug" has been specified, draw this sound's attenuation shape when the sound is audible. For debugging purpose only. */
	UPROPERTY(EditAnywhere, Category=Playback)
	uint32 bDebug:1;

	/** If we try to play a new version of this sound when at the max concurrent count how should it be resolved. */
	UPROPERTY(EditAnywhere, Category=Playback)
	TEnumAsByte<EMaxConcurrentResolutionRule::Type> MaxConcurrentResolutionRule;

	/** Maximum number of times this sound can be played concurrently. */
	UPROPERTY(EditAnywhere, Category=Playback)
	int32 MaxConcurrentPlayCount;

	/** Duration of sound in seconds. */
	UPROPERTY(Category=Info, AssetRegistrySearchable, VisibleAnywhere, BlueprintReadOnly)
	float Duration;

	/** Attenuation settings package for the sound */
	UPROPERTY(EditAnywhere, Category=Attenuation)
	USoundAttenuation* AttenuationSettings;

public:	
	/** Number of times this cue is currently being played. */
	int32 CurrentPlayCount;

	// Begin UObject interface.
	virtual void PostInitProperties() override;
	// End UObject interface.
	
	/** Returns whether the sound base is set up in a playable manner */
	virtual bool IsPlayable() const;

	/** Returns a pointer to the attenuation settings that are to be applied for this node */
	virtual const FAttenuationSettings* GetAttenuationSettingsToApply() const;

	/**
	 * Checks to see if a location is audible
	 */
	bool IsAudible( const FVector& SourceLocation, const FVector& ListenerLocation, AActor* SourceActor, bool& bIsOccluded, bool bCheckOcclusion );

	/** 
	 * Does a simple range check to all listeners to test hearability
	 *
	 * @param Location				Location to check against
	 * @param AttenuationSettings	Optional Attenuation override if not using settings from the sound
	 */
	bool IsAudibleSimple( class FAudioDevice* AudioDevice, const FVector Location, USoundAttenuation* AttenuationSettings = NULL );

	/** 
	 * Returns the farthest distance at which the sound could be heard
	 */
	virtual float GetMaxAudibleDistance();

	/** 
	 * Returns the length of the sound
	 */
	virtual float GetDuration();

	virtual float GetVolumeMultiplier();
	virtual float GetPitchMultiplier();

	/** 
	 * Parses the Sound to generate the WaveInstances to play
	 */
	virtual void Parse( class FAudioDevice* AudioDevice, const UPTRINT NodeWaveInstanceHash, FActiveSound& ActiveSound, const FSoundParseParameters& ParseParams, TArray<FWaveInstance*>& WaveInstances ) { }

	/**
	 * Returns the SoundClass used for this sound
	 */
	virtual USoundClass* GetSoundClass() const;
};

