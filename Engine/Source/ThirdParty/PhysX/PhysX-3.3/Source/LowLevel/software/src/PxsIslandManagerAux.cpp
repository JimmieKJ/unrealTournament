/*
 * Copyright (c) 2008-2015, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.  

#include "PxsIslandManagerAux.h"

using namespace physx;

#include "PxsRigidBody.h"
#include "PxvDynamics.h"
#include "PxsArticulation.h"
#include "CmEventProfiler.h"
#include "PxProfileEventId.h"

#ifndef __SPU__
void EdgeChangeManager::cleanupEdgeEvents(PxI32* edgeEventWorkBuffer, const PxU32 entryCapacity)
{
	PX_UNUSED(entryCapacity);

	//
	//The same edge can contain duplicate and competing events. For example, if the user does a sequence of wakeUp and putToSleep calls without
	//every simulating or if the user puts an object to sleep which then gets hit by an awake object.
	//The following code removes duplicates and competing events.
	//

	const PxU32 oldBrokenEdgesSize = mBrokenEdgesSize;
	const PxU32 oldJoinedEdgesSize = mJoinedEdgesSize;
	PX_ASSERT(oldBrokenEdgesSize && oldJoinedEdgesSize);  // else do not call this method

	EdgeType* PX_RESTRICT brokenEdges = mBrokenEdges;
	EdgeType* PX_RESTRICT joinedEdges = mJoinedEdges;

	bool cleanupNeeded = false;
	for(PxU32 i=0; i < oldBrokenEdgesSize; i++)
	{
		const EdgeType edgeId = brokenEdges[i];
		PX_ASSERT(edgeId < entryCapacity);
		cleanupNeeded |= (edgeEventWorkBuffer[edgeId] != 0);
		edgeEventWorkBuffer[edgeId]--;
	}

	for(PxU32 i=0; i < oldJoinedEdgesSize; i++)
	{
		const EdgeType edgeId = joinedEdges[i];
		PX_ASSERT(edgeId < entryCapacity);
		cleanupNeeded |= (edgeEventWorkBuffer[edgeId] != 0);
		edgeEventWorkBuffer[edgeId]++;
	}

	if (cleanupNeeded)
	{
		//
		// edge event count:
		//  0: There were the same number of connect/unconnect events for the edge -> they cancel each other out, so ignore events for this edge
		//  1: There was one more connect event than unconnect event -> record one connect event
		// -1: There was one more unconnect event than connect event -> record one unconnect event
		//

		PxU32 newEdgeEventCount = 0;
		for(PxU32 i=0; i < oldBrokenEdgesSize; i++)
		{
			const EdgeType edgeId = brokenEdges[i];
			PX_ASSERT((edgeEventWorkBuffer[edgeId] <= 1) || (edgeEventWorkBuffer[edgeId] >= -1));

			if (edgeEventWorkBuffer[edgeId] < 0)
			{
				brokenEdges[newEdgeEventCount] = brokenEdges[i];
				newEdgeEventCount++;
				edgeEventWorkBuffer[edgeId] = 0;  // make sure this is the only event recorded for this edge
			}
		}
		mBrokenEdgesSize = newEdgeEventCount;

		newEdgeEventCount = 0;
		for(PxU32 i=0; i < oldJoinedEdgesSize; i++)
		{
			const EdgeType edgeId = joinedEdges[i];
			PX_ASSERT((edgeEventWorkBuffer[edgeId] <= 1) || (edgeEventWorkBuffer[edgeId] >= -1));

			if (edgeEventWorkBuffer[edgeId] > 0)
			{
				joinedEdges[newEdgeEventCount] = joinedEdges[i];
				newEdgeEventCount++;
				edgeEventWorkBuffer[edgeId] = 0;  // make sure this is the only event recorded for this edge
			}
		}
		mJoinedEdgesSize = newEdgeEventCount;
	}
}

void EdgeChangeManager::cleanupBrokenEdgeEvents(const Edge* allEdges)
{
	//
	//only applicable for second island gen pass because there we know that only unconnect events can make it through.
	//If an object did not have touch before activation and has touch after activation, then there will be an unconnect
	//(triggered by the activation) followed by a connect event (triggered by narrowphase finding a touch). Those events
	//should cancel each other out and the current state will be the same as the state before activation which, for second 
	//pass edges, means connected. Hence, this case can be detected by finding edges in the broken edge event list
	//which are connected.
	//

	if (mJoinedEdgesSize)  // else the problem can not occur
	{
		const Edge* PX_RESTRICT edges = allEdges;
		EdgeType* PX_RESTRICT brokenEdges = mBrokenEdges;
		const PxU32 oldBrokenEdgesSize = mBrokenEdgesSize;
		PxU32 newBrokenEdgesSize = 0;
		for(PxU32 i=0; i < oldBrokenEdgesSize; i++)
		{
			const EdgeType edgeId = brokenEdges[i];
			const Edge& edge = edges[edgeId];

			if (!edge.getIsConnected())
			{
				brokenEdges[newBrokenEdgesSize] = brokenEdges[i];
				newBrokenEdgesSize++;
			}
		}
		PX_ASSERT(mJoinedEdgesSize == (mBrokenEdgesSize - newBrokenEdgesSize));  //in the second island gen pass, there should be no join events apart from the ones described here

		mBrokenEdgesSize = newBrokenEdgesSize;

		mJoinedEdgesSize = 0;
	}
}
#endif

static PX_FORCE_INLINE void releaseIsland(const IslandType islandId, IslandManager& islands)
{
	islands.release(islandId);
	PX_ASSERT(islands.getBitmap().test(islandId));
	islands.getBitmap().reset(islandId);
}

static PX_FORCE_INLINE IslandType getNewIsland(IslandManager& islands)
{
	const IslandType newIslandId=islands.getAvailableElemNoResize();
	PX_ASSERT(!islands.getBitmap().test(newIslandId));
	islands.getBitmap().set(newIslandId);
	return newIslandId;
}

static PX_FORCE_INLINE void addEdgeToIsland
(const IslandType islandId, const EdgeType edgeId, 
 EdgeType* PX_RESTRICT nextEdgeIds, const PxU32 allEdgesCapacity, 
 IslandManager& islands)
{
	//Add the edge to the island.
	Island& island=islands.get(islandId);
	PX_ASSERT(edgeId<allEdgesCapacity);
	PX_ASSERT(edgeId!=island.mStartEdgeId);
	PX_UNUSED(allEdgesCapacity);

	nextEdgeIds[edgeId]=island.mStartEdgeId;
	island.mStartEdgeId=edgeId;
	island.mEndEdgeId=(INVALID_EDGE==island.mEndEdgeId ? edgeId : island.mEndEdgeId);
}

static PX_FORCE_INLINE void addNodeToIsland
(const IslandType islandId, const NodeType nodeId,
 Node* PX_RESTRICT allNodes, NodeType* PX_RESTRICT nextNodeIds, const PxU32 allNodesCapacity, 
 IslandManager& islands)
{
	PX_ASSERT(islands.getBitmap().test(islandId));
	PX_ASSERT(nodeId<allNodesCapacity);
	PX_ASSERT(!allNodes[nodeId].getIsDeleted());
	PX_UNUSED(allNodesCapacity);

	//Get the island and the node to be added to the island.
	Node& node=allNodes[nodeId];
	Island& island=islands.get(islandId);

	//Set the island of the node.
	node.setIslandId(islandId);

	//Connect the linked list of nodes
	PX_ASSERT(nodeId!=island.mStartNodeId);
	nextNodeIds[nodeId]=island.mStartNodeId;
	island.mStartNodeId=nodeId;
	island.mEndNodeId=(INVALID_NODE==island.mEndNodeId ? nodeId : island.mEndNodeId);
}

static PX_FORCE_INLINE void joinIslands
(const IslandType islandId1, const IslandType islandId2,
 Node* PX_RESTRICT /*allNodes*/, const PxU32 /*allNodesCapacity*/,
 Edge* PX_RESTRICT /*allEdges*/, const PxU32 /*allEdgesCapacity*/,
 NodeType* nextNodeIds, EdgeType* nextEdgeIds,
 IslandManager& islands)
{
	PX_ASSERT(islandId1!=islandId2);

	Island* PX_RESTRICT allIslands=islands.getAll();

	//Get both islands.
	PX_ASSERT(islands.getBitmap().test(islandId1));
	PX_ASSERT(islandId1<islands.getCapacity());
	Island& island1=allIslands[islandId1];
	PX_ASSERT(islands.getBitmap().test(islandId2));
	PX_ASSERT(islandId2<islands.getCapacity());
	Island& island2=allIslands[islandId2];

	//Join the list of edges together.
	//If Island2 has no edges then the list of edges is just the edges in island1 (so there is nothing to in this case).
	//If island1 has no edges then the list of edges is just the edges in island2.
	if(INVALID_EDGE==island1.mStartEdgeId)
	{
		//Island1 has no edges so the list of edges is just the edges in island 2.
		PX_ASSERT(INVALID_EDGE==island1.mEndEdgeId);
		island1.mStartEdgeId=island2.mStartEdgeId;
		island1.mEndEdgeId=island2.mEndEdgeId;
	}
	else if(INVALID_EDGE!=island2.mStartEdgeId)
	{
		//Both island1 and island2 have edges.
		PX_ASSERT(INVALID_EDGE!=island1.mStartEdgeId);
		PX_ASSERT(INVALID_EDGE!=island1.mEndEdgeId);
		PX_ASSERT(INVALID_EDGE!=island2.mStartEdgeId);
		PX_ASSERT(INVALID_EDGE!=island2.mEndEdgeId);
		PX_ASSERT(island1.mEndEdgeId!=island2.mStartEdgeId);
		nextEdgeIds[island1.mEndEdgeId]=island2.mStartEdgeId;
		island1.mEndEdgeId=island2.mEndEdgeId;
	}

	//Join the list of nodes together.
	//If Island2 has no nodes then the list of nodes is just the nodes in island1 (so there is nothing to in this case).
	//If island1 has no nodes then the list of nodes is just the nodes in island2.
	if(INVALID_NODE==island1.mStartNodeId)
	{
		//Island1 has no nodes so the list of nodes is just the nodes in island 2.
		PX_ASSERT(INVALID_NODE==island1.mEndNodeId);
		island1.mStartNodeId=island2.mStartNodeId;
		island1.mEndNodeId=island2.mEndNodeId;
	}
	else if(INVALID_NODE!=island2.mStartNodeId)
	{
		//Both island1 and island2 have nodes.
		PX_ASSERT(INVALID_NODE!=island1.mStartNodeId);
		PX_ASSERT(INVALID_NODE!=island1.mEndNodeId);
		PX_ASSERT(INVALID_NODE!=island2.mStartNodeId);
		PX_ASSERT(INVALID_NODE!=island2.mEndNodeId);
		PX_ASSERT(island2.mStartNodeId!=island1.mEndNodeId);
		nextNodeIds[island1.mEndNodeId]=island2.mStartNodeId;
		island1.mEndNodeId=island2.mEndNodeId;
	}

	//remove island 2
	releaseIsland(islandId2,islands);
}

static PX_FORCE_INLINE void setNodeIslandIdsAndJoinIslands
(const IslandType islandId1, const IslandType islandId2,
 Node* PX_RESTRICT allNodes, const PxU32 allNodesCapacity,
 Edge* PX_RESTRICT allEdges, const PxU32 allEdgesCapacity,
 NodeType* nextNodeIds, EdgeType* nextEdgeIds,
 IslandManager& islands)
{
	PX_ASSERT(islandId1!=islandId2);

	//Get island 2
	Island* PX_RESTRICT allIslands=islands.getAll();
	PX_ASSERT(islands.getBitmap().test(islandId2));
	PX_ASSERT(islandId2<islands.getCapacity());
	Island& island2=allIslands[islandId2];

	//Set all nodes in island 2 to be in island 1.
	NodeType nextNode=island2.mStartNodeId;
	while(nextNode!=INVALID_NODE)
	{
		PX_ASSERT(nextNode<allNodesCapacity);
		allNodes[nextNode].setIslandId(islandId1);
		nextNode=nextNodeIds[nextNode];
	}

	joinIslands(islandId1,islandId2,allNodes,allNodesCapacity,allEdges,allEdgesCapacity,nextNodeIds,nextEdgeIds,islands);
}

static void removeDeletedNodesFromIsland
(const IslandType islandId, NodeManager& nodeManager,  EdgeManager& /*edgeManager*/, IslandManager& islands)
{
	PX_ASSERT(islands.getBitmap().test(islandId));
	Island& island=islands.get(islandId);

	Node* PX_RESTRICT allNodes=nodeManager.getAll();
	NodeType* PX_RESTRICT nextNodeIds=nodeManager.getNextNodeIds();

	PX_ASSERT(INVALID_NODE==island.mStartNodeId || island.mStartNodeId!=nextNodeIds[island.mStartNodeId]);

	//If the start node has been deleted then keep 
	//updating until we find a start node that isn't deleted.
	{
		NodeType nextNode=island.mStartNodeId;
		while(nextNode!=INVALID_NODE && allNodes[nextNode].getIsDeleted())
		{
			const NodeType currNode=nextNode;
			nextNode=nextNodeIds[currNode];
			nextNodeIds[currNode]=INVALID_NODE;
		}
		island.mStartNodeId=nextNode;
	}

	//Remove deleted nodes from the linked list.
	{
		NodeType endNode=island.mStartNodeId;
		NodeType undeletedNode=island.mStartNodeId;
		while(undeletedNode!=INVALID_NODE)
		{
			PX_ASSERT(!allNodes[undeletedNode].getIsDeleted());

			NodeType nextNode=nextNodeIds[undeletedNode];
			while(nextNode!=INVALID_NODE && allNodes[nextNode].getIsDeleted())
			{
				const NodeType currNode=nextNode;
				nextNode=nextNodeIds[nextNode];
				nextNodeIds[currNode]=INVALID_NODE;
			}
			nextNodeIds[undeletedNode]=nextNode;
			endNode=undeletedNode;
			undeletedNode=nextNode;
		}
		island.mEndNodeId=endNode;
	}

	PX_ASSERT(INVALID_NODE==island.mStartNodeId || island.mStartNodeId!=nextNodeIds[island.mStartNodeId]);
}

static void removeDeletedNodesFromIslands2
(const IslandType* islandsToUpdate, const PxU32 numIslandsToUpdate,
 NodeManager& nodeManager,  EdgeManager& edgeManager, IslandManager& islands,
 Cm::BitMap& emptyIslandsBitmap)
{
	//Remove broken edges from affected islands.
	//Remove deleted nodes from affected islands.
	for(PxU32 i=0;i<numIslandsToUpdate;i++)
	{
		//Get the island.
		const IslandType islandId=islandsToUpdate[i];
		removeDeletedNodesFromIsland(islandId,nodeManager,edgeManager,islands);

		PX_ASSERT(islands.getBitmap().test(islandId));
		Island& island=islands.get(islandId);
		if(INVALID_NODE==island.mEndNodeId)
		{
			PX_ASSERT(INVALID_NODE==island.mStartNodeId);
			emptyIslandsBitmap.set(islandId);
		}
	}
}

static void removeDeletedNodesFromIslands
(const NodeType* deletedNodes, const PxU32 numDeletedNodes,
 NodeManager& nodeManager, EdgeManager& edgeManager, IslandManager& islands,
 Cm::BitMap& affectedIslandsBitmap, Cm::BitMap& emptyIslandsBitmap)
{
	Node* PX_RESTRICT allNodes=nodeManager.getAll();

	//Add to the list any islands that contain a deleted node.
	for(PxU32 i=0;i<numDeletedNodes;i++)
	{
		const NodeType nodeId=deletedNodes[i];
		PX_ASSERT(nodeId<nodeManager.getCapacity());
		const Node& node=allNodes[nodeId];
		//If a body was added after PxScene.simulate and before PxScene.fetchResults then 
		//the addition will be delayed and not processed until the end of fetchResults.
		//If this body is then released straight after PxScene.fetchResults then at the 
		//next PxScene.simulate we will have a body that has been both added and removed.
		//The removal must take precedence over the addition.
		if(node.getIsDeleted() && !node.getIsNew())
		{
			const IslandType islandId=node.getIslandId();
			PX_ASSERT(islandId!=INVALID_ISLAND);
			PX_ASSERT(islands.getBitmap().test(islandId));
			affectedIslandsBitmap.set(islandId);
		}
	}

#define MAX_NUM_ISLANDS_TO_UPDATE 1024

	//Gather a simple list of all islands affected by a deleted node.
	IslandType islandsToUpdate[MAX_NUM_ISLANDS_TO_UPDATE];
	PxU32 numIslandsToUpdate=0;
	const PxU32 lastSetBit = affectedIslandsBitmap.findLast();
	for(PxU32 w = 0; w <= lastSetBit >> 5; ++w)
	{
		for(PxU32 b = affectedIslandsBitmap.getWords()[w]; b; b &= b-1)
		{
			const IslandType islandId = (IslandType)(w<<5|Ps::lowestSetBit(b));
			PX_ASSERT(islands.getBitmap().test(islandId));

			if(numIslandsToUpdate<MAX_NUM_ISLANDS_TO_UPDATE)
			{
				islandsToUpdate[numIslandsToUpdate]=islandId;
				numIslandsToUpdate++;
			}
			else
			{
				removeDeletedNodesFromIslands2(islandsToUpdate,numIslandsToUpdate,nodeManager,edgeManager,islands,emptyIslandsBitmap);
				islandsToUpdate[0]=islandId;
				numIslandsToUpdate=1;
			}
		}
	}

	removeDeletedNodesFromIslands2(islandsToUpdate,numIslandsToUpdate,nodeManager,edgeManager,islands,emptyIslandsBitmap);
}

static void removeBrokenEdgesFromIslands2
(const IslandType* PX_RESTRICT islandsToUpdate, const PxU32 numIslandsToUpdate,
 NodeManager& /*nodeManager*/, EdgeManager& edgeManager, IslandManager& islands)
{
	Edge* PX_RESTRICT allEdges=edgeManager.getAll();
	EdgeType* PX_RESTRICT nextEdgeIds=edgeManager.getNextEdgeIds();

	//Remove broken edges from affected islands.
	//Remove deleted nodes from affected islands.
	for(PxU32 i=0;i<numIslandsToUpdate;i++)
	{
		//Get the island.
		const IslandType islandId=islandsToUpdate[i];
		PX_ASSERT(islands.getBitmap().test(islandId));
		Island& island=islands.get(islandId);

		PX_ASSERT(INVALID_EDGE==island.mStartEdgeId || island.mStartEdgeId!=nextEdgeIds[island.mStartEdgeId]);

		//If the start edge has been deleted then keep 
		//updating until we find a start edge that isn't deleted.
		{
			EdgeType nextEdge=island.mStartEdgeId;
			while(nextEdge!=INVALID_EDGE && !allEdges[nextEdge].getIsConnected())
			{
				const EdgeType currEdge=nextEdge;
				nextEdge=nextEdgeIds[currEdge];
				nextEdgeIds[currEdge]=INVALID_EDGE;
			}
			island.mStartEdgeId=nextEdge;
		}

		//Remove broken or deleted edges from the linked list.
		{
			EdgeType endEdge=island.mStartEdgeId;
			EdgeType undeletedEdge=island.mStartEdgeId;
			while(undeletedEdge!=INVALID_EDGE)
			{
				PX_ASSERT(allEdges[undeletedEdge].getIsConnected());

				EdgeType nextEdge=nextEdgeIds[undeletedEdge];
				while(nextEdge!=INVALID_EDGE && !allEdges[nextEdge].getIsConnected())
				{
					const EdgeType currEdge=nextEdge;
					nextEdge=nextEdgeIds[nextEdge];
					nextEdgeIds[currEdge]=INVALID_EDGE;
				}
				nextEdgeIds[undeletedEdge]=nextEdge;
				endEdge=undeletedEdge;
				undeletedEdge=nextEdge;
			}
			island.mEndEdgeId=endEdge;
		}

		PX_ASSERT(INVALID_EDGE==island.mStartEdgeId || island.mStartEdgeId!=nextEdgeIds[island.mStartEdgeId]);
	}
}

static void removeBrokenEdgesFromIslands
(const EdgeType* PX_RESTRICT brokenEdgeIds, const PxU32 numBrokenEdges, const EdgeType* PX_RESTRICT deletedEdgeIds, const PxU32 numDeletedEdges, 
 const NodeType* PX_RESTRICT kinematicProxySourceNodes, NodeManager& nodeManager, EdgeManager& edgeManager, IslandManager& islands,
 Cm::BitMap& brokenEdgeIslandsBitmap)
{
	Node* PX_RESTRICT allNodes=nodeManager.getAll();
	//const PxU32 allNodesCapacity=nodeManager.getCapacity();
	Edge* PX_RESTRICT allEdges=edgeManager.getAll();
	//const PxU32 allEdgesCapacity=edgeManager.getCapacity();

	//Gather a list of islands that contain a broken edge.
	for(PxU32 i=0;i<numBrokenEdges;i++)
	{
		//Get the edge that has just been broken and add it to the bitmap.
		const EdgeType edgeId=brokenEdgeIds[i];
		PX_ASSERT(edgeId<edgeManager.getCapacity());
		Edge& edge=allEdges[edgeId];

		//Check that the edge is legal.
		PX_ASSERT(!edge.getIsConnected());

		//Get the two nodes of the edge.
		//Get the two islands of the edge and add them to the bitmap.
		const NodeType nodeId1=edge.getNode1();
		const NodeType nodeId2=edge.getNode2();
		IslandType islandId1=INVALID_ISLAND;
		IslandType islandId2=INVALID_ISLAND;
		if(INVALID_NODE!=nodeId1)
		{
			PX_ASSERT(nodeId1<nodeManager.getCapacity());
			Node& node1=allNodes[nodeId1];
			islandId1=node1.getIslandId();
			if(INVALID_ISLAND!=islandId1)
			{
				brokenEdgeIslandsBitmap.set(islandId1);
			}

			if (kinematicProxySourceNodes && node1.getIsKinematic())
			{
				//in the second island pass, we need to map the nodes of kinematics from the proxy
				//back to the original because the brokeness prevents them from being processed in 
				//with the other proxies.
				PX_ASSERT(kinematicProxySourceNodes[nodeId1] != INVALID_NODE);
				edge.setNode1(kinematicProxySourceNodes[nodeId1]);
			}
		}
		if(INVALID_NODE!=nodeId2)
		{
			PX_ASSERT(nodeId2<nodeManager.getCapacity());
			Node& node2=allNodes[nodeId2];
			islandId2=node2.getIslandId();
			if(INVALID_ISLAND!=islandId2)
			{
				brokenEdgeIslandsBitmap.set(islandId2);
			}
			if (kinematicProxySourceNodes && node2.getIsKinematic())
			{
				//see comment in mirrored code above.
				PX_ASSERT(kinematicProxySourceNodes[nodeId2] != INVALID_NODE);
				edge.setNode2(kinematicProxySourceNodes[nodeId2]);
			}
		}
	}

	for(PxU32 i=0;i<numDeletedEdges;i++)
	{
		//Get the edge that has just been broken and add it to the bitmap.
		const EdgeType edgeId=deletedEdgeIds[i];
		PX_ASSERT(edgeId<edgeManager.getCapacity());
		Edge& edge=allEdges[edgeId];

		//If the edge was connected and was then deleted without having being disconnected
		//then we have to handle this as a broken edge.
		//The edge still has to be released so it can be reused.  This will be done at a later stage.
		if(edge.getIsConnected())
		{
			edge.setUnconnected();

			//Check that the edge is legal.
			PX_ASSERT(!edge.getIsConnected());

			//Get the two nodes of the edge.
			//Get the two islands of the edge and add them to the bitmap.
			const NodeType nodeId1=edge.getNode1();
			const NodeType nodeId2=edge.getNode2();
			IslandType islandId1=INVALID_ISLAND;
			IslandType islandId2=INVALID_ISLAND;
			if(INVALID_NODE!=nodeId1)
			{
				PX_ASSERT(nodeId1<nodeManager.getCapacity());
				islandId1=allNodes[nodeId1].getIslandId();
				if(INVALID_ISLAND!=islandId1)
				{
					brokenEdgeIslandsBitmap.set(islandId1);
				}
			}
			if(INVALID_NODE!=nodeId2)
			{
				PX_ASSERT(nodeId2<nodeManager.getCapacity());
				islandId2=allNodes[nodeId2].getIslandId();
				if(INVALID_ISLAND!=islandId2)
				{
					brokenEdgeIslandsBitmap.set(islandId2);
				}
			}
		}
	}

#define MAX_NUM_ISLANDS_TO_UPDATE 1024

	//Gather a simple list of all islands affected by broken edge or deleted node.
	IslandType islandsToUpdate[MAX_NUM_ISLANDS_TO_UPDATE];
	PxU32 numIslandsToUpdate=0;
	const PxU32 lastSetBit = brokenEdgeIslandsBitmap.findLast();
	for(PxU32 w = 0; w <= lastSetBit >> 5; ++w)
	{
		for(PxU32 b = brokenEdgeIslandsBitmap.getWords()[w]; b; b &= b-1)
		{
			const IslandType islandId = (IslandType)(w<<5|Ps::lowestSetBit(b));
			PX_ASSERT(islandId!=INVALID_ISLAND);
			PX_ASSERT(islands.getBitmap().test(islandId));

			if(numIslandsToUpdate<MAX_NUM_ISLANDS_TO_UPDATE)
			{
				islandsToUpdate[numIslandsToUpdate]=islandId;
				numIslandsToUpdate++;
			}
			else
			{
				removeBrokenEdgesFromIslands2(islandsToUpdate,numIslandsToUpdate,nodeManager,edgeManager,islands);
				islandsToUpdate[0]=islandId;
				numIslandsToUpdate=1;
			}
		}
	}

	removeBrokenEdgesFromIslands2(islandsToUpdate,numIslandsToUpdate,nodeManager,edgeManager,islands);
}

static void releaseEmptyIslands(const Cm::BitMap& emptyIslandsBitmap, IslandManager& islands, Cm::BitMap& brokenEdgeIslandsBitmap)
{
	const PxU32* PX_RESTRICT emptyIslandsBitmapWords=emptyIslandsBitmap.getWords();
	const PxU32 lastSetBit = emptyIslandsBitmap.findLast();
	for(PxU32 w = 0; w <= lastSetBit >> 5; ++w)
	{
		for(PxU32 b = emptyIslandsBitmapWords[w]; b; b &= b-1)
		{
			const IslandType islandId = (IslandType)(w<<5|Ps::lowestSetBit(b));
			PX_ASSERT(islands.getBitmap().test(islandId));
			PX_ASSERT(INVALID_NODE==islands.get(islandId).mStartNodeId);
			PX_ASSERT(INVALID_NODE==islands.get(islandId).mEndNodeId);
			PX_ASSERT(INVALID_EDGE==islands.get(islandId).mStartEdgeId);
			PX_ASSERT(INVALID_EDGE==islands.get(islandId).mEndEdgeId);
			releaseIsland(islandId,islands);
			brokenEdgeIslandsBitmap.reset(islandId);
			PX_ASSERT(!islands.getBitmap().test(islandId));
		}
	}
}


static void processJoinedEdges
(const EdgeType* PX_RESTRICT joinedEdges, const PxU32 numJoinedEdges, 
 NodeManager& nodeManager, EdgeManager& edgeManager, IslandManager& islands,
 Cm::BitMap& brokenEdgeIslandsBitmap, 
 Cm::BitMap& affectedIslandsBitmap,
 NodeType* PX_RESTRICT graphNextNodes, IslandType* PX_RESTRICT graphStartIslands, IslandType* PX_RESTRICT graphNextIslands)
{
	Node* PX_RESTRICT allNodes=nodeManager.getAll();
	const PxU32 allNodesCapacity=nodeManager.getCapacity();
	Edge* PX_RESTRICT allEdges=edgeManager.getAll();
	const PxU32 allEdgesCapacity=edgeManager.getCapacity();

	NodeType* PX_RESTRICT nextNodeIds=nodeManager.getNextNodeIds();
	EdgeType* PX_RESTRICT nextEdgeIds=edgeManager.getNextEdgeIds();

	PxMemSet(graphNextNodes, 0xff, sizeof(IslandType)*islands.getCapacity());
	PxMemSet(graphStartIslands, 0xff, sizeof(IslandType)*islands.getCapacity());
	PxMemSet(graphNextIslands, 0xff, sizeof(IslandType)*islands.getCapacity());

	//Add new nodes in a joined edge to an island on their own.
	//Record the bitmap of islands with nodes affected by joined edges.
	for(PxU32 i=0;i<numJoinedEdges;i++)
	{
		//Get the edge that has just been connected.
		const EdgeType edgeId=joinedEdges[i];
		PX_ASSERT(edgeId<allEdgesCapacity);
		const Edge& edge=allEdges[edgeId];

		if(!edge.getIsRemoved())
		{
			PX_ASSERT(edge.getIsConnected());

			const NodeType nodeId1=edge.getNode1();
			if(INVALID_NODE!=nodeId1)
			{
				Node&  node=allNodes[nodeId1];
				IslandType islandId=node.getIslandId();
				if(INVALID_ISLAND!=islandId)
				{
					affectedIslandsBitmap.set(islandId);
				}
				else
				{
					PX_ASSERT(node.getIsNew());
					islandId=getNewIsland(islands);
					addNodeToIsland(islandId,nodeId1,allNodes,nextNodeIds,allNodesCapacity,islands);
					affectedIslandsBitmap.set(islandId);
				}
			}

			const NodeType nodeId2=edge.getNode2();
			if(INVALID_NODE!=nodeId2)
			{
				Node&  node=allNodes[nodeId2];
				IslandType islandId=node.getIslandId();
				if(INVALID_ISLAND!=islandId)
				{
					affectedIslandsBitmap.set(islandId);
				}
				else
				{
					PX_ASSERT(node.getIsNew());
					islandId=getNewIsland(islands);
					addNodeToIsland(islandId,nodeId2,allNodes,nextNodeIds,allNodesCapacity,islands);
					affectedIslandsBitmap.set(islandId);
				}
			}
		}
	}

	//Iterate over all islands affected by a node in a joined edge.
	//Record all affected nodes and the start island of all affected nodes. 
	NodeType startNode=INVALID_NODE;
	IslandType prevIslandId=INVALID_ISLAND;
	const PxU32 lastSetBit = affectedIslandsBitmap.findLast();
	for(PxU32 w = 0; w <= lastSetBit >> 5; ++w)
	{
		for(PxU32 b = affectedIslandsBitmap.getWords()[w]; b; b &= b-1)
		{
			const IslandType islandId = (IslandType)(w<<5|Ps::lowestSetBit(b));
			PX_ASSERT(islandId<islands.getCapacity());
			PX_ASSERT(islands.getBitmap().test(islandId));
			const Island& island=islands.get(islandId);

			NodeType nextNode=island.mStartNodeId;
			PX_ASSERT(INVALID_NODE!=nextNode);
			if(INVALID_NODE!=prevIslandId)
			{
				const Island& prevIsland=islands.get(prevIslandId);
				graphNextNodes[prevIsland.mEndNodeId]=island.mStartNodeId;
			}
			else
			{
				startNode=nextNode;
			}
			prevIslandId=islandId;

			while(INVALID_NODE!=nextNode)
			{
				const Node& node=allNodes[nextNode];
				const IslandType islandId1=node.getIslandId();
				PX_ASSERT(islandId1!=INVALID_ISLAND);
				graphNextNodes[nextNode]=nextNodeIds[nextNode];
				graphStartIslands[nextNode]=islandId1;
				graphNextIslands[islandId1]=INVALID_ISLAND;
				nextNode=nextNodeIds[nextNode];
			}
		}
	}

	//Join all the edges by merging islands.
	for(PxU32 i=0;i<numJoinedEdges;i++)
	{
		//Get the edge that has just been connected.
		const EdgeType edgeId=joinedEdges[i];
		PX_ASSERT(edgeId<allEdgesCapacity);
		const Edge& edge=allEdges[edgeId];

		if(!edge.getIsRemoved())
		{
			PX_ASSERT(edge.getIsConnected());

			//Get the two islands of the edge nodes.

			IslandType islandId1=INVALID_ISLAND;
			{
				const NodeType nodeId1=edge.getNode1();
				if(INVALID_NODE!=nodeId1)
				{
					PX_ASSERT(nodeId1<allNodesCapacity);
					IslandType nextIsland=graphStartIslands[nodeId1];
					while(INVALID_ISLAND!=nextIsland)
					{
						islandId1=nextIsland;
						nextIsland=graphNextIslands[nextIsland];
					}
				}
			}

			IslandType islandId2=INVALID_ISLAND;
			{
				const NodeType nodeId2=edge.getNode2();
				if(INVALID_NODE!=nodeId2)
				{
					PX_ASSERT(nodeId2<allNodesCapacity);
					IslandType nextIsland=graphStartIslands[nodeId2];
					while(INVALID_ISLAND!=nextIsland)
					{
						islandId2=nextIsland;
						nextIsland=graphNextIslands[nextIsland];
					}
				}
			}

			if(INVALID_ISLAND!=islandId1 && INVALID_ISLAND!=islandId2)
			{
				addEdgeToIsland(islandId1,edgeId,nextEdgeIds,allEdgesCapacity,islands);
				if(islandId1!=islandId2)
				{
					graphNextIslands[islandId2]=islandId1;

					PX_ASSERT(islands.getBitmap().test(islandId1));
					PX_ASSERT(islands.getBitmap().test(islandId2));
					joinIslands(islandId1,islandId2,allNodes,allNodesCapacity,allEdges,allEdgesCapacity,nextNodeIds,nextEdgeIds,islands);
					PX_ASSERT(islands.getBitmap().test(islandId1));
					PX_ASSERT(!islands.getBitmap().test(islandId2));
					if(brokenEdgeIslandsBitmap.test(islandId2))
					{
						brokenEdgeIslandsBitmap.set(islandId1);
						brokenEdgeIslandsBitmap.reset(islandId2);
					}
				}
			}
			else if(INVALID_ISLAND==islandId1 && INVALID_ISLAND!=islandId2)
			{
				addEdgeToIsland(islandId2,edgeId,nextEdgeIds,allEdgesCapacity,islands);
			}
			else if(INVALID_ISLAND!=islandId1 && INVALID_ISLAND==islandId2)
			{
				addEdgeToIsland(islandId1,edgeId,nextEdgeIds,allEdgesCapacity,islands);
			}
			else
			{
				PX_ASSERT(false);
			}
		}
	}

	//Set the island of all nodes in an island affected by a joined edge.
	NodeType nextNode=startNode;
	while(INVALID_NODE!=nextNode)
	{
		IslandType islandId=INVALID_ISLAND;
		IslandType nextIsland=graphStartIslands[nextNode];
		while(INVALID_ISLAND!=nextIsland)
		{
			islandId=nextIsland;
			nextIsland=graphNextIslands[nextIsland];
		}
		Node& node=allNodes[nextNode];
		node.setIslandId(islandId);
		nextNode=graphNextNodes[nextNode];
	}
}


PX_FORCE_INLINE PxU32 alignSize16(const PxU32 size)
{
	return ((size + 15) & ~15);
}

#ifdef PX_DEBUG
NodeType getLastNode(NodeType* nextNodes, const NodeType nodeId)
{
	NodeType mapLast = nodeId;
	NodeType mapNext = nextNodes[nodeId];
	while (mapNext != INVALID_NODE)
	{
		mapLast = mapNext;
		mapNext = nextNodes[mapNext];
	}
	return mapLast;
}
#endif

static void duplicateKinematicNodes
(const Cm::BitMap& kinematicNodesBitmap,
 NodeManager& nodeManager, EdgeManager& edgeManager, IslandManager& islands,
 NodeType* kinematicProxySourceNodes, NodeType* kinematicProxyNextNodes,  NodeType* kinematicProxyLastNodes,
 Cm::BitMap& kinematicIslandsBitmap)
{
	Node* PX_RESTRICT allNodes=nodeManager.getAll();
	const PxU32 allNodesCapacity=nodeManager.getCapacity();
	NodeType* PX_RESTRICT nextNodeIds=nodeManager.getNextNodeIds();

	Edge* PX_RESTRICT allEdges=edgeManager.getAll();
	EdgeType* PX_RESTRICT nextEdgeIds=edgeManager.getNextEdgeIds();

	PxMemSet(kinematicProxySourceNodes, 0xff, sizeof(NodeType)*allNodesCapacity);
	PxMemSet(kinematicProxyNextNodes, 0xff, sizeof(NodeType)*allNodesCapacity);
	PxMemSet(kinematicProxyLastNodes, 0xff, sizeof(NodeType)*allNodesCapacity);

	//Compute all the islands that need updated.
	//Set all kinematic nodes as deleted so they can be identified as needing removed from their islands.
	{
		const PxU32* PX_RESTRICT kinematicNodesBitmapWords=kinematicNodesBitmap.getWords();
		const PxU32 lastSetBit = kinematicNodesBitmap.findLast();
		for(PxU32 w = 0; w <= lastSetBit >> 5; ++w)
		{
			for(PxU32 b = kinematicNodesBitmapWords[w]; b; b &= b-1)
			{
				const NodeType kinematicNodeId = (NodeType)(w<<5|Ps::lowestSetBit(b));
				PX_ASSERT(kinematicNodeId<allNodesCapacity);
				Node& node=allNodes[kinematicNodeId];
				PX_ASSERT(node.getIsKinematic());

				//Get the island of the kinematic node and mark it as needing updated.
				const IslandType islandId=node.getIslandId();
				PX_ASSERT(INVALID_ISLAND!=islandId);
				PX_ASSERT(islands.getBitmap().test(islandId));
				kinematicIslandsBitmap.set(islandId);

				//Set the node as deleted so it can be identified as needing to be removed.
				node.setIsDeleted();
			}
		}
	}

	//Remove all kinematics nodes from the islands.
	//Replace kinematics with proxies from each edge referencing each kinematic.
	{
		const PxU32 lastSetBit = kinematicIslandsBitmap.findLast();
		for(PxU32 w = 0; w <= lastSetBit >> 5; ++w)
		{
			for(PxU32 b = kinematicIslandsBitmap.getWords()[w]; b; b &= b-1)
			{
				const IslandType islandId = (IslandType)(w<<5|Ps::lowestSetBit(b));
				PX_ASSERT(islandId<islands.getCapacity());

				//Get the island.
				Island& island=islands.get(islandId);

				//Remove all kinematics from the island so that we can replace them with proxy nodes.
				removeDeletedNodesFromIsland(islandId,nodeManager,edgeManager,islands);
				
				//Create a proxy kinematic node for each edge that references a kinematic.
				EdgeType nextEdge=island.mStartEdgeId;
				while(nextEdge!=INVALID_EDGE)
				{
					//Get the current edge.
					PX_ASSERT(nextEdge<edgeManager.getCapacity());
					Edge& edge=allEdges[nextEdge];

					//Check the edge is legal.
					PX_ASSERT(edge.getIsConnected());
					PX_ASSERT(!edge.getIsRemoved());
					PX_ASSERT(edge.getNode1()!=INVALID_NODE || edge.getNode2()!=INVALID_NODE);

					//Add a proxy node for node1 if it is a kinematic.
					const NodeType nodeId1=edge.getNode1();
					if(INVALID_NODE!=nodeId1)
					{
						PX_ASSERT(nodeId1<allNodesCapacity);
						Node& node1=allNodes[nodeId1];
						if(node1.getIsKinematic())
						{
							//Add a proxy node.
							const NodeType proxyNodeId=nodeManager.getAvailableElemNoResize();
							kinematicProxySourceNodes[proxyNodeId]=nodeId1;

							//Add it to the list of proxies for the source kinematic.
							NodeType mapLast = nodeId1;
							if(INVALID_NODE != kinematicProxyLastNodes[nodeId1])
							{
								mapLast = kinematicProxyLastNodes[nodeId1];
							}
							PX_ASSERT(getLastNode(kinematicProxyNextNodes, nodeId1) == mapLast);
							kinematicProxyNextNodes[mapLast]=proxyNodeId;
							kinematicProxyNextNodes[proxyNodeId]=INVALID_NODE;
							kinematicProxyLastNodes[nodeId1] = proxyNodeId;
							PX_ASSERT(getLastNode(kinematicProxyNextNodes, nodeId1) == kinematicProxyLastNodes[nodeId1]);

							//Set up the proxy node.
							//Don't copy the deleted flag across because that was artificially added to help remove the node from the island.
							Node& proxyNode=allNodes[proxyNodeId];
							proxyNode.setRigidBodyOwner(node1.getRigidBodyOwner());
							PX_ASSERT(node1.getIsDeleted());
							proxyNode.setFlags(PxU8(node1.getFlags() & ~Node::eDELETED));
							PX_ASSERT(!proxyNode.getIsDeleted());

							//Add the proxy to the island.
							addNodeToIsland(islandId,proxyNodeId,allNodes,nextNodeIds,allNodesCapacity,islands);
							PX_ASSERT(proxyNode.getIslandId()==islandId);

							//Set the edge to reference the proxy.
							edge.setNode1(proxyNodeId);
						}
					}

					//Add a proxy node for node2 if it is a kinematic.
					const NodeType nodeId2=edge.getNode2();
					if(INVALID_NODE!=nodeId2)
					{
						PX_ASSERT(nodeId2<allNodesCapacity);
						Node& node2=allNodes[nodeId2];
						if(node2.getIsKinematic())
						{
							//Add a proxy node.
							const NodeType proxyNodeId=nodeManager.getAvailableElemNoResize();
							kinematicProxySourceNodes[proxyNodeId]=nodeId2;

							//Add it to the list of proxies for the source kinematic.
							NodeType mapLast = nodeId2;
							if(INVALID_NODE != kinematicProxyLastNodes[nodeId2])
							{
								mapLast = kinematicProxyLastNodes[nodeId2];
							}
							PX_ASSERT(getLastNode(kinematicProxyNextNodes, nodeId2) == mapLast);
							kinematicProxyNextNodes[mapLast]=proxyNodeId;
							kinematicProxyNextNodes[proxyNodeId]=INVALID_NODE;
							kinematicProxyLastNodes[nodeId2] = proxyNodeId;
							PX_ASSERT(getLastNode(kinematicProxyNextNodes, nodeId2) == kinematicProxyLastNodes[nodeId2]);

							//Set up the proxy node.
							//Don't copy the deleted flag across because that was artificially added to help remove the node from the island.
							Node& proxyNode=allNodes[proxyNodeId];
							proxyNode.setRigidBodyOwner(node2.getRigidBodyOwner());
							PX_ASSERT(node2.getIsDeleted());
							proxyNode.setFlags(PxU8(node2.getFlags() & ~Node::eDELETED));
							PX_ASSERT(!proxyNode.getIsDeleted());

							//Add the proxy to the island.
							addNodeToIsland(islandId,proxyNodeId,allNodes,nextNodeIds,allNodesCapacity,islands);
							PX_ASSERT(proxyNode.getIslandId()==islandId);

							//Set the edge to reference the proxy.
							edge.setNode2(proxyNodeId);
						}
					}
					
					nextEdge=nextEdgeIds[nextEdge];
				}
			}
		}
	}

	//Kinematics were flagged as deleted to identify them as needing removed from the island
	//so we nos need to clear the kinematic flag.  Take care that if a kinematic was referenced by 
	//no edges then it will have been removed flagged as deleted and then removed above but not 
	//replaced by anything.  Just put lone kinematics back in their original island.
	{
		const PxU32* PX_RESTRICT kinematicNodesBitmapWords=kinematicNodesBitmap.getWords();
		const PxU32 lastSetBit = kinematicNodesBitmap.findLast();
		for(PxU32 w = 0; w <= lastSetBit >> 5; ++w)
		{
			for(PxU32 b = kinematicNodesBitmapWords[w]; b; b &= b-1)
			{
				const NodeType kinematicNodeId = (NodeType)(w<<5|Ps::lowestSetBit(b));
				PX_ASSERT(kinematicNodeId<allNodesCapacity);
				Node& node=allNodes[kinematicNodeId];
				PX_ASSERT(node.getIsKinematic());
				PX_ASSERT(node.getIsDeleted());
				PX_ASSERT(node.getIslandId()!=INVALID_ISLAND);
				PX_ASSERT(node.getIslandId()<islands.getCapacity());
				node.clearIsDeleted();

				//If the kinematic has no edges referencing it put it back in its own island.
				//If the kinematic has edges referencing it makes sense to flag the kinematic as having been replaced with proxies.
				if(INVALID_NODE==kinematicProxyNextNodes[kinematicNodeId])
				{
					const IslandType islandId=node.getIslandId();
					PX_ASSERT(INVALID_ISLAND!=islandId);
					addNodeToIsland(islandId,kinematicNodeId,allNodes,nextNodeIds,allNodesCapacity,islands);
				}
				else
				{
					//The node has been replaced with proxies so we can reset the node to neutral for now.
					node.setIslandId(INVALID_ISLAND);
				}
			}
		}
	}
}

static void processBrokenEdgeIslands2
(const IslandType* islandsToUpdate, const PxU32 numIslandsToUpdate,
 NodeManager& nodeManager, EdgeManager& edgeManager, IslandManager& islands,
 NodeType* graphNextNodes, IslandType* graphStartIslands, IslandType* graphNextIslands,
 Cm::BitMap* affectedIslandsBitmap)
{
	Node* PX_RESTRICT allNodes=nodeManager.getAll();
	const PxU32 allNodesCapacity=nodeManager.getCapacity();
	Edge* PX_RESTRICT allEdges=edgeManager.getAll();
	const PxU32 allEdgesCapacity=edgeManager.getCapacity();
	NodeType* PX_RESTRICT nextNodeIds=nodeManager.getNextNodeIds();
	EdgeType* PX_RESTRICT nextEdgeIds=edgeManager.getNextEdgeIds();

	PxMemSet(graphNextNodes, 0xff, sizeof(IslandType)*islands.getCapacity());
	PxMemSet(graphStartIslands, 0xff, sizeof(IslandType)*islands.getCapacity());
	PxMemSet(graphNextIslands, 0xff, sizeof(IslandType)*islands.getCapacity());

	for(PxU32 i=0;i<numIslandsToUpdate;i++)
	{
		//Get the island.
		const IslandType islandId0=islandsToUpdate[i];
		PX_ASSERT(islands.getBitmap().test(islandId0));
		const Island island0=islands.get(islandId0);
		const NodeType startNode=island0.mStartNodeId;
		const EdgeType startEdge=island0.mStartEdgeId;
		releaseIsland(islandId0,islands);

		//Create a dummy island for each node with a single node per island.
		NodeType nextNode=startNode;
		while(nextNode!=INVALID_NODE)
		{
			const IslandType newIslandId=getNewIsland(islands);
			graphNextNodes[nextNode]=nextNodeIds[nextNode];
			graphStartIslands[nextNode]=newIslandId;
			graphNextIslands[newIslandId]=INVALID_ISLAND;
			nextNode=nextNodeIds[nextNode];
		}

		//Add all the edges.
		EdgeType nextEdge=startEdge;
		while(nextEdge!=INVALID_EDGE)
		{
			const EdgeType currEdge=nextEdge;
			nextEdge=nextEdgeIds[nextEdge];

			//Get the current edge.
			PX_ASSERT(currEdge<allEdgesCapacity);
			Edge& edge=allEdges[currEdge];

			//Check the edge is legal.
			PX_ASSERT(edge.getIsConnected());
			PX_ASSERT(!edge.getIsRemoved());
			PX_ASSERT(edge.getNode1()!=INVALID_NODE || edge.getNode2()!=INVALID_NODE);

			//Get the two nodes of the edge.
			//Get the two islands of the edge.
			const NodeType nodeId1=edge.getNode1();
			const NodeType nodeId2=edge.getNode2();
			IslandType islandId1=INVALID_ISLAND;
			IslandType islandId2=INVALID_ISLAND;
			PxU32 depth1=0;
			PxU32 depth2=0;
			if(INVALID_NODE!=nodeId1)
			{
				PX_ASSERT(nodeId1<allNodesCapacity);
				IslandType nextIsland=graphStartIslands[nodeId1];
				PX_ASSERT(nextIsland!=INVALID_ISLAND);
				while(nextIsland!=INVALID_ISLAND)
				{
					islandId1=nextIsland;
					nextIsland=graphNextIslands[nextIsland];
					depth1++;
				}
			}
			if(INVALID_NODE!=nodeId2)
			{
				PX_ASSERT(nodeId2<allNodesCapacity);
				IslandType nextIsland=graphStartIslands[nodeId2];
				PX_ASSERT(nextIsland!=INVALID_ISLAND);
				while(nextIsland!=INVALID_ISLAND)
				{
					islandId2=nextIsland;
					nextIsland=graphNextIslands[nextIsland];
					depth2++;
				}
			}

			//Set island2 to be joined to island 1.
			if(INVALID_ISLAND!=islandId1 && INVALID_ISLAND!=islandId2)
			{
				if(islandId1!=islandId2)
				{
					if(depth1<depth2)
					{
						graphNextIslands[islandId1]=islandId2;
					}
					else
					{
						graphNextIslands[islandId2]=islandId1;
					}
				}
			}
			else if(INVALID_ISLAND==islandId1 && INVALID_ISLAND!=islandId2)
			{
				//Node2 is already in island 2.
				//Nothing to do.
			}
			else if(INVALID_ISLAND!=islandId1 && INVALID_ISLAND==islandId2)
			{
				//Node1 is already in island 1.
				//Nothing to do.
			}
			else
			{
				PX_ASSERT(false);
			}
		}

		//Go over all the nodes and add them to their islands.
		nextNode=startNode;
		while(nextNode!=INVALID_NODE)
		{
			IslandType islandId=graphStartIslands[nextNode];
			IslandType nextIsland=graphStartIslands[nextNode];
			PX_ASSERT(nextIsland!=INVALID_ISLAND);
			while(nextIsland!=INVALID_ISLAND)
			{
				islandId=nextIsland;
				nextIsland=graphNextIslands[nextIsland];
			}

			//Add the node to the island.
			//Island& island=islands.get(islandId);
			addNodeToIsland(islandId,nextNode,allNodes,nextNodeIds,allNodesCapacity,islands);

			//Next node.
			nextNode=graphNextNodes[nextNode];
		}

		//Release all empty islands.
		nextNode=startNode;
		while(nextNode!=INVALID_NODE)
		{
			const IslandType islandId=graphStartIslands[nextNode];
			Island& island=islands.get(islandId);
			if(INVALID_NODE==island.mStartNodeId)
			{
				PX_ASSERT(INVALID_NODE==island.mStartNodeId);
				PX_ASSERT(INVALID_NODE==island.mEndNodeId);
				PX_ASSERT(INVALID_EDGE==island.mStartEdgeId);
				PX_ASSERT(INVALID_EDGE==island.mEndEdgeId);
				releaseIsland(islandId,islands);

				if (affectedIslandsBitmap)
					affectedIslandsBitmap->reset(islandId);  // remove from list of islands to process
			}
			else if (affectedIslandsBitmap)
			{
				// for second pass island gen, we have a list of islands to process. If an island gets split up, we need
				// to add the subislands to the list as well
				affectedIslandsBitmap->set(islandId);
			}

			//Next node.
			nextNode=graphNextNodes[nextNode];
		}

		//Now add all the edges to the islands.
		nextEdge=island0.mStartEdgeId;
		while(nextEdge!=INVALID_EDGE)
		{
			const EdgeType currEdge=nextEdge;
			nextEdge=nextEdgeIds[nextEdge];

			//Get the current edge.
			PX_ASSERT(currEdge<allEdgesCapacity);
			Edge& edge=allEdges[currEdge];

			//Check the edge is legal.
			PX_ASSERT(edge.getIsConnected());
			PX_ASSERT(!edge.getIsRemoved());
			PX_ASSERT(edge.getNode1()!=INVALID_NODE || edge.getNode2()!=INVALID_NODE);

			//Get the two nodes of the edge.
			//Get the two islands of the edge.
			const NodeType nodeId1=edge.getNode1();
			const NodeType nodeId2=edge.getNode2();
			PX_ASSERT(INVALID_NODE!=nodeId1 || INVALID_NODE!=nodeId2);
			if(INVALID_NODE!=nodeId1)
			{
				PX_ASSERT(nodeId1<allNodesCapacity);
				const IslandType islandId1=allNodes[nodeId1].getIslandId();
				PX_ASSERT(INVALID_ISLAND!=islandId1);
				PX_ASSERT(INVALID_NODE==nodeId2 || allNodes[nodeId2].getIslandId()==islandId1);
				addEdgeToIsland(islandId1,currEdge,nextEdgeIds,allEdgesCapacity,islands);
			}
			else if(INVALID_NODE!=nodeId2)
			{
				PX_ASSERT(nodeId2<allNodesCapacity);
				const IslandType islandId2=allNodes[nodeId2].getIslandId();
				PX_ASSERT(INVALID_ISLAND!=islandId2);
				PX_ASSERT(INVALID_NODE==nodeId1 || allNodes[nodeId1].getIslandId()==islandId2);
				addEdgeToIsland(islandId2,currEdge,nextEdgeIds,allEdgesCapacity,islands);
			}
		}
	}
}

static void processBrokenEdgeIslands
(const Cm::BitMap& brokenEdgeIslandsBitmap,
 NodeManager& nodeManager, EdgeManager& edgeManager, IslandManager& islands,
 NodeType* graphNextNodes, IslandType* graphStartIslands, IslandType* graphNextIslands,
 Cm::BitMap* affectedIslandsBitmap)
{
#define MAX_NUM_ISLANDS_TO_UPDATE 1024

	//Gather a simple list of all islands affected by broken edge or deleted node.
	IslandType islandsToUpdate[MAX_NUM_ISLANDS_TO_UPDATE];
	PxU32 numIslandsToUpdate=0;
	const PxU32 lastSetBit = brokenEdgeIslandsBitmap.findLast();
	for(PxU32 w = 0; w <= lastSetBit >> 5; ++w)
	{
		for(PxU32 b = brokenEdgeIslandsBitmap.getWords()[w]; b; b &= b-1)
		{
			const IslandType islandId = (IslandType)(w<<5|Ps::lowestSetBit(b));

			if(islands.getBitmap().test(islandId))
			{
				if(numIslandsToUpdate<MAX_NUM_ISLANDS_TO_UPDATE)
				{
					islandsToUpdate[numIslandsToUpdate]=islandId;
					numIslandsToUpdate++;
				}
				else
				{
					processBrokenEdgeIslands2(islandsToUpdate,numIslandsToUpdate,nodeManager,edgeManager,islands,graphNextNodes,graphStartIslands,graphNextIslands,
												affectedIslandsBitmap);
					islandsToUpdate[0]=islandId;
					numIslandsToUpdate=1;
				}
			}
		}
	}

	processBrokenEdgeIslands2(islandsToUpdate,numIslandsToUpdate,nodeManager,edgeManager,islands,graphNextNodes,graphStartIslands,graphNextIslands,
								affectedIslandsBitmap);
}

static void releaseDeletedEdges(const EdgeType* PX_RESTRICT deletedEdges, const PxU32 numDeletedEdges, EdgeManager& edgeManager)
{
	//Now release the deleted edges.
	for(PxU32 i=0;i<numDeletedEdges;i++)
	{
		//Get the deleted edge.
		const EdgeType edgeId=deletedEdges[i];

		//Test the edge is legal.
		PX_ASSERT(edgeManager.get(edgeId).getIsRemoved());

		//Release the edge.
		edgeManager.release(edgeId);
	}
}

static void processCreatedNodes
(const NodeType* PX_RESTRICT createdNodes, const PxU32 numCreatedNodes, NodeManager& nodeManager, IslandManager& islands)
{
	Node* PX_RESTRICT allNodes=nodeManager.getAll();
	const PxU32 allNodesCapacity=nodeManager.getCapacity();
	NodeType* PX_RESTRICT nextNodeIds=nodeManager.getNextNodeIds();

	for(PxU32 i=0;i<numCreatedNodes;i++)
	{
		const NodeType nodeId=createdNodes[i];
		PX_ASSERT(nodeId<allNodesCapacity);
		Node& node=allNodes[nodeId];
		node.clearIsNew();

		//If a body was added after PxScene.simulate and before PxScene.fetchResults then 
		//the addition will be delayed and not processed until the end of fetchResults.
		//If this body is then released straight after PxScene.fetchResults then at the 
		//next PxScene.simulate we will have a body that has been both added and removed.
		//The removal must take precedence over the addition.
		if(!node.getIsDeleted() && node.getIslandId()==INVALID_ISLAND)
		{
			const IslandType islandId=getNewIsland(islands);
			PX_ASSERT(islands.getCapacity()<=allNodesCapacity);
			addNodeToIsland(islandId,nodeId,allNodes,nextNodeIds,allNodesCapacity,islands);
		}
	}
}

static void releaseDeletedNodes(const NodeType* PX_RESTRICT deletedNodes, const PxU32 numDeletedNodes, NodeManager& nodeManager)
{
	for(PxU32 i=0;i<numDeletedNodes;i++)
	{
		const NodeType nodeId=deletedNodes[i];
		PX_ASSERT(nodeManager.get(nodeId).getIsDeleted());
		nodeManager.release(nodeId);
	}
}	

static const bool tSecondPass = true;  // to make use of template parameter more readable

#ifndef __SPU__
template <bool TSecondPass>
static void processSleepingIslands
(const Cm::BitMap& islandsToUpdate, const PxU32 rigidBodyOffset, 
 NodeManager& nodeManager, EdgeManager& edgeManager, IslandManager& islands, ArticulationRootManager& articulationRootManager, 
 const NodeType* kinematicSourceNodeIds,
 ProcessSleepingIslandsComputeData& psicData)
{
	if (!TSecondPass)
	{
		psicData.mBodiesToWakeSize=0;
		psicData.mBodiesToSleepSize=0;
		psicData.mNarrowPhaseContactManagersSize=0;
		psicData.mSolverKinematicsSize=0;
		psicData.mSolverBodiesSize=0;
		psicData.mSolverArticulationsSize=0;
		psicData.mSolverContactManagersSize=0;
		psicData.mSolverConstraintsSize=0;
		psicData.mIslandIndicesSize=0;
		psicData.mIslandIndicesSecondPassSize=0;
	}
	else
	{
		psicData.mBodiesToWakeSize=0;
		psicData.mBodiesToSleepSize=0;
	}

	Node* PX_RESTRICT allNodes=nodeManager.getAll();
	NodeType* PX_RESTRICT nextNodeIds=nodeManager.getNextNodeIds();
	//const PxU32 allNodesCapacity=nodeManager.getCapacity();
	Edge* PX_RESTRICT allEdges=edgeManager.getAll();
	EdgeType* PX_RESTRICT nextEdgeIds=edgeManager.getNextEdgeIds();
	//const PxU32 allEdgesCapacity=edgeManager.getCapacity();
	ArticulationRoot* PX_RESTRICT allArticRoots=articulationRootManager.getAll();
	//const PxU32 allArtisCootsCapacity=articulationRootManager.getCapacity();
	Island* PX_RESTRICT allIslands=islands.getAll();
	//const PxU32 allIslandsCapacity=islands.getCapacity();

	NodeType* PX_RESTRICT solverBodyMap=psicData.mSolverBodyMap;
	const PxU32 solverBodyMapCapacity=psicData.mSolverBodyMapCapacity;
	PxU8** PX_RESTRICT bodiesToWakeOrSleep=psicData.mBodiesToWakeOrSleep;
	const PxU32 bodiesToWakeOrSleepCapacity=psicData.mBodiesToWakeOrSleepCapacity;
	NarrowPhaseContactManager* PX_RESTRICT npContactManagers=psicData.mNarrowPhaseContactManagers;
	if (TSecondPass)
		PX_UNUSED(npContactManagers);
	//const PxU32 npContactManagersCapacity=psicData.mNarrowPhaseContactManagersCapacity;
	PxsRigidBody** PX_RESTRICT solverKinematics=psicData.mSolverKinematics;
	//const PxU32 solverKinematicsCapacity=psicData.mSolverKinematicsCapacity;
	PxsRigidBody** PX_RESTRICT solverBodies=psicData.mSolverBodies;
	//const PxU32 solverBodiesCapacity=psicData.mSolverBodiesCapacity;
	PxsArticulation** PX_RESTRICT solverArticulations=psicData.mSolverArticulations;
	void** PX_RESTRICT solverArticulationOwners=psicData.mSolverArticulationOwners;
	//const PxU32 solverArticulationsCapacity=psicData.mSolverArticulationsCapacity;
	PxsIndexedContactManager* PX_RESTRICT solverContactManagers=psicData.mSolverContactManagers;
	//const PxU32 solverContactManagersCapacity=psicData.mSolverContactManagersCapacity;
	PxsIndexedConstraint* PX_RESTRICT solverConstraints=psicData.mSolverConstraints;
	//const PxU32 solverConstraintsCapacity=psicData.mSolverConstraintsCapacity;
	PxsIslandIndices* PX_RESTRICT islandIndices=psicData.mIslandIndices;
	const PxU32 islandIndicesCapacity=psicData.mIslandIndicesCapacity;

	PxU32 bodiesToWakeSize=0;
	if (TSecondPass)
		PX_UNUSED(bodiesToWakeSize);
	PxU32 bodiesToSleepSize=0;
	PxU32 npContactManagersSize=0;
	if (TSecondPass)
		PX_UNUSED(npContactManagersSize);
	PxU32 solverKinematicsSize=psicData.mSolverKinematicsSize;
	PxU32 solverBodiesSize=psicData.mSolverBodiesSize;
	PxU32 solverArticulationsSize=psicData.mSolverArticulationsSize;
	PxU32 solverContactManagersSize=psicData.mSolverContactManagersSize;
	PxU32 solverConstraintsSize=psicData.mSolverConstraintsSize;
	PxU32 islandIndicesSize=psicData.mIslandIndicesSize;
	PxU32 islandIndicesSecondPassSize = psicData.mIslandIndicesSecondPassSize;
	if (TSecondPass)
		PX_UNUSED(islandIndicesSecondPassSize);

	PxMemSet(solverBodyMap, 0xff, sizeof(NodeType)*solverBodyMapCapacity);

	const PxU32* PX_RESTRICT bitmapWords=islandsToUpdate.getWords();
	const PxU32 lastSetBit = islandsToUpdate.findLast();
	for(PxU32 w = 0; w <= lastSetBit >> 5; ++w)
	{
		for(PxU32 b = bitmapWords[w]; b; b &= b-1)
		{
			const IslandType islandId = (IslandType)(w<<5|Ps::lowestSetBit(b));
	
			PX_ASSERT(islandId<islands.getCapacity());
			PX_ASSERT(islands.getBitmap().test(islandId));
			const Island& island=allIslands[islandId];

			//First compute the new state of the island 
			//(determine if any nodes have non-zero wake counter).
			NodeType nextNode0=island.mStartNodeId;
			PxU32 islandFlags=0;
			while(INVALID_NODE!=nextNode0)
			{
				PX_ASSERT(nextNode0<nodeManager.getCapacity());
				const Node& node=allNodes[nextNode0];
				islandFlags |= node.getFlags();
				nextNode0=nextNodeIds[nextNode0];
			}

			if(0 == (islandFlags & Node::eNOTREADYFORSLEEPING))
			{
				//Island is asleep because no nodes have non-zero wake counter.
				NodeType nextNode=island.mStartNodeId;
				while(nextNode!=INVALID_NODE)
				{
					//Get the node.
					PX_ASSERT(nextNode<nodeManager.getCapacity());
					Node& node=allNodes[nextNode];
					Ps::prefetchLine((PxU8*)(((size_t)((PxU8*)&allNodes[nextNodeIds[nextNode]]) + 128) & ~127));
					PX_ASSERT(node.getIsReadyForSleeping());
					// in second island gen pass all nodes should have been woken up.
					PX_ASSERT(!TSecondPass || node.getIsKinematic() || !node.getIsInSleepingIsland());

					//Work out if the node was previously in a sleeping island.
					const bool wasInSleepingIsland=node.getIsInSleepingIsland();
					if (TSecondPass)
						PX_UNUSED(wasInSleepingIsland);

					//If the node has changed from not being in a sleeping island to 
					//being in a sleeping island then the node needs put to sleep
					//in the high level.  Store the body pointer for reporting to hl.
					if(TSecondPass || (!wasInSleepingIsland))
					{
						if(!node.getIsArticulated())
						{
							if(!node.getIsKinematic())
							{
								//Set the node to be in a sleeping island.
								node.setIsInSleepingIsland();

								PX_ASSERT((bodiesToWakeSize+bodiesToSleepSize)<bodiesToWakeOrSleepCapacity);
								PX_ASSERT(0==((size_t)node.getRigidBodyOwner() & 0x0f));
								bodiesToWakeOrSleep[bodiesToWakeOrSleepCapacity-1-bodiesToSleepSize]=(PxU8*)node.getRigidBodyOwner();
								Ps::prefetchLine((PxU8*)(((size_t)((PxU8*)&bodiesToWakeOrSleep[bodiesToWakeOrSleepCapacity-1-bodiesToSleepSize-1]) + 128) & ~127));
								bodiesToSleepSize++;
							}
						}
						else if(node.getIsRootArticulationLink())
						{
							//Set the node to be in a sleeping island.
							node.setIsInSleepingIsland();

							const PxU32 rootArticId=(PxU32)node.getRootArticulationId();
							const ArticulationRoot& rootArtic=allArticRoots[rootArticId];
							PxU8* articOwner=(PxU8*)rootArtic.mArticulationOwner;
							PX_ASSERT(0==((size_t)articOwner & 0x0f));
							PX_ASSERT((bodiesToWakeSize+bodiesToSleepSize)<bodiesToWakeOrSleepCapacity);
							bodiesToWakeOrSleep[bodiesToWakeOrSleepCapacity-1-bodiesToSleepSize]=(PxU8*)((size_t)articOwner | 0x01);
							Ps::prefetchLine((PxU8*)(((size_t)((PxU8*)&bodiesToWakeOrSleep[bodiesToWakeOrSleepCapacity-1-bodiesToSleepSize-1]) + 128) & ~127));
							bodiesToSleepSize++;
						}
					}

					nextNode=nextNodeIds[nextNode];
				}
			}
			else
			{
				//Island is awake because at least one node has a non-zero wake counter.
				PX_ASSERT(island.mStartNodeId!=INVALID_NODE);

				//If an island needs a second pass, we need to revert the counters of written out data
				const PxU32 previousSolverKinematicsSize=solverKinematicsSize;
				const PxU32 previousSolverBodiesSize=solverBodiesSize;
				const PxU32 previousSolverArticulationsSize=solverArticulationsSize;
				const PxU32 previousSolverContactManagersSize=solverContactManagersSize;
				const PxU32 previousSolverConstraintsSize=solverConstraintsSize;

				//Add the indices of the island.
				PX_ASSERT(TSecondPass || (islandIndicesSize<(islandIndicesCapacity - islandIndicesSecondPassSize)));
				islandIndices[islandIndicesSize].articulations=solverArticulationsSize;
				islandIndices[islandIndicesSize].bodies=(NodeType)solverBodiesSize;
				islandIndices[islandIndicesSize].contactManagers=(EdgeType)solverContactManagersSize;
				islandIndices[islandIndicesSize].constraints=(EdgeType)solverConstraintsSize;

				Ps::prefetchLine((PxU8*)(((size_t)((PxU8*)&islandIndices[islandIndicesSize+1]) + 128) & ~127));

				NodeType nextNode=island.mStartNodeId;
				while(nextNode!=INVALID_NODE)
				{
					//Get the node.
					PX_ASSERT(nextNode<nodeManager.getCapacity());
					Node& node=allNodes[nextNode];
					Ps::prefetchLine((PxU8*)(((size_t)((PxU8*)&allNodes[nextNodeIds[nextNode]]) + 128) & ~127));

					//Work out if the node was previously in a sleeping island.
					const bool wasInSleepingIsland=node.getIsInSleepingIsland();
					if (TSecondPass)
						PX_UNUSED(wasInSleepingIsland);

					// in second island gen pass all nodes should have been woken up.
					PX_ASSERT(!TSecondPass || node.getIsKinematic() || !node.getIsInSleepingIsland());

					//If the node has changed from being in a sleeping island to 
					//not being in a sleeping island then the node needs to be woken up
					//in the high level.  Store the body pointer for reporting to hl.
					if ((!TSecondPass) && wasInSleepingIsland)
					{
						if(!node.getIsArticulated())
						{
							if(!node.getIsKinematic())
							{
								node.clearIsInSleepingIsland();

								PX_ASSERT((bodiesToWakeSize+bodiesToSleepSize)<bodiesToWakeOrSleepCapacity);
								PX_ASSERT(0==((size_t)node.getRigidBodyOwner() & 0x0f));
								bodiesToWakeOrSleep[bodiesToWakeSize]=(PxU8*)node.getRigidBodyOwner();
								Ps::prefetchLine((PxU8*)(((size_t)((PxU8*)&bodiesToWakeOrSleep[bodiesToWakeSize+1]) + 128) & ~127));
								bodiesToWakeSize++;
							}
						}
						else if(node.getIsRootArticulationLink())
						{
							node.clearIsInSleepingIsland();

							PX_ASSERT((bodiesToWakeSize+bodiesToSleepSize)<bodiesToWakeOrSleepCapacity);
							const PxU32 rootArticId=(PxU32)node.getRootArticulationId();
							const ArticulationRoot& rootArtic=allArticRoots[rootArticId];
							PxU8* articOwner=(PxU8*)rootArtic.mArticulationOwner;
							PX_ASSERT(0==((size_t)articOwner & 0x0f));
							bodiesToWakeOrSleep[bodiesToWakeSize]=(PxU8*)((size_t)articOwner | 0x01);
							Ps::prefetchLine((PxU8*)(((size_t)((PxU8*)&bodiesToWakeOrSleep[bodiesToWakeSize+1]) + 128) & ~127));
							bodiesToWakeSize++;
						}
					}

					if(!node.getIsKinematic())
					{
						if(!node.getIsArticulated())
						{
							//Create the mapping between the entry id in mNodeManager and the entry id in mSolverBoldies
							PX_ASSERT(nextNode<psicData.mSolverBodyMapCapacity);
							solverBodyMap[nextNode]=(NodeType)solverBodiesSize;

							//Add rigid body to solver island.
							PX_ASSERT(solverBodiesSize<psicData.mSolverBodiesCapacity);
							solverBodies[solverBodiesSize]=node.getRigidBody(rigidBodyOffset);
							Ps::prefetchLine((PxU8*)(((size_t)((PxU8*)&solverBodies[solverBodiesSize+1]) + 128) & ~127));
							solverBodiesSize++;
						}
						else if(node.getIsRootArticulationLink())
						{
							//Add articulation to solver island.
							const PxU32 rootArticId=(PxU32)node.getRootArticulationId();
							const ArticulationRoot& rootArtic=allArticRoots[rootArticId];
							PxsArticulationLinkHandle articLinkHandle=rootArtic.mArticulationLinkHandle;
							void* articOwner=rootArtic.mArticulationOwner;
							PX_ASSERT((solverArticulationsSize)<psicData.mSolverArticulationsCapacity);
							solverArticulations[solverArticulationsSize]=getArticulation(articLinkHandle);
							solverArticulationOwners[solverArticulationsSize]=articOwner;
							Ps::prefetchLine((PxU8*)(((size_t)((PxU8*)&solverArticulations[solverArticulationsSize+1]) + 128) & ~127));
							Ps::prefetchLine((PxU8*)(((size_t)((PxU8*)&solverArticulationOwners[solverArticulationsSize+1]) + 128) & ~127));
							solverArticulationsSize++;
						}
					}
					else
					{
						if(INVALID_NODE != kinematicSourceNodeIds[nextNode] && INVALID_NODE != solverBodyMap[kinematicSourceNodeIds[nextNode]])
						{
							//Kinematic node participates in an edge and is a proxy.
							//This is not the first time we've encountered the node represented by the proxy.
							//To avoid duplicates we can use the value stored for the "source" node rather than the proxy.

							PX_ASSERT(nextNode<psicData.mSolverBodyMapCapacity);
							PX_ASSERT(kinematicSourceNodeIds[nextNode]<psicData.mSolverBodyMapCapacity);
							solverBodyMap[nextNode]=solverBodyMap[kinematicSourceNodeIds[nextNode]];
						}
						else
						{
							//Kinematic will be a proxy if it participates in an edge but not if it has no edges.
							//This is the first time we've encountered the node (the node represented by the proxy, if applicable).
							//Set solverBodyMap for the proxy node (if applicable) and for the source node. Setting 
							//solverBodyMap[sourceNode] means that we will know if we subsequently meet a proxy 
							//node that represents the same source node. When this happens we can reuse the 
							//source node rather than adding an entry for each proxy.  The result of this is that
							//solverKinematicsSize is never greater than the number of kinematics in the scene
							//ie we avoid duplicates in the solverKinematics array.

							//Create the mapping between the entry id in mNodeManager and the entry id in mSolverBoldies
							PX_ASSERT(nextNode<psicData.mSolverBodyMapCapacity);
							solverBodyMap[nextNode]=(NodeType)solverKinematicsSize;
							if(INVALID_NODE != kinematicSourceNodeIds[nextNode])
							{
								PX_ASSERT(kinematicSourceNodeIds[nextNode]<psicData.mSolverBodyMapCapacity);
								solverBodyMap[kinematicSourceNodeIds[nextNode]]=(NodeType)solverKinematicsSize;
							}

							//Add kinematic to array of all kinematics.
							PX_ASSERT(!node.getIsArticulated());
							PX_ASSERT((solverKinematicsSize)<psicData.mSolverKinematicsCapacity);
							solverKinematics[solverKinematicsSize]=node.getRigidBody(rigidBodyOffset);
							Ps::prefetchLine((PxU8*)(((size_t)((PxU8*)&solverKinematics[solverKinematicsSize+1]) + 128) & ~127));
							solverKinematicsSize++;
						}
					}
	
					nextNode=nextNodeIds[nextNode];
				}

				bool hasStaticContact=false;
				bool hasSecondPassPairs=false;  // island has pairs that need a second narrowphase/island gen pass
				EdgeType nextEdgeId=island.mStartEdgeId;
				while(nextEdgeId!=INVALID_EDGE)
				{
					PX_ASSERT(nextEdgeId<edgeManager.getCapacity());
					Edge& edge=allEdges[nextEdgeId];
					Ps::prefetchLine((PxU8*)(((size_t)((PxU8*)&allEdges[nextEdgeIds[nextEdgeId]]) + 128) & ~127));

					const NodeType node1Id=edge.getNode1();
					const NodeType node2Id=edge.getNode2();
					PxU8 node1Type=PxsIndexedInteraction::eWORLD;
					PxU8 node2Type=PxsIndexedInteraction::eWORLD;
					PxsArticulationLinkHandle body1=(PxsArticulationLinkHandle)-1;
					PxsArticulationLinkHandle body2=(PxsArticulationLinkHandle)-1;
					bool node1IsKineOrStatic=true;
					bool node2IsKineOrStatic=true;

					if (TSecondPass || (!hasSecondPassPairs))  //we can skip a lot of logic as soon as we know the island needs to go through a second pass
					{
						if(node1Id!=INVALID_NODE)
						{
							PX_ASSERT(node1Id<nodeManager.getCapacity());
							const Node& node1=allNodes[node1Id];
							node1IsKineOrStatic=false;
							if(!node1.getIsArticulated())
							{
								node1IsKineOrStatic=node1.getIsKinematic();
								node1Type=(!node1IsKineOrStatic) ? (PxU8)PxsIndexedInteraction::eBODY : (PxU8)PxsIndexedInteraction::eKINEMATIC;
								PX_ASSERT(node1Id<psicData.mSolverBodyMapCapacity);
								body1=solverBodyMap[node1Id];
								PX_ASSERT((node1.getIsKinematic() && body1<psicData.mSolverKinematicsCapacity) || (!node1.getIsKinematic() && body1<psicData.mSolverBodiesCapacity));
							}
							else if(!node1.getIsRootArticulationLink())
							{
								node1Type=PxsIndexedInteraction::eARTICULATION;
								body1=node1.getArticulationLink();
							}
							else
							{
								node1Type=PxsIndexedInteraction::eARTICULATION;
								const PxU32 articRootId=(PxU32)node1.getRootArticulationId();
								const ArticulationRoot& articRoot=allArticRoots[articRootId];
								body1=articRoot.mArticulationLinkHandle;
							}
						}
						else
						{
							hasStaticContact=true;
						}
						if(node2Id!=INVALID_NODE)
						{
							PX_ASSERT(node2Id<nodeManager.getCapacity());
							const Node& node2=allNodes[node2Id];
							node2IsKineOrStatic=false;
							if(!node2.getIsArticulated())
							{
								node2IsKineOrStatic=node2.getIsKinematic();
								node2Type=(!node2IsKineOrStatic) ? (PxU8)PxsIndexedInteraction::eBODY : (PxU8)PxsIndexedInteraction::eKINEMATIC;
								PX_ASSERT(node2Id<psicData.mSolverBodyMapCapacity);
								body2=solverBodyMap[node2Id];
								PX_ASSERT((node2.getIsKinematic() && body2<psicData.mSolverKinematicsCapacity) || (!node2.getIsKinematic() && body2<psicData.mSolverBodiesCapacity));
							}
							else if(!node2.getIsRootArticulationLink())
							{
								node2Type=PxsIndexedInteraction::eARTICULATION;
								body2=node2.getArticulationLink();
							}						
							else
							{
								node2Type=PxsIndexedInteraction::eARTICULATION;
								const PxU32 articRootId=(PxU32)node2.getRootArticulationId();
								const ArticulationRoot& articRoot=allArticRoots[articRootId];
								body2=articRoot.mArticulationLinkHandle;
							}
						}
						else
						{
							hasStaticContact=true;
						}
					}
					else
					{
						if(node1Id!=INVALID_NODE)
						{
							PX_ASSERT(node1Id<nodeManager.getCapacity());
							const Node& node1=allNodes[node1Id];
							node1IsKineOrStatic=false;
							if(!node1.getIsArticulated())
							{
								node1IsKineOrStatic=node1.getIsKinematic();
							}
						}
						
						if(node2Id!=INVALID_NODE)
						{
							PX_ASSERT(node2Id<nodeManager.getCapacity());
							const Node& node2=allNodes[node2Id];
							node2IsKineOrStatic=false;
							if(!node2.getIsArticulated())
							{
								node2IsKineOrStatic=node2.getIsKinematic();
							}
						}
					}

					//Work out if we have an interaction handled by the solver.
					//The solver doesn't handle kinematic-kinematic or kinematic-static
					const bool isSolverInteraction = (!node1IsKineOrStatic || !node2IsKineOrStatic);

					//If both nodes were asleep at the last island gen then they need to undergo a narrowphase overlap prior to running the solver.
					//It is possible that one or other of the nodes has been woken up since the last island gen.  If either was woken up in this way then 
					//the pair will already have a contact manager provided by high-level. This being the case, the narrowphase overlap of the pair 
					//will already have been performed prior to the current island gen.  If neither has been woken up since the last island gen then 
					//there will be no contact manager and narrowphase needs to be performed on the pair in a second pass after the island gen.  The bodies 
					//will be woken up straight after the current island gen (because they are in the array of bodies to be woken up) and the pair will be 
					//given a contact manager by high-level.
					if(!TSecondPass && edge.getIsTypeCM() && !edge.getCM() && isSolverInteraction)
					{
						//Add to list for second np pass.
						PX_ASSERT((npContactManagersSize)<psicData.mNarrowPhaseContactManagersCapacity);
						npContactManagers[npContactManagersSize].mEdgeId=nextEdgeId;
						npContactManagers[npContactManagersSize].mCM=NULL;
						Ps::prefetchLine((PxU8*)(((size_t)((PxU8*)&npContactManagers[npContactManagersSize+1]) + 128) & ~127));
						npContactManagersSize++;
						hasSecondPassPairs = true;
					}

					if (TSecondPass || (!hasSecondPassPairs))
					{
						if(edge.getIsTypeCM() && isSolverInteraction)
						{
							//Add to contact managers for the solver island.
							PX_ASSERT((solverContactManagersSize)<psicData.mSolverContactManagersCapacity);
							PxsIndexedContactManager& interaction=solverContactManagers[solverContactManagersSize];
							interaction.contactManager=edge.getCM();
							interaction.indexType0=node1Type;
							interaction.indexType1=node2Type;
							interaction.solverBody0=body1;
							interaction.solverBody1=body2;
							Ps::prefetchLine((PxU8*)(((size_t)((PxU8*)&solverContactManagers[solverContactManagersSize+1]) + 128) & ~127));
							solverContactManagersSize++;
						}
						else if(edge.getIsTypeConstraint() && isSolverInteraction)
						{
							//Add to constraints for the solver island.
							PX_ASSERT((solverConstraintsSize)<psicData.mSolverConstraintsCapacity);
							PxsIndexedConstraint& interaction=solverConstraints[solverConstraintsSize];
							interaction.constraint=edge.getConstraint();
							interaction.indexType0=node1Type;
							interaction.indexType1=node2Type;
							interaction.solverBody0=body1;
							interaction.solverBody1=body2;
							Ps::prefetchLine((PxU8*)(((size_t)((PxU8*)&solverConstraints[solverConstraintsSize+1]) + 128) & ~127));
							solverConstraintsSize++;
						}
						else
						{
							//Already stored in the articulation.
							PX_ASSERT(edge.getIsTypeArticulation() || !isSolverInteraction);
						}
					}

					nextEdgeId=nextEdgeIds[nextEdgeId];
				}

				if (TSecondPass || (!hasSecondPassPairs))
				{
					//Record if the island has static contact and increment the island size.
					islandIndices[islandIndicesSize].setHasStaticContact(hasStaticContact);
					islandIndicesSize++;
				}
				else
				{
					//The island needs a second pass (narrowphase for sure, maybe second island gen pass too) -> track the island.
					PX_ASSERT((islandIndicesSecondPassSize + islandIndicesSize) < islandIndicesCapacity);
					islandIndicesSecondPassSize++;
					islandIndices[islandIndicesCapacity - islandIndicesSecondPassSize].islandId=(IslandType)islandId;

					//Revert the stored solver information
					solverKinematicsSize = previousSolverKinematicsSize;
					solverBodiesSize = previousSolverBodiesSize;
					solverArticulationsSize = previousSolverArticulationsSize;
					solverContactManagersSize = previousSolverContactManagersSize;
					solverConstraintsSize = previousSolverConstraintsSize;
				}
			}
		}
	}

	// add end count item
	PX_ASSERT(islandIndicesSize<=islands.getCapacity());
	psicData.mIslandIndices[islandIndicesSize].bodies=(NodeType)solverBodiesSize;
	psicData.mIslandIndices[islandIndicesSize].articulations=(NodeType)solverArticulationsSize;
	psicData.mIslandIndices[islandIndicesSize].contactManagers=(EdgeType)solverContactManagersSize;
	psicData.mIslandIndices[islandIndicesSize].constraints=(EdgeType)solverConstraintsSize;

	if (!TSecondPass)
		psicData.mBodiesToWakeSize=bodiesToWakeSize;
	psicData.mBodiesToSleepSize=bodiesToSleepSize;
	if (!TSecondPass)
		psicData.mNarrowPhaseContactManagersSize=npContactManagersSize;
	psicData.mSolverKinematicsSize=solverKinematicsSize;
	psicData.mSolverBodiesSize=solverBodiesSize;
	psicData.mSolverArticulationsSize=solverArticulationsSize;
	psicData.mSolverContactManagersSize=solverContactManagersSize;
	psicData.mSolverConstraintsSize=solverConstraintsSize;
	psicData.mIslandIndicesSize=islandIndicesSize;
	if (TSecondPass)
		psicData.mIslandIndicesSecondPassSize=0;
	else
		psicData.mIslandIndicesSecondPassSize=islandIndicesSecondPassSize;
}
#endif

void physx::mergeKinematicProxiesBackToSource
(const Cm::BitMap& kinematicNodesBitmap,
 const NodeType* PX_RESTRICT kinematicProxySourceNodes, const NodeType* PX_RESTRICT kinematicProxyNextNodes,
 NodeManager& nodeManager, EdgeManager& edgeManager, IslandManager& islands,
 Cm::BitMap& kinematicIslandsBitmap,
 IslandType* graphStartIslands, IslandType* graphNextIslands)
{
	Node* PX_RESTRICT allNodes=nodeManager.getAll();
	const PxU32 allNodesCapacity=nodeManager.getCapacity();
	NodeType* PX_RESTRICT nextNodeIds=nodeManager.getNextNodeIds();

	Edge* PX_RESTRICT allEdges=edgeManager.getAll();
	const PxU32 allEdgesCapacity=edgeManager.getCapacity();
	EdgeType* PX_RESTRICT nextEdgeIds=edgeManager.getNextEdgeIds();

	PxMemSet(graphStartIslands, 0xff, sizeof(IslandType)*islands.getCapacity());
	PxMemSet(graphNextIslands, 0xff, sizeof(IslandType)*islands.getCapacity());

	//Remove all the proxies from all islands.

	//Compute the bitmap of islands affected by kinematic proxy nodes.
	//Work out if the node was in a non-sleeping island.
	{
		const PxU32* PX_RESTRICT kinematicNodesBitmapWords=kinematicNodesBitmap.getWords();
		const PxU32 lastSetBit = kinematicNodesBitmap.findLast();
		for(PxU32 w = 0; w <= lastSetBit >> 5; ++w)
		{
			for(PxU32 b = kinematicNodesBitmapWords[w]; b; b &= b-1)
			{
				const NodeType kinematicNodeId = (NodeType)(w<<5|Ps::lowestSetBit(b));
				PX_ASSERT(kinematicNodeId<allNodesCapacity);
				PX_ASSERT(allNodes[kinematicNodeId].getIsKinematic());
				NodeType nextProxyNodeId=kinematicProxyNextNodes[kinematicNodeId];
				while(nextProxyNodeId!=INVALID_NODE)
				{
					PX_ASSERT(nextProxyNodeId<allNodesCapacity);
					Node& node=allNodes[nextProxyNodeId];
					node.setIsDeleted();
					const IslandType islandId=node.getIslandId();
					PX_ASSERT(islandId<islands.getCapacity());
					graphStartIslands[nextProxyNodeId]=islandId;
					graphNextIslands[nextProxyNodeId]=INVALID_ISLAND;
					kinematicIslandsBitmap.set(islandId);
					nextProxyNodeId=kinematicProxyNextNodes[nextProxyNodeId];
				}
			}//b
		}//w
	}

	//Iterate over all islands with a kinematic proxy and remove them from the node list and from the edges.
	{
		const PxU32 lastSetBit = kinematicIslandsBitmap.findLast();
		for(PxU32 w = 0; w <= lastSetBit >> 5; ++w)
		{
			for(PxU32 b = kinematicIslandsBitmap.getWords()[w]; b; b &= b-1)
			{
				const IslandType islandId = (IslandType)(w<<5|Ps::lowestSetBit(b));
				PX_ASSERT(islandId<islands.getCapacity());

				//Remove all kinematic proxy nodes from all affected islands.
				removeDeletedNodesFromIsland
					(islandId,
					nodeManager,edgeManager,islands);

				//Make the edges reference the original node.
				const Island& island=islands.get(islandId);
				EdgeType nextEdge=island.mStartEdgeId;
				while(INVALID_EDGE!=nextEdge)
				{
					//Get the edge.
					PX_ASSERT(nextEdge<allEdgesCapacity);
					Edge& edge=allEdges[nextEdge];

					const NodeType node1=edge.getNode1();
					if(INVALID_NODE!=node1)
					{
						PX_ASSERT(node1<allNodesCapacity);
						if(kinematicProxySourceNodes[node1]!=INVALID_NODE)
						{
							PX_ASSERT(allNodes[node1].getIsKinematic());
							PX_ASSERT(allNodes[node1].getIsDeleted());
							edge.setNode1(kinematicProxySourceNodes[node1]);
						}
					}

					const NodeType node2=edge.getNode2();
					if(INVALID_NODE!=node2)
					{
						PX_ASSERT(node2<allNodesCapacity);
						if(kinematicProxySourceNodes[node2]!=INVALID_NODE)
						{
							PX_ASSERT(allNodes[node2].getIsKinematic());
							PX_ASSERT(allNodes[node2].getIsDeleted());
							edge.setNode2(kinematicProxySourceNodes[node2]);
						}
					}

					nextEdge=nextEdgeIds[nextEdge];			
				}//nextEdge
			}//b
		}//w
	}

	//Merge all the islands.
	{
		const PxU32* PX_RESTRICT kinematicNodesBitmapWords=kinematicNodesBitmap.getWords();
		const PxU32 lastSetBit = kinematicNodesBitmap.findLast();
		for(PxU32 w = 0; w <= lastSetBit >> 5; ++w)
		{
			for(PxU32 b = kinematicNodesBitmapWords[w]; b; b &= b-1)
			{
				const NodeType kinematicNodeId = (NodeType)(w<<5|Ps::lowestSetBit(b));

				//Get the first proxy and its island.
				const NodeType firstProxyNodeId=kinematicProxyNextNodes[kinematicNodeId];

				if(INVALID_NODE!=firstProxyNodeId)
				{
					//The kinematic is referenced by at least one edge.
					//Safe to get the first proxy node.
					PX_ASSERT(firstProxyNodeId<allNodesCapacity);
					IslandType firstIslandId=INVALID_ISLAND;
					IslandType nextIslandId0=graphStartIslands[firstProxyNodeId];
					PX_ASSERT(nextIslandId0!=INVALID_ISLAND);
					while(nextIslandId0!=INVALID_ISLAND)
					{
						firstIslandId=nextIslandId0;
						nextIslandId0=graphNextIslands[nextIslandId0];
					}
					PX_ASSERT(firstIslandId<islands.getCapacity());

					//First set the node to be in the correct island.
					const NodeType kinematicSourceId=kinematicProxySourceNodes[firstProxyNodeId];
					PX_ASSERT(kinematicSourceId==kinematicNodeId);
					PX_ASSERT(kinematicSourceId<allNodesCapacity);
					Node& kinematicSourceNode=allNodes[kinematicSourceId];
					kinematicSourceNode.setIslandId(firstIslandId);

					//Now add the node to the island.
					PX_ASSERT(kinematicSourceId<allNodesCapacity);
					addNodeToIsland(firstIslandId,kinematicSourceId,allNodes,nextNodeIds,allNodesCapacity,islands);

					//Now merge the other islands containing a kinematic proxy with the first island.
					NodeType nextProxyNodeId=kinematicProxyNextNodes[firstProxyNodeId];
					while(INVALID_NODE!=nextProxyNodeId)
					{
						PX_ASSERT(nextProxyNodeId<allNodesCapacity);

						//Get the island.
						IslandType islandId=INVALID_ISLAND;
						IslandType nextIslandId=graphStartIslands[nextProxyNodeId];
						PX_ASSERT(nextIslandId!=INVALID_ISLAND);
						while(nextIslandId!=INVALID_ISLAND)
						{
							islandId=nextIslandId;
							nextIslandId=graphNextIslands[nextIslandId];
						}
						PX_ASSERT(islandId<islands.getCapacity());

						//Merge the two islands provided they are 
						//(i)  different
						//(ii) islandId hasn't already been merged into another island and released

						if(firstIslandId!=islandId && islands.getBitmap().test(islandId))
						{
							setNodeIslandIdsAndJoinIslands(firstIslandId,islandId,allNodes,allNodesCapacity,allEdges,allEdgesCapacity,nextNodeIds,nextEdgeIds,islands);
							graphNextIslands[islandId]=firstIslandId;
						}

						//Get the next proxy node.			
						nextProxyNodeId=kinematicProxyNextNodes[nextProxyNodeId];
					}
				}//INVALID_NODE!=firstProxyNodeId

				//Nodes with no edges require no treatment.
				//They were already put in their own island when during node duplication.
			}//b
		}//w
	}

	//Release the proxies.
	{
		const PxU32* PX_RESTRICT kinematicNodesBitmapWords=kinematicNodesBitmap.getWords();
		const PxU32 lastSetBit = kinematicNodesBitmap.findLast();
		for(PxU32 w = 0; w <= lastSetBit >> 5; ++w)
		{
			for(PxU32 b = kinematicNodesBitmapWords[w]; b; b &= b-1)
			{
				const NodeType kinematicNodeId = (NodeType)(w<<5|Ps::lowestSetBit(b));
				NodeType nextProxyNode=kinematicProxyNextNodes[kinematicNodeId];
				while(nextProxyNode!=INVALID_NODE)
				{
					PX_ASSERT(allNodes[nextProxyNode].getIsKinematic());
					PX_ASSERT(allNodes[nextProxyNode].getIsDeleted());
					nodeManager.release(nextProxyNode);
					nextProxyNode=kinematicProxyNextNodes[nextProxyNode];
				}
			}
		}
	}
}

#ifndef __SPU__
#define ISLANDGEN_PROFILE 1
#else
#define ISLANDGEN_PROFILE 0
#endif

void physx::updateIslandsMain
(const PxU32 rigidBodyOffset,
 const NodeType* PX_RESTRICT deletedNodes, const PxU32 numDeletedNodes,
 const NodeType* PX_RESTRICT createdNodes, const PxU32 numCreatedNodes,
 const EdgeType* PX_RESTRICT deletedEdges, const PxU32 numDeletedEdges,
 const EdgeType* PX_RESTRICT /*createdEdges*/, const PxU32 /*numCreatedEdges*/,
 const EdgeType* PX_RESTRICT brokenEdges, const PxU32 numBrokenEdges,
 const EdgeType* PX_RESTRICT joinedEdges, const PxU32 numJoinedEdges,
 const Cm::BitMap& kinematicNodesBitmap, const PxU32 numAddedKinematics,
 NodeManager& nodeManager, EdgeManager& edgeManager, IslandManager& islands, ArticulationRootManager& articulationRootManager,
 ProcessSleepingIslandsComputeData& psicData,
 IslandManagerUpdateWorkBuffers& workBuffers,
 Cm::EventProfiler* profiler)
{
#if PX_IS_SPU || !(defined(PX_CHECKED) || defined(PX_PROFILE) || defined(PX_DEBUG))
	PX_UNUSED(profiler);
#endif

	//Bitmaps of islands affected by joined/broken edges.
	Cm::BitMap& brokenEdgeIslandsBitmap=*workBuffers.mIslandBitmap1;
	brokenEdgeIslandsBitmap.clearFast();

	//Remove deleted nodes from islands.
	//Remove deleted/broken edges from islands.
	//Release empty islands.
	{
#if ISLANDGEN_PROFILE
		CM_PROFILE_START(profiler, Cm::ProfileEventId::IslandGen::GetemptyIslands());
#endif

		//When removing deleted nodes from islands some islands end up empty.
		//Don't immediately release these islands because we also want to 
		//remove edges from islands too and it makes it harder to manage
		//the book-keeping if some islands have already been released.
		//Store a list of empty islands and release after removing 
		//deleted edges from islands.

		Cm::BitMap& emptyIslandsBitmap=*workBuffers.mIslandBitmap2;
		emptyIslandsBitmap.clearFast();

		//Remove deleted nodes from islands with the help of a bitmap to record
		//all islands affected by a deleted node.
		removeDeletedNodesFromIslands(
			deletedNodes,numDeletedNodes,
			nodeManager,edgeManager,islands,
			brokenEdgeIslandsBitmap,emptyIslandsBitmap);
		brokenEdgeIslandsBitmap.clearFast();

		//Remove broken/deleted edges from islands.
		removeBrokenEdgesFromIslands(
			brokenEdges,numBrokenEdges,
			deletedEdges,numDeletedEdges,
			NULL,
			nodeManager,edgeManager,islands,
			brokenEdgeIslandsBitmap);

		//Now release all islands.
		releaseEmptyIslands(
			emptyIslandsBitmap,
			islands,
			brokenEdgeIslandsBitmap);
		emptyIslandsBitmap.clearFast();

#if ISLANDGEN_PROFILE
		CM_PROFILE_STOP(profiler, Cm::ProfileEventId::IslandGen::GetemptyIslands());
#endif
	}

	//Process all edges that are flagged as connected by joining islands together.
	{
#if ISLANDGEN_PROFILE
		CM_PROFILE_START(profiler, Cm::ProfileEventId::IslandGen::GetjoinedEdges());
#endif

		Cm::BitMap& affectedIslandsBitmap=*workBuffers.mIslandBitmap2;
		affectedIslandsBitmap.clearFast();

		NodeType* graphNextNodes=workBuffers.mGraphNextNodes;
		IslandType* graphStartIslands=workBuffers.mGraphStartIslands;
		IslandType* graphNextIslands=workBuffers.mGraphNextIslands;

		processJoinedEdges(
			joinedEdges,numJoinedEdges,
			nodeManager,edgeManager,islands,
			brokenEdgeIslandsBitmap,
			affectedIslandsBitmap,
			graphNextNodes,graphStartIslands,graphNextIslands);

#if ISLANDGEN_PROFILE
		CM_PROFILE_STOP(profiler, Cm::ProfileEventId::IslandGen::GetjoinedEdges());
#endif
	}

	//Any new nodes not involved in a joined edge need to be placed in their own island.
	//Release deleted edges/nodes so they are available for reuse.
	{
#if ISLANDGEN_PROFILE
		CM_PROFILE_START(profiler, Cm::ProfileEventId::IslandGen::GetcreatedNodes());
#endif

		processCreatedNodes(
			createdNodes,numCreatedNodes,
			nodeManager,islands);

#if ISLANDGEN_PROFILE
		CM_PROFILE_STOP(profiler, Cm::ProfileEventId::IslandGen::GetcreatedNodes());
#endif
	}

	//Release deleted edges/nodes so they are available for reuse.
	{
#if ISLANDGEN_PROFILE
		CM_PROFILE_START(profiler, Cm::ProfileEventId::IslandGen::GetdeletedNodesEdges());
#endif

		releaseDeletedNodes(
			deletedNodes, numDeletedNodes, 
			nodeManager);

		releaseDeletedEdges(
			deletedEdges,numDeletedEdges,
			edgeManager);

#if ISLANDGEN_PROFILE
		CM_PROFILE_STOP(profiler, Cm::ProfileEventId::IslandGen::GetdeletedNodesEdges());
#endif
	}

	//Duplicate all kinematics with temporary proxy nodes to ensure that kinematics don't
	//act as a bridge between islands.
	if(numAddedKinematics>0)
	{
#if ISLANDGEN_PROFILE
		CM_PROFILE_START(profiler, Cm::ProfileEventId::IslandGen::GetduplicateKinematicNodes());
#endif

		NodeType* kinematicProxySourceNodeIds=workBuffers.mKinematicProxySourceNodeIds;
		NodeType* kinematicProxyNextNodeIds=workBuffers.mKinematicProxyNextNodeIds;
		NodeType* kinematicProxyLastNodeIds=workBuffers.mKinematicProxyLastNodeIds;

		duplicateKinematicNodes(
			kinematicNodesBitmap,
			nodeManager,edgeManager,islands,
			kinematicProxySourceNodeIds,kinematicProxyNextNodeIds,kinematicProxyLastNodeIds,
			brokenEdgeIslandsBitmap);

#if ISLANDGEN_PROFILE
		CM_PROFILE_STOP(profiler, Cm::ProfileEventId::IslandGen::GetduplicateKinematicNodes());
#endif
	}

	//Any islands with a broken edge or an edge referencing a kinematic need to be rebuilt into their sub-islands.
	{
#if ISLANDGEN_PROFILE
		CM_PROFILE_START(profiler, Cm::ProfileEventId::IslandGen::GetbrokenEdgeIslands());
#endif

		NodeType* graphNextNodes=workBuffers.mGraphNextNodes;
		IslandType* graphStartIslands=workBuffers.mGraphStartIslands;
		IslandType* graphNextIslands=workBuffers.mGraphNextIslands;

		processBrokenEdgeIslands(
			brokenEdgeIslandsBitmap,
			nodeManager,edgeManager,islands,
			graphNextNodes,graphStartIslands,graphNextIslands,
			NULL);

#if ISLANDGEN_PROFILE
		CM_PROFILE_STOP(profiler, Cm::ProfileEventId::IslandGen::GetbrokenEdgeIslands());
#endif
	}

	//Process all the sleeping and awake islands to compute the solver islands data.
	{
#if ISLANDGEN_PROFILE
		CM_PROFILE_START(profiler, Cm::ProfileEventId::IslandGen::GetprocessSleepingIslands());
#endif
		
#ifndef __SPU__
		processSleepingIslands<!tSecondPass>(
			islands.getBitmap(), rigidBodyOffset,
			nodeManager,edgeManager,islands,articulationRootManager,
			workBuffers.mKinematicProxySourceNodeIds, 
			psicData);
#else
		processSleepingIslands(
			islands.getBitmap(), rigidBodyOffset,
			nodeManager,edgeManager,islands,articulationRootManager,
			workBuffers.mKinematicProxySourceNodeIds, 
			psicData);
#endif

#if ISLANDGEN_PROFILE
		CM_PROFILE_STOP(profiler, Cm::ProfileEventId::IslandGen::GetprocessSleepingIslands());
#endif
	}
}

void physx::updateIslandsSecondPassMain
(const PxU32 rigidBodyOffset, Cm::BitMap& affectedIslandsBitmap,
 const EdgeType* PX_RESTRICT brokenEdges, const PxU32 numBrokenEdges,
 NodeManager& nodeManager, EdgeManager& edgeManager, IslandManager& islands, ArticulationRootManager& articulationRootManager,
 ProcessSleepingIslandsComputeData& psicData,
 IslandManagerUpdateWorkBuffers& workBuffers,
 Cm::EventProfiler* profiler)
{
#if PX_IS_SPU || !(defined(PX_CHECKED) || defined(PX_PROFILE) || defined(PX_DEBUG))
	PX_UNUSED(profiler);
#endif

	//Bitmaps of islands affected by broken edges.
	Cm::BitMap& brokenEdgeIslandsBitmap=*workBuffers.mIslandBitmap1;
	brokenEdgeIslandsBitmap.clearFast();

	//Remove broken edges from islands.
	//Release empty islands.
	{
#if ISLANDGEN_PROFILE
		CM_PROFILE_START(profiler, Cm::ProfileEventId::IslandGen::GetemptyIslands());
#endif

		//Remove broken/deleted edges from islands.
		removeBrokenEdgesFromIslands(
			brokenEdges,numBrokenEdges,
			NULL,0,
			workBuffers.mKinematicProxySourceNodeIds,
			nodeManager,edgeManager,islands,
			brokenEdgeIslandsBitmap);

#if ISLANDGEN_PROFILE
		CM_PROFILE_STOP(profiler, Cm::ProfileEventId::IslandGen::GetemptyIslands());
#endif
	}

	//Any islands with a broken edge need to be rebuilt into their sub-islands.
	{
#if ISLANDGEN_PROFILE
		CM_PROFILE_START(profiler, Cm::ProfileEventId::IslandGen::GetbrokenEdgeIslands());
#endif

		NodeType* graphNextNodes=workBuffers.mGraphNextNodes;
		IslandType* graphStartIslands=workBuffers.mGraphStartIslands;
		IslandType* graphNextIslands=workBuffers.mGraphNextIslands;

		processBrokenEdgeIslands(
			brokenEdgeIslandsBitmap,
			nodeManager,edgeManager,islands,
			graphNextNodes,graphStartIslands,graphNextIslands,
			&affectedIslandsBitmap);

#if ISLANDGEN_PROFILE
		CM_PROFILE_STOP(profiler, Cm::ProfileEventId::IslandGen::GetbrokenEdgeIslands());
#endif
	}

	//Process all the sleeping and awake islands to compute the solver islands data.
	{
#if ISLANDGEN_PROFILE
		CM_PROFILE_START(profiler, Cm::ProfileEventId::IslandGen::GetprocessSleepingIslands());
#endif

#ifndef __SPU__
		processSleepingIslands<tSecondPass>(
			affectedIslandsBitmap, rigidBodyOffset,
			nodeManager,edgeManager,islands,articulationRootManager,
			workBuffers.mKinematicProxySourceNodeIds,
			psicData);
#else
		processSleepingIslandsSecondPass(
			affectedIslandsBitmap, rigidBodyOffset,
			nodeManager,edgeManager,islands,articulationRootManager,
			workBuffers.mKinematicProxySourceNodeIds,
			psicData);
#endif

#if ISLANDGEN_PROFILE
		CM_PROFILE_STOP(profiler, Cm::ProfileEventId::IslandGen::GetprocessSleepingIslands());
#endif
	}
}
