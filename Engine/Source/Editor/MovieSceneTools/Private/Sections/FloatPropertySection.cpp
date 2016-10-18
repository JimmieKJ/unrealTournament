// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneToolsPrivatePCH.h"
#include "FloatCurveKeyArea.h"
#include "FloatPropertySection.h"
#include "MovieSceneFloatSection.h"


void FFloatPropertySection::GenerateSectionLayout(class ISectionLayoutBuilder& LayoutBuilder) const
{
	UMovieSceneFloatSection* FloatSection = Cast<UMovieSceneFloatSection>(&SectionObject);
	KeyArea = MakeShareable(new FFloatCurveKeyArea(&FloatSection->GetFloatCurve(), FloatSection));
	LayoutBuilder.SetSectionAsKeyArea(KeyArea.ToSharedRef());
}


void FFloatPropertySection::SetIntermediateValue(FPropertyChangedParams PropertyChangedParams)
{
	KeyArea->SetIntermediateValue(PropertyChangedParams.GetPropertyValue<float>());
}


void FFloatPropertySection::ClearIntermediateValue()
{
	KeyArea->ClearIntermediateValue();
}
