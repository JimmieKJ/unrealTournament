// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "SequencerPrivatePCH.h"
#include "SequencerSelection.h"
#include "MovieSceneSection.h"

FSequencerSelection::FSequencerSelection()
	: SuspendBroadcastCount(0)
	, bOutlinerNodeSelectionChangedBroadcastPending(false)
{
}

const TSet<FSequencerSelectedKey>& FSequencerSelection::GetSelectedKeys() const
{
	return SelectedKeys;
}

const TSet<TWeakObjectPtr<UMovieSceneSection>>& FSequencerSelection::GetSelectedSections() const
{
	return SelectedSections;
}

const TSet<TSharedRef<FSequencerDisplayNode>>& FSequencerSelection::GetSelectedOutlinerNodes() const
{
	return SelectedOutlinerNodes;
}

const TSet<TSharedRef<FSequencerDisplayNode>>& FSequencerSelection::GetNodesWithSelectedKeysOrSections() const
{
	return NodesWithSelectedKeysOrSections;
}

FSequencerSelection::FOnSelectionChanged& FSequencerSelection::GetOnKeySelectionChanged()
{
	return OnKeySelectionChanged;
}

FSequencerSelection::FOnSelectionChanged& FSequencerSelection::GetOnSectionSelectionChanged()
{
	return OnSectionSelectionChanged;
}

FSequencerSelection::FOnSelectionChanged& FSequencerSelection::GetOnOutlinerNodeSelectionChanged()
{
	return OnOutlinerNodeSelectionChanged;
}

FSequencerSelection::FOnSelectionChanged& FSequencerSelection::GetOnNodesWithSelectedKeysOrSectionsChanged()
{
	return OnNodesWithSelectedKeysOrSectionsChanged;
}

void FSequencerSelection::AddToSelection(FSequencerSelectedKey Key)
{
	SelectedKeys.Add(Key);
	if ( IsBroadcasting() )
	{
		OnKeySelectionChanged.Broadcast();
	}

	// Deselect any outliner nodes that aren't within the trunk of this key
	if (Key.KeyArea.IsValid())
	{
		EmptySelectedOutlinerNodesWithoutSection(Key.KeyArea->GetOwningSection());
	}

	EmptySelectedSections();
}

void FSequencerSelection::AddToSelection(UMovieSceneSection* Section)
{
	SelectedSections.Add(Section);
	if ( IsBroadcasting() )
	{
		OnSectionSelectionChanged.Broadcast();
	}

	// Deselect any outliner nodes that aren't within the trunk of this section
	if (Section)
	{
		EmptySelectedOutlinerNodesWithoutSection(Section);
	}

	EmptySelectedKeys();
}

void FSequencerSelection::AddToSelection(TSharedRef<FSequencerDisplayNode> OutlinerNode)
{
	SelectedOutlinerNodes.Add(OutlinerNode);
	if ( IsBroadcasting() )
	{
		OnOutlinerNodeSelectionChanged.Broadcast();
	}
	EmptySelectedKeys();
	EmptySelectedSections();
	EmptyNodesWithSelectedKeysOrSections();
}

void FSequencerSelection::AddToNodesWithSelectedKeysOrSections(TSharedRef<FSequencerDisplayNode> OutlinerNode)
{
	NodesWithSelectedKeysOrSections.Add(OutlinerNode);
	if ( IsBroadcasting() )
	{
		OnNodesWithSelectedKeysOrSectionsChanged.Broadcast();
	}
}


void FSequencerSelection::RemoveFromSelection(FSequencerSelectedKey Key)
{
	SelectedKeys.Remove(Key);
	if ( IsBroadcasting() )
	{
		OnKeySelectionChanged.Broadcast();
	}
}

void FSequencerSelection::RemoveFromSelection(UMovieSceneSection* Section)
{
	SelectedSections.Remove(Section);
	if ( IsBroadcasting() )
	{
		OnSectionSelectionChanged.Broadcast();
	}
}

void FSequencerSelection::RemoveFromSelection(TSharedRef<FSequencerDisplayNode> OutlinerNode)
{
	SelectedOutlinerNodes.Remove(OutlinerNode);
	if ( IsBroadcasting() )
	{
		OnOutlinerNodeSelectionChanged.Broadcast();
	}
}

void FSequencerSelection::RemoveFromNodesWithSelectedKeysOrSections(TSharedRef<FSequencerDisplayNode> OutlinerNode)
{
	NodesWithSelectedKeysOrSections.Remove(OutlinerNode);
	if ( IsBroadcasting() )
	{
		OnNodesWithSelectedKeysOrSectionsChanged.Broadcast();
	}
}

bool FSequencerSelection::IsSelected(FSequencerSelectedKey Key) const
{
	return SelectedKeys.Contains(Key);
}

bool FSequencerSelection::IsSelected(UMovieSceneSection* Section) const
{
	return SelectedSections.Contains(Section);
}

bool FSequencerSelection::IsSelected(TSharedRef<FSequencerDisplayNode> OutlinerNode) const
{
	return SelectedOutlinerNodes.Contains(OutlinerNode);
}

bool FSequencerSelection::NodeHasSelectedKeysOrSections(TSharedRef<FSequencerDisplayNode> OutlinerNode) const
{
	return NodesWithSelectedKeysOrSections.Contains(OutlinerNode);
}

void FSequencerSelection::Empty()
{
	EmptySelectedKeys();
	EmptySelectedSections();
	EmptySelectedOutlinerNodes();
	EmptyNodesWithSelectedKeysOrSections();
}

void FSequencerSelection::EmptySelectedKeys()
{
	if (!SelectedKeys.Num())
	{
		return;
	}

	SelectedKeys.Empty();
	if ( IsBroadcasting() )
	{
		OnKeySelectionChanged.Broadcast();
	}
}

void FSequencerSelection::EmptySelectedSections()
{
	if (!SelectedSections.Num())
	{
		return;
	}

	SelectedSections.Empty();
	if ( IsBroadcasting() )
	{
		OnSectionSelectionChanged.Broadcast();
	}
}

void FSequencerSelection::EmptySelectedOutlinerNodes()
{
	if (!SelectedOutlinerNodes.Num())
	{
		return;
	}

	SelectedOutlinerNodes.Empty();
	if ( IsBroadcasting() )
	{
		OnOutlinerNodeSelectionChanged.Broadcast();
	}
}

void FSequencerSelection::EmptyNodesWithSelectedKeysOrSections()
{
	if (!NodesWithSelectedKeysOrSections.Num())
	{
		return;
	}

	NodesWithSelectedKeysOrSections.Empty();
	if ( IsBroadcasting() )
	{
		OnNodesWithSelectedKeysOrSectionsChanged.Broadcast();
	}
}

/** Suspend or resume broadcast of selection changing  */
void FSequencerSelection::SuspendBroadcast()
{
	SuspendBroadcastCount++;
}

void FSequencerSelection::ResumeBroadcast()
{
	SuspendBroadcastCount--;
	checkf(SuspendBroadcastCount >= 0, TEXT("Suspend/Resume broadcast mismatch!"));
}

bool FSequencerSelection::IsBroadcasting() 
{
	return SuspendBroadcastCount == 0;
}

void FSequencerSelection::EmptySelectedOutlinerNodesWithoutSection(UMovieSceneSection* Section)
{
	for (auto SelectedOutlinerNode : SelectedOutlinerNodes)
	{
		TSet<TSharedRef<FSequencerDisplayNode> > TrunkNodes;
		TrunkNodes.Add(SelectedOutlinerNode);
		SequencerHelpers::GetDescendantNodes(SelectedOutlinerNode, TrunkNodes);
		
		bool bFoundMatch = false;
		for (auto TrunkIt = TrunkNodes.CreateConstIterator(); TrunkIt && !bFoundMatch; ++TrunkIt)
		{
			TSet<TWeakObjectPtr<UMovieSceneSection>> AllSections;
			SequencerHelpers::GetAllSections(*TrunkIt, AllSections);

			for (auto SectionIt = AllSections.CreateConstIterator(); SectionIt && !bFoundMatch; ++SectionIt)
			{
				if (*SectionIt == Section)
				{
					bFoundMatch = true;
					break;
				}
			}
		}

		if (!bFoundMatch)
		{
			RemoveFromSelection(SelectedOutlinerNode);
		}
	}
}

void FSequencerSelection::RequestOutlinerNodeSelectionChangedBroadcast()
{
	if ( IsBroadcasting() )
	{
		bOutlinerNodeSelectionChangedBroadcastPending = true;
	}
}

void FSequencerSelection::Tick()
{
	if ( bOutlinerNodeSelectionChangedBroadcastPending && IsBroadcasting() )
	{
		bOutlinerNodeSelectionChangedBroadcastPending = false;
		OnOutlinerNodeSelectionChanged.Broadcast();
	}
}

