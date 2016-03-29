// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneToolsPrivatePCH.h"
#include "ActorReferencePropertySection.h"
#include "MovieSceneActorReferenceSection.h"

void FActorReferencePropertySection::GenerateSectionLayout( class ISectionLayoutBuilder& LayoutBuilder ) const
{
	UMovieSceneActorReferenceSection* ActorReferenceSection = Cast<UMovieSceneActorReferenceSection>( &SectionObject );
	KeyArea = MakeShareable( new FActorReferenceKeyArea( ActorReferenceSection->GetActorReferenceCurve(), ActorReferenceSection ) );
	LayoutBuilder.SetSectionAsKeyArea( KeyArea.ToSharedRef() );
}

void FActorReferencePropertySection::SetIntermediateValue( FPropertyChangedParams PropertyChangedParams )
{
}

void FActorReferencePropertySection::ClearIntermediateValue()
{
	KeyArea->ClearIntermediateValue();
}