// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ByteKeyArea.h"


/**
 * A key area for displaying and editing integral curves representing enums.
 */
class FEnumKeyArea
	: public FByteKeyArea
{
public:

	/* Create and initialize a new instance. */
	FEnumKeyArea(FIntegralCurve& InCurve, UMovieSceneSection* InOwningSection, const UEnum* InEnum)
		: FByteKeyArea(InCurve, InOwningSection)
		, Enum(InEnum)
	{ }

public:

	// FByteKeyArea overrides

	virtual bool CanCreateKeyEditor() override;
	virtual TSharedRef<SWidget> CreateKeyEditor(ISequencer* Sequencer) override;

private:

	/** The enum which provides available integral values for this key area. */
	const UEnum* Enum;
};
