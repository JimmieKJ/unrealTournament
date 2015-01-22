// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovieSceneSection.h"
#include "MovieSceneAnimationSection.generated.h"

/**
 * Audio section, for use in the master audio, or by attached audio objects
 */
UCLASS( MinimalAPI )
class UMovieSceneAnimationSection : public UMovieSceneSection
{
	GENERATED_UCLASS_BODY()
public:
	/** Sets the animation sequence for this section */
	void SetAnimSequence(class UAnimSequence* InAnimSequence) {AnimSequence = InAnimSequence;}
	
	/** Gets the animation sequence for this section */
	class UAnimSequence* GetAnimSequence() {return AnimSequence;}
	
	/** Sets the time that the animation is supposed to be played at */
	void SetAnimationStartTime(float InAnimationStartTime) {AnimationStartTime = InAnimationStartTime;}
	
	/** Gets the (absolute) time that the animation is supposed to be played at */
	float GetAnimationStartTime() const {return AnimationStartTime;}
	
	/** Gets the animation duration, modified by dilation */
	float GetAnimationDuration() const {return AnimSequence->SequenceLength * AnimationDilationFactor;}

	/**
	 * @return The dilation multiplier of the animation
	 */
	float GetAnimationDilationFactor() const {return AnimationDilationFactor;}

	/** MovieSceneSection interface */
	virtual void MoveSection( float DeltaPosition ) override;
	virtual void DilateSection( float DilationFactor, float Origin ) override;
	virtual void GetSnapTimes(TArray<float>& OutSnapTimes, bool bGetSectionBorders) const override;

private:
	/** The animation sequence this section has */
	UPROPERTY()
	class UAnimSequence* AnimSequence;

	/** The absolute time that the animation starts playing at */
	UPROPERTY()
	float AnimationStartTime;
	
	/** The amount which this animation is time dilated by */
	UPROPERTY()
	float AnimationDilationFactor;
};
