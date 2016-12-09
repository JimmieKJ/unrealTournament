// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "Sections/MovieSceneAudioSection.h"
#include "Sound/SoundBase.h"
#include "Evaluation/MovieSceneAudioTemplate.h"

UMovieSceneAudioSection::UMovieSceneAudioSection( const FObjectInitializer& ObjectInitializer )
	: Super( ObjectInitializer )
{

	Sound = nullptr;
	AudioStartTime = 0.f;
	AudioDilationFactor = 1.f;
	AudioVolume = 1.f;
#if WITH_EDITORONLY_DATA
	bShowIntensity = false;
#endif
	bSuppressSubtitles = false;

	EvalOptions.EnableAndSetCompletionMode(EMovieSceneCompletionMode::RestoreState);
}

FMovieSceneEvalTemplatePtr UMovieSceneAudioSection::GenerateTemplate() const
{
	return FMovieSceneAudioSectionTemplate(*this);
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


void UMovieSceneAudioSection::MoveSection( float DeltaTime, TSet<FKeyHandle>& KeyHandles )
{
	Super::MoveSection(DeltaTime, KeyHandles);

	AudioStartTime += DeltaTime;
}


void UMovieSceneAudioSection::DilateSection( float DilationFactor, float Origin, TSet<FKeyHandle>& KeyHandles )
{
	AudioStartTime = (AudioStartTime - Origin) * DilationFactor + Origin;
	AudioDilationFactor *= DilationFactor;

	Super::DilateSection(DilationFactor, Origin, KeyHandles);
}


void UMovieSceneAudioSection::GetKeyHandles(TSet<FKeyHandle>& KeyHandles, TRange<float> TimeRange) const
{
	// do nothing
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
