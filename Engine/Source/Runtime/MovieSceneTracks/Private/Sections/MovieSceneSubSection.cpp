// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneTracksPrivatePCH.h"
#include "IMovieScenePlayer.h"
#include "MovieSceneSequence.h"
#include "MovieSceneSubSection.h"


/* UMovieSceneSubSection structors
 *****************************************************************************/

UMovieSceneSubSection::UMovieSceneSubSection()
	: StartOffset(0.0f)
	, TimeScale(1.0f)
{ }
