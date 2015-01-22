// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


/** Type definition for shared pointers to instances of FDeviceProcessesProcessTreeNode. */
typedef TSharedPtr<class FDeviceProcessesProcessTreeNode> FDeviceProcessesProcessTreeNodePtr;

/** Type definition for shared references to instances of FDeviceProcessesProcessTreeNode. */
typedef TSharedRef<class FDeviceProcessesProcessTreeNode> FDeviceProcessesProcessTreeNodeRef;


/**
 * Implements a node for the process tree view.
 */
class FDeviceProcessesProcessTreeNode
{
public:

	/**
	 * Creates and initializes a new instance.
	 *
	 * @param InProcessInfo The node's process information.
	 */
	FDeviceProcessesProcessTreeNode( const FTargetDeviceProcessInfo& InProcessInfo )
		: ProcessInfo(InProcessInfo)
	{ }

public:

	/**
	 * Adds a child process node to this node.
	 *
	 * @param The child node to add.
	 */
	void AddChild( const FDeviceProcessesProcessTreeNodePtr& Child )
	{
		Children.Add(Child);
	}

	/**
	 * Clears the collection of child nodes.
	 */
	void ClearChildren( )
	{
		Children.Reset();
	}

	/**
	 * Gets the child nodes.
	 *
	 * @return Child nodes.
	 */
	const TArray<FDeviceProcessesProcessTreeNodePtr>& GetChildren( )
	{
		return Children;
	}

	/**
	 * Gets the parent node.
	 *
	 * @return Parent node.
	 */
	const FDeviceProcessesProcessTreeNodePtr& GetParent( )
	{
		return Parent;
	}

	/**
	 * Gets the node's process information.
	 *
	 * @return The process information.
	 */
	const FTargetDeviceProcessInfo& GetProcessInfo( ) const
	{
		return ProcessInfo;
	}

	/**
	 * Sets the parent node.
	 *
	 * @param Node The parent node to set.
	 */
	void SetParent( const FDeviceProcessesProcessTreeNodePtr& Node )
	{
		Parent = Node;
	}

private:

	// Holds the child process nodes
	TArray<FDeviceProcessesProcessTreeNodePtr> Children;

	// Holds a pointer to the parent node.
	FDeviceProcessesProcessTreeNodePtr Parent;

	// Holds the process information
	FTargetDeviceProcessInfo ProcessInfo;
};
