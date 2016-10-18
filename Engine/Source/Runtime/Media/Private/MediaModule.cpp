// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MediaPCH.h"
#include "IMediaModule.h"
#include "IMediaPlayerFactory.h"


/**
 * Implements the Media module.
 */
class FMediaModule
	: public IMediaModule
{
public:

	//~ IMediaModule interface

	virtual const TArray<IMediaPlayerFactory*>& GetPlayerFactories() const override
	{
		return MediaPlayerFactories;
	}

	virtual IMediaPlayerFactory* GetPlayerFactory(const FName& FactoryName) const override
	{
		for (IMediaPlayerFactory* Factory : MediaPlayerFactories)
		{
			if (Factory->GetName() == FactoryName)
			{
				return Factory;
			}
		}

		return nullptr;
	}

	virtual void RegisterPlayerFactory(IMediaPlayerFactory& Factory) override
	{
		MediaPlayerFactories.AddUnique(&Factory);
	}

	virtual void UnregisterPlayerFactory(IMediaPlayerFactory& Factory) override
	{
		MediaPlayerFactories.Remove(&Factory);
	}

public:

	//~ IModuleInterface interface

	virtual void StartupModule() override { }

	virtual void ShutdownModule() override
	{
		MediaPlayerFactories.Reset();
	}

	virtual bool SupportsDynamicReloading() override
	{
		return false;
	}

private:

	/** Holds the registered video player factories. */
	TArray<IMediaPlayerFactory*> MediaPlayerFactories;
};


IMPLEMENT_MODULE(FMediaModule, Media);
