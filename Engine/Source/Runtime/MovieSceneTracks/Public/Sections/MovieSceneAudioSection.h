// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Curves/KeyHandle.h"
#include "MovieSceneSection.h"
#include "MovieSceneAudioSection.generated.h"

class USoundBase;

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
	class USoundBase* GetSound() const {return Sound;}
	
	/** Sets the time that the sound is supposed to be played at */
	void SetAudioStartTime(float InAudioStartTime) {AudioStartTime = InAudioStartTime;}
	
	/** Gets the (absolute) time that the sound is supposed to be played at */
	float GetAudioStartTime() const {return AudioStartTime;}
	
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
	* Sets the dilation multiplier of the sound
	*/
	void SetAudioDilationFactor( float InAudioDilationFactor ) { AudioDilationFactor = InAudioDilationFactor; }

	/**
	* @return The volume the sound will be played with.
	*/
	float GetAudioVolume() const { return AudioVolume; }

	/**
	* Sets the volume the sound will be played with.
	*/
	void SetAudioVolume( float InAudioVolume ) { AudioVolume = InAudioVolume; }

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

#if WITH_EDITORONLY_DATA
	/**
	 * @return Whether to show actual intensity on the waveform thumbnail or not
	 */
	bool ShouldShowIntensity() const
	{
		return bShowIntensity;
	}
#endif

	/**
	 * @return Whether subtitles should be suppressed
	 */
	bool GetSuppressSubtitles() const
	{
		return bSuppressSubtitles;
	}

public:

	// MovieSceneSection interface

	virtual void MoveSection( float DeltaPosition, TSet<FKeyHandle>& KeyHandles ) override;
	virtual void DilateSection( float DilationFactor, float Origin, TSet<FKeyHandle>& KeyHandles ) override;
	virtual void GetKeyHandles(TSet<FKeyHandle>& OutKeyHandles, TRange<float> TimeRange) const override;
	virtual void GetSnapTimes(TArray<float>& OutSnapTimes, bool bGetSectionBorders) const override;
	virtual TOptional<float> GetKeyTime( FKeyHandle KeyHandle ) const override { return TOptional<float>(); }
	virtual void SetKeyTime( FKeyHandle KeyHandle, float Time ) override { }
	virtual FMovieSceneEvalTemplatePtr GenerateTemplate() const override;

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

	/** The volume the sound will be played with. */
	UPROPERTY( EditAnywhere, Category = "Audio" )
	float AudioVolume;

#if WITH_EDITORONLY_DATA
	/** Whether to show the actual intensity of the wave on the thumbnail, as well as the smoothed RMS */
	UPROPERTY(EditAnywhere, Category="Audio")
	bool bShowIntensity;
#endif

	UPROPERTY(EditAnywhere, Category="Audio")
	bool bSuppressSubtitles;
};
