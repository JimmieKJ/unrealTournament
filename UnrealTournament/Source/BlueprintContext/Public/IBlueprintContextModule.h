// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ModuleInterface.h"

class IBlueprintContextModule : public IModuleInterface
{
public:
	/**
	 * Called when a local player is added to the game
	 *
	 * @param NewPlayer a pointer to the new player
	 * @return void
	 */
	virtual void LocalPlayerAdded( class ULocalPlayer* NewPlayer ) = 0;

	/**
	 * Called when a local player is removed
	 *
	 * @param OldPlayer a pointer to the dying player
	 * @return void
	 */
	virtual void LocalPlayerRemoved( class ULocalPlayer* OldPlayer ) = 0;
};

DECLARE_LOG_CATEGORY_EXTERN( LogBlueprintContext, Log, All );