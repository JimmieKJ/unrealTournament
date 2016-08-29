// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PropertySection.h"


class FFloatCurveKeyArea;
class UMovieSceneSection;


/**
 * An implementation of float property sections.
 */
class FFloatPropertySection
	: public FPropertySection
{
public:

	FFloatPropertySection(UMovieSceneSection& InSectionObject, const FText& SectionName)
		: FPropertySection(InSectionObject, SectionName)
	{ }

public:

	//~ FPropertySection interface 

	virtual void GenerateSectionLayout(class ISectionLayoutBuilder& LayoutBuilder) const override;
	virtual void SetIntermediateValue(FPropertyChangedParams PropertyChangedParams) override;
	virtual void ClearIntermediateValue() override;

private:

	mutable TSharedPtr<FFloatCurveKeyArea> KeyArea;
};
