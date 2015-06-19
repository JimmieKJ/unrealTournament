// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "ModuleManager.h"
#include "IMergeActorsTool.h"


/**
 * Merge Actors module interface
 */
class IMergeActorsModule : public IModuleInterface
{

public:

	/**
	 * Get reference to the Merge Actors module instance
	 */
	static inline IMergeActorsModule& Get()
	{
		return FModuleManager::LoadModuleChecked<IMergeActorsModule>("MergeActors");
	}

	/**
	 * Register an IMergeActorsTool with the module, passing ownership to it
	 */
	MERGEACTORS_API virtual bool RegisterMergeActorsTool(TUniquePtr<IMergeActorsTool> Tool) = 0;

	/**
	 * Unregister an IMergeActorsTool with the module
	 */
	MERGEACTORS_API virtual bool UnregisterMergeActorsTool(IMergeActorsTool* Tool) = 0;
};
