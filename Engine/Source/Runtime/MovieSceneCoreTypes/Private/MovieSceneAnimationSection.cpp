// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneCoreTypesPCH.h"
#include "MovieSceneAnimationSection.h"

UMovieSceneAnimationSection::UMovieSceneAnimationSection( const FObjectInitializer& ObjectInitializer )
	: Super( ObjectInitializer )
{
	AnimSequence = NULL;
	AnimationStartTime = 0.f;
	AnimationDilationFactor = 1.f;
}

void UMovieSceneAnimationSection::MoveSection( float DeltaTime )
{
	Super::MoveSection(DeltaTime);

	AnimationStartTime += DeltaTime;
}

void UMovieSceneAnimationSection::DilateSection( float DilationFactor, float Origin )
{
	AnimationStartTime = (AnimationStartTime - Origin) * DilationFactor + Origin;
	AnimationDilationFactor *= DilationFactor;

	Super::DilateSection(DilationFactor, Origin);
}

void UMovieSceneAnimationSection::GetSnapTimes(TArray<float>& OutSnapTimes, bool bGetSectionBorders) const
{
	Super::GetSnapTimes(OutSnapTimes, bGetSectionBorders);
	float CurrentTime = GetAnimationStartTime();
	while (CurrentTime <= GetEndTime())
	{
		if (CurrentTime >= GetStartTime())
		{
			OutSnapTimes.Add(CurrentTime);
		}
		CurrentTime += GetAnimationDuration();
	}
}
