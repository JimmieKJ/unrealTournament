// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "Core.h"
#include "NetworkReplayStreaming.h"

IMPLEMENT_MODULE( FNetworkReplayStreaming, NetworkReplayStreaming );

INetworkReplayStreamingFactory& FNetworkReplayStreaming::GetFactory(const TCHAR* FactoryNameOverride)
{
	FString FactoryName = TEXT( "NullNetworkReplayStreaming" );

	if (FactoryNameOverride == nullptr)
	{
		GConfig->GetString( TEXT( "NetworkReplayStreaming" ), TEXT( "DefaultFactoryName" ), FactoryName, GEngineIni );
	}
	else
	{
		FactoryName = FactoryNameOverride;
	}

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