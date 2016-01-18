// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BoolKeyArea.h"


/**
* An implementation of bool property sections
*/
class FBoolPropertySection
	: public FPropertySection
{
public:

	FBoolPropertySection( UMovieSceneSection& InSectionObject, const FText& SectionName, ISequencer* InSequencer )
		: FPropertySection( InSectionObject, SectionName )
		, Sequencer( InSequencer ) {}

public:

	// FPropertySection interface

	virtual void GenerateSectionLayout( class ISectionLayoutBuilder& LayoutBuilder ) const override;
	virtual int32 OnPaintSection( const FGeometry& AllottedGeometry, const FSlateRect& SectionClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, bool bParentEnabled ) const override;
	virtual void SetIntermediateValue( FPropertyChangedParams PropertyChangedParams ) override;
	virtual void ClearIntermediateValue() override;

protected:

	mutable TSharedPtr<FBoolKeyArea> KeyArea;

private:

	ISequencer* Sequencer;
};
