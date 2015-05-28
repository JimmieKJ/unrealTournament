// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "NetworkReplayStreaming.h"

IMPLEMENT_MODULE( FNetworkReplayStreaming, NetworkReplayStreaming );

INetworkReplayStreamingFactory& FNetworkReplayStreaming::GetFactory()
{
	FString FactoryName = TEXT( "NullNetworkReplayStreaming" );
	GConfig->GetString( TEXT( "NetworkReplayStreaming" ), TEXT( "DefaultFactoryName" ), FactoryName, GEngineIni );

	// See if we need to forcefully fallback to the null streamer
	if ( !FModuleManager::Get().IsModuleLoaded( *FactoryName ) )
	{
		FModuleManager::Get().LoadModule( *FactoryName );
	
		if ( !FModuleManager::Get().IsModuleLoaded( *FactoryName ) )
		{
			FactoryName = TEXT( "NullNetworkReplayStreaming" );
		}
	}

	return FModuleManager::Get().LoadModuleChecked< INetworkReplayStreamingFactory >( *FactoryName );
}