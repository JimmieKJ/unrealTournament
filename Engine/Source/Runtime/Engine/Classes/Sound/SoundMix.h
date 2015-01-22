// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Audio.h"
#include "SoundMix.generated.h"

USTRUCT()
struct FAudioEQEffect
{
	GENERATED_USTRUCT_BODY()

	/* Start time of effect */
	double RootTime;

	/* High frequency filter cutoff frequency (Hz) */
	UPROPERTY(EditAnywhere, Category=HighPass )
	float HFFrequency;

	/* High frequency gain - 0.0 is silent, 1.0 is full volume. */
	UPROPERTY(EditAnywhere, Category=HighPass )
	float HFGain;

	/* Middle (band) frequency filter cutoff frequency (Hz). */
	UPROPERTY(EditAnywhere, Category=BandPass )
	float MFCutoffFrequency;

	/* Middle (band) frequency filter bandwidth frequency (Hz) - Range (0.1 to 2.0). */
	UPROPERTY(EditAnywhere, Category=BandPass )
	float MFBandwidth;

	/* Middle (band) frequency filter gain - 0.0 is silent, 1.0 is full volume. */
	UPROPERTY(EditAnywhere, Category=BandPass )
	float MFGain;

	/* Low frequency filter cutoff frequency (Hz) */
	UPROPERTY(EditAnywhere, Category=LowPass )
	float LFFrequency;

	/* Low frequency filter gain - 0.0 is silent, 1.0 is full volume. */
	UPROPERTY(EditAnywhere, Category=LowPass )
	float LFGain;



		// Cannot use strcutdefaultproperties here as this class is a member of a native class
		FAudioEQEffect()
		:	RootTime( 0.0 )
		,	HFFrequency( DEFAULT_HIGH_FREQUENCY )
		,	HFGain( 1.0f )
		,	MFCutoffFrequency( DEFAULT_MID_FREQUENCY )
		,	MFBandwidth( 1.0f )
		,	MFGain( 1.0f )
		,	LFFrequency( DEFAULT_LOW_FREQUENCY )
		,	LFGain( 1.0f )
		{
		}

		/** 
		 * Interpolate EQ settings based on time
		 */
		void Interpolate( float InterpValue, const FAudioEQEffect& Start, const FAudioEQEffect& End );
		
		/** 
		 * Validate all settings are in range
		 */
		void ClampValues( void );
	
};

/**
 * Elements of data for sound group volume control
 */
USTRUCT()
struct FSoundClassAdjuster
{
	GENERATED_USTRUCT_BODY()

	/* The sound class this adjuster affects. */
	UPROPERTY(EditAnywhere, Category=SoundClassAdjuster, DisplayName = "Sound Class" )
	USoundClass* SoundClassObject;

	/* A multiplier applied to the volume. */
	UPROPERTY(EditAnywhere, Category=SoundClassAdjuster )
	float VolumeAdjuster;

	/* A multiplier applied to the pitch. */
	UPROPERTY(EditAnywhere, Category=SoundClassAdjuster )
	float PitchAdjuster;

	/* Set to true to apply this adjuster to all children of the sound class. */
	UPROPERTY(EditAnywhere, Category=SoundClassAdjuster )
	uint32 bApplyToChildren:1;

	/* A multiplier applied to VoiceCenterChannelVolume. */
	UPROPERTY(EditAnywhere, Category=SoundClassAdjuster )
	float VoiceCenterChannelVolumeAdjuster;



		FSoundClassAdjuster()
		: SoundClassObject(NULL)
		, VolumeAdjuster(1)
		, PitchAdjuster(1)
		, bApplyToChildren(false)
		, VoiceCenterChannelVolumeAdjuster(1)
		{
		}
	
};

UCLASS(BlueprintType, hidecategories=object, MinimalAPI)
class USoundMix : public UObject
{
	GENERATED_UCLASS_BODY()

	/** Whether to apply the EQ effect */
	UPROPERTY(EditAnywhere, Category=EQ, AssetRegistrySearchable )
	uint32 bApplyEQ:1;

	UPROPERTY(EditAnywhere, Category=EQ)
	float EQPriority;

	UPROPERTY(EditAnywhere, Category=EQ)
	struct FAudioEQEffect EQSettings;

	/* Array of changes to be applied to groups. */
	UPROPERTY(EditAnywhere, Category=SoundClasses)
	TArray<struct FSoundClassAdjuster> SoundClassEffects;

	/* Initial delay in seconds before the the mix is applied. */
	UPROPERTY(EditAnywhere, Category=SoundMix )
	float InitialDelay;

	/* Time taken in seconds for the mix to fade in. */
	UPROPERTY(EditAnywhere, Category=SoundMix )
	float FadeInTime;

	/* Duration of mix, negative means it will be applied until another mix is set. */
	UPROPERTY(EditAnywhere, Category=SoundMix )
	float Duration;

	/* Time taken in seconds for the mix to fade out. */
	UPROPERTY(EditAnywhere, Category=SoundMix )
	float FadeOutTime;

#if WITH_EDITOR
	bool CausesPassiveDependencyLoop(TArray<USoundClass*>& ProblemClasses) const;
#endif // WITH_EDITOR

protected:
	// Begin UObject interface.
	virtual FString GetDesc( void ) override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR
	virtual void BeginDestroy() override;
	// End UObject interface.

#if WITH_EDITOR
	bool CheckForDependencyLoop(USoundClass* SoundClass, TArray<USoundClass*>& ProblemClasses, bool CheckChildren) const;
#endif // WITH_EDITOR
};



