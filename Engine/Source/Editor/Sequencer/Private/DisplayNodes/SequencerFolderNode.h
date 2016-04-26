// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SequencerDisplayNode.h"

/** A sequencer display node representing folders in the outliner. */
class FSequencerFolderNode : public FSequencerDisplayNode
{
public:
	FSequencerFolderNode( UMovieSceneFolder& InMovieSceneFolder, TSharedPtr<FSequencerDisplayNode> InParentNode, FSequencerNodeTree& InParentTree );

	// FSequencerDisplayNode interface
	virtual ESequencerNode::Type GetType() const override;
	virtual float GetNodeHeight() const override;
	virtual FNodePadding GetNodePadding() const override;
	virtual bool CanRenameNode() const override;
	virtual FText GetDisplayName() const override;
	virtual void SetDisplayName( const FText& NewDisplayName ) override;
	virtual const FSlateBrush* GetIconBrush() const override;
	virtual bool CanDrag() const override;
	virtual TOptional<EItemDropZone> CanDrop( FSequencerDisplayNodeDragDropOp& DragDropOp, EItemDropZone ItemDropZone ) const override;
	virtual void Drop( const TArray<TSharedRef<FSequencerDisplayNode>>& DraggedNodes, EItemDropZone ItemDropZone ) override;

	/** Adds a child node to this folder node. */
	void AddChildNode( TSharedRef<FSequencerDisplayNode> ChildNode );

	/** Gets the folder data for this display node. */
	UMovieSceneFolder& GetFolder() const;

private:

	/** The brush used to draw the icon when this folder is open .*/
	const FSlateBrush* FolderOpenBrush;

	/** The brush used to draw the icon when this folder is closed. */
	const FSlateBrush* FolderClosedBrush;

	/** The movie scene folder data which this node represents. */
	UMovieSceneFolder& MovieSceneFolder;
};