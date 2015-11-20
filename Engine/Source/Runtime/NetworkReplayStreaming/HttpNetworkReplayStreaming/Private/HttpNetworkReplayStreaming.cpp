// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "HttpNetworkReplayStreaming.h"

DEFINE_LOG_CATEGORY_STATIC( LogHttpReplay, Log, All );

static TAutoConsoleVariable<FString> CVarMetaFilterOverride( TEXT( "httpReplay.MetaFilterOverride" ), TEXT( "" ), TEXT( "" ) );

class FNetworkReplayListItem : public FOnlineJsonSerializable
{
public:
	FNetworkReplayListItem() : SizeInBytes( 0 ), DemoTimeInMs( 0 ), NumViewers( 0 ), bIsLive( false ), Changelist( 0 ), bShouldKeep( false ) {}
	virtual ~FNetworkReplayListItem() {}

	FString		AppName;
	FString		SessionName;
	FString		FriendlyName;
	FDateTime	Timestamp;
	int32		SizeInBytes;
	int32		DemoTimeInMs;
	int32		NumViewers;
	bool		bIsLive;
	int32		Changelist;
	bool		bShouldKeep;

	// FOnlineJsonSerializable
	BEGIN_ONLINE_JSON_SERIALIZER
		ONLINE_JSON_SERIALIZE( "AppName",		AppName );
		ONLINE_JSON_SERIALIZE( "SessionName",	SessionName );
		ONLINE_JSON_SERIALIZE( "FriendlyName",	FriendlyName );
		ONLINE_JSON_SERIALIZE( "Timestamp",		Timestamp );
		ONLINE_JSON_SERIALIZE( "SizeInBytes",	SizeInBytes );
		ONLINE_JSON_SERIALIZE( "DemoTimeInMs",	DemoTimeInMs );
		ONLINE_JSON_SERIALIZE( "NumViewers",	NumViewers );
		ONLINE_JSON_SERIALIZE( "bIsLive",		bIsLive );
		ONLINE_JSON_SERIALIZE( "Changelist",	Changelist );
		ONLINE_JSON_SERIALIZE( "shouldKeep",	bShouldKeep );
	END_ONLINE_JSON_SERIALIZER
};

class FNetworkReplayList : public FOnlineJsonSerializable
{
public:
	FNetworkReplayList()
	{}
	virtual ~FNetworkReplayList() {}

	TArray< FNetworkReplayListItem > Replays;

	// FOnlineJsonSerializable
	BEGIN_ONLINE_JSON_SERIALIZER
		ONLINE_JSON_SERIALIZE_ARRAY_SERIALIZABLE( "replays", Replays, FNetworkReplayListItem );
	END_ONLINE_JSON_SERIALIZER
};

class FNetworkReplayUserList : public FOnlineJsonSerializable
{
public:
	FNetworkReplayUserList()
	{
	}
	virtual ~FNetworkReplayUserList()
	{
	}

	TArray< FString > Users;

	// FOnlineJsonSerializable
	BEGIN_ONLINE_JSON_SERIALIZER
		ONLINE_JSON_SERIALIZE_ARRAY( "users", Users );
	END_ONLINE_JSON_SERIALIZER
};

class FNetworkReplayStartUploadingResponse : public FOnlineJsonSerializable
{
public:
	FNetworkReplayStartUploadingResponse()
	{}
	virtual ~FNetworkReplayStartUploadingResponse() {}

	FString SessionId;

	// FOnlineJsonSerializable
	BEGIN_ONLINE_JSON_SERIALIZER
		ONLINE_JSON_SERIALIZE("sessionId", SessionId);
	END_ONLINE_JSON_SERIALIZER
};

class FNetworkReplayStartDownloadingResponse : public FOnlineJsonSerializable
{
public:
	FNetworkReplayStartDownloadingResponse()
	{}
	virtual ~FNetworkReplayStartDownloadingResponse() {}

	FString State;
	FString Viewer;
	int32 Time;
	int32 NumChunks;

	// FOnlineJsonSerializable
	BEGIN_ONLINE_JSON_SERIALIZER
		ONLINE_JSON_SERIALIZE("state", State);
		ONLINE_JSON_SERIALIZE("numChunks", NumChunks);
		ONLINE_JSON_SERIALIZE("time", Time);
		ONLINE_JSON_SERIALIZE("viewerId", Viewer);
	END_ONLINE_JSON_SERIALIZER
};

void FHttpStreamFArchive::Serialize( void* V, int64 Length ) 
{
	if ( IsLoading() )
	{
		if ( Pos + Length > Buffer.Num() )
		{
			ArIsError = true;
			return;
		}
			
		FMemory::Memcpy( V, Buffer.GetData() + Pos, Length );

		Pos += Length;
	}
	else
	{
		check( Pos <= Buffer.Num() );

		const int32 SpaceNeeded = Length - ( Buffer.Num() - Pos );

		if ( SpaceNeeded > 0 )
		{
			Buffer.AddZeroed( SpaceNeeded );
		}

		FMemory::Memcpy( Buffer.GetData() + Pos, V, Length );

		Pos += Length;
	}
}

int64 FHttpStreamFArchive::Tell() 
{
	return Pos;
}

int64 FHttpStreamFArchive::TotalSize()
{
	return Buffer.Num();
}

void FHttpStreamFArchive::Seek( int64 InPos ) 
{
	check( InPos < Buffer.Num() );

	Pos = InPos;
}

bool FHttpStreamFArchive::AtEnd() 
{
	return Pos >= Buffer.Num() && bAtEndOfReplay;
}

FHttpNetworkReplayStreamer::FHttpNetworkReplayStreamer() : 
	StreamChunkIndex( 0 ), 
	LastChunkTime( 0 ), 
	LastRefreshViewerTime( 0 ),
	LastRefreshCheckpointTime( 0 ),
	StreamerState( EStreamerState::Idle ), 
	bStopStreamingCalled( false ), 
	bStreamIsLive( false ),
	NumTotalStreamChunks( 0 ),
	TotalDemoTimeInMS( 0 ),
	StreamTimeRangeStart( 0 ),
	StreamTimeRangeEnd( 0 ),
	HighPriorityEndTime( 0 ),
	StreamerLastError( ENetworkReplayError::None ),
	DownloadCheckpointIndex( -1 ),
	LastGotoTimeInMS( -1 )
{
	// Initialize the server URL
	GConfig->GetString( TEXT( "HttpNetworkReplayStreaming" ), TEXT( "ServerURL" ), ServerURL, GEngineIni );
}

void FHttpNetworkReplayStreamer::StartStreaming( const FString& CustomName, const FString& FriendlyName, const TArray< FString >& UserNames, bool bRecord, const FNetworkReplayVersion& InReplayVersion, const FOnStreamReadyDelegate& Delegate )
{
	if ( !SessionName.IsEmpty() )
	{
		UE_LOG( LogHttpReplay, Warning, TEXT( "FHttpNetworkReplayStreamer::StartStreaming. SessionName already set." ) );
		return;
	}

	if ( IsStreaming() )
	{
		UE_LOG( LogHttpReplay, Warning, TEXT( "FHttpNetworkReplayStreamer::StartStreaming. IsStreaming == true." ) );
		return;
	}

	if ( IsHttpRequestInFlight() )
	{
		UE_LOG( LogHttpReplay, Warning, TEXT( "FHttpNetworkReplayStreamer::StartStreaming. IsHttpRequestInFlight == true." ) );
		return;
	}

	ReplayVersion = InReplayVersion;

	// Remember the delegate, which we'll call as soon as the header is available
	StartStreamingDelegate = Delegate;

	// Setup the archives
	StreamArchive.ArIsLoading		= !bRecord;
	StreamArchive.ArIsSaving		= !StreamArchive.ArIsLoading;
	StreamArchive.bAtEndOfReplay	= false;

	HeaderArchive.ArIsLoading		= StreamArchive.ArIsLoading;
	HeaderArchive.ArIsSaving		= StreamArchive.ArIsSaving;

	CheckpointArchive.ArIsLoading	= StreamArchive.ArIsLoading;
	CheckpointArchive.ArIsSaving	= StreamArchive.ArIsSaving;

	LastChunkTime = FPlatformTime::Seconds();

	TotalDemoTimeInMS = 0;

	StreamTimeRangeStart	= 0;
	StreamTimeRangeEnd		= 0;

	// Create the Http request and add to pending request list
	TSharedRef<class IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();

	StreamChunkIndex = 0;

	if ( !bRecord )
	{
		// We are streaming down
		StreamerState = EStreamerState::StreamingDown;

		SessionName = CustomName;

		FString UserName;

		if ( UserNames.Num() == 1 )
		{
			UserName = UserNames[0];
		}

		// Notify the http server that we want to start downloading a replay
		const FString URL = FString::Printf( TEXT( "%sreplay/%s/startDownloading?user=%s" ), *ServerURL, *SessionName, *UserName );

		UE_LOG( LogHttpReplay, Log, TEXT( "FHttpNetworkReplayStreamer::StartStreaming. URL: %s" ), *URL );

		HttpRequest->SetURL( URL );
		HttpRequest->SetVerb( TEXT( "POST" ) );

		HttpRequest->OnProcessRequestComplete().BindRaw( this, &FHttpNetworkReplayStreamer::HttpStartDownloadingFinished );
	
		// Add the request to start downloading
		AddRequestToQueue( EQueuedHttpRequestType::StartDownloading, HttpRequest );

		// Download the header (will add to queue)
		DownloadHeader();

		// Download the first set of checkpoints (will add to queue)
		EnumerateCheckpoints();
	}
	else
	{
		// We are streaming up
		StreamerState = EStreamerState::StreamingUp;

		SessionName.Empty();

		FString MetaString;

		if ( FParse::Value( FCommandLine::Get(), TEXT( "ReplayMeta=" ), MetaString ) && !MetaString.IsEmpty() )
		{
			// Notify the http server that we want to start uploading a replay
			HttpRequest->SetURL( FString::Printf( TEXT( "%sreplay?app=%s&version=%u&cl=%u&friendlyName=%s&meta=%s" ), *ServerURL, *ReplayVersion.AppString, ReplayVersion.NetworkVersion, ReplayVersion.Changelist, *FGenericPlatformHttp::UrlEncode( FriendlyName ), *FGenericPlatformHttp::UrlEncode( MetaString ) ) );
		}
		else
		{
			// Notify the http server that we want to start uploading a replay
			HttpRequest->SetURL( FString::Printf( TEXT( "%sreplay?app=%s&version=%u&cl=%u&friendlyName=%s" ), *ServerURL, *ReplayVersion.AppString, ReplayVersion.NetworkVersion, ReplayVersion.Changelist, *FGenericPlatformHttp::UrlEncode( FriendlyName ) ) );
		}

		HttpRequest->SetVerb( TEXT( "POST" ) );

		HttpRequest->OnProcessRequestComplete().BindRaw( this, &FHttpNetworkReplayStreamer::HttpStartUploadingFinished );

		if ( UserNames.Num() > 0 )
		{
			FNetworkReplayUserList UserList;

			UserList.Users = UserNames;
			HttpRequest->SetContentAsString( UserList.ToJson() );
			HttpRequest->SetHeader( TEXT( "Content-Type" ), TEXT( "application/json" ) );
		}

		AddRequestToQueue( EQueuedHttpRequestType::StartUploading, HttpRequest );
		
		// We need to upload the header AFTER StartUploading is done (so we have session name)
		AddRequestToQueue( EQueuedHttpRequestType::UploadHeader, nullptr );
	}
}

void FHttpNetworkReplayStreamer::AddRequestToQueue( const EQueuedHttpRequestType::Type Type, TSharedPtr< class IHttpRequest > Request )
{
	UE_LOG( LogHttpReplay, Verbose, TEXT( "FHttpNetworkReplayStreamer::AddRequestToQueue. Type: %s" ), EQueuedHttpRequestType::ToString( Type ) );

	QueuedHttpRequests.Enqueue( TSharedPtr< FQueuedHttpRequest >( new FQueuedHttpRequest( Type, Request ) ) );
}

void FHttpNetworkReplayStreamer::AddCustomRequestToQueue( TSharedPtr< FQueuedHttpRequest > Request )
{
	UE_LOG( LogHttpReplay, Verbose, TEXT( "FHttpNetworkReplayStreamer::AddCustomRequestToQueue. Type: %s" ), EQueuedHttpRequestType::ToString( Request->Type ) );

	QueuedHttpRequests.Enqueue( Request );
}

void FHttpNetworkReplayStreamer::StopStreaming()
{
	if ( StartStreamingDelegate.IsBound() )
	{
		UE_LOG( LogHttpReplay, Warning, TEXT( "FHttpNetworkReplayStreamer::StopStreaming. Called while existing StartStreaming request wasn't finished" ) );
		CancelStreamingRequests();
		check( !IsStreaming() );
		return;
	}

	if ( !IsStreaming() )
	{
		UE_LOG( LogHttpReplay, Warning, TEXT( "FHttpNetworkReplayStreamer::StopStreaming. Not currently streaming." ) );
		check( bStopStreamingCalled == false );
		return;
	}

	if ( bStopStreamingCalled )
	{
		UE_LOG( LogHttpReplay, Warning, TEXT( "FHttpNetworkReplayStreamer::StopStreaming. Already called" ) );
		return;
	}

	bStopStreamingCalled = true;

	if ( StreamerState == EStreamerState::StreamingDown )
	{
		// Refresh one last time, pasing in bFinal == true, this will notify the server we're done (remove us from viewers list)
		RefreshViewer( true );
	}
	else if ( StreamerState == EStreamerState::StreamingUp )
	{
		// Flush any final pending stream
		FlushStream();
		// Send one last http request to stop uploading		
		StopUploading();
	}

	// Finally, add the stop streaming request, which should put things in the right state after the above requests are done
	AddRequestToQueue( EQueuedHttpRequestType::StopStreaming, nullptr );
}

void FHttpNetworkReplayStreamer::UploadHeader()
{
	check( StreamArchive.ArIsSaving );

	if ( SessionName.IsEmpty() )
	{
		// IF there is no active session, or we are not recording, we don't need to flush
		UE_LOG( LogHttpReplay, Warning, TEXT( "FHttpNetworkReplayStreamer::UploadHeader. No session name!" ) );
		return;
	}

	if ( HeaderArchive.Buffer.Num() == 0 )
	{
		// Header wasn't serialized
		UE_LOG( LogHttpReplay, Warning, TEXT( "FHttpNetworkReplayStreamer::UploadHeader. No header to upload" ) );
		return;
	}

	check( StreamChunkIndex == 0 );

	// First upload the header
	UE_LOG( LogHttpReplay, Log, TEXT( "FHttpNetworkReplayStreamer::UploadHeader. Header. StreamChunkIndex: %i, Size: %i" ), StreamChunkIndex, StreamArchive.Buffer.Num() );

	// Create the Http request and add to pending request list
	TSharedRef<class IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();

	HttpRequest->OnProcessRequestComplete().BindRaw( this, &FHttpNetworkReplayStreamer::HttpHeaderUploadFinished );

	HttpRequest->SetURL( FString::Printf( TEXT( "%sreplay/%s/file/replay.header?numChunks=%i&time=%i" ), *ServerURL, *SessionName, StreamChunkIndex, TotalDemoTimeInMS ) );
	HttpRequest->SetVerb( TEXT( "POST" ) );
	HttpRequest->SetHeader( TEXT( "Content-Type" ), TEXT( "application/octet-stream" ) );
	HttpRequest->SetContent( HeaderArchive.Buffer );

	// We're done with the header archive
	HeaderArchive.Buffer.Empty();
	HeaderArchive.Pos = 0;

	AddRequestToQueue( EQueuedHttpRequestType::UploadingHeader, HttpRequest );

	LastChunkTime = FPlatformTime::Seconds();
}

void FHttpNetworkReplayStreamer::FlushStream()
{
	check( StreamArchive.ArIsSaving );

	if ( SessionName.IsEmpty() )
	{
		// If we haven't uploaded the header, or we are not recording, we don't need to flush
		UE_LOG( LogHttpReplay, Warning, TEXT( "FHttpNetworkReplayStreamer::FlushStream. Waiting on header upload." ) );
		return;
	}

	if ( StreamArchive.Buffer.Num() == 0 )
	{
		// Nothing to flush
		return;
	}

	// Upload any new streamed data to the http server
	UE_LOG( LogHttpReplay, Verbose, TEXT( "FHttpNetworkReplayStreamer::FlushStream. StreamChunkIndex: %i, Size: %i" ), StreamChunkIndex, StreamArchive.Buffer.Num() );

	// Create the Http request and add to pending request list
	TSharedRef<class IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();

	HttpRequest->OnProcessRequestComplete().BindRaw( this, &FHttpNetworkReplayStreamer::HttpUploadStreamFinished );

	HttpRequest->SetURL(FString::Printf(TEXT("%sreplay/%s/file/stream.%i?numChunks=%i&time=%i&mTime1=%i&mTime2=%i"), *ServerURL, *SessionName, StreamChunkIndex, StreamChunkIndex + 1, TotalDemoTimeInMS, StreamTimeRangeStart, StreamTimeRangeEnd));
	HttpRequest->SetVerb( TEXT( "POST" ) );
	HttpRequest->SetHeader( TEXT( "Content-Type" ), TEXT( "application/octet-stream" ) );
	HttpRequest->SetContent( StreamArchive.Buffer );

	StreamArchive.Buffer.Empty();
	StreamArchive.Pos = 0;

	// Keep track of the time range we have in our buffer, so we can accurately upload that each time we submit a chunk
	StreamTimeRangeStart = StreamTimeRangeEnd;

	StreamChunkIndex++;

	AddRequestToQueue( EQueuedHttpRequestType::UploadingStream, HttpRequest );

	LastChunkTime = FPlatformTime::Seconds();
}

void FHttpNetworkReplayStreamer::ConditionallyFlushStream()
{
	const double FLUSH_TIME_IN_SECONDS = 10;

	if ( FPlatformTime::Seconds() - LastChunkTime > FLUSH_TIME_IN_SECONDS )
	{
		FlushStream();
	}
};

void FHttpNetworkReplayStreamer::StopUploading()
{
	// Create the Http request and add to pending request list
	TSharedRef< IHttpRequest > HttpRequest = FHttpModule::Get().CreateRequest();

	HttpRequest->OnProcessRequestComplete().BindRaw( this, &FHttpNetworkReplayStreamer::HttpStopUploadingFinished );

	HttpRequest->SetURL( FString::Printf( TEXT( "%sreplay/%s/stopUploading?numChunks=%i&time=%i" ), *ServerURL, *SessionName, StreamChunkIndex, TotalDemoTimeInMS ) );
	HttpRequest->SetVerb( TEXT( "POST" ) );
	HttpRequest->SetHeader( TEXT( "Content-Type" ), TEXT( "application/octet-stream" ) );

	AddRequestToQueue( EQueuedHttpRequestType::StopUploading, HttpRequest );
};

void FHttpNetworkReplayStreamer::FlushCheckpoint( const uint32 TimeInMS )
{
	if ( CheckpointArchive.Buffer.Num() == 0 )
	{
		UE_LOG( LogHttpReplay, Warning, TEXT( "FHttpNetworkReplayStreamer::FlushCheckpoint. Checkpoint is empty." ) );
		return;
	}
	
	// Flush any existing stream, we need checkpoints to line up with the next chunk
	FlushStream();

	// Flush the checkpoint
	FlushCheckpointInternal( TimeInMS );
}

void FHttpNetworkReplayStreamer::GotoCheckpointIndex( const int32 CheckpointIndex, const FOnCheckpointReadyDelegate& Delegate )
{
	if ( GotoCheckpointDelegate.IsBound() || DownloadCheckpointIndex != -1 )
	{
		// If we're currently going to a checkpoint now, ignore this request
		UE_LOG( LogHttpReplay, Warning, TEXT( "FHttpNetworkReplayStreamer::GotoCheckpointIndex. Busy processing another checkpoint." ) );
		Delegate.ExecuteIfBound( false, -1 );
		return;
	}

	check( DownloadCheckpointIndex == -1 );

	if ( CheckpointIndex == -1 )
	{
		// Make sure to reset the checkpoint archive (this is how we signify that the engine should start from the beginning of the steam (we don't need a checkpoint for that))
		CheckpointArchive.Buffer.Empty();
		CheckpointArchive.Pos = 0;

		// Completely reset our stream (we're going to start downloading from the start of the checkpoint)
		StreamArchive.Buffer.Empty();
		StreamArchive.Pos				= 0;
		StreamArchive.bAtEndOfReplay	= false;

		// Reset our stream range
		StreamTimeRangeStart	= 0;
		StreamTimeRangeEnd		= 0;

		StreamChunkIndex		= 0;

		Delegate.ExecuteIfBound( true, LastGotoTimeInMS );
		LastGotoTimeInMS = -1;
		return;
	}

	if ( !CheckpointList.ReplayEvents.IsValidIndex( CheckpointIndex ) )
	{
		UE_LOG( LogHttpReplay, Warning, TEXT( "FHttpNetworkReplayStreamer::GotoCheckpointIndex. Invalid checkpoint index." ) );
		Delegate.ExecuteIfBound( false, -1 );
		return;
	}

	TSharedRef<class IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();

	// Download the next stream chunk
	HttpRequest->SetURL( FString::Printf( TEXT( "%sevent/%s" ), *ServerURL,  *CheckpointList.ReplayEvents[CheckpointIndex].ID ) );
	HttpRequest->SetVerb( TEXT( "GET" ) );

	HttpRequest->OnProcessRequestComplete().BindRaw( this, &FHttpNetworkReplayStreamer::HttpDownloadCheckpointFinished );

	GotoCheckpointDelegate	= Delegate;
	DownloadCheckpointIndex = CheckpointIndex;

	AddRequestToQueue( EQueuedHttpRequestType::DownloadingCheckpoint, HttpRequest );
}

void FHttpNetworkReplayStreamer::SearchEvents(const FString& EventGroup, const FOnEnumerateStreamsComplete& Delegate)
{
	TSharedRef<class IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();

	HttpRequest->SetURL(FString::Printf(TEXT("%sevent?group=%s"), *ServerURL, *EventGroup));
	HttpRequest->SetVerb(TEXT("GET"));

	HttpRequest->OnProcessRequestComplete().BindRaw(this, &FHttpNetworkReplayStreamer::HttpEnumerateSessionsFinished, Delegate);

	AddRequestToQueue(EQueuedHttpRequestType::EnumeratingSessions, HttpRequest);
}

void FHttpNetworkReplayStreamer::RequestEventData(const FString& EventID, const FOnRequestEventDataComplete& Delegate)
{
	TSharedRef<class IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();

	// Download the next stream chunk
	HttpRequest->SetURL(FString::Printf(TEXT("%sevent/%s"), *ServerURL, *EventID));
	HttpRequest->SetVerb(TEXT("GET"));

	HttpRequest->OnProcessRequestComplete().BindRaw(this, &FHttpNetworkReplayStreamer::HttpRequestEventDataFinished, Delegate);
	
	AddRequestToQueue(EQueuedHttpRequestType::RequestEventData, HttpRequest);
}

void FHttpNetworkReplayStreamer::HttpRequestEventDataFinished(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded, FOnRequestEventDataComplete RequestEventDataCompleteDelegate)
{
	RequestFinished(StreamerState, EQueuedHttpRequestType::RequestEventData, HttpRequest);

	check(RequestEventDataCompleteDelegate.IsBound());

	if (bSucceeded && HttpResponse->GetResponseCode() == EHttpResponseCodes::Ok)
	{
		RequestEventDataCompleteDelegate.ExecuteIfBound(HttpResponse->GetContent(), true);
		UE_LOG(LogHttpReplay, Verbose, TEXT("FHttpNetworkReplayStreamer::HttpRequestEventDataFinished."));
	}
	else
	{
		// FAIL
		UE_LOG(LogHttpReplay, Error, TEXT("FHttpNetworkReplayStreamer::HttpRequestEventDataFinished. FAILED, Response code: %d"), HttpResponse.IsValid() ? HttpResponse->GetResponseCode() : 0);

		RequestEventDataCompleteDelegate.ExecuteIfBound(TArray<uint8>(), false);
	}
}

void FHttpNetworkReplayStreamer::GotoTimeInMS( const uint32 TimeInMS, const FOnCheckpointReadyDelegate& Delegate )
{
	if ( LastGotoTimeInMS != -1 || DownloadCheckpointIndex != -1 )
	{
		// If we're processing requests, be on the safe side and cancel the scrub
		// FIXME: We can cancel the in-flight requests as well
		UE_LOG( LogHttpReplay, Log, TEXT( "FHttpNetworkReplayStreamer::GotoTimeInMS. Busy processing pending requests." ) );
		Delegate.ExecuteIfBound( false, -1 );
		return;
	}

	if ( GotoCheckpointDelegate.IsBound() )
	{
		// If we're currently going to a checkpoint now, ignore this request
		UE_LOG( LogHttpReplay, Warning, TEXT( "FHttpNetworkReplayStreamer::GotoTimeInMS. Busy processing another checkpoint." ) );
		Delegate.ExecuteIfBound( false, -1 );
		return;
	}

	UE_LOG( LogHttpReplay, Verbose, TEXT( "FHttpNetworkReplayStreamer::GotoTimeInMS. TimeInMS: %i" ), (int)TimeInMS );

	check( DownloadCheckpointIndex == -1 );
	check( LastGotoTimeInMS == -1 );

	int32 CheckpointIndex = -1;

	LastGotoTimeInMS = FMath::Min( TimeInMS, TotalDemoTimeInMS );

	if ( CheckpointList.ReplayEvents.Num() > 0 && TimeInMS >= CheckpointList.ReplayEvents[ CheckpointList.ReplayEvents.Num() - 1 ].Time1 )
	{
		// If we're after the very last checkpoint, that's the one we want
		CheckpointIndex = CheckpointList.ReplayEvents.Num() - 1;
	}
	else
	{
		// Checkpoints should be sorted by time, return the checkpoint that exists right before the current time
		// For fine scrubbing, we'll fast forward the rest of the way
		// NOTE - If we're right before the very first checkpoint, we'll return -1, which is what we want when we want to start from the very beginning
		for ( int i = 0; i < CheckpointList.ReplayEvents.Num(); i++ )
		{
			if ( TimeInMS < CheckpointList.ReplayEvents[i].Time1 )
			{
				CheckpointIndex = i - 1;
				break;
			}
		}
	}

	GotoCheckpointIndex( CheckpointIndex, Delegate );
}

void FHttpNetworkReplayStreamer::FlushCheckpointInternal( uint32 TimeInMS )
{
	if ( SessionName.IsEmpty() || StreamerState != EStreamerState::StreamingUp || CheckpointArchive.Buffer.Num() == 0 )
	{
		// If there is no active session, or we are not recording, we don't need to flush
		CheckpointArchive.Buffer.Empty();
		CheckpointArchive.Pos = 0;
		return;
	}

	// Upload any new streamed data to the http server
	UE_LOG( LogHttpReplay, Log, TEXT( "FHttpNetworkReplayStreamer::FlushCheckpointInternal. Size: %i, StreamChunkIndex: %i" ), CheckpointArchive.Buffer.Num(), StreamChunkIndex );

	// Create the Http request and add to pending request list
	TSharedRef<class IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();

	HttpRequest->OnProcessRequestComplete().BindRaw( this, &FHttpNetworkReplayStreamer::HttpUploadCheckpointFinished );

	HttpRequest->SetURL( FString::Printf( TEXT( "%sreplay/%s/event?group=checkpoint&time1=%i&time2=%i&meta=%i" ), *ServerURL, *SessionName, TimeInMS, TimeInMS, StreamChunkIndex ) );
	HttpRequest->SetVerb( TEXT( "POST" ) );
	HttpRequest->SetHeader( TEXT( "Content-Type" ), TEXT( "application/octet-stream" ) );
	HttpRequest->SetContent( CheckpointArchive.Buffer );

	CheckpointArchive.Buffer.Empty();
	CheckpointArchive.Pos = 0;

	AddRequestToQueue( EQueuedHttpRequestType::UploadingCheckpoint, HttpRequest );
}

FQueuedHttpRequestAddEvent::FQueuedHttpRequestAddEvent( const FString& InName, const uint32 InTimeInMS, const FString& InGroup, const FString& InMeta, const TArray<uint8>& InData, TSharedRef< class IHttpRequest > InHttpRequest ) : FQueuedHttpRequest( EQueuedHttpRequestType::UploadingCustomEvent, InHttpRequest )
{
	Request->SetVerb( TEXT( "POST" ) );
	Request->SetHeader( TEXT( "Content-Type" ), TEXT( "application/octet-stream" ) );
	Request->SetContent( InData );

	Name		= InName;
	TimeInMS	= InTimeInMS;
	Group		= InGroup;
	Meta		= InMeta;
}

bool FQueuedHttpRequestAddEvent::PreProcess( const FString& ServerURL, const FString& SessionName )
{
	if ( SessionName.IsEmpty() )
	{
		UE_LOG( LogHttpReplay, Warning, TEXT( "FQueuedHttpRequestAddEvent::PreProcess. SessionName is empty." ) );
		return false;
	}

	//
	// Now that we have the session name, we can set the URL
	//

	if ( !Name.IsEmpty() )
	{
		// Add or update existing event
		const FString EventName = SessionName + TEXT( "_" ) + Name;
		Request->SetURL( FString::Printf( TEXT( "%sreplay/%s/event/%s?group=%s&time1=%i&time2=%i&meta=%s" ), *ServerURL, *SessionName, *EventName, *Group, TimeInMS, TimeInMS, *FGenericPlatformHttp::UrlEncode( Meta ) ) );
	}
	else
	{
		Request->SetURL( FString::Printf( TEXT( "%sreplay/%s/event?group=%s&time1=%i&time2=%i&meta=%s" ), *ServerURL, *SessionName, *Group, TimeInMS, TimeInMS, *FGenericPlatformHttp::UrlEncode( Meta ) ) );
	}

	return true;
}

void FHttpNetworkReplayStreamer::AddOrUpdateEvent( const FString& Name, const uint32 TimeInMS, const FString& Group, const FString& Meta, const TArray<uint8>& Data )
{
	if ( StreamerState != EStreamerState::StreamingUp && StreamerState != EStreamerState::StreamingDown )
	{
		UE_LOG( LogHttpReplay, Warning, TEXT( "FHttpNetworkReplayStreamer::AddOrUpdateEvent. Not streaming." ) );
		return;
	}

	// Upload a custom event to replay server
	UE_LOG( LogHttpReplay, Verbose, TEXT( "FHttpNetworkReplayStreamer::AddEvent. Size: %i, StreamChunkIndex: %i" ), Data.Num(), StreamChunkIndex );

	// Create the Http request and add to pending request list
	TSharedRef< class IHttpRequest > HttpRequest = FHttpModule::Get().CreateRequest();

	HttpRequest->OnProcessRequestComplete().BindRaw( this, &FHttpNetworkReplayStreamer::HttpUploadCustomEventFinished );

	// Add it as a custom event so we can snag the session name at the time of send (which we should have by then)
	AddCustomRequestToQueue( TSharedPtr< FQueuedHttpRequest >( new FQueuedHttpRequestAddEvent( Name, TimeInMS, Group, Meta, Data, HttpRequest ) ) );
}

void FHttpNetworkReplayStreamer::AddEvent( const uint32 TimeInMS, const FString& Group, const FString& Meta, const TArray<uint8>& Data )
{
	if (StreamerState != EStreamerState::StreamingUp && StreamerState != EStreamerState::StreamingDown)
	{
		UE_LOG( LogHttpReplay, Warning, TEXT( "FHttpNetworkReplayStreamer::AddEvent. Not streaming." ) );
		return;
	}

	AddOrUpdateEvent( TEXT( "" ), TimeInMS, Group, Meta, Data );
}

void FHttpNetworkReplayStreamer::DownloadHeader()
{
	// Download header first
	TSharedRef<class IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();

	HttpRequest->SetURL( FString::Printf( TEXT( "%sreplay/%s/file/replay.header" ), *ServerURL, *SessionName, *ViewerName ) );
	HttpRequest->SetVerb( TEXT( "GET" ) );

	HttpRequest->OnProcessRequestComplete().BindRaw( this, &FHttpNetworkReplayStreamer::HttpDownloadHeaderFinished );

	AddRequestToQueue( EQueuedHttpRequestType::DownloadingHeader, HttpRequest );
}

void FHttpNetworkReplayStreamer::ConditionallyDownloadNextChunk()
{
	if ( IsHttpRequestInFlight() )
	{
		// We never download next chunk if there is an http request in flight
		return;
	}

	if ( GotoCheckpointDelegate.IsBound() )
	{
		// Don't download stream while we're waiting for a checkpoint to download
		return;
	}

	const bool bMoreChunksDefinitelyAvilable	= StreamChunkIndex < NumTotalStreamChunks;		// We know for a fact there are more chunks available
	const bool bMoreChunksMayBeAvilable			= bStreamIsLive;								// If we are live, there is a chance of more chunks even if we're at the end currently

	if ( !bMoreChunksDefinitelyAvilable && !bMoreChunksMayBeAvilable )
	{
		return;
	}

	// Determine if it's time to download the next chunk
	const double CHECK_FOR_NEXT_CHUNK_IN_SECONDS = 5;

	const bool bTimeToDownloadChunk			= ( FPlatformTime::Seconds() - LastChunkTime > CHECK_FOR_NEXT_CHUNK_IN_SECONDS );	// Enough time has passed
	const bool bApproachingEnd				= StreamArchive.Pos >= StreamArchive.Buffer.Num();									// We're getting close to the end
	const bool bHighPriorityMode			= ( HighPriorityEndTime > 0 && StreamTimeRangeEnd < HighPriorityEndTime );			// We're within the high priority time range
	const bool bReallyNeedToDownloadChunk	= ( bApproachingEnd || bHighPriorityMode ) && bMoreChunksDefinitelyAvilable;

	// If it's either time to download the next chunk, or we need to for high priority reasons, download it now
	// NOTE - bTimeToDownloadChunk may be true, even though bMoreChunksDefinitelyAvilable is false
	//	This is because bMoreChunksMayBeAvilable is true, and we need to try and see if a new piece of the live stream is ready for download
	if ( !bTimeToDownloadChunk && !bReallyNeedToDownloadChunk )
	{
		// Not time yet
		return;
	}

	check( bMoreChunksDefinitelyAvilable || bMoreChunksMayBeAvilable );

	TSharedRef<class IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();

	// Download the next stream chunk
	const FString URL = FString::Printf( TEXT( "%sreplay/%s/file/stream.%i" ), *ServerURL, *SessionName, StreamChunkIndex );

	UE_LOG( LogHttpReplay, Verbose, TEXT( "FHttpNetworkReplayStreamer::ConditionallyDownloadNextChunk. URL: %s" ), *URL );

	HttpRequest->SetURL( URL );
	HttpRequest->SetVerb( TEXT( "GET" ) );

	HttpRequest->OnProcessRequestComplete().BindRaw( this, &FHttpNetworkReplayStreamer::HttpDownloadFinished );

	AddRequestToQueue( EQueuedHttpRequestType::DownloadingStream, HttpRequest );

	LastChunkTime = FPlatformTime::Seconds();
}

void FHttpNetworkReplayStreamer::KeepReplay( const FString& ReplayName, const bool bKeep )
{
	TSharedRef<class IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();

	// Download the next stream chunk
	if ( bKeep )
	{
		HttpRequest->SetURL( FString::Printf( TEXT( "%sreplay/%s?shouldKeep=true" ), *ServerURL, *ReplayName ) );
	}
	else
	{
		HttpRequest->SetURL( FString::Printf( TEXT( "%sreplay/%s?shouldKeep=false" ), *ServerURL, *ReplayName ) );
	}

	HttpRequest->SetVerb( TEXT( "POST" ) );
	HttpRequest->SetHeader( TEXT( "Content-Type" ), TEXT( "application/json" ) );

	HttpRequest->OnProcessRequestComplete().BindRaw( this, &FHttpNetworkReplayStreamer::KeepReplayFinished );

	AddRequestToQueue( EQueuedHttpRequestType::KeepReplay, HttpRequest );
}

void FHttpNetworkReplayStreamer::KeepReplayFinished( FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded )
{
	RequestFinished( StreamerState, EQueuedHttpRequestType::KeepReplay, HttpRequest );

	if ( !bSucceeded || HttpResponse->GetResponseCode() != EHttpResponseCodes::NoContent )
	{
		UE_LOG( LogHttpReplay, Error, TEXT( "FHttpNetworkReplayStreamer::KeepReplayFinished. FAILED, Response code: %d" ), HttpResponse.IsValid() ? HttpResponse->GetResponseCode() : 0 );
	}
}

void FHttpNetworkReplayStreamer::RefreshViewer( const bool bFinal )
{
	TSharedRef<class IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();

	// Download the next stream chunk
	if ( bFinal )
	{
		HttpRequest->SetURL( FString::Printf( TEXT( "%sreplay/%s/viewer/%s?final=true" ), *ServerURL, *SessionName, *ViewerName ) );
	}
	else
	{
		HttpRequest->SetURL( FString::Printf( TEXT( "%sreplay/%s/viewer/%s" ), *ServerURL, *SessionName, *ViewerName ) );
	}

	HttpRequest->SetVerb( TEXT( "POST" ) );

	HttpRequest->OnProcessRequestComplete().BindRaw( this, &FHttpNetworkReplayStreamer::HttpRefreshViewerFinished );

	AddRequestToQueue( EQueuedHttpRequestType::RefreshingViewer, HttpRequest );

	LastRefreshViewerTime = FPlatformTime::Seconds();
}

void FHttpNetworkReplayStreamer::ConditionallyRefreshViewer()
{
	const double REFRESH_VIEWER_IN_SECONDS = 10;

	if ( FPlatformTime::Seconds() - LastRefreshViewerTime > REFRESH_VIEWER_IN_SECONDS )
	{
		RefreshViewer( false );
	}
};

void FHttpNetworkReplayStreamer::SetLastError( const ENetworkReplayError::Type InLastError )
{
	CancelStreamingRequests();

	StreamerLastError = InLastError;
}

void FHttpNetworkReplayStreamer::CancelStreamingRequests()
{
	// Cancel any in flight request
	if ( InFlightHttpRequest.IsValid() )
	{
		InFlightHttpRequest->Request->CancelRequest();
		InFlightHttpRequest = NULL;
	}

	// Empty the request queue
	TSharedPtr< FQueuedHttpRequest > QueuedRequest;
	while ( QueuedHttpRequests.Dequeue( QueuedRequest ) )
	{
	}

	StreamerState			= EStreamerState::Idle;
	bStopStreamingCalled	= false;
}

ENetworkReplayError::Type FHttpNetworkReplayStreamer::GetLastError() const
{
	return StreamerLastError;
}

FArchive* FHttpNetworkReplayStreamer::GetHeaderArchive()
{
	return &HeaderArchive;
}

FArchive* FHttpNetworkReplayStreamer::GetStreamingArchive()
{
	return &StreamArchive;
}

FArchive* FHttpNetworkReplayStreamer::GetCheckpointArchive()
{	
	if ( SessionName.IsEmpty() )
	{
		// If we need to upload the header, we're not ready to save checkpoints
		// NOTE - The code needs to be resilient to this, and keep trying!!!!
		return NULL;
	}

	return &CheckpointArchive;
}

FArchive* FHttpNetworkReplayStreamer::GetMetadataArchive()
{
	return NULL;
}

void FHttpNetworkReplayStreamer::UpdateTotalDemoTime( uint32 TimeInMS )
{
	TotalDemoTimeInMS = TimeInMS;
	StreamTimeRangeEnd = TimeInMS;
}

bool FHttpNetworkReplayStreamer::IsDataAvailable() const
{
	if ( GetLastError() != ENetworkReplayError::None )
	{
		return false;
	}

	if ( GotoCheckpointDelegate.IsBound() )
	{
		return false;
	}

	if ( HighPriorityEndTime > 0 )
	{
		// If we are waiting for a high priority portion of the stream, pretend like we don't have any data so that game code waits for the entire portion
		// of the high priority stream to download.
		// We do this because we assume the game wants to race through this high priority portion of the stream in a single frame
		return false;
	}

	// If we are loading, and we have more data
	if ( StreamArchive.IsLoading() && StreamArchive.Pos < StreamArchive.Buffer.Num() && NumTotalStreamChunks > 0 )
	{
		return true;
	}
	
	return false;
}

void FHttpNetworkReplayStreamer::SetHighPriorityTimeRange( const uint32 StartTimeInMS, const uint32 EndTimeInMS )
{
	// Set the amount of stream we should download before saying we have anymore data available
	// We will also put a high priority on this portion of the stream so that it downloads as fast as possible
	HighPriorityEndTime = EndTimeInMS;
}

bool FHttpNetworkReplayStreamer::IsDataAvailableForTimeRange( const uint32 StartTimeInMS, const uint32 EndTimeInMS )
{
	if ( GetLastError() != ENetworkReplayError::None )
	{
		return false;
	}

	// If the time is within the stream range we have downloaded, we will return true
	return ( StartTimeInMS >= StreamTimeRangeStart && EndTimeInMS <= StreamTimeRangeEnd );
}

bool FHttpNetworkReplayStreamer::IsLive() const 
{
	return bStreamIsLive;
}

bool FHttpNetworkReplayStreamer::IsLoadingCheckpoint() const
{
	return GotoCheckpointDelegate.IsBound();
}

void FHttpNetworkReplayStreamer::DeleteFinishedStream( const FString& StreamName, const FOnDeleteFinishedStreamComplete& Delegate ) const
{
	// Stubbed!
	Delegate.ExecuteIfBound(false);
}

void FHttpNetworkReplayStreamer::EnumerateStreams( const FNetworkReplayVersion& InReplayVersion, const FString& UserString, const FString& MetaString, const FOnEnumerateStreamsComplete& Delegate )
{
	EnumerateStreams( InReplayVersion, UserString, MetaString, TArray< FString >(), Delegate );
}

void FHttpNetworkReplayStreamer::EnumerateStreams( const FNetworkReplayVersion& InReplayVersion, const FString& UserString, const FString& MetaString, const TArray< FString >& ExtraParms, const FOnEnumerateStreamsComplete& Delegate )
{
	TSharedRef<class IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();

	// Build base URL
	FString URL = FString::Printf( TEXT( "%sreplay?app=%s" ), *ServerURL, *InReplayVersion.AppString );

	// Add optional stuff
	if ( InReplayVersion.Changelist != 0 )
	{
		URL += FString::Printf( TEXT( "&cl=%u" ), InReplayVersion.Changelist );
	}

	if ( InReplayVersion.NetworkVersion != 0 )
	{
		URL += FString::Printf( TEXT( "&version=%u" ), InReplayVersion.NetworkVersion );
	}

	const FString MetaStringToUse = !CVarMetaFilterOverride.GetValueOnGameThread().IsEmpty() ? CVarMetaFilterOverride.GetValueOnGameThread() : MetaString;

	// Add optional Meta parameter (filter replays by meta tag)
	if ( !MetaStringToUse.IsEmpty() )
	{
		URL += FString::Printf( TEXT( "&meta=%s" ), *MetaStringToUse );
	}

	// Add optional User parameter (filter replays by a user that was in the replay)
	if ( !UserString.IsEmpty() )
	{
		URL += FString::Printf( TEXT( "&user=%s" ), *UserString );
	}

	// Add any extra parms now
	for ( int i = 0; i < ExtraParms.Num(); i++ )
	{
		URL += FString::Printf( TEXT( "&%s" ), *ExtraParms[i] );
	}

	HttpRequest->SetURL( URL );

	HttpRequest->SetVerb( TEXT( "GET" ) );

	HttpRequest->OnProcessRequestComplete().BindRaw( this, &FHttpNetworkReplayStreamer::HttpEnumerateSessionsFinished, Delegate );

	AddRequestToQueue( EQueuedHttpRequestType::EnumeratingSessions, HttpRequest );
}

void FHttpNetworkReplayStreamer::EnumerateRecentStreams( const FNetworkReplayVersion& InReplayVersion, const FString& InRecentViewer, const FOnEnumerateStreamsComplete& Delegate )
{
	TSharedRef<class IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();

	// Enumerate all of the sessions
	HttpRequest->SetURL( FString::Printf( TEXT( "%sreplay?app=%s&version=%u&cl=%u&recent=%s" ), *ServerURL, *InReplayVersion.AppString, InReplayVersion.NetworkVersion, InReplayVersion.Changelist, *InRecentViewer ) );
	HttpRequest->SetVerb( TEXT( "GET" ) );

	HttpRequest->OnProcessRequestComplete().BindRaw( this, &FHttpNetworkReplayStreamer::HttpEnumerateSessionsFinished, Delegate );

	AddRequestToQueue( EQueuedHttpRequestType::EnumeratingSessions, HttpRequest );
}

void FHttpNetworkReplayStreamer::AddUserToReplay(const FString& UserString)
{
	if ( StreamerState != EStreamerState::StreamingUp )
	{
		return;
	}

	if ( UserString.IsEmpty() )
	{
		UE_LOG(LogHttpReplay, Log, TEXT("FHttpNetworkReplayStreamer::AddUserToReplay: can't add a user with an empty UserString."));
		return;
	}

	TSharedRef<class IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();

	HttpRequest->SetURL( FString::Printf( TEXT( "%sreplay/%s/users" ), *ServerURL, *SessionName ) );
	HttpRequest->SetVerb( TEXT( "POST" ) );

	FNetworkReplayUserList UserList;

	UserList.Users.Add(UserString);
	FString JsonString = UserList.ToJson();
	HttpRequest->SetContentAsString( JsonString );
	HttpRequest->SetHeader( TEXT( "Content-Type" ), TEXT( "application/json" ) );

	HttpRequest->OnProcessRequestComplete().BindRaw( this, &FHttpNetworkReplayStreamer::HttpAddUserFinished );

	AddRequestToQueue( EQueuedHttpRequestType::AddingUser, HttpRequest );
}

void FHttpNetworkReplayStreamer::EnumerateCheckpoints()
{
	TSharedRef<class IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();

	// Enumerate all of the sessions
	HttpRequest->SetURL( FString::Printf( TEXT( "%sreplay/%s/event?group=checkpoint" ), *ServerURL, *SessionName ) );
	HttpRequest->SetVerb( TEXT( "GET" ) );

	HttpRequest->OnProcessRequestComplete().BindRaw( this, &FHttpNetworkReplayStreamer::HttpEnumerateCheckpointsFinished );

	AddRequestToQueue( EQueuedHttpRequestType::EnumeratingCheckpoints, HttpRequest );

	LastRefreshCheckpointTime = FPlatformTime::Seconds();
}

void FHttpNetworkReplayStreamer::ConditionallyEnumerateCheckpoints()
{
	if ( !bStreamIsLive )
	{
		// We don't need to enumerate more than once for non live streams
		return;
	}

	// During live games, check for new checkpoints every 30 seconds
	const double REFRESH_CHECKPOINTS_IN_SECONDS = 30;

	if ( FPlatformTime::Seconds() - LastRefreshCheckpointTime > REFRESH_CHECKPOINTS_IN_SECONDS )
	{
		EnumerateCheckpoints();
	}
};

void FHttpNetworkReplayStreamer::EnumerateEvents(const FString& Group, const FEnumerateEventsCompleteDelegate& EnumerationCompleteDelegate)
{
	EnumerateEvents( SessionName, Group, EnumerationCompleteDelegate );
}

void FHttpNetworkReplayStreamer::EnumerateEvents( const FString& ReplayName, const FString& Group, const FEnumerateEventsCompleteDelegate& EnumerationCompleteDelegate )
{
	TSharedRef<class IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();

	// Enumerate all of the events
	HttpRequest->SetURL( FString::Printf( TEXT( "%sreplay/%s/event?group=%s" ), *ServerURL, *ReplayName, *Group ) );
	HttpRequest->SetVerb( TEXT( "GET" ) );

	HttpRequest->OnProcessRequestComplete().BindRaw( this, &FHttpNetworkReplayStreamer::HttpEnumerateEventsFinished, EnumerationCompleteDelegate );

	AddRequestToQueue( EQueuedHttpRequestType::EnumeratingCustomEvent, HttpRequest );
}

void FHttpNetworkReplayStreamer::RequestFinished( EStreamerState ExpectedStreamerState, EQueuedHttpRequestType::Type ExpectedType, FHttpRequestPtr HttpRequest )
{
	check( StreamerState == ExpectedStreamerState );
	check( InFlightHttpRequest.IsValid() );
	check( InFlightHttpRequest->Request == HttpRequest );
	check( InFlightHttpRequest->Type == ExpectedType );

	InFlightHttpRequest = NULL;
};

static FString BuildRequestErrorString( FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse )
{
	FString ExtraInfo;

	if ( HttpRequest.IsValid() )
	{
		ExtraInfo += FString::Printf( TEXT( "URL: %s" ), *HttpRequest->GetURL() );
		ExtraInfo += FString::Printf( TEXT( ", Verb: %s" ), *HttpRequest->GetVerb() );

		const TArray< FString > AllHeaders = HttpRequest->GetAllHeaders();

		for ( int i = 0; i < AllHeaders.Num(); i++ )
		{
			ExtraInfo += TEXT( ", " );
			ExtraInfo += AllHeaders[i];
		}
	}
	else
	{
		ExtraInfo = TEXT( "HttpRequest NULL." );
	}

	return FString::Printf( TEXT( "Response code: %d, Extra info: %s" ), HttpResponse.IsValid() ? HttpResponse->GetResponseCode() : 0, *ExtraInfo );
}

void FHttpNetworkReplayStreamer::HttpStartUploadingFinished( FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded )
{
	RequestFinished( EStreamerState::StreamingUp, EQueuedHttpRequestType::StartUploading, HttpRequest );

	if ( bSucceeded && HttpResponse->GetResponseCode() == EHttpResponseCodes::Ok )
	{
		FString JsonString = HttpResponse->GetContentAsString();

		FNetworkReplayStartUploadingResponse StartUploadingResponse;

		if (!StartUploadingResponse.FromJson(JsonString))
		{
			UE_LOG(LogHttpReplay, Warning, TEXT("FHttpNetworkReplayStreamer::HttpStartUploadingFinished. FromJson FAILED"));
			// FIXME: Is there more I should do here?
			return;
		}

		SessionName = StartUploadingResponse.SessionId;

		UE_LOG( LogHttpReplay, Log, TEXT( "FHttpNetworkReplayStreamer::HttpStartUploadingFinished. SessionName: %s" ), *SessionName );
	}
	else
	{
		UE_LOG( LogHttpReplay, Error, TEXT( "FHttpNetworkReplayStreamer::HttpStartUploadingFinished. FAILED, %s" ), *BuildRequestErrorString( HttpRequest, HttpResponse ) );
		SetLastError( ENetworkReplayError::ServiceUnavailable );
	}
}

void FHttpNetworkReplayStreamer::HttpStopUploadingFinished( FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded )
{
	RequestFinished( EStreamerState::StreamingUp, EQueuedHttpRequestType::StopUploading, HttpRequest );

	if ( bSucceeded && HttpResponse->GetResponseCode() == EHttpResponseCodes::NoContent )
	{
		UE_LOG( LogHttpReplay, Log, TEXT( "FHttpNetworkReplayStreamer::HttpStopUploadingFinished. SessionName: %s" ), *SessionName );
	}
	else
	{
		UE_LOG( LogHttpReplay, Error, TEXT( "FHttpNetworkReplayStreamer::HttpStopUploadingFinished. FAILED, %s" ), *BuildRequestErrorString( HttpRequest, HttpResponse ) );
		SetLastError( ENetworkReplayError::ServiceUnavailable );
	}

	StreamArchive.ArIsLoading = false;
	StreamArchive.ArIsSaving = false;
	StreamArchive.Buffer.Empty();
	StreamArchive.Pos = 0;
	StreamChunkIndex = 0;
	SessionName.Empty();
}

void FHttpNetworkReplayStreamer::HttpHeaderUploadFinished( FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded )
{
	RequestFinished( EStreamerState::StreamingUp, EQueuedHttpRequestType::UploadingHeader, HttpRequest );

	check( StartStreamingDelegate.IsBound() );

	if ( bSucceeded && HttpResponse->GetResponseCode() == EHttpResponseCodes::NoContent )
	{
		UE_LOG( LogHttpReplay, Log, TEXT( "FHttpNetworkReplayStreamer::HttpHeaderUploadFinished." ) );

		StartStreamingDelegate.ExecuteIfBound( true, true );
	}
	else
	{
		UE_LOG( LogHttpReplay, Error, TEXT( "FHttpNetworkReplayStreamer::HttpHeaderUploadFinished. FAILED, %s" ), *BuildRequestErrorString( HttpRequest, HttpResponse ) );
		StartStreamingDelegate.ExecuteIfBound( false, true );
		SetLastError( ENetworkReplayError::ServiceUnavailable );
	}

	// Reset delegate
	StartStreamingDelegate = FOnStreamReadyDelegate();
}

void FHttpNetworkReplayStreamer::HttpUploadStreamFinished( FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded )
{
	RequestFinished( EStreamerState::StreamingUp, EQueuedHttpRequestType::UploadingStream, HttpRequest );

	if ( bSucceeded && HttpResponse->GetResponseCode() == EHttpResponseCodes::NoContent )
	{
		UE_LOG( LogHttpReplay, Verbose, TEXT( "FHttpNetworkReplayStreamer::HttpUploadStreamFinished." ) );
	}
	else
	{
		UE_LOG( LogHttpReplay, Error, TEXT( "FHttpNetworkReplayStreamer::HttpUploadStreamFinished. FAILED, %s" ), *BuildRequestErrorString( HttpRequest, HttpResponse ) );
		SetLastError( ENetworkReplayError::ServiceUnavailable );
	}
}

void FHttpNetworkReplayStreamer::HttpUploadCheckpointFinished( FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded )
{
	RequestFinished( EStreamerState::StreamingUp, EQueuedHttpRequestType::UploadingCheckpoint, HttpRequest );

	if ( bSucceeded && ( HttpResponse->GetResponseCode() == EHttpResponseCodes::Ok || HttpResponse->GetResponseCode() == EHttpResponseCodes::NoContent ) )
	{
		UE_LOG( LogHttpReplay, Verbose, TEXT( "FHttpNetworkReplayStreamer::HttpUploadCheckpointFinished." ) );
	}
	else
	{
		UE_LOG( LogHttpReplay, Error, TEXT( "FHttpNetworkReplayStreamer::HttpUploadCheckpointFinished. FAILED, %s" ), *BuildRequestErrorString( HttpRequest, HttpResponse ) );
		SetLastError( ENetworkReplayError::ServiceUnavailable );
	}
}

void FHttpNetworkReplayStreamer::HttpUploadCustomEventFinished(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded)
{
	RequestFinished(StreamerState, EQueuedHttpRequestType::UploadingCustomEvent, HttpRequest);

	if ( bSucceeded && ( HttpResponse->GetResponseCode() == EHttpResponseCodes::Ok || HttpResponse->GetResponseCode() == EHttpResponseCodes::NoContent ) )
	{
		UE_LOG(LogHttpReplay, Verbose, TEXT("FHttpNetworkReplayStreamer::HttpUploadCustomEventFinished."));
	}
	else
	{
		FString ExtraInfo;

		if ( HttpRequest.IsValid() )
		{
			ExtraInfo += FString::Printf( TEXT( "URL: %s" ),	*HttpRequest->GetURL() );
			ExtraInfo += FString::Printf( TEXT( ", Verb: %s" ), *HttpRequest->GetVerb() );

			const TArray< FString > AllHeaders = HttpRequest->GetAllHeaders();

			for ( int i = 0; i < AllHeaders.Num(); i++ )
			{
				ExtraInfo += TEXT( ", " );
				ExtraInfo += AllHeaders[i];
			}
		}
		else
		{
			ExtraInfo = TEXT( "HttpRequest NULL." );
		}

		UE_LOG( LogHttpReplay, Error, TEXT( "FHttpNetworkReplayStreamer::HttpUploadCustomEventFinished. FAILED. Response code: %d, Extra info: %s" ), HttpResponse.IsValid() ? HttpResponse->GetResponseCode() : 0, *ExtraInfo );
		// Don't disconect service here, just report the failure
		//SetLastError(ENetworkReplayError::ServiceUnavailable);
	}
}

void FHttpNetworkReplayStreamer::HttpStartDownloadingFinished( FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded )
{
	RequestFinished( EStreamerState::StreamingDown, EQueuedHttpRequestType::StartDownloading, HttpRequest );

	if ( bSucceeded && HttpResponse->GetResponseCode() == EHttpResponseCodes::Ok )
	{

		FString JsonString = HttpResponse->GetContentAsString();

		FNetworkReplayStartDownloadingResponse StartDownloadingResponse;

		if (!StartDownloadingResponse.FromJson(JsonString))
		{
			UE_LOG(LogHttpReplay, Warning, TEXT("FHttpNetworkReplayStreamer::HttpStartDownloadingFinished. FromJson FAILED"));
			// FIXME: Is there more I should do here?
			return;
		}

		FString State = StartDownloadingResponse.State;
		ViewerName = StartDownloadingResponse.Viewer;

		bStreamIsLive = State == TEXT( "Live" );

		NumTotalStreamChunks = StartDownloadingResponse.NumChunks;
		TotalDemoTimeInMS = StartDownloadingResponse.Time;

		UE_LOG( LogHttpReplay, Log, TEXT( "FHttpNetworkReplayStreamer::HttpStartDownloadingFinished. Viewer: %s, State: %s, NumChunks: %i, DemoTime: %2.2f" ), *ViewerName, *State, NumTotalStreamChunks, (float)TotalDemoTimeInMS / 1000 );

		if ( NumTotalStreamChunks == 0 )
		{			
			UE_LOG( LogHttpReplay, Warning, TEXT( "FHttpNetworkReplayStreamer::HttpStartDownloadingFinished. NO CHUNKS" ) );

			StartStreamingDelegate.ExecuteIfBound( false, true );

			// Reset delegate
			StartStreamingDelegate = FOnStreamReadyDelegate();

			SetLastError( ENetworkReplayError::ServiceUnavailable );
		}
	}
	else
	{
		UE_LOG( LogHttpReplay, Error, TEXT( "FHttpNetworkReplayStreamer::HttpStartDownloadingFinished. FAILED, %s" ), *BuildRequestErrorString( HttpRequest, HttpResponse ) );

		StartStreamingDelegate.ExecuteIfBound( false, true );

		// Reset delegate
		StartStreamingDelegate = FOnStreamReadyDelegate();

		SetLastError( ENetworkReplayError::ServiceUnavailable );
	}
}

void FHttpNetworkReplayStreamer::HttpDownloadHeaderFinished( FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded )
{
	RequestFinished( EStreamerState::StreamingDown, EQueuedHttpRequestType::DownloadingHeader, HttpRequest );

	check( StreamArchive.IsLoading() );
	check( StartStreamingDelegate.IsBound() );

	if ( bSucceeded && HttpResponse->GetResponseCode() == EHttpResponseCodes::Ok )
	{
		HeaderArchive.Buffer.Append( HttpResponse->GetContent() );

		UE_LOG( LogHttpReplay, Log, TEXT( "FHttpNetworkReplayStreamer::HttpDownloadHeaderFinished. Size: %i" ), HeaderArchive.Buffer.Num() );
	}
	else
	{
		// FAIL
		UE_LOG( LogHttpReplay, Error, TEXT( "FHttpNetworkReplayStreamer::HttpDownloadHeaderFinished. FAILED, %s" ), *BuildRequestErrorString( HttpRequest, HttpResponse ) );

		StreamArchive.Buffer.Empty();
		StartStreamingDelegate.ExecuteIfBound( false, false );

		// Reset delegate
		StartStreamingDelegate = FOnStreamReadyDelegate();

		SetLastError( ENetworkReplayError::ServiceUnavailable );
	}
}

void FHttpNetworkReplayStreamer::HttpDownloadFinished( FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded )
{
	RequestFinished( EStreamerState::StreamingDown, EQueuedHttpRequestType::DownloadingStream, HttpRequest );

	check( StreamArchive.IsLoading() );

	if ( bSucceeded && HttpResponse->GetResponseCode() == EHttpResponseCodes::Ok )
	{
		if ( HttpResponse->GetHeader( TEXT( "NumChunks" ) ) == TEXT( "" ) )
		{
			// Assume this is an implcit status update
			UE_LOG( LogHttpReplay, Log, TEXT( "FHttpNetworkReplayStreamer::HttpDownloadFinished. NO HEADER FIELDS. Live: %i, Progress: %i / %i, Start: %i, End: %i, DemoTime: %2.2f" ), ( int )bStreamIsLive, StreamChunkIndex, NumTotalStreamChunks, ( int )StreamTimeRangeStart, ( int )StreamTimeRangeEnd, ( float )TotalDemoTimeInMS / 1000 );
			return;
		}

		NumTotalStreamChunks = FCString::Atoi( *HttpResponse->GetHeader( TEXT( "NumChunks" ) ) );
		TotalDemoTimeInMS = FCString::Atoi( *HttpResponse->GetHeader( TEXT( "Time" ) ) );
		bStreamIsLive = HttpResponse->GetHeader( TEXT( "State" ) ) == TEXT( "Live" );

		if ( StreamArchive.Buffer.Num() == 0 )
		{
			// If we haven't started streaming yet, this will be the start of this stream
			StreamTimeRangeStart = FCString::Atoi( *HttpResponse->GetHeader( TEXT( "MTime1" ) ) );
		}

		// This is the new end of the stream
		StreamTimeRangeEnd = FCString::Atoi( *HttpResponse->GetHeader( TEXT( "MTime2" ) ) );

		if ( HttpResponse->GetContent().Num() > 0 || bStreamIsLive )
		{
			if ( HttpResponse->GetContent().Num() > 0 )
			{
				StreamArchive.Buffer.Append( HttpResponse->GetContent() );
				StreamChunkIndex++;
			}

			UE_LOG( LogHttpReplay, Verbose, TEXT( "FHttpNetworkReplayStreamer::HttpDownloadFinished. Live: %i, Progress: %i / %i, Start: %i, End: %i, DemoTime: %2.2f" ), (int)bStreamIsLive, StreamChunkIndex, NumTotalStreamChunks, (int)StreamTimeRangeStart, (int)StreamTimeRangeEnd, (float)TotalDemoTimeInMS / 1000 );
		}
		else
		{
			UE_LOG( LogHttpReplay, Error, TEXT( "FHttpNetworkReplayStreamer::HttpDownloadFinished. FAILED." ) );
			StreamArchive.Buffer.Empty();
			SetLastError( ENetworkReplayError::ServiceUnavailable );
		}		
	}
	else
	{
		if ( bStreamIsLive )
		{
			bStreamIsLive = false;

			UE_LOG( LogHttpReplay, Warning, TEXT( "FHttpNetworkReplayStreamer::HttpDownloadFinished. Failed live, turning off live flag. Response code: %d, Live: %i, Progress: %i / %i, Start: %i, End: %i, DemoTime: %2.2f" ), HttpResponse.IsValid() ? HttpResponse->GetResponseCode() : 0, ( int )bStreamIsLive, StreamChunkIndex, NumTotalStreamChunks, ( int )StreamTimeRangeStart, ( int )StreamTimeRangeEnd, ( float )TotalDemoTimeInMS / 1000 );
		}
		else
		{
			UE_LOG( LogHttpReplay, Error, TEXT( "FHttpNetworkReplayStreamer::HttpDownloadFinished. FAILED, %s" ), *BuildRequestErrorString( HttpRequest, HttpResponse ) );
			StreamArchive.Buffer.Empty();
			SetLastError( ENetworkReplayError::ServiceUnavailable );
		}
	}
}

void FHttpNetworkReplayStreamer::HttpDownloadCheckpointFinished( FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded )
{
	UE_LOG( LogHttpReplay, Verbose, TEXT( "FHttpNetworkReplayStreamer::HttpDownloadCheckpointFinished." ) );

	RequestFinished( EStreamerState::StreamingDown, EQueuedHttpRequestType::DownloadingCheckpoint, HttpRequest );

	check( StreamArchive.IsLoading() );
	check( GotoCheckpointDelegate.IsBound() );
	check( DownloadCheckpointIndex >= 0 );

	if ( bSucceeded && HttpResponse->GetResponseCode() == EHttpResponseCodes::Ok )
	{
		if ( HttpResponse->GetContent().Num() == 0 )
		{
			UE_LOG( LogHttpReplay, Warning, TEXT( "FHttpNetworkReplayStreamer::HttpDownloadCheckpointFinished. Checkpoint empty." ) );
			GotoCheckpointDelegate.ExecuteIfBound( false, -1 );
			GotoCheckpointDelegate = FOnCheckpointReadyDelegate();
			return;
		}

		// Get the checkpoint data
		CheckpointArchive.Buffer	= HttpResponse->GetContent();
		CheckpointArchive.Pos		= 0;

		// Completely reset our stream (we're going to start downloading from the start of the checkpoint)
		StreamArchive.Buffer.Empty();
		StreamArchive.Pos				= 0;
		StreamArchive.bAtEndOfReplay	= false;

		// Reset any time we were waiting on in the past
		HighPriorityEndTime	= 0;

		// Reset our stream range
		StreamTimeRangeStart	= 0;
		StreamTimeRangeEnd		= 0;

		// Set the next chunk to be right after this checkpoint (which was stored in the metadata)
		StreamChunkIndex = FCString::Atoi( *CheckpointList.ReplayEvents[ DownloadCheckpointIndex ].Metadata );

		// If we want to fast forward past the end of a stream, clamp to the checkpoint
		if ( LastGotoTimeInMS >= 0 && StreamChunkIndex >= NumTotalStreamChunks )
		{
			UE_LOG( LogHttpReplay, Warning, TEXT( "FHttpNetworkReplayStreamer::HttpDownloadCheckpointFinished. Clamped to checkpoint: %i" ), LastGotoTimeInMS );

			StreamTimeRangeStart	= CheckpointList.ReplayEvents[DownloadCheckpointIndex].Time1;
			StreamTimeRangeEnd		= CheckpointList.ReplayEvents[DownloadCheckpointIndex].Time1;
			LastGotoTimeInMS		= -1;
		}

		if ( LastGotoTimeInMS >= 0 )
		{
			// If we are fine scrubbing, make sure to wait on the part of the stream that is needed to do this in one frame
			SetHighPriorityTimeRange( CheckpointList.ReplayEvents[ DownloadCheckpointIndex ].Time1, LastGotoTimeInMS );
			
			// Subtract off checkpoint time so we pass in the leftover to the engine to fast forward through for the fine scrubbing part
			LastGotoTimeInMS -= CheckpointList.ReplayEvents[ DownloadCheckpointIndex ].Time1;
		}

		// Notify game code of success
		GotoCheckpointDelegate.ExecuteIfBound( true, LastGotoTimeInMS );

		UE_LOG( LogHttpReplay, Verbose, TEXT( "FHttpNetworkReplayStreamer::HttpDownloadCheckpointFinished. SUCCESS. StreamChunkIndex: %i" ), StreamChunkIndex );
	}
	else
	{
		// Oops, something went wrong, notify game code of failure
		UE_LOG( LogHttpReplay, Error, TEXT( "FHttpNetworkReplayStreamer::HttpDownloadCheckpointFinished. FAILED, %s" ), *BuildRequestErrorString( HttpRequest, HttpResponse ) );
		GotoCheckpointDelegate.ExecuteIfBound( false, -1 );
	}

	// Reset things
	GotoCheckpointDelegate	= FOnCheckpointReadyDelegate();
	DownloadCheckpointIndex = -1;
	LastGotoTimeInMS		= -1;
}

void FHttpNetworkReplayStreamer::HttpRefreshViewerFinished( FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded )
{
	RequestFinished( EStreamerState::StreamingDown, EQueuedHttpRequestType::RefreshingViewer, HttpRequest );

	if ( !bSucceeded || HttpResponse->GetResponseCode() != EHttpResponseCodes::NoContent )
	{
		UE_LOG( LogHttpReplay, Error, TEXT( "FHttpNetworkReplayStreamer::HttpRefreshViewerFinished. FAILED, %s" ), *BuildRequestErrorString( HttpRequest, HttpResponse ) );
		SetLastError( ENetworkReplayError::ServiceUnavailable );
	}
}

void FHttpNetworkReplayStreamer::HttpEnumerateSessionsFinished( FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded, FOnEnumerateStreamsComplete Delegate )
{
	check( InFlightHttpRequest.IsValid() );
	check( InFlightHttpRequest->Request == HttpRequest );
	check( InFlightHttpRequest->Type == EQueuedHttpRequestType::EnumeratingSessions );

	InFlightHttpRequest = NULL;

	if ( bSucceeded && HttpResponse->GetResponseCode() == EHttpResponseCodes::Ok )
	{		
		UE_LOG( LogHttpReplay, Verbose, TEXT( "FHttpNetworkReplayStreamer::HttpEnumerateSessionsFinished." ) );

		TArray<FNetworkReplayStreamInfo> Streams;
		FString JsonString = HttpResponse->GetContentAsString();

		FNetworkReplayList ReplayList;

		if ( !ReplayList.FromJson( JsonString ) )
		{
			UE_LOG( LogHttpReplay, Warning, TEXT( "FHttpNetworkReplayStreamer::HttpEnumerateSessionsFinished. FromJson FAILED" ) );
			Delegate.ExecuteIfBound( TArray<FNetworkReplayStreamInfo>() );		// FIXME: Notify failure here
			return;
		}

		for ( int32 i = 0; i < ReplayList.Replays.Num(); i++ )
		{
			FNetworkReplayStreamInfo NewStream;

			NewStream.Name			= ReplayList.Replays[i].SessionName;
			NewStream.FriendlyName	= ReplayList.Replays[i].FriendlyName;
			NewStream.Timestamp		= ReplayList.Replays[i].Timestamp;
			NewStream.SizeInBytes	= ReplayList.Replays[i].SizeInBytes;
			NewStream.LengthInMS	= ReplayList.Replays[i].DemoTimeInMs;
			NewStream.NumViewers	= ReplayList.Replays[i].NumViewers;
			NewStream.bIsLive		= ReplayList.Replays[i].bIsLive;
			NewStream.Changelist	= ReplayList.Replays[i].Changelist;
			NewStream.bShouldKeep	= ReplayList.Replays[i].bShouldKeep;

			Streams.Add( NewStream );
		}

		Delegate.ExecuteIfBound( Streams );
	}
	else
	{
		UE_LOG( LogHttpReplay, Warning, TEXT( "FHttpNetworkReplayStreamer::HttpEnumerateSessionsFinished. FAILED, %s" ), *BuildRequestErrorString( HttpRequest, HttpResponse ) );
		Delegate.ExecuteIfBound( TArray<FNetworkReplayStreamInfo>() );		// FIXME: Notify failure here
	}
}

void FHttpNetworkReplayStreamer::HttpEnumerateCheckpointsFinished( FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded )
{
	RequestFinished( EStreamerState::StreamingDown, EQueuedHttpRequestType::EnumeratingCheckpoints, HttpRequest );

	if ( bSucceeded && HttpResponse->GetResponseCode() == EHttpResponseCodes::Ok )
	{		
		UE_LOG( LogHttpReplay, Verbose, TEXT( "FHttpNetworkReplayStreamer::HttpEnumerateCheckpointsFinished." ) );

		FString JsonString = HttpResponse->GetContentAsString();

		CheckpointList.ReplayEvents.Empty();

		if ( !CheckpointList.FromJson( JsonString ) )
		{
			UE_LOG( LogHttpReplay, Warning, TEXT( "FHttpNetworkReplayStreamer::HttpEnumerateCheckpointsFinished. FromJson FAILED" ) );

			StartStreamingDelegate.ExecuteIfBound( false, false );

			// Reset delegate
			StartStreamingDelegate = FOnStreamReadyDelegate();
			SetLastError( ENetworkReplayError::ServiceUnavailable );
			return;
		}

		// Sort checkpoints by time
		struct FCompareCheckpointTime
		{
			FORCEINLINE bool operator()(const FReplayEventListItem & A, const FReplayEventListItem & B) const
			{
				return A.Time1 < B.Time1;
			}
		};

		Sort( CheckpointList.ReplayEvents.GetData(), CheckpointList.ReplayEvents.Num(), FCompareCheckpointTime() );

		StartStreamingDelegate.ExecuteIfBound( true, false );
	}
	else
	{
		UE_LOG( LogHttpReplay, Warning, TEXT( "FHttpNetworkReplayStreamer::HttpEnumerateCheckpointsFinished. FAILED, %s" ), *BuildRequestErrorString( HttpRequest, HttpResponse ) );
		StartStreamingDelegate.ExecuteIfBound( false, false );

		// Reset delegate
		StartStreamingDelegate = FOnStreamReadyDelegate();

		SetLastError( ENetworkReplayError::ServiceUnavailable );
	}

	// Reset delegate
	StartStreamingDelegate = FOnStreamReadyDelegate();
}

void FHttpNetworkReplayStreamer::HttpEnumerateEventsFinished(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded, FEnumerateEventsCompleteDelegate EnumerateEventsDelegate)
{
	RequestFinished(StreamerState, EQueuedHttpRequestType::EnumeratingCustomEvent, HttpRequest);

	if (bSucceeded && HttpResponse->GetResponseCode() == EHttpResponseCodes::Ok)
	{
		FReplayEventList EventList;
		FString JsonString = HttpResponse->GetContentAsString();
		if (!EventList.FromJson(JsonString))
		{
			UE_LOG(LogHttpReplay, Warning, TEXT("FHttpNetworkReplayStreamer::HttpEnumerateEventsFinished. FromJson FAILED"));

			EnumerateEventsDelegate.ExecuteIfBound(FReplayEventList(), false);

			// Reset delegate
			EnumerateEventsDelegate = FEnumerateEventsCompleteDelegate();

			SetLastError(ENetworkReplayError::ServiceUnavailable);

			return;
		}

		UE_LOG(LogHttpReplay, Verbose, TEXT("FHttpNetworkReplayStreamer::HttpEnumerateEventsFinished. %s"), *JsonString);
		EnumerateEventsDelegate.ExecuteIfBound(EventList, true);
	}
	else
	{
		UE_LOG( LogHttpReplay, Warning, TEXT( "FHttpNetworkReplayStreamer::HttpEnumerateEventsFinished. FAILED, %s" ), *BuildRequestErrorString( HttpRequest, HttpResponse ) );
		
		EnumerateEventsDelegate.ExecuteIfBound(FReplayEventList(), false);
		
		SetLastError(ENetworkReplayError::ServiceUnavailable);
	}
}

void FHttpNetworkReplayStreamer::HttpAddUserFinished(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded)
{
	RequestFinished( EStreamerState::StreamingUp, EQueuedHttpRequestType::AddingUser, HttpRequest );

	if ( bSucceeded && HttpResponse->GetResponseCode() == EHttpResponseCodes::NoContent )
	{		
		UE_LOG( LogHttpReplay, Verbose, TEXT( "FHttpNetworkReplayStreamer::HttpAddUserFinished." ) );
	}
	else
	{
		// Don't consider this a fatal error
		UE_LOG( LogHttpReplay, Warning, TEXT( "FHttpNetworkReplayStreamer::HttpAddUserFinished. FAILED, %s" ), *BuildRequestErrorString( HttpRequest, HttpResponse ) );
	}
}

bool FHttpNetworkReplayStreamer::ProcessNextHttpRequest()
{
	if ( IsHttpRequestInFlight() )
	{
		// We only process one http request at a time to keep things simple
		return false;
	}

	TSharedPtr< FQueuedHttpRequest > QueuedRequest;

	if ( QueuedHttpRequests.Dequeue( QueuedRequest ) )
	{
		UE_LOG( LogHttpReplay, Verbose, TEXT( "FHttpNetworkReplayStreamer::ProcessNextHttpRequest. Dequeue Type: %s" ), EQueuedHttpRequestType::ToString( QueuedRequest->Type ) );

		check( !InFlightHttpRequest.IsValid() );

		// Check for a couple special requests that aren't really http calls, but just using the request system for order of operations management
		if ( QueuedRequest->Type == EQueuedHttpRequestType::UploadHeader )
		{
			check( !SessionName.IsEmpty() );
			UploadHeader();
			return ProcessNextHttpRequest();
		}
		else if ( QueuedRequest->Type == EQueuedHttpRequestType::StopStreaming )
		{
			check( IsStreaming() );
			StreamerState = EStreamerState::Idle;
			bStopStreamingCalled = false;
			return ProcessNextHttpRequest();
		}

		if ( !QueuedRequest->PreProcess( ServerURL, SessionName ) )
		{
			// This request failed, go ahead and process the next request
			return ProcessNextHttpRequest();
		}

		InFlightHttpRequest = QueuedRequest;

		ProcessRequestInternal( InFlightHttpRequest->Request );

		// We can return now since we know a http request is now in flight
		return true;
	}

	return false;
}

void FHttpNetworkReplayStreamer::ProcessRequestInternal( TSharedPtr< class IHttpRequest > Request )
{
	Request->ProcessRequest();
}

void FHttpNetworkReplayStreamer::Tick( const float DeltaTime )
{
	if ( IsHttpRequestInFlight() )
	{
		// We only process one http request at a time to keep things simple
		return;
	}

	// Attempt to process the next http request
	if ( ProcessNextHttpRequest() )
	{
		// We can now return since we know there is a request in flight
		check( IsHttpRequestInFlight() );
		return;
	}

	// We should have no pending or requests in flight at this point
	check( !HasPendingHttpRequests() );

	if ( StreamerState == EStreamerState::StreamingUp )
	{
		ConditionallyFlushStream();
	}
	else if ( StreamerState == EStreamerState::StreamingDown )
	{
		// Check to see if we're done downloading the high priority portion of the strema
		// If so, we can cancel the request
		if ( HighPriorityEndTime > 0 && StreamTimeRangeEnd >= HighPriorityEndTime )
		{
			HighPriorityEndTime = 0;
		}

		// Check to see if we're at the end of non live streams
		if ( StreamChunkIndex >= NumTotalStreamChunks && !bStreamIsLive )
		{
			// Make note of when we reach the end of non live stream
			StreamArchive.bAtEndOfReplay = true;
		}

		ConditionallyRefreshViewer();
		ConditionallyDownloadNextChunk();
		ConditionallyEnumerateCheckpoints();
	}
}

bool FHttpNetworkReplayStreamer::IsHttpRequestInFlight() const
{
	return InFlightHttpRequest.IsValid();
}

bool FHttpNetworkReplayStreamer::HasPendingHttpRequests() const
{
	// If there is currently one in flight, or we have more to process, return true
	return IsHttpRequestInFlight() || !QueuedHttpRequests.IsEmpty();
}

bool FHttpNetworkReplayStreamer::IsStreaming() const
{
	return StreamerState != EStreamerState::Idle;
}

IMPLEMENT_MODULE( FHttpNetworkReplayStreamingFactory, HttpNetworkReplayStreaming )

TSharedPtr< INetworkReplayStreamer > FHttpNetworkReplayStreamingFactory::CreateReplayStreamer()
{
	TSharedPtr< FHttpNetworkReplayStreamer > Streamer( new FHttpNetworkReplayStreamer );

	HttpStreamers.Add( Streamer );

	return Streamer;
}

void FHttpNetworkReplayStreamingFactory::Tick( float DeltaTime )
{
	for ( int i = HttpStreamers.Num() - 1; i >= 0; i-- )
	{
		HttpStreamers[i]->Tick( DeltaTime );
		
		// We can release our hold when streaming is completely done
		if ( HttpStreamers[i].IsUnique() && !HttpStreamers[i]->HasPendingHttpRequests() )
		{
			if ( HttpStreamers[i]->IsStreaming() )
			{
				UE_LOG( LogHttpReplay, Warning, TEXT( "FHttpNetworkReplayStreamingFactory::Tick. Stream was stopped early." ) );
			}

			HttpStreamers.RemoveAt( i );
		}
	}
}

bool FHttpNetworkReplayStreamingFactory::IsTickable() const
{
	return true;
}

bool FHttpNetworkReplayStreamingFactory::IsTickableWhenPaused() const
{
	return true;
}

TStatId FHttpNetworkReplayStreamingFactory::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT( FHttpNetworkReplayStreamingFactory, STATGROUP_Tickables );
}
