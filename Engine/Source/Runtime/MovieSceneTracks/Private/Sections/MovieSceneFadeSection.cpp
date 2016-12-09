// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "Sections/MovieSceneFadeSection.h"


/* UMovieSceneFadeSection structors
 *****************************************************************************/

UMovieSceneFadeSection::UMovieSceneFadeSection()
	: UMovieSceneFloatSection()
	, FadeColor(FLinearColor::Black)
	, bFadeAudio(false)
{
	SetIsInfinite(true);

	EvalOptions.EnableAndSetCompletionMode(EMovieSceneCompletionMode::RestoreState);
}
