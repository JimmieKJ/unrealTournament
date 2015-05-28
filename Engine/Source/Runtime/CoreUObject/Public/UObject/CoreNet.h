// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ObjectBase.h"


DECLARE_DELEGATE_RetVal_OneParam( bool, FNetObjectIsDynamic, const UObject*);


//
// Information about a field.
//
class COREUOBJECT_API FFieldNetCache
{
public:
	UField* Field;
	int32	FieldNetIndex;
	uint32	FieldChecksum;
	FFieldNetCache()
	{}
	FFieldNetCache( UField* InField, int32 InFieldNetIndex, uint32 InFieldChecksum )
		: Field(InField), FieldNetIndex(InFieldNetIndex), FieldChecksum(InFieldChecksum)
	{}
};

//
// Information about a class, cached for network coordination.
//
class COREUOBJECT_API FClassNetCache
{
	friend class FClassNetCacheMgr;
public:
	FClassNetCache();
	FClassNetCache( const UClass* Class );

	int32 GetMaxIndex() const
	{
		return FieldsBase + Fields.Num();
	}

	const FFieldNetCache* GetFromField( const UObject* Field ) const
	{
		FFieldNetCache* Result = NULL;

		for ( const FClassNetCache* C= this; C; C = C->Super )
		{
			if ( ( Result = C->FieldMap.FindRef( Field ) ) != NULL )
			{
				break;
			}
		}
		return Result;
	}

	const FFieldNetCache* GetFromChecksum( const uint32 Checksum ) const
	{
		FFieldNetCache* Result = NULL;

		for ( const FClassNetCache* C = this; C; C = C->Super )
		{
			if ( ( Result = C->FieldChecksumMap.FindRef( Checksum ) ) != NULL )
			{
				break;
			}
		}
		return Result;
	}

	const FFieldNetCache* GetFromIndex( const int32 Index ) const
	{
		for ( const FClassNetCache* C = this; C; C = C->Super )
		{
			if ( Index >= C->FieldsBase && Index < C->FieldsBase + C->Fields.Num() )
			{
				return &C->Fields[Index-C->FieldsBase];
			}
		}
		return NULL;
	}

	uint32 GetClassChecksum() const { return ClassChecksum; }

private:
	int32								FieldsBase;
	const FClassNetCache*				Super;
	TWeakObjectPtr< const UClass >		Class;
	uint32								ClassChecksum;
	TArray< FFieldNetCache >			Fields;
	TMap< UObject*, FFieldNetCache* >	FieldMap;
	TMap< uint32, FFieldNetCache* >		FieldChecksumMap;
};


class COREUOBJECT_API FClassNetCacheMgr
{
public:
	FClassNetCacheMgr() : bDebugChecksum( false ), DebugChecksumIndent( 0 ) { }

	/** get the cached field to index mappings for the given class */
	const FClassNetCache*	GetClassNetCache( const UClass* Class );
	void					ClearClassNetCache();

	void				SortProperties( TArray< UProperty* >& Properties ) const;
	uint32				SortedStructFieldsChecksum( const UStruct* Struct, uint32 Checksum ) const;
	uint32				GetPropertyChecksum( const UProperty* Property, uint32 Checksum ) const;
	uint32				GetFunctionChecksum( const UFunction* Function, uint32 Checksum ) const;
	uint32				GetFieldChecksum( const UField* Field, uint32 Checksum ) const;

	bool				bDebugChecksum;
	int					DebugChecksumIndent;

private:
	TMap< TWeakObjectPtr< const UClass >, FClassNetCache* > ClassFieldIndices;
};


//
// Maps objects and names to and from indices for network communication.
//
class COREUOBJECT_API UPackageMap : public UObject
{
	DECLARE_CLASS_INTRINSIC( UPackageMap, UObject, CLASS_Transient | CLASS_Abstract | 0, CoreUObject );

	virtual bool		WriteObject( FArchive & Ar, UObject* Outer, FNetworkGUID NetGUID, FString ObjName ) { return false; }

	// @todo document
	virtual bool		SerializeObject( FArchive& Ar, UClass* Class, UObject*& Obj, FNetworkGUID *OutNetGUID = NULL ) { return false; }

	// @todo document
	virtual bool		SerializeName( FArchive& Ar, FName& Name );

	virtual UObject*	ResolvePathAndAssignNetGUID( const FNetworkGUID& NetGUID, const FString& PathName ) { return NULL; }

	virtual bool		SerializeNewActor(FArchive & Ar, class UActorChannel * Channel, class AActor *& Actor) { return false; }

	virtual void		ReceivedNak( const int32 NakPacketId ) { }
	virtual void		ReceivedAck( const int32 AckPacketId ) { }
	virtual void		NotifyBunchCommit( const int32 OutPacketId, const TArray< FNetworkGUID > & ExportNetGUIDs ) { }

	virtual void		GetNetGUIDStats(int32& AckCount, int32& UnAckCount, int32& PendingCount) { }

	virtual void		NotifyStreamingLevelUnload( UObject* UnloadedLevel ) { }

	virtual bool		PrintExportBatch() { return false; }

	void				SetDebugContextString( const FString& Str ) { DebugContextString = Str; }
	void				ClearDebugContextString() { DebugContextString.Empty(); }

	void							ResetTrackedUnmappedGuids( bool bShouldTrack ) { TrackedUnmappedNetGuids.Empty(); bShouldTrackUnmappedGuids = bShouldTrack; }
	const TArray< FNetworkGUID > &	GetTrackedUnmappedGuids() const { return TrackedUnmappedNetGuids; }

	virtual void		LogDebugInfo( FOutputDevice & Ar) { }
	virtual UObject*	GetObjectFromNetGUID( const FNetworkGUID& NetGUID, const bool bIgnoreMustBeMapped ) { return NULL; }
	virtual bool		IsGUIDBroken( const FNetworkGUID& NetGUID, const bool bMustBeRegistered ) const { return false; }

protected:

	bool					bSuppressLogs;

	bool					bShouldTrackUnmappedGuids;
	TArray< FNetworkGUID >	TrackedUnmappedNetGuids;

	FString					DebugContextString;
};


/** Represents a range of PacketIDs, inclusive */
struct FPacketIdRange
{
	FPacketIdRange(int32 _First, int32 _Last) : First(_First), Last(_Last) { }
	FPacketIdRange(int32 PacketId) : First(PacketId), Last(PacketId) { }
	FPacketIdRange() : First(INDEX_NONE), Last(INDEX_NONE) { }
	int32	First;
	int32	Last;

	bool	InRange(int32 PacketId)
	{
		return (First <= PacketId && PacketId <= Last);
	}
};


/** Information for tracking retirement and retransmission of a property. */
struct FPropertyRetirement
{
	FPropertyRetirement * Next;

	TSharedPtr<class INetDeltaBaseState> DynamicState;

	FPacketIdRange	OutPacketIdRange;

	uint32			Reliable		: 1;	// Whether it was sent reliably.
	uint32			CustomDelta		: 1;	// True if this property uses custom delta compression
	uint32			Config			: 1;

	FPropertyRetirement()
		:	Next ( NULL )
		,	DynamicState ( NULL )
		,   Reliable( 0 )
		,   CustomDelta( 0 )
		,	Config( 0 )
	{}
};


/** Secondary condition to check before considering the replication of a lifetime property. */
enum ELifetimeCondition
{
	COND_None				= 0,		// This property has no condition, and will send anytime it changes
	COND_InitialOnly		= 1,		// This property will only attempt to send on the initial bunch
	COND_OwnerOnly			= 2,		// This property will only send to the actor's owner
	COND_SkipOwner			= 3,		// This property send to every connection EXCEPT the owner
	COND_SimulatedOnly		= 4,		// This property will only send to simulated actors
	COND_AutonomousOnly		= 5,		// This property will only send to autonomous actors
	COND_SimulatedOrPhysics	= 6,		// This property will send to simulated OR bRepPhysics actors
	COND_InitialOrOwner		= 7,		// This property will send on the initial packet, or to the actors owner
	COND_Custom				= 8,		// This property has no particular condition, but wants the ability to toggle on/off via SetCustomIsActiveOverride
	COND_Max				= 9,
};


enum ELifetimeRepNotifyCondition
{
	REPNOTIFY_OnChanged		= 0,		// Only call the property's RepNotify function if it changes from the local value
	REPNOTIFY_Always		= 1,		// Always Call the property's RepNotify function when it is received from the server
};


/** FLifetimeProperty
 *	This class is used to track a property that is marked to be replicated for the lifetime of the actor channel.
 *  This doesn't mean the property will necessarily always be replicated, it just means:
 *	"check this property for replication for the life of the actor, and I don't want to think about it anymore"
 *  A secondary condition can also be used to skip replication based on the condition results
 */
class FLifetimeProperty
{
public:
	uint16				RepIndex;
	ELifetimeCondition	Condition;
	ELifetimeRepNotifyCondition RepNotifyCondition;

	FLifetimeProperty() : RepIndex( 0 ), Condition( COND_None ), RepNotifyCondition(REPNOTIFY_OnChanged) {}
	FLifetimeProperty( int32 InRepIndex ) : RepIndex( InRepIndex ), Condition( COND_None ), RepNotifyCondition(REPNOTIFY_OnChanged) { check( InRepIndex <= 65535 ); }
	FLifetimeProperty(int32 InRepIndex, ELifetimeCondition InCondition, ELifetimeRepNotifyCondition InRepNotifyCondition=REPNOTIFY_OnChanged) : RepIndex(InRepIndex), Condition(InCondition), RepNotifyCondition(InRepNotifyCondition) { check(InRepIndex <= 65535); }

	inline bool operator==( const FLifetimeProperty& Other ) const
	{
		if ( RepIndex == Other.RepIndex )
		{
			check( Condition == Other.Condition );		// Can't have different conditions if the RepIndex matches, doesn't make sense
			check( RepNotifyCondition == Other.RepNotifyCondition);
			return true;
		}

		return false;
	}
};

template <> struct TIsZeroConstructType<FLifetimeProperty> { enum { Value = true }; };


/**
 * FNetBitWriter
 *	A bit writer that serializes FNames and UObject* through
 *	a network packagemap.
 */
class COREUOBJECT_API FNetBitWriter : public FBitWriter
{
public:
	FNetBitWriter( UPackageMap * InPackageMap, int64 InMaxBits );
	FNetBitWriter( int64 InMaxBits );
	FNetBitWriter();

	class UPackageMap * PackageMap;

	virtual FArchive& operator<<(FName& Name) override;
	virtual FArchive& operator<<(UObject*& Object) override;
	virtual FArchive& operator<<(FStringAssetReference& Value) override;
};


/**
 * FNetBitReader
 *	A bit reader that serializes FNames and UObject* through
 *	a network packagemap.
 */
class COREUOBJECT_API FNetBitReader : public FBitReader
{
public:
	FNetBitReader( UPackageMap* InPackageMap=NULL, uint8* Src=NULL, int64 CountBits=0 );
	FNetBitReader( int64 InMaxBits );

	class UPackageMap * PackageMap;

	virtual FArchive& operator<<(FName& Name) override;
	virtual FArchive& operator<<(UObject*& Object) override;
	virtual FArchive& operator<<(FStringAssetReference& Value) override;
};


/**
 * INetDeltaBaseState
 *	An abstract interface for the base state used in net delta serialization. See notes in NetSerialization.h
 */
class INetDeltaBaseState
{
public:
	INetDeltaBaseState() { }
	virtual ~INetDeltaBaseState() { }

	virtual bool IsStateEqual(INetDeltaBaseState* Otherstate) = 0;

private:
};


class INetSerializeCB
{
public:
	INetSerializeCB() { }

	virtual void NetSerializeStruct( UScriptStruct* Struct, FArchive& Ar, UPackageMap* Map, void* Data, bool& bHasUnmapped ) = 0;
};


class IRepChangedPropertyTracker
{
public:
	IRepChangedPropertyTracker() { }

	virtual void SetCustomIsActiveOverride( const uint16 RepIndex, const bool bIsActive ) = 0;
};


/**
 * FNetDeltaSerializeInfo
 *  This is the parameter structure for delta serialization. It is kind of a dumping ground for anything custom implementations may need.
 */
struct FNetDeltaSerializeInfo
{
	FNetDeltaSerializeInfo()
	{
		Writer		= NULL;
		Reader		= NULL;

		NewState	= NULL;
		OldState	= NULL;
		Map			= NULL;
		Data		= NULL;

		Struct		= NULL;

		NetSerializeCB = NULL;

		bUpdateUnmappedObjects		= false;
		bOutSomeObjectsWereMapped	= false;
		bCalledPreNetReceive		= false;
		bOutHasMoreUnmapped			= false;
		Object						= NULL;
	}

	// Used when writing
	FBitWriter*						Writer;

	// Used for when reading
	FBitReader*						Reader;

	TSharedPtr<INetDeltaBaseState>*	NewState;		// SharedPtr to new base state created by NetDeltaSerialize.
	INetDeltaBaseState*				OldState;				// Pointer to the previous base state.
	UPackageMap*					Map;
	void*							Data;

	// Only used for fast TArray replication
	UStruct*						Struct;

	INetSerializeCB*				NetSerializeCB;

	bool							bUpdateUnmappedObjects;		// If true, we are wanting to update unmapped objects
	bool							bOutSomeObjectsWereMapped;
	bool							bCalledPreNetReceive;
	bool							bOutHasMoreUnmapped;
	UObject*						Object;

	// Debugging variables
	FString							DebugName;
};


/**
 * Checksum macros for verifying archives stay in sync
 */
COREUOBJECT_API void SerializeChecksum(FArchive &Ar, uint32 x, bool ErrorOK);

#define NET_ENABLE_CHECKSUMS 0


#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST) && NET_ENABLE_CHECKSUMS

#define NET_CHECKSUM_OR_END(Ser) \
{ \
	SerializeChecksum(Ser,0xE282FA84, true); \
}

#define NET_CHECKSUM(Ser) \
{ \
	SerializeChecksum(Ser,0xE282FA84, false); \
}

#define NET_CHECKSUM_CUSTOM(Ser, x) \
{ \
	SerializeChecksum(Ser,x, false); \
}

// There are cases where a checksum failure is expected, but we still need to eat the next word (just dont without erroring)
#define NET_CHECKSUM_IGNORE(Ser) \
{ \
	uint32 Magic = 0; \
	Ser << Magic; \
}

#else

// No ops in shipping builds
#define NET_CHECKSUM(Ser)
#define NET_CHECKSUM_IGNORE(Ser)
#define NET_CHECKSUM_CUSTOM(Ser, x)
#define NET_CHECKSUM_OR_END(ser)


#endif


/**
 * Functions to assist in detecting errors during RPC calls
 */
COREUOBJECT_API void			RPC_ResetLastFailedReason();
COREUOBJECT_API void			RPC_ValidateFailed( const TCHAR* Reason );
COREUOBJECT_API const TCHAR *	RPC_GetLastFailedReason();
