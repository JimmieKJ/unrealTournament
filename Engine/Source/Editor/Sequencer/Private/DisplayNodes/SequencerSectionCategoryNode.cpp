// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SequencerPrivatePCH.h"
#include "SSequencer.h"


namespace SequencerSectionCategoryNodeConstants
{
	float NodePadding = 2.f;
}


/* FSequencerDisplayNode interface
 *****************************************************************************/

bool FSequencerSectionCategoryNode::CanRenameNode() const
{
	return false;
}


FText FSequencerSectionCategoryNode::GetDisplayName() const
{
	return DisplayName;
}


float FSequencerSectionCategoryNode::GetNodeHeight() const 
{
	return SequencerLayoutConstants::CategoryNodeHeight;
}


FNodePadding FSequencerSectionCategoryNode::GetNodePadding() const
{
	return FNodePadding(SequencerSectionCategoryNodeConstants::NodePadding);
}


ESequencerNode::Type FSequencerSectionCategoryNode::GetType() const
{
	return ESequencerNode::Category;
}


void FSequencerSectionCategoryNode::SetDisplayName(const FText& NewDisplayName)
{
	check(false);
}
