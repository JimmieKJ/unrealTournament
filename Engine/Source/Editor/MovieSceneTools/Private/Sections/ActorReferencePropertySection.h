// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IntegralKeyArea.h"
#include "PropertySection.h"


struct FIntegralCurve;
class FActorReferenceKeyArea;
class UMovieSceneSection;


/** A key area for displaying and editing an actor reference property. */
class FActorReferenceKeyArea
	: public FIntegralKeyArea<int32>
{
public:

	FActorReferenceKeyArea(FIntegralCurve& InCurve, UMovieSceneSection* InOwningSection)
		: FIntegralKeyArea(InCurve, InOwningSection)
	{ }

public:

	//~ FIntegralKeyArea overrides

	virtual bool CanCreateKeyEditor() override { return false; }
	virtual TSharedRef<SWidget> CreateKeyEditor(ISequencer* Sequencer) override { return SNew(SSpacer); }
};


/**
 * A property section for actor references.
 */
class FActorReferencePropertySection
	: public FPropertySection
{
public:

	FActorReferencePropertySection(UMovieSceneSection& InSectionObject, const FText& SectionName)
		: FPropertySection(InSectionObject, SectionName)
	{ }

public:

	//~ FPropertySection interface

	virtual void GenerateSectionLayout(class ISectionLayoutBuilder& LayoutBuilder) const override;
	virtual void SetIntermediateValue(FPropertyChangedParams PropertyChangedParams) override;
	virtual void ClearIntermediateValue() override;

private:

	mutable TSharedPtr<FActorReferenceKeyArea> KeyArea;
};
