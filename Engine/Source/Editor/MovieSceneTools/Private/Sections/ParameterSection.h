// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PropertySection.h"


class UMovieSceneSection;


/**
 * A movie scene section for material parameters.
 */
class FParameterSection
	: public FPropertySection
{
public:

	FParameterSection(UMovieSceneSection& InSectionObject, const FText& SectionName)
		: FPropertySection(InSectionObject, SectionName)
	{ }

public:

	//~ FPropertySection interface

	virtual void SetIntermediateValue(FPropertyChangedParams PropertyChangedParams) override { }
	virtual void ClearIntermediateValue() override { }

public:

	//~ ISequencerSection interface

	virtual void GenerateSectionLayout(class ISectionLayoutBuilder& LayoutBuilder) const override;
	virtual bool RequestDeleteCategory(const TArray<FName>& CategoryNamePath) override;
	virtual bool RequestDeleteKeyArea(const TArray<FName>& KeyAreaNamePath) override;
};