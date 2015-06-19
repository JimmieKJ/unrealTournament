// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "Net/UnrealNetwork.h"
#include "Net/NetworkProfiler.h"
#include "Net/DataChannel.h"
#include "Net/DataReplication.h"
#include "Engine/ActorChannel.h"
#include "RepLayout.h"
#include "Engine/PackageMapClient.h"

// ( OutPacketId == GUID_PACKET_NOT_ACKED ) == NAK'd		(this GUID is not acked, and is not pending either, so sort of waiting)
// ( OutPacketId == GUID_PACKET_ACKED )		== FULLY ACK'd	(this GUID is fully acked, and we no longer need to send full path)
// ( OutPacketId > GUID_PACKET_ACKED )		== PENDING		(this GUID needs to be acked, it has been recently reference, and path was sent)

static const int GUID_PACKET_NOT_ACKED	= -2;		
static const int GUID_PACKET_ACKED		= -1;		

/**
 * Don't allow infinite recursion of InternalLoadObject - an attacker could
 * send malicious packets that cause a stack overflow on the server.
 */
static const int INTERNAL_LOAD_OBJECT_RECURSION_LIMIT = 16;

static TAutoConsoleVariable<int32> CVarAllowAsyncLoading( TEXT( "net.AllowAsyncLoading" ), 0, TEXT( "Allow async loading" ) );
static TAutoConsoleVariable<int32> CVarIgnorePackageMismatch( TEXT( "net.IgnorePackageMismatch" ), 0, TEXT( "Ignore when package versions are different" ) );

/*-----------------------------------------------------------------------------
	UPackageMapClient implementation.
-----------------------------------------------------------------------------*/
UPackageMapClient::UPackageMapClient(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
  , Connection(nullptr)
{
}

static uint32 GetPackageChecksum( const UPackage* Package )
{
	const FGuid Guid = Package->GetGuid();

	return FCrc::MemCrc32( &Guid, sizeof( FGuid ) );
}

/**
 *	This is the meat of the PackageMap class which serializes a reference to Object.
 */
bool UPackageMapClient::SerializeObject( FArchive& Ar, UClass* Class, UObject*& Object, FNetworkGUID *OutNetGUID)
{
	SCOPE_CYCLE_COUNTER(STAT_PackageMap_SerializeObjectTime);

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	static IConsoleVariable* DebugObjectCvar = IConsoleManager::Get().FindConsoleVariable(TEXT("net.PackageMap.DebugObject"));
	if (DebugObjectCvar && !DebugObjectCvar->GetString().IsEmpty() && Object && Object->GetName().Contains(DebugObjectCvar->GetString()))
	{
		UE_LOG(LogNetPackageMap, Log, TEXT("Serialized Object %s"), *Object->GetName());
	}
#endif

	if (Ar.IsSaving())
	{
		// If pending kill, just serialize as NULL.
		// TWeakObjectPtrs of PendingKill objects will behave strangely with TSets and TMaps
		//	PendingKill objects will collide with each other and with NULL objects in those data structures.
		if (Object && Object->IsPendingKill())
		{
			UObject* NullObj = NULL;
			return SerializeObject( Ar, Class, NullObj, OutNetGUID);
		}

		FNetworkGUID NetGUID = GuidCache->GetOrAssignNetGUID( Object );

		// Write out NetGUID to caller if necessary
		if (OutNetGUID)
		{
			*OutNetGUID = NetGUID;
		}

		// Write object NetGUID to the given FArchive
		InternalWriteObject( Ar, NetGUID, Object, TEXT( "" ), NULL );

		// If we need to export this GUID (its new or hasnt been ACKd, do so here)
		if (!NetGUID.IsDefault() && ShouldSendFullPath(Object, NetGUID))
		{
			check(IsNetGUIDAuthority());
			if ( !ExportNetGUID( NetGUID, Object, TEXT(""), NULL ) )
			{
				UE_LOG( LogNetPackageMap, Verbose, TEXT( "Failed to export in ::SerializeObject %s"), *Object->GetName() );
			}
		}

		return true;
	}
	else if (Ar.IsLoading())
	{
		// ----------------	
		// Read NetGUID from stream and resolve object
		// ----------------	
		FNetworkGUID NetGUID = InternalLoadObject( Ar, Object, 0 );

		// Write out NetGUID to caller if necessary
		if ( OutNetGUID )
		{
			*OutNetGUID = NetGUID;
		}	

		// ----------------	
		// Final Checks/verification
		// ----------------	

		// NULL if we haven't finished loading the objects level yet
		if ( !ObjectLevelHasFinishedLoading( Object ) )
		{
			UE_LOG(LogNetPackageMap, Warning, TEXT("Using None instead of replicated reference to %s because the level it's in has not been made visible"), *Object->GetFullName());
			Object = NULL;
		}

		// Check that we got the right class
		if ( Object && !Object->IsA( Class ) )
		{
			UE_LOG(LogNetPackageMap, Warning, TEXT("Forged object: got %s, expecting %s"),*Object->GetFullName(),*Class->GetFullName());
			Object = NULL;
		}

		if ( NetGUID.IsValid() && Object == NULL && bShouldTrackUnmappedGuids )
		{
			TrackedUnmappedNetGuids.AddUnique( NetGUID );
		}

		UE_CLOG(!bSuppressLogs, LogNetPackageMap, Log, TEXT("UPackageMapClient::SerializeObject Serialized Object %s as <%s>"), Object ? *Object->GetPathName() : TEXT("NULL"), *NetGUID.ToString() );

		// reference is mapped if it was not NULL (or was explicitly null)
		return (Object != NULL || !NetGUID.IsValid());
	}

	return true;
}

/**
 *	Slimmed down version of SerializeObject, that writes an object reference given a NetGUID and name
 *	(e.g, it does not require the actor to actually exist anymore to serialize the reference).
 *	This must be kept in sync with UPackageMapClient::SerializeObject.
 */
bool UPackageMapClient::WriteObject( FArchive& Ar, UObject* ObjOuter, FNetworkGUID NetGUID, FString ObjName )
{
	Ar << NetGUID;
	NET_CHECKSUM(Ar);

	UE_LOG(LogNetPackageMap, Log, TEXT("WroteObject %s NetGUID <%s>"), *ObjName, *NetGUID.ToString() );

	if (NetGUID.IsStatic() && !NetGUID.IsDefault() && !NetGUIDHasBeenAckd(NetGUID))
	{
		if ( !ExportNetGUID( NetGUID, NULL, ObjName, ObjOuter ) )
		{
			UE_LOG( LogNetPackageMap, Warning, TEXT( "Failed to export in ::WriteObject %s" ), *ObjName );
		}
	}

	return true;
}

/**
 *	Standard method of serializing a new actor.
 *		For static actors, this will just be a single call to SerializeObject, since they can be referenced by their path name.
 *		For dynamic actors, first the actor's reference is serialized but will not resolve on clients since they haven't spawned the actor yet.
 *			The actor archetype is then serialized along with the starting location, rotation, and velocity.
 *			After reading this information, the client spawns this actor via GWorld and assigns it the NetGUID it read at the top of the function.
 *
 *		returns true if a new actor was spawned. false means an existing actor was found for the netguid.
 */
bool UPackageMapClient::SerializeNewActor(FArchive& Ar, class UActorChannel *Channel, class AActor*& Actor)
{
	UE_LOG( LogNetPackageMap, VeryVerbose, TEXT( "SerializeNewActor START" ) );

	uint8 bIsClosingChannel = 0;

	if (Ar.IsLoading() )
	{
		FInBunch* InBunch = (FInBunch*)&Ar;
		bIsClosingChannel = InBunch->bClose;		// This is so we can determine that this channel was opened/closed for destruction
		UE_LOG(LogNetPackageMap, Log, TEXT("UPackageMapClient::SerializeNewActor BitPos: %d"), InBunch->GetPosBits() );
	}

	NET_CHECKSUM(Ar);

	FNetworkGUID NetGUID;
	UObject *NewObj = Actor;
	SerializeObject(Ar, AActor::StaticClass(), NewObj, &NetGUID);

	if ( Ar.IsError() )
	{
		UE_LOG( LogNetPackageMap, Error, TEXT( "UPackageMapClient::SerializeNewActor: Ar.IsError after SerializeObject 1" ) );
		return false;
	}

	Actor = Cast<AActor>(NewObj);

	// When we return an actor, we don't necessarily always spawn it (we might have found it already in memory)
	// The calling code may want to know, so this is why we distinguish
	bool bActorWasSpawned = false;

	if ( Ar.AtEnd() && NetGUID.IsDynamic() )
	{
		// This must be a destruction info coming through or something is wrong
		// If so, we should be both closing the channel and we should find the actor
		// This can happen when dormant actors that don't have channels get destroyed
		if ( bIsClosingChannel == 0 || Actor == NULL )
		{
			UE_LOG( LogNetPackageMap, Error, TEXT( "UPackageMapClient::SerializeNewActor: bIsClosingChannel == 0 || Actor == NULL : %s" ), Actor ? *Actor->GetName() : TEXT( "NULL" ) );
			Ar.SetError();
			return false;
		}

		UE_LOG( LogNetPackageMap, Log, TEXT( "UPackageMapClient::SerializeNewActor:  Skipping full read because we are deleting dynamic actor: %s" ), Actor ? *Actor->GetName() : TEXT( "NULL" ) );
		return false;		// This doesn't mean an error. This just simply means we didn't spawn an actor.
	}

	if ( NetGUID.IsDynamic() )
	{
		UObject* Archetype = NULL;
		FVector_NetQuantize10 Location;
		FVector_NetQuantize10 Velocity;
		FRotator Rotation;
		bool SerSuccess;

		if (Ar.IsSaving())
		{
			Archetype = Actor->GetArchetype();

			check( Archetype != NULL );
			check( Actor->NeedsLoadForClient() );			// We have no business sending this unless the client can load
			check( Archetype->NeedsLoadForClient() );		// We have no business sending this unless the client can load

			Location = Actor->GetRootComponent() ? Actor->GetActorLocation() : FVector::ZeroVector;
			Rotation = Actor->GetRootComponent() ? Actor->GetActorRotation() : FRotator::ZeroRotator;
			Velocity = Actor->GetVelocity();
		}

		FNetworkGUID ArchetypeNetGUID;
		SerializeObject( Ar, UObject::StaticClass(), Archetype, &ArchetypeNetGUID );

		if ( ArchetypeNetGUID.IsValid() && Archetype == NULL )
		{
			const FNetGuidCacheObject* ExistingCacheObjectPtr = GuidCache->ObjectLookup.Find( ArchetypeNetGUID );

			if ( ExistingCacheObjectPtr != NULL )
			{
				UE_LOG( LogNetPackageMap, Error, TEXT( "UPackageMapClient::SerializeNewActor. Unresolved Archetype GUID. Path: %s, NetGUID: %s." ), *ExistingCacheObjectPtr->PathName.ToString(), *ArchetypeNetGUID.ToString() );
			}
			else
			{
				UE_LOG( LogNetPackageMap, Error, TEXT( "UPackageMapClient::SerializeNewActor. Unresolved Archetype GUID. Guid not registered! NetGUID: %s." ), *ArchetypeNetGUID.ToString() );
			}
		}

		// SerializeCompressedInitial
		{			
			// read/write compressed location
			Location.NetSerialize( Ar, this, SerSuccess );
			Rotation.NetSerialize( Ar, this, SerSuccess );
			Velocity.NetSerialize( Ar, this, SerSuccess );

			if ( Channel && Ar.IsSaving() )
			{
				FObjectReplicator * RepData = &Channel->GetActorReplicationData();
				uint8* Recent = RepData && RepData->RepState != NULL && RepData->RepState->StaticBuffer.Num() ? RepData->RepState->StaticBuffer.GetData() : NULL;
				if ( Recent )
				{
					((AActor*)Recent)->ReplicatedMovement.Location = Location;
					((AActor*)Recent)->ReplicatedMovement.Rotation = Rotation;
					((AActor*)Recent)->ReplicatedMovement.LinearVelocity = Velocity;
				}
			}
		}

		if ( Ar.IsLoading() )
		{
			// Spawn actor if necessary (we may have already found it if it was dormant)
			if ( Actor == NULL )
			{
				if ( Archetype )
				{
					FActorSpawnParameters SpawnInfo;
					SpawnInfo.Template = Cast<AActor>(Archetype);
					SpawnInfo.bNoCollisionFail = true;
					SpawnInfo.bRemoteOwned = true;
					SpawnInfo.bNoFail = true;
					Actor = Connection->Driver->GetWorld()->SpawnActor(Archetype->GetClass(), &Location, &Rotation, SpawnInfo );
					Actor->PostNetReceiveVelocity(Velocity);

					GuidCache->RegisterNetGUID_Client( NetGUID, Actor );
					bActorWasSpawned = true;
				}
				else
				{
					UE_LOG(LogNetPackageMap, Error, TEXT("UPackageMapClient::SerializeNewActor Unable to read Archetype for NetGUID %s / %s"), *NetGUID.ToString(), *ArchetypeNetGUID.ToString() );
				}
			}
		}
	}
	else if ( Ar.IsLoading() && Actor == NULL )
	{
		UE_LOG( LogNetPackageMap, Warning, TEXT( "SerializeNewActor: Static actor failed to load: FullNetGuidPath: %s, Channel: %d" ), *GuidCache->FullNetGUIDPath( NetGUID ), Channel->ChIndex );
	}

	UE_LOG( LogNetPackageMap, Log, TEXT( "SerializeNewActor END: Finished Serializing. Actor: %s, FullNetGUIDPath: %s, Channel: %d, IsLoading: %i, IsDynamic: %i" ), Actor ? *Actor->GetName() : TEXT("NULL"), *GuidCache->FullNetGUIDPath( NetGUID ), Channel->ChIndex, (int)Ar.IsLoading(), (int)NetGUID.IsDynamic() );

	return bActorWasSpawned;
}

//--------------------------------------------------------------------
//
//	Writing
//
//--------------------------------------------------------------------

struct FExportFlags
{
	union
	{
		struct
		{
			uint8 bHasPath				: 1;
			uint8 bNoLoad				: 1;
			uint8 bHasNetworkChecksum	: 1;
		};

		uint8	Value;
	};

	FExportFlags()
	{
		Value = 0;
	}
};

static bool CanClientLoadObject( const UObject* Object, const FNetworkGUID& NetGUID )
{
	if ( !NetGUID.IsValid() || NetGUID.IsDynamic() )
	{
		return false;		// We should never tell the client to load dynamic objects (actors or objects created during play for example)
	}

	// PackageMapClient can't load maps, we must wait for the client to load the map when ready
	// These guids are special guids, where the guid and all child guids resolve once the map has been loaded
	if ( Object != NULL && Object->GetOutermost()->ContainsMap() )
	{
		return false;
	}

	// We can load everything else
	return true;
}

/** Writes an object NetGUID given the NetGUID and either the object itself, or FString full name of the object. Appends full name/path if necessary */
void UPackageMapClient::InternalWriteObject( FArchive & Ar, FNetworkGUID NetGUID, const UObject* Object, FString ObjectPathName, UObject* ObjectOuter )
{
	check( Ar.IsSaving() );

	const bool bNoLoad = !CanClientLoadObject( Object, NetGUID );

	if ( CVarAllowAsyncLoading.GetValueOnGameThread() > 0 && IsNetGUIDAuthority() && !GuidCache->IsExportingNetGUIDBunch && !bNoLoad )
	{
		// These are guids that must exist on the client in a package
		// The client needs to know about these so it can determine if it has finished loading them
		// and pause the network stream for that channel if it hasn't
		MustBeMappedGuidsInLastBunch.AddUnique( NetGUID );
	}

	Ar << NetGUID;
	NET_CHECKSUM( Ar );

	if ( !NetGUID.IsValid() )
	{
		// We're done writing
		return;
	}

	// Write export flags
	//   note: Default NetGUID is implied to always send path
	FExportFlags ExportFlags;

	ExportFlags.bHasNetworkChecksum = GuidCache->ShouldUseNetworkChecksum() ? 1 : 0;

	if ( NetGUID.IsDefault() )
	{
		// Only the client sends default guids
		check( !IsNetGUIDAuthority() );
		ExportFlags.bHasPath = 1;
	}
	else if ( GuidCache->IsExportingNetGUIDBunch )
	{
		// Only the server should be exporting guids
		check( IsNetGUIDAuthority() );

		if ( Object != NULL )
		{
			ExportFlags.bHasPath = ShouldSendFullPath( Object, NetGUID ) ? 1 : 0;
		}
		else
		{
			ExportFlags.bHasPath = ObjectPathName.IsEmpty() ? 0 : 1;
		}

		ExportFlags.bNoLoad	= bNoLoad ? 1 : 0;

		Ar << ExportFlags.Value;
	}

	if ( ExportFlags.bHasPath )
	{
		if ( Object != NULL )
		{
			// If the object isn't NULL, expect an empty path name, then fill it out with the actual info
			check( ObjectOuter == NULL );
			check( ObjectPathName.IsEmpty() );
			ObjectPathName = Object->GetName();
			ObjectOuter = Object->GetOuter();
		}
		else
		{
			// If we don't have an object, expect an already filled out path name
			check( ObjectOuter != NULL );
			check( !ObjectPathName.IsEmpty() );
		}

		const bool bIsPackage = ( NetGUID.IsStatic() && Object != NULL && Object->GetOuter() == NULL );

		check( bIsPackage == ( Cast< UPackage >( Object ) != NULL ) );		// Make sure it really is a package

		// Serialize reference to outer. This is basically a form of compression.
		FNetworkGUID OuterNetGUID = GuidCache->GetOrAssignNetGUID( ObjectOuter );

		InternalWriteObject( Ar, OuterNetGUID, ObjectOuter, TEXT( "" ), NULL );

		GEngine->NetworkRemapPath(Connection->Driver->GetWorld(), ObjectPathName, false);

		// Serialize Name of object
		Ar << ObjectPathName;

		uint32 NetworkChecksum = 0;
		uint32 PackageChecksum = 0;

		if ( ExportFlags.bHasNetworkChecksum )
		{
			NetworkChecksum = GuidCache->GetNetworkChecksum( Object );
			Ar << NetworkChecksum;
		}

		if ( bIsPackage )
		{
			PackageChecksum = GetPackageChecksum( CastChecked< const UPackage >( Object ) );
			Ar << PackageChecksum;
			UE_LOG( LogNetPackageMap, VeryVerbose, TEXT( "InternalWriteObject: Package: %s, GUID: %u" ), *ObjectPathName, PackageChecksum );
		}

		FNetGuidCacheObject* CacheObject = GuidCache->ObjectLookup.Find( NetGUID );

		if ( CacheObject != NULL )
		{
			CacheObject->PathName			= FName( *ObjectPathName );
			CacheObject->OuterGUID			= OuterNetGUID;
			CacheObject->bNoLoad			= ExportFlags.bNoLoad;
			CacheObject->bIgnoreWhenMissing = ExportFlags.bNoLoad;
			CacheObject->NetworkChecksum	= NetworkChecksum;
			CacheObject->PackageChecksum	= PackageChecksum;
		}

		if ( GuidCache->IsExportingNetGUIDBunch )
		{
			CurrentExportNetGUIDs.Add( NetGUID );

			int32& Count = NetGUIDExportCountMap.FindOrAdd( NetGUID );
			Count++;
		}
	}
}

//--------------------------------------------------------------------
//
//	Loading
//
//--------------------------------------------------------------------

static void SanityCheckExport( 
	const FNetGUIDCache *	GuidCache,
	const UObject *			Object, 
	const FNetworkGUID &	NetGUID, 
	const FString &			ExpectedPathName, 
	const uint32			ExpectedPackageChecksum,
	const UObject *			ExpectedOuter, 
	const FNetworkGUID &	ExpectedOuterGUID,
	const FExportFlags &	ExportFlags )
{
	
	check( GuidCache != NULL );
	check( Object != NULL );

	const FNetGuidCacheObject* CacheObject = GuidCache->ObjectLookup.Find( NetGUID );

	if ( CacheObject != NULL )
	{
		if ( CacheObject->OuterGUID != ExpectedOuterGUID )
		{
			UE_LOG( LogNetPackageMap, Warning, TEXT( "SanityCheckExport: CacheObject->OuterGUID != ExpectedOuterGUID. NetGUID: %s, Object: %s, Expected: %s" ), *NetGUID.ToString(), *Object->GetPathName(), *ExpectedPathName );
		}

		if ( CacheObject->PackageChecksum != ExpectedPackageChecksum )
		{
			UE_LOG( LogNetPackageMap, Warning, TEXT( "SanityCheckExport: CacheObject->PackageChecksum != ExpectedPackageChecksum. NetGUID: %s, Object: %s, Expected: %s" ), *NetGUID.ToString(), *Object->GetPathName(), *ExpectedPathName );
		}
	}
	else
	{
		UE_LOG( LogNetPackageMap, Warning, TEXT( "SanityCheckExport: CacheObject == NULL. NetGUID: %s, Object: %s, Expected: %s" ), *NetGUID.ToString(), *Object->GetPathName(), *ExpectedPathName );
	}

	if ( Object->GetName() != ExpectedPathName )
	{
		UE_LOG( LogNetPackageMap, Warning, TEXT( "SanityCheckExport: Name mismatch. NetGUID: %s, Object: %s, Expected: %s" ), *NetGUID.ToString(), *Object->GetPathName(), *ExpectedPathName );
	}

	if ( Object->GetOuter() != ExpectedOuter )
	{
		const FString CurrentOuterName	= Object->GetOuter() != NULL ? *Object->GetOuter()->GetName() : TEXT( "NULL" );
		const FString ExpectedOuterName = ExpectedOuter != NULL ? *ExpectedOuter->GetName() : TEXT( "NULL" );
		UE_LOG( LogNetPackageMap, Warning, TEXT( "SanityCheckExport: Outer mismatch. Object: %s, NetGUID: %s, Current: %s, Expected: %s" ), *Object->GetPathName(), *NetGUID.ToString(), *CurrentOuterName, *ExpectedOuterName );
	}

	const bool bIsPackage = ( NetGUID.IsStatic() && Object->GetOuter() == NULL );
	const UPackage* Package = Cast< const UPackage >( Object );

	if ( bIsPackage != ( Package != NULL ) )
	{
		UE_LOG( LogNetPackageMap, Warning, TEXT( "SanityCheckExport: Package type mismatch. Object:%s, NetGUID: %s" ), *Object->GetPathName(), *NetGUID.ToString() );
	}
}

/** Loads a UObject from an FArchive stream. Reads object path if there, and tries to load object if its not already loaded */
FNetworkGUID UPackageMapClient::InternalLoadObject( FArchive & Ar, UObject *& Object, const int InternalLoadObjectRecursionCount )
{
	if ( InternalLoadObjectRecursionCount > INTERNAL_LOAD_OBJECT_RECURSION_LIMIT ) 
	{
		UE_LOG( LogNetPackageMap, Warning, TEXT( "InternalLoadObject: Hit recursion limit." ) );
		Ar.SetError(); 
		Object = NULL;
		return FNetworkGUID(); 
	} 

	// ----------------	
	// Read the NetGUID
	// ----------------	
	FNetworkGUID NetGUID;
	Ar << NetGUID;
	NET_CHECKSUM_OR_END( Ar );

	if ( Ar.IsError() )
	{
		Object = NULL;
		return NetGUID;
	}

	if ( !NetGUID.IsValid() )
	{
		Object = NULL;
		return NetGUID;
	}

	// ----------------	
	// Try to resolve NetGUID
	// ----------------	
	if ( NetGUID.IsValid() && !NetGUID.IsDefault() )
	{
		Object = GetObjectFromNetGUID( NetGUID, GuidCache->IsExportingNetGUIDBunch );

		UE_CLOG( !bSuppressLogs, LogNetPackageMap, Log, TEXT( "InternalLoadObject loaded %s from NetGUID <%s>" ), Object ? *Object->GetFullName() : TEXT( "NULL" ), *NetGUID.ToString() );
	}

	// ----------------	
	// Read the full if its there
	// ----------------	
	FExportFlags ExportFlags;

	if ( NetGUID.IsDefault() )
	{
		ExportFlags.bHasPath = 1;
	}
	else if ( GuidCache->IsExportingNetGUIDBunch )
	{
		Ar << ExportFlags.Value;

		if ( Ar.IsError() )
		{
			Object = NULL;
			return NetGUID;
		}
	}

	if ( ExportFlags.bHasPath )
	{
		UObject* ObjOuter = NULL;

		FNetworkGUID OuterGUID = InternalLoadObject( Ar, ObjOuter, InternalLoadObjectRecursionCount + 1 );

		FString PathName;
		uint32	NetworkChecksum = 0;
		uint32	PackageChecksum = 0;

		Ar << PathName;

		if ( ExportFlags.bHasNetworkChecksum )
		{
			Ar << NetworkChecksum;
		}

		const bool bIsPackage = NetGUID.IsStatic() && !OuterGUID.IsValid();

		if ( bIsPackage )
		{
			Ar << PackageChecksum;
			UE_LOG( LogNetPackageMap, VeryVerbose, TEXT( "InternalLoadObject: Package: %s, GUID: %u" ), *PathName, PackageChecksum );
		}

		if ( Ar.IsError() )
		{
			UE_LOG( LogNetPackageMap, Error, TEXT( "InternalLoadObject: Failed to load path name" ) );
			Object = NULL;
			return NetGUID;
		}

		// Remap name for PIE
		GEngine->NetworkRemapPath( Connection->Driver->GetWorld(), PathName, true );

		if ( Object != NULL )
		{
			// If we already have the object, just do some sanity checking and return
			SanityCheckExport( GuidCache.Get(), Object, NetGUID, PathName, PackageChecksum, ObjOuter, OuterGUID, ExportFlags );
			return NetGUID;
		}

		if ( NetGUID.IsDefault() )
		{
			// This should be from the client
			// If we get here, we want to go ahead and assign a network guid, 
			// then export that to the client at the next available opportunity
			check( IsNetGUIDAuthority() );

			Object = StaticFindObject( UObject::StaticClass(), ObjOuter, *PathName, false );

			if ( Object == NULL )
			{
				UE_LOG( LogNetPackageMap, Warning, TEXT( "UPackageMapClient::InternalLoadObject: Unable to resolve default guid from client: PathName: %s, ObjOuter: %s " ), *PathName, ObjOuter != NULL ? *ObjOuter->GetPathName() : TEXT( "NULL" ) );
				return NetGUID;
			}

			if (Object->IsPendingKill())
			{
				UE_LOG( LogNetPackageMap, Warning, TEXT( "UPackageMapClient::InternalLoadObject: Received reference to pending kill object from client: PathName: %s, ObjOuter: %s "), *PathName, ObjOuter != NULL ? *ObjOuter->GetPathName() : TEXT( "NULL" ) );
				Object = NULL;
				return NetGUID;
			}

			if ( bIsPackage )
			{
				UPackage * Package = Cast< UPackage >( Object );

				if ( Package == NULL )
				{
					UE_LOG( LogNetPackageMap, Error, TEXT( "UPackageMapClient::InternalLoadObject: Default object not a package from client: PathName: %s, ObjOuter: %s " ), *PathName, ObjOuter != NULL ? *ObjOuter->GetPathName() : TEXT( "NULL" ) );
					Object = NULL;
					return NetGUID;
				}

				if ( !GuidCache->ShouldIgnorePackageMismatch() && GetPackageChecksum( Package ) != PackageChecksum )
				{
					UE_LOG( LogNetPackageMap, Error, TEXT( "UPackageMapClient::InternalLoadObject: Default object package guid mismatch! PathName: %s, ObjOuter: %s, GUID1: %u, GUID2: %u " ), *PathName, ObjOuter != NULL ? *ObjOuter->GetPathName() : TEXT( "NULL" ), GetPackageChecksum( Package ), PackageChecksum );
					Object = NULL;
					return NetGUID;
				}
			}

			// Assign the guid to the object
			NetGUID = GuidCache->GetOrAssignNetGUID( Object );

			// Let this client know what guid we assigned
			HandleUnAssignedObject( Object );

			return NetGUID;
		}

		// If we are the server, we should have found the object by now
		if ( IsNetGUIDAuthority() )
		{
			UE_LOG( LogNetPackageMap, Warning, TEXT( "UPackageMapClient::InternalLoadObject: Server could not resolve non default guid from client. PathName: %s, ObjOuter: %s " ), *PathName, ObjOuter != NULL ? *ObjOuter->GetPathName() : TEXT( "NULL" ) );
			return NetGUID;
		}

		//
		// At this point, only the client gets this far
		//

		const bool bIgnoreWhenMissing = ExportFlags.bNoLoad;

		// Register this path and outer guid combo with the net guid
		GuidCache->RegisterNetGUIDFromPath_Client( NetGUID, PathName, OuterGUID, NetworkChecksum, PackageChecksum, ExportFlags.bNoLoad, bIgnoreWhenMissing );

		// Try again now that we've registered the path
		Object = GuidCache->GetObjectFromNetGUID( NetGUID, GuidCache->IsExportingNetGUIDBunch );

		if ( Object == NULL && !GuidCache->ShouldIgnoreWhenMissing( NetGUID ) )
		{
			UE_LOG( LogNetPackageMap, Warning, TEXT( "InternalLoadObject: Unable to resolve object from path. Path: %s, Outer: %s, NetGUID: %s" ), *PathName, ObjOuter ? *ObjOuter->GetPathName() : TEXT( "NULL" ), *NetGUID.ToString() );
		}
	}
	else if ( Object == NULL && !GuidCache->ShouldIgnoreWhenMissing( NetGUID ) )
	{
		UE_LOG( LogNetPackageMap, Warning, TEXT( "InternalLoadObject: Unable to resolve object. FullNetGUIDPath: %s" ), *GuidCache->FullNetGUIDPath( NetGUID ) );
	}

	return NetGUID;
}

UObject* UPackageMapClient::ResolvePathAndAssignNetGUID( const FNetworkGUID& NetGUID, const FString& PathName )
{
	check( 0 );
	return NULL;
}

//--------------------------------------------------------------------
//
//	Network - NetGUID Bunches (Export Table)
//
//	These functions deal with exporting new NetGUIDs in separate, discrete bunches.
//	These bunches are appended to normal 'content' bunches. You can think of it as an
//	export table that is prepended to bunches.
//
//--------------------------------------------------------------------

/** Exports the NetGUID and paths needed to the CurrentExportBunch */
bool UPackageMapClient::ExportNetGUID( FNetworkGUID NetGUID, const UObject* Object, FString PathName, UObject* ObjOuter )
{
	check( NetGUID.IsValid() );
	check( ( Object == NULL ) == !PathName.IsEmpty() );
	check( !NetGUID.IsDefault() );
	check( Object == NULL || ShouldSendFullPath( Object, NetGUID ) );

	// Two passes are used to export this net guid:
	// 1. Attempt to append this net guid to current bunch
	// 2. If step 1 fails, append to fresh new bunch
	for ( int32 NumTries = 0; NumTries < 2; NumTries++ )
	{
		if ( !CurrentExportBunch )
		{
			check( ExportNetGUIDCount == 0 );

			CurrentExportBunch = new FOutBunch(this, Connection->MaxPacket*8-MAX_BUNCH_HEADER_BITS-MAX_PACKET_TRAILER_BITS-MAX_PACKET_HEADER_BITS );
			CurrentExportBunch->SetAllowResize(false);
			CurrentExportBunch->bHasGUIDs = true;
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			CurrentExportBunch->DebugString = TEXT("NetGUIDs");
#endif

			ExportNetGUIDCount = 0;
			*CurrentExportBunch << ExportNetGUIDCount;
			NET_CHECKSUM( *CurrentExportBunch );
		}

		if ( CurrentExportNetGUIDs.Num() != 0 )
		{
			UE_LOG( LogNetPackageMap, Fatal, TEXT( "ExportNetGUID - CurrentExportNetGUIDs.Num() != 0 (%s)." ), Object ? *Object->GetName() : *PathName );
			return false;
		}

		// Push our current state in case we overflow with this export and have to pop it off.
		FBitWriterMark LastExportMark;
		LastExportMark.Init( *CurrentExportBunch );

		GuidCache->IsExportingNetGUIDBunch = true;

		InternalWriteObject( *CurrentExportBunch, NetGUID, Object, PathName, ObjOuter );

		GuidCache->IsExportingNetGUIDBunch = false;

		if ( CurrentExportNetGUIDs.Num() == 0 )
		{
			// Some how we failed to export this GUID 
			// This means no path names were written, which means we possibly are incorrectly not writing paths out, or we shouldn't be here in the first place
			UE_LOG( LogNetPackageMap, Warning, TEXT( "ExportNetGUID - InternalWriteObject no GUID's were exported: %s " ), Object ? *Object->GetName() : *PathName );
			LastExportMark.Pop( *CurrentExportBunch );
			return false;
		}
	
		if ( !CurrentExportBunch->IsError() )
		{
			// Success, append these exported guid's to the list going out on this bunch
			CurrentExportBunch->ExportNetGUIDs.Append( CurrentExportNetGUIDs.Array() );
			CurrentExportNetGUIDs.Empty();		// Done with this
			ExportNetGUIDCount++;
			return true;
		}

		// Overflowed, wrap up the currently pending bunch, and start a new one
		LastExportMark.Pop( *CurrentExportBunch );

		// Make sure we reset this so it doesn't persist into the next batch
		CurrentExportNetGUIDs.Empty();

		if ( ExportNetGUIDCount == 0 || NumTries == 1 )
		{
			// This means we couldn't serialize this NetGUID into a single bunch. The path could be ridiculously big (> ~512 bytes) or something else is very wrong
			UE_LOG( LogNetPackageMap, Fatal, TEXT( "ExportNetGUID - Failed to serialize NetGUID into single bunch. (%s)" ), Object ? *Object->GetName() : *PathName  );
			return false;
		}

		for ( auto It = CurrentExportNetGUIDs.CreateIterator(); It; ++It )
		{
			int32& Count = NetGUIDExportCountMap.FindOrAdd( *It );
			Count--;
		}

		// Export current bunch, create a new one, and try again.
		ExportNetGUIDHeader();
	}

	check( 0 );		// Shouldn't get here

	return false;
}

/** Called when an export bunch is finished. It writes how many NetGUIDs are contained in the bunch and finalizes the book keeping so we know what NetGUIDs are in the bunch */
void UPackageMapClient::ExportNetGUIDHeader()
{
	check(CurrentExportBunch);
	FOutBunch& Out = *CurrentExportBunch;

	UE_LOG(LogNetPackageMap, Log, TEXT("	UPackageMapClient::ExportNetGUID. Bytes: %d Bits: %d ExportNetGUIDCount: %d"), CurrentExportBunch->GetNumBytes(), CurrentExportBunch->GetNumBits(), ExportNetGUIDCount);

	// Rewrite how many NetGUIDs were exported.
	FBitWriterMark Reset;
	FBitWriterMark Restore(Out);
	Reset.PopWithoutClear(Out);
	Out << ExportNetGUIDCount;
	Restore.PopWithoutClear(Out);

	// If we've written new NetGUIDs to the 'bunch' set (current+1)
	if (UE_LOG_ACTIVE(LogNetPackageMap,Verbose))
	{
		UE_LOG(LogNetPackageMap, Verbose, TEXT("ExportNetGUIDHeader: "));
		for (auto It = CurrentExportBunch->ExportNetGUIDs.CreateIterator(); It; ++It)
		{
			UE_LOG(LogNetPackageMap, Verbose, TEXT("  NetGUID: %s"), *It->ToString());
		}
	}

	// CurrentExportBunch *should* always have NetGUIDs to export. If it doesn't warn. This is a bug.
	if ( CurrentExportBunch->ExportNetGUIDs.Num() != 0 )	
	{
		ExportBunches.Add( CurrentExportBunch );
	}
	else
	{
		UE_LOG(LogNetPackageMap, Warning, TEXT("Attempted to export a NetGUID Bunch with no NetGUIDs!"));
	}
	
	CurrentExportBunch = NULL;
	ExportNetGUIDCount = 0;
}

void UPackageMapClient::ReceiveNetGUIDBunch( FInBunch &InBunch )
{
	GuidCache->IsExportingNetGUIDBunch = true;
	check(InBunch.bHasGUIDs);

	int32 NumGUIDsInBunch = 0;
	InBunch << NumGUIDsInBunch;

	static const int32 MAX_GUID_COUNT = 2048;

	if ( NumGUIDsInBunch > MAX_GUID_COUNT )
	{
		UE_LOG( LogNetPackageMap, Error, TEXT( "UPackageMapClient::ReceiveNetGUIDBunch: NumGUIDsInBunch > MAX_GUID_COUNT (%i)" ), NumGUIDsInBunch );
		InBunch.SetError();
		GuidCache->IsExportingNetGUIDBunch = false;
		return;
	}

	NET_CHECKSUM(InBunch);

	UE_LOG(LogNetPackageMap, Log, TEXT("UPackageMapClient::ReceiveNetGUIDBunch %d NetGUIDs. PacketId %d. ChSequence %d. ChIndex %d"), NumGUIDsInBunch, InBunch.PacketId, InBunch.ChSequence, InBunch.ChIndex );

	int32 NumGUIDsRead = 0;
	while( NumGUIDsRead < NumGUIDsInBunch )
	{
		UObject* Obj = NULL;
		InternalLoadObject( InBunch, Obj, 0 );

		if ( InBunch.IsError() )
		{
			UE_LOG( LogNetPackageMap, Error, TEXT( "UPackageMapClient::ReceiveNetGUIDBunch: InBunch.IsError() after InternalLoadObject" ) );
			GuidCache->IsExportingNetGUIDBunch = false;
			return;
		}
		NumGUIDsRead++;
	}

	UE_LOG(LogNetPackageMap, Log, TEXT("UPackageMapClient::ReceiveNetGUIDBunch end. BitPos: %d"), InBunch.GetPosBits() );
	GuidCache->IsExportingNetGUIDBunch = false;
}

bool UPackageMapClient::AppendExportBunches(TArray<FOutBunch *>& OutgoingBunches)
{
	// Finish current in progress bunch if necessary
	if (ExportNetGUIDCount > 0)
	{
		ExportNetGUIDHeader();
	}

	// Let the profiler know about exported GUID bunches
	for (const FOutBunch* ExportBunch : ExportBunches )
	{
		if (ExportBunch != nullptr)
		{
			NETWORK_PROFILER(GNetworkProfiler.TrackExportBunch(ExportBunch->GetNumBits()));
		}
	}

	// Append the bunches we've made to the passed in list reference
	if (ExportBunches.Num() > 0)
	{
		if (UE_LOG_ACTIVE(LogNetPackageMap,Verbose))
		{
			UE_LOG(LogNetPackageMap, Verbose, TEXT("AppendExportBunches. ExportBunches: %d, ExportNetGUIDCount: %d"), ExportBunches.Num(), ExportNetGUIDCount);
			for (auto It=ExportBunches.CreateIterator(); It; ++It)
			{
				UE_LOG(LogNetPackageMap, Verbose, TEXT("   BunchIndex: %d, ExportNetGUIDs: %d, NumBytes: %d, NumBits: %d"), It.GetIndex(), (*It)->ExportNetGUIDs.Num(), (*It)->GetNumBytes(), (*It)->GetNumBits() );
			}
		}

		OutgoingBunches.Append(ExportBunches);
		ExportBunches.Empty();
		return true;
	}

	return false;
}

//--------------------------------------------------------------------
//
//	Network - ACKing
//
//--------------------------------------------------------------------

/**
 *	Called when a bunch is committed to the connection's Out buffer.
 *	ExportNetGUIDs is the list of GUIDs stored on the bunch that we use to update the expected sequence for those exported GUIDs
 */
void UPackageMapClient::NotifyBunchCommit( const int32 OutPacketId, const TArray< FNetworkGUID > & ExportNetGUIDs )
{
	check( OutPacketId > GUID_PACKET_ACKED );	// Assumptions break if this isn't true ( We assume ( OutPacketId > GUID_PACKET_ACKED ) == PENDING )
	check( ExportNetGUIDs.Num() > 0 );			// We should have never created this bunch if there are not exports

	for ( int32 i = 0; i < ExportNetGUIDs.Num(); i++ )
	{
		if ( !NetGUIDAckStatus.Contains( ExportNetGUIDs[i] ) )
		{
			NetGUIDAckStatus.Add( ExportNetGUIDs[i], GUID_PACKET_NOT_ACKED );
		}

		int32& ExpectedPacketIdRef = NetGUIDAckStatus.FindChecked( ExportNetGUIDs[i] );

		// Only update expected sequence if this guid was previously nak'd
		// If we always update to the latest packet id, we risk prolonging the ack for no good reason
		// (GUID information doesn't change, so updating to the latest expected sequence is unnecessary)
		if ( ExpectedPacketIdRef == GUID_PACKET_NOT_ACKED )
		{
			if ( Connection->InternalAck )
			{
				// Auto ack now if the connection is 100% reliable
				ExpectedPacketIdRef = GUID_PACKET_ACKED;
				continue;
			}

			ExpectedPacketIdRef = OutPacketId;
			check( !PendingAckGUIDs.Contains( ExportNetGUIDs[i] ) );	// If we hit this assert, this means the lists are out of sync
			PendingAckGUIDs.AddUnique( ExportNetGUIDs[i] );
		}
	}
}

/**
 *	Called by the PackageMap's UConnection after a receiving an ack
 *	Updates the respective GUIDs that were acked by this packet
 */
void UPackageMapClient::ReceivedAck( const int32 AckPacketId )
{
	for ( int32 i = PendingAckGUIDs.Num() - 1; i >= 0; i-- )
	{
		int32& ExpectedPacketIdRef = NetGUIDAckStatus.FindChecked( PendingAckGUIDs[i] );

		check( ExpectedPacketIdRef > GUID_PACKET_ACKED );		// Make sure we really are pending, since we're on the list

		if ( ExpectedPacketIdRef > GUID_PACKET_ACKED && ExpectedPacketIdRef <= AckPacketId )
		{
			ExpectedPacketIdRef = GUID_PACKET_ACKED;	// Fully acked
			PendingAckGUIDs.RemoveAt( i );				// Remove from pending list, since we're now acked
		}
	}
}

/**
 *	Handles a NACK for given packet id. If this packet ID contained a NetGUID reference, we redirty the NetGUID by setting
 *	its entry in NetGUIDAckStatus to GUID_PACKET_NOT_ACKED.
 */
void UPackageMapClient::ReceivedNak( const int32 NakPacketId )
{
	for ( int32 i = PendingAckGUIDs.Num() - 1; i >= 0; i-- )
	{
		int32& ExpectedPacketIdRef = NetGUIDAckStatus.FindChecked( PendingAckGUIDs[i] );

		check( ExpectedPacketIdRef > GUID_PACKET_ACKED );		// Make sure we aren't acked, since we're on the list

		if ( ExpectedPacketIdRef == NakPacketId )
		{
			ExpectedPacketIdRef = GUID_PACKET_NOT_ACKED;
			// Remove from pending list since we're no longer pending
			// If we send another reference to this GUID, it will get added back to this list to hopefully get acked next time
			PendingAckGUIDs.RemoveAt( i );	
		}
	}
}

/**
 *	Returns true if this PackageMap's connection has ACK'd the given NetGUID.
 */
bool UPackageMapClient::NetGUIDHasBeenAckd(FNetworkGUID NetGUID)
{
	if (!NetGUID.IsValid())
	{
		// Invalid NetGUID == NULL obect, so is ack'd by default
		return true;
	}

	if (NetGUID.IsDefault())
	{
		// Default NetGUID is 'unassigned' but valid. It is never Ack'd
		return false;
	}

	if (!IsNetGUIDAuthority())
	{
		// We arent the ones assigning NetGUIDs, so yes this is fully ackd
		return true;
	}

	// If brand new, add it to map with GUID_PACKET_NOT_ACKED
	if ( !NetGUIDAckStatus.Contains( NetGUID ) )
	{
		NetGUIDAckStatus.Add( NetGUID, GUID_PACKET_NOT_ACKED );
	}

	int32& AckPacketId = NetGUIDAckStatus.FindChecked( NetGUID );	

	if ( AckPacketId == GUID_PACKET_ACKED )
	{
		// This GUID has been fully Ackd
		UE_LOG( LogNetPackageMap, Verbose, TEXT("NetGUID <%s> is fully ACKd (AckPacketId: %d <= Connection->OutAckPacketIdL %d) "), *NetGUID.ToString(), AckPacketId, Connection->OutAckPacketId );
		return true;
	}
	else if ( AckPacketId == GUID_PACKET_NOT_ACKED )
	{
		
	}

	return false;
}

/** Immediately export an Object's NetGUID. This will */
void UPackageMapClient::HandleUnAssignedObject(const UObject* Obj)
{
	check( Obj != NULL );

	FNetworkGUID NetGUID = GuidCache->GetOrAssignNetGUID( Obj );

	if ( !NetGUID.IsDefault() && ShouldSendFullPath( Obj, NetGUID ) )
	{
		if ( !ExportNetGUID( NetGUID, Obj, TEXT( "" ), NULL ) )
		{
			UE_LOG( LogNetPackageMap, Verbose, TEXT( "Failed to export in ::HandleUnAssignedObject %s" ), Obj ? *Obj->GetName() : TEXT("NULL") );
		}
	}
}

//--------------------------------------------------------------------
//
//	Misc
//
//--------------------------------------------------------------------

/** Do we need to include the full path of this object for the client to resolve it? */
bool UPackageMapClient::ShouldSendFullPath( const UObject* Object, const FNetworkGUID &NetGUID )
{
	if ( !Connection )
	{
		return false;
	}

	// NetGUID is already exported
	if ( CurrentExportBunch != NULL && CurrentExportBunch->ExportNetGUIDs.Contains( NetGUID ) )
	{
		return false;
	}

	if ( !NetGUID.IsValid() )
	{
		return false;
	}

	if ( !Object->IsNameStableForNetworking() )
	{
		check( !NetGUID.IsDefault() );
		check( NetGUID.IsDynamic() );
		return false;		// We only export objects that have stable names
	}

	if ( NetGUID.IsDefault() )
	{
		check( !IsNetGUIDAuthority() );
		check( Object->IsNameStableForNetworking() );
		return true;
	}

	return !NetGUIDHasBeenAckd( NetGUID );
}

/**
 *	Prints debug info about this package map's state
 */
void UPackageMapClient::LogDebugInfo( FOutputDevice & Ar )
{
	for ( auto It = GuidCache->NetGUIDLookup.CreateIterator(); It; ++It )
	{
		FNetworkGUID NetGUID = It.Value();

		FString Status = TEXT("Unused");
		if ( NetGUIDAckStatus.Contains( NetGUID ) )
		{
			const int32 PacketId = NetGUIDAckStatus.FindRef(NetGUID);
			if ( PacketId == GUID_PACKET_NOT_ACKED )
			{
				Status = TEXT("UnAckd");
			}
			else if ( PacketId == GUID_PACKET_ACKED )
			{
				Status = TEXT("Ackd");
			}
			else
			{
				Status = TEXT("Pending");
			}
		}

		UObject *Obj = It.Key().Get();
		FString Str = FString::Printf(TEXT("%s [%s] [%s] - %s"), *NetGUID.ToString(), *Status, NetGUID.IsDynamic() ? TEXT("Dynamic") : TEXT("Static") , Obj ? *Obj->GetPathName() : TEXT("NULL"));
		Ar.Logf(*Str);
		UE_LOG(LogNetPackageMap, Log, TEXT("%s"), *Str);
	}
}

/**
 *	Returns true if Object's outer level has completely finished loading.
 */
bool UPackageMapClient::ObjectLevelHasFinishedLoading(UObject* Object)
{
	if (Object != NULL && Connection!= NULL && Connection->Driver != NULL && Connection->Driver->GetWorld() != NULL)
	{
		// get the level for the object
		ULevel* Level = NULL;
		for (UObject* Obj = Object; Obj != NULL; Obj = Obj->GetOuter())
		{
			Level = Cast<ULevel>(Obj);
			if (Level != NULL)
			{
				break;
			}
		}
		
		if (Level != NULL && Level != Connection->Driver->GetWorld()->PersistentLevel)
		{
			return Level->bIsVisible;
		}
	}

	return true;
}

/**
 * Return false if our connection is the netdriver's server connection
 *  This is ugly but probably better than adding a shadow variable that has to be
 *  set/cleared at the net driver level.
 */
bool UPackageMapClient::IsNetGUIDAuthority() const
{
	return GuidCache->IsNetGUIDAuthority();
}

/**	
 *	Returns stats for NetGUID usage
 */
void UPackageMapClient::GetNetGUIDStats(int32 &AckCount, int32 &UnAckCount, int32 &PendingCount)
{
	AckCount = UnAckCount = PendingCount = 0;
	for ( auto It = NetGUIDAckStatus.CreateIterator(); It; ++It )
	{
		// Sanity check that we're in sync
		check( ( It.Value() > GUID_PACKET_ACKED ) == PendingAckGUIDs.Contains( It.Key() ) );

		if ( It.Value() == GUID_PACKET_NOT_ACKED )
		{
			UnAckCount++;
		}
		else if ( It.Value() == GUID_PACKET_ACKED )
		{
			AckCount++;
		}
		else
		{
			PendingCount++;
		}
	}

	// Sanity check that we're in sync
	check( PendingAckGUIDs.Num() == PendingCount );
}

void UPackageMapClient::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	return Super::AddReferencedObjects(InThis, Collector);
}

void UPackageMapClient::NotifyStreamingLevelUnload(UObject* UnloadedLevel)
{
}

bool UPackageMapClient::PrintExportBatch()
{
	if ( ExportNetGUIDCount <= 0 && CurrentExportBunch == NULL )
	{
		return false;
	}

	// Print the whole thing for reference
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	for (auto It = GuidCache->History.CreateIterator(); It; ++It)
	{
		FString Str = It.Value();
		FNetworkGUID NetGUID = It.Key();
		UE_LOG(LogNetPackageMap, Warning, TEXT("<%s> - %s"), *NetGUID.ToString(), *Str);
	}
#endif

	UE_LOG(LogNetPackageMap, Warning, TEXT("\n\n"));
	if ( CurrentExportBunch != NULL )
	{
		for (auto It = CurrentExportBunch->ExportNetGUIDs.CreateIterator(); It; ++It)
		{
			UE_LOG(LogNetPackageMap, Warning, TEXT("  CurrentExportBunch->ExportNetGUIDs: %s"), *It->ToString());
		}
	}

	UE_LOG(LogNetPackageMap, Warning, TEXT("\n"));
	for (auto It = CurrentExportNetGUIDs.CreateIterator(); It; ++It)
	{
		UE_LOG(LogNetPackageMap, Warning, TEXT("  CurrentExportNetGUIDs: %s"), *It->ToString());
	}

	return true;
}

UObject* UPackageMapClient::GetObjectFromNetGUID( const FNetworkGUID& NetGUID, const bool bIgnoreMustBeMapped )
{
	return GuidCache->GetObjectFromNetGUID( NetGUID, bIgnoreMustBeMapped );
}

//----------------------------------------------------------------------------------------
//	FNetGUIDCache
//----------------------------------------------------------------------------------------

FNetGUIDCache::FNetGUIDCache( UNetDriver * InDriver ) : IsExportingNetGUIDBunch( false ), Driver( InDriver ), bUseNetworkChecksum( false ), bIgnorePackageMismatchOverride( false )
{
	UniqueNetIDs[0] = UniqueNetIDs[1] = 0;
}

class FArchiveCountMemGUID : public FArchive
{
public:
	FArchiveCountMemGUID() : Mem( 0 ) { ArIsCountingMemory = true; }
	void CountBytes( SIZE_T InNum, SIZE_T InMax ) { Mem += InMax; }
	SIZE_T Mem;
};

void FNetGUIDCache::CleanReferences()
{
	// Mark all static or non valid dynamic guids to timeout after NETWORK_GUID_TIMEOUT seconds
	// We want to leave them around for a certain amount of time to allow in-flight references to these guids to continue to resolve
	for ( auto It = ObjectLookup.CreateIterator(); It; ++It )
	{
		if ( It.Value().ReadOnlyTimestamp != 0 )
		{
			// If this guid was suppose to time out, check to see if it has, otherwise ignore it
			const double NETWORK_GUID_TIMEOUT = 90;

			if ( FPlatformTime::Seconds() - It.Value().ReadOnlyTimestamp > NETWORK_GUID_TIMEOUT )
			{
				It.RemoveCurrent();
			}

			continue;
		}

		if ( !It.Value().Object.IsValid() || It.Key().IsStatic() )
		{
			// We will leave this guid around for NETWORK_GUID_TIMEOUT seconds to make sure any in-flight guids can be resolved
			It.Value().ReadOnlyTimestamp = FPlatformTime::Seconds();
		}
	}

	for ( auto It = NetGUIDLookup.CreateIterator(); It; ++It )
	{
		if ( !It.Key().IsValid() || !ObjectLookup.Contains( It.Value() ) )
		{
			It.RemoveCurrent();
		}
	}

	// Sanity check
	// (make sure look-ups are reciprocal)
	for ( auto It = ObjectLookup.CreateIterator(); It; ++It )
	{
		check( !It.Key().IsDefault() );
		check( It.Key().IsStatic() != It.Key().IsDynamic() );

		checkf( !It.Value().Object.IsValid() || NetGUIDLookup.FindRef( It.Value().Object ) == It.Key() || It.Value().ReadOnlyTimestamp != 0, TEXT( "Failed to validate ObjectLookup map in UPackageMap. Object '%s' was not in the NetGUIDLookup map with with value '%s'." ), *It.Value().Object.Get()->GetPathName(), *It.Key().ToString() );
	}

	for ( auto It = NetGUIDLookup.CreateIterator(); It; ++It )
	{
		check( It.Key().IsValid() );
		checkf( ObjectLookup.FindRef( It.Value() ).Object == It.Key(), TEXT("Failed to validate NetGUIDLookup map in UPackageMap. GUID '%s' was not in the ObjectLookup map with with object '%s'."), *It.Value().ToString(), *It.Key().Get()->GetPathName());
	}

	FArchiveCountMemGUID CountBytesAr;

	ObjectLookup.CountBytes( CountBytesAr );
	NetGUIDLookup.CountBytes( CountBytesAr );

	// Make this a warning to be a constant reminder that we need to ultimately free this memory when we find a solution
	UE_LOG( LogNetPackageMap, Warning, TEXT( "FNetGUIDCache::CleanReferences: ObjectLookup: %i, NetGUIDLookup: %i, Mem: %i kB" ), ObjectLookup.Num(), NetGUIDLookup.Num(), ( CountBytesAr.Mem / 1024 ) );
}

bool FNetGUIDCache::SupportsObject( const UObject* Object ) const
{
	// NULL is always supported
	if ( !Object )
	{
		return true;
	}

	// If we already gave it a NetGUID, its supported.
	// This should happen for dynamic subobjects.
	FNetworkGUID NetGUID = NetGUIDLookup.FindRef( Object );

	if ( NetGUID.IsValid() )
	{
		return true;
	}

	if ( Object->IsFullNameStableForNetworking() )
	{
		// If object is fully net addressable, it's definitely supported
		return true;
	}

	if ( Object->IsSupportedForNetworking() )
	{
		// This means the server will explicitly tell the client to spawn and assign the id for this object
		return true;
	}

	UE_LOG( LogNetPackageMap, Warning, TEXT( "FNetGUIDCache::SupportsObject: %s NOT Supported." ), *Object->GetFullName() );
	//UE_LOG( LogNetPackageMap, Warning, TEXT( "   %s"), *DebugContextString );

	return false;
}

/**
 *	Dynamic objects are actors or sub-objects that were spawned in the world at run time, and therefor cannot be
 *	referenced with a path name to the client.
 */
bool FNetGUIDCache::IsDynamicObject( const UObject* Object )
{
	check( Object != NULL );
	check( Object->IsSupportedForNetworking() );

	// Any non net addressable object is dynamic
	return !Object->IsFullNameStableForNetworking();
}

bool FNetGUIDCache::IsNetGUIDAuthority() const
{
	return Driver == NULL || Driver->IsServer();
}

/** Gets or assigns a new NetGUID to this object. Returns whether the object is fully mapped or not */
FNetworkGUID FNetGUIDCache::GetOrAssignNetGUID( const UObject* Object )
{
	if ( !Object || !SupportsObject( Object ) )
	{
		// Null of unsupported object, leave as default NetGUID and just return mapped=true
		return FNetworkGUID();
	}

	// ----------------
	// Assign NetGUID if necessary
	// ----------------
	FNetworkGUID NetGUID = NetGUIDLookup.FindRef( Object );

	if ( NetGUID.IsValid() )
	{
		const FNetGuidCacheObject* CacheObject = ObjectLookup.Find( NetGUID );

		// Check to see if this guid is read only
		// If so, we should ignore this entry, and create a new one (or send default as client)
		const bool bReadOnly = CacheObject != NULL && CacheObject->ReadOnlyTimestamp > 0;

		if ( bReadOnly )
		{
			// Reset this object's guid, we will re-assign below (or send default as a client)
			NetGUIDLookup.Remove( Object );
		}
		else
		{
			return NetGUID;
		}
	}

	if ( !IsNetGUIDAuthority() )
	{
		// We cannot make or assign new NetGUIDs
		// Generate a default GUID, which signifies we write the full path
		// The server should detect this, and assign a full-time guid, and send that back to us
		return FNetworkGUID::GetDefault();
	}

	return AssignNewNetGUID_Server( Object );
}

/**
 *	Generate a new NetGUID for this object and assign it.
 */
FNetworkGUID FNetGUIDCache::AssignNewNetGUID_Server( const UObject* Object )
{
	check( IsNetGUIDAuthority() );

#define COMPOSE_NET_GUID( Index, IsStatic )	( ( ( Index ) << 1 ) | ( IsStatic ) )
#define ALLOC_NEW_NET_GUID( IsStatic )		( COMPOSE_NET_GUID( ++UniqueNetIDs[ IsStatic ], IsStatic ) )

	// Generate new NetGUID and assign it
	const int32 IsStatic = IsDynamicObject( Object ) ? 0 : 1;

	const FNetworkGUID NewNetGuid( ALLOC_NEW_NET_GUID( IsStatic ) );

	RegisterNetGUID_Server( NewNetGuid, Object );

	return NewNetGuid;
}

void FNetGUIDCache::RegisterNetGUID_Internal( const FNetworkGUID& NetGUID, const FNetGuidCacheObject& CacheObject )
{
	// We're pretty strict in this function, we expect everything to have been handled before we get here
	check( !ObjectLookup.Contains( NetGUID ) );

	ObjectLookup.Add( NetGUID, CacheObject );

	if ( CacheObject.Object != NULL )
	{
		check( !NetGUIDLookup.Contains( CacheObject.Object ) );

		// If we have an object, associate it with this guid now
		NetGUIDLookup.Add( CacheObject.Object, NetGUID );

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		History.Add( NetGUID, CacheObject.Object->GetPathName() );
#endif
	}
	else
	{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		History.Add( NetGUID, CacheObject.PathName.ToString() );
#endif
	}
}

/**
 *	Associates a net guid directly with an object
 *  This function is only called on server
 */
void FNetGUIDCache::RegisterNetGUID_Server( const FNetworkGUID& NetGUID, const UObject* Object )
{
	check( Object != NULL );
	check( IsNetGUIDAuthority() );				// Only the server should call this
	check( !Object->IsPendingKill() );
	check( !NetGUID.IsDefault() );
	check( !ObjectLookup.Contains( NetGUID ) );	// Server should never add twice

	FNetGuidCacheObject CacheObject;

	CacheObject.Object			= Object;
	
	RegisterNetGUID_Internal( NetGUID, CacheObject );
}

/**
 *	Associates a net guid directly with an object
 *  This function is only called on clients with dynamic guids
 */
void FNetGUIDCache::RegisterNetGUID_Client( const FNetworkGUID& NetGUID, const UObject* Object )
{
	check( !IsNetGUIDAuthority() );			// Only clients should be here
	check( !Object->IsPendingKill() );
	check( !NetGUID.IsDefault() );
	check( NetGUID.IsDynamic() );	// Clients should only assign dynamic guids through here (static guids go through RegisterNetGUIDFromPath_Client)

	UE_LOG( LogNetPackageMap, Log, TEXT( "RegisterNetGUID_Client: NetGUID: %s, Object: %s" ), *NetGUID.ToString(), Object ? *Object->GetName() : TEXT( "NULL" ) );
	
	//
	// If we have an existing entry, make sure things match up properly
	// We also completely disassociate anything so that RegisterNetGUID_Internal can be fairly strict
	//

	const FNetGuidCacheObject* ExistingCacheObjectPtr = ObjectLookup.Find( NetGUID );

	if ( ExistingCacheObjectPtr )
	{
		if ( ExistingCacheObjectPtr->PathName != NAME_None )
		{
			UE_LOG( LogNetPackageMap, Warning, TEXT( "RegisterNetGUID_Client: Guid with pathname. FullNetGUIDPath: %s" ), *FullNetGUIDPath( NetGUID ) );
		}

		// If this net guid was found but the old object is NULL, this can happen due to:
		//	1. Actor channel was closed locally (but we don't remove the net guid entry, since we can't know for sure if it will be referenced again)
		//		a. Then when we re-create a channel, and assign this actor, we will find the old guid entry here
		//	2. Dynamic object was locally GC'd, but then exported again from the server
		//
		// If this net guid was found and the objects match, we don't care. This can happen due to:
		//	1. Same thing above can happen, but if we for some reason didn't destroy the actor/object we will see this case
		//
		// If the object pointers are different, this can be a problem, 
		//	since this should only be possible if something gets out of sync during the net guid exchange code

		const UObject* OldObject = ExistingCacheObjectPtr->Object.Get();

		if ( OldObject != NULL && OldObject != Object )
		{
			UE_LOG( LogNetPackageMap, Warning, TEXT( "RegisterNetGUID_Client: Reassigning NetGUID <%s> to %s (was assigned to object %s)" ), *NetGUID.ToString(), Object ? *Object->GetPathName() : TEXT( "NULL" ), OldObject ? *OldObject->GetPathName() : TEXT( "NULL" ) );
		}
		else
		{
			UE_LOG( LogNetPackageMap, Verbose, TEXT( "RegisterNetGUID_Client: Reassigning NetGUID <%s> to %s (was assigned to object %s)" ), *NetGUID.ToString(), Object ? *Object->GetPathName() : TEXT( "NULL" ), OldObject ? *OldObject->GetPathName() : TEXT( "NULL" ) );
		}

		NetGUIDLookup.Remove( ExistingCacheObjectPtr->Object );
		ObjectLookup.Remove( NetGUID );
	}

	const FNetworkGUID* ExistingNetworkGUIDPtr = NetGUIDLookup.Find( Object );

	if ( ExistingNetworkGUIDPtr )
	{
		// This shouldn't happen on dynamic guids
		UE_LOG( LogNetPackageMap, Warning, TEXT( "Changing NetGUID on object %s from <%s:%s> to <%s:%s>" ), Object ? *Object->GetPathName() : TEXT( "NULL" ), *ExistingNetworkGUIDPtr->ToString(), ExistingNetworkGUIDPtr->IsDynamic() ? TEXT("TRUE") : TEXT("FALSE"), *NetGUID.ToString(), NetGUID.IsDynamic() ? TEXT("TRUE") : TEXT("FALSE") );
		ObjectLookup.Remove( *ExistingNetworkGUIDPtr );
		NetGUIDLookup.Remove( Object );
	}

	FNetGuidCacheObject CacheObject;

	CacheObject.Object = Object;

	RegisterNetGUID_Internal( NetGUID, CacheObject );
}

/**
 *	Associates a net guid with a path, that can be loaded or found later
 *  This function is only called on the client
 */
void FNetGUIDCache::RegisterNetGUIDFromPath_Client( const FNetworkGUID& NetGUID, const FString& PathName, const FNetworkGUID& OuterGUID, const uint32 NetworkChecksum, const uint32 PackageChecksum, const bool bNoLoad, const bool bIgnoreWhenMissing )
{
	check( !IsNetGUIDAuthority() );		// Server never calls this locally
	check( !NetGUID.IsDefault() );

	UE_LOG( LogNetPackageMap, Log, TEXT( "RegisterNetGUIDFromPath_Client: NetGUID: %s, PathName: %s, OuterGUID: %s" ), *NetGUID.ToString(), *PathName, *OuterGUID.ToString() );

	const FNetGuidCacheObject* ExistingCacheObjectPtr = ObjectLookup.Find( NetGUID );

	// If we find this guid, make sure nothing changes
	if ( ExistingCacheObjectPtr != NULL )
	{
		if ( ExistingCacheObjectPtr->PathName.ToString() != PathName )
		{
			UE_LOG( LogNetPackageMap, Warning, TEXT( "FNetGUIDCache::RegisterNetGUIDFromPath_Client: Path mismatch. Path: %s, Expected: %s, NetGUID: %s" ), *PathName, *ExistingCacheObjectPtr->PathName.ToString(), *NetGUID.ToString() );
		}

		if ( ExistingCacheObjectPtr->OuterGUID != OuterGUID )
		{
			UE_LOG( LogNetPackageMap, Warning, TEXT( "FNetGUIDCache::RegisterNetGUIDFromPath_Client: Outer mismatch. Path: %s, Outer: %s, Expected: %s, NetGUID: %s" ), *PathName, *OuterGUID.ToString(), *ExistingCacheObjectPtr->OuterGUID.ToString(), *NetGUID.ToString() );
		}

		if ( ExistingCacheObjectPtr->Object != NULL )
		{
			FNetworkGUID CurrentNetGUID = NetGUIDLookup.FindRef( ExistingCacheObjectPtr->Object );

			if ( CurrentNetGUID != NetGUID )
			{
				UE_LOG( LogNetPackageMap, Warning, TEXT( "FNetGUIDCache::RegisterNetGUIDFromPath_Client: Netguid mismatch. Path: %s, NetGUID: %s, Expected: %s" ), *PathName, *NetGUID.ToString(), *CurrentNetGUID.ToString() );
			}
		}

		return;
	}

	// Register a new guid with this path
	FNetGuidCacheObject CacheObject;

	CacheObject.PathName			= FName( *PathName );
	CacheObject.OuterGUID			= OuterGUID;
	CacheObject.NetworkChecksum		= NetworkChecksum;
	CacheObject.PackageChecksum		= PackageChecksum;
	CacheObject.bNoLoad				= bNoLoad;
	CacheObject.bIgnoreWhenMissing	= bIgnoreWhenMissing;

	RegisterNetGUID_Internal( NetGUID, CacheObject );
}

void FNetGUIDCache::AsyncPackageCallback(const FName& PackageName, UPackage * Package, EAsyncLoadingResult::Type Result)
{
	check( Package == NULL || Package->IsFullyLoaded() );

	FNetworkGUID NetGUID = PendingAsyncPackages.FindRef(PackageName);

	PendingAsyncPackages.Remove(PackageName);

	if ( !NetGUID.IsValid() )
	{
		UE_LOG( LogNetPackageMap, Error, TEXT( "AsyncPackageCallback: Could not find package. Path: %s" ), *PackageName.ToString() );
		return;
	}

	FNetGuidCacheObject * CacheObject = ObjectLookup.Find( NetGUID );

	if ( CacheObject == NULL )
	{
		UE_LOG( LogNetPackageMap, Error, TEXT( "AsyncPackageCallback: Could not find net guid. Path: %s, NetGUID: %s" ), *PackageName.ToString(), *NetGUID.ToString() );
		return;
	}

	if ( !CacheObject->bIsPending )
	{
		UE_LOG( LogNetPackageMap, Error, TEXT( "AsyncPackageCallback: Package wasn't pending. Path: %s, NetGUID: %s" ), *PackageName.ToString(), *NetGUID.ToString() );
	}

	CacheObject->bIsPending = false;
	
	if ( Package == NULL )
	{
		CacheObject->bIsBroken = true;
		UE_LOG( LogNetPackageMap, Error, TEXT( "AsyncPackageCallback: Package FAILED to load. Path: %s, NetGUID: %s" ), *PackageName.ToString(), *NetGUID.ToString() );
	}
	else if ( !ShouldIgnorePackageMismatch() && GetPackageChecksum( Package ) != CacheObject->PackageChecksum )
	{
		CacheObject->bIsBroken = true;
		UE_LOG( LogNetPackageMap, Error, TEXT( "AsyncPackageCallback: Package GUID mismatch! Path: %s, NetGUID: %s, GUID1: %u, GUID2: %u" ), *PackageName.ToString(), *NetGUID.ToString(), GetPackageChecksum( Package ), CacheObject->PackageChecksum );
	}
}

UObject* FNetGUIDCache::GetObjectFromNetGUID( const FNetworkGUID& NetGUID, const bool bIgnoreMustBeMapped )
{
	if ( !ensure( NetGUID.IsValid() ) )
	{
		return NULL;
	}

	if ( !ensure( !NetGUID.IsDefault() ) )
	{
		return NULL;
	}

	FNetGuidCacheObject * CacheObjectPtr = ObjectLookup.Find( NetGUID );

	if ( CacheObjectPtr == NULL )
	{
		// This net guid has never been registered
		return NULL;
	}

	UObject* Object = CacheObjectPtr->Object.Get();

	if ( Object != NULL )
	{
		// Either the name should match, or this is dynamic, or we're on the server
		check( Object->GetFName() == CacheObjectPtr->PathName || NetGUID.IsDynamic() || IsNetGUIDAuthority() );
		return Object;
	}

	if ( CacheObjectPtr->bIsBroken )
	{
		// This object is broken, we know it won't load
		// At this stage, any warnings should have already been logged, so we just need to ignore from this point forward
		return NULL;
	}

	if ( CacheObjectPtr->bIsPending )
	{
		// We're not done loading yet (and no error has been reported yet)
		return NULL;
	}

	if ( IsNetGUIDAuthority() )
	{
		// The client should never force the server to load objects
		// In this case, make sure to log it, but we're relying on the calling code to know what's best to do.
		// As of now, clients only send references to objects to the server via RPC, so this is usually just a NULL parameter that the game code
		// needs to handle.
		UE_LOG( LogNetPackageMap, Log, TEXT( "GetObjectFromNetGUID: Guid with no object on server. FullNetGUIDPath: %s" ), *FullNetGUIDPath( NetGUID ) );
		return NULL;
	}

	if ( CacheObjectPtr->PathName == NAME_None )
	{
		// If we don't have a path, assume this is a non stably named guid
		check( NetGUID.IsDynamic() );
		return NULL;
	}

	// First, resolve the outer
	UObject* ObjOuter = NULL;

	if ( CacheObjectPtr->OuterGUID.IsValid() )
	{
		// If we get here, we depend on an outer to fully load, don't go further until we know we have a fully loaded outer
		FNetGuidCacheObject * OuterCacheObject = ObjectLookup.Find( CacheObjectPtr->OuterGUID );

		if ( OuterCacheObject == NULL )
		{
			// Shouldn't be possible, but just in case...
			if ( CacheObjectPtr->OuterGUID.IsStatic() )
			{
				UE_LOG( LogNetPackageMap, Error, TEXT( "GetObjectFromNetGUID: Static outer not registered. FullNetGUIDPath: %s" ), *FullNetGUIDPath( NetGUID ) );
				CacheObjectPtr->bIsBroken = 1;	// Set this to 1 so that we don't keep spamming
			}
			return NULL;
		}

		// If outer is broken, we will never load, set outselves to broken as well and bail
		if ( OuterCacheObject->bIsBroken )
		{
			UE_LOG( LogNetPackageMap, Error, TEXT( "GetObjectFromNetGUID: Outer is broken. FullNetGUIDPath: %s" ), *FullNetGUIDPath( NetGUID ) );
			CacheObjectPtr->bIsBroken = 1;	// Set this to 1 so that we don't keep spamming
			return NULL;
		}

		// Try to resolve the outer
		ObjOuter = GetObjectFromNetGUID( CacheObjectPtr->OuterGUID, bIgnoreMustBeMapped );

		// If we can't resolve the outer
		if ( ObjOuter == NULL )
		{
			// If the outer is missing, warn unless told to ignore
			if ( !ShouldIgnoreWhenMissing( CacheObjectPtr->OuterGUID ) )
			{
				UE_LOG( LogNetPackageMap, Error, TEXT( "GetObjectFromNetGUID: Failed to find outer. FullNetGUIDPath: %s" ), *FullNetGUIDPath( NetGUID ) );
			}

			return NULL;
		}
	}

	// At this point, we either have an outer, or we are a package
	check( !CacheObjectPtr->bIsPending );
	check( ObjOuter == NULL || ObjOuter->GetOutermost()->IsFullyLoaded() );

	// See if this object is in memory
	Object = StaticFindObject( UObject::StaticClass(), ObjOuter, *CacheObjectPtr->PathName.ToString(), false );

	// Assume this is a package if the outer is invalid and this is a static guid
	const bool bIsPackage = NetGUID.IsStatic() && !CacheObjectPtr->OuterGUID.IsValid();

	if ( Object == NULL && !CacheObjectPtr->bNoLoad )
	{
		if ( bIsPackage )
		{
			// Async load the package if:
			//	1. We are actually a package
			//	2. We aren't already pending
			//	3. We're actually suppose to load (levels don't load here for example)
			check( !PendingAsyncPackages.Contains( CacheObjectPtr->PathName ) );

			if ( CVarAllowAsyncLoading.GetValueOnGameThread() > 0 )
			{
				PendingAsyncPackages.Add( CacheObjectPtr->PathName, NetGUID );
				CacheObjectPtr->bIsPending = true;
				LoadPackageAsync(CacheObjectPtr->PathName.ToString(), FLoadPackageAsyncDelegate::CreateRaw(this, &FNetGUIDCache::AsyncPackageCallback));

				UE_LOG( LogNetPackageMap, Log, TEXT( "GetObjectFromNetGUID: Async loading package. Path: %s, NetGUID: %s" ), *CacheObjectPtr->PathName.ToString(), *NetGUID.ToString() );

				// There is nothing else to do except wait on the delegate to tell us this package is done loading
				return NULL;
			}
			else
			{
				// Async loading disabled
				Object = LoadPackage( NULL, *CacheObjectPtr->PathName.ToString(), LOAD_None );
			}
		}
		else
		{
			// If we have a package, but for some reason didn't find the object then do a blocking load as a last attempt
			// This can happen for a few reasons:
			//	1. The object was GC'd, but the package wasn't, so we need to reload
			//	2. Someone else started async loading the outer package, and it's not fully loaded yet
			Object = StaticLoadObject( UObject::StaticClass(), ObjOuter, *CacheObjectPtr->PathName.ToString(), NULL, LOAD_NoWarn );

			if ( CVarAllowAsyncLoading.GetValueOnGameThread() > 0 )
			{
				UE_LOG( LogNetPackageMap, Error, TEXT( "GetObjectFromNetGUID: Forced blocking load. Path: %s, NetGUID: %s" ), *CacheObjectPtr->PathName.ToString(), *NetGUID.ToString() );
			}
		}
	}

	if ( Object == NULL )
	{
		if ( !CacheObjectPtr->bIgnoreWhenMissing )
		{
			CacheObjectPtr->bIsBroken = 1;	// Set this to 1 so that we don't keep spamming
			UE_LOG( LogNetPackageMap, Error, TEXT( "GetObjectFromNetGUID: Failed to resolve path. FullNetGUIDPath: %s" ), *FullNetGUIDPath( NetGUID ) );
		}

		return NULL;
	}

	if ( bIsPackage )
	{
		UPackage * Package = Cast< UPackage >( Object );

		if ( Package == NULL )
		{
			// This isn't really a package but it should be
			CacheObjectPtr->bIsBroken = true;
			UE_LOG( LogNetPackageMap, Error, TEXT( "GetObjectFromNetGUID: Object is not a package but should be! Path: %s, NetGUID: %s" ), *CacheObjectPtr->PathName.ToString(), *NetGUID.ToString() );
			return NULL;
		}

		if ( !Package->IsFullyLoaded() )
		{
			if ( CVarAllowAsyncLoading.GetValueOnGameThread() > 0 )
			{
				// If this package isn't fully loaded (and we are async loading here), don't complain, assume it will fully load at some point
				return NULL;
			}
			else
			{
				// If package isn't fully loaded, flush async loading now
				FlushAsyncLoading(); 
				check( Package->IsFullyLoaded() );
			}
		}

		if ( !ShouldIgnorePackageMismatch() && GetPackageChecksum( Package ) != CacheObjectPtr->PackageChecksum )
		{
			// If the package guid doesn't match, don't allow it to load
			CacheObjectPtr->bIsBroken = true;
			UE_LOG( LogNetPackageMap, Error, TEXT( "GetObjectFromNetGUID: Package GUID mismatch! Path: %s, NetGUID: %s, GUID1: %u, GUID2: %u" ), *CacheObjectPtr->PathName.ToString(), *NetGUID.ToString(), GetPackageChecksum( Package ), CacheObjectPtr->PackageChecksum );
			return NULL;
		}
	}

	if ( CacheObjectPtr->NetworkChecksum != 0 )
	{
		const uint32 NetworkChecksum = GetNetworkChecksum( Object );

		if ( CacheObjectPtr->NetworkChecksum != NetworkChecksum )
		{
			UE_LOG( LogNetPackageMap, Warning, TEXT( "GetObjectFromNetGUID: Network checksum mismatch. FullNetGUIDPath: %s, %u, %u" ), *FullNetGUIDPath( NetGUID ), CacheObjectPtr->NetworkChecksum, NetworkChecksum );

			CacheObjectPtr->bIsBroken = true;
			return NULL;
		}
	}

	// Assign the resolved object to this guid
	CacheObjectPtr->Object = Object;		

	// Assign the guid to the object 
	// We don't want to assign this guid to the object if this guid is timing out
	// But we'll have to if there is no other guid yet
	if ( CacheObjectPtr->ReadOnlyTimestamp == 0 || !NetGUIDLookup.Contains( Object ) )
	{
		if ( CacheObjectPtr->ReadOnlyTimestamp > 0 )
		{
			UE_LOG( LogNetPackageMap, Warning, TEXT( "GetObjectFromNetGUID: Attempt to reassign read-only guid. FullNetGUIDPath: %s" ), *FullNetGUIDPath( NetGUID ) );
		}

		NetGUIDLookup.Add( Object, NetGUID );
	}

	return Object;
}

bool FNetGUIDCache::ShouldIgnoreWhenMissing( const FNetworkGUID& NetGUID ) const
{
	if ( IsNetGUIDAuthority() )
	{
		return false;		// Server never ignores when missing, always warns
	}

	if ( NetGUID.IsDynamic() )
	{
		return true;		// Ignore missing dynamic guids
	}

	const FNetGuidCacheObject* CacheObject = ObjectLookup.Find( NetGUID );

	if ( CacheObject == NULL )
	{
		return false;		// If we haven't been told about this static guid before, we need to warn
	}

	const FNetGuidCacheObject* OutermostCacheObject = CacheObject;

	while ( OutermostCacheObject->OuterGUID.IsValid() )
	{
		OutermostCacheObject = ObjectLookup.Find( OutermostCacheObject->OuterGUID );
	}

	if ( OutermostCacheObject != NULL )
	{
		// If our outer package is not fully loaded, then don't warn, assume it will eventually come in
		if ( OutermostCacheObject->bIsPending )
		{
			// Outer is pending, don't warn
			return true;
		}
		// Sometimes, other systems async load packages, which we don't track, but still must be aware of
		if ( OutermostCacheObject->Object != NULL && !OutermostCacheObject->Object->GetOutermost()->IsFullyLoaded() )
		{
			return true;
		}
	}

	// Ignore warnings when we explicitly are told to
	return CacheObject->bIgnoreWhenMissing;
}

bool FNetGUIDCache::IsGUIDRegistered( const FNetworkGUID& NetGUID ) const
{
	if ( !NetGUID.IsValid() )
	{
		return false;
	}

	if ( NetGUID.IsDefault() )
	{
		return false;
	}

	return ObjectLookup.Contains( NetGUID );
}

bool FNetGUIDCache::IsGUIDLoaded( const FNetworkGUID& NetGUID ) const
{
	if ( !NetGUID.IsValid() )
	{
		return false;
	}

	if ( NetGUID.IsDefault() )
	{
		return false;
	}

	const FNetGuidCacheObject* CacheObjectPtr = ObjectLookup.Find( NetGUID );

	if ( CacheObjectPtr == NULL )
	{
		return false;
	}

	return CacheObjectPtr->Object != NULL;
}

bool FNetGUIDCache::IsGUIDBroken( const FNetworkGUID& NetGUID, const bool bMustBeRegistered ) const
{
	if ( !NetGUID.IsValid() )
	{
		return false;
	}

	if ( NetGUID.IsDefault() )
	{
		return false;
	}

	const FNetGuidCacheObject* CacheObjectPtr = ObjectLookup.Find( NetGUID );

	if ( CacheObjectPtr == NULL )
	{
		return bMustBeRegistered;
	}

	return CacheObjectPtr->bIsBroken;
}

FString FNetGUIDCache::FullNetGUIDPath( const FNetworkGUID& NetGUID ) const
{
	FString FullPath;

	GenerateFullNetGUIDPath_r( NetGUID, FullPath );

	return FullPath;
}

void FNetGUIDCache::GenerateFullNetGUIDPath_r( const FNetworkGUID& NetGUID, FString& FullPath ) const
{
	if ( !NetGUID.IsValid() )
	{
		// This is the end of the outer chain, we're done
		return;
	}

	const FNetGuidCacheObject* CacheObject = ObjectLookup.Find( NetGUID );

	if ( CacheObject == NULL )
	{
		// Doh, this shouldn't be possible, but if this happens, we can't continue
		// So warn, and return
		FullPath += FString::Printf( TEXT( "[%s]NOT_IN_CACHE" ), *NetGUID.ToString() );
		return;
	}

	GenerateFullNetGUIDPath_r( CacheObject->OuterGUID, FullPath );

	if ( !FullPath.IsEmpty() )
	{
		FullPath += TEXT( "." );
	}

	// Prefer the object name first, since non stable named objects don't store the path
	if ( CacheObject->Object.IsValid() )
	{
		// Sanity check that the names match if the path was stored
		if ( CacheObject->PathName != NAME_None && CacheObject->Object->GetName() != CacheObject->PathName.ToString() )
		{
			UE_LOG( LogNetPackageMap, Warning, TEXT( "GenerateFullNetGUIDPath_r: Name mismatch! %s != %s" ), *CacheObject->PathName.ToString(), *CacheObject->Object->GetName() );	
		}

		FullPath += FString::Printf( TEXT( "[%s]%s" ), *NetGUID.ToString(), *CacheObject->Object->GetName() );
	}
	else
	{
		if ( CacheObject->PathName == NAME_None )
		{
			// This can happen when a non stably named object is NULL
			FullPath += FString::Printf( TEXT( "[%s]EMPTY" ), *NetGUID.ToString() );
		}
		else
		{
			FullPath += FString::Printf( TEXT( "[%s]%s" ), *NetGUID.ToString(), *CacheObject->PathName.ToString() );			
		}
	}
}

uint32 FNetGUIDCache::GetClassNetworkChecksum( const UClass* Class )
{
	return Driver->NetCache->GetClassNetCache( Class )->GetClassChecksum();
}

uint32 FNetGUIDCache::GetNetworkChecksum( const UObject* Obj )
{
	if ( Obj == NULL )
	{
		return 0;
	}

	// If Obj is already a class, we can use that directly
	const UClass* Class = Cast< UClass >( Obj );

	return ( Class != NULL ) ? GetClassNetworkChecksum( Class ) : GetClassNetworkChecksum( Obj->GetClass() );
}

bool FNetGUIDCache::ShouldIgnorePackageMismatch() const
{
	return bIgnorePackageMismatchOverride || CVarIgnorePackageMismatch.GetValueOnGameThread();
};

void FNetGUIDCache::SetShouldUseNetworkChecksum( const bool bInUseNetworkChecksum )
{
	bUseNetworkChecksum = bInUseNetworkChecksum;
}

bool FNetGUIDCache::ShouldUseNetworkChecksum() const
{
	return bUseNetworkChecksum;
}

//------------------------------------------------------
// Debug command to see how many times we've exported each NetGUID
// Used for measuring inefficiencies. Some duplication is unavoidable since we cannot garuntee atomicicity across multiple channels.
// (for example if you have 100 actor channels of the same actor class go out at once, each will have to export the actor's class path in 
// order to be safey resolved... until the NetGUID is ACKd and then new actor channels will not have to export it again).
//
//------------------------------------------------------

static void	ListNetGUIDExports()
{
	struct FCompareNetGUIDCount
	{
		FORCEINLINE bool operator()( const int32& A, const int32& B ) const { return A > B; }
	};

	for (TObjectIterator<UPackageMapClient> PmIt; PmIt; ++PmIt)
	{
		UPackageMapClient *PackageMap = *PmIt;

		
		PackageMap->NetGUIDExportCountMap.ValueSort(FCompareNetGUIDCount());


		UE_LOG(LogNetPackageMap, Warning, TEXT("-----------------------"));
		for (auto It = PackageMap->NetGUIDExportCountMap.CreateIterator(); It; ++It)
		{
			UE_LOG(LogNetPackageMap, Warning, TEXT("NetGUID <%s> - %d"), *(It.Key().ToString()), It.Value() );	
		}
		UE_LOG(LogNetPackageMap, Warning, TEXT("-----------------------"));
	}			
}

FAutoConsoleCommand	ListNetGUIDExportsCommand(
	TEXT("net.ListNetGUIDExports"), 
	TEXT( "Lists open actor channels" ), 
	FConsoleCommandDelegate::CreateStatic(ListNetGUIDExports)
	);

// ----------------------------------------------------------------
