// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SequencerPrivatePCH.h"
#include "Sequencer.h"
#include "SequencerSectionPainter.h"

FSequencerSectionPainter::FSequencerSectionPainter(FSlateWindowElementList& OutDrawElements, const FGeometry& InSectionGeometry, UMovieSceneSection& InSection)
	: Section(InSection)
	, DrawElements(OutDrawElements)
	, SectionGeometry(InSectionGeometry)
	, LayerId(0)
	, bParentEnabled(true)
{
}

int32 FSequencerSectionPainter::PaintSectionBackground()
{
	return PaintSectionBackground(GetTrack()->GetColorTint());
}

UMovieSceneTrack* FSequencerSectionPainter::GetTrack() const
{
	return Section.GetTypedOuter<UMovieSceneTrack>();
}

FLinearColor FSequencerSectionPainter::BlendColor(FLinearColor InColor)
{
	static FLinearColor BaseColor(FColor(71,71,71));

	const float Alpha = InColor.A;
	InColor.A = 1.f;
	
	return BaseColor * (1.f - Alpha) + InColor * Alpha;
}