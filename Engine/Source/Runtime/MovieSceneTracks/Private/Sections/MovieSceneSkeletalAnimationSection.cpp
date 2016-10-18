// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneTracksPrivatePCH.h"
#include "MovieSceneSkeletalAnimationSection.h"
#include "Animation/AnimSequence.h"

FName UMovieSceneSkeletalAnimationSection::DefaultSlotName( "DefaultSlot" );

UMovieSceneSkeletalAnimationSection::UMovieSceneSkeletalAnimationSection( const FObjectInitializer& ObjectInitializer )
	: Super( ObjectInitializer )
{
	AnimSequence_DEPRECATED = nullptr;
	Animation = nullptr;
	StartOffset = 0.f;
	EndOffset = 0.f;
	PlayRate = 1.f;
#if WITH_EDITOR
	PreviousPlayRate = PlayRate;
#endif
	bReverse = false;
	SlotName = DefaultSlotName;
}

void UMovieSceneSkeletalAnimationSection::PostLoad()
{
	if (AnimSequence_DEPRECATED)
	{
		Animation = AnimSequence_DEPRECATED;
	}

	Super::PostLoad();
}

void UMovieSceneSkeletalAnimationSection::MoveSection( float DeltaTime, TSet<FKeyHandle>& KeyHandles )
{
	Super::MoveSection(DeltaTime, KeyHandles);
}


void UMovieSceneSkeletalAnimationSection::DilateSection( float DilationFactor, float Origin, TSet<FKeyHandle>& KeyHandles )
{
	PlayRate /= DilationFactor;

	Super::DilateSection(DilationFactor, Origin, KeyHandles);
}

UMovieSceneSection* UMovieSceneSkeletalAnimationSection::SplitSection(float SplitTime)
{
	float AnimPlayRate = FMath::IsNearlyZero(GetPlayRate()) ? 1.0f : GetPlayRate();
	float AnimPosition = (SplitTime - GetStartTime()) * AnimPlayRate;
	float SeqLength = GetSequenceLength() - (GetStartOffset() + GetEndOffset());

	float NewOffset = FMath::Fmod(AnimPosition, SeqLength);
	NewOffset += GetStartOffset();

	UMovieSceneSection* NewSection = Super::SplitSection(SplitTime);
	if (NewSection != nullptr)
	{
		UMovieSceneSkeletalAnimationSection* NewSkeletalSection = Cast<UMovieSceneSkeletalAnimationSection>(NewSection);
		NewSkeletalSection->SetStartOffset(NewOffset);
	}
	return NewSection;
}


void UMovieSceneSkeletalAnimationSection::GetKeyHandles(TSet<FKeyHandle>& KeyHandles, TRange<float> TimeRange) const
{
	// do nothing
}


void UMovieSceneSkeletalAnimationSection::GetSnapTimes(TArray<float>& OutSnapTimes, bool bGetSectionBorders) const
{
	Super::GetSnapTimes(OutSnapTimes, bGetSectionBorders);

	float CurrentTime = GetStartTime();
	float AnimPlayRate = FMath::IsNearlyZero(GetPlayRate()) ? 1.0f : GetPlayRate();
	float SeqLength = (GetSequenceLength() - (GetStartOffset() + GetEndOffset())) / AnimPlayRate;

	// Snap to the repeat times
	while (CurrentTime <= GetEndTime() && !FMath::IsNearlyZero(GetDuration()) && SeqLength > 0)
	{
		if (CurrentTime >= GetStartTime())
		{
			OutSnapTimes.Add(CurrentTime);
		}

		CurrentTime += SeqLength;
	}
}

#if WITH_EDITOR
void UMovieSceneSkeletalAnimationSection::PreEditChange(UProperty* PropertyAboutToChange)
{
	// Store the current play rate so that we can compute the amount to compensate the section end time when the play rate changes
	PreviousPlayRate = GetPlayRate();

	Super::PreEditChange(PropertyAboutToChange);
}

void UMovieSceneSkeletalAnimationSection::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	// Adjust the duration automatically if the play rate changes
	if (PropertyChangedEvent.Property != nullptr &&
		PropertyChangedEvent.Property->GetFName() == TEXT("PlayRate"))
	{
		float NewPlayRate = GetPlayRate();

		if (!FMath::IsNearlyZero(NewPlayRate))
		{
			float CurrentDuration = GetEndTime() - GetStartTime();
			float NewDuration = CurrentDuration * (PreviousPlayRate / NewPlayRate);
			float NewEndTime = GetStartTime() + NewDuration;
			SetEndTime(NewEndTime);

			PreviousPlayRate = NewPlayRate;
		}
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif
