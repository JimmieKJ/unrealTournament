// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ModuleInterface.h"
#include "ModuleManager.h"


// forward declarations
class IMediaPlayer;
class IMediaPlayerFactory;


/**
 * Interface for media modules.
 */
class IMediaModule
	: public IModuleInterface
{
public:

	/**
	 * Creates a media player for the specified media URL.
	 *
	 * @param Url The URL to the media file container to create the player for.
	 */
	virtual TSharedPtr<IMediaPlayer> CreatePlayer(const FString& Url) = 0;

	/**
	 * Gets the collection of supported media file types.
	 *
	 * @param OutFileTypes Will hold the supported file types.
	 */
	virtual int32 GetSupportedFileTypes(TMap<FString, FText>& OutFileTypes) = 0;

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
	virtual void UnregisterPlayerFactory( IMediaPlayerFactory& Factory ) = 0;

public:

	/**
	 * Gets an event delegate that is invoked after a media player factory has been added.
	 *
	 * @return The event delegate.
	 * @see FOnFactoryRemoved
	 */
	DECLARE_EVENT(IMediaModule, FOnFactoryAdded);
	virtual FOnFactoryAdded& OnFactoryAdded() = 0;

	/**
	 * Gets an event delegate that is invoked after a media player factory has been removed.
	 *
	 * @return The event delegate.
	 * @see FOnFactoryAdded
	 */
	DECLARE_EVENT(IMediaModule, FOnFactoryRemoved);
	virtual FOnFactoryRemoved& OnFactoryRemoved() = 0;

public:

	/**
	 * Gets a reference to the messaging module instance.
	 *
	 * @todo gmp: better implementation using dependency injection.
	 * @return A reference to the Media module.
	 */
	static IMediaModule& Get()
	{
#if PLATFORM_IOS
        static IMediaModule& MediaModule = FModuleManager::LoadModuleChecked<IMediaModule>("Media");
        return MediaModule;
#else
        return FModuleManager::LoadModuleChecked<IMediaModule>("Media");
#endif
	}

public:

	/** Virtual destructor. */
	virtual ~IMediaModule() { }
};
