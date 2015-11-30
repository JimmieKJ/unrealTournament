// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SequencerPrivatePCH.h"
#include "SequencerHotspots.h"
#include "Tools/EditToolDragOperations.h"
#include "Sequencer.h"
#include "MovieSceneSection.h"
#include "SequencerContextMenus.h"

void FKeyHotspot::PopulateContextMenu(FMenuBuilder& MenuBuilder, ISequencer& InSequencer, float MouseDownTime)
{
	FSequencer& Sequencer = static_cast<FSequencer&>(InSequencer);
	FKeyContextMenu::BuildMenu(MenuBuilder, Sequencer);
}

void FSectionHotspot::PopulateContextMenu(FMenuBuilder& MenuBuilder, ISequencer& InSequencer, float MouseDownTime)
{
	FSequencer& Sequencer = static_cast<FSequencer&>(InSequencer);

	TSharedPtr<ISequencerSection> SectionInterface = Section.TrackNode->GetSections()[Section.SectionIndex];

	FGuid ObjectBinding;
	if (Section.TrackNode.IsValid())
	{
		TSharedPtr<FSequencerDisplayNode> ParentNode = Section.TrackNode;

		while (ParentNode.IsValid())
		{
			if (ParentNode.Get()->GetType() == ESequencerNode::Object)
			{
				TSharedPtr<FSequencerObjectBindingNode> ObjectNode = StaticCastSharedPtr<FSequencerObjectBindingNode>(ParentNode);
				if (ObjectNode.IsValid())
				{
					ObjectBinding = ObjectNode.Get()->GetObjectBinding();
					break;
				}
			}
			ParentNode = ParentNode.Get()->GetParent();
		}
	}

	SectionInterface->BuildSectionContextMenu(MenuBuilder, ObjectBinding);

	FSectionContextMenu::BuildMenu(MenuBuilder, Sequencer, MouseDownTime);
}

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