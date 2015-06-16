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
}

IMPLEMENT_MODULE( FUTReplayStreamingFactory, UTReplayStreamer )

TSharedPtr< INetworkReplayStreamer > FUTReplayStreamingFactory::CreateReplayStreamer()
{
	TSharedPtr< FHttpNetworkReplayStreamer > Streamer( new FUTReplayStreamer );

	HttpStreamers.Add( Streamer );

	return Streamer;
}
