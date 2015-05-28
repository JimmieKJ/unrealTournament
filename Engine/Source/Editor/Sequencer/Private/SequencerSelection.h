// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SelectedKey.h"

class UMovieSceneSection;
class FSequencerDisplayNode;

/**
 * Manages the selection of keys, sections, and outliner nodes for the sequencer.
 */
class FSequencerSelection
{
public:
	DECLARE_MULTICAST_DELEGATE(FOnSelectionChanged)

	/** Options for the current active selection. */
	enum class EActiveSelection
	{
		KeyAndSection,
		OutlinerNode,
		None
	};

public:
	FSequencerSelection();

	/** Gets a set of the selected keys. */
	const TSet<FSelectedKey>* GetSelectedKeys() const;

	/** Gets a set of the selected sections */
	const TSet<TWeakObjectPtr<UMovieSceneSection>>* GetSelectedSections() const;

	/** Gets a set of the selected outliner nodes. */
	const TSet<TSharedRef<const FSequencerDisplayNode>>* GetSelectedOutlinerNodes() const;

	/** 
	 * Gets the currently active selection.  This is used to determine which selection actions
	 * like delete should act on.
	 */
	EActiveSelection GetActiveSelection() const;

	/** Adds a key to the selection */
	void AddToSelection(FSelectedKey Key);

	/** Adds a section to the selection */
	void AddToSelection(UMovieSceneSection* Section);

	/** Adds an outliner node to the selection */
	void AddToSelection(TSharedRef<const FSequencerDisplayNode> OutlinerNode);

	/** Returns whether or not the key is selected. */
	bool IsSelected(FSelectedKey Key) const;

	/** Returns whether or not the section is selected. */
	bool IsSelected(UMovieSceneSection* Section) const;

	/** Returns whether or not the outliner node is selected. */
	bool IsSelected(TSharedRef<FSequencerDisplayNode> OutlinerNode) const;

	/** Empties all selections. */
	void Empty();

	/** Empties the key selection. */
	void EmptySelectedKeys();

	/** Empties the section selection. */
	void EmptySelectedSections();

	/** Empties the outliner node selection. */
	void EmptySelectedOutlinerNodes();

	/** Gets a multicast delegate which is called when the key selection changes. */
	FOnSelectionChanged* GetOnKeySelectionChanged();

	/** Gets a multicast delegate which is called when the section selection changes. */
	FOnSelectionChanged* GetOnSectionSelectionChanged();

	/** Gets a multicast delegate which is called when the outliner node selection changes. */
	FOnSelectionChanged* GetOnOutlinerNodeSelectionChanged();
	
private:
	TSet<FSelectedKey> SelectedKeys;
	TSet<TWeakObjectPtr<UMovieSceneSection>> SelectedSections;
	TSet<TSharedRef<const FSequencerDisplayNode>> SelectedOutlinerNodes;

	FOnSelectionChanged OnKeySelectionChanged;
	FOnSelectionChanged OnSectionSelectionChanged;
	FOnSelectionChanged OnOutlinerNodeSelectionChanged;

	EActiveSelection ActiveSelection;
};