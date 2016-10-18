// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_LIBCURL
#if PLATFORM_WINDOWS
#include "AllowWindowsPlatformTypes.h"
#endif
	#include "curl/curl.h"
#if PLATFORM_WINDOWS
#include "HideWindowsPlatformTypes.h"
#endif
#include "HttpManager.h"

class FCurlHttpManager : public FHttpManager
{
public:
	static void InitCurl();
	static void ShutdownCurl();
	static CURLSH* GShareHandle;
	static CURLM * GMultiHandle;

	static struct FCurlRequestOptions
	{
		FCurlRequestOptions()
			:	bVerifyPeer(true)
			,	bUseHttpProxy(false)
			,	bDontReuseConnections(false)
			,	CertBundlePath(nullptr)
		{}

		/** Prints out the options to the log */
		void Log();

		/** Whether or not should verify peer certificate (disable to allow self-signed certs) */
		bool bVerifyPeer;

		/** Whether or not should use HTTP proxy */
		bool bUseHttpProxy;

		/** Forbid reuse connections (for debugging purposes, since normally it's faster to reuse) */
		bool bDontReuseConnections;

		/** Address of the HTTP proxy */
		FString HttpProxyAddress;

		/** A path to certificate bundle */
		const char * CertBundlePath;
	}
	CurlRequestOptions;

protected:
	//~ Begin HttpManager Interface
	virtual FHttpThread* CreateHttpThread() override;
	//~ End HttpManager Interface
};

#endif //WITH_LIBCURL
