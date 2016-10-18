// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "FloatCurveKeyArea.h"
#include "PropertySection.h"


class FFloatCurveKeyArea;
class ISequencer;
class UMovieSceneColorSection;
class UMovieSceneColorTrack;
class UMovieSceneSection;
class UMovieSceneTrack;


/**
* A color section implementation
*/
class FColorPropertySection
	: public FPropertySection
{
public:

	/**
	 * Create and initialize a new instance.
	 *
	 * @param InSectionObject
	 * @param SectionName The name of the section.
	 * @param InSequencer The sequencer that manages the section.
	 * @param InTrack The track that owns the section.
	 */
	FColorPropertySection(UMovieSceneSection& InSectionObject, ISequencer* InSequencer, UMovieSceneTrack& InTrack)
		: FPropertySection(InSectionObject, InTrack.GetDisplayName())
		, Sequencer(InSequencer)
		, Track(*Cast<UMovieSceneColorTrack>(&InTrack))
	{ }

public:

	// FPropertySection interface

	virtual void GenerateSectionLayout(class ISectionLayoutBuilder& LayoutBuilder) const override;
	virtual int32 OnPaintSection(FSequencerSectionPainter& Painter) const override;
	virtual void SetIntermediateValue( FPropertyChangedParams PropertyChangedParams ) override;
	virtual void ClearIntermediateValue() override;

protected:

	/** Consolidate color curves for all track sections. */
	void ConsolidateColorCurves(TArray< TKeyValuePair<float, FLinearColor> >& OutColorKeys, const UMovieSceneColorSection* Section) const;
	
	/** Find the Slate color of the specified name in the track. */
	FLinearColor FindSlateColor(const FName& ColorName) const;

private:

	mutable TSharedPtr<FFloatCurveKeyArea> RedKeyArea;
	mutable TSharedPtr<FFloatCurveKeyArea> GreenKeyArea;
	mutable TSharedPtr<FFloatCurveKeyArea> BlueKeyArea;
	mutable TSharedPtr<FFloatCurveKeyArea> AlphaKeyArea;

	/** The sequencer that manages the section. */
	ISequencer* Sequencer;

	/** The track that owns the section. */
	UMovieSceneColorTrack& Track;
};
