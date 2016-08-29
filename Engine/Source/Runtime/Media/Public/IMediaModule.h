// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ModuleInterface.h"


class IMediaPlayer;
class IMediaPlayerFactory;


/**
 * Interface for the Media module.
 */
class IMediaModule
	: public IModuleInterface
{
public:

	/**
	 * Get the list of installed media player factories.
	 *
	 * @return Collection of media player factories.
	 * @see GetPlayerInfo
	 */
	virtual const TArray<IMediaPlayerFactory*>& GetPlayerFactories() const = 0;

	/**
	 * Get a media player factory by name.
	 *
	 * @param FactoryName The name of the factory.
	 * @return The factory, or nullptr if not found.
	 * @see GetPlayerFactories
	 */
	virtual IMediaPlayerFactory* GetPlayerFactory(const FName& FactoryName) const = 0;

	/**
	 * Registers a media player factory.
	 *
	 * @param Factory The media player factory to register.
	 * @see UnregisterPlayerFactory
	 */
	virtual void RegisterPlayerFactory(IMediaPlayerFactory& Factory) = 0;

	/**
	 * Unregisters a media player factory.
	 *
	 * @param Factory The media player factory to unregister.
	 * @see RegisterPlayerFactory
	 */
	virtual void UnregisterPlayerFactory(IMediaPlayerFactory& Factory) = 0;

public:

	/** Virtual destructor. */
	virtual ~IMediaModule() { }
};
