// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ISequencerSection.h"

/**
 * A generic implementation for displaying simple property sections
 */
class FPropertySection : public ISequencerSection
{
public:
	FPropertySection( UMovieSceneSection& InSectionObject, FName SectionName )
		: SectionObject( InSectionObject )
	{
		DisplayName = FText::FromString( FName::NameToDisplayString( SectionName.ToString(), false ) );
	}

	/** ISequencerSection interface */
	virtual UMovieSceneSection* GetSectionObject() override { return &SectionObject; }

	virtual FText GetDisplayName() const override
	{
		return DisplayName;
	}
	
	virtual FText GetSectionTitle() const override { return FText::GetEmpty(); }

	virtual void GenerateSectionLayout( class ISectionLayoutBuilder& LayoutBuilder ) const override {}

	virtual int32 OnPaintSection( const FGeometry& AllottedGeometry, const FSlateRect& SectionClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, bool bParentEnabled ) const override 
	{
		// Add a box for the section
		FSlateDrawElement::MakeBox( 
			OutDrawElements,
			LayerId,
			AllottedGeometry.ToPaintGeometry(),
			FEditorStyle::GetBrush("Sequencer.GenericSection.Background"),
			SectionClippingRect
		);

		return LayerId;
	}
	
protected:
	/** Display name of the section */
	FText DisplayName;
	/** The section we are visualizing */
	UMovieSceneSection& SectionObject;
};

