// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovieSceneSection.h"
#include "MovieSceneSkeletalAnimationSection.generated.h"

/**
 * Audio section, for use in the master audio, or by attached audio objects
 */
UCLASS( MinimalAPI )
class UMovieSceneSkeletalAnimationSection : public UMovieSceneSection
{
	GENERATED_UCLASS_BODY()
public:
	/** Sets the animation sequence for this section */
	void SetAnimSequence(class UAnimSequence* InAnimSequence) { AnimSequence = InAnimSequence; }
	
	/** Gets the animation sequence for this section */
	class UAnimSequence* GetAnimSequence() { return AnimSequence; }
	
	/** Gets the start offset into the animation clip */
	float GetStartOffset() const { return StartOffset; }

	/** Sets the start offset into the animation clip */
	void SetStartOffset(float InStartOffset) { StartOffset = InStartOffset; }
	
	/** Gets the end offset into the animation clip */
	float GetEndOffset() const { return EndOffset; }

	/** Sets the end offset into the animation clip */
	void SetEndOffset(float InEndOffset) { EndOffset = InEndOffset; }
	
	/** Gets the animation duration, modified by play rate */
	float GetDuration() const { return FMath::IsNearlyZero(PlayRate) || AnimSequence == nullptr ? 0.f : AnimSequence->SequenceLength / PlayRate; }

	/** Gets the animation sequence length, not modified by play rate */
	float GetSequenceLength() const { return AnimSequence != nullptr ? AnimSequence->SequenceLength : 0.f; }

	/** Sets the play rate of the animation clip */
	float GetPlayRate() const { return PlayRate; }

	/** Sets the play rate of the animation clip */
	void SetPlayRate(float InPlayRate) { PlayRate = InPlayRate; }

	/** Gets whether the playback is reversed */
	bool GetReverse() const { return bReverse; }

	/** Sets whether the playback is reversed */
	void SetReverse(bool bInReverse) { bReverse = bInReverse; }

	/** MovieSceneSection interface */
	virtual void MoveSection( float DeltaPosition, TSet<FKeyHandle>& KeyHandles ) override;
	virtual void DilateSection( float DilationFactor, float Origin, TSet<FKeyHandle>& KeyHandles  ) override;
	virtual UMovieSceneSection* SplitSection(float SplitTime) override;
	virtual void GetKeyHandles(TSet<FKeyHandle>& KeyHandles) const override;
	virtual void GetSnapTimes(TArray<float>& OutSnapTimes, bool bGetSectionBorders) const override;

private:

	// UObject interface
#if WITH_EDITOR
	virtual void PreEditChange(UProperty* PropertyAboutToChange) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	float PreviousPlayRate;
#endif

	/** The animation sequence this section has */
	UPROPERTY(EditAnywhere, Category="Animation")
	class UAnimSequence* AnimSequence;

	/** The offset into the beginning of the animation clip */
	UPROPERTY(EditAnywhere, Category="Animation")
	float StartOffset;
	
	/** The offset into the end of the animation clip */
	UPROPERTY(EditAnywhere, Category="Animation")
	float EndOffset;
	
	/** The playback rate of the animation clip */
	UPROPERTY(EditAnywhere, Category="Animation")
	float PlayRate;

	/** Reverse the playback of the animation clip */
	UPROPERTY(EditAnywhere, Category="Animation")
	uint32 bReverse:1;
};
