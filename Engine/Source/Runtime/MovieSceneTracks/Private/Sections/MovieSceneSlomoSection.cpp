// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "Sections/MovieSceneSlomoSection.h"


/* UMovieSceneSlomoSection structors
 *****************************************************************************/

UMovieSceneSlomoSection::UMovieSceneSlomoSection()
	: UMovieSceneFloatSection()
{
	SetIsInfinite(true);
	GetFloatCurve().SetDefaultValue(1.0f);

	EvalOptions.EnableAndSetCompletionMode(EMovieSceneCompletionMode::RestoreState);
}
