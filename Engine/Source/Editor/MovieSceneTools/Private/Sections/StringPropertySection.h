// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PropertySection.h"


class FStringCurveKeyArea;
class UMovieSceneSection;


/**
 * An implementation of FString property sections.
 */
class FStringPropertySection
	: public FPropertySection
{
public:

	FStringPropertySection(UMovieSceneSection& InSectionObject, const FText& SectionName)
		: FPropertySection(InSectionObject, SectionName)
	{ }

public:

	//~ FPropertySection interface 

	virtual void GenerateSectionLayout(class ISectionLayoutBuilder& LayoutBuilder) const override;
	virtual void SetIntermediateValue(FPropertyChangedParams PropertyChangedParams) override;
	virtual void ClearIntermediateValue() override;

private:

	mutable TSharedPtr<FStringCurveKeyArea> KeyArea;
};
