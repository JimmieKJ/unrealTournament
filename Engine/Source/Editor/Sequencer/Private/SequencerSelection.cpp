// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SequencerPrivatePCH.h"
#include "SequencerSelection.h"
#include "MovieSceneSection.h"

FSequencerSelection::FSequencerSelection()
{
	ActiveSelection = EActiveSelection::None;
}

const TSet<FSelectedKey>* FSequencerSelection::GetSelectedKeys() const
{
	return &SelectedKeys;
}

const TSet<TWeakObjectPtr<UMovieSceneSection>>* FSequencerSelection::GetSelectedSections() const
{
	return &SelectedSections;
}

const TSet<TSharedRef<const FSequencerDisplayNode>>* FSequencerSelection::GetSelectedOutlinerNodes() const
{
	return &SelectedOutlinerNodes;
}

FSequencerSelection::EActiveSelection FSequencerSelection::GetActiveSelection() const
{
	return ActiveSelection;
}

FSequencerSelection::FOnSelectionChanged* FSequencerSelection::GetOnKeySelectionChanged()
{
	return &OnKeySelectionChanged;
}

FSequencerSelection::FOnSelectionChanged* FSequencerSelection::GetOnSectionSelectionChanged()
{
	return &OnSectionSelectionChanged;
}

FSequencerSelection::FOnSelectionChanged* FSequencerSelection::GetOnOutlinerNodeSelectionChanged()
{
	return &OnOutlinerNodeSelectionChanged;
}

void FSequencerSelection::AddToSelection(FSelectedKey Key)
{
	SelectedKeys.Add(Key);
	ActiveSelection = EActiveSelection::KeyAndSection;
	OnKeySelectionChanged.Broadcast();
}

void FSequencerSelection::AddToSelection(UMovieSceneSection* Section)
{
	SelectedSections.Add(Section);
	ActiveSelection = EActiveSelection::KeyAndSection;
	OnSectionSelectionChanged.Broadcast();
}

void FSequencerSelection::AddToSelection(TSharedRef<const FSequencerDisplayNode> OutlinerNode)
{
	SelectedOutlinerNodes.Add(OutlinerNode);
	ActiveSelection = EActiveSelection::OutlinerNode;
	OnOutlinerNodeSelectionChanged.Broadcast();
}

bool FSequencerSelection::IsSelected(FSelectedKey Key) const
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
	OnKeySelectionChanged.Broadcast();
}

void FSequencerSelection::EmptySelectedSections()
{
	SelectedSections.Empty();
	OnSectionSelectionChanged.Broadcast();
}

void FSequencerSelection::EmptySelectedOutlinerNodes()
{
	SelectedOutlinerNodes.Empty();
	OnOutlinerNodeSelectionChanged.Broadcast();
}