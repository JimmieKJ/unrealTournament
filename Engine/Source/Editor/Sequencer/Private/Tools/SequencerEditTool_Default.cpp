// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SequencerPrivatePCH.h"
#include "SequencerEditTool_Default.h"
#include "Sequencer.h"


FReply FSequencerEditTool_Default::OnMouseButtonUp(SWidget& OwnerWidget, const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		FSequencer& Sequencer = static_cast<FSequencer&>(GetSequencer());
		Sequencer.GetSelection().EmptySelectedSections();
		Sequencer.GetSelection().EmptySelectedKeys();
	}

	return FReply::Unhandled();
}
