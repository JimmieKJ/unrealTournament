// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovieSceneSection.h"
#include "MovieSceneSkeletalAnimationSection.generated.h"

class UAnimSequenceBase;

/**
 * Movie scene section that control skeletal animation
 */
UCLASS( MinimalAPI )
class UMovieSceneSkeletalAnimationSection
	: public UMovieSceneSection
{
	GENERATED_UCLASS_BODY()

public:

	/** Sets the animation sequence for this section */
	void SetAnimSequence(UAnimSequenceBase* InAnimation) { Animation = InAnimation; }

	/** Gets the animation sequence for this section */
	UAnimSequenceBase* GetAnimSequence() { return Animation; }
	
	/** Gets the start offset into the animation clip */
	float GetStartOffset() const { return StartOffset; }

	/** Sets the start offset into the animation clip */
	void SetStartOffset(float InStartOffset) { StartOffset = InStartOffset; }
	
	/** Gets the end offset into the animation clip */
	float GetEndOffset() const { return EndOffset; }

	/** Sets the end offset into the animation clip */
	void SetEndOffset(float InEndOffset) { EndOffset = InEndOffset; }
	
	/** Gets the animation duration, modified by play rate */
	float GetDuration() const { return FMath::IsNearlyZero(PlayRate) || Animation == nullptr ? 0.f : Animation->SequenceLength / PlayRate; }

	/** Gets the animation sequence length, not modified by play rate */
	float GetSequenceLength() const { return Animation != nullptr ? Animation->SequenceLength : 0.f; }

	/** Sets the play rate of the animation clip */
	float GetPlayRate() const { return PlayRate; }

	/** Sets the play rate of the animation clip */
	void SetPlayRate(float InPlayRate) { PlayRate = InPlayRate; }

	/** Gets whether the playback is reversed */
	bool GetReverse() const { return bReverse; }

	/** Sets whether the playback is reversed */
	void SetReverse(bool bInReverse) { bReverse = bInReverse; }

	/** Gets the anim BP slot name. */
	FName GetSlotName() const { return SlotName; }

	/** Sets the anim BP slot name. */
	void SetSlotName( FName InSlotName ) { SlotName = InSlotName; }

public:

	//~ MovieSceneSection interface

	virtual void MoveSection( float DeltaPosition, TSet<FKeyHandle>& KeyHandles ) override;
	virtual void DilateSection( float DilationFactor, float Origin, TSet<FKeyHandle>& KeyHandles  ) override;
	virtual UMovieSceneSection* SplitSection(float SplitTime) override;
	virtual void GetKeyHandles(TSet<FKeyHandle>& OutKeyHandles, TRange<float> TimeRange) const override;
	virtual void GetSnapTimes(TArray<float>& OutSnapTimes, bool bGetSectionBorders) const override;
	virtual TOptional<float> GetKeyTime( FKeyHandle KeyHandle ) const override { return TOptional<float>(); }
	virtual void SetKeyTime( FKeyHandle KeyHandle, float Time ) override { }

	/** ~UObject interface */
	virtual void PostLoad() override;
	
private:

	//~ UObject interface

#if WITH_EDITOR

	virtual void PreEditChange(UProperty* PropertyAboutToChange) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	float PreviousPlayRate;

#endif

private:

	static FName DefaultSlotName;

	UPROPERTY()
	class UAnimSequence* AnimSequence_DEPRECATED;

	/** The animation this section plays */
	UPROPERTY(EditAnywhere, Category="Animation")
	UAnimSequenceBase* Animation;

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

	/** The slot name to use for the animation */
	UPROPERTY( EditAnywhere, Category = "Animation" )
	FName SlotName;
};
