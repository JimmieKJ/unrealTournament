// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "DisplayNodes/SequencerSectionKeyAreaNode.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBox.h"
#include "SSequencer.h"
#include "IKeyArea.h"
#include "SKeyNavigationButtons.h"

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


TSharedRef<SWidget> FSequencerSectionKeyAreaNode::GetCustomOutlinerContent()
{
	// @todo sequencer: support multiple sections/key areas?
	TArray<TSharedRef<IKeyArea>> AllKeyAreas = GetAllKeyAreas();

	if (AllKeyAreas.Num() > 0)
	{
		if (AllKeyAreas[0]->CanCreateKeyEditor())
		{
			return SNew(SBox)
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Center)
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.HAlign(HAlign_Right)
				.VAlign(VAlign_Center)
				[
					SNew(SBox)
					.WidthOverride(100)
					.HAlign(HAlign_Left)
					[
						AllKeyAreas[0]->CreateKeyEditor(&GetSequencer())
					]
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SKeyNavigationButtons, AsShared())
				]
			];
		}
	}

	return FSequencerDisplayNode::GetCustomOutlinerContent();
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
	return FNodePadding(0.f);//FNodePadding(0.f, 1.f);
}


ESequencerNode::Type FSequencerSectionKeyAreaNode::GetType() const
{
	return ESequencerNode::KeyArea;
}


void FSequencerSectionKeyAreaNode::SetDisplayName(const FText& NewDisplayName)
{
	check(false);
}
