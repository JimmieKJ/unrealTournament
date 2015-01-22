// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GenericPlatform/GenericPlatformHttp.h"


/**
 * Platform specific HTTP implementations.
 */
class FWindowsPlatformHttp : public FGenericPlatformHttp
{
public:

	/** Platform initialization step. */
	static void Init();

	/**
	 * Creates a platform-specific HTTP manager.
	 *
	 * @return nullptr if default implementation is to be used.
	 */
	static FHttpManager* CreatePlatformHttpManager();

	/** Platform shutdown step. */
	static void Shutdown();

	/**
	 * Creates a new HTTP request instance for the current platform.
	 *
	 * @return The request object.
	 */
	static IHttpRequest* ConstructRequest();
};


typedef FWindowsPlatformHttp FPlatformHttp;
