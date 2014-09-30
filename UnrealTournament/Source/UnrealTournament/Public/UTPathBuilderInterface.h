// implement this interface to allow Actors to affect the path node graph UT builds on top of the navmesh
// simply implementing the interface makes it a seed point for building and you can optionally
// add functionality to generate special paths with unique properties or requirements (e.g. teleporters)
// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
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

	virtual void AddSpecialPaths(class UUTPathNode* MyNode, class AUTRecastNavMesh* NavData)
	{}
};
