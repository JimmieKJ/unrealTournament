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


#include "ScConstraintProjectionManager.h"
#include "ScBodySim.h"
#include "ScConstraintSim.h"
#include "ScConstraintInteraction.h"

using namespace physx;

Sc::ConstraintProjectionManager::ConstraintProjectionManager() : 
	mNodePool(PX_DEBUG_EXP("projectionNodePool")),
	mPendingGroupUpdates(PX_DEBUG_EXP("pendingProjectionGroupUpdateArray"))
{
}


void Sc::ConstraintProjectionManager::addToPendingGroupUpdates(Sc::ConstraintSim& s)
{
	PX_ASSERT(!s.readFlag(ConstraintSim::ePENDING_GROUP_UPDATE));
	mPendingGroupUpdates.pushBack(&s);
	s.setFlag(ConstraintSim::ePENDING_GROUP_UPDATE);
}


void Sc::ConstraintProjectionManager::removeFromPendingGroupUpdates(Sc::ConstraintSim& s)
{
	PX_ASSERT(s.readFlag(ConstraintSim::ePENDING_GROUP_UPDATE));
	mPendingGroupUpdates.findAndReplaceWithLast(&s);  // not super fast but most likely not used often
	s.clearFlag(ConstraintSim::ePENDING_GROUP_UPDATE);
}


PX_INLINE Sc::ConstraintGroupNode* Sc::ConstraintProjectionManager::createGroupNode(BodySim& b)
{
	ConstraintGroupNode* n = mNodePool.construct(b);
	b.setConstraintGroup(n);
	return n;
}


//
// Implementation of UNION of 
// UNION-FIND algo.
// It also updates the group traversal
// linked list.
//
void Sc::ConstraintProjectionManager::groupUnion(ConstraintGroupNode& root0, ConstraintGroupNode& root1)
{
	// Should only get called for the roots
	PX_ASSERT(&root0 == root0.parent);
	PX_ASSERT(&root1 == root1.parent);

	if (&root0 != &root1)	//different groups?  If not, its already merged.
	{
		//UNION(this, other);	//union-find algo unites groups.
		ConstraintGroupNode* newRoot;
		ConstraintGroupNode* otherRoot;
		if (root0.rank > root1.rank)
		{
			//hisGroup appended to mygroup.
			newRoot = &root0;
			otherRoot = &root1;
		}
		else
		{
			//myGroup appended to hisGroup. 
			newRoot = &root1;
			otherRoot = &root0;
			//there is a chance that the two ranks were equal, in which case the tree depth just increased.
			root1.rank++;
		}

		PX_ASSERT(newRoot->parent == newRoot);
		otherRoot->parent = newRoot;
		
		//update traversal linked list:
		newRoot->tail->next = otherRoot;
		newRoot->tail = otherRoot->tail;
	}
}


//
// Add a body to a constraint projection group.
//
void Sc::ConstraintProjectionManager::addToGroup(BodySim& b, BodySim* other, ConstraintSim& c)
{
	// If both bodies of the constraint are defined, we want to fetch the reference to the group root
	// from body 0 by default (allows to avoid checking both)
	PX_ASSERT(&b == c.getBody(0) || (c.getBody(0) == NULL && &b == c.getBody(1)));
	PX_UNUSED(c);

	ConstraintGroupNode* myRoot;
	if (!b.getConstraintGroup())
		myRoot = createGroupNode(b);
	else
	{
		myRoot = &b.getConstraintGroup()->getRoot();
		if (myRoot->hasProjectionTreeRoot())
			myRoot->purgeProjectionTrees();  // If a new constraint gets added to a constraint group, projection trees need to be recreated
	}

	if (other)
	{
		ConstraintGroupNode* otherRoot;
		if (!other->getConstraintGroup())
			otherRoot = createGroupNode(*other);
		else
		{
			otherRoot = &other->getConstraintGroup()->getRoot();
			if (otherRoot->hasProjectionTreeRoot())
				otherRoot->purgeProjectionTrees();  // If a new constraint gets added to a constraint group, articulations need to be recreated
		}

		//merge the two groups, if disjoint.
		groupUnion(*myRoot, *otherRoot);
	}
}


//
// Add all constraints connected to the specified body to the pending update list but
// ignore the specified constraint. If specified, ignore non-projecting constraints 
// too.
//
void Sc::ConstraintProjectionManager::dumpConnectedConstraints(BodySim& b, ConstraintSim* c, bool projConstrOnly)
{
	Cm::Range<Interaction*const> interactions = b.getActorInteractions();
	for(; !interactions.empty(); interactions.popFront())
	{
		Interaction *const interaction = interactions.front();
		if (interaction->getType() == PX_INTERACTION_TYPE_CONSTRAINTSHADER)
		{
			ConstraintSim* ct = static_cast<ConstraintInteraction*>(interaction)->getConstraint();

			if ((ct != c) && (!projConstrOnly || ct->needsProjection()))
			{
				//push constraint down the pending update list:
				if (!ct->readFlag(ConstraintSim::ePENDING_GROUP_UPDATE))
					addToPendingGroupUpdates(*ct);
			}
		}
	}
}


void Sc::ConstraintProjectionManager::buildGroups()
{
	//
	// if there are new/dirty constraints, update groups
	//
	if(mPendingGroupUpdates.size())
	{
#ifdef PX_DEBUG
		// At the beginning the list should only contain constraints with projection.
		// Further below other constraints, connected to the constraints with projection, will be added too.
		for(PxU32 i=0; i < mPendingGroupUpdates.size(); i++)
		{
			PX_ASSERT(mPendingGroupUpdates[i]->needsProjection());
		}
#endif
		PxU32 nbConstrWithProjection = mPendingGroupUpdates.size();
		for(PxU32 i=0; i < mPendingGroupUpdates.size(); i++)
		{
			ConstraintSim* c = mPendingGroupUpdates[i];
			c->clearFlag(ConstraintSim::ePENDING_GROUP_UPDATE);

			// Find all constraints connected to the two bodies of the dirty constraint.
			// - Constraints to static anchors are ignored (note: kinematics can't be ignored because they might get switched to dynamics any time which
			//   does trigger a projection tree rebuild but not a constraint tree rebuild
			// - Already processed bodies are ignored as well
			BodySim* b0 = c->getBody(0);
			if (b0 && !b0->getConstraintGroup())
			{
				dumpConnectedConstraints(*b0, c, false);
			}
			BodySim* b1 = c->getBody(1);
			if (b1 && !b1->getConstraintGroup())
			{
				dumpConnectedConstraints(*b1, c, false);
			}

			BodySim* b = c->getAnyBody();
			PX_ASSERT(b);

			addToGroup(*b, c->getOtherBody(b), *c);  //this will eventually merge some body's constraint groups.
		}

		// Now find all the newly made groups and build projection trees.
		// Don't need to iterate over the additionally constraints since the roots are supposed to be
		// fetchable from any node.
		for (PxU32 i=0; i < nbConstrWithProjection; i++)
		{
			ConstraintSim* c = mPendingGroupUpdates[i];
			BodySim* b = c->getAnyBody();
			PX_ASSERT(b);
			PX_ASSERT(b->getConstraintGroup());

			ConstraintGroupNode& root = b->getConstraintGroup()->getRoot();
			if (!root.hasProjectionTreeRoot())  // Build projection tree only once
				root.buildProjectionTrees();
		}

		mPendingGroupUpdates.clear();
	}
}


//
// Called if a body or a constraint gets deleted. All projecting constraints of the
// group (except the deleted one) are moved to the dirty list and all group nodes are destroyed.
//
void Sc::ConstraintProjectionManager::invalidateGroup(ConstraintGroupNode& node, ConstraintSim* deletedConstraint)
{
	ConstraintGroupNode* n = &node.getRoot();
	while (n) //go through nodes in constraint group
	{
		dumpConnectedConstraints(*n->body, deletedConstraint, true);

		//destroy the body's constraint group information

		ConstraintGroupNode* next = n->next;	//save next node ptr before we destroy it!

		BodySim* b = n->body;
		b->setConstraintGroup(NULL);
		if (n->hasProjectionTreeRoot())
			n->purgeProjectionTrees();
		mNodePool.destroy(n);

		n = next;
	}
}
