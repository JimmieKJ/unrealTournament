// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

/** 
 * The base class for a playable sound object 
 */

#include "Audio.h"
#include "Sound/SoundConcurrency.h"
#include "SoundBase.generated.h"


struct FActiveSound;
struct FSoundParseParameters;
struct FWaveInstance;
struct FAttenuationSettings; 
struct FSoundConcurrencySettings;

class USoundClass;
class USoundAttenuation;
class USoundConcurrency;

UCLASS(config=Engine, hidecategories=Object, abstract, editinlinenew, BlueprintType)
class ENGINE_API USoundBase : public UObject
{
	GENERATED_UCLASS_BODY()

private:
	static USoundClass* DefaultSoundClassObject;
	static USoundConcurrency* DefaultSoundConcurrencyObject;

protected:
	/** Sound class this sound belongs to */
	UPROPERTY(EditAnywhere, Category=Sound, meta=(DisplayName = "Sound Class"))
	USoundClass* SoundClassObject;

public:
	/** When "stat sounds -debug" has been specified, draw this sound's attenuation shape when the sound is audible. For debugging purpose only. */
	UPROPERTY(EditAnywhere, Category = Debug)
	uint32 bDebug:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Concurrency)
	uint32 bOverrideConcurrency:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Attenuation)
	uint32 bIgnoreFocus:1;

	/** If bOverridePlayback is false, the sound concurrency settings to use for this sound. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Concurrency, meta = (EditCondition = "!bOverrideConcurrency"))
	class USoundConcurrency* SoundConcurrencySettings;

	/** If bOverridePlayback is true, concurrency settings to use. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Concurrency, meta = (EditCondition = "bOverrideConcurrency"))
	struct FSoundConcurrencySettings ConcurrencyOverrides;

	UPROPERTY()
	TEnumAsByte<enum EMaxConcurrentResolutionRule::Type> MaxConcurrentResolutionRule_DEPRECATED;

	/** Maximum number of times this sound can be played concurrently. */
	UPROPERTY()
	int32 MaxConcurrentPlayCount_DEPRECATED;

	/** Duration of sound in seconds. */
	UPROPERTY(Category=Info, AssetRegistrySearchable, VisibleAnywhere, BlueprintReadOnly)
	float Duration;

	/** Attenuation settings package for the sound */
	UPROPERTY(EditAnywhere, Category=Attenuation)
	USoundAttenuation* AttenuationSettings;

	/** Sound priority (higher value is higher priority) used for concurrency resolution. This priority value is weighted against the final volume of the sound. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Concurrency, meta = (ClampMin = "0.0", UIMin = "0.0", ClampMax = "100.0", UIMax = "100.0") )
	float Priority;

public:	
	/** Number of times this cue is currently being played. */
	int32 CurrentPlayCount;

	//~ Begin UObject Interface.
	virtual void PostInitProperties() override;
	virtual void PostLoad() override;
	//~ End UObject interface.
	
	/** Returns whether the sound base is set up in a playable manner */
	virtual bool IsPlayable() const;

	/** Returns a pointer to the attenuation settings that are to be applied for this node */
	virtual const FAttenuationSettings* GetAttenuationSettingsToApply() const;

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
	  * Returns the subtitle priority
	  */
	virtual float GetSubtitlePriority() const { return DEFAULT_SUBTITLE_PRIORITY; };
	
	/** Returns whether or not any part of this sound wants interior volumes applied to it */
	virtual bool ShouldApplyInteriorVolumes() const;

	/** Returns whether or not this sound is looping. */
	bool IsLooping();

	/** 
	 * Parses the Sound to generate the WaveInstances to play
	 */
	virtual void Parse( class FAudioDevice* AudioDevice, const UPTRINT NodeWaveInstanceHash, FActiveSound& ActiveSound, const FSoundParseParameters& ParseParams, TArray<FWaveInstance*>& WaveInstances ) { }

	/**
	 * Returns the SoundClass used for this sound
	 */
	virtual USoundClass* GetSoundClass() const;

	/** Returns the FSoundConcurrencySettings struct to use */
	const FSoundConcurrencySettings* GetSoundConcurrencySettingsToApply();

	/** Returns the priority to use when evaluating concurrency */
	float GetPriority() const;

	/** Returns the sound concurrency object ID if it exists. If it doesn't exist, returns 0. */
	uint32 GetSoundConcurrencyObjectID() const;

};

