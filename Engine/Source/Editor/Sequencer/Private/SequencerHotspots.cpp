// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SequencerPrivatePCH.h"
#include "SequencerHotspots.h"
#include "Tools/EditToolDragOperations.h"
#include "Sequencer.h"
#include "MovieSceneSection.h"

TSharedPtr<ISequencerEditToolDragOperation> FSectionResizeHotspot::InitiateDrag(ISequencer& Sequencer)
{
	const auto& SelectedSections = Sequencer.GetSelection().GetSelectedSections();
	auto SectionHandles = StaticCastSharedRef<SSequencer>(Sequencer.GetSequencerWidget())->GetSectionHandles(SelectedSections);
	
	if (!SelectedSections.Contains(Section.GetSectionObject()))
	{
		SectionHandles.Add(Section);
	}
	return MakeShareable( new FResizeSection(static_cast<FSequencer&>(Sequencer), SectionHandles, HandleType == Right) );
}