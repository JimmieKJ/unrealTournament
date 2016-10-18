// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BoolPropertySection.h"


/**
 * An implementation of visibility property sections
 */
class FVisibilityPropertySection
	: public FBoolPropertySection
{
public:

	FVisibilityPropertySection(UMovieSceneSection& InSectionObject, const FText& SectionName, ISequencer* InSequencer)
		: FBoolPropertySection(InSectionObject, SectionName, InSequencer)
	{
		DisplayName = FText::FromString(TEXT("Visible"));
	}

	virtual void SetIntermediateValue(FPropertyChangedParams PropertyChangedParams) override;
};
