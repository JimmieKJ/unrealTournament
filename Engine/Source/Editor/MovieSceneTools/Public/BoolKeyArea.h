// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IntegralKeyArea.h"
#include "ClipboardTypes.h"

/**
 * A key area for displaying and editing integral curves representing bools.
 */
class FBoolKeyArea
	: public FIntegralKeyArea<bool>
{
public:

	/* Create and initialize a new instance. */
	FBoolKeyArea(FIntegralCurve& InCurve, UMovieSceneSection* InOwningSection)
		: FIntegralKeyArea(InCurve, InOwningSection)
	{ }

public:

	// FIntegralKeyArea overrides

	virtual bool CanCreateKeyEditor() override;
	virtual TSharedRef<SWidget> CreateKeyEditor(ISequencer* Sequencer) override;

protected:

	void OnValueChanged(bool InValue);
};
