// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once
#include "DiffResults.h"

/** Used to find differences between revisions of a graph. */
class GRAPHEDITOR_API FGraphDiffControl
{
public:
	/** A struct to represent a found pair of nodes that match each other (for comparisons sake) */
	struct GRAPHEDITOR_API FNodeMatch
	{
		FNodeMatch()
			: NewNode(NULL)
			, OldNode(NULL)
		{}

		class UEdGraphNode* NewNode;
		class UEdGraphNode* OldNode;

		/**
		 * Looks at the two node members and compares them to see if there are any
		 * differences. If one of the nodes is NULL, then this will return true with
		 * a EDiffType::NODE_ADDED result.
		 * 
		 * @param  DiffsArrayOut	If supplied, this will be filled out with all the differences that were found.
		 * @return True if there were differences found, false if the two nodes are identical.
		 */
		bool Diff(TArray<FDiffSingleResult>* DiffsResultsOut = NULL) const;

		/**
		 * Checks to see if this is a valid match.
		 * @return False if NewNode or OldNode is NULL, true if both are valid.
		 */
		bool IsValid() const;
	};

	/**
	 * Looks through the supplied graph for a node that best matches the one specified.
	 * Sometimes (when diff'ing separate assets) there could be more than one possible
	 * match, so providing a list of already matched nodes helps us narrow it down (and 
	 * prevents us from matching one node with multiple others).
	 * 
	 * @param  Graph		The graph you want to search.
	 * @param  Node			The node you want to match.
	 * @param  PriorMatches	Previous made matches to exclude from our search.
	 * @return A pair of nodes (including the supplied one) that best match each other (one may be NULL if no match was found).
	 */
	static FNodeMatch FindNodeMatch(class UEdGraph* OldGraph, class UEdGraphNode* Node, TArray<FNodeMatch> const& PriorMatches);

	/**
	 * Looks for node differences between the two supplied graphs. Diffs will be
	 * returned in the context of "what has changed from OldGraph in NewGraph?" 
	 * 
	 * @param  OldGraph		The baseline graph to compare against.
	 * @param  NewGraph		The second graph to look for changes in.
	 * @param  DiffsOut		All the differences that were found between the two.
	 * @return True if any differences were found, false if both graphs are identical.
	 */
	static bool DiffGraphs(class UEdGraph* const OldGraph, class UEdGraph* const NewGraph, TArray<FDiffSingleResult>& DiffsOut);
};
