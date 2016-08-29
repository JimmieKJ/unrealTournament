// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	BuildPatchHTTP.h: Declares a simple class to wrap queuing HTTP requests
	in order to insure the HTTP module is only used from the main thread
=============================================================================*/

#ifndef __BuildPatchHTTP_h__
#define __BuildPatchHTTP_h__

#pragma once

struct FAnalyticsEventAttribute;

/**
 * Implements the BuildPatchHTTP
 */
class FBuildPatchHTTP : public TSharedFromThis<FBuildPatchHTTP>
{
private:

	/**
	 * A simple struct to hold details required to set off a HTTP request
	 */
	struct FHttpRequestInfo
	{
		// Holds the delegate that will get called when the request completes
		FHttpRequestCompleteDelegate OnCompleteDelegate;

		// Holds the delegate that will get called for the request progress
		FHttpRequestProgressDelegate OnProgressDelegate;

		// The url for the request
		FString UrlRequest;

		// An ID for this request
		int32 RequestID;
	};

	/**
	 * A simple struct to hold details required to record an analytics event
	 */
	struct FAnalyticsEventInfo
	{
		// The analytics event name
		FString EventName;

		// The list of attributes
		TArray< FAnalyticsEventAttribute > Attributes;
	};

public:

	/**
	 * A helper to queue up a HTTP request, which must be created on the main thread.
	 * @param UrlRequest				The url for the request
	 * @param OnCompletionDelegate		The delegate to be called on request complete
	 * @param OnProgressDelegate		The delegate to be called for request progress
	 * @return	An ID for this request
	 */
	static int32 QueueHttpRequest( const FString& UrlRequest, const FHttpRequestCompleteDelegate& OnCompleteDelegate, const FHttpRequestProgressDelegate& OnProgressDelegate );

	/**
	 * Cancel a HTTP request started with QueueHttpRequest.
	 * @param RequestID		The ID that was returned from QueueHttpRequest for the request.
	 */
	static void CancelHttpRequest( const int32& RequestID );

	/**
	 * Cancels all HTTP requests started with QueueHttpRequest.
	 */
	static void CancelAllHttpRequests();

	/**
	 * A helper to queue up an Analytics Event, which must be created on the main thread.
	 * @param EventName		The name of the event
	 * @param Attributes	The list of attributes
	 */
	static void QueueAnalyticsEvent( const FString& EventName, const TArray< FAnalyticsEventAttribute >& Attributes );

	/**
	 * Ensures that the wrapper is initialized
	 */
	static void Initialize();

	/**
	 * Clears any remaining queue and deletes the singleton. NB: Be weary of timing, if one of the static functions above is called after this
	 * then the singleton will be re-created
	 */
	static void OnShutdown();

private:

	// A queue of HTTP requests that need to be started
	TArray< FHttpRequestInfo > HttpRequestQueue;

	// A queue of requests that have been canceled
	TArray< int32 > CancelRequestQueue;

	// A map of HTTP requests that have been started
	TMap< int32, TWeakPtr< IHttpRequest > > HttpRequestMap;

	// A counter for giving out HttpRequest IDs
	int32 HttpRequestIDCount;

	// A critical section to protect access to HttpRequestQueue
	FCriticalSection HttpRequestQueueCS;

	// A queue of Analytics Events that need to be recorded
	TArray< FAnalyticsEventInfo > AnalyticsEventQueue;

	// A critical section to protect access to AnalyticsEventQueue
	FCriticalSection AnalyticsEventQueueCS;

private:

	/**
	 * A ticker used to perform actions on the main thread.
	 * @param Delta		The tick delta
	 * @return	true to continue ticking
	 */
	bool Tick( float Delta );

	/**
	 * Internal HTTP request complete function, used to clean out this request from the map and remove any pending cancels which
	 * could cause double complete delegates
	 * @param Request			The http request interface
	 * @param Response			The http response interface
	 * @param bSucceeded		Whether the request was successful
	 * @param HttpRequestInfo	The original request details
	 */
	void HttpRequestComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bSucceeded, FHttpRequestInfo HttpRequestInfo);

/* Here we have private static access for the singleton
*****************************************************************************/
private:
	static FBuildPatchHTTP& Get();
	static TSharedPtr<FBuildPatchHTTP> SingletonInstance;
	static FDelegateHandle TickDelegateHandle;
};

#endif // __BuildPatchHTTP_h__
