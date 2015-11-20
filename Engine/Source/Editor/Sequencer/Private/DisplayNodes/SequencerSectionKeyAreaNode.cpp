// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SequencerPrivatePCH.h"
#include "IKeyArea.h"
#include "Sequencer.h"


/* FSectionKeyAreaNode interface
 *****************************************************************************/

void FSequencerSectionKeyAreaNode::AddKeyArea(TSharedRef<IKeyArea> KeyArea)
{
	KeyAreas.Add(KeyArea);
}


/* FSequencerDisplayNode interface
 *****************************************************************************/

bool FSequencerSectionKeyAreaNode::CanRenameNode() const
{
	return false;
}


TSharedRef<SWidget> FSequencerSectionKeyAreaNode::GenerateEditWidgetForOutliner()
{
	// @todo sequencer: support multiple sections/key areas?
	TArray<TSharedRef<IKeyArea>> AllKeyAreas = GetAllKeyAreas();

	if (AllKeyAreas.Num() > 0)
	{
		if (AllKeyAreas[0]->CanCreateKeyEditor())
		{
			return AllKeyAreas[0]->CreateKeyEditor(&GetSequencer());
		}
	}

	return FSequencerDisplayNode::GenerateEditWidgetForOutliner();
}


FText FSequencerSectionKeyAreaNode::GetDisplayName() const
{
	return DisplayName;
}


float FSequencerSectionKeyAreaNode::GetNodeHeight() const
{
	//@todo sequencer: should be defined by the key area probably
	return SequencerLayoutConstants::KeyAreaHeight;
}


FNodePadding FSequencerSectionKeyAreaNode::GetNodePadding() const
{
	return FNodePadding(0.f, 1.f);
}


ESequencerNode::Type FSequencerSectionKeyAreaNode::GetType() const
{
	return ESequencerNode::KeyArea;
}


void FSequencerSectionKeyAreaNode::SetDisplayName(const FText& InDisplayName)
{
	check(false);
}
