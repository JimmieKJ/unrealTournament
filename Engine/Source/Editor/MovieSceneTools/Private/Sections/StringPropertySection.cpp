// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneToolsPrivatePCH.h"
#include "StringCurveKeyArea.h"
#include "StringPropertySection.h"
#include "MovieSceneStringSection.h"


void FStringPropertySection::GenerateSectionLayout(class ISectionLayoutBuilder& LayoutBuilder) const
{
	UMovieSceneStringSection* StringSection = Cast<UMovieSceneStringSection>(&SectionObject);
	KeyArea = MakeShareable(new FStringCurveKeyArea(&StringSection->GetStringCurve(), StringSection));
	LayoutBuilder.SetSectionAsKeyArea(KeyArea.ToSharedRef());
}


void FStringPropertySection::SetIntermediateValue(FPropertyChangedParams PropertyChangedParams)
{
	void* CurrentObject = PropertyChangedParams.ObjectsThatChanged[0];
	void* PropertyValue = nullptr;
	for (int32 i = 0; i < PropertyChangedParams.PropertyPath.Num(); i++)
	{
		CurrentObject = PropertyChangedParams.PropertyPath[i]->ContainerPtrToValuePtr<FString>(CurrentObject, 0);
	}

	const UStrProperty* StrProperty = Cast<const UStrProperty>( PropertyChangedParams.PropertyPath.Last() );
	if ( StrProperty )
	{
		FString StrPropertyValue = StrProperty->GetPropertyValue(CurrentObject);
		KeyArea->SetIntermediateValue(StrPropertyValue);
	}
}


void FStringPropertySection::ClearIntermediateValue()
{
	KeyArea->ClearIntermediateValue();
}
