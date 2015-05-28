// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MediaPrivatePCH.h"
#include "IMediaModule.h"
#include "IMediaPlayerFactory.h"
#include "IMediaPlayer.h"
#include "ModuleManager.h"


/**
 * Implements the Media module.
 */
class FMediaModule
	: public IMediaModule
{
public:

	// IModuleInterface interface

	virtual void StartupModule( ) override { }

	virtual void ShutdownModule( ) override
	{
		MediaPlayerFactories.Reset();
	}

	virtual bool SupportsDynamicReloading( ) override
	{
		return false;
	}

public:

	// IMediaModuleInterface

	virtual TSharedPtr<IMediaPlayer> CreatePlayer( const FString& Url ) override
	{
		TSharedPtr<IMediaPlayer> Player;

		for (IMediaPlayerFactory* PlayerFactory : MediaPlayerFactories)
		{
			if (PlayerFactory->SupportsUrl(Url))
			{
				Player = PlayerFactory->CreatePlayer();

				if (Player.IsValid())
				{
					break;
				}
			}
		}

		return Player;
	}

	virtual int32 GetSupportedFileTypes( FMediaFileTypes& OutFileTypes ) override
	{
		OutFileTypes.Reset();

		for (IMediaPlayerFactory* Factory : MediaPlayerFactories)
		{
			OutFileTypes.Append(Factory->GetSupportedFileTypes());
		}
	
		return OutFileTypes.Num();
	}

	DECLARE_DERIVED_EVENT(FMediaModule, IMediaModule::FOnFactoryAdded, FOnFactoryAdded);
	virtual FOnFactoryAdded& OnFactoryAdded( ) override
	{
		return FactoryAddedEvent;
	}

	DECLARE_DERIVED_EVENT(FMediaModule, IMediaModule::FOnFactoryRemoved, FOnFactoryRemoved);
	virtual FOnFactoryRemoved& OnFactoryRemoved( ) override
	{
		return FactoryRemovedEvent;
	}

	virtual void RegisterPlayerFactory( IMediaPlayerFactory& Factory ) override
	{
		MediaPlayerFactories.AddUnique(&Factory);
		FactoryAddedEvent.Broadcast();
	}

	virtual void UnregisterPlayerFactory( IMediaPlayerFactory& Factory ) override
	{
		MediaPlayerFactories.Remove(&Factory);
		FactoryRemovedEvent.Broadcast();
	}

private:

	/** Holds an event delegate that is invoked after a video player factory has been added. */
	FOnFactoryAdded FactoryAddedEvent;

	/** Holds an event delegate that is invoked after a video player factory has been removed. */
	FOnFactoryRemoved FactoryRemovedEvent;

	/** Holds the registered video player factories. */
	TArray<IMediaPlayerFactory*> MediaPlayerFactories;
};


IMPLEMENT_MODULE(FMediaModule, Media);
