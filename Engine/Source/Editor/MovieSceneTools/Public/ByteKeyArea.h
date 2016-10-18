// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IntegralKeyArea.h"


class UMovieSceneSection;


/**
 * A key area for displaying and editing byte curves.
 */
class FByteKeyArea
	: public FIntegralKeyArea<uint8>
{
public:

	/* Create and initialize a new instance. */
	FByteKeyArea(FIntegralCurve& InCurve, UMovieSceneSection* InOwningSection)
		: FIntegralKeyArea(InCurve, InOwningSection)
	{ }

public:

	//~ FIntegralKeyArea interface

	virtual bool CanCreateKeyEditor() override;
	virtual TSharedRef<SWidget> CreateKeyEditor(ISequencer* Sequencer) override;
};
