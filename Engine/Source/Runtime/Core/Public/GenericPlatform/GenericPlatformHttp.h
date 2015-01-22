// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "HAL/Platform.h"

class FHttpManager;
class FString;
class IHttpRequest;

/**
 * Platform specific Http implementations
 */
class CORE_API FGenericPlatformHttp
{
public:

	/**
	 * Platform initialization step
	 */
	static void Init();

	/**
	 * Creates a platform-specific HTTP manager.
	 *
	 * @return nullptr if default implementation is to be used
	 */
	static FHttpManager* CreatePlatformHttpManager()
	{
		return nullptr;
	}

	/**
	 * Platform shutdown step
	 */
	static void Shutdown();

	/**
	 * Creates a new Http request instance for the current platform
	 *
	 * @return request object
	 */
	static IHttpRequest* ConstructRequest();

	/**
	 * Returns a percent-encoded version of the passed in string
	 *
	 * @param FString& The unencoded string to convert to percent-encoding
	 * @return The percent-encoded string
	 */
	static FString UrlEncode(const FString &);
};
