// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UMovieSceneSequence;
struct FMovieSceneEvaluationTemplate;

struct FMovieSceneSequenceTemplateStore
{
	FMovieSceneSequenceTemplateStore()
#if WITH_EDITORONLY_DATA
		: bTemplatesAreVolatile(true)
#else
		// In cooked builds we assume the evaluation template can never change, and is thus always up-to-date
		: bTemplatesAreVolatile(false)
#endif
	{
	}

	virtual ~FMovieSceneSequenceTemplateStore() {}
	
	bool AreTemplatesVolatile() const
	{
		return bTemplatesAreVolatile;
	}

	MOVIESCENE_API virtual FMovieSceneEvaluationTemplate& GetCompiledTemplate(UMovieSceneSequence& Sequence);

protected:

	/** True where templates are liable to change, and we should listen for changes */
	bool bTemplatesAreVolatile;
};
