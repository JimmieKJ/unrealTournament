// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneTracksPrivatePCH.h"
#include "MovieSceneFadeSection.h"


/* UMovieSceneFadeSection structors
 *****************************************************************************/

UMovieSceneFadeSection::UMovieSceneFadeSection()
	: UMovieSceneFloatSection()
	, FadeColor(FLinearColor::Black)
	, bFadeAudio(false)
{
	SetIsInfinite(true);
}
