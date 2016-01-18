// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneTracksPrivatePCH.h"
#include "MovieSceneSlomoSection.h"


/* UMovieSceneSlomoSection structors
 *****************************************************************************/

UMovieSceneSlomoSection::UMovieSceneSlomoSection()
	: UMovieSceneFloatSection()
{
	SetIsInfinite(true);
	GetFloatCurve().SetDefaultValue(1.0f);
}
