// implement this interface to allow Actors to affect the path node graph UT builds on top of the navmesh
// simply implementing the interface makes it a seed point for building and you can optionally
// add functionality to generate special paths with unique properties or requirements (e.g. teleporters)
// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTPathBuilderInterface.generated.h"

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UUTPathBuilderInterface : public UInterface
{
	GENERATED_UINTERFACE_BODY()
};

class UNREALTOURNAMENT_API IUTPathBuilderInterface
{
	GENERATED_IINTERFACE_BODY()

	/** return whether this Actor should be stored in its PathNode's list for efficient access during path searches
	 * return false for objects that only need to run path building code and not game time code
	 */
	virtual bool IsPOI() const
	{
		return true;
	}

	/** add special paths needed to interact with this Actor
	 * NOTE: if IsPOI() is false, MyNode will be NULL since the Actor won't be put in any node's POI list
	 */
	virtual void AddSpecialPaths(class UUTPathNode* MyNode, class AUTRecastNavMesh* NavData)
	{}

	/** if true, the following happens in path building:
	 * 1) the navmesh poly the POI is on gets its own UTPathNode that contains just that one poly
	 * 2) standard movement paths aren't generated that go FROM that node to other nodes (paths from others to this are still generated and the POI can create any special paths it wants)
	 * this is used primarily for objects that cause movement on touch, so the AI can't expect to go wherever it wants when it is touching this point
	 */
	virtual bool IsDestinationOnly() const
	{
		return false;
	}
};
