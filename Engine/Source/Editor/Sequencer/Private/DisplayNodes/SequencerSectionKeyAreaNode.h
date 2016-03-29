// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SequencerDisplayNode.h"


class FSequencerNodeTree;
class IKeyArea;


/**
 * Represents an area inside a section where keys are displayed.
 *
 * There is one key area per section that defines that key area.
 */
class FSequencerSectionKeyAreaNode
	: public FSequencerDisplayNode
{
public:

	/**
	 * Create and initialize a new instance.
	 * 
	 * @param InNodeName The name identifier of then node.
	 * @param InDisplayName Display name of the category.
	 * @param InParentNode The parent of this node, or nullptr if this is a root node.
	 * @param InParentTree The tree this node is in.
	 * @param bInTopLevel If true the node is part of the section itself instead of taking up extra height in the section.
	 */
	FSequencerSectionKeyAreaNode(FName NodeName, const FText& InDisplayName, TSharedPtr<FSequencerDisplayNode> InParentNode, FSequencerNodeTree& InParentTree, bool bInTopLevel = false)
		: FSequencerDisplayNode(NodeName, InParentNode, InParentTree)
		, DisplayName(InDisplayName)
		, bTopLevel(bInTopLevel)
	{ }

public:

	/**
	 * Adds a key area to this node.
	 *
	 * @param KeyArea The key area interface to add.
	 */
	void AddKeyArea(TSharedRef<IKeyArea> KeyArea);

	/**
	 * Returns a key area at the given index.
	 * 
	 * @param Index	The index of the key area to get.
	 * @return the key area at the index.
	 */
	TSharedRef<IKeyArea> GetKeyArea(uint32 Index) const
	{
		return KeyAreas[Index];
	}

	/**
	 * Returns all key area for this node
	 * 
	 * @return All key areas
	 */
	const TArray<TSharedRef<IKeyArea>>& GetAllKeyAreas() const
	{
		return KeyAreas;
	}

	/** @return Whether the node is top level.  (I.E., is part of the section itself instead of taking up extra height in the section) */
	bool IsTopLevel() const
	{
		return bTopLevel;
	}

public:

	// FSequencerDisplayNode interface

	virtual bool CanRenameNode() const override;
	virtual TSharedRef<SWidget> GetCustomOutlinerContent() override;
	virtual FText GetDisplayName() const override;
	virtual float GetNodeHeight() const override;
	virtual FNodePadding GetNodePadding() const override;
	virtual ESequencerNode::Type GetType() const override;
	virtual void SetDisplayName(const FText& NewDisplayName) override;

private:

	/** The display name of the key area. */
	FText DisplayName;

	/** All key areas on this node (one per section). */
	TArray<TSharedRef<IKeyArea>> KeyAreas;

	/** If true the node is part of the section itself instead of taking up extra height in the section. */
	bool bTopLevel;
};
