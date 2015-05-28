// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "NetworkReplayStreaming.h"
#include "Net/UnrealNetwork.h"
#include "GeneralProjectSettings.h"

FNetworkVersion::FGetLocalNetworkVersionOverride FNetworkVersion::GetLocalNetworkVersionOverride;
FNetworkVersion::FIsNetworkCompatibleOverride FNetworkVersion::IsNetworkCompatibleOverride;

enum ENetworkVersionHistory
{
	HISTORY_INITIAL					= 1,
	HISTORY_INTERNAL_ACK			= 3,				// We no longer save packet/channel sequence in stream. We can derive this for 100% reliable connections.
	HISTORY_REPLAY_CHECKSUMS		= 4,				// We now save replay checksums into stream for backwards compatibility checks
	HISTORY_REPLAY_CHECKSUMS2		= 5,				// Saving package and network checksum to packagemap, no longer save checksum in BeginContentBlockHeader
	HISTORY_REPLAY_CHECKSUMS3		= 6,				// Various network checksum fixes
	HISTORY_REPLAY_CHECKSUMS4		= 7,				// Added multi-cast RPC's to network checksum
	HISTORY_REPLAY_RPC_CHECKSUMS	= 8,				// RPC's save checksums separately now
};

const uint32 FNetworkVersion::InternalProtocolVersion = HISTORY_REPLAY_RPC_CHECKSUMS;

uint32 FNetworkVersion::GetLocalNetworkVersion()
{
	if ( GetLocalNetworkVersionOverride.IsBound() )
	{
		const uint32 LocalNetworkVersion = GetLocalNetworkVersionOverride.Execute();

		UE_LOG( LogNet, Log, TEXT( "GetLocalNetworkVersionOverride: LocalNetworkVersion: %u" ), LocalNetworkVersion );

		return LocalNetworkVersion;
	}

	// Get the project name (NOT case sensitive)
	const FString ProjectName( FString( FApp::GetGameName() ).ToLower() );

	// Get the project version string (IS case sensitive!)
	const FString& ProjectVersion = Cast<UGeneralProjectSettings>(UGeneralProjectSettings::StaticClass()->GetDefaultObject())->ProjectVersion;

	// Start with engine version as seed, and then hash with project name + project version
	const uint32 VersionHash = FCrc::StrCrc32( *ProjectVersion, FCrc::StrCrc32( *ProjectName, GEngineNetVersion ) );

	// Hash with internal protocol version
	uint32 LocalNetworkVersion = FCrc::MemCrc32( &InternalProtocolVersion, sizeof( InternalProtocolVersion ), VersionHash );

#if 0//!(UE_BUILD_SHIPPING || UE_BUILD_TEST)	// DISABLED FOR NOW, MESSES UP COPIED BUILDS
	if ( !GEngineVersion.IsPromotedBuild() )
	{
		// Further hash with machine id if this is a non promoted build
		const FString MachineId = FPlatformMisc::GetMachineId().ToString( EGuidFormats::Digits ).ToLower();

		const uint32 LocalNetworkVersionNonPromoted = FCrc::StrCrc32( *MachineId, LocalNetworkVersion );

		UE_LOG( LogNet, Log, TEXT( "GetLocalNetworkVersion: NON-PROMOTED: MachineId: %s, ProjectName: %s, ProjectVersion: %s, InternalProtocolVersion: %i, LocalNetworkVersionNonPromoted: %u" ), *MachineId, *ProjectName, *ProjectVersion, InternalProtocolVersion, LocalNetworkVersionNonPromoted );

		return LocalNetworkVersionNonPromoted;
	}
#endif

	UE_LOG( LogNet, Log, TEXT( "GetLocalNetworkVersion: GEngineNetVersion: %i, ProjectName: %s, ProjectVersion: %s, InternalProtocolVersion: %i, LocalNetworkVersion: %u" ), GEngineNetVersion, *ProjectName, *ProjectVersion, InternalProtocolVersion, LocalNetworkVersion );

	return LocalNetworkVersion;
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
	return FNetworkReplayVersion( FApp::GetGameName(), GetLocalNetworkVersion(), GEngineVersion.GetChangelist() );
}


// ----------------------------------------------------------------
