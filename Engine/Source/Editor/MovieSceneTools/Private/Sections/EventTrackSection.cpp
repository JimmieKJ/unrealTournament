// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneToolsPrivatePCH.h"
#include "EventTrackSection.h"
#include "MovieSceneEventSection.h"
#include "NameCurveKeyArea.h"


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


void FEventTrackSection::GenerateSectionLayout(class ISectionLayoutBuilder& LayoutBuilder) const
{
	LayoutBuilder.SetSectionAsKeyArea(MakeShareable(new FNameCurveKeyArea(Section->GetEventCurve(), Section)));
}


#undef LOCTEXT_NAMESPACE
