// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ModuleManager.h"
#include "HttpManager.h"

class IHttpRequest;

/**
 * Module for Http request implementations
 * Use FHttpFactory to create a new Http request
 */
class FHttpModule : 
	public IModuleInterface, public FSelfRegisteringExec
{

public:

	// FSelfRegisteringExec

	/**
	 * Handle exec commands starting with "HTTP"
	 *
	 * @param InWorld	the world context
	 * @param Cmd		the exec command being executed
	 * @param Ar		the archive to log results to
	 *
	 * @return true if the handler consumed the input, false to continue searching handlers
	 */
	virtual bool Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) override;

	/** 
	 * Exec command handlers
	 */
	bool HandleHTTPCommand( const TCHAR* Cmd, FOutputDevice& Ar );

	// FHttpModule

	/**
	 * Singleton-like access to this module's interface.  This is just for convenience!
	 * Beware of calling this during the shutdown phase, though.  Your module might have been unloaded already.
	 *
	 * @return Returns singleton instance, loading the module on demand if needed
	 */
	HTTP_API static FHttpModule& Get();

	/**
	 * Instantiates a new Http request for the current platform
	 *
	 * @return new Http request instance
	 */
	virtual TSharedRef<IHttpRequest> CreateRequest();

	/**
	 * Only meant to be used by Http request/response implementations
	 *
	 * @return Http request manager used by the module
	 */
	inline FHttpManager& GetHttpManager()
	{
		check(HttpManager != NULL);
		return *HttpManager;
	}

	/**
	 * @return timeout in seconds for the entire http request to complete
	 */
	inline float GetHttpTimeout() const
	{
		return HttpTimeout;
	}

	/**
	 * Sets timeout in seconds for the entire http request to complete
	 */
	inline void SetHttpTimeout(float TimeOutInSec)
	{
		HttpTimeout = TimeOutInSec;
	}

	/**
	 * @return timeout in seconds to establish the connection
	 */
	inline float GetHttpConnectionTimeout() const
	{
		return HttpConnectionTimeout;
	}

	/**
	 * @return timeout in seconds to receive a response on the connection 
	 */
	inline float GetHttpReceiveTimeout() const
	{
		return HttpReceiveTimeout;
	}

	/**
	 * @return timeout in seconds to send a request on the connection
	 */
	inline float GetHttpMaxConnectionsPerServer() const
	{
		return HttpMaxConnectionsPerServer;
	}

	/**
	 * @return max number of simultaneous connections to a specific server
	 */
	inline float GetHttpSendTimeout() const
	{
		return HttpSendTimeout;
	}

	/**
	 * @return max read buffer size for http requests
	 */
	inline int32 GetMaxReadBufferSize() const
	{
		return MaxReadBufferSize;
	}

	/**
	 * Sets timeout in seconds for the entire http request to complete
	 * @param SizeInBytes	The maximum number of bytes to use for the read buffer
	 */
	inline void SetMaxReadBufferSize(int32 SizeInBytes)
	{
		MaxReadBufferSize = SizeInBytes;
	}

	/**
	 * @return true if http requests are enabled
	 */
	inline bool IsHttpEnabled() const
	{
		return bEnableHttp;
	}

	/**
	 * toggle null http implementation
	 */
	inline void ToggleNullHttp(bool bEnabled)
	{
		bUseNullHttp = bEnabled;
	}

	/**
	 * @return true if null http is being used
	 */
	inline bool IsNullHttpEnabled() const
	{
		return bUseNullHttp;
	}

	/**
	 * @return min delay time for each http request
	 */
	inline float GetHttpDelayTime() const
	{
		return HttpDelayTime;
	}

	/**
	 * Set the min delay time for each http request
	 */
	inline void SetHttpDelayTime(float InHttpDelayTime)
	{
		HttpDelayTime = InHttpDelayTime;
	}

	/**
	 * @return Target tick rate of an http thread
	 */
	inline float GetHttpThreadTickRate() const
	{
		return HttpThreadTickRate;
	}

	/**
	 * Set the target tick rate of an http thread
	 */
	inline void SetHttpThreadTickRate(float InHttpThreadTickRate)
	{
		HttpThreadTickRate = InHttpThreadTickRate;
	}

private:

	// IModuleInterface

	/**
	 * Called when Http module is loaded
	 * Initialize platform specific parts of Http handling
	 */
	virtual void StartupModule() override;
	
	/**
	 * Called when Http module is unloaded
	 * Shutdown platform specific parts of Http handling
	 */
	virtual void ShutdownModule() override;


	/** Keeps track of Http requests while they are being processed */
	FHttpManager* HttpManager;
	/** timeout in seconds for the entire http request to complete. 0 is no timeout */
	float HttpTimeout;
	/** timeout in seconds to establish the connection. -1 for system defaults, 0 is no timeout */
	float HttpConnectionTimeout;
	/** timeout in seconds to receive a response on the connection. -1 for system defaults */
	float HttpReceiveTimeout;
	/** timeout in seconds to send a request on the connection. -1 for system defaults */
	float HttpSendTimeout;
	/** total time to delay the request */
	float HttpDelayTime;
	/** Target tick rate (Hz) of HTTP thread (if applicable to platform's HTTP manager) */
	float HttpThreadTickRate;
	/** Max number of simultaneous connections to a specific server */
	int32 HttpMaxConnectionsPerServer;
	/** Max buffer size for individual http reads */
	int32 MaxReadBufferSize;
	/** toggles http requests */
	bool bEnableHttp;
	/** toggles null (mock) http requests */
	bool bUseNullHttp;
	/** singleton for the module while loaded and available */
	static FHttpModule* Singleton;
};
