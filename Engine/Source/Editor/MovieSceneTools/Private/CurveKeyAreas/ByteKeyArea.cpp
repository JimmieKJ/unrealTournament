// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneToolsPrivatePCH.h"
#include "ByteKeyArea.h"
#include "SIntegralCurveKeyEditor.h"


/* IKeyArea interface
 *****************************************************************************/

bool FByteKeyArea::CanCreateKeyEditor()
{
	return true;
}


TSharedRef<SWidget> FByteKeyArea::CreateKeyEditor(ISequencer* Sequencer)
{
	return SNew(SIntegralCurveKeyEditor)
		.Sequencer(Sequencer)
		.OwningSection(OwningSection)
		.Curve(&Curve)
		.IntermediateValue_Lambda( [this] {
			return IntermediateValue;
		});
};
