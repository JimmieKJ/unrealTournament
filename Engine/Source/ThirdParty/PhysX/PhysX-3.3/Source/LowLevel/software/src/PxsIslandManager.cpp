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

#include "PxsIslandManager.h"
#include "PxsIslandManagerAux.h"
#include "PxsArticulation.h"
#include "PxsRigidBody.h"
#include "PxsContactManager.h"
#include "PxsConstraint.h"
#include "CmEventProfiler.h"
#ifdef PX_PS3
#include "PS3Support.h"
#include "PxSpuTask.h"
#include "CellIslandGenTask.h"
#endif

using namespace physx;

#ifdef PX_PS3

namespace physx
{

class IslandGenSpuTask : public PxSpuTask
{
	#define SPU_PROFILE 0

public:

	IslandGenSpuTask(bool secondIslandPass)
		:  PxSpuTask(NULL, 0, 0), mContext(NULL), mSecondIslandPass(secondIslandPass)
	{
	}

	IslandGenSpuTask (void* context, PxU32 numSpusToUse, bool secondIslandPass)
		: PxSpuTask(gPS3GetElfImage(SPU_ELF_ISLAND_GEN_TASK), gPS3GetElfSize(SPU_ELF_ISLAND_GEN_TASK), PxMin((PxU32)1,numSpusToUse)), mContext(context), mSecondIslandPass(secondIslandPass)
	{
		PxsIslandManager* PX_RESTRICT islandManager=(PxsIslandManager*)mContext;

		mCellIslandGenSPUInput.mRigidBodyOffset=islandManager->mRigidBodyOffset;

		mCellIslandGenSPUInput.mNodeManagerElems=(Node*)islandManager->mNodeManager.mElems;
		mCellIslandGenSPUInput.mNodeManagerFreeElems=(NodeType*)islandManager->mNodeManager.mFreeElems;
		mCellIslandGenSPUInput.mNodeManagerNextFreeElem=islandManager->mNodeManager.mNextFreeElem;
		mCellIslandGenSPUInput.mNodeManagerCapacity=islandManager->mNodeManager.mCapacity;
		mCellIslandGenSPUInput.mNodeManagerNextNodeIds=islandManager->mNodeManager.mNextNodeIds;
		mCellIslandGenSPUInput.mKinematicNodeBitmapWords=islandManager->mNodeManager.mKinematicNodeIdBitmapWords;
		mCellIslandGenSPUInput.mKinematicNodeBitmapWordCount=islandManager->mNodeManager.mKinematicNodeIdBitmapWordsSize;

		mCellIslandGenSPUInput.mEdgeManagerElems=(Edge*)islandManager->mEdgeManager.mElems;
		mCellIslandGenSPUInput.mEdgeManagerFreeElems=(EdgeType*)islandManager->mEdgeManager.mFreeElems;
		mCellIslandGenSPUInput.mEdgeManagerNextFreeElem=islandManager->mEdgeManager.mNextFreeElem;
		mCellIslandGenSPUInput.mEdgeManagerCapacity=islandManager->mEdgeManager.mCapacity;
		mCellIslandGenSPUInput.mEdgeManagerNextEdgeIds=islandManager->mEdgeManager.mNextEdgeIds;

		mCellIslandGenSPUInput.mIslandsElems=(Island*)islandManager->mIslands.mElems;
		mCellIslandGenSPUInput.mIslandsFreeElems=(IslandType*)islandManager->mIslands.mFreeElems;
		mCellIslandGenSPUInput.mIslandsNextFreeElem=islandManager->mIslands.mNextFreeElem;
		mCellIslandGenSPUInput.mIslandsCapacity=islandManager->mIslands.mCapacity;
		mCellIslandGenSPUInput.mIslandsBitmapWords=islandManager->mIslands.mBitmapWords;
		mCellIslandGenSPUInput.mIslandsBitmapWordCount=islandManager->mIslands.mBitmapWordCount;

		mCellIslandGenSPUInput.mArticRootManagerElems=(ArticulationRoot*)islandManager->mRootArticulationManager.mElems;
		mCellIslandGenSPUInput.mArticRootManagerCapacity=islandManager->mRootArticulationManager.mCapacity;

		mCellIslandGenSPUInput.mNodeChangeManagerDeletedNodes=islandManager->mNodeChangeManager.mDeletedNodes;
		mCellIslandGenSPUInput.mNodeChangeManagerNumDeletedNodes=islandManager->mNodeChangeManager.mDeletedNodesSize;
		mCellIslandGenSPUInput.mNodeChangeManagerCreatedNodes=islandManager->mNodeChangeManager.mCreatedNodes;
		mCellIslandGenSPUInput.mNodeChangeManagerNumCreatedNodes=islandManager->mNodeChangeManager.mCreatedNodesSize;
		mCellIslandGenSPUInput.mEdgeChangeManagerDeletedEdges=islandManager->mEdgeChangeManager.mDeletedEdges;
		mCellIslandGenSPUInput.mEdgeChangeManagerNumDeletedEdges=islandManager->mEdgeChangeManager.mDeletedEdgesSize;
		mCellIslandGenSPUInput.mEdgeChangeManagerCreatedEdges=islandManager->mEdgeChangeManager.mCreatedEdges;
		mCellIslandGenSPUInput.mEdgeChangeManagerNumCreatedEdges=islandManager->mEdgeChangeManager.mCreatedEdgesSize;
		mCellIslandGenSPUInput.mEdgeChangeManagerBrokenEdges=islandManager->mEdgeChangeManager.mBrokenEdges;
		mCellIslandGenSPUInput.mEdgeChangeManagerNumBrokenEdges=islandManager->mEdgeChangeManager.mBrokenEdgesSize;
		mCellIslandGenSPUInput.mEdgeChangeManagerJoinedEdges=islandManager->mEdgeChangeManager.mJoinedEdges;
		mCellIslandGenSPUInput.mEdgeChangeManagerNumJoinedEdges=islandManager->mEdgeChangeManager.mJoinedEdgesSize;

		mCellIslandGenSPUInput.mBodiesToWakeOrSleep=islandManager->mProcessSleepingIslandsComputeData.mBodiesToWakeOrSleep;
		mCellIslandGenSPUInput.mBodiesToWakeOrSleepCapacity=islandManager->mProcessSleepingIslandsComputeData.mBodiesToWakeOrSleepCapacity;
		mCellIslandGenSPUInput.mNarrowPhaseContactManagers=islandManager->mProcessSleepingIslandsComputeData.mNarrowPhaseContactManagers;
		mCellIslandGenSPUInput.mNarrowPhaseContactManagersCapacity=islandManager->mProcessSleepingIslandsComputeData.mNarrowPhaseContactManagersCapacity;
		mCellIslandGenSPUInput.mSolverBodyMap=islandManager->mProcessSleepingIslandsComputeData.mSolverBodyMap;
		mCellIslandGenSPUInput.mSolverBodyMapCapacity=islandManager->mProcessSleepingIslandsComputeData.mSolverBodyMapCapacity;
		mCellIslandGenSPUInput.mSolverKinematics=islandManager->mProcessSleepingIslandsComputeData.mSolverKinematics;
		mCellIslandGenSPUInput.mSolverKinematicsCapacity=islandManager->mProcessSleepingIslandsComputeData.mSolverKinematicsCapacity;
		mCellIslandGenSPUInput.mSolverBodies=islandManager->mProcessSleepingIslandsComputeData.mSolverBodies;
		mCellIslandGenSPUInput.mSolverBodiesCapacity=islandManager->mProcessSleepingIslandsComputeData.mSolverBodiesCapacity;
		mCellIslandGenSPUInput.mSolverArtics=islandManager->mProcessSleepingIslandsComputeData.mSolverArticulations;
		mCellIslandGenSPUInput.mSolverArticOwners=islandManager->mProcessSleepingIslandsComputeData.mSolverArticulationOwners;
		mCellIslandGenSPUInput.mSolverArticsCapacity=islandManager->mProcessSleepingIslandsComputeData.mSolverArticulationsCapacity;
		mCellIslandGenSPUInput.mSolverContactManagers=islandManager->mProcessSleepingIslandsComputeData.mSolverContactManagers;
		mCellIslandGenSPUInput.mSolverContactManagersCapacity=islandManager->mProcessSleepingIslandsComputeData.mSolverContactManagersCapacity;
		mCellIslandGenSPUInput.mSolverConstraints=islandManager->mProcessSleepingIslandsComputeData.mSolverConstraints;
		mCellIslandGenSPUInput.mSolverConstraintsCapacity=islandManager->mProcessSleepingIslandsComputeData.mSolverConstraintsCapacity;
		mCellIslandGenSPUInput.mSolverIslandIndices=islandManager->mProcessSleepingIslandsComputeData.mIslandIndices;
		mCellIslandGenSPUInput.mIslandIndicesCapacity=islandManager->mProcessSleepingIslandsComputeData.mIslandIndicesCapacity;

		mCellIslandGenSPUInput.mKinematicSourceNodeIds=islandManager->mIslandManagerUpdateWorkBuffers.mKinematicProxySourceNodeIds;
		mCellIslandGenSPUInput.mKinematicNextNodeIds=islandManager->mIslandManagerUpdateWorkBuffers.mKinematicProxyNextNodeIds;
		mCellIslandGenSPUInput.mKinematicLastNodeIds=islandManager->mIslandManagerUpdateWorkBuffers.mKinematicProxyLastNodeIds;
		mCellIslandGenSPUInput.mNumAddedKinematics=islandManager->mNumAddedKinematics;

		mCellIslandGenSPUInput.mSecondIslandPass = secondIslandPass;

		if (!secondIslandPass)
		{
			mCellIslandGenSPUInput.mSolverKinematicsSize=0;
			mCellIslandGenSPUInput.mSolverBodiesSize=0;
			mCellIslandGenSPUInput.mSolverArticsSize=0;
			mCellIslandGenSPUInput.mSolverContactManagersSize=0;
			mCellIslandGenSPUInput.mSolverConstraintsSize=0;
			mCellIslandGenSPUInput.mIslandIndicesSize=0;
			mCellIslandGenSPUInput.mIslandIndicesSecondPassSize=0;
			mCellIslandGenSPUInput.mSecondPassIslandsBitmapWords=NULL;
		}
		else
		{
			mCellIslandGenSPUInput.mSolverKinematicsSize=islandManager->mProcessSleepingIslandsComputeData.mSolverKinematicsSize;
			mCellIslandGenSPUInput.mSolverBodiesSize=islandManager->mProcessSleepingIslandsComputeData.mSolverBodiesSize;
			mCellIslandGenSPUInput.mSolverArticsSize=islandManager->mProcessSleepingIslandsComputeData.mSolverArticulationsSize;
			mCellIslandGenSPUInput.mSolverContactManagersSize=islandManager->mProcessSleepingIslandsComputeData.mSolverContactManagersSize;
			mCellIslandGenSPUInput.mSolverConstraintsSize=islandManager->mProcessSleepingIslandsComputeData.mSolverConstraintsSize;
			mCellIslandGenSPUInput.mIslandIndicesSize=islandManager->mProcessSleepingIslandsComputeData.mIslandIndicesSize;
			mCellIslandGenSPUInput.mIslandIndicesSecondPassSize=islandManager->mProcessSleepingIslandsComputeData.mIslandIndicesSecondPassSize;
			mCellIslandGenSPUInput.mSecondPassIslandsBitmapWords=islandManager->mIslandManagerUpdateWorkBuffers.mIslandBitmap2Words;
		}

#if SPU_PROFILE
		for(PxU32 i=0;i<MAX_NUM_SPU_PROFILE_ZONES;i++)
		{
			mProfileCounters[i]=0;
		}
		mCellIslandGenSPUInput.mProfileZones=mProfileCounters;
#endif

		// Prepare the output data that will be passed to all the spus.
		mCellIslandGenSPUOutput.mSuccessState=CellIslandGenSPUOutput::eSUCCESS_STATE_FAILED;

		//Start all the spu tasks.
		setArgs(0, 0 | (unsigned int)&mCellIslandGenSPUOutput, (unsigned int)&mCellIslandGenSPUInput);
	}

	virtual void release()
	{
		PxsIslandManager* PX_RESTRICT islandManager=(PxsIslandManager*)mContext;

		if(CellIslandGenSPUOutput::eSUCCESS_STATE_PASSED==mCellIslandGenSPUOutput.mSuccessState)
		{
			islandManager->mNodeManager.mNextFreeElem=mCellIslandGenSPUOutput.mNodeManagerNextFreeElem;
			islandManager->mEdgeManager.mNextFreeElem=mCellIslandGenSPUOutput.mEdgeManagerNextFreeElem;
			islandManager->mIslands.mNextFreeElem=mCellIslandGenSPUOutput.mIslandsNextFreeElem;

			islandManager->mProcessSleepingIslandsComputeData.mBodiesToWakeSize=mCellIslandGenSPUOutput.mBodiesToWakeSize;
			islandManager->mProcessSleepingIslandsComputeData.mBodiesToSleepSize=mCellIslandGenSPUOutput.mBodiesToSleepSize;

			islandManager->mProcessSleepingIslandsComputeData.mSolverKinematicsSize=mCellIslandGenSPUOutput.mSolverKinematicsSize;
			islandManager->mProcessSleepingIslandsComputeData.mSolverBodiesSize=mCellIslandGenSPUOutput.mSolverBodiesSize;
			islandManager->mProcessSleepingIslandsComputeData.mSolverArticulationsSize=mCellIslandGenSPUOutput.mSolverArticsSize;
			islandManager->mProcessSleepingIslandsComputeData.mSolverContactManagersSize=mCellIslandGenSPUOutput.mSolverContactManagersSize;
			islandManager->mProcessSleepingIslandsComputeData.mSolverConstraintsSize=mCellIslandGenSPUOutput.mSolverConstraintsSize;
			islandManager->mProcessSleepingIslandsComputeData.mIslandIndicesSize=mCellIslandGenSPUOutput.mIslandIndicesSize;

			if (!mSecondIslandPass)
			{
				islandManager->mProcessSleepingIslandsComputeData.mNarrowPhaseContactManagersSize=mCellIslandGenSPUOutput.mNarrowPhaseContactManagersSize;
				islandManager->mProcessSleepingIslandsComputeData.mIslandIndicesSecondPassSize=mCellIslandGenSPUOutput.mIslandIndicesSecondPassSize;
			}

			PX_ASSERT(islandManager->isValid());
		}
		else
		{
			islandManager->mPPUFallback=true;

			if (!mSecondIslandPass)
			{
				updateIslandsMain(
					islandManager->mRigidBodyOffset,
					islandManager->mNodeChangeManager.getDeletedNodes(),islandManager->mNodeChangeManager.getNumDeletedNodes(),
					islandManager->mNodeChangeManager.getCreatedNodes(),islandManager->mNodeChangeManager.getNumCreatedNodes(),
					islandManager->mEdgeChangeManager.getDeletedEdges(),islandManager->mEdgeChangeManager.getNumDeletedEdges(),
					islandManager->mEdgeChangeManager.getCreatedEdges(),islandManager->mEdgeChangeManager.getNumCreatedEdges(),
					islandManager->mEdgeChangeManager.getBrokenEdges(),islandManager->mEdgeChangeManager.getNumBrokenEdges(),
					islandManager->mEdgeChangeManager.getJoinedEdges(),islandManager->mEdgeChangeManager.getNumJoinedEdges(),
					islandManager->mNodeManager.getKinematicNodeBitmap(), islandManager->mNumAddedKinematics,
					islandManager->mNodeManager,islandManager->mEdgeManager,islandManager->mIslands,islandManager->mRootArticulationManager,
					islandManager->mProcessSleepingIslandsComputeData,
					islandManager->mIslandManagerUpdateWorkBuffers,
					islandManager->mEventProfiler);
			}
			else
			{
				updateIslandsSecondPassMain(
					islandManager->mRigidBodyOffset, *islandManager->mIslandManagerUpdateWorkBuffers.mIslandBitmap2,
					islandManager->mEdgeChangeManager.getBrokenEdges(),islandManager->mEdgeChangeManager.getNumBrokenEdges(),
					islandManager->mNodeManager,islandManager->mEdgeManager,islandManager->mIslands,islandManager->mRootArticulationManager,
					islandManager->mProcessSleepingIslandsComputeData,
					islandManager->mIslandManagerUpdateWorkBuffers,
					islandManager->mEventProfiler);
			}

			PX_ASSERT(islandManager->isValid());
		}

		PX_ASSERT(islandManager->mProcessSleepingIslandsComputeData.mSolverBodiesSize<=islandManager->mNumAddedRBodies);
		PX_ASSERT(islandManager->mProcessSleepingIslandsComputeData.mSolverKinematicsSize<=islandManager->mNumEdgesWithKinematicNodes);
		PX_ASSERT(islandManager->mProcessSleepingIslandsComputeData.mSolverArticulationsSize<=islandManager->mNumAddedArtics);

		islandManager->mNodeChangeManager.reset();
		islandManager->mEdgeChangeManager.reset();

		PxSpuTask::release();
	}

	const char* getName() const { return "PxsIslandManager.islandGenSpu"; }

	void* mContext;

	CellIslandGenSPUInput		PX_ALIGN(128, mCellIslandGenSPUInput);
	CellIslandGenSPUOutput		PX_ALIGN(128, mCellIslandGenSPUOutput);

	bool mSecondIslandPass;

#if SPU_PROFILE
	PxU64						PX_ALIGN(16, mProfileCounters[MAX_NUM_SPU_PROFILE_ZONES])={0};
#endif
};

} //namespace physx

#endif //PX_PS3


PxsIslandManager::PxsIslandManager(const PxU32 rigidBodyOffset, PxcScratchAllocator& scratchAllocator, Cm::EventProfiler* eventProfiler)
: mRigidBodyOffset(rigidBodyOffset),
  mScratchAllocator(scratchAllocator),
  mEventProfiler(eventProfiler)
{

	mNumAddedRBodies=0;
	mNumAddedArtics=0;
	mNumAddedKinematics=0;
	mNumAddedEdges[EDGE_TYPE_CONTACT_MANAGER]=0;
	mNumAddedEdges[EDGE_TYPE_CONSTRAINT]=0;
	mNumAddedEdges[EDGE_TYPE_ARTIC]=0;

	mNumEdgesWithKinematicNodes=0;

	mEverythingAsleep = false;
	mHasAnythingChanged = true;
	mPerformIslandUpdate = false;

	mIslandManagerUpdateWorkBuffers.mIslandBitmap1Words=NULL;
	mIslandManagerUpdateWorkBuffers.mIslandBitmap1WordCount=0;
	mIslandManagerUpdateWorkBuffers.mIslandBitmap1=(Cm::BitMap*)mIslandManagerUpdateWorkBuffers.mIslandsBitmap1Buffer;

	mIslandManagerUpdateWorkBuffers.mIslandBitmap2Words=NULL;
	mIslandManagerUpdateWorkBuffers.mIslandBitmap2WordCount=0;
	mIslandManagerUpdateWorkBuffers.mIslandBitmap2=(Cm::BitMap*)mIslandManagerUpdateWorkBuffers.mIslandsBitmap2Buffer;

	mIslandManagerUpdateWorkBuffers.mKinematicProxySourceNodeIds=NULL;
	mIslandManagerUpdateWorkBuffers.mKinematicProxyNextNodeIds=NULL;
	mIslandManagerUpdateWorkBuffers.mKinematicProxyLastNodeIds=NULL;

	mIslandManagerUpdateWorkBuffers.mGraphNextNodes=NULL;
	mIslandManagerUpdateWorkBuffers.mGraphStartIslands=NULL;
	mIslandManagerUpdateWorkBuffers.mGraphNextIslands=NULL;

	mProcessSleepingIslandsComputeData.mDataBlock=NULL;
	mProcessSleepingIslandsComputeData.mDataBlockSize=0;

	mProcessSleepingIslandsComputeData.mBodiesToWakeOrSleep=NULL;
	mProcessSleepingIslandsComputeData.mBodiesToWakeSize=0;
	mProcessSleepingIslandsComputeData.mBodiesToSleepSize=0;
	mProcessSleepingIslandsComputeData.mBodiesToWakeOrSleepCapacity=0;

	mProcessSleepingIslandsComputeData.mNarrowPhaseContactManagers=NULL;
	mProcessSleepingIslandsComputeData.mNarrowPhaseContactManagersSize=0;
	mProcessSleepingIslandsComputeData.mNarrowPhaseContactManagersCapacity=0;

	mProcessSleepingIslandsComputeData.mSolverBodyMap=NULL;
	mProcessSleepingIslandsComputeData.mSolverBodyMapCapacity=0;

	mProcessSleepingIslandsComputeData.mSolverKinematics=NULL;
	mProcessSleepingIslandsComputeData.mSolverKinematicsSize=0;
	mProcessSleepingIslandsComputeData.mSolverKinematicsCapacity=0;

	mProcessSleepingIslandsComputeData.mSolverBodies=NULL;
	mProcessSleepingIslandsComputeData.mSolverBodiesSize=0;
	mProcessSleepingIslandsComputeData.mSolverBodiesCapacity=0;

	mProcessSleepingIslandsComputeData.mSolverArticulations=NULL;
	mProcessSleepingIslandsComputeData.mSolverArticulationOwners=NULL;
	mProcessSleepingIslandsComputeData.mSolverArticulationsSize=0;
	mProcessSleepingIslandsComputeData.mSolverArticulationsCapacity=0;

	mProcessSleepingIslandsComputeData.mSolverContactManagers=NULL;
	mProcessSleepingIslandsComputeData.mSolverContactManagersSize=0;
	mProcessSleepingIslandsComputeData.mSolverContactManagersCapacity=0;

	mProcessSleepingIslandsComputeData.mSolverConstraints=NULL;
	mProcessSleepingIslandsComputeData.mSolverConstraintsSize=0;
	mProcessSleepingIslandsComputeData.mSolverConstraintsCapacity=0;

	mProcessSleepingIslandsComputeData.mIslandIndices=NULL;
	mProcessSleepingIslandsComputeData.mIslandIndicesSize=0;
	mProcessSleepingIslandsComputeData.mIslandIndicesCapacity=0;
	mProcessSleepingIslandsComputeData.mIslandIndicesSecondPassSize = 0;

	mBufferSize=0;
	mBuffer=NULL;

#ifdef PX_PS3
	mPPUFallback = false;
	mIslandGenSpuTask = (IslandGenSpuTask*)Ps::AlignedAllocator<128>().allocate(sizeof(IslandGenSpuTask), __FILE__, __LINE__);
	mIslandGenSecondPassSpuTask = (IslandGenSpuTask*)Ps::AlignedAllocator<128>().allocate(sizeof(IslandGenSpuTask), __FILE__, __LINE__);
#endif

#ifdef LOG_ISLANDGEN
	char islandGenLogName[100];
	sprintf_s(islandGenLogName, 100,"IslandGenLog.dat");
	gIslandGenLogFile = fopen(islandGenLogName, "wb");
#endif
}

PxsIslandManager::~PxsIslandManager()
{
	if(mBuffer) PX_FREE(mBuffer);
#ifdef PX_PS3
	Ps::AlignedAllocator<128>().deallocate(mIslandGenSecondPassSpuTask);
	Ps::AlignedAllocator<128>().deallocate(mIslandGenSpuTask);
#endif

#ifdef LOG_ISLANDGEN
	fclose(gIslandGenLogFile);
#endif
}

void PxsIslandManager::preAllocate(PxU32 nbBodies)
{
	if(0==nbBodies)
	{
		mNodeManager.init(DEFAULT_NUM_NODES);

		mEdgeManager.init(DEFAULT_NUM_EDGES);

		mNodeChangeManager.init(DEFAULT_NUM_NODES);

		mEdgeChangeManager.init(DEFAULT_NUM_EDGES);

		mIslands.init(DEFAULT_NUM_NODES);

		mRootArticulationManager.init(DEFAULT_NUM_ARTICS);
	}
	else
	{
		//Choose a multiple of 32 to help with bitmaps.
		const PxU32 actualNumBodies = ((nbBodies + 31) & ~31);

		mNodeManager.init(actualNumBodies);

		mEdgeManager.init(2*actualNumBodies);//Guess of 2 edges per body

		mNodeChangeManager.init(actualNumBodies);

		mEdgeChangeManager.init(2*actualNumBodies);//Guess of 2 edge per body

		mIslands.init(actualNumBodies);//Guess of 1 edge per body

		mRootArticulationManager.init(DEFAULT_NUM_ARTICS);
	}
}

void PxsIslandManager::setKinematic(const PxsIslandManagerNodeHook& hook, bool isKinematic)
{
	Node& node = mNodeManager.get(hook.index);
	PX_ASSERT(!node.getIsArticulated());
	const bool wasKinematic = node.getIsKinematic();
	node.setKinematic(isKinematic);

	mNumAddedRBodies += (wasKinematic!=isKinematic ? (!isKinematic ? 1 : -1) : 0);
	mNumAddedKinematics += (wasKinematic!=isKinematic ? (!isKinematic ? -1 : 1) : 0);

	if(wasKinematic!=isKinematic)
	{
		if(isKinematic)
		{
			mNodeManager.setKinematicNode(hook.index);
		}
		else
		{
			mNodeManager.clearKinematicNode(hook.index);
		}

		mHasAnythingChanged = true;
	}
}


void PxsIslandManager::addArticulationLink(PxsIslandManagerNodeHook& hook)
{
	const NodeType freeNode=mNodeManager.getAvailableElem();
	Node& node=mNodeManager.get(freeNode);
	node.init();
	node.setArticulated();
	node.setIsNew();
	mNodeChangeManager.addCreatedNode(freeNode);
	hook.index=freeNode;
}

void PxsIslandManager::setArticulationLinkHandle(PxsArticulationLinkHandle link, void* /*owner*/, const PxsIslandManagerNodeHook& hook)
{
	Node& node=mNodeManager.get(hook.index);
	node.setArticulationLink(link);
	mHasAnythingChanged = true;
}

void PxsIslandManager::setArticulationRootLinkHandle(PxsArticulationLinkHandle link, void* owner, const PxsIslandManagerNodeHook& hook)
{
	const NodeType articulationRootId=mRootArticulationManager.getAvailableElem();
	ArticulationRoot& articulationRoot=mRootArticulationManager.get(articulationRootId);
	articulationRoot.mArticulationLinkHandle=link;
	articulationRoot.mArticulationOwner=owner;
	Node& node=mNodeManager.get(hook.index);
	node.setRootArticulationId(articulationRootId);
	mNumAddedArtics += 1;
	mHasAnythingChanged = true;
}

void PxsIslandManager::removeNode(PxsIslandManagerNodeHook& hook)
{
	Node& node = mNodeManager.get(hook.index);
	const bool isKinematic = node.getIsKinematic();
	const bool isArticulated = node.getIsArticulated();
	PX_ASSERT(!isKinematic || !isArticulated);
	PX_ASSERT(!node.getIsDeleted());
	node.setIsDeleted();
	mNodeChangeManager.addDeletedNode(hook.index);

	if(!isArticulated)
	{
		mNumAddedRBodies += (!isKinematic ? -1 : 0);
		mNumAddedKinematics += (!isKinematic ? 0 : -1);
	}
	else if(node.getIsRootArticulationLink())
	{
		mNumAddedArtics -=  1;
	}

	if(node.getIsKinematic())
	{
		mNodeManager.deleteKinematicNode(hook.index);
	}

	hook.index=INVALID_NODE;

	mHasAnythingChanged = true;
}

void PxsIslandManager::addEdge(const eEdgeType edgeType, const PxsIslandManagerNodeHook body0hook, const PxsIslandManagerNodeHook body1hook, PxsIslandManagerEdgeHook& cmHook)
{
	PX_ASSERT(body0hook.index!=body1hook.index);
	PX_ASSERT(body0hook.isManaged() || body1hook.isManaged());
	PX_ASSERT(!cmHook.isManaged());
	const EdgeType freeEdge=mEdgeManager.getAvailableElem();
	cmHook.index = freeEdge;
	PX_CHECK_AND_RETURN(cmHook.isManaged(), INVALID_EDGE_REPORT);
	Edge& edge=mEdgeManager.get(freeEdge);
	edge.init();
	edge.setNode1(body0hook.index);
	edge.setNode2(body1hook.index);
	mEdgeChangeManager.addCreatedEdge(freeEdge);
	mNumAddedEdges[edgeType]++;
}

void PxsIslandManager::setEdgeRigidCM(const PxsIslandManagerEdgeHook& edgeHook, PxsContactManager* cm)
{
	PX_CHECK_AND_RETURN(edgeHook.isManaged(), INVALID_EDGE_REPORT);
	Edge& edge=mEdgeManager.get(edgeHook.index);
	edge.setCM(cm);
	mHasAnythingChanged = true;
}

void PxsIslandManager::clearEdgeRigidCM(const PxsIslandManagerEdgeHook& edgeHook)
{
	PX_CHECK_AND_RETURN(edgeHook.isManaged(), INVALID_EDGE_REPORT);
	Edge& edge=mEdgeManager.get(edgeHook.index);
	edge.clearCM();
	mHasAnythingChanged = true;
}

void PxsIslandManager::setEdgeConstraint(const PxsIslandManagerEdgeHook& edgeHook, PxsConstraint* constraint)
{
	PX_CHECK_AND_RETURN(edgeHook.isManaged(), INVALID_EDGE_REPORT);
	PX_ASSERT(constraint);
	Edge& edge=mEdgeManager.get(edgeHook.index);
	edge.setConstraint(constraint);
	mHasAnythingChanged = true;
}

void PxsIslandManager::setEdgeArticulationJoint(const PxsIslandManagerEdgeHook& edgeHook)
{
	PX_CHECK_AND_RETURN(edgeHook.isManaged(), INVALID_EDGE_REPORT);
	Edge& edge=mEdgeManager.get(edgeHook.index);
	edge.setArticulation();
	mHasAnythingChanged = true;
}

void PxsIslandManager::setEdgeConnected(const PxsIslandManagerEdgeHook& edgeHook)
{
	PX_CHECK_AND_RETURN(edgeHook.isManaged(), INVALID_EDGE_REPORT);
	Edge& edge=mEdgeManager.get(edgeHook.index);
	if (!edge.getIsConnected())  //The pair is created in the connected state. Hence, setEdgeConnected might get called twice if NP then reports a touch.
	{
		edge.setConnected();
		mEdgeChangeManager.addJoinedEdge(edgeHook.index);
		mHasAnythingChanged = true;
	}
}

void PxsIslandManager::setEdgeUnconnected(const PxsIslandManagerEdgeHook& edgeHook)
{
	PX_CHECK_AND_RETURN(edgeHook.isManaged(), INVALID_EDGE_REPORT);
	Edge& edge=mEdgeManager.get(edgeHook.index);
	if (edge.getIsConnected())  //This might get called as part of the pair activation even if the pair is already unconnected.
	{
		edge.setUnconnected();
		mEdgeChangeManager.addBrokenEdge(edgeHook.index);
		mHasAnythingChanged = true;
	}
}

void PxsIslandManager::removeEdge(const eEdgeType edgeType, PxsIslandManagerEdgeHook& edgeHook)
{
	PX_CHECK_AND_RETURN(edgeHook.isManaged(), INVALID_EDGE_REPORT);
	Edge& edge=mEdgeManager.get(edgeHook.index);
	PX_ASSERT(!edge.getIsRemoved());
	edge.setRemoved();
	mEdgeChangeManager.addDeletedEdge(edgeHook.index);
	edgeHook.index=INVALID_EDGE;
	mNumAddedEdges[edgeType]--;
	mHasAnythingChanged = true;
}

void PxsIslandManager::freeBuffers()
{
	//CCD can call postSolver so we can get here an extra time but there will be nothing to do for ccd passes.
	if(NULL==mIslandManagerUpdateWorkBuffers.mIslandBitmap1Words)
	{
		return;
	}

	//Merge all islands involving duplicated kinematics.
	//CCD can call postSolver so we can get here an extra time but there will be nothing to do for ccd passes.
	if(mPerformIslandUpdate && mNumAddedKinematics>0)
	{
		NodeType* kinematicProxySourceNodeIds=mIslandManagerUpdateWorkBuffers.mKinematicProxySourceNodeIds;
		NodeType* kinematicProxyNextNodeIds=mIslandManagerUpdateWorkBuffers.mKinematicProxyNextNodeIds;

		IslandType* graphStartIslands=mIslandManagerUpdateWorkBuffers.mGraphStartIslands;
		IslandType* graphNextIslands=mIslandManagerUpdateWorkBuffers.mGraphNextIslands;

		Cm::BitMap& kinematicIslandsBitmap=*mIslandManagerUpdateWorkBuffers.mIslandBitmap2;
		kinematicIslandsBitmap.clearFast();

		const Cm::BitMap& kinematicNodesBitmap=mNodeManager.getKinematicNodeBitmap();

		mergeKinematicProxiesBackToSource
			(kinematicNodesBitmap,
			kinematicProxySourceNodeIds, kinematicProxyNextNodeIds,
			mNodeManager, mEdgeManager, mIslands,
			kinematicIslandsBitmap,
			graphStartIslands,graphNextIslands);
	}

	//Work out if everything is asleep.
	//If there are no islands for the solver then there is no solver work to do and we can safely say that everything is asleep.
	//If everything is asleep and nothing changes before the next update then we can do nothing in the next update.
	if(0 == mProcessSleepingIslandsComputeData.mIslandIndicesSize)
	{
		mEverythingAsleep = true;
	}
	else
	{
		mEverythingAsleep = false;
	}
	mHasAnythingChanged = false;
	mPerformIslandUpdate = false;

	mIslandManagerUpdateWorkBuffers.mIslandBitmap1Words=NULL;
	mIslandManagerUpdateWorkBuffers.mIslandBitmap2Words=NULL;

	mIslandManagerUpdateWorkBuffers.mKinematicProxySourceNodeIds=NULL;
	mIslandManagerUpdateWorkBuffers.mKinematicProxyNextNodeIds=NULL;
	mIslandManagerUpdateWorkBuffers.mKinematicProxyLastNodeIds=NULL;
	mIslandManagerUpdateWorkBuffers.mGraphNextNodes=NULL;
	mIslandManagerUpdateWorkBuffers.mGraphStartIslands=NULL;
	mIslandManagerUpdateWorkBuffers.mGraphNextIslands=NULL;

	mProcessSleepingIslandsComputeData.mDataBlock=NULL;
	mProcessSleepingIslandsComputeData.mBodiesToWakeOrSleep=NULL;
	mProcessSleepingIslandsComputeData.mNarrowPhaseContactManagers=NULL;
	mProcessSleepingIslandsComputeData.mSolverBodyMap=NULL;
	mProcessSleepingIslandsComputeData.mSolverKinematics=NULL;
	mProcessSleepingIslandsComputeData.mSolverBodies=NULL;
	mProcessSleepingIslandsComputeData.mSolverArticulations=NULL;
	mProcessSleepingIslandsComputeData.mSolverArticulationOwners=NULL;
	mProcessSleepingIslandsComputeData.mSolverContactManagers=NULL;
	mProcessSleepingIslandsComputeData.mSolverConstraints=NULL;
	mProcessSleepingIslandsComputeData.mIslandIndices=NULL;
}

PX_FORCE_INLINE PxU32 alignSize16(const PxU32 size)
{
	return ((size + 15) & ~15);
}

PxU32 PxsIslandManager::resizeForKinematics()
{
	//First count the number of available nodes.
	const PxU32 numAvailableNodes=mNodeManager.getNumAvailableElems();

	//Now count the number of edges that reference a kinematic.
	PxU32 numEdgesWithKinematicNode=0;
	const Node* PX_RESTRICT allNodes=mNodeManager.getAll();
	const Edge* PX_RESTRICT allEdges=mEdgeManager.getAll();

	//First count the new edges that aren't connected.
	//(don't count new edges twice because new edges that are connected will also be in the joined edges array).
	//Any new kinematics can only be involved in a new edge so this takes care of all the new kinematics.
	const PxU32 createdEdgeSize=mEdgeChangeManager.getNumCreatedEdges();
	const EdgeType* PX_RESTRICT createdEdges=mEdgeChangeManager.getCreatedEdges();
	for(PxU32 i=0;i<createdEdgeSize;i++)
	{
		const EdgeType edgeId=createdEdges[i];
		PX_ASSERT(edgeId < mEdgeManager.getCapacity());
		const Edge& edge=allEdges[edgeId];
		if(!edge.getIsConnected())
		{
			const NodeType nodeId1=edge.getNode1();
			if(INVALID_NODE!=nodeId1)
			{
				PX_ASSERT(nodeId1<mNodeManager.getCapacity());
				const Node& node1=allNodes[nodeId1];
				if(node1.getIsKinematic())
				{
					numEdgesWithKinematicNode++;				
				}
			}

			const NodeType nodeId2=edge.getNode2();
			if(INVALID_NODE!=nodeId2)
			{
				PX_ASSERT(nodeId2<mNodeManager.getCapacity());
				const Node& node2=allNodes[nodeId2];
				if(node2.getIsKinematic())
				{
					numEdgesWithKinematicNode++;				
				}
			}
		}
	}

	//Now count the joined edges.
	const PxU32 joinedEdgesSize=mEdgeChangeManager.getNumJoinedEdges();
	const EdgeType* PX_RESTRICT joinedEdges=mEdgeChangeManager.getJoinedEdges();
	for(PxU32 i=0;i<joinedEdgesSize;i++)
	{
		const EdgeType edgeId=joinedEdges[i];
		PX_ASSERT(edgeId < mEdgeManager.getCapacity());
		const Edge& edge=allEdges[edgeId];

		const NodeType nodeId1=edge.getNode1();
		if(INVALID_NODE!=nodeId1)
		{
			PX_ASSERT(nodeId1<mNodeManager.getCapacity());
			const Node& node1=allNodes[nodeId1];
			if(node1.getIsKinematic())
			{
				numEdgesWithKinematicNode++;				
			}
		}

		const NodeType nodeId2=edge.getNode2();
		if(INVALID_NODE!=nodeId2)
		{
			PX_ASSERT(nodeId2<mNodeManager.getCapacity());
			const Node& node2=allNodes[nodeId2];
			if(node2.getIsKinematic())
			{
				numEdgesWithKinematicNode++;				
			}
		}
	}

	//Now count the edges that have already been sorted into islands.
	//No need to count edges that have been removed.
	if(mBuffer)
	{
		//Compute the islands affected by a kinematic.
		Cm::BitMap islandsAffectedBitmap;

		//Set up the bitmap words.
		const PxU32 islandsAffectedWordCount = (mIslands.getCapacity() >> 5);
		const PxU32 islandsAffectedWordsByteSize = alignSize16(sizeof(PxU32)*islandsAffectedWordCount);
		PX_ASSERT(islandsAffectedWordsByteSize<=mBufferSize);
		PxU32* islandsAffectedWords=(PxU32*)(mBuffer);
		PxMemZero(islandsAffectedWords, islandsAffectedWordsByteSize);
		islandsAffectedBitmap.setWords(islandsAffectedWords,islandsAffectedWordCount);

		//Set the bitmap of islands affected by kinematic nodes.
		{
			const Cm::BitMap& kinematicNodeBitmap=mNodeManager.getKinematicNodeBitmap();
			const PxU32* PX_RESTRICT kinematicNodeBitmapWords=kinematicNodeBitmap.getWords();
			const PxU32 lastSetBit = kinematicNodeBitmap.findLast();
			for(PxU32 w = 0; w <= lastSetBit >> 5; ++w)
			{
				for(PxU32 b = kinematicNodeBitmapWords[w]; b; b &= b-1)
				{
					const NodeType kinematicNodeId = (NodeType)(w<<5|Ps::lowestSetBit(b));
					PX_ASSERT(kinematicNodeId<mNodeManager.getCapacity());
					const Node& kinematicNode=allNodes[kinematicNodeId];
					PX_ASSERT(kinematicNode.getIsKinematic());
					PX_ASSERT(!kinematicNode.getIsDeleted());
					if(!kinematicNode.getIsNew())
					{
						PX_ASSERT(kinematicNode.getIslandId()!=INVALID_ISLAND);
						PX_ASSERT(kinematicNode.getIslandId() < mIslands.getCapacity());
						islandsAffectedBitmap.set(kinematicNode.getIslandId());
					}
				}
			}
		}

		//Iterate over all affected islands and count the number of edges that reference a kinematic.
		const PxU32 lastSetBit = islandsAffectedBitmap.findLast();
		const Island* PX_RESTRICT allIslands=mIslands.getAll();
		const EdgeType* PX_RESTRICT nextEdgeIds=mEdgeManager.getNextEdgeIds();
		for(PxU32 w = 0; w <= lastSetBit >> 5; ++w)
		{
			for(PxU32 b = islandsAffectedWords[w]; b; b &= b-1)
			{
				const IslandType islandId = (IslandType)(w<<5|Ps::lowestSetBit(b));
				PX_ASSERT(islandId < mIslands.getCapacity());
				const Island& island=allIslands[islandId];
				const EdgeType startEdge=island.mStartEdgeId;
				EdgeType nextEdge=startEdge;
				while(INVALID_EDGE!=nextEdge)
				{
					PX_ASSERT(nextEdge < mEdgeManager.getCapacity());
					const Edge& edge=allEdges[nextEdge];
					if(!edge.getIsRemoved())
					{
						const NodeType nodeId1=edge.getNode1();
						if(INVALID_NODE!=nodeId1)
						{
							PX_ASSERT(nodeId1<mNodeManager.getCapacity());
							const Node& node1=allNodes[nodeId1];
							if(node1.getIsKinematic())
							{
								numEdgesWithKinematicNode++;				
							}
						}

						const NodeType nodeId2=edge.getNode2();
						if(INVALID_NODE!=nodeId2)
						{
							PX_ASSERT(nodeId2<mNodeManager.getCapacity());
							const Node& node2=allNodes[nodeId2];
							if(node2.getIsKinematic())
							{
								numEdgesWithKinematicNode++;				
							}
						}
					}

					nextEdge=nextEdgeIds[nextEdge];
				}//edges
			}//bitmap word
		}//bitmap words
	}

	const PxU32 requiredNodeCapacity = mNodeManager.getCapacity() - numAvailableNodes + numEdgesWithKinematicNode;
	const PxU32 currentCapacity = mNodeManager.getCapacity();
	if(requiredNodeCapacity > currentCapacity)
	{
		const PxU32 roundedCapacity = ((requiredNodeCapacity + 31) & ~31);
		mNodeManager.resize(roundedCapacity);
	}

	return numEdgesWithKinematicNode;
}

void PxsIslandManager::resizeArrays()
{
	//Compute the number of edges that reference a kinematic node.
	//We will create a proxy kinematic node for each referenced kinematic node.
	//Don't forget that a kinematic not referenced by an edge still needs to exist in an island.
	PxU32 numEdgesWithKinematicNodes=resizeForKinematics() + mNumAddedKinematics;
	mNumEdgesWithKinematicNodes=numEdgesWithKinematicNodes;

	const PxU32 allNodesCapacity=mNodeManager.getCapacity();
	const PxU32 allEdgesCapacity=mEdgeManager.getCapacity();
	PX_UNUSED(allEdgesCapacity);

	const PxU32 islandsAffectedWordCount = (allNodesCapacity >> 5);
	const PxU32 emptyIslandsWordCount = (allNodesCapacity >> 5);

	const PxU32 numContactManagerEdges = mNumAddedEdges[EDGE_TYPE_CONTACT_MANAGER];
	const PxU32 numConstraintEdges = mNumAddedEdges[EDGE_TYPE_CONSTRAINT];
	const PxU32 numArticEdges = mNumAddedEdges[EDGE_TYPE_ARTIC];
	const PxU32 numEdges = numContactManagerEdges + numConstraintEdges + numArticEdges;
	PX_ASSERT(numEdges<=allEdgesCapacity);

	const PxU32 numBodiesToWakeSleep = mNumAddedRBodies + mNumAddedArtics;
	const PxU32 numSolverBodyItems = mNumAddedRBodies + mNumEdgesWithKinematicNodes + mNumAddedArtics;

	const PxU32 kinematicSourceNodesByteSize = alignSize16(sizeof(NodeType)*allNodesCapacity);
	const PxU32 kinematicProxyNextNodeIdsByteSize = alignSize16(sizeof(NodeType)*allNodesCapacity);
	const PxU32 kinematicProxyLastNodeIdsByteSize = alignSize16(sizeof(NodeType)*allNodesCapacity);

	const PxU32 bodiesToWakeOrSleepByteSize =  alignSize16(sizeof(PxU8*)*numBodiesToWakeSleep);
	const PxU32 npContactManagersByteSize = alignSize16(sizeof(NarrowPhaseContactManager)*numEdges);
	const PxU32 solverBodyMapByteSize = alignSize16(sizeof(NodeType)*allNodesCapacity);
	const PxU32 solverRBodyByteSize = alignSize16(sizeof(PxsRigidBody*)*mNumAddedRBodies);
	const PxU32 solverKinematicsByteSize = alignSize16(sizeof(PxsRigidBody*)*numEdgesWithKinematicNodes);
	const PxU32 solverArticByteSize = alignSize16(sizeof(PxsArticulation*)*mNumAddedArtics);
	const PxU32 solverArticOwnerByteSize = alignSize16(sizeof(void*)*mNumAddedArtics);
	const PxU32 solverContactManagerByteSize = alignSize16(sizeof(PxsIndexedContactManager)*numContactManagerEdges);
	const PxU32 solverConstraintByteSize = alignSize16(sizeof(PxsIndexedConstraint)*numConstraintEdges);
	const PxU32 islandIndicesByteSize = alignSize16(sizeof(PxsIslandIndices)*(numSolverBodyItems+1));

	const PxU32 islandsAffectedWordsByteSize = alignSize16(sizeof(PxU32)*islandsAffectedWordCount);
	const PxU32 emptyIslandsWordsByteSize = alignSize16(sizeof(PxU32)*emptyIslandsWordCount);

	const PxU32 graphNextNodesByteSize = alignSize16(sizeof(NodeType)*allNodesCapacity);
	const PxU32 graphStartIslandsByteSize = alignSize16(sizeof(IslandType)*allNodesCapacity);
	const PxU32 graphNextIslandsByteSize = alignSize16(sizeof(IslandType)*allNodesCapacity);

	const PxU32 solverDataByteSize=
		bodiesToWakeOrSleepByteSize + 
		npContactManagersByteSize +
		solverBodyMapByteSize + 
		solverKinematicsByteSize + 
		solverRBodyByteSize + 
		solverArticByteSize + 
		solverArticOwnerByteSize +
		solverContactManagerByteSize +
		solverConstraintByteSize +
		islandIndicesByteSize;

	const PxU32 persistentWorkbufferByteSize=
		kinematicSourceNodesByteSize +
		kinematicProxyNextNodeIdsByteSize +
		kinematicProxyLastNodeIdsByteSize +
		solverDataByteSize;

	const PxU32 nonPersistentworkBufferByteSize=
		islandsAffectedWordsByteSize+
		emptyIslandsWordsByteSize+
		graphNextNodesByteSize+
		graphStartIslandsByteSize+
		graphNextIslandsByteSize;

	const PxU32 byteSize=persistentWorkbufferByteSize+nonPersistentworkBufferByteSize;

	if(byteSize>mBufferSize)
	{
		//Need to allocate byteSize but just add on 16K for good measure.
		mBufferSize=byteSize + 16384;
		if(mBuffer) PX_FREE(mBuffer);
		mBuffer=(PxU8*)PX_ALLOC(mBufferSize, PX_DEBUG_EXP("PxsIslandManager"));
	}
	else if((byteSize < (mBufferSize>>1)) && byteSize>=16384)
	{
		//Reduce allocation down to minimum limit of 16K.
		mBufferSize=byteSize;
		if(mBuffer) PX_FREE(mBuffer);
		mBuffer=(PxU8*)PX_ALLOC(mBufferSize, PX_DEBUG_EXP("PxsIslandManager"));
	}

	PX_ASSERT(byteSize<=mBufferSize);
	PxU8* PX_RESTRICT newBuffer=(PxU8*)mBuffer;
	PxU32 offset=0;

	//////////////////////////////
	//PERSISTENT WORK BUFFER DATA
	//////////////////////////////

	//Array for kinematic proxy management.
	PX_ASSERT(NULL==mIslandManagerUpdateWorkBuffers.mKinematicProxySourceNodeIds);
	mIslandManagerUpdateWorkBuffers.mKinematicProxySourceNodeIds = (NodeType*)(newBuffer + offset);
	offset += kinematicSourceNodesByteSize;

	//Array for kinematic proxy management.
	PX_ASSERT(NULL==mIslandManagerUpdateWorkBuffers.mKinematicProxyNextNodeIds);
	mIslandManagerUpdateWorkBuffers.mKinematicProxyNextNodeIds = (NodeType*)(newBuffer + offset);
	offset += kinematicProxyNextNodeIdsByteSize;

	PX_ASSERT(NULL==mIslandManagerUpdateWorkBuffers.mKinematicProxyLastNodeIds);
	mIslandManagerUpdateWorkBuffers.mKinematicProxyLastNodeIds = (NodeType*)(newBuffer + offset);
	offset += kinematicProxyLastNodeIdsByteSize;


	//////////////////////////////
	//SOLVER DATA
	//////////////////////////////

	mProcessSleepingIslandsComputeData.mDataBlock = (newBuffer + offset);
	mProcessSleepingIslandsComputeData.mDataBlockSize = solverDataByteSize;

	//Resize the array of bodies to wake up to be big enough for all bodies.
	PX_ASSERT(NULL==mProcessSleepingIslandsComputeData.mBodiesToWakeOrSleep);
	mProcessSleepingIslandsComputeData.mBodiesToWakeOrSleep=(PxU8**)(newBuffer + offset);
	offset+=bodiesToWakeOrSleepByteSize;
	mProcessSleepingIslandsComputeData.mBodiesToWakeOrSleepCapacity=numBodiesToWakeSleep;
	mProcessSleepingIslandsComputeData.mBodiesToWakeSize=0;
	mProcessSleepingIslandsComputeData.mBodiesToSleepSize=0;

	//Resize the array of body pairs for 2nd np pass back to the default size.
	PX_ASSERT(NULL==mProcessSleepingIslandsComputeData.mNarrowPhaseContactManagers);
	mProcessSleepingIslandsComputeData.mNarrowPhaseContactManagers=(NarrowPhaseContactManager*)(newBuffer + offset);
	offset+=npContactManagersByteSize;
	mProcessSleepingIslandsComputeData.mNarrowPhaseContactManagersCapacity=numEdges;
	mProcessSleepingIslandsComputeData.mNarrowPhaseContactManagersSize=0;

	//Resize the array that maps between entry id in mNodeMananger and entry id in mSolverBodies
	PX_ASSERT(NULL==mProcessSleepingIslandsComputeData.mSolverBodyMap);
	mProcessSleepingIslandsComputeData.mSolverBodyMap=(NodeType*)(newBuffer + offset);
	offset+=solverBodyMapByteSize;
	mProcessSleepingIslandsComputeData.mSolverBodyMapCapacity=allNodesCapacity;

	//Resize the array of solver kinematics to be big enough for all bodies.
	PX_ASSERT(NULL==mProcessSleepingIslandsComputeData.mSolverKinematics);
	mProcessSleepingIslandsComputeData.mSolverKinematics=(PxsRigidBody**)(newBuffer+offset);
	offset+=solverKinematicsByteSize;
	mProcessSleepingIslandsComputeData.mSolverKinematicsCapacity=numEdgesWithKinematicNodes;
	mProcessSleepingIslandsComputeData.mSolverKinematicsSize=0;

	//Resize the array of solver bodies to to be big enough for all bodies.
	PX_ASSERT(NULL==mProcessSleepingIslandsComputeData.mSolverBodies);
	mProcessSleepingIslandsComputeData.mSolverBodies=(PxsRigidBody**)(newBuffer + offset);
	offset+=solverRBodyByteSize;
	mProcessSleepingIslandsComputeData.mSolverBodiesCapacity=mNumAddedRBodies;
	mProcessSleepingIslandsComputeData.mSolverBodiesSize=0;

	//Resize the array of solver articulations to to be big enough for all articulations
	PX_ASSERT(NULL==mProcessSleepingIslandsComputeData.mSolverArticulations);
	PX_ASSERT(NULL==mProcessSleepingIslandsComputeData.mSolverArticulationOwners);
	mProcessSleepingIslandsComputeData.mSolverArticulations=(PxsArticulation**)(newBuffer+offset);
	offset+=solverArticByteSize;
	mProcessSleepingIslandsComputeData.mSolverArticulationOwners=(void**)(newBuffer+offset);
	offset+=solverArticByteSize;
	mProcessSleepingIslandsComputeData.mSolverArticulationsCapacity=mNumAddedArtics;
	mProcessSleepingIslandsComputeData.mSolverArticulationsSize=0;

	//Resize the array of solver contact managers to to be big enough for all edges.
	PX_ASSERT(NULL==mProcessSleepingIslandsComputeData.mSolverContactManagers);
	mProcessSleepingIslandsComputeData.mSolverContactManagers=(PxsIndexedContactManager*)(newBuffer+offset);
	offset+=solverContactManagerByteSize;
	mProcessSleepingIslandsComputeData.mSolverContactManagersCapacity=numContactManagerEdges;
	mProcessSleepingIslandsComputeData.mSolverContactManagersSize=0;

	//Resize the array of solver constraints
	PX_ASSERT(NULL==mProcessSleepingIslandsComputeData.mSolverConstraints);
	mProcessSleepingIslandsComputeData.mSolverConstraints=(PxsIndexedConstraint*)(newBuffer+offset);
	offset+=solverConstraintByteSize;
	mProcessSleepingIslandsComputeData.mSolverConstraintsCapacity=numConstraintEdges;
	mProcessSleepingIslandsComputeData.mSolverConstraintsSize=0;

	//Resize array of island indices.
	PX_ASSERT(NULL==mProcessSleepingIslandsComputeData.mIslandIndices);
	mProcessSleepingIslandsComputeData.mIslandIndices=(PxsIslandIndices*)(newBuffer+offset);
	offset+=islandIndicesByteSize;
#ifdef PX_PS3
	PX_COMPILE_TIME_ASSERT(sizeof(PxsIslandIndices) == 8);
#endif
	mProcessSleepingIslandsComputeData.mIslandIndicesCapacity=islandIndicesByteSize / sizeof(PxsIslandIndices); //for PS3 it is important that the end of this buffer is aligned as well
	mProcessSleepingIslandsComputeData.mIslandIndicesSize=0;
	mProcessSleepingIslandsComputeData.mIslandIndicesSecondPassSize = 0;

	PX_ASSERT(byteSize>=offset);

	//////////////////////////////
	//NONPERSISTENT WORK BUFFER DATA
	//////////////////////////////

	offset=persistentWorkbufferByteSize;

	//First island bitmap.
	PX_ASSERT(NULL==mIslandManagerUpdateWorkBuffers.mIslandBitmap1Words);
	mIslandManagerUpdateWorkBuffers.mIslandBitmap1Words=(PxU32*)(newBuffer+offset);
	offset+=islandsAffectedWordsByteSize;
	mIslandManagerUpdateWorkBuffers.mIslandBitmap1WordCount=islandsAffectedWordCount;
	PxMemZero(mIslandManagerUpdateWorkBuffers.mIslandBitmap1Words, sizeof(PxU32)*islandsAffectedWordCount);
	mIslandManagerUpdateWorkBuffers.mIslandBitmap1->setWords(mIslandManagerUpdateWorkBuffers.mIslandBitmap1Words,mIslandManagerUpdateWorkBuffers.mIslandBitmap1WordCount);

	//Second island bitmap.
	PX_ASSERT(NULL==mIslandManagerUpdateWorkBuffers.mIslandBitmap2Words);
	mIslandManagerUpdateWorkBuffers.mIslandBitmap2Words=(PxU32*)(newBuffer+offset);
	offset+=emptyIslandsWordsByteSize;
	mIslandManagerUpdateWorkBuffers.mIslandBitmap2WordCount=emptyIslandsWordCount;
	PxMemZero(mIslandManagerUpdateWorkBuffers.mIslandBitmap2Words, sizeof(PxU32)*emptyIslandsWordCount);
	mIslandManagerUpdateWorkBuffers.mIslandBitmap2->setWords(mIslandManagerUpdateWorkBuffers.mIslandBitmap2Words,mIslandManagerUpdateWorkBuffers.mIslandBitmap2WordCount);

	//Array for a copy of nodes for each island.
	PX_ASSERT(NULL==mIslandManagerUpdateWorkBuffers.mGraphNextNodes);
	mIslandManagerUpdateWorkBuffers.mGraphNextNodes = (NodeType*)(newBuffer + offset);
	offset += graphNextNodesByteSize;

	//Array for kinematic dulicates management.
	PX_ASSERT(NULL==mIslandManagerUpdateWorkBuffers.mGraphStartIslands);
	mIslandManagerUpdateWorkBuffers.mGraphStartIslands = (IslandType*)(newBuffer + offset);
	offset += graphStartIslandsByteSize;

	//Array for kinematic dulicates management.
	PX_ASSERT(NULL==mIslandManagerUpdateWorkBuffers.mGraphNextIslands);
	mIslandManagerUpdateWorkBuffers.mGraphNextIslands = (IslandType*)(newBuffer + offset);
	offset += graphNextIslandsByteSize;

	PX_ASSERT(byteSize>=offset);


	//Resize islands to make sure that we have one island per node.
	if(allNodesCapacity>mIslands.getCapacity())
	{
		mIslands.resize(allNodesCapacity);
	}

	//This can be set right now - no need to do this later.
	mIslandObjects.bodies=mProcessSleepingIslandsComputeData.mSolverBodies;
	mIslandObjects.articulations=mProcessSleepingIslandsComputeData.mSolverArticulations;
	mIslandObjects.articulationOwners=mProcessSleepingIslandsComputeData.mSolverArticulationOwners;
	mIslandObjects.contactManagers=mProcessSleepingIslandsComputeData.mSolverContactManagers;
	mIslandObjects.constraints=mProcessSleepingIslandsComputeData.mSolverConstraints;
}


void PxsIslandManager::cleanupEdgeEvents()
{
	const PxU32 oldBrokenEdgesSize = mEdgeChangeManager.getNumBrokenEdges();
	const PxU32 oldJoinedEdgesSize = mEdgeChangeManager.getNumJoinedEdges();

	if (oldBrokenEdgesSize && oldJoinedEdgesSize)
	{
		const PxU32 edgeCapacity = mEdgeManager.getCapacity();
		const PxU32 necessaryByteCapacity = edgeCapacity * sizeof(PxI32);
		
		PxU8* workBuffer;
		bool scratchAllocated = false;
		if (mProcessSleepingIslandsComputeData.mDataBlockSize >= necessaryByteCapacity)
			workBuffer = mProcessSleepingIslandsComputeData.mDataBlock;
		else
		{
			workBuffer = (PxU8*)mScratchAllocator.alloc(necessaryByteCapacity, true);
			scratchAllocated = true;
		}

		if (workBuffer)
		{
			PxMemZero(workBuffer, necessaryByteCapacity);
			mEdgeChangeManager.cleanupEdgeEvents((PxI32*)workBuffer, edgeCapacity);

			if (!scratchAllocated)
				return;
			else
			{
				mScratchAllocator.free(workBuffer);
				return;
			}
		}
		else
		{
			Ps::getFoundation().error(PxErrorCode::eOUT_OF_MEMORY, __FILE__, __LINE__, "Island generation: objects might not get woken up or put to sleep properly.");
			mEdgeChangeManager.clearJoinedEdgeEvents();
			mEdgeChangeManager.clearBrokenEdgeEvents();
		}
	}
}


void PxsIslandManager::setWokenPairContactManagers()
{
	Edge* allEdges=mEdgeManager.getAll();
	const PxU32 allEdgesCapacity=mEdgeManager.getCapacity();
	PX_UNUSED(allEdgesCapacity);

	NarrowPhaseContactManager* PX_RESTRICT narrowPhaseContactManagers=mProcessSleepingIslandsComputeData.mNarrowPhaseContactManagers;
	const PxU32 narrowPhaseContactManagersSize=mProcessSleepingIslandsComputeData.mNarrowPhaseContactManagersSize;

	for(PxU32 i=0;i<narrowPhaseContactManagersSize;i++)
	{
		//Get the edge and the cm of the edge.
		const EdgeType edgeId=narrowPhaseContactManagers[i].mEdgeId;
		PX_ASSERT(edgeId<allEdgesCapacity);
		Edge& edge=allEdges[edgeId];
		PX_ASSERT(edge.getIsTypeCM());
		narrowPhaseContactManagers[i].mCM=edge.getCM();
	}
}

void PxsIslandManager::removeSleepingPairContactManagers()
{
	Edge* allEdges=mEdgeManager.getAll();
	const PxU32 allEdgesCapacity=mEdgeManager.getCapacity();
	PX_UNUSED(allEdgesCapacity);

	NarrowPhaseContactManager* PX_RESTRICT narrowPhaseContactManagers=mProcessSleepingIslandsComputeData.mNarrowPhaseContactManagers;
	const PxU32 narrowPhaseContactManagersSize=mProcessSleepingIslandsComputeData.mNarrowPhaseContactManagersSize;

	PxU32 newNarrowPhaseContactManagersSize = 0;
	for(PxU32 i=0;i<narrowPhaseContactManagersSize;i++)
	{
		//Get the edge and the cm of the edge.
		const EdgeType edgeId=narrowPhaseContactManagers[i].mEdgeId;
		PX_ASSERT(edgeId<allEdgesCapacity);
		Edge& edge=allEdges[edgeId];
		PX_ASSERT(edge.getIsTypeCM());
		PxsContactManager* PX_RESTRICT cm=edge.getCM();

		if (cm)
		{
			narrowPhaseContactManagers[newNarrowPhaseContactManagersSize] = narrowPhaseContactManagers[i];
			newNarrowPhaseContactManagersSize++;
		}
	}

	mProcessSleepingIslandsComputeData.mNarrowPhaseContactManagersSize = newNarrowPhaseContactManagersSize;
}

void PxsIslandManager::finishSolverIslands()
{
#ifdef LOG_ISLANDGEN

	const PxU32 eos=0xffffffff;

	//Write all the islands.

	const Cm::BitMap& islandBitMap=mIslands.getBitmap();
	const PxU32 lastSetBit = islandBitMap.findLast();
	for(PxU32 w = 0; w <= lastSetBit >> 5; ++w)
	{
		for(PxU32 b = islandBitMap.getWords()[w]; b; b &= b-1)
		{
			const IslandType islandId = (w<<5|Ps::lowestSetBit(b));
			Island& island=mIslands.get(islandId);

			fwrite(&islandId, sizeof(PxU32), 1, gIslandGenLogFile);


			//Write the island nodes.
			{
				const NodeType* nextNodeIds=mNodeManager.getNextNodeIds();
				NodeType nextNodeId=island.mStartNodeId;
				while(INVALID_NODE!=nextNodeId)
				{
					//Write the node id.
					fwrite(&nextNodeId, sizeof(NodeType), 1, gIslandGenLogFile);

					//Write the node coords.
					const Node& node=mNodeManager.get(nextNodeId);
					const PxsRigidBody* rb=node.getRigidBody(mRigidBodyOffset);
					const PxTransform& t=rb->getPose();
					fwrite(&t.p.x, sizeof(PxF32), 1, gIslandGenLogFile);
					fwrite(&t.p.y, sizeof(PxF32), 1, gIslandGenLogFile);
					fwrite(&t.p.z, sizeof(PxF32), 1, gIslandGenLogFile);

					//Write the node flags.
					const PxU32 flags=node.getFlags();
					fwrite(&flags, sizeof(PxU32), 1, gIslandGenLogFile);

					nextNodeId=nextNodeIds[nextNodeId];
				}
				fwrite(&eos, sizeof(PxU32), 1, gIslandGenLogFile);
			}

			//Write the island edges.
			{
				const EdgeType* nextEdgeIds=mEdgeManager.getNextEdgeIds();
				EdgeType nextEdgeId=island.mStartEdgeId;
				while(INVALID_EDGE!=nextEdgeId)
				{
					//Write the edge id.
					fwrite(&nextEdgeId, sizeof(EdgeType), 1, gIslandGenLogFile);

					//Write the edge flags.
					Edge& edge=mEdgeManager.get(nextEdgeId);
					const PxU32 flags=((size_t)edge.getCM() & 0x0f);
					fwrite(&flags, sizeof(PxU32), 1, gIslandGenLogFile);

					nextEdgeId=nextEdgeIds[nextEdgeId];
				}
				fwrite(&eos, sizeof(PxU32), 1, gIslandGenLogFile);
			}
		}
	}
	fwrite(&eos, sizeof(PxU32), 1, gIslandGenLogFile);

	fflush(gIslandGenLogFile);

	gFrameCount++;

#endif
}

#ifdef PX_DEBUG
PxU32 IslandManager::getNumUsedIslands() const 
{
	PxU32 count=0;
	const Cm::BitMap& bitmap=getBitmap();
	const PxU32 lastSetBit = bitmap.findLast();
	for(PxU32 w = 0; w <= lastSetBit >> 5; ++w)
	{
		for(PxU32 b = bitmap.getWords()[w]; b; b &= b-1)
		{
			count++;
		}
	}
	return count;
}

PxU32 IslandManager::getNumFreeIslands() const
{
	PxU32 count=0;
	IslandType nextFree=(IslandType)mNextFreeElem;
	while(nextFree!=INVALID_ISLAND)
	{
		count++;
		nextFree=mFreeElems[nextFree];
	}
	return count;
}

bool PxsIslandManager::isValid()
{
	PxU32 numNodes=0;

	const Node* PX_RESTRICT allNodes=mNodeManager.getAll();
	const PxU32 allNodesCapacity=mNodeManager.getCapacity();
	const Edge* PX_RESTRICT allEdges=mEdgeManager.getAll();
	const PxU32 allEdgesCapacity=mEdgeManager.getCapacity();
	const NodeType* PX_RESTRICT nextNodeIds=mNodeManager.getNextNodeIds();
	const EdgeType* PX_RESTRICT nextEdgeIds=mEdgeManager.getNextEdgeIds();
	Cm::BitMap& islandBitmap=mIslands.getBitmap();

	const PxU32 lastSetBit = islandBitmap.findLast();
	for(PxU32 w = 0; w <= lastSetBit >> 5; ++w)
	{
		for(PxU32 b = islandBitmap.getWords()[w]; b; b &= b-1)
		{
			const IslandType islandId = (IslandType)(w<<5|Ps::lowestSetBit(b));
			Island& island=mIslands.get(islandId);

			//Test that all nodes in the island know they are in the island.
			NodeType nextNode=island.mStartNodeId;
			while(INVALID_NODE!=nextNode)
			{
				numNodes++;
				PX_ASSERT(nextNode<allNodesCapacity);
				if(allNodes[nextNode].getIslandId()!=islandId)
				{
					return false;
				}
				nextNode=nextNodeIds[nextNode];
			}

			//Test that all edges in the island reference two nodes that are in the island.
			EdgeType nextEdge=island.mStartEdgeId;
			while(INVALID_EDGE!=nextEdge)
			{
				PX_ASSERT(nextEdge<allEdgesCapacity);
				const Edge& edge=allEdges[nextEdge];

				//Test that node1 is in the correct island.
				if(INVALID_NODE!=edge.getNode1())
				{
					PX_ASSERT(edge.getNode1()<allNodesCapacity);
					if(allNodes[edge.getNode1()].getIslandId()!=islandId)
					{
						return false;
					}
				}

				//Test that node2 is in the correct island.
				if(INVALID_NODE!=edge.getNode2())
				{
					PX_ASSERT(edge.getNode2()<allNodesCapacity);
					if(allNodes[edge.getNode2()].getIslandId()!=islandId)
					{
						return false;
					}
				}

				/*
				//Test that the edge does not join a kinematic pair.
				if(INVALID_NODE!=edge.getNode1() && INVALID_NODE!=edge.getNode2())
				{
					PX_ASSERT(edge.getNode1()<allNodesCapacity);
					PX_ASSERT(edge.getNode2()<allNodesCapacity);
					const Node& node1=mNodeManager.get(edge.getNode1());
					const Node& node2=mNodeManager.get(edge.getNode2());
					if(node1.getIsKinematic() && node2.getIsKinematic())
					{
						return false;
					}
				}
				*/

				//Test that the edge does not join a static pair.
				if(INVALID_NODE==edge.getNode1() && INVALID_NODE==edge.getNode2())
				{
					return false;
				}

				nextEdge=nextEdgeIds[nextEdge];
			}
		}
	}

	if(mIslands.getNumUsedIslands() + mIslands.getNumFreeIslands() != mIslands.getCapacity())
	{
		return false;
	}

	return true;
}
#endif

#ifdef PX_PS3
void PxsIslandManager::updateIslandsSPU(PxBaseTask* continuation, const PxU32 numSpus)
{
	mIslandGenSpuTask = PX_PLACEMENT_NEW(mIslandGenSpuTask, IslandGenSpuTask)(this, PxMin(numSpus, (PxU32)1), false);
	mIslandGenSpuTask->setContinuation(continuation);
	mIslandGenSpuTask->removeReference();
}

void PxsIslandManager::updateIslandsSecondPassSPU(PxBaseTask* continuation, const PxU32 numSpus)
{
	mIslandGenSecondPassSpuTask = PX_PLACEMENT_NEW(mIslandGenSecondPassSpuTask, IslandGenSpuTask)(this, PxMin(numSpus, (PxU32)1), true);
	mIslandGenSecondPassSpuTask->setContinuation(continuation);
	mIslandGenSecondPassSpuTask->removeReference();
}
#endif

void PxsIslandManager::updateIslands()
{
	updateIslandsMain(
		mRigidBodyOffset,
		mNodeChangeManager.getDeletedNodes(),mNodeChangeManager.getNumDeletedNodes(),
		mNodeChangeManager.getCreatedNodes(),mNodeChangeManager.getNumCreatedNodes(),
		mEdgeChangeManager.getDeletedEdges(),mEdgeChangeManager.getNumDeletedEdges(),
		mEdgeChangeManager.getCreatedEdges(),mEdgeChangeManager.getNumCreatedEdges(),
		mEdgeChangeManager.getBrokenEdges(),mEdgeChangeManager.getNumBrokenEdges(),
		mEdgeChangeManager.getJoinedEdges(),mEdgeChangeManager.getNumJoinedEdges(),
		mNodeManager.getKinematicNodeBitmap(),mNumAddedKinematics,
		mNodeManager,mEdgeManager,mIslands,mRootArticulationManager,
		mProcessSleepingIslandsComputeData,
		mIslandManagerUpdateWorkBuffers,
		mEventProfiler);

	PX_ASSERT(mProcessSleepingIslandsComputeData.mSolverBodiesSize<=mNumAddedRBodies);
	PX_ASSERT(mProcessSleepingIslandsComputeData.mSolverKinematicsSize<=mNumEdgesWithKinematicNodes);
	PX_ASSERT(mProcessSleepingIslandsComputeData.mSolverArticulationsSize<=mNumAddedArtics);

	PX_ASSERT(isValid());	
	mNodeChangeManager.reset();
	mEdgeChangeManager.reset();
}

void PxsIslandManager::updateIslands(PxBaseTask* continuation, const PxU32 numSpus)
{
	PX_UNUSED(continuation);
	PX_UNUSED(numSpus);
#ifdef LOG_ISLANDGEN

	PxU32 eos=0xffffffff;

	//Write the frame count.
	fwrite(&gFrameCount, sizeof(PxU32), 1, gIslandGenLogFile);
	fwrite(&eos, sizeof(PxU32), 1, gIslandGenLogFile);

	//Write the changed nodes.

	//Created nodes.
	{
		const PxU32 numCreatedNodes=mNodeChangeManager.getNumCreatedNodes();
		const NodeType* createdNodes=mNodeChangeManager.getCreatedNodes();
		fwrite(&numCreatedNodes, sizeof(PxU32), 1, gIslandGenLogFile);
		for(PxU32 i=0;i<numCreatedNodes;i++)
		{
			fwrite(&createdNodes[i], sizeof(NodeType), 1, gIslandGenLogFile);

			//Write the coordinate of the body.
			const Node& node=mNodeManager.get(createdNodes[i]);
			const PxsRigidBody* rb=node.getRigidBody(mRigidBodyOffset);
			const PxTransform t=rb->getPose();
			fwrite(&t.p.x, sizeof(PxReal), 1, gIslandGenLogFile);
			fwrite(&t.p.y, sizeof(PxReal), 1, gIslandGenLogFile);
			fwrite(&t.p.z, sizeof(PxReal), 1, gIslandGenLogFile);
		}
		fwrite(&eos, sizeof(PxU32), 1, gIslandGenLogFile);
	}
	//Deleted nodes.
	{
		const PxU32 numDeletedNodes=mNodeChangeManager.getNumDeletedNodes();
		const NodeType* deletedNodes=mNodeChangeManager.getDeletedNodes();
		fwrite(&numDeletedNodes, sizeof(PxU32), 1, gIslandGenLogFile);
		for(PxU32 i=0;i<numDeletedNodes;i++)
		{
			fwrite(&deletedNodes[i], sizeof(NodeType), 1, gIslandGenLogFile);
		}
		fwrite(&eos, sizeof(PxU32), 1, gIslandGenLogFile);
	}

	//Write the changed edges

	//Created edges
	{
		const PxU32 numCreatedEdges=mEdgeChangeManager.getNumCreatedEdges();
		const EdgeType* createdEdges=mEdgeChangeManager.getCreatedEdges();
		fwrite(&numCreatedEdges, sizeof(EdgeType), 1, gIslandGenLogFile);
		for(PxU32 i=0;i<numCreatedEdges;i++)
		{
			fwrite(&createdEdges[i], sizeof(EdgeType), 1, gIslandGenLogFile);

			//Write the connected nodes.
			Edge& edge=mEdgeManager.get(createdEdges[i]);
			const NodeType node1=edge.getNode1();
			const NodeType node2=edge.getNode2();
			const PxU32 flags =((size_t)edge.getCM() & 0x0f);
			fwrite(&node1, sizeof(NodeType), 1, gIslandGenLogFile);
			fwrite(&node2, sizeof(NodeType), 1, gIslandGenLogFile);
			fwrite(&flags, sizeof(PxU32), 1, gIslandGenLogFile);
		}
		fwrite(&eos, sizeof(PxU32), 1, gIslandGenLogFile);
	}

	//Deleted edges
	{
		const PxU32 numDeletedEdges=mEdgeChangeManager.getNumDeletedEdges();
		const EdgeType* deletedEdges=mEdgeChangeManager.getDeletedEdges();
		fwrite(&numDeletedEdges, sizeof(EdgeType), 1, gIslandGenLogFile);
		for(PxU32 i=0;i<numDeletedEdges;i++)
		{
			fwrite(&deletedEdges[i], sizeof(EdgeType), 1, gIslandGenLogFile);
		}
		fwrite(&eos, sizeof(PxU32), 1, gIslandGenLogFile);
	}

	//Connected edges
	{
		const PxU32 numJoinedEdges=mEdgeChangeManager.getNumJoinedEdges();
		const EdgeType* joinedEdges=mEdgeChangeManager.getJoinedEdges();
		fwrite(&numJoinedEdges, sizeof(EdgeType), 1, gIslandGenLogFile);
		for(PxU32 i=0;i<numJoinedEdges;i++)
		{
			fwrite(&joinedEdges[i], sizeof(EdgeType), 1, gIslandGenLogFile);
		}
		fwrite(&eos, sizeof(PxU32), 1, gIslandGenLogFile);
	}

	//Broken edges
	{
		const PxU32 numBrokenEdges=mEdgeChangeManager.getNumBrokenEdges();
		const EdgeType* brokenEdges=mEdgeChangeManager.getBrokenEdges();
		fwrite(&numBrokenEdges, sizeof(EdgeType), 1, gIslandGenLogFile);
		for(PxU32 i=0;i<numBrokenEdges;i++)
		{
			fwrite(&brokenEdges[i], sizeof(EdgeType), 1, gIslandGenLogFile);
		}
		fwrite(&eos, sizeof(PxU32), 1, gIslandGenLogFile);
	}

#endif //LOG_ISLANDGEN

	resizeArrays();

	cleanupEdgeEvents();

	//If everything was asleep at the last update and nothing has changed since the last update
	//then we can avoid doing all island work.
	if(mEverythingAsleep && !mHasAnythingChanged)
	{
		mPerformIslandUpdate = false;
		return;
	}

	mPerformIslandUpdate = true;

#ifdef PX_PS3
	mPPUFallback=true;
	if(numSpus>0 && true)
	{
		mPPUFallback=false;
		updateIslandsSPU(continuation, numSpus);
	}
	else
#endif
	{
		updateIslands();
		PX_ASSERT(isValid());
	}
}

void PxsIslandManager::updateIslandsSecondPass(Cm::BitMap& affectedIslandsBitmap)
{
	updateIslandsSecondPassMain(
		mRigidBodyOffset, affectedIslandsBitmap,
		mEdgeChangeManager.getBrokenEdges(),mEdgeChangeManager.getNumBrokenEdges(),
		mNodeManager,mEdgeManager,mIslands,mRootArticulationManager,
		mProcessSleepingIslandsComputeData,
		mIslandManagerUpdateWorkBuffers,
		mEventProfiler);

	PX_ASSERT(mProcessSleepingIslandsComputeData.mSolverBodiesSize<=mNumAddedRBodies);
	PX_ASSERT(mProcessSleepingIslandsComputeData.mSolverKinematicsSize<=mNumEdgesWithKinematicNodes);
	PX_ASSERT(mProcessSleepingIslandsComputeData.mSolverArticulationsSize<=mNumAddedArtics);

	PX_ASSERT(isValid());
	mEdgeChangeManager.reset();
}

void PxsIslandManager::updateIslandsSecondPass(PxBaseTask* continuation, const PxU32 numSpus)
{
	PX_UNUSED(continuation);
	PX_UNUSED(numSpus);

	//
	// On the second pass of island gen, there can be no new/deleted nodes or new/deleted edges.
	// It operates on a list of islands that could not be resolved during the first pass only.
	// Those islands might get broken up into subislands and put to sleep, no other state changes
	// should get applied.
	//

	PX_ASSERT(mNodeChangeManager.getNumCreatedNodes() == 0);
	PX_ASSERT(mNodeChangeManager.getNumDeletedNodes() == 0);
	PX_ASSERT(mEdgeChangeManager.getNumCreatedEdges() == 0);
	PX_ASSERT(mEdgeChangeManager.getNumDeletedEdges() == 0);

	mEdgeChangeManager.cleanupBrokenEdgeEvents(mEdgeManager.getAll());

	//Mark all second pass islands dirty (note: when splitting islands some might get added/removed later)
	Cm::BitMap& affectedIslandsBitmap=*mIslandManagerUpdateWorkBuffers.mIslandBitmap2;
	affectedIslandsBitmap.clearFast();
	PxsIslandIndices* PX_RESTRICT islandIndices=mProcessSleepingIslandsComputeData.mIslandIndices;
	const PxU32 islandIndicesCapacity=mProcessSleepingIslandsComputeData.mIslandIndicesCapacity;
	const PxU32 islandIndicesSecondPassIdx=islandIndicesCapacity - mProcessSleepingIslandsComputeData.mIslandIndicesSecondPassSize;
	for(PxU32 i = islandIndicesSecondPassIdx; i < islandIndicesCapacity; i++)
	{
		const IslandType islandId = islandIndices[i].islandId;
		affectedIslandsBitmap.set(islandId);
	}

#ifdef PX_PS3
	mPPUFallback=true;
	if(numSpus>0 && true)
	{
		mPPUFallback=false;
		updateIslandsSecondPassSPU(continuation, numSpus);
	}
	else
#endif
	{
		updateIslandsSecondPass(affectedIslandsBitmap);
	}
}
