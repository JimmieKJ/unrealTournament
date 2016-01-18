// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SequencerDisplayNode.h"


/**
 * A node for displaying an object binding
 */
class FSequencerObjectBindingNode
	: public FSequencerDisplayNode
{
public:
	/**
	 * Create and initialize a new instance.
	 * 
	 * @param InNodeName The name identifier of then node.
	 * @param InObjectName The name of the object we're binding to.
	 * @param InObjectBinding Object binding guid for associating with live objects.
	 * @param InParentNode The parent of this node, or nullptr if this is a root node.
	 * @param InParentTree The tree this node is in.
	 */
	FSequencerObjectBindingNode(FName NodeName, const FText& InDisplayName, const FGuid& InObjectBinding, TSharedPtr<FSequencerDisplayNode> InParentNode, FSequencerNodeTree& InParentTree)
		: FSequencerDisplayNode(NodeName, InParentNode, InParentTree)
		, ObjectBinding(InObjectBinding)
		, DefaultDisplayName(InDisplayName)
	{ }

public:


	/** @return The object binding on this node */
	const FGuid& GetObjectBinding() const
	{
		return ObjectBinding;
	}
	
public:

	// FSequencerDisplayNode interface

	virtual void BuildContextMenu(FMenuBuilder& MenuBuilder) override;
	virtual bool CanRenameNode() const override;
	virtual TSharedRef<SWidget> GenerateEditWidgetForOutliner() override;
	virtual FText GetDisplayName() const override;
	virtual float GetNodeHeight() const override;
	virtual FNodePadding GetNodePadding() const override;
	virtual ESequencerNode::Type GetType() const override;
	virtual void SetDisplayName(const FText& NewDisplayName) override;

protected:

	void AddPropertyMenuItems(FMenuBuilder& AddTrackMenuBuilder, TArray<TArray<UProperty*>> KeyableProperties, int32 PropertyNameIndexStart = 0, int32 PropertyNameIndexEnd = -1);

	/** Get class for object binding */
	const UClass* GetClassForObjectBinding();

private:

	TSharedRef<SWidget> HandleAddTrackComboButtonGetMenuContent();
	
	void HandleAddTrackSubMenuNew(FMenuBuilder& AddTrackMenuBuilder, TArray<TArray<UProperty*>> KeyablePropertyPath);

	void HandleLabelsSubMenuCreate(FMenuBuilder& MenuBuilder);
	
	void HandlePropertyMenuItemExecute(TArray<UProperty*> PropertyPath);

private:

	/** The binding to live objects */
	FGuid ObjectBinding;

	/** The default display name of the object which is used if the binding manager doesn't provide one. */
	FText DefaultDisplayName;
};
