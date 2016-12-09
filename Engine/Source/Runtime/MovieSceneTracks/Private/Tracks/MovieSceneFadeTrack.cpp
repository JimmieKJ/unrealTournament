// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "Tracks/MovieSceneFadeTrack.h"
#include "Sections/MovieSceneFadeSection.h"
#include "Evaluation/MovieSceneFadeTemplate.h"
#include "Evaluation/MovieSceneEvaluationTrack.h"
#include "Templates/Casts.h"

#define LOCTEXT_NAMESPACE "MovieSceneFadeTrack"


/* UMovieSceneFadeTrack interface
 *****************************************************************************/
UMovieSceneFadeTrack::UMovieSceneFadeTrack(const FObjectInitializer& Init)
	: Super(Init)
{
	EvalOptions.bCanEvaluateNearestSection = true;
	EvalOptions.bEvaluateNearestSection = false;
}

UMovieSceneSection* UMovieSceneFadeTrack::CreateNewSection()
{
	return NewObject<UMovieSceneSection>(this, UMovieSceneFadeSection::StaticClass(), NAME_None, RF_Transactional);
}

FMovieSceneEvalTemplatePtr UMovieSceneFadeTrack::CreateTemplateForSection(const UMovieSceneSection& InSection) const
{
	return FMovieSceneFadeSectionTemplate(*CastChecked<UMovieSceneFadeSection>(&InSection));
}

#if WITH_EDITORONLY_DATA
FText UMovieSceneFadeTrack::GetDefaultDisplayName() const
{
	return LOCTEXT("TrackName", "Fade");
}
#endif


#undef LOCTEXT_NAMESPACE
