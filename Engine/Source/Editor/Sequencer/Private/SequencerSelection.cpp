// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SequencerPrivatePCH.h"
#include "SequencerSelection.h"
#include "MovieSceneSection.h"

FSequencerSelection::FSequencerSelection()
{
	ActiveSelection = EActiveSelection::None;
	bSuspendBroadcast = false;
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

FSequencerSelection::EActiveSelection FSequencerSelection::GetActiveSelection() const
{
	return ActiveSelection;
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

void FSequencerSelection::AddToSelection(FSequencerSelectedKey Key)
{
	SelectedKeys.Add(Key);
	ActiveSelection = EActiveSelection::KeyAndSection;
	if (!bSuspendBroadcast)
	{
		OnKeySelectionChanged.Broadcast();
	}
}

void FSequencerSelection::AddToSelection(UMovieSceneSection* Section)
{
	SelectedSections.Add(Section);
	ActiveSelection = EActiveSelection::KeyAndSection;
	if (!bSuspendBroadcast)
	{
		OnSectionSelectionChanged.Broadcast();
	}
}

void FSequencerSelection::AddToSelection(TSharedRef<FSequencerDisplayNode> OutlinerNode)
{
	SelectedOutlinerNodes.Add(OutlinerNode);
	ActiveSelection = EActiveSelection::OutlinerNode;
	if (!bSuspendBroadcast)
	{
		OnOutlinerNodeSelectionChanged.Broadcast();
	}
}

void FSequencerSelection::RemoveFromSelection(FSequencerSelectedKey Key)
{
	SelectedKeys.Remove(Key);
	ActiveSelection = EActiveSelection::KeyAndSection;
	if (!bSuspendBroadcast)
	{
		OnKeySelectionChanged.Broadcast();
	}
}

void FSequencerSelection::RemoveFromSelection(UMovieSceneSection* Section)
{
	SelectedSections.Remove(Section);
	ActiveSelection = EActiveSelection::KeyAndSection;
	if (!bSuspendBroadcast)
	{
		OnSectionSelectionChanged.Broadcast();
	}
}

void FSequencerSelection::RemoveFromSelection(TSharedRef<FSequencerDisplayNode> OutlinerNode)
{
	SelectedOutlinerNodes.Remove(OutlinerNode);
	ActiveSelection = EActiveSelection::OutlinerNode;
	if (!bSuspendBroadcast)
	{
		OnOutlinerNodeSelectionChanged.Broadcast();
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

void FSequencerSelection::Empty()
{
	EmptySelectedKeys();
	EmptySelectedSections();
	EmptySelectedOutlinerNodes();
}

void FSequencerSelection::EmptySelectedKeys()
{
	SelectedKeys.Empty();
	if (!bSuspendBroadcast)
	{
		OnKeySelectionChanged.Broadcast();
	}
}

void FSequencerSelection::EmptySelectedSections()
{
	SelectedSections.Empty();
	if (!bSuspendBroadcast)
	{
		OnSectionSelectionChanged.Broadcast();
	}
}

void FSequencerSelection::EmptySelectedOutlinerNodes()
{
	SelectedOutlinerNodes.Empty();
	if (!bSuspendBroadcast)
	{
		OnOutlinerNodeSelectionChanged.Broadcast();
	}
}