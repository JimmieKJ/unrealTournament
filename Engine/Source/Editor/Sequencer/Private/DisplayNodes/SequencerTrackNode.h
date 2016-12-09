// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/WeakObjectPtr.h"
#include "Widgets/SWidget.h"
#include "DisplayNodes/SequencerDisplayNode.h"
#include "DisplayNodes/SequencerSectionKeyAreaNode.h"
#include "ISequencerSection.h"
#include "IKeyArea.h"

class FMenuBuilder;
class ISequencerTrackEditor;
class UMovieSceneTrack;
struct FSlateBrush;

/**
 * Represents an area to display Sequencer sections (possibly on multiple lines).
 */
class FSequencerTrackNode
	: public FSequencerDisplayNode
{
public:

	/**
	 * Create and initialize a new instance.
	 * 
	 * @param InAssociatedType The track that this node represents.
	 * @param InAssociatedEditor The track editor for the track that this node represents.
	 * @param bInCanBeDragged Whether or not this node can be dragged and dropped.
	 * @param InParentNode The parent of this node, or nullptr if this is a root node.
	 * @param InParentTree The tree this node is in.
	 */
	FSequencerTrackNode(UMovieSceneTrack& InAssociatedTrack, ISequencerTrackEditor& InAssociatedEditor, bool bInCanBeDragged, TSharedPtr<FSequencerDisplayNode> InParentNode, FSequencerNodeTree& InParentTree);

public:

	/**
	 * Adds a section to this node
	 *
	 * @param SequencerSection	The section to add
	 */
	void AddSection(TSharedRef<ISequencerSection>& SequencerSection)
	{
		Sections.Add(SequencerSection);
	}

	/**
	 * Makes the section itself a key area without taking up extra space
	 *
	 * @param KeyArea	Interface for the key area
	 */
	void SetSectionAsKeyArea(TSharedRef<IKeyArea>& KeyArea);
	
	/**
	 * Adds a key to the track
	 *
	 */
	void AddKey(const FGuid& ObjectGuid);

	/**
	 * @return All sections in this node
	 */
	const TArray<TSharedRef<ISequencerSection>>& GetSections() const
	{
		return Sections;
	}

	TArray<TSharedRef<ISequencerSection>>& GetSections()
	{
		return Sections;
	}

	/** @return Returns the top level key node for the section area if it exists */
	TSharedPtr<FSequencerSectionKeyAreaNode> GetTopLevelKeyNode() const
	{
		return TopLevelKeyNode;
	}

	/** @return the track associated with this section */
	UMovieSceneTrack* GetTrack() const
	{
		return AssociatedTrack.Get();
	}

	/** Gets the greatest row index of all the sections we have */
	int32 GetMaxRowIndex() const;

	/** Ensures all row indices which have no sections are gone */
	void FixRowIndices();

public:

	// FSequencerDisplayNode interface

	virtual void BuildContextMenu( FMenuBuilder& MenuBuilder );
	virtual bool CanRenameNode() const override;
	virtual TSharedRef<SWidget> GetCustomOutlinerContent() override;
	virtual void GetChildKeyAreaNodesRecursively(TArray<TSharedRef<FSequencerSectionKeyAreaNode>>& OutNodes) const override;
	virtual FText GetDisplayName() const override;
	virtual float GetNodeHeight() const override;
	virtual FNodePadding GetNodePadding() const override;
	virtual ESequencerNode::Type GetType() const override;
	virtual void SetDisplayName(const FText& NewDisplayName) override;
	virtual const FSlateBrush* GetIconBrush() const override;
	virtual bool CanDrag() const override;

private:

	/** The track editor for the track associated with this node. */
	ISequencerTrackEditor& AssociatedEditor;

	/** The type associated with the sections in this node */
	TWeakObjectPtr<UMovieSceneTrack> AssociatedTrack;

	/** All of the sequencer sections in this node */
	TArray<TSharedRef<ISequencerSection>> Sections;

	/** If the section area is a key area itself, this represents the node for the keys */
	TSharedPtr<FSequencerSectionKeyAreaNode> TopLevelKeyNode;

	/** Whether or not this track node can be dragged. */
	bool bCanBeDragged;
};
