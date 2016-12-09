// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "Sections/MovieSceneCameraCutSection.h"

#include "Evaluation/MovieSceneCameraCutTemplate.h"

/* UMovieSceneCameraCutSection interface
 *****************************************************************************/

FMovieSceneEvalTemplatePtr UMovieSceneCameraCutSection::GenerateTemplate() const
{
	return FMovieSceneCameraCutSectionTemplate(*this);
}
