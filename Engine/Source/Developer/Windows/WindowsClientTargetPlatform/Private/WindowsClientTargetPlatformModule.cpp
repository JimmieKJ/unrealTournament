// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "WindowsClientTargetPlatformPrivatePCH.h"


/** Holds the target platform singleton. */
static ITargetPlatform* Singleton = nullptr;


/**
 * Module for the Windows target platform as a server.
 */
class FWindowsClientTargetPlatformModule
	: public ITargetPlatformModule
{
public:

	virtual ~FWindowsClientTargetPlatformModule( )
	{
		Singleton = nullptr;
	}

	virtual ITargetPlatform* GetTargetPlatform( )
	{
		if (Singleton == nullptr)
		{
			Singleton = new TGenericWindowsTargetPlatform<false, false, true>();
		}

		return Singleton;
	}
};


IMPLEMENT_MODULE(FWindowsClientTargetPlatformModule, WindowsClientTargetPlatform);
