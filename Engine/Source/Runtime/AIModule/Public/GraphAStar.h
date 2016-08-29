// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

struct FGraphAStarDefaultPolicy
{
	static const int32 NodePoolSize = 64;
	static const int32 OpenSetSize = 64;
	static const int32 FatalPathLength = 10000;
	static const bool bReuseNodePoolInSubsequentSearches = false;
};

enum EGraphAStarResult
{
	SearchFail,
	SearchSuccess,
	GoalUnreachable,
	InfiniteLoop
};

/**
 *	Generic graph A* implementation
 *
 *	TGraph holds graph representation. Needs to implement two functions:
 *		int32 GetNeighbourCount(FNodeRef NodeRef) const;	- returns number of neighbours that the graph node identified with NodeRef has
 *		bool IsValidRef(FNodeRef NodeRef) const;			- returns whether given node identyfication is correct
 *	
 *	it also needs to specify node type 
 *		FNodeRef		- type used as identification of nodes in the graph
  *	
 *	TQueryFilter (FindPath's parameter) filter class is what decides which graph edges can be used and at what cost. It needs to implement following functions:
 *		float GetHeuristicScale() const;														- used as GetHeuristicCost's multiplier
 *		float GetHeuristicCost(const FNodeRef StartNodeRef, const FNodeRef EndNodeRef) const;	- estimate of cost from StartNodeRef to EndNodeRef
 *		float GetTraversalCost(const FNodeRef StartNodeRef, const FNodeRef EndNodeRef) const;	- real cost of traveling from StartNodeRef directly to EndNodeRef
 *		bool IsTraversalAllowed(const FNodeRef NodeA, const FNodeRef NodeB) const;				- whether traversing given edge is allowed
 *		bool WantsPartialSolution() const;														- whether to accept solutions that do not reach the goal
 *
 */
template<typename TGraph, typename Policy = FGraphAStarDefaultPolicy>
struct FGraphAStar
{
	typedef typename TGraph::FNodeRef FGraphNodeRef;

	struct FSearchNode
	{
		const FGraphNodeRef NodeRef;
		FGraphNodeRef ParentRef;
		float TraversalCost;
		float TotalCost;
		int32 SearchNodeIndex;
		int32 ParentNodeIndex;
		uint8 bIsOpened : 1;
		uint8 bIsClosed : 1;

		FSearchNode(const FGraphNodeRef& InNodeRef)
			: NodeRef(InNodeRef)
			, ParentRef(INDEX_NONE)
			, TraversalCost(FLT_MAX)
			, TotalCost(FLT_MAX)
			, SearchNodeIndex(INDEX_NONE)
			, ParentNodeIndex(INDEX_NONE)			
			, bIsOpened(false)
			, bIsClosed(false)
		{}

		FORCEINLINE void MarkOpened() { bIsOpened = true; }
		FORCEINLINE void MarkNotOpened() { bIsOpened = false; }
		FORCEINLINE void MarkClosed() { bIsClosed = true; }
		FORCEINLINE void MarkNotClosed() { bIsClosed = false; }
		FORCEINLINE bool IsOpened() const { return bIsOpened; }
		FORCEINLINE bool IsClosed() const { return bIsClosed; }
	};

	struct FNodeSorter
	{
		const TArray<FSearchNode>& NodePool;

		FNodeSorter(const TArray<FSearchNode>& InNodePool)
			: NodePool(InNodePool)
		{}

		bool operator()(const int32 A, const int32 B) const
		{
			return NodePool[A].TotalCost < NodePool[B].TotalCost;
		}
	};

	struct FNodePool : TArray<FSearchNode>
	{
		typedef TArray<FSearchNode> Super;
		TMap<FGraphNodeRef, int32> NodeMap;
		
		FNodePool()
			: Super()
		{
			Super::Reserve(Policy::NodePoolSize);
		}

		FORCEINLINE FSearchNode& Add(const FSearchNode& SearchNode)
		{
			const int32 Index = TArray<FSearchNode>::Add(SearchNode);
			NodeMap.Add(SearchNode.NodeRef, Index);
			FSearchNode& NewNode = (*this)[Index];
			NewNode.SearchNodeIndex = Index;
			return NewNode;
		}

		FORCEINLINE FSearchNode& Get(const FGraphNodeRef NodeRef)
		{
			const int32* IndexPtr = NodeMap.Find(NodeRef);
			return IndexPtr ? (*this)[*IndexPtr] : Add(NodeRef);
		}

		FORCEINLINE void Reset()
		{
			Super::Reset(Policy::NodePoolSize);
			NodeMap.Reset();
		}

		FORCEINLINE void ReinitNodes()
		{
			for (FSearchNode& Node : *this)
			{
				new (&Node) FSearchNode(Node.NodeRef);
			}
		}
	};

	struct FOpenList : TArray<int32>
	{
		typedef TArray<int32> Super;
		TArray<FSearchNode>& NodePool;
		const FNodeSorter& NodeSorter;

		FOpenList(TArray<FSearchNode>& InNodePool, const FNodeSorter& InNodeSorter)
			: NodePool(InNodePool), NodeSorter(InNodeSorter)
		{
			Super::Reserve(Policy::OpenSetSize);
		}

		void Push(FSearchNode& SearchNode)
		{
			HeapPush(SearchNode.SearchNodeIndex, NodeSorter);
			SearchNode.MarkOpened();
		}

		FSearchNode& Pop(bool bAllowShrinking = true)
		{
			int32 SearchNodeIndex = INDEX_NONE; 
			HeapPop(SearchNodeIndex, NodeSorter, /*bAllowShrinking = */false);
			NodePool[SearchNodeIndex].MarkNotOpened();
			return NodePool[SearchNodeIndex];
		}
	};

	const TGraph& Graph;
	FNodePool NodePool;
	FNodeSorter NodeSorter;
	FOpenList OpenList;

	FGraphAStar(const TGraph& InGraph)
		: Graph(InGraph), NodeSorter(FNodeSorter(NodePool)), OpenList(FOpenList(NodePool, NodeSorter))
	{
		NodePool.Reserve(Policy::NodePoolSize);
	}

	/** 
	 *	Performs the actual search.
	 *	@param [OUT] OutPath - on successful search contains a sequence of graph nodes representing 
	 *		solution optimal within given constraints
	 */
	template<typename TQueryFilter>
	EGraphAStarResult FindPath(const FGraphNodeRef StartNodeRef, const FGraphNodeRef EndNodeRef, const TQueryFilter& Filter, TArray<FGraphNodeRef>& OutPath)
	{
		if (!(Graph.IsValidRef(StartNodeRef) && Graph.IsValidRef(EndNodeRef)))
		{
			return SearchFail;
		}

		if (StartNodeRef == EndNodeRef)
		{
			return SearchSuccess;
		}

		const float HeuristicScale = Filter.GetHeuristicScale();

		if (Policy::bReuseNodePoolInSubsequentSearches)
		{
			NodePool.ReinitNodes();
		}
		else
		{
			NodePool.Reset();
		}
		OpenList.Reset();

		// kick off the search with the first node
		FSearchNode& StartNode = NodePool.Add(FSearchNode(StartNodeRef));
		StartNode.TraversalCost = 0;
		StartNode.TotalCost = Filter.GetHeuristicCost(StartNodeRef, EndNodeRef) * HeuristicScale;

		OpenList.Push(StartNode);

		int32 BestNodeIndex = StartNode.SearchNodeIndex;
		float BestNodeCost = StartNode.TotalCost;

		EGraphAStarResult Result = EGraphAStarResult::SearchSuccess;

		while (OpenList.Num() > 0)
		{
			// Pop next best node and put it on closed list
			FSearchNode& ConsideredNode = OpenList.Pop();
			ConsideredNode.MarkClosed();
			
			// We're there, store and move to result composition
			if (ConsideredNode.NodeRef == EndNodeRef)
			{
				BestNodeIndex = ConsideredNode.SearchNodeIndex;
				BestNodeCost = 0.f;
				break;
			}

			// consider every neighbor of BestNode
			for (int32 NeighbourNodeIndex = 0; NeighbourNodeIndex < Graph.GetNeighbourCount(ConsideredNode.NodeRef); ++NeighbourNodeIndex)
			{
				const FGraphNodeRef NeighbourRef = Graph.GetNeighbour(ConsideredNode.NodeRef, NeighbourNodeIndex);
				
				// validate and sanitize
				if (Graph.IsValidRef(NeighbourRef) == false
					|| NeighbourRef == ConsideredNode.ParentRef
					|| NeighbourRef == ConsideredNode.NodeRef
					|| Filter.IsTraversalAllowed(ConsideredNode.NodeRef, NeighbourRef) == false)
				{
					continue;
				}

				FSearchNode& NeighbourNode = NodePool.Get(NeighbourRef);

				// Calculate cost and heuristic.
				const float NewTraversalCost = Filter.GetTraversalCost(ConsideredNode.NodeRef, NeighbourNode.NodeRef) + ConsideredNode.TraversalCost;
				const float NewHeuristicCost = (NeighbourNode.NodeRef != EndNodeRef) 
					? (Filter.GetHeuristicCost(NeighbourNode.NodeRef, EndNodeRef) * HeuristicScale)
					: 0.f;
				const float NewTotalCost = NewTraversalCost + NewHeuristicCost;

				// check if this is better then the potential previous approach
				if (NewTotalCost >= NeighbourNode.TotalCost)
				{
					// if not, skip
					continue;
				}

				// fill in
				NeighbourNode.TraversalCost = NewTraversalCost;
				ensure(NewTraversalCost > 0);
				NeighbourNode.TotalCost = NewTotalCost;
				NeighbourNode.ParentRef = ConsideredNode.NodeRef;
				NeighbourNode.ParentNodeIndex = ConsideredNode.SearchNodeIndex;
				NeighbourNode.MarkNotClosed();

				if (NeighbourNode.IsOpened() == false)
				{
					OpenList.Push(NeighbourNode);
				}

				// In case there's no path let's store information on
				// "closest to goal" node
				// using Heuristic cost here rather than Traversal or Total cost
				// since this is what we'll care about if there's no solution - this node 
				// will be the one estimated-closest to the goal
				if (NewHeuristicCost < BestNodeCost)
				{
					BestNodeCost = NewHeuristicCost;
					BestNodeIndex = NeighbourNode.SearchNodeIndex;
				}
			}
		}

		// check if we've reached the goal
		if (BestNodeCost != 0.f)
		{
			Result = EGraphAStarResult::GoalUnreachable;
		}

		// no point to waste perf creating the path if querier doesn't want it
		if (Result == EGraphAStarResult::SearchSuccess || Filter.WantsPartialSolution())
		{
			// store the path. Note that it will be reversed!
			int32 SearchNodeIndex = BestNodeIndex;
			int32 PathLength = 1;
			do 
			{
				SearchNodeIndex = NodePool[SearchNodeIndex].ParentNodeIndex;
				++PathLength;
			} while (NodePool[SearchNodeIndex].NodeRef != StartNodeRef && ensure(PathLength < Policy::FatalPathLength));
			
			if (PathLength >= Policy::FatalPathLength)
			{
				Result = EGraphAStarResult::InfiniteLoop;
			}

			OutPath.Reset(PathLength);
			OutPath.AddZeroed(PathLength);

			// store the path
			SearchNodeIndex = BestNodeIndex;
			int32 ResultNodeIndex = PathLength - 1;
			do
			{
				OutPath[ResultNodeIndex--] = NodePool[SearchNodeIndex].NodeRef;
				SearchNodeIndex = NodePool[SearchNodeIndex].ParentNodeIndex;
			} while (ResultNodeIndex >= 0);
		}
		
		return Result;
	}
};
