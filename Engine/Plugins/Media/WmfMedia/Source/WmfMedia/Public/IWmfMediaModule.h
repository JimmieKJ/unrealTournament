// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ModuleInterface.h"


class IMediaPlayer;


/**
 * Interface for the WmfMedia module.
 */
class IWmfMediaModule
	: public IModuleInterface
{
public:

	/**
	 * Creates a Windows Media Foundation based media player.
	 *
	 * @return A new media player, or nullptr if a player couldn't be created.
	 */
	virtual TSharedPtr<IMediaPlayer> CreatePlayer() = 0;

public:

	/** Virtual destructor. */
	virtual ~IWmfMediaModule() { }
};
