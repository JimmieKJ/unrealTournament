// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovieSceneSection.h"
#include "MovieSceneAudioSection.generated.h"


/**
 * Audio section, for use in the master audio, or by attached audio objects
 */
UCLASS( MinimalAPI )
class UMovieSceneAudioSection
	: public UMovieSceneSection
{
	GENERATED_UCLASS_BODY()

public:

	/** Sets this section's sound */
	void SetSound(class USoundBase* InSound) {Sound = InSound;}

	/** Gets the sound for this section */
	class USoundBase* GetSound() {return Sound;}
	
	/** Sets the time that the sound is supposed to be played at */
	void SetAudioStartTime(float InAudioStartTime) {AudioStartTime = InAudioStartTime;}
	
	/** Gets the (absolute) time that the sound is supposed to be played at */
	float GetAudioStartTime() {return AudioStartTime;}
	
	/**
	 * @return The range of times that the sound plays, truncated by the section limits
	 */
	TRange<float> MOVIESCENETRACKS_API GetAudioRange() const;
	
	/**
	 * @return The range of times that the sound plays, not truncated by the section limits
	 */
	TRange<float> MOVIESCENETRACKS_API GetAudioTrueRange() const;
	
	/**
	 * @return The dilation multiplier of the sound
	 */
	float GetAudioDilationFactor() const {return AudioDilationFactor;}

	/**
	 * Returns whether or not a provided position in time is within the timespan of the audio range
	 *
	 * @param Position	The position to check
	 * @return true if the position is within the timespan, false otherwise
	 */
	bool IsTimeWithinAudioRange( float Position ) const 
	{
		TRange<float> AudioRange = GetAudioRange();
		return Position >= AudioRange.GetLowerBoundValue() && Position <= AudioRange.GetUpperBoundValue();
	}

public:

	// MovieSceneSection interface

	virtual void MoveSection( float DeltaPosition, TSet<FKeyHandle>& KeyHandles ) override;
	virtual void DilateSection( float DilationFactor, float Origin, TSet<FKeyHandle>& KeyHandles ) override;
	virtual void GetKeyHandles(TSet<FKeyHandle>& KeyHandles) const override;
	virtual void GetSnapTimes(TArray<float>& OutSnapTimes, bool bGetSectionBorders) const override;

private:

	/** The sound cue or wave that this section plays */
	UPROPERTY(EditAnywhere, Category="Audio")
	USoundBase* Sound;

	/** The absolute time that the sound starts playing at */
	UPROPERTY(EditAnywhere, Category="Audio")
	float AudioStartTime;
	
	/** The amount which this audio is time dilated by */
	UPROPERTY(EditAnywhere, Category="Audio")
	float AudioDilationFactor;
};
