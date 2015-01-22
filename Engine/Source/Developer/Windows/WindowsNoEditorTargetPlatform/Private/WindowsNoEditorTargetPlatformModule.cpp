// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "WindowsNoEditorTargetPlatformPrivatePCH.h"


/** Holds the target platform singleton. */
static ITargetPlatform* Singleton = nullptr;


/**
 * Module for the Windows target platform (without editor).
 */
class FWindowsNoEditorTargetPlatformModule
	: public ITargetPlatformModule
{
public:

	virtual ~FWindowsNoEditorTargetPlatformModule( )
	{
		Singleton = nullptr;
	}

	virtual ITargetPlatform* GetTargetPlatform( )
	{
		if (Singleton == nullptr)
		{
			Singleton = new TGenericWindowsTargetPlatform<false, false, false>();
		}

		return Singleton;
	}
};


IMPLEMENT_MODULE(FWindowsNoEditorTargetPlatformModule, WindowsNoEditorTargetPlatform);
