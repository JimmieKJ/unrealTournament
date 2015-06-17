// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UTReplayStreamer.h"

FUTReplayStreamer::FUTReplayStreamer()
{
	// Initialize the server URL
	ServerURL = TEXT( "https://replay-public-service-prod10.ol.epicgames.com/replay/" );

	FString McpConfigOverride;

	FParse::Value( FCommandLine::Get(), TEXT( "MCPCONFIG=" ), McpConfigOverride );

	if ( McpConfigOverride == TEXT( "gamedev" ) )
	{
		ServerURL = TEXT( "https://replay-public-service-gamedev.ol.epicgames.net/replay/" );
	}

	IOnlineSubsystem* OnlineSubsystem = IOnlineSubsystem::Get();

	if ( OnlineSubsystem )
	{
		OnlineIdentityInterface = OnlineSubsystem->GetIdentityInterface();
	}
}

void FUTReplayStreamer::ProcessRequestInternal( TSharedPtr< class IHttpRequest > Request )
{
	if ( OnlineIdentityInterface.IsValid() )
	{
		FString AuthToken = OnlineIdentityInterface->GetAuthToken( 0 );
		Request->SetHeader( TEXT( "Authorization" ), FString( TEXT( "bearer " ) ) + AuthToken );
	}

	Request->ProcessRequest();
}

IMPLEMENT_MODULE( FUTReplayStreamingFactory, UTReplayStreamer )

TSharedPtr< INetworkReplayStreamer > FUTReplayStreamingFactory::CreateReplayStreamer()
{
	TSharedPtr< FHttpNetworkReplayStreamer > Streamer( new FUTReplayStreamer );

	HttpStreamers.Add( Streamer );

	return Streamer;
}
