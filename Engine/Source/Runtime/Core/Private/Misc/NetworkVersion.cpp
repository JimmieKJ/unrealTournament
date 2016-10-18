// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "CorePrivatePCH.h"
#include "NetworkVersion.h"
#include "EngineVersion.h"
#include "Runtime/Launch/Resources/Version.h"

DEFINE_LOG_CATEGORY( LogNetVersion );

FNetworkVersion::FGetLocalNetworkVersionOverride FNetworkVersion::GetLocalNetworkVersionOverride;
FNetworkVersion::FIsNetworkCompatibleOverride FNetworkVersion::IsNetworkCompatibleOverride;

FString FNetworkVersion::ProjectVersion;

enum EEngineNetworkVersionHistory
{
	HISTORY_INITIAL					= 1,
	HISTORY_REPLAY_BACKWARDS_COMPAT	= 2,		// Bump version to get rid of older replays before backwards compat was turned on officially
};

bool FNetworkVersion::bHasCachedNetworkChecksum			= false;
uint32 FNetworkVersion::CachedNetworkChecksum			= 0;

uint32 FNetworkVersion::EngineNetworkProtocolVersion	= HISTORY_REPLAY_BACKWARDS_COMPAT;
uint32 FNetworkVersion::GameNetworkProtocolVersion		= 0;

uint32 FNetworkVersion::GetNetworkCompatibleChangelist()
{
	//return FEngineVersion::Current().GetChangelist();
	return ENGINE_NET_VERSION;
}

uint32 FNetworkVersion::GetReplayCompatibleChangelist()
{
	return BUILT_FROM_CHANGELIST;
}

uint32 FNetworkVersion::GetEngineNetworkProtocolVersion()
{
	return EngineNetworkProtocolVersion;
}

uint32 FNetworkVersion::GetGameNetworkProtocolVersion()
{
	return GameNetworkProtocolVersion;
}

uint32 FNetworkVersion::GetLocalNetworkVersion( bool AllowOverrideDelegate /*=true*/ )
{
	if ( bHasCachedNetworkChecksum )
	{
		return CachedNetworkChecksum;
	}

	if ( AllowOverrideDelegate && GetLocalNetworkVersionOverride.IsBound() )
	{
		CachedNetworkChecksum = GetLocalNetworkVersionOverride.Execute();

		UE_LOG( LogNetVersion, Log, TEXT( "GetLocalNetworkVersionOverride: NetworkChecksum: %u" ), CachedNetworkChecksum );

		bHasCachedNetworkChecksum = true;

		return CachedNetworkChecksum;
	}

	// Get the project name (NOT case sensitive)
	const FString ProjectName( FString( FApp::GetGameName() ).ToLower() );

	// Start with project name+compatible changelist as seed
	CachedNetworkChecksum = FCrc::StrCrc32( *ProjectName, GetNetworkCompatibleChangelist() );

	// Next, hash with project version
	CachedNetworkChecksum = FCrc::StrCrc32( *ProjectVersion, CachedNetworkChecksum );

	// Finally, hash with engine/game network version
	const uint32 EngineNetworkVersion	= FNetworkVersion::GetEngineNetworkProtocolVersion();
	const uint32 GameNetworkVersion		= FNetworkVersion::GetGameNetworkProtocolVersion();

	CachedNetworkChecksum = FCrc::MemCrc32( &EngineNetworkVersion, sizeof( EngineNetworkVersion ), CachedNetworkChecksum );
	CachedNetworkChecksum = FCrc::MemCrc32( &GameNetworkVersion, sizeof( GameNetworkVersion ), CachedNetworkChecksum );

	UE_LOG( LogNetVersion, Log, TEXT( "GetLocalNetworkVersion: CL: %u, ProjectName: %s, ProjectVersion: %s, EngineNetworkVersion: %i, GameNetworkVersion: %i, NetworkChecksum: %u" ), FEngineVersion::CompatibleWith().GetChangelist(), *ProjectName, *ProjectVersion, EngineNetworkVersion, GameNetworkVersion, CachedNetworkChecksum );

	bHasCachedNetworkChecksum = true;

	return CachedNetworkChecksum;
}

bool FNetworkVersion::IsNetworkCompatible( const uint32 LocalNetworkVersion, const uint32 RemoteNetworkVersion )
{
	if ( IsNetworkCompatibleOverride.IsBound() )
	{
		return IsNetworkCompatibleOverride.Execute( LocalNetworkVersion, RemoteNetworkVersion );
	}

	return LocalNetworkVersion == RemoteNetworkVersion;
}

FNetworkReplayVersion FNetworkVersion::GetReplayVersion()
{
	const uint32 ReplayVersion = ( GameNetworkProtocolVersion << 16 ) | EngineNetworkProtocolVersion;

	return FNetworkReplayVersion( FApp::GetGameName(), ReplayVersion, GetReplayCompatibleChangelist() );
}
