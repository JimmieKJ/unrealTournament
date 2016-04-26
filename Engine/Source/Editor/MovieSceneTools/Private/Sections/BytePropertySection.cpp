// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneToolsPrivatePCH.h"
#include "MovieSceneByteTrack.h"
#include "MovieSceneByteSection.h"
#include "BytePropertySection.h"
#include "EnumKeyArea.h"


void FBytePropertySection::GenerateSectionLayout( class ISectionLayoutBuilder& LayoutBuilder ) const
{
	UMovieSceneByteSection* ByteSection = Cast<UMovieSceneByteSection>( &SectionObject );
	if ( Enum != nullptr )
	{
		KeyArea = MakeShareable( new FEnumKeyArea( ByteSection->GetCurve(), ByteSection, Enum ) );
		LayoutBuilder.SetSectionAsKeyArea( KeyArea.ToSharedRef() );
	}
	else
	{
		KeyArea = MakeShareable( new FByteKeyArea( ByteSection->GetCurve(), ByteSection ) );
		LayoutBuilder.SetSectionAsKeyArea( KeyArea.ToSharedRef() );
	}
}


void FBytePropertySection::SetIntermediateValue( FPropertyChangedParams PropertyChangedParams )
{
	KeyArea->SetIntermediateValue( PropertyChangedParams.GetPropertyValue<uint8>() );
}


void FBytePropertySection::ClearIntermediateValue()
{
	KeyArea->ClearIntermediateValue();
}