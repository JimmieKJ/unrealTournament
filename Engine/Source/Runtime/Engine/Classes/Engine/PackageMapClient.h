// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


/**
 * PackageMap implementation that is client/connection specific. This subclass implements all NetGUID Acking and interactions with a UConnection.
 *	On the server, each client will have their own instance of UPackageMapClient.
 *
 *	UObjects are first serialized as <NetGUID, Name/Path> pairs. UPackageMapClient tracks each NetGUID's usage and knows when a NetGUID has been ACKd.
 *	Once ACK'd, objects are just serialized as <NetGUID>. The result is higher bandwidth usage upfront for new clients, but minimal bandwidth once
 *	things gets going.
 *
 *	A further optimization is enabled by default. References will actually be serialized via:
 *	<NetGUID, <(Outer *), Object Name> >. Where Outer * is a reference to the UObject's Outer.
 *
 *	The main advantages from this are:
 *		-Flexibility. No precomputed net indices or large package lists need to be exchanged for UObject serialization.
 *		-Cross version communication. The name is all that is needed to exchange references.
 *		-Efficiency in that a very small % of UObjects will ever be serialized. Only Objects that serialized are assigned NetGUIDs.
 */

#pragma once
#include "Net/DataBunch.h"
#include "PackageMapClient.generated.h"


/** Stores an object with path associated with FNetworkGUID */
class FNetGuidCacheObject
{
public:
	FNetGuidCacheObject() : NetworkChecksum( 0 ), PackageChecksum( 0 ), ReadOnlyTimestamp( 0 ), bNoLoad( 0 ), bIgnoreWhenMissing( 0 ), bIsPending( 0 ), bIsBroken( 0 )
	{
	}

	TWeakObjectPtr< UObject >	Object;

	// These fields are set when this guid is static
	FNetworkGUID				OuterGUID;
	FName						PathName;
	uint32						NetworkChecksum;			// Network checksum saved, used to determine backwards compatible
	uint32						PackageChecksum;			// If this is a package, this is the guid to expect

	double						ReadOnlyTimestamp;			// Time in second when we should start timing out after going read only

	uint8						bNoLoad				: 1;	// Don't load this, only do a find
	uint8						bIgnoreWhenMissing	: 1;	// Don't warn when this asset can't be found or loaded
	uint8						bIsPending			: 1;	// This object is waiting to be fully loaded
	uint8						bIsBroken			: 1;	// If this object failed to load, then we set this to signify that we should stop trying
};

class ENGINE_API FNetGUIDCache
{
public:
	FNetGUIDCache( UNetDriver * InDriver );

	void			CleanReferences();
	bool			SupportsObject( const UObject* Object ) const;
	bool			IsDynamicObject( const UObject* Object );
	bool			IsNetGUIDAuthority() const;
	FNetworkGUID	GetOrAssignNetGUID( const UObject* Object );
	FNetworkGUID	AssignNewNetGUID_Server( const UObject* Object );
	void			RegisterNetGUID_Internal( const FNetworkGUID& NetGUID, const FNetGuidCacheObject& CacheObject );
	void			RegisterNetGUID_Server( const FNetworkGUID& NetGUID, const UObject* Object );
	void			RegisterNetGUID_Client( const FNetworkGUID& NetGUID, const UObject* Object );
	void			RegisterNetGUIDFromPath_Client( const FNetworkGUID& NetGUID, const FString& PathName, const FNetworkGUID& OuterGUID, const uint32 NetworkChecksum, const uint32 PackageChecksum, const bool bNoLoad, const bool bIgnoreWhenMissing );
	UObject *		GetObjectFromNetGUID( const FNetworkGUID& NetGUID, const bool bIgnoreMustBeMapped );
	bool			ShouldIgnoreWhenMissing( const FNetworkGUID& NetGUID ) const;
	bool			IsGUIDRegistered( const FNetworkGUID& NetGUID ) const;
	bool			IsGUIDLoaded( const FNetworkGUID& NetGUID ) const;
	bool			IsGUIDBroken( const FNetworkGUID& NetGUID, const bool bMustBeRegistered ) const;
	FString			FullNetGUIDPath( const FNetworkGUID& NetGUID ) const;
	void			GenerateFullNetGUIDPath_r( const FNetworkGUID& NetGUID, FString& FullPath ) const;
	void			SetIgnorePackageMismatchOverride( const bool bInIgnorePackageMismatchOverride ) { bIgnorePackageMismatchOverride = bInIgnorePackageMismatchOverride; }
	bool			ShouldIgnorePackageMismatch() const;
	uint32			GetClassNetworkChecksum( const UClass* Class );
	uint32			GetNetworkChecksum( const UObject* Obj );
	void			SetShouldUseNetworkChecksum( const bool bInUseNetworkChecksum );
	bool			ShouldUseNetworkChecksum() const;

	void			AsyncPackageCallback(const FName& PackageName, UPackage * Package, EAsyncLoadingResult::Type Result);
	
	TMap< FNetworkGUID, FNetGuidCacheObject >		ObjectLookup;
	TMap< TWeakObjectPtr< UObject >, FNetworkGUID >	NetGUIDLookup;
	int32											UniqueNetIDs[2];

	bool											IsExportingNetGUIDBunch;

	UNetDriver *									Driver;

	TMap< FName, FNetworkGUID >						PendingAsyncPackages;

	bool											bUseNetworkChecksum;
	bool											bIgnorePackageMismatchOverride;

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	// History for debugging entries in the guid cache
	TMap<FNetworkGUID, FString>						History;
#endif
};

UCLASS(transient)
class ENGINE_API UPackageMapClient : public UPackageMap
{
public:
	GENERATED_BODY()

	UPackageMapClient(const FObjectInitializer & ObjectInitializer = FObjectInitializer::Get());

	void Initialize(UNetConnection * InConnection, TSharedPtr<FNetGUIDCache> InNetGUIDCache)
	{
		Connection = InConnection;
		GuidCache = InNetGUIDCache;
		ExportNetGUIDCount = 0;
	}

	virtual ~UPackageMapClient()
	{
		if (CurrentExportBunch)
		{
			delete CurrentExportBunch;
			CurrentExportBunch = NULL;
		}
	}

	
	// UPackageMap Interface
	virtual bool SerializeObject( FArchive& Ar, UClass* Class, UObject*& Obj, FNetworkGUID *OutNetGUID = NULL ) override;
	virtual bool SerializeNewActor( FArchive& Ar, class UActorChannel *Channel, class AActor*& Actor) override;
	
	virtual bool WriteObject( FArchive& Ar, UObject* Outer, FNetworkGUID NetGUID, FString ObjName ) override;

	// UPackageMapClient Connection specific methods

	bool NetGUIDHasBeenAckd(FNetworkGUID NetGUID);

	virtual void ReceivedNak( const int32 NakPacketId ) override;
	virtual void ReceivedAck( const int32 AckPacketId ) override;
	virtual void NotifyBunchCommit( const int32 OutPacketId, const TArray< FNetworkGUID > & ExportNetGUIDs ) override;
	virtual void GetNetGUIDStats(int32 &AckCount, int32 &UnAckCount, int32 &PendingCount) override;

	void ReceiveNetGUIDBunch( FInBunch &InBunch );
	bool AppendExportBunches(TArray<FOutBunch *>& OutgoingBunches);

	TMap<FNetworkGUID, int32>	NetGUIDExportCountMap;	// How many times we've exported each NetGUID on this connection. Public for ListNetGUIDExports 

	void HandleUnAssignedObject( const UObject* Obj );

	static void	AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);

	virtual void NotifyStreamingLevelUnload(UObject* UnloadedLevel) override;

	virtual bool PrintExportBatch() override;

	virtual void		LogDebugInfo( FOutputDevice & Ar) override;
	virtual UObject *	GetObjectFromNetGUID( const FNetworkGUID& NetGUID, const bool bIgnoreMustBeMapped ) override;
	virtual bool		IsGUIDBroken( const FNetworkGUID& NetGUID, const bool bMustBeRegistered ) const override { return GuidCache->IsGUIDBroken( NetGUID, bMustBeRegistered ); }

	TArray< FNetworkGUID > & GetMustBeMappedGuidsInLastBunch() { return MustBeMappedGuidsInLastBunch; }

protected:

	bool	ExportNetGUID( FNetworkGUID NetGUID, const UObject* Object, FString PathName, UObject* ObjOuter );
	void	ExportNetGUIDHeader();

	void			InternalWriteObject( FArchive& Ar, FNetworkGUID NetGUID, const UObject* Object, FString ObjectPathName, UObject* ObjectOuter );	
	FNetworkGUID	InternalLoadObject( FArchive & Ar, UObject *& Object, int InternalLoadObjectRecursionCount );

	virtual UObject* ResolvePathAndAssignNetGUID( const FNetworkGUID& NetGUID, const FString& PathName ) override;

	bool	ShouldSendFullPath(const UObject* Object, const FNetworkGUID &NetGUID);
	
	bool IsNetGUIDAuthority() const;

	class UNetConnection* Connection;

	bool ObjectLevelHasFinishedLoading(UObject* Obj);

	TSet< FNetworkGUID >				CurrentExportNetGUIDs;				// Current list of NetGUIDs being written to the Export Bunch.

	TMap< FNetworkGUID, int32 >			NetGUIDAckStatus;
	TArray< FNetworkGUID >				PendingAckGUIDs;					// Quick access to all GUID's that haven't been acked

	// Bunches of NetGUID/path tables to send with the current content bunch
	TArray<FOutBunch* >					ExportBunches;
	FOutBunch *							CurrentExportBunch;

	int32								ExportNetGUIDCount;

	TSharedPtr< FNetGUIDCache >			GuidCache;

	TArray< FNetworkGUID >				MustBeMappedGuidsInLastBunch;
};
