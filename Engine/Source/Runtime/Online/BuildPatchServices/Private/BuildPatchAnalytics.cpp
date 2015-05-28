// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	BuildPatchAnalytics.cpp: Implements static helper class for sending analytics
	events for the module.
=============================================================================*/

#define ERROR_EVENT_SEND_LIMIT 20

#include "BuildPatchServicesPrivatePCH.h"
#include "HttpServiceTracker.h"

TSharedPtr< IAnalyticsProvider > FBuildPatchAnalytics::Analytics;

TSharedPtr< FHttpServiceTracker > FBuildPatchAnalytics::HttpTracker;

FThreadSafeCounter FBuildPatchAnalytics::DownloadErrors;

FThreadSafeCounter FBuildPatchAnalytics::CacheErrors;

FThreadSafeCounter FBuildPatchAnalytics::ConstructionErrors;

void FBuildPatchAnalytics::SetAnalyticsProvider( TSharedPtr< IAnalyticsProvider > AnalyticsProvider )
{
	Analytics = AnalyticsProvider;
}

void FBuildPatchAnalytics::ResetCounters()
{
	DownloadErrors.Reset();
	CacheErrors.Reset();
	ConstructionErrors.Reset();
}

void FBuildPatchAnalytics::RecordChunkDownloadError( const FString& ChunkUrl, const int32& ResponseCode, const FString& ErrorString )
{
	if ( DownloadErrors.Increment() <= ERROR_EVENT_SEND_LIMIT )
	{
		TArray<FAnalyticsEventAttribute> Attributes;
		Attributes.Add( FAnalyticsEventAttribute( TEXT( "ChunkURL" ), ChunkUrl ) );
		Attributes.Add( FAnalyticsEventAttribute( TEXT( "ResponseCode" ), ResponseCode ) );
		Attributes.Add( FAnalyticsEventAttribute( TEXT( "ErrorString" ), ErrorString ) );
		FBuildPatchHTTP::QueueAnalyticsEvent( TEXT( "Patcher.Error.Download" ), Attributes );
	}
}

void FBuildPatchAnalytics::RecordChunkDownloadAborted( const FString& ChunkUrl, const double& ChunkTime, const double& ChunkMean, const double& ChunkStd, const double& BreakingPoint )
{
	TArray<FAnalyticsEventAttribute> Attributes;
	Attributes.Add( FAnalyticsEventAttribute( TEXT( "ChunkURL" ), ChunkUrl ) );
	Attributes.Add( FAnalyticsEventAttribute( TEXT( "ChunkTime" ), ChunkTime ) );
	Attributes.Add( FAnalyticsEventAttribute( TEXT( "ChunkMean" ), ChunkMean ) );
	Attributes.Add( FAnalyticsEventAttribute( TEXT( "ChunkStd" ), ChunkStd ) );
	Attributes.Add( FAnalyticsEventAttribute( TEXT( "BreakingPoint" ), BreakingPoint ) );
	FBuildPatchHTTP::QueueAnalyticsEvent( TEXT( "Patcher.Warning.ChunkAborted" ), Attributes );
}

void FBuildPatchAnalytics::RecordChunkCacheError( const FGuid& ChunkGuid, const FString& Filename, const int32 LastError, const FString& SystemName, const FString& ErrorString )
{
	if ( CacheErrors.Increment() <= ERROR_EVENT_SEND_LIMIT )
	{
		TArray<FAnalyticsEventAttribute> Attributes;
		Attributes.Add( FAnalyticsEventAttribute( TEXT( "ChunkGuid" ), ChunkGuid.ToString() ) );
		Attributes.Add( FAnalyticsEventAttribute( TEXT( "Filename" ), Filename ) );
		Attributes.Add( FAnalyticsEventAttribute( TEXT( "LastError" ), LastError ) );
		Attributes.Add( FAnalyticsEventAttribute( TEXT( "SystemName" ), SystemName ) );
		Attributes.Add( FAnalyticsEventAttribute( TEXT( "ErrorString" ), ErrorString ) );
		FBuildPatchHTTP::QueueAnalyticsEvent( TEXT( "Patcher.Error.Cache" ), Attributes );
	}
}

void FBuildPatchAnalytics::RecordConstructionError( const FString& Filename, const int32 LastError, const FString& ErrorString )
{
	if ( ConstructionErrors.Increment() <= ERROR_EVENT_SEND_LIMIT )
	{
		TArray<FAnalyticsEventAttribute> Attributes;
		Attributes.Add( FAnalyticsEventAttribute( TEXT( "Filename" ), Filename ) );
		Attributes.Add( FAnalyticsEventAttribute( TEXT( "LastError" ), LastError ) );
		Attributes.Add( FAnalyticsEventAttribute( TEXT( "ErrorString" ), ErrorString ) );
		FBuildPatchHTTP::QueueAnalyticsEvent( TEXT( "Patcher.Error.Construction" ), Attributes );
	}
}

void FBuildPatchAnalytics::RecordPrereqInstallnError( const FString& Filename, const FString& CommandLine, const int32 ErrorCode, const FString& ErrorString )
{
	TArray<FAnalyticsEventAttribute> Attributes;
	Attributes.Add( FAnalyticsEventAttribute( TEXT( "Filename" ), Filename ) );
	Attributes.Add( FAnalyticsEventAttribute( TEXT( "CommandLine" ), CommandLine ) );
	Attributes.Add( FAnalyticsEventAttribute( TEXT( "ErrorCode" ), ErrorCode ) );
	Attributes.Add( FAnalyticsEventAttribute( TEXT( "ErrorString" ), ErrorString ) );
	FBuildPatchHTTP::QueueAnalyticsEvent( TEXT( "Patcher.Error.Prerequisites" ), Attributes );

}

void FBuildPatchAnalytics::RecordEvent( const FString& EventName, const TArray<FAnalyticsEventAttribute>& Attributes )
{
	// Check for being only main thread
	check( IsInGameThread() );
	if ( Analytics.IsValid() )
	{
		IAnalyticsProvider& AnalyticsProvider = *Analytics.Get();
		AnalyticsProvider.RecordEvent( EventName, Attributes );
	}
}

void FBuildPatchAnalytics::SetHttpTracker(TSharedPtr< FHttpServiceTracker > InHttpTracker)
{
	HttpTracker = InHttpTracker;
}

void FBuildPatchAnalytics::TrackRequest(const FHttpRequestPtr& Request)
{
	if (HttpTracker.IsValid())
	{
		HttpTracker->TrackRequest(Request, "CDN.Chunk");
	}
}
