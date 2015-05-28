// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	RepLayout.h:
	FRepLayout is a helper class to quickly replicate properties that are marked for replication
=============================================================================*/
#pragma once

#include "CoreNet.h"
#include "Engine/EngineTypes.h"

class FOutBunch;
class FInBunch;

class FRepChangedParent
{
public:
	FRepChangedParent() : Active( 1 ), OldActive( 1 ), IsConditional( 0 ) {}

	TArray< uint16 >	Changed;

	uint32				Active			: 1;
	uint32				OldActive		: 1;
	uint32				IsConditional	: 1;
};

/** FRepChangedPropertyTracker
 * This class is used to store the change list for a group of properties of a particular actor/object
 * This information is shared across connections when possible
 */
class FRepChangedPropertyTracker : public IRepChangedPropertyTracker
{
public:
	FRepChangedPropertyTracker() : LastReplicationGroupFrame( 0 ), LastReplicationFrame( 0 ), ActiveStatusChanged( false ), UnconditionalPropChanged( false ) { }
	virtual ~FRepChangedPropertyTracker() { }

	virtual void SetCustomIsActiveOverride( const uint16 RepIndex, const bool bIsActive ) override
	{
		FRepChangedParent & Parent = Parents[RepIndex];

		checkSlow( Parent.IsConditional );

		Parent.Active = bIsActive ? 1 : 0;

		if ( Parent.Active != Parent.OldActive )
		{
			ActiveStatusChanged++;		// So we know to rebuild the conditional list with/without this property
		}

		Parent.OldActive = Parent.Active;
	}

	uint32						LastReplicationGroupFrame;		// Most recent frame number of the last FRepState that was compared this frame
	uint32						LastReplicationFrame;
	class FRepState *			LastRepState;

	TArray< FRepChangedParent >	Parents;

	uint32						ActiveStatusChanged;
	bool						UnconditionalPropChanged;
};

class FRepLayout;

class FRepChangedHistory
{
public:
	FRepChangedHistory() : Resend( false ) {}

	FPacketIdRange		OutPacketIdRange;
	TArray< uint16 >	Changed;
	bool				Resend;
};

class FUnmappedGuidMgrElement
{
public:
	FUnmappedGuidMgrElement() : NumBufferBits( 0 ), Array( NULL ) {}
	FUnmappedGuidMgrElement( FBitReader & InReader, FBitReaderMark & InMark, const TArray< FNetworkGUID > & InUnmappedGUIDs, const int32 InParentIndex, const int32 InCmdIndex ) : UnmappedGUIDs( InUnmappedGUIDs ), Array( NULL ), ParentIndex( InParentIndex ), CmdIndex( InCmdIndex ) 
	{
		NumBufferBits = InReader.GetPosBits() - InMark.GetPos();
		InMark.Copy( InReader, Buffer );
	}
	FUnmappedGuidMgrElement( class FUnmappedGuidMgr * InArray, const int32 InParentIndex, const int32 InCmdIndex ) : NumBufferBits( 0 ), Array( InArray ), ParentIndex( InParentIndex ), CmdIndex( InCmdIndex ) {}

	~FUnmappedGuidMgrElement();

	TArray< FNetworkGUID >		UnmappedGUIDs;
	TArray< uint8 >				Buffer;
	int32						NumBufferBits;

	class FUnmappedGuidMgr *	Array;
	int32						ParentIndex;
	int32						CmdIndex;
};

class FUnmappedGuidMgr
{
public:
	TMap< int32, FUnmappedGuidMgrElement > Map;
};

/** FRepState
 *  Stores state used by the FRepLayout manager
*/
class FRepState
{
public:
	FRepState() : 
		HistoryStart( 0 ), 
		HistoryEnd( 0 ),
		LastReplicationFrame( 0 ),
		NumNaks( 0 ),
		OpenAckedCalled( false ),
		AwakeFromDormancy( false ),
		ActiveStatusChanged( 0 )
	{ }

	~FRepState();

	TArray< uint8 >				StaticBuffer;

	FUnmappedGuidMgr			UnmappedGuids;

	TSharedPtr< FRepLayout >	RepLayout;
	
	TArray< UProperty * >		RepNotifies;

	TSharedPtr<FRepChangedPropertyTracker> RepChangedPropertyTracker;

	static const int32 MAX_CHANGE_HISTORY = 32;

	FRepChangedHistory			ChangeHistory[MAX_CHANGE_HISTORY];
	int32						HistoryStart;
	int32						HistoryEnd;

	uint32						LastReplicationFrame;
	int32						NumNaks;

	TArray< FRepChangedHistory >	PreOpenAckHistory;

	bool							OpenAckedCalled;
	bool							AwakeFromDormancy;

	TArray< uint16 >				ConditionalLifetime;		// Properties the need to be checked conditionally (based on net initial, role, etc)
	FReplicationFlags				RepFlags;
	uint32							ActiveStatusChanged;
};

enum ERepLayoutCmdType
{
	REPCMD_DynamicArray			= 0,	// Dynamic array
	REPCMD_Return				= 1,	// Return from array, or end of stream
	REPCMD_Property				= 2,	// Generic property

	REPCMD_PropertyBool			= 3,
	REPCMD_PropertyFloat		= 4,
	REPCMD_PropertyInt			= 5,
	REPCMD_PropertyByte			= 6,
	REPCMD_PropertyName			= 7,
	REPCMD_PropertyObject		= 8,
	REPCMD_PropertyUInt32		= 9,
	REPCMD_PropertyVector		= 10,
	REPCMD_PropertyRotator		= 11,
	REPCMD_PropertyPlane		= 12,
	REPCMD_PropertyVector100	= 13,
	REPCMD_PropertyNetId		= 14,
	REPCMD_RepMovement			= 15,
	REPCMD_PropertyVectorNormal	= 16,
	REPCMD_PropertyVector10		= 17,
	REPCMD_PropertyVectorQ		= 18,
	REPCMD_PropertyString		= 19,
	REPCMD_PropertyUInt64		= 20,
};

enum ERepParentFlags
{
	PARENT_IsLifetime			= ( 1 << 0 ),
	PARENT_IsConditional		= ( 1 << 1 ),		// True if this property has a secondary condition to check
	PARENT_IsConfig				= ( 1 << 2 ),		// True if this property is defaulted from a config file
	PARENT_IsCustomDelta		= ( 1 << 3 )		// True if this property uses custom delta compression
};

class FRepParentCmd
{
public:
	FRepParentCmd( UProperty * InProperty, int32 InArrayIndex ) : 
		Property( InProperty ), 
		ArrayIndex( InArrayIndex ), 
		CmdStart( 0 ), 
		CmdEnd( 0 ), 
		RoleSwapIndex( -1 ), 
		Condition( COND_None ),
		RepNotifyCondition(REPNOTIFY_OnChanged),
		Flags( 0 )
	{}

	UProperty *			Property;
	int32				ArrayIndex;
	uint16				CmdStart;
	uint16				CmdEnd;
	int32				RoleSwapIndex;
	ELifetimeCondition	Condition;
	ELifetimeRepNotifyCondition	RepNotifyCondition;

	uint32				Flags;
};

class FRepLayoutCmd
{
public:
	UProperty * Property;		// Pointer back to property, used for NetSerialize calls, etc.
	uint8		Type;
	uint16		EndCmd;			// For arrays, this is the cmd index to jump to, to skip this arrays inner elements
	uint16		ElementSize;	// For arrays, element size of data
	int32		Offset;			// Absolute offset of property
	uint16		RelativeHandle;	// Handle relative to start of array, or top list
	uint16		ParentIndex;	// Index into Parents
};

class FRepWriterState
{
public:
	FRepWriterState( FNetBitWriter & InWriter, TArray< uint16 > & InChanged, bool bInDoChecksum ) : 
		Writer( InWriter ), 
		Changed( InChanged ),
		CurrentChanged( 0 ),
		bDoChecksum( bInDoChecksum )
	{
	}

	FNetBitWriter &		Writer;
	TArray< uint16 > &	Changed;
	int32				CurrentChanged;
	bool				bDoChecksum;
};

/** FRepLayout
 *  This class holds all replicated properties for a parent property, and all its children
 *	Helpers functions exist to read/write and compare property state.
*/
class FRepLayout
{
	friend class FRepState;

public:
	FRepLayout() : FirstNonCustomParent( 0 ), RoleIndex( -1 ), RemoteRoleIndex( -1 ), Owner( NULL ) {}

	void OpenAcked( FRepState * RepState ) const;

	void InitRepState( 
		FRepState *									RepState, 
		UClass *									InObjectClass, 
		uint8 *										Src, 
		TSharedPtr< FRepChangedPropertyTracker > &	InRepChangedPropertyTracker ) const;

	void InitChangedTracker( FRepChangedPropertyTracker * ChangedTracker ) const;

	void WritePropertyHeader( 
		UObject *			Object,
		UClass *			ObjectClass,
		UActorChannel *		OwningChannel,
		UProperty *			Property, 
		FOutBunch &			Bunch, 
		uint32				ArrayIndex, 
		bool &				bContentBlockWritten ) const;

	bool ReplicateProperties( 
		FRepState * RESTRICT		RepState, 
		const uint8* RESTRICT		Data, 
		UClass *					ObjectClass,
		UActorChannel *				OwningChannel,
		FOutBunch &					Writer, 
		const FReplicationFlags &	RepFlags,
		bool &						bContentBlockWritten ) const;

	void SendProperties( 
		FRepState *	RESTRICT		RepState, 
		const FReplicationFlags &	RepFlags,
		const uint8* RESTRICT		Data, 
		UClass *					ObjectClass,
		UActorChannel *				OwningChannel,
		FOutBunch &					Writer, 
		TArray< uint16 >	 &		Changed, 
		bool &						bContentBlockWritten ) const;

	ENGINE_API void InitFromObjectClass( UClass * InObjectClass );

	bool ReceiveProperties( UClass * InObjectClass, FRepState * RESTRICT RepState, void* RESTRICT Data, FNetBitReader & InBunch, bool & bOutHasUnmapped ) const;
	void UpdateUnmappedObjects( FRepState *	RepState, UPackageMap * PackageMap, UObject* Object, bool & bOutSomeObjectsWereMapped, bool & bOutHasMoreUnmapped ) const;

	void CallRepNotifies( FRepState * RepState, UObject* Object ) const;
	void PostReplicate( FRepState * RepState, FPacketIdRange & PacketRange, bool bReliable ) const;
	void ReceivedNak( FRepState * RepState, int32 NakPacketId ) const;
	bool AllAcked( FRepState * RepState ) const;
	bool ReadyForDormancy( FRepState * RepState ) const;

	void ValidateWithChecksum( const void* RESTRICT Data, FArchive & Ar ) const;
	uint32 GenerateChecksum( const FRepState* RepState ) const;

	void MergeDirtyList( FRepState * RepState, const void* RESTRICT Data, const TArray< uint16 > & Dirty1, const TArray< uint16 > & Dirty2, TArray< uint16 > & MergedDirty ) const;

	bool DiffProperties( FRepState * RepState, const void* RESTRICT Data, const bool bSync ) const;

	void GetLifetimeCustomDeltaProperties(TArray< int32 > & OutCustom, TArray< ELifetimeCondition >	& OutConditions);

	// RPC support
	void InitFromFunction( UFunction * InFunction );
	void SendPropertiesForRPC( UObject* Object, UFunction * Function, UActorChannel * Channel, FNetBitWriter & Writer, void* Data ) const;
	void ReceivePropertiesForRPC( UObject* Object, UFunction * Function, UActorChannel * Channel, FNetBitReader & Reader, void* Data ) const;

	// Struct support
	void SerializePropertiesForStruct( UStruct * Struct, FArchive & Ar, UPackageMap	* Map, void* Data, bool & bHasUnmapped ) const;	
	void InitFromStruct( UStruct * InStruct );

	// Serializes all replicated properties of a UObject in or out of an archive (depending on what type of archive it is)
	ENGINE_API void SerializeObjectReplicatedProperties(UObject* Object, FArchive & Ar) const;

	void WriteNetworkChecksum( FOutBunch& Bunch );
	bool ReadNetworkChecksum( FInBunch& Bunch );

private:
	void RebuildConditionalProperties( FRepState * RESTRICT	RepState, const FRepChangedPropertyTracker& ChangedTracker, const FReplicationFlags& RepFlags ) const;

	void LogChangeListMismatches( 
		const uint8* 						Data, 
		UActorChannel *						OwningChannel, 
		const TArray< uint16 > &			PropertyList,
		FRepState *							RepState1, 
		FRepState * 						RepState2, 
		const TArray< FRepChangedParent > & ChangedParents1,
		const TArray< FRepChangedParent > & ChangedParents2 ) const;

	void SanityCheckShadowStateAgainstChangeList(
		FRepState * 						RepState, 
		const uint8* 						Data, 
		UActorChannel *						OwningChannel, 
		const TArray< uint16 > &			PropertyList,
		FRepState *							OtherRepState,
		const TArray< FRepChangedParent > & OtherChangedParents ) const;

	void UpdateChangelistHistory( FRepState * RepState, UClass * ObjectClass, const uint8* RESTRICT Data, const int32 AckPacketId, TArray< uint16 > * OutMerged ) const;

	uint16 CompareProperties_r(
		const int32				CmdStart,
		const int32				CmdEnd,
		const uint8* RESTRICT	CompareData, 
		const uint8* RESTRICT	Data,
		TArray< uint16 > &		Changed,
		uint16					Handle ) const;

	void CompareProperties_Array_r( 
		const uint8* RESTRICT	CompareData, 
		const uint8* RESTRICT	Data,
		TArray< uint16 > &		Changed,
		const uint16			CmdIndex,
		const uint16			Handle ) const;

	bool CompareProperties( 
		FRepState * RESTRICT				RepState, 
		const uint8* RESTRICT				CompareData,
		const uint8* RESTRICT				Data, 
		TArray< FRepChangedParent > &		OutChangedParents,
		const TArray< uint16 > &			PropertyList ) const;

	void SendProperties_DynamicArray_r( 
		FRepState *	RESTRICT		RepState, 
		const FReplicationFlags &	RepFlags,
		FRepWriterState &			WriterState,
		const int32					CmdIndex, 
		const uint8* RESTRICT		StoredData, 
		const uint8* RESTRICT		Data, 
		uint16						Handle ) const;

	uint16 SendProperties_r( 
		FRepState *	RESTRICT		RepState, 
		const FReplicationFlags &	RepFlags,
		FRepWriterState &			WriterState,
		const int32					CmdStart, 
		const int32					CmdEnd, 
		const uint8* RESTRICT		StoredData, 
		const uint8* RESTRICT		Data, 
		uint16						Handle ) const;

	void UpdateUnmappedObjects_r( 
		FRepState *			RepState, 
		FUnmappedGuidMgr *	UnmappedGuids, 
		UObject *			OriginalObject,
		UPackageMap *		PackageMap, 
		uint8* RESTRICT		StoredData, 
		uint8* RESTRICT		Data, 
		const int32			MaxAbsOffset,
		bool &				bOutSomeObjectsWereMapped,
		bool &				bOutHasMoreUnmapped ) const;

	void ValidateWithChecksum_DynamicArray_r( const FRepLayoutCmd& Cmd, const int32 CmdIndex, const uint8* RESTRICT Data, FArchive & Ar ) const;
	void ValidateWithChecksum_r( const int32 CmdStart, const int32 CmdEnd, const uint8* RESTRICT Data, FArchive & Ar ) const;

	void SanityCheckChangeList_DynamicArray_r( 
		const int32				CmdIndex, 
		const uint8* RESTRICT	Data, 
		TArray< uint16 > &		Changed,
		int32 &					ChangedIndex ) const;

	uint16 SanityCheckChangeList_r( 
		const int32				CmdStart, 
		const int32				CmdEnd, 
		const uint8* RESTRICT	Data, 
		TArray< uint16 > &		Changed,
		int32 &					ChangedIndex,
		uint16					Handle ) const;

	void SanityCheckChangeList( const uint8* RESTRICT Data, TArray< uint16 > & Changed ) const;

	uint16 AddParentProperty( UProperty * Property, int32 ArrayIndex );

	int32 InitFromProperty_r( UProperty * Property, int32 Offset, int32 RelativeHandle, int32 ParentIndex );

	void AddPropertyCmd( UProperty * Property, int32 Offset, int32 RelativeHandle, int32 ParentIndex );
	void AddArrayCmd( UArrayProperty * Property, int32 Offset, int32 RelativeHandle, int32 ParentIndex );
	void AddReturnCmd();

	void SerializeProperties_DynamicArray_r( 
		FArchive &			Ar, 
		UPackageMap	*		Map,
		const int32			CmdIndex,
		uint8 *				Data,
		bool &				bHasUnmapped ) const;

	void SerializeProperties_r( 
		FArchive &			Ar, 
		UPackageMap	*		Map,
		const int32			CmdStart, 
		const int32			CmdEnd, 
		void *				Data,
		bool &				bHasUnmapped ) const;

	void ConstructProperties( FRepState * RepState ) const;
	void InitProperties( FRepState * RepState, uint8* Src ) const;
	void DestructProperties( FRepState * RepState ) const;

	TArray< FRepParentCmd >		Parents;
	TArray< FRepLayoutCmd >		Cmds;

	TArray< uint16 >			UnconditionalLifetime;		// Properties that don't need special checks (net initial, role, etc)
	TArray< FLifetimeProperty >	ConditionalLifetime;		// Properties the need to be checked conditionally (based on net initial, role, etc)

	int32						FirstNonCustomParent;
	int32						RoleIndex;
	int32						RemoteRoleIndex;

	UObject *					Owner;						// Either a UCkass or UFunction
};
