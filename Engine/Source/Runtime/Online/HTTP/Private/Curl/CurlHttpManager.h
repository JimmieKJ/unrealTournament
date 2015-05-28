// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

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
protected:

	/** multi handle that groups all the requests - not owned by this class */
	CURLM * MultiHandle;

	/** Cached value of running request to reduce number of read info calls*/
	int LastRunningRequests;

	/** Mapping of libcurl easy handles to HTTP requests */
	TMap<CURL*, TSharedRef<class IHttpRequest> > HandlesToRequests;

public:

	// Begin HttpManager interface
	virtual void AddRequest(const TSharedRef<class IHttpRequest>& Request) override;
	virtual void RemoveRequest(const TSharedRef<class IHttpRequest>& Request) override;
	virtual bool Tick(float DeltaSeconds) override;
	// End HttpManager interface

	FCurlHttpManager();

	static void InitCurl();
	static void ShutdownCurl();
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
};

#endif //WITH_LIBCURL
