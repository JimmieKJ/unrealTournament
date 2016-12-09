// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "Sections/EventTrackSection.h"
#include "ISectionLayoutBuilder.h"
#include "SequencerSectionPainter.h"
#include "Sections/MovieSceneEventSection.h"
#include "GenericKeyArea.h"


#define LOCTEXT_NAMESPACE "FEventTrackSection"


/* FEventTrackEditor static functions
 *****************************************************************************/

FEventTrackSection::FEventTrackSection(UMovieSceneSection& InSection, TSharedPtr<ISequencer> InSequencer)
	: Section(Cast<UMovieSceneEventSection>(&InSection))
	, Sequencer(InSequencer)
{ }


/* FEventTrackEditor static functions
 *****************************************************************************/

UMovieSceneSection* FEventTrackSection::GetSectionObject()
{
	return Section;
}

int32 FEventTrackSection::OnPaintSection( FSequencerSectionPainter& InPainter ) const
{
	return InPainter.PaintSectionBackground();
}


FText FEventTrackSection::GetDisplayName() const
{
	return LOCTEXT("DisplayName", "Events");
}


FText FEventTrackSection::GetSectionTitle() const
{
	return FText::GetEmpty();
}


void FEventTrackSection::GenerateSectionLayout(ISectionLayoutBuilder& LayoutBuilder) const
{
	auto KeyArea = MakeShared<TGenericKeyArea<FEventPayload, float>>(Section->GetCurveInterface(), Section);
	LayoutBuilder.SetSectionAsKeyArea(KeyArea);
}


#undef LOCTEXT_NAMESPACE
