// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	BuildPatchHTTP.cpp: Implements a simple class to wrap queuing HTTP requests
	in order to insure the HTTP module is only used from the main thread.
=============================================================================*/

#include "BuildPatchServicesPrivatePCH.h"

/* FBuildPatchHTTP implementation
 *****************************************************************************/
bool FBuildPatchHTTP::Tick( float Delta )
{
	// Process the http request and cancel queues
	{
		FScopeLock ScopeLock( &HttpRequestQueueCS );
		if( FBuildPatchInstallError::HasFatalError() )
		{
			// Make sure all requests get canceled if we get fatal error
			TArray< int32 > RequestKeys;
			HttpRequestMap.GetKeys( RequestKeys );
			CancelRequestQueue.Append( RequestKeys );
			while( HttpRequestQueue.Num() > 0 )
			{
				FHttpRequestInfo HttpRequestInfo = HttpRequestQueue.Pop();
				HttpRequestInfo.OnCompleteDelegate.ExecuteIfBound( NULL, NULL, false );
			}
		}
		else
		{
			while( HttpRequestQueue.Num() > 0 )
			{
				FHttpRequestInfo HttpRequestInfo = HttpRequestQueue.Pop();
				TSharedRef< IHttpRequest > HttpRequest = FHttpModule::Get().CreateRequest();
				HttpRequest->OnProcessRequestComplete().BindSP(this, &FBuildPatchHTTP::HttpRequestComplete, HttpRequestInfo);
				HttpRequest->OnRequestProgress() = HttpRequestInfo.OnProgressDelegate;
				HttpRequest->SetURL( HttpRequestInfo.UrlRequest );
				HttpRequest->ProcessRequest();
				HttpRequestMap.Add( HttpRequestInfo.RequestID, HttpRequest );
			}
		}
		while( CancelRequestQueue.Num() > 0 )
		{
			const int32 CancelID = CancelRequestQueue.Pop();
			TWeakPtr< IHttpRequest > HttpRequest = HttpRequestMap.FindRef( CancelID );
			HttpRequestMap.Remove( CancelID );
			TSharedPtr< IHttpRequest > HttpRequestPinned = HttpRequest.Pin();
			if( HttpRequestPinned.IsValid() )
			{
				HttpRequestPinned->CancelRequest();
			}
		}
	}

	// Process the Analytics Event queue
	{
		FScopeLock ScopeLock( &AnalyticsEventQueueCS );
		while( AnalyticsEventQueue.Num() > 0 )
		{
			const FAnalyticsEventInfo& EventInfo = AnalyticsEventQueue[ AnalyticsEventQueue.Num() -1 ];
			FBuildPatchAnalytics::RecordEvent( EventInfo.EventName, EventInfo.Attributes );
			AnalyticsEventQueue.Pop();
		}
	}

	return true;
}

void FBuildPatchHTTP::HttpRequestComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bSucceeded, FHttpRequestInfo HttpRequestInfo)
{
	// Remove this request from our map
	HttpRequestMap.Remove(HttpRequestInfo.RequestID);
	HttpRequestInfo.OnCompleteDelegate.ExecuteIfBound(Request, Response, bSucceeded);
}

int32 FBuildPatchHTTP::QueueHttpRequest( const FString& UrlRequest, const FHttpRequestCompleteDelegate& OnCompleteDelegate, const FHttpRequestProgressDelegate& OnProgressDelegate )
{
	if( !FBuildPatchInstallError::HasFatalError() )
	{
		FBuildPatchHTTP& BuildPatchHTTP = Get();
		// Queue the request creation
		{
			FScopeLock ScopeLock( &BuildPatchHTTP.HttpRequestQueueCS );
			FHttpRequestInfo RequestInfo;
			RequestInfo.OnCompleteDelegate = OnCompleteDelegate;
			RequestInfo.OnProgressDelegate = OnProgressDelegate;
			RequestInfo.UrlRequest = UrlRequest;
			RequestInfo.RequestID = BuildPatchHTTP.HttpRequestIDCount++;
			BuildPatchHTTP.HttpRequestQueue.Add( RequestInfo );
			return RequestInfo.RequestID;
		}
	}
	else
	{
		OnCompleteDelegate.ExecuteIfBound( NULL, NULL, false );
		return INDEX_NONE;
	}
}

void FBuildPatchHTTP::CancelHttpRequest( const int32& RequestID )
{
	FBuildPatchHTTP& BuildPatchHTTP = Get();
	// Queue the request cancellation
	{
		FScopeLock ScopeLock( &BuildPatchHTTP.HttpRequestQueueCS );
		BuildPatchHTTP.CancelRequestQueue.Add( RequestID );
	}
}

void FBuildPatchHTTP::CancelAllHttpRequests()
{
	FBuildPatchHTTP& BuildPatchHTTP = Get();
	// Queue the request cancellation
	{
		FScopeLock ScopeLock( &BuildPatchHTTP.HttpRequestQueueCS );
		TArray< int32 > RequestKeys;
		BuildPatchHTTP.HttpRequestMap.GetKeys( RequestKeys );
		BuildPatchHTTP.CancelRequestQueue.Append( RequestKeys );
		BuildPatchHTTP.Tick(0);
	}
}

void FBuildPatchHTTP::QueueAnalyticsEvent( const FString& EventName, const TArray< FAnalyticsEventAttribute >& Attributes )
{
	FBuildPatchHTTP& BuildPatchHTTP = Get();
	// Queue the event recording
	{
		FScopeLock ScopeLock( &BuildPatchHTTP.AnalyticsEventQueueCS );
		FAnalyticsEventInfo EventInfo;
		EventInfo.EventName = EventName;
		EventInfo.Attributes = Attributes;
		BuildPatchHTTP.AnalyticsEventQueue.Add( EventInfo );
	}
}

/* Static initialization
 *****************************************************************************/
TSharedPtr<FBuildPatchHTTP> FBuildPatchHTTP::SingletonInstance = NULL;
FBuildPatchHTTP& FBuildPatchHTTP::Get()
{
	if (SingletonInstance.IsValid() == false)
	{
		checkf(IsInGameThread(), TEXT("FBuildPatchHTTP::Get() called for first time from secondary thread."));
		SingletonInstance = MakeShareable(new FBuildPatchHTTP());
		TickDelegateHandle = FTicker::GetCoreTicker().AddTicker( FTickerDelegate::CreateSP( SingletonInstance.Get(), &FBuildPatchHTTP::Tick ) );
	}
	return *SingletonInstance.Get();
}
void FBuildPatchHTTP::Initialize()
{
	FBuildPatchHTTP::Get();
}
void FBuildPatchHTTP::OnShutdown()
{
	checkf(IsInGameThread(), TEXT("FBuildPatchHTTP::OnShutdown() called from secondary thread."));
	if (SingletonInstance.IsValid())
	{
		SingletonInstance->Tick( 0.0f );
		FTicker::GetCoreTicker().RemoveTicker( TickDelegateHandle );
		SingletonInstance.Reset();
	}
}
FDelegateHandle FBuildPatchHTTP::TickDelegateHandle;
