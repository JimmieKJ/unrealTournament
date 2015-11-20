// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


/**
 * A movie scene section for material parameters.
 */
class FMaterialParameterSection
	: public FPropertySection
{
public:

	FMaterialParameterSection(UMovieSceneSection& InSectionObject, const FText& SectionName)
		: FPropertySection(InSectionObject, SectionName)
	{ }

public:

	// FPropertySection interface
	virtual void SetIntermediateValue(FPropertyChangedParams PropertyChangedParams) override { }
	virtual void ClearIntermediateValue() override { }

public:

	// ISequencerSection interface

	virtual void GenerateSectionLayout(class ISectionLayoutBuilder& LayoutBuilder) const override;
	virtual bool RequestDeleteCategory(const TArray<FName>& CategoryNamePath) override;
	virtual bool RequestDeleteKeyArea(const TArray<FName>& KeyAreaNamePath) override;
};