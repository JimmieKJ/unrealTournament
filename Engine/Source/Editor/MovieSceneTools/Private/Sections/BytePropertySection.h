// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PropertySection.h"


class FByteKeyArea;
class UMovieSceneSection;


/**
 * An implementation of byte property sections
 */
class FBytePropertySection
	: public FPropertySection
{
public:

	FBytePropertySection(UMovieSceneSection& InSectionObject, const FText& InSectionName, UEnum* InEnum)
		: FPropertySection(InSectionObject, InSectionName)
		, Enum( InEnum )
	{ }

public:

	// FPropertySection interface

	virtual void GenerateSectionLayout(class ISectionLayoutBuilder& LayoutBuilder) const override;
	virtual void SetIntermediateValue(FPropertyChangedParams PropertyChangedParams) override;
	virtual void ClearIntermediateValue() override;

private:

	mutable TSharedPtr<FByteKeyArea> KeyArea;
	UEnum* Enum;
};
