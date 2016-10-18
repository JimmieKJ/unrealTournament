// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	LinuxClientTargetPlatformModule.cpp: Implements the FLinuxClientTargetPlatformModule class.
=============================================================================*/

#include "LinuxClientTargetPlatformPrivatePCH.h"


/**
 * Holds the target platform singleton.
 */
static ITargetPlatform* Singleton = NULL;


/**
 * Module for the Linux target platform (without editor).
 */
class FLinuxClientTargetPlatformModule
	: public ITargetPlatformModule
{
public:

	virtual ~FLinuxClientTargetPlatformModule( )
	{
		Singleton = NULL;
	}

	virtual ITargetPlatform* GetTargetPlatform( )
	{
		if (Singleton == NULL)
		{
 			Singleton = new TLinuxTargetPlatform<false, false, true>();
		}

		return Singleton;
	}
};

// 
IMPLEMENT_MODULE(FLinuxClientTargetPlatformModule, LinuxClientTargetPlatform);