// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

/** This is the type of action that occurred on a given graph */
enum EEdGraphActionType
{
	/** A default edit with no information occurred */
	GRAPHACTION_Default = 0x0,
	/** An action involving a user initiated action occurred on the graph */
	GRAPHACTION_UserInitiated = 0x1 << 0,

	/** A node was added to the graph */
	GRAPHACTION_AddNode = 0x1 << 1,
	/** A node was added to the graph at the users' request */
	GRAPHACTION_AddNodeUI = GRAPHACTION_AddNode | GRAPHACTION_UserInitiated,

	GRAPHACTION_SelectNode = 0x1 << 2,
};


/** Struct containing information about what actions occurred on the graph */
struct FEdGraphEditAction
{
	/** The action(s) that occurred */
	EEdGraphActionType Action;
	/** The graph the action occurred on */
	class UEdGraph* Graph;
	/** [Optional] The object the action occurred on */
	TSet<const class UEdGraphNode*> Nodes;

	FEdGraphEditAction()
		: Action(GRAPHACTION_Default)
		, Graph (NULL)
	{}

	explicit FEdGraphEditAction(EEdGraphActionType InAction, class UEdGraph* InGraph, class UEdGraphNode* InNode)
		: Action(InAction) 
		, Graph(InGraph)
	{
		Nodes.Add(InNode);
	}
};