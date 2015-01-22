// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	LinuxNoEditorTargetPlatformModule.cpp: Implements the FLinuxNoEditorTargetPlatformModule class.
=============================================================================*/

#include "LinuxNoEditorTargetPlatformPrivatePCH.h"


/**
 * Holds the target platform singleton.
 */
static ITargetPlatform* Singleton = NULL;


/**
 * Module for the Linux target platform (without editor).
 */
class FLinuxNoEditorTargetPlatformModule
	: public ITargetPlatformModule
{
public:

	virtual ~FLinuxNoEditorTargetPlatformModule( )
	{
		Singleton = NULL;
	}

	virtual ITargetPlatform* GetTargetPlatform( )
	{
		if (Singleton == NULL)
		{
			Singleton = new TLinuxTargetPlatform<false, false, false>();
		}

		return Singleton;
	}
};


IMPLEMENT_MODULE(FLinuxNoEditorTargetPlatformModule, LinuxNoEditorTargetPlatform);