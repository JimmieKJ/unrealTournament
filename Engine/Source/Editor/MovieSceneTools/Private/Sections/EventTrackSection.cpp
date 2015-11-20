// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneToolsPrivatePCH.h"
#include "EventTrackSection.h"
#include "MovieSceneEventSection.h"


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


int32 FEventTrackSection::OnPaintSection(const FGeometry& AllottedGeometry, const FSlateRect& SectionClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, bool bParentEnabled) const
{
	return LayerId + 1;
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
