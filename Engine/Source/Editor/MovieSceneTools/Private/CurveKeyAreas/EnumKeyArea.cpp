// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneToolsPrivatePCH.h"
#include "EnumKeyArea.h"
#include "SEnumCurveKeyEditor.h"


/* IKeyArea interface
 *****************************************************************************/

bool FEnumKeyArea::CanCreateKeyEditor()
{
	return true;
}


TSharedRef<SWidget> FEnumKeyArea::CreateKeyEditor(ISequencer* Sequencer)
{
	return SNew(SEnumCurveKeyEditor)
		.Sequencer(Sequencer)
		.OwningSection(OwningSection)
		.Curve(&Curve)
		.Enum(Enum)
		.IntermediateValue_Lambda([this] {
			return IntermediateValue;
		});
};
