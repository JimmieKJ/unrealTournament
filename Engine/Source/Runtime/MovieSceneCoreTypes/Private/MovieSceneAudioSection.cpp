// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneCoreTypesPCH.h"
#include "MovieSceneAudioSection.h"
#include "SoundDefinitions.h"

UMovieSceneAudioSection::UMovieSceneAudioSection( const FObjectInitializer& ObjectInitializer )
	: Super( ObjectInitializer )
{

	Sound = NULL;
	AudioStartTime = 0.f;
	AudioDilationFactor = 1.f;
}

TRange<float> UMovieSceneAudioSection::GetAudioRange() const
{
	return !Sound ? TRange<float>::Empty() :
		TRange<float>(FMath::Max(GetStartTime(), AudioStartTime),
		FMath::Min(AudioStartTime + Sound->GetDuration() * AudioDilationFactor, GetEndTime()));
}

TRange<float> UMovieSceneAudioSection::GetAudioTrueRange() const
{
	return !Sound ? TRange<float>::Empty() : TRange<float>(AudioStartTime, AudioStartTime + Sound->GetDuration() * AudioDilationFactor);
}

void UMovieSceneAudioSection::MoveSection( float DeltaTime )
{
	Super::MoveSection(DeltaTime);

	AudioStartTime += DeltaTime;
}

void UMovieSceneAudioSection::DilateSection( float DilationFactor, float Origin )
{
	AudioStartTime = (AudioStartTime - Origin) * DilationFactor + Origin;
	AudioDilationFactor *= DilationFactor;

	Super::DilateSection(DilationFactor, Origin);
}

void UMovieSceneAudioSection::GetSnapTimes(TArray<float>& OutSnapTimes, bool bGetSectionBorders) const
{
	Super::GetSnapTimes(OutSnapTimes, bGetSectionBorders);
	if (AudioStartTime > GetStartTime())
	{
		OutSnapTimes.Add(AudioStartTime);
	}
	float AudioEndTime = GetAudioTrueRange().GetUpperBoundValue();
	if (AudioEndTime < GetEndTime())
	{
		OutSnapTimes.Add(AudioEndTime);
	}
	// @todo Sequencer handle snapping for time dilation
	// @todo Don't add redundant times (can't use AddUnique due to floating point equality issues)
}
