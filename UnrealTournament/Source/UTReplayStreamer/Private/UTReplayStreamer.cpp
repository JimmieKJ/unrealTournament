// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UTReplayStreamer.h"

DEFINE_LOG_CATEGORY_STATIC( LogUTReplay, Log, All );

FUTReplayStreamer::FUTReplayStreamer()
{
	FString McpConfigOverride;

	FParse::Value( FCommandLine::Get(), TEXT( "MCPCONFIG=" ), McpConfigOverride );

	const bool bCmdProductionEnvironment	= McpConfigOverride.Equals( TEXT( "prodnet" ), ESearchCase::IgnoreCase );
	const bool bCmdGamedevEnvironment		= McpConfigOverride.Equals( TEXT( "gamedev" ), ESearchCase::IgnoreCase );
	const bool bCmdLocalhostEnvironment		= McpConfigOverride.Equals( TEXT( "localhost" ), ESearchCase::IgnoreCase );

	const TCHAR* ProdURL	= TEXT( "https://replay-public-service-prod10.ol.epicgames.com/replay/" );
	const TCHAR* GamedevURL	= TEXT( "https://replay-public-service-gamedev.ol.epicgames.net/replay/" );
	const TCHAR* LocalURL	= TEXT( "http://localhost:8080/replay/" );

	//
	// In case nothing explicitly specified:
	//	If we're ARE a shipping build, we need to opt out of prod
	//	If we're NOT a shipping build, we have to opt in
	//
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	ServerURL = GamedevURL;
#else
	ServerURL = ProdURL;
#endif

	// Check if the mode was explicitly specified, and override appropriately
	if ( bCmdProductionEnvironment )
	{
		ServerURL = ProdURL;
	}
	else if ( bCmdLocalhostEnvironment )
	{
		ServerURL = LocalURL;
	}
	else if ( bCmdGamedevEnvironment )
	{
		ServerURL = GamedevURL;
	}

	check( !ServerURL.IsEmpty() )

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
	else
	{
		UE_LOG( LogUTReplay, Warning, TEXT( "FUTReplayStreamer::ProcessRequestInternal: No identity interface." ) );
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
