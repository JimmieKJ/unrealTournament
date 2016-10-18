// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	DataChannel.cpp: Unreal datachannel implementation.
=============================================================================*/

#include "EnginePrivate.h"
#include "Net/UnrealNetwork.h"
#include "Net/NetworkProfiler.h"
#include "Net/RepLayout.h"
#include "Net/DataReplication.h"
#include "Engine/ActorChannel.h"
#include "Engine/PackageMapClient.h"
#include "Engine/DemoNetDriver.h"

static TAutoConsoleVariable<int32> CVarMaxRPCPerNetUpdate( TEXT( "net.MaxRPCPerNetUpdate" ), 2, TEXT( "Maximum number of RPCs allowed per net update" ) );

class FNetSerializeCB : public INetSerializeCB
{
public:
	FNetSerializeCB() : Driver( NULL ) { check( 0 ); }

	FNetSerializeCB( UNetDriver * InNetDriver ) : Driver( InNetDriver ) { }

	virtual void NetSerializeStruct( UScriptStruct* Struct, FArchive& Ar, UPackageMap* Map, void* Data, bool& bHasUnmapped )
	{
		if (Struct->StructFlags & STRUCT_NetSerializeNative)
		{
			UScriptStruct::ICppStructOps* CppStructOps = Struct->GetCppStructOps();
			check(CppStructOps); // else should not have STRUCT_NetSerializeNative
			check(!Struct->InheritedCppStructOps()); // else should not have STRUCT_NetSerializeNative
			bool bSuccess = true;
			if (!CppStructOps->NetSerialize(Ar, Map, bSuccess, Data))
			{
				bHasUnmapped = true;
			}

			if (!bSuccess)
			{
				UE_LOG(LogRep, Warning, TEXT("NetSerializeStruct: Native NetSerialize %s failed."), *Struct->GetFullName());
			}
		}
		else
		{
			TSharedPtr<FRepLayout> RepLayout = Driver->GetStructRepLayout(Struct);

			UPackageMapClient* PackageMapClient = ( ( UPackageMapClient* )Map );

			if ( PackageMapClient && PackageMapClient->GetConnection()->InternalAck )
			{
				if ( Ar.IsSaving() )
				{
					TArray< uint16 > Changed;
					RepLayout->SendProperties_BackwardsCompatible( nullptr, (uint8*)Data, PackageMapClient->GetConnection(), static_cast< FNetBitWriter& >( Ar ), Changed );
				}
				else
				{
					bool bHasGuidsChanged = false;
					RepLayout->ReceiveProperties_BackwardsCompatible( PackageMapClient->GetConnection(), nullptr, Data, static_cast< FNetBitReader& >( Ar ), bHasUnmapped, false, bHasGuidsChanged );
				}
			}
			else
			{
				RepLayout->SerializePropertiesForStruct( Struct, Ar, Map, Data, bHasUnmapped );
			}
		}
	}

	UNetDriver * Driver;
};

bool FObjectReplicator::SerializeCustomDeltaProperty( UNetConnection * Connection, void* Src, UProperty * Property, uint32 ArrayIndex, FNetBitWriter & OutBunch, TSharedPtr<INetDeltaBaseState> &NewFullState, TSharedPtr<INetDeltaBaseState> & OldState )
{
	check( NewFullState.IsValid() == false ); // NewState is passed in as NULL and instantiated within this function if necessary

	SCOPE_CYCLE_COUNTER( STAT_NetSerializeItemDeltaTime );

	UStructProperty * StructProperty = CastChecked< UStructProperty >( Property );

	//------------------------------------------------
	//	Custom NetDeltaSerialization
	//------------------------------------------------
	if ( !ensure( ( StructProperty->Struct->StructFlags & STRUCT_NetDeltaSerializeNative ) != 0 ) )
	{
		return false;
	}

	FNetDeltaSerializeInfo Parms;

	FNetSerializeCB NetSerializeCB( Connection->Driver );

	Parms.Writer				= &OutBunch;
	Parms.Map					= Connection->PackageMap;
	Parms.OldState				= OldState.Get();
	Parms.NewState				= &NewFullState;
	Parms.NetSerializeCB		= &NetSerializeCB;
	Parms.bIsWritingOnClient	= (Connection->Driver && Connection->Driver->GetWorld()) ? Connection->Driver->GetWorld()->IsRecordingClientReplay() : false;


	UScriptStruct::ICppStructOps * CppStructOps = StructProperty->Struct->GetCppStructOps();

	check(CppStructOps); // else should not have STRUCT_NetSerializeNative
	check(!StructProperty->Struct->InheritedCppStructOps()); // else should not have STRUCT_NetSerializeNative

	Parms.Struct = StructProperty->Struct;

	if ( Property->ArrayDim != 1 )
	{
		OutBunch.SerializeIntPacked( ArrayIndex );
	}

	return CppStructOps->NetDeltaSerialize( Parms, Property->ContainerPtrToValuePtr<void>( Src, ArrayIndex ) );
}

/** 
 *	Utility function to make a copy of the net properties 
 *	@param	Source - Memory to copy initial state from
**/
void FObjectReplicator::InitRecentProperties( uint8* Source )
{
	check( GetObject() != NULL );
	check( Connection != NULL );
	check( RepState == NULL );

	UClass * InObjectClass = GetObject()->GetClass();

	RepState = new FRepState;

	// Initialize the RepState memory
	TSharedPtr<FRepChangedPropertyTracker> RepChangedPropertyTracker = Connection->Driver->FindOrCreateRepChangedPropertyTracker( GetObject() );

	RepLayout->InitRepState( RepState, InObjectClass, Source, RepChangedPropertyTracker );
	RepState->RepLayout = RepLayout;

	// Init custom delta property state
	for ( TFieldIterator<UProperty> It( InObjectClass ); It; ++It )
	{
		if ( It->PropertyFlags & CPF_Net )
		{
			if ( IsCustomDeltaProperty( *It ) )
			{
				// We have to handle dynamic properties of the array individually
				for ( int32 ArrayIdx = 0; ArrayIdx < It->ArrayDim; ++ArrayIdx )
				{
					FOutBunch DeltaState( Connection->PackageMap );
					TSharedPtr<INetDeltaBaseState> & NewState = RecentCustomDeltaState.FindOrAdd(It->RepIndex + ArrayIdx);
					NewState.Reset();					

					TSharedPtr<INetDeltaBaseState> OldState;

					SerializeCustomDeltaProperty( Connection, Source, *It, ArrayIdx, DeltaState, NewState, OldState );

					// Store the initial delta state in case we need it for when we're asked to resend all data since channel was first openeded (bResendAllDataSinceOpen)
					CDOCustomDeltaState.Add( It->RepIndex + ArrayIdx, NewState );
				}
			}
		}
	}
}

/** Takes Data, and compares against shadow state to log differences */
bool FObjectReplicator::ValidateAgainstState( const UObject* ObjectState )
{
	if ( !RepLayout.IsValid() )
	{
		UE_LOG(LogRep, Warning, TEXT("ValidateAgainstState: RepLayout.IsValid() == false"));
		return false;
	}

	if ( RepState == NULL )
	{
		UE_LOG(LogRep, Warning, TEXT("ValidateAgainstState: RepState == NULL"));
		return false;
	}

	if ( RepLayout->DiffProperties( &(RepState->RepNotifies), RepState->StaticBuffer.GetData(), ObjectState, false ) )
	{
		UE_LOG(LogRep, Warning, TEXT("ValidateAgainstState: Properties changed for %s"), *ObjectState->GetName());
		return false;
	}

	return true;
}

void FObjectReplicator::InitWithObject( UObject* InObject, UNetConnection * InConnection, bool bUseDefaultState )
{
	check( GetObject() == NULL );
	check( ObjectClass == NULL );
	check( bLastUpdateEmpty == false );
	check( Connection == NULL );
	check( OwningChannel == NULL );
	check( RepState == NULL );
	check( RemoteFunctions == NULL );
	check( !RepLayout.IsValid() );

	SetObject( InObject );

	if ( GetObject() == NULL )
	{
		// This may seem weird that we're checking for NULL, but the SetObject above will wrap this object with TWeakObjectPtr
		// If the object is pending kill, it will switch to NULL, we're just making sure we handle this invalid edge case
		UE_LOG(LogRep, Error, TEXT("InitWithObject: Object == NULL"));
		return;
	}

	ObjectClass					= InObject->GetClass();
	Connection					= InConnection;
	RemoteFunctions				= NULL;
	bHasReplicatedProperties	= false;
	bOpenAckCalled				= false;
	RepState					= NULL;
	OwningChannel				= NULL;		// Initially NULL until StartReplicating is called
	TrackedGuidMemoryBytes		= 0;

	RepLayout = Connection->Driver->GetObjectClassRepLayout( ObjectClass );

	// Make a copy of the net properties
	uint8* Source = bUseDefaultState ? (uint8*)GetObject()->GetArchetype() : (uint8*)InObject;

	InitRecentProperties( Source );

	RepLayout->GetLifetimeCustomDeltaProperties( LifetimeCustomDeltaProperties, LifetimeCustomDeltaPropertyConditions );
}

void FObjectReplicator::CleanUp()
{
	if ( OwningChannel != NULL )
	{
		StopReplicating( OwningChannel );		// We shouldn't get here, but just in case
	}

	SetObject( NULL );

	ObjectClass					= NULL;
	Connection					= NULL;
	RemoteFunctions				= NULL;
	bHasReplicatedProperties	= false;
	bOpenAckCalled				= false;

	// Cleanup custom delta state
	RecentCustomDeltaState.Empty();

	LifetimeCustomDeltaProperties.Empty();
	LifetimeCustomDeltaPropertyConditions.Empty();

	if ( RepState != NULL )
	{
		delete RepState;
		RepState = NULL;
	}
}

void FObjectReplicator::StartReplicating( class UActorChannel * InActorChannel )
{
	check( OwningChannel == NULL );

	if ( GetObject() == NULL )
	{
		UE_LOG(LogRep, Error, TEXT("StartReplicating: Object == NULL"));
		return;
	}

	OwningChannel = InActorChannel;

	// Cache off netGUID so if this object gets deleted we can close it
	ObjectNetGUID = OwningChannel->Connection->Driver->GuidCache->GetOrAssignNetGUID( GetObject() );
	check( !ObjectNetGUID.IsDefault() && ObjectNetGUID.IsValid() );

	// Allocate retirement list.
	// SetNum now constructs, so this is safe
	Retirement.SetNum( ObjectClass->ClassReps.Num() );

	// figure out list of replicated object properties
	for ( UProperty* Prop = ObjectClass->PropertyLink; Prop != NULL; Prop = Prop->PropertyLinkNext )
	{
		if ( Prop->PropertyFlags & CPF_Net )
		{
			if ( IsCustomDeltaProperty( Prop ) )
			{
				for ( int32 i = 0; i < Prop->ArrayDim; i++ )
				{
					Retirement[Prop->RepIndex + i].CustomDelta = 1;
				}
			}

			if ( Prop->GetPropertyFlags() & CPF_Config )
			{
				for ( int32 i = 0; i < Prop->ArrayDim; i++ )
				{
					Retirement[Prop->RepIndex + i].Config = 1;
				}
			}
		}
	}
}

static FORCEINLINE void ValidateRetirementHistory( const FPropertyRetirement & Retire, const UObject* Object )
{
	checkf( Retire.SanityTag == FPropertyRetirement::ExpectedSanityTag, TEXT( "Invalid Retire.SanityTag. Object: %s" ), Object ? *Object->GetFullName() : TEXT( "NULL" ) );

	FPropertyRetirement * Rec = Retire.Next;	// Note the first element is 'head' that we dont actually use

	FPacketIdRange LastRange;

	while ( Rec != NULL )
	{
		checkf( Rec->SanityTag == FPropertyRetirement::ExpectedSanityTag, TEXT( "Invalid Rec->SanityTag. Object: %s" ), Object ? *Object->GetFullName() : TEXT( "NULL" ) );
		checkf( Rec->OutPacketIdRange.Last >= Rec->OutPacketIdRange.First, TEXT( "Invalid packet id range (Last < First). Object: %s" ), Object ? *Object->GetFullName() : TEXT( "NULL" ) );
		checkf( Rec->OutPacketIdRange.First >= LastRange.Last, TEXT( "Invalid packet id range (First < LastRange.Last). Object: %s" ), Object ? *Object->GetFullName() : TEXT( "NULL" ) );		// Bunch merging and queuing can cause this overlap

		LastRange = Rec->OutPacketIdRange;

		Rec = Rec->Next;
	}
}

void FObjectReplicator::StopReplicating( class UActorChannel * InActorChannel )
{
	check( OwningChannel != NULL );
	check( OwningChannel->Connection == Connection );
	check( OwningChannel == InActorChannel );

	for ( const FNetworkGUID& GUID : ReferencedGuids )
	{
		TSet< FObjectReplicator* >& Replicators = Connection->Driver->GuidToReplicatorMap.FindChecked( GUID );

		Replicators.Remove( this );

		if ( Replicators.Num() == 0 )
		{
			Connection->Driver->GuidToReplicatorMap.Remove( GUID );
		}
	}

	Connection->Driver->UnmappedReplicators.Remove( this );

	ReferencedGuids.Empty();
	
	Connection->Driver->TotalTrackedGuidMemoryBytes -= TrackedGuidMemoryBytes;
	TrackedGuidMemoryBytes = 0;

	OwningChannel = NULL;

	const UObject* Object = GetObject();

	// Cleanup retirement records
	for ( int32 i = Retirement.Num() - 1; i >= 0; i-- )
	{
		ValidateRetirementHistory( Retirement[i], Object );

		FPropertyRetirement * Rec = Retirement[i].Next;
		Retirement[i].Next = NULL;

		// We dont need to explicitly delete Retirement[i], but anything in the Next chain needs to be.
		while ( Rec != NULL )
		{
			FPropertyRetirement * Next = Rec->Next;
			delete Rec;
			Rec = Next;
		}
	}

	Retirement.Empty();

	if ( RemoteFunctions != NULL )
	{
		delete RemoteFunctions;
		RemoteFunctions = NULL;
	}
}

void FObjectReplicator::ReceivedNak( int32 NakPacketId )
{
	const UObject* Object = GetObject();

	if ( Object == NULL )
	{
		UE_LOG(LogNet, Verbose, TEXT("ReceivedNak: Object == NULL"));
		return;
	}

	if ( Object != NULL && ObjectClass != NULL )
	{
		RepLayout->ReceivedNak( RepState, NakPacketId );

		for ( int32 i = Retirement.Num() - 1; i >= 0; i-- )
		{
			ValidateRetirementHistory( Retirement[i], Object );

			// If this is a dynamic array property, we have to look through the list of retirement records to see if we need to reset the base state
			FPropertyRetirement * Rec = Retirement[i].Next; // Retirement[i] is head and not actually used in this case
			while ( Rec != NULL )
			{
				if ( NakPacketId > Rec->OutPacketIdRange.Last )
				{
					// We can assume this means this record's packet was ack'd, so we can get rid of the old state
					check( Retirement[i].Next == Rec );
					Retirement[i].Next = Rec->Next;
					delete Rec;
					Rec = Retirement[i].Next;
					continue;
				}
				else if ( NakPacketId >= Rec->OutPacketIdRange.First && NakPacketId <= Rec->OutPacketIdRange.Last )
				{
					UE_LOG(LogNet, Verbose, TEXT("Restoring Previous Base State of dynamic property Channel %d. NakId: %d  (%d -%d)"), OwningChannel->ChIndex, NakPacketId, Rec->OutPacketIdRange.First, Rec->OutPacketIdRange.Last);

					// The Nack'd packet did update this property, so we need to replace the buffer in RecentDynamic
					// with the buffer we used to create this update (which was dropped), so that the update will be recreated on the next replicate actor
					if ( Rec->DynamicState.IsValid() )
					{
						TSharedPtr<INetDeltaBaseState> & RecentState = RecentCustomDeltaState.FindChecked( i );
						
						RecentState.Reset();
						RecentState = Rec->DynamicState;
					}

					// We can get rid of the rest of the saved off base states since we will be regenerating these updates on the next replicate actor
					while ( Rec != NULL )
					{
						FPropertyRetirement * DeleteNext = Rec->Next;
						delete Rec;
						Rec = DeleteNext;
					}

					// Finished
					Retirement[i].Next = NULL;
					break;				
				}
				Rec = Rec->Next;
			}
				
			ValidateRetirementHistory( Retirement[i], Object );
		}
	}
}

#define HANDLE_INCOMPATIBLE_PROP		\
	if ( bIsServer )					\
	{									\
		return false;					\
	}									\
	FieldCache->bIncompatible = true;	\
	continue;							\

bool FObjectReplicator::ReceivedBunch( FNetBitReader& Bunch, const FReplicationFlags& RepFlags, const bool bHasRepLayout, bool& bOutHasUnmapped )
{
	UObject* Object = GetObject();

	if ( Object == NULL )
	{
		UE_LOG(LogNet, Verbose, TEXT("ReceivedBunch: Object == NULL"));
		return false;
	}

	UPackageMap * PackageMap = OwningChannel->Connection->PackageMap;

	const bool bIsServer = OwningChannel->Connection->Driver->IsServer();

	const FClassNetCache * ClassCache = OwningChannel->Connection->Driver->NetCache->GetClassNetCache( ObjectClass );

	if ( ClassCache == NULL )
	{
		UE_LOG(LogNet, Error, TEXT("ReceivedBunch: ClassCache == NULL: %s"), *Object->GetFullName());
		return false;
	}

	bool bGuidsChanged = false;

	// Handle replayout properties
	if ( bHasRepLayout )
	{
		// Server shouldn't receive properties.
		if ( bIsServer )
		{
			UE_LOG( LogNet, Error, TEXT( "Server received RepLayout properties: %s" ), *Object->GetFullName() );
			return false;
		}

		if ( !bHasReplicatedProperties )
		{
			bHasReplicatedProperties = true;		// Persistent, not reset until PostNetReceive is called
			PreNetReceive();
		}

		const bool bShouldReceiveRepNotifies = Connection->Driver->ShouldReceiveRepNotifiesForObject( Object );

		bool bLocalHasUnmapped = false;

		if ( !RepLayout->ReceiveProperties( OwningChannel, ObjectClass, RepState, ( void* )Object, Bunch, bLocalHasUnmapped, bShouldReceiveRepNotifies, bGuidsChanged ) )
		{
			UE_LOG( LogRep, Error, TEXT( "RepLayout->ReceiveProperties FAILED: %s" ), *Object->GetFullName() );
			return false;
		}

		if ( bLocalHasUnmapped )
		{
			bOutHasUnmapped = true;
		}
	}

	FNetFieldExportGroup* NetFieldExportGroup = OwningChannel->GetNetFieldExportGroupForClassNetCache( ObjectClass );

	FNetBitReader Reader( Bunch.PackageMap );

	// Read fields from stream
	const FFieldNetCache * FieldCache = nullptr;
	
	// Read each property/function blob into Reader (so we've safely jumped over this data in the Bunch/stream at this point)
	while ( OwningChannel->ReadFieldHeaderAndPayload( Object, ClassCache, NetFieldExportGroup, Bunch, &FieldCache, Reader ) )
	{
		if ( Bunch.IsError() )
		{
			UE_LOG( LogNet, Error, TEXT( "ReceivedBunch: Error reading field: %s" ), *Object->GetFullName() );
			return false;
		}

		if ( FieldCache == nullptr )
		{
			UE_LOG( LogNet, Warning, TEXT( "ReceivedBunch: FieldCache == nullptr: %s" ), *Object->GetFullName() );
			continue;
		}

		if ( FieldCache->bIncompatible )
		{
			// We've already warned about this property once, so no need to continue to do so
			UE_LOG( LogNet, Verbose, TEXT( "ReceivedBunch: FieldCache->bIncompatible == true. Object: %s, Field: %s" ), *Object->GetFullName(), *FieldCache->Field->GetFName().ToString() );
			continue;
		}

		// Handle property
		if ( UProperty* ReplicatedProp = Cast< UProperty >( FieldCache->Field ) )
		{
			// Server shouldn't receive properties.
			if ( bIsServer )
			{
				UE_LOG(LogNet, Error, TEXT("Server received unwanted property value %s in %s"), *ReplicatedProp->GetName(), *Object->GetFullName());
				return false;
			}
		
			// We should only be receiving custom delta properties (since RepLayout handles the rest)
			if ( !Retirement[ReplicatedProp->RepIndex].CustomDelta )
			{
				UE_LOG( LogNet, Error, TEXT( "Client received non custom delta property value %s in %s" ), *ReplicatedProp->GetName(), *Object->GetFullName() );
				return false;
			}

			// Call PreNetReceive if we haven't yet
			if ( !bHasReplicatedProperties )
			{
				bHasReplicatedProperties = true;		// Persistent, not reset until PostNetReceive is called
				PreNetReceive();
			}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			{
				static IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("net.Replication.DebugProperty"));
				if (CVar && !CVar->GetString().IsEmpty() && ReplicatedProp->GetName().Contains(CVar->GetString()) )
				{
					UE_LOG(LogRep, Log, TEXT("Replicating Property[%d] %s on %s"), ReplicatedProp->RepIndex, *ReplicatedProp->GetName(), *Object->GetName());
				}
			}
#endif

			// Receive array index (static sized array, i.e. MemberVariable[4])
			uint32 Element = 0;
			if ( ReplicatedProp->ArrayDim != 1 )
			{
				check( ReplicatedProp->ArrayDim >= 2 );

				Reader.SerializeIntPacked( Element );

				if ( Element >= (uint32)ReplicatedProp->ArrayDim )
				{
					UE_LOG(LogRep, Error, TEXT("Element index too large %s in %s"), *ReplicatedProp->GetName(), *Object->GetFullName());
					return false;
				}
			}

			// Pointer to destination.
			uint8* Data = ReplicatedProp->ContainerPtrToValuePtr<uint8>((uint8*)Object, Element);
			TArray<uint8>	MetaData;
			const PTRINT DataOffset = Data - (uint8*)Object;

			// Receive custom delta property.
			UStructProperty * StructProperty = Cast< UStructProperty >( ReplicatedProp );

			if ( StructProperty == NULL )
			{
				// This property isn't custom delta
				UE_LOG(LogRepTraffic, Error, TEXT("Property isn't custom delta %s"), *ReplicatedProp->GetName());
				return false;
			}

			UScriptStruct * InnerStruct = StructProperty->Struct;

			if ( !( InnerStruct->StructFlags & STRUCT_NetDeltaSerializeNative ) )
			{
				// This property isn't custom delta
				UE_LOG(LogRepTraffic, Error, TEXT("Property isn't custom delta %s"), *ReplicatedProp->GetName());
				return false;
			}

			UScriptStruct::ICppStructOps * CppStructOps = InnerStruct->GetCppStructOps();

			check( CppStructOps );
			check( !InnerStruct->InheritedCppStructOps() );

			FNetDeltaSerializeInfo Parms;

			FNetSerializeCB NetSerializeCB( OwningChannel->Connection->Driver );
			
			Parms.DebugName				= StructProperty->GetName();
			Parms.Struct				= InnerStruct;
			Parms.Map					= PackageMap;
			Parms.Reader				= &Reader;
			Parms.NetSerializeCB		= &NetSerializeCB;
			Parms.bIsWritingOnClient	= false;

			// Call the custom delta serialize function to handle it
			CppStructOps->NetDeltaSerialize( Parms, Data );

			if ( Reader.IsError() )
			{
				UE_LOG(LogNet, Error, TEXT("ReceivedBunch: NetDeltaSerialize - Reader.IsError() == true. Property: %s, Object: %s"), *StructProperty->GetName(), *Object->GetFullName());
				HANDLE_INCOMPATIBLE_PROP
			}

			if ( Reader.GetBitsLeft() != 0 )
			{
				UE_LOG( LogNet, Error, TEXT( "ReceivedBunch: NetDeltaSerialize - Mismatch read. Property: %s, Object: %s" ), *StructProperty->GetName(), *Object->GetFullName() );
				HANDLE_INCOMPATIBLE_PROP
			}

			if ( Parms.bOutHasMoreUnmapped )
			{
				UnmappedCustomProperties.Add( DataOffset, StructProperty );
				bOutHasUnmapped = true;
			}

			if ( Parms.bGuidListsChanged )
			{
				bGuidsChanged = true;
			}

			// Successfully received it.
			UE_LOG(LogRepTraffic, Log, TEXT(" %s - %s"), *Object->GetName(), *ReplicatedProp->GetName());

			// Notify the Object if this var is RepNotify
			QueuePropertyRepNotify( Object, ReplicatedProp, Element, MetaData );
		}
		// Handle function call
		else if ( Cast< UFunction >( FieldCache->Field ) )
		{
			FName FunctionName = FieldCache->Field->GetFName();
			UFunction * Function = Object->FindFunction( FunctionName );

			if ( Function == NULL )
			{
				UE_LOG( LogNet, Error, TEXT( "ReceivedBunch: Function not found. Object: %s, Function: %s" ), *Object->GetFullName(), *FunctionName.ToString() );
				HANDLE_INCOMPATIBLE_PROP
			}

			if ( ( Function->FunctionFlags & FUNC_Net ) == 0 )
			{
				UE_LOG( LogRep, Error, TEXT( "Rejected non RPC function. Object: %s, Function: %s" ), *Object->GetFullName(), *FunctionName.ToString() );
				HANDLE_INCOMPATIBLE_PROP
			}

			if ( ( Function->FunctionFlags & ( bIsServer ? FUNC_NetServer : ( FUNC_NetClient | FUNC_NetMulticast ) ) ) == 0 )
			{
				UE_LOG( LogRep, Error, TEXT( "Rejected RPC function due to access rights. Object: %s, Function: %s" ), *Object->GetFullName(), *FunctionName.ToString() );
				HANDLE_INCOMPATIBLE_PROP
			}

			UE_LOG( LogRepTraffic, Log, TEXT( "      Received RPC: %s" ), *FunctionName.ToString() );

			// validate that the function is callable here
			const bool bCanExecute = ( !bIsServer || RepFlags.bNetOwner );		// we are client or net owner

			if ( bCanExecute )
			{
				// Get the parameters.
				FMemMark Mark( FMemStack::Get() );
				uint8* Parms = new( FMemStack::Get(), MEM_Zeroed, Function->ParmsSize )uint8;

				// Use the replication layout to receive the rpc parameter values
				TSharedPtr<FRepLayout> FuncRepLayout = OwningChannel->Connection->Driver->GetFunctionRepLayout( Function );

				FuncRepLayout->ReceivePropertiesForRPC( Object, Function, OwningChannel, Reader, Parms );

				if ( Reader.IsError() )
				{
					UE_LOG( LogRep, Error, TEXT( "ReceivedBunch: ReceivePropertiesForRPC - Reader.IsError() == true: Function: %s, Object: %s" ), *FunctionName.ToString(), *Object->GetFullName() );
					HANDLE_INCOMPATIBLE_PROP
				}

				if ( Reader.GetBitsLeft() != 0 )
				{
					UE_LOG( LogNet, Error, TEXT( "ReceivedBunch: ReceivePropertiesForRPC - Mismatch read. Function: %s, Object: %s" ), *FunctionName.ToString(), *Object->GetFullName() );
					HANDLE_INCOMPATIBLE_PROP
				}

				// Forward the RPC to a client recorded replay, if needed.
				const UWorld* const OwningDriverWorld = OwningChannel->Connection->Driver->World;
				if (OwningDriverWorld && OwningDriverWorld->IsRecordingClientReplay())
				{
					// If Object is not the channel actor, assume the target of the RPC is a subobject.
					UObject* const SubObject = Object != OwningChannel->Actor ? Object : nullptr;
					OwningDriverWorld->DemoNetDriver->ProcessRemoteFunction(OwningChannel->Actor, Function, Parms, nullptr, nullptr, SubObject);
				}

				// Call the function.
				RPC_ResetLastFailedReason();

				Object->ProcessEvent( Function, Parms );

				// Destroy the parameters.
				// warning: highly dependent on UObject::ProcessEvent freeing of parms!
				for ( TFieldIterator<UProperty> It( Function ); It && ( It->PropertyFlags & ( CPF_Parm | CPF_ReturnParm ) ) == CPF_Parm; ++It )
				{
					It->DestroyValue_InContainer( Parms );
				}

				Mark.Pop();

				if ( RPC_GetLastFailedReason() != NULL )
				{
					UE_LOG(LogRep, Error, TEXT("ReceivedBunch: RPC_GetLastFailedReason: %s"), RPC_GetLastFailedReason());
					return false;
				}
			}
			else
			{
				UE_LOG( LogRep, Verbose, TEXT( "Rejected unwanted function %s in %s" ), *FunctionName.ToString(), *Object->GetFullName() );
			}

			if ( Object == NULL || Object->IsPendingKill() )
			{
				// replicated function destroyed Object
				return true;
			}
		}
		else
		{
			UE_LOG( LogRep, Error, TEXT( "ReceivedBunch: Invalid replicated field %i in %s" ), FieldCache->FieldNetIndex, *Object->GetFullName() );
			return false;
		}
	}
	
	// If guids changed, then rebuild acceleration tables
	if ( !bIsServer && bGuidsChanged )
	{
		UpdateGuidToReplicatorMap();
	}

	return true;
}

void FObjectReplicator::UpdateGuidToReplicatorMap()
{
	SCOPE_CYCLE_COUNTER( STAT_NetUpdateGuidToReplicatorMap );

	const bool bIsServer = Connection->Driver->IsServer();

	if ( bIsServer )
	{
		return;
	}

	TSet< FNetworkGUID > LocalReferencedGuids;
	int32 LocalTrackedGuidMemoryBytes = 0;

	// Gather guids on rep layout
	if ( RepLayout.IsValid() && RepState )
	{
		RepLayout->GatherGuidReferences( RepState, LocalReferencedGuids, LocalTrackedGuidMemoryBytes );
	}

	UObject* Object = GetObject();

	// Gather guids on fast tarray
	for ( const int32 CustomIndex : LifetimeCustomDeltaProperties )
	{
		FRepRecord* Rep	= &ObjectClass->ClassReps[CustomIndex];

		UStructProperty* StructProperty = CastChecked< UStructProperty >( Rep->Property );

		FNetDeltaSerializeInfo Parms;

		FNetSerializeCB NetSerializeCB( Connection->Driver );

		Parms.NetSerializeCB			= &NetSerializeCB;
		Parms.GatherGuidReferences		= &LocalReferencedGuids;
		Parms.TrackedGuidMemoryBytes	= &LocalTrackedGuidMemoryBytes;

		UScriptStruct::ICppStructOps* CppStructOps = StructProperty->Struct->GetCppStructOps();

		Parms.Struct = StructProperty->Struct;

		if ( Object != nullptr )
		{
			CppStructOps->NetDeltaSerialize( Parms, StructProperty->ContainerPtrToValuePtr<void>( Object, Rep->Index ) );
		}
	}

	// Go over all referenced guids, and make sure we're tracking them in the GuidToReplicatorMap
	for ( const FNetworkGUID& GUID : LocalReferencedGuids )
	{
		if ( !ReferencedGuids.Contains( GUID ) )
		{
			Connection->Driver->GuidToReplicatorMap.FindOrAdd( GUID ).Add( this );
		}
	}

	// Remove any guids that we were previously tracking but no longer should
	for ( const FNetworkGUID& GUID : ReferencedGuids )
	{
		if ( !LocalReferencedGuids.Contains( GUID ) )
		{
			TSet< FObjectReplicator* >& Replicators = Connection->Driver->GuidToReplicatorMap.FindChecked( GUID );

			Replicators.Remove( this );

			if ( Replicators.Num() == 0 )
			{
				Connection->Driver->GuidToReplicatorMap.Remove( GUID );
			}
		}
	}

	Connection->Driver->TotalTrackedGuidMemoryBytes -= TrackedGuidMemoryBytes;
	TrackedGuidMemoryBytes = LocalTrackedGuidMemoryBytes;
	Connection->Driver->TotalTrackedGuidMemoryBytes += TrackedGuidMemoryBytes;

	ReferencedGuids = MoveTemp( LocalReferencedGuids );
}

bool FObjectReplicator::MoveMappedObjectToUnmapped( const FNetworkGUID& GUID )
{
	bool bFound = false;

	if ( RepLayout.IsValid() )
	{
		if ( RepLayout->MoveMappedObjectToUnmapped( RepState, GUID ) )
		{
			bFound = true;
		}
	}

	UObject* Object = GetObject();

	for ( const int32 CustomIndex : LifetimeCustomDeltaProperties )
	{
		FRepRecord* Rep	= &ObjectClass->ClassReps[CustomIndex];

		UStructProperty* StructProperty = CastChecked< UStructProperty >( Rep->Property );

		FNetDeltaSerializeInfo Parms;

		FNetSerializeCB NetSerializeCB( Connection->Driver );

		Parms.NetSerializeCB			= &NetSerializeCB;
		Parms.MoveGuidToUnmapped		= &GUID;

		UScriptStruct::ICppStructOps* CppStructOps = StructProperty->Struct->GetCppStructOps();

		Parms.Struct = StructProperty->Struct;

		if ( Object != nullptr )
		{
			void* Data = StructProperty->ContainerPtrToValuePtr<void>( Object, Rep->Index );

			if ( CppStructOps->NetDeltaSerialize( Parms, Data ) )
			{
				UnmappedCustomProperties.Add( (uint8*)Data - (uint8*)Object, StructProperty );
				bFound = true;
			}
		}
	}

	return bFound;
}

void FObjectReplicator::PostReceivedBunch()
{
	if ( GetObject() == NULL )
	{
		UE_LOG(LogNet, Verbose, TEXT("PostReceivedBunch: Object == NULL"));
		return;
	}

	// Call PostNetReceive
	const bool bIsServer = (OwningChannel->Connection->Driver->ServerConnection == NULL);
	if (!bIsServer && bHasReplicatedProperties)
	{
		PostNetReceive();
		bHasReplicatedProperties = false;
	}

	// Check if PostNetReceive() destroyed Object
	UObject *Object = GetObject();
	if (Object == NULL || Object->IsPendingKill())
	{
		return;
	}

	// Call RepNotifies
	CallRepNotifies(true);

	if (!Object->IsPendingKill())
	{
		Object->PostRepNotifies();
	}
}

static FORCEINLINE FPropertyRetirement ** UpdateAckedRetirements( FPropertyRetirement &	Retire, const int32 OutAckPacketId, const UObject* Object )
{
	ValidateRetirementHistory( Retire, Object );

	FPropertyRetirement ** Rec = &Retire.Next;	// Note the first element is 'head' that we dont actually use

	while ( *Rec != NULL )
	{
		if ( OutAckPacketId >= (*Rec)->OutPacketIdRange.Last )
		{
			UE_LOG(LogRepTraffic, Verbose, TEXT("Deleting Property Record (%d >= %d)"), OutAckPacketId, (*Rec)->OutPacketIdRange.Last);

			// They've ack'd this packet so we can ditch this record (easier to do it here than look for these every Ack)
			FPropertyRetirement * ToDelete = *Rec;
			check( Retire.Next == ToDelete ); // This should only be able to happen to the first record in the list
			Retire.Next = ToDelete->Next;
			Rec = &Retire.Next;

			delete ToDelete;
			continue;
		}

		Rec = &(*Rec)->Next;
	}

	return Rec;
}

void FObjectReplicator::ReplicateCustomDeltaProperties( FNetBitWriter & Bunch, FReplicationFlags RepFlags )
{
	if ( LifetimeCustomDeltaProperties.Num() == 0 )
	{
		// No custom properties
		return;
	}

	UObject* Object = GetObject();

	check( Object );
	check( OwningChannel );

	UNetConnection * OwningChannelConnection = OwningChannel->Connection;

	// Initialize a map of which conditions are valid

	bool ConditionMap[COND_Max];
	const bool bIsInitial = RepFlags.bNetInitial ? true : false;
	const bool bIsOwner = RepFlags.bNetOwner ? true : false;
	const bool bIsSimulated = RepFlags.bNetSimulated ? true : false;
	const bool bIsPhysics = RepFlags.bRepPhysics ? true : false;
	const bool bIsReplay = RepFlags.bReplay ? true : false;

	ConditionMap[COND_None] = true;
	ConditionMap[COND_InitialOnly] = bIsInitial;
	ConditionMap[COND_OwnerOnly] = bIsOwner;
	ConditionMap[COND_SkipOwner] = !bIsOwner;
	ConditionMap[COND_SimulatedOnly] = bIsSimulated;
	ConditionMap[COND_SimulatedOnlyNoReplay] = bIsSimulated && !bIsReplay;
	ConditionMap[COND_AutonomousOnly] = !bIsSimulated;
	ConditionMap[COND_SimulatedOrPhysics] = bIsSimulated || bIsPhysics;
	ConditionMap[COND_SimulatedOrPhysicsNoReplay] = ( bIsSimulated || bIsPhysics ) && !bIsReplay;
	ConditionMap[COND_InitialOrOwner] = bIsInitial || bIsOwner;
	ConditionMap[COND_Custom] = true;
	ConditionMap[COND_ReplayOrOwner] = bIsReplay || bIsOwner;
	ConditionMap[COND_ReplayOnly] = bIsReplay;

	// Make sure net field export group is registered
	FNetFieldExportGroup* NetFieldExportGroup = OwningChannel->GetOrCreateNetFieldExportGroupForClassNetCache( Object );

	// Replicate those properties.
	for ( int32 i = 0; i < LifetimeCustomDeltaProperties.Num(); i++ )
	{
		// Get info.
		const int32				RetireIndex	= LifetimeCustomDeltaProperties[i];
		FPropertyRetirement &	Retire		= Retirement[RetireIndex];
		FRepRecord *			Rep			= &ObjectClass->ClassReps[RetireIndex];
		UProperty *				It			= Rep->Property;
		int32					Index		= Rep->Index;

		if (LifetimeCustomDeltaPropertyConditions.IsValidIndex(i))
		{
			// Check the replication condition here
			ELifetimeCondition RepCondition = LifetimeCustomDeltaPropertyConditions[i];

			check(RepCondition >= 0 && RepCondition < COND_Max);

			if (!ConditionMap[RepCondition])
			{
				// We didn't pass the condition so don't replicate us
				continue;
			}
		}

		// If this is a dynamic array, we do the delta here
		TSharedPtr<INetDeltaBaseState> NewState;

		FNetBitWriter TempBitWriter( OwningChannel->Connection->PackageMap, 0 );

		if ( Connection->bResendAllDataSinceOpen )
		{
			// If we are resending data since open, we don't want to affect the current state of channel/replication, so just do the minimum and send the data, and return
			// In this case, we'll send all of the properties since the CDO, so use the initial CDO delta state
			TSharedPtr<INetDeltaBaseState>& OldState = CDOCustomDeltaState.FindChecked( RetireIndex );

			if ( SerializeCustomDeltaProperty( OwningChannelConnection, ( void* )Object, It, Index, TempBitWriter, NewState, OldState ) )
			{
				// Write property header and payload to the bunch
				WritePropertyHeaderAndPayload( Object, It, NetFieldExportGroup, Bunch, TempBitWriter );
			}
			continue;
		}

		// Update Retirement records with this new state so we can handle packet drops.
		// LastNext will be pointer to the last "Next" pointer in the list (so pointer to a pointer)
		FPropertyRetirement** LastNext = UpdateAckedRetirements( Retire, OwningChannelConnection->OutAckPacketId, Object );

		check( LastNext != NULL );
		check( *LastNext == NULL );

		ValidateRetirementHistory( Retire, Object );

		TSharedPtr<INetDeltaBaseState>& OldState = RecentCustomDeltaState.FindOrAdd( RetireIndex );

		//-----------------------------------------
		//	Do delta serialization on dynamic properties
		//-----------------------------------------
		const bool WroteSomething = SerializeCustomDeltaProperty( OwningChannelConnection, (void*)Object, It, Index, TempBitWriter, NewState, OldState );

		if ( !WroteSomething )
		{
			continue;
		}

		*LastNext = new FPropertyRetirement();

		// Remember what the old state was at this point in time.  If we get a nak, we will need to revert back to this.
		(*LastNext)->DynamicState = OldState;		

		// Save NewState into the RecentCustomDeltaState array (old state is a reference into our RecentCustomDeltaState map)
		OldState = NewState; 

		// Write property header and payload to the bunch
		WritePropertyHeaderAndPayload( Object, It, NetFieldExportGroup, Bunch, TempBitWriter );

		NETWORK_PROFILER( GNetworkProfiler.TrackReplicateProperty( It, TempBitWriter.GetNumBits(), Connection ) );
	}
}

/** Replicates properties to the Bunch. Returns true if it wrote anything */
bool FObjectReplicator::ReplicateProperties( FOutBunch & Bunch, FReplicationFlags RepFlags )
{
	UObject* Object = GetObject();

	if ( Object == NULL )
	{
		UE_LOG(LogRep, Verbose, TEXT("ReplicateProperties: Object == NULL"));
		return false;
	}

	// some games ship checks() in Shipping so we cannot rely on DO_CHECK here, and these checks are in an extremely hot path
	if (!UE_BUILD_SHIPPING && !UE_BUILD_TEST)
	{
		check( OwningChannel );
		check( RepLayout.IsValid() );
		check( RepState )
		check( RepState->StaticBuffer.Num() );
	}

	UNetConnection* OwningChannelConnection = OwningChannel->Connection;

	FNetBitWriter Writer( Bunch.PackageMap, 0 );

	// Replicate properties in the layout
	const bool bHasRepLayout = RepLayout->ReplicateProperties( RepState, ( uint8* )Object, ObjectClass, OwningChannel, Writer, RepFlags );

	// Replicate all the custom delta properties (fast arrays, etc)
	ReplicateCustomDeltaProperties( Writer, RepFlags );

	if ( OwningChannelConnection->bResendAllDataSinceOpen )
	{
		// If we are resending data since open, we don't want to affect the current state of channel/replication, so just send the data, and return
		const bool WroteImportantData = Writer.GetNumBits() != 0;

		if ( WroteImportantData )
		{
			OwningChannel->WriteContentBlockPayload( Object, Bunch, bHasRepLayout, Writer );
			return true;
		}

		return false;
	}

	// LastUpdateEmpty - this is done before dequeing the multicasted unreliable functions on purpose as they should not prevent
	// an actor channel from going dormant.
	bLastUpdateEmpty = Writer.GetNumBits() == 0;

	// Replicate Queued (unreliable functions)
	if ( RemoteFunctions != NULL && RemoteFunctions->GetNumBits() > 0 )
	{
		static const auto* CVar = IConsoleManager::Get().FindTConsoleVariableDataInt( TEXT( "net.RPC.Debug" ) );

		if ( UNLIKELY( CVar && CVar->GetValueOnGameThread() == 1 ) )
		{
			UE_LOG( LogRepTraffic, Warning,	TEXT("      Sending queued RPCs: %s. Channel[%d] [%.1f bytes]"), *Object->GetName(), OwningChannel->ChIndex, RemoteFunctions->GetNumBits() / 8.f );
		}

		Writer.SerializeBits( RemoteFunctions->GetData(), RemoteFunctions->GetNumBits() );
		RemoteFunctions->Reset();
		RemoteFuncInfo.Empty();

		NETWORK_PROFILER(GNetworkProfiler.FlushQueuedRPCs(OwningChannelConnection, Object));
	}

	// See if we wrote something important (anything but the 'end' int below).
	// Note that queued unreliable functions are considered important (WroteImportantData) but not for bLastUpdateEmpty. LastUpdateEmpty
	// is used for dormancy purposes. WroteImportantData is for determining if we should not include a component in replication.
	const bool WroteImportantData = Writer.GetNumBits() != 0;

	if ( WroteImportantData )
	{
		OwningChannel->WriteContentBlockPayload( Object, Bunch, bHasRepLayout, Writer );
	}

	return WroteImportantData;
}

void FObjectReplicator::ForceRefreshUnreliableProperties()
{
	if ( GetObject() == NULL )
	{
		UE_LOG( LogRep, Verbose, TEXT( "ForceRefreshUnreliableProperties: Object == NULL" ) );
		return;
	}

	check( !bOpenAckCalled );

	RepLayout->OpenAcked( RepState );

	bOpenAckCalled = true;
}

void FObjectReplicator::PostSendBunch( FPacketIdRange & PacketRange, uint8 bReliable )
{
	const UObject* Object = GetObject();

	if ( Object == nullptr )
	{
		UE_LOG(LogNet, Verbose, TEXT("PostSendBunch: Object == NULL"));
		return;
	}

	RepLayout->PostReplicate( RepState, PacketRange, bReliable ? true : false );

	for ( int32 i = 0; i < LifetimeCustomDeltaProperties.Num(); i++ )
	{
		FPropertyRetirement & Retire = Retirement[LifetimeCustomDeltaProperties[i]];

		FPropertyRetirement * Next = Retire.Next;

		while ( Next != NULL )
		{
			// This is updating the dynamic properties retirement record that was created above during property replication
			// (we have to wait until we actually send the bunch to know the packetID, which is why we look for .First==INDEX_NONE)
			if ( Next->OutPacketIdRange.First == INDEX_NONE )
			{
				Next->OutPacketIdRange	= PacketRange;
				Next->Reliable			= bReliable;

				// Mark the last time on this retirement slot that a property actually changed
				Retire.OutPacketIdRange = PacketRange;
				Retire.Reliable			= bReliable;
			}

			Next = Next->Next;
		}

		ValidateRetirementHistory( Retire, Object );
	}
}

void FObjectReplicator::Serialize(FArchive& Ar)
{		
	if (Ar.IsCountingMemory())
	{
		Retirement.CountBytes(Ar);
	}
}

void FObjectReplicator::QueueRemoteFunctionBunch( UFunction* Func, FOutBunch &Bunch )
{
	// This is a pretty basic throttling method - just don't let same func be called more than
	// twice in one network update period.
	//
	// Long term we want to have priorities and stronger cross channel traffic management that
	// can handle this better
	int32 InfoIdx=INDEX_NONE;
	for (int32 i=0; i < RemoteFuncInfo.Num(); ++i)
	{
		if (RemoteFuncInfo[i].FuncName == Func->GetFName())
		{
			InfoIdx = i;
			break;
		}
	}
	if (InfoIdx==INDEX_NONE)
	{
		InfoIdx = RemoteFuncInfo.AddUninitialized();
		RemoteFuncInfo[InfoIdx].FuncName = Func->GetFName();
		RemoteFuncInfo[InfoIdx].Calls = 0;
	}
	
	if (++RemoteFuncInfo[InfoIdx].Calls > CVarMaxRPCPerNetUpdate.GetValueOnGameThread())
	{
		UE_LOG(LogRep, Verbose, TEXT("Too many calls (%d) to RPC %s within a single netupdate. Skipping. %s.  LastCallTime: %.2f. CurrentTime: %.2f. LastRelevantTime: %.2f. LastUpdateTime: %.2f "),
			RemoteFuncInfo[InfoIdx].Calls, *Func->GetName(), *GetPathNameSafe(GetObject()), RemoteFuncInfo[InfoIdx].LastCallTime, OwningChannel->Connection->Driver->Time, OwningChannel->RelevantTime, OwningChannel->LastUpdateTime);
		return;
	}
	
	RemoteFuncInfo[InfoIdx].LastCallTime = OwningChannel->Connection->Driver->Time;

	if (RemoteFunctions == NULL)
	{
		RemoteFunctions = new FOutBunch(OwningChannel, 0);
	}

	RemoteFunctions->SerializeBits( Bunch.GetData(), Bunch.GetNumBits() );

	if ( Connection != NULL && Connection->PackageMap != NULL )
	{
		UPackageMapClient * PackageMapClient = CastChecked< UPackageMapClient >( Connection->PackageMap );

		// We need to copy over any info that was obtained on the package map during serialization, and remember it until we actually call SendBunch
		if ( PackageMapClient->GetMustBeMappedGuidsInLastBunch().Num() )
		{
			OwningChannel->QueuedMustBeMappedGuidsInLastBunch.Append( PackageMapClient->GetMustBeMappedGuidsInLastBunch() );
			PackageMapClient->GetMustBeMappedGuidsInLastBunch().Empty();
		}

		// Copy over any exported bunches
		PackageMapClient->AppendExportBunches( OwningChannel->QueuedExportBunches );
	}
}

bool FObjectReplicator::ReadyForDormancy(bool suppressLogs)
{
	if ( GetObject() == NULL )
	{
		UE_LOG( LogRep, Verbose, TEXT( "ReadyForDormancy: Object == NULL" ) );
		return true;		// Technically, we don't want to hold up dormancy, but the owner needs to clean us up, so we warn
	}

	// Can't go dormant until last update produced no new property updates
	if ( !bLastUpdateEmpty )
	{
		if ( !suppressLogs )
		{
			UE_LOG( LogRepTraffic, Verbose, TEXT( "    [%d] Not ready for dormancy. bLastUpdateEmpty = false" ), OwningChannel->ChIndex );
		}

		return false;
	}

	// Can't go dormant if there are unAckd property updates
	for ( int32 i = 0; i < Retirement.Num(); ++i )
	{
		if ( Retirement[i].Next != NULL )
		{
			if ( !suppressLogs )
			{
				UE_LOG( LogRepTraffic, Verbose, TEXT( "    [%d] OutAckPacketId: %d First: %d Last: %d " ), OwningChannel->ChIndex, OwningChannel->Connection->OutAckPacketId, Retirement[i].OutPacketIdRange.First, Retirement[i].OutPacketIdRange.Last );
			}
			return false;
		}
	}

	return RepLayout->ReadyForDormancy( RepState );
}

void FObjectReplicator::StartBecomingDormant()
{
	if ( GetObject() == NULL )
	{
		UE_LOG( LogRep, Verbose, TEXT( "StartBecomingDormant: Object == NULL" ) );
		return;
	}

	bLastUpdateEmpty = false; // Ensure we get one more attempt to update properties
}

void FObjectReplicator::CallRepNotifies(bool bSkipIfChannelHasQueuedBunches)
{
	UObject* Object = GetObject();

	if ( Object == NULL || Object->IsPendingKill() )
	{
		return;
	}

	if ( Connection != NULL && Connection->Driver != NULL && Connection->Driver->ShouldSkipRepNotifies() )
	{
		return;
	}

	if ( bSkipIfChannelHasQueuedBunches && ( OwningChannel != NULL && OwningChannel->QueuedBunches.Num() > 0 ) )
	{
		return;
	}

	RepLayout->CallRepNotifies( RepState, Object );

	if ( RepNotifies.Num() > 0 )
	{
		for (int32 RepNotifyIdx = 0; RepNotifyIdx < RepNotifies.Num(); RepNotifyIdx++)
		{
			//UE_LOG(LogRep, Log,  TEXT("Calling Object->%s with %s"), *RepNotifies(RepNotifyIdx)->RepNotifyFunc.ToString(), *RepNotifies(RepNotifyIdx)->GetName()); 						
			UProperty* RepProperty = RepNotifies[RepNotifyIdx];
			UFunction* RepNotifyFunc = Object->FindFunction(RepProperty->RepNotifyFunc);

			if (RepNotifyFunc == nullptr)
			{
				UE_LOG(LogRep, Warning, TEXT("FObjectReplicator::CallRepNotifies: Can't find RepNotify function %s for property %s on object %s."),
					*RepProperty->RepNotifyFunc.ToString(), *RepProperty->GetName(), *Object->GetName());
				continue;
			}

			if (RepNotifyFunc->NumParms == 0)
			{
				Object->ProcessEvent(RepNotifyFunc, NULL);
			}
			else if (RepNotifyFunc->NumParms == 1)
			{
				Object->ProcessEvent(RepNotifyFunc, RepProperty->ContainerPtrToValuePtr<uint8>(RepState->StaticBuffer.GetData()) );
			}
			else if (RepNotifyFunc->NumParms == 2)
			{
				// Fixme: this isn't as safe as it could be. Right now we have two types of parameters: MetaData (a TArray<uint8>)
				// and the last local value (pointer into the Recent[] array).
				//
				// Arrays always expect MetaData. Everything else, including structs, expect last value.
				// This is enforced with UHT only. If a ::NetSerialize function ever starts producing a MetaData array thats not in UArrayProperty,
				// we have no static way of catching this and the replication system could pass the wrong thing into ProcessEvent here.
				//
				// But this is all sort of an edge case feature anyways, so its not worth tearing things up too much over.

				FMemMark Mark(FMemStack::Get());
				uint8* Parms = new(FMemStack::Get(),MEM_Zeroed,RepNotifyFunc->ParmsSize)uint8;

				TFieldIterator<UProperty> Itr(RepNotifyFunc);
				check(Itr);

				Itr->CopyCompleteValue( Itr->ContainerPtrToValuePtr<void>(Parms), RepProperty->ContainerPtrToValuePtr<uint8>(RepState->StaticBuffer.GetData()) );
				++Itr;
				check(Itr);

				TArray<uint8> *NotifyMetaData = RepNotifyMetaData.Find(RepNotifies[RepNotifyIdx]);
				check(NotifyMetaData);
				Itr->CopyCompleteValue( Itr->ContainerPtrToValuePtr<void>(Parms), NotifyMetaData );

				Object->ProcessEvent(RepNotifyFunc, Parms );

				Mark.Pop();
			}

			if (Object == NULL || Object->IsPendingKill())
			{
				// script event destroyed Object
				break;
			}
		}
	}

	RepNotifies.Reset();
	RepNotifyMetaData.Empty();
}

void FObjectReplicator::UpdateUnmappedObjects( bool & bOutHasMoreUnmapped )
{
	UObject* Object = GetObject();

	if ( Object == NULL || Object->IsPendingKill() )
	{
		bOutHasMoreUnmapped = false;
		return;
	}

	if ( Connection->State == USOCK_Closed )
	{
		UE_LOG(LogNet, Verbose, TEXT("FObjectReplicator::UpdateUnmappedObjects: Connection->State == USOCK_Closed"));
		return;
	}

	checkf( RepState->RepNotifies.Num() == 0, TEXT("Failed RepState RepNotifies check. Num=%d. Object=%s"), RepState->RepNotifies.Num(), *Object->GetFullName() );
	checkf( RepNotifies.Num() == 0, TEXT("Failed replicator RepNotifies check. Num=%d. Object=%s."), RepNotifies.Num(), *Object->GetFullName() );

	bool bSomeObjectsWereMapped = false;

	// Let the rep layout update any unmapped properties
	RepLayout->UpdateUnmappedObjects( RepState, Connection->PackageMap, Object, bSomeObjectsWereMapped, bOutHasMoreUnmapped );

	// Update unmapped objects for custom properties (currently just fast tarray)
	for ( auto It = UnmappedCustomProperties.CreateIterator(); It; ++It )
	{
		const int32			Offset			= It.Key();
		UStructProperty*	StructProperty	= It.Value();
		UScriptStruct*		InnerStruct		= StructProperty->Struct;

		check( InnerStruct->StructFlags & STRUCT_NetDeltaSerializeNative );

		UScriptStruct::ICppStructOps* CppStructOps = InnerStruct->GetCppStructOps();

		check( CppStructOps );
		check( !InnerStruct->InheritedCppStructOps() );

		FNetDeltaSerializeInfo Parms;

		FNetSerializeCB NetSerializeCB( OwningChannel->Connection->Driver );

		Parms.DebugName			= StructProperty->GetName();
		Parms.Struct			= InnerStruct;
		Parms.Map				= Connection->PackageMap;
		Parms.NetSerializeCB	= &NetSerializeCB;

		Parms.bUpdateUnmappedObjects	= true;
		Parms.bCalledPreNetReceive		= bSomeObjectsWereMapped;	// RepLayout used this to flag whether PreNetReceive was called
		Parms.bIsWritingOnClient		= false;
		Parms.Object					= Object;

		// Call the custom delta serialize function to handle it
		CppStructOps->NetDeltaSerialize( Parms, (uint8*)Object + Offset );

		// Merge in results
		bSomeObjectsWereMapped	|= Parms.bOutSomeObjectsWereMapped;
		bOutHasMoreUnmapped		|= Parms.bOutHasMoreUnmapped;

		if ( Parms.bOutSomeObjectsWereMapped )
		{
			// If we mapped a property, call the rep notify
			TArray<uint8> MetaData;
			QueuePropertyRepNotify( Object, StructProperty, 0, MetaData );
		}

		// If this property no longer has unmapped objects, we can stop checking it
		if ( !Parms.bOutHasMoreUnmapped )
		{
			It.RemoveCurrent();
		}
	}

	// Call any rep notifies that need to happen when object pointers change
	// Pass in false to override the check for queued bunches. Otherwise, if the owning channel has queued bunches,
	// the RepNotifies will remain in the list and the check for 0 RepNotifies above will fail next time.
	CallRepNotifies(false);

	if ( bSomeObjectsWereMapped )
	{
		// If we mapped some objects, make sure to call PostNetReceive (some game code will need to think this was actually replicated to work)
		PostNetReceive();

		UpdateGuidToReplicatorMap();
	}
}

void FObjectReplicator::QueuePropertyRepNotify( UObject* Object, UProperty * Property, const int32 ElementIndex, TArray< uint8 > & MetaData )
{
	if ( !Property->HasAnyPropertyFlags( CPF_RepNotify ) )
	{
		return;
	}

	//@note: AddUniqueItem() here for static arrays since RepNotify() currently doesn't indicate index,
	//			so reporting the same property multiple times is not useful and wastes CPU
	//			were that changed, this should go back to AddItem() for efficiency
	// @todo UE4 - not checking if replicated value is changed from old.  Either fix or document, as may get multiple repnotifies of unacked properties.
	RepNotifies.AddUnique( Property );

	UFunction * RepNotifyFunc = Object->FindFunctionChecked( Property->RepNotifyFunc );

	if ( RepNotifyFunc->NumParms > 0 )
	{
		if ( Property->ArrayDim != 1 )
		{
			// For static arrays, we build the meta data here, but adding the Element index that was just read into the PropMetaData array.
			UE_LOG( LogRepTraffic, Verbose, TEXT("Property %s had ArrayDim: %d change"), *Property->GetName(), ElementIndex );

			// Property is multi dimensional, keep track of what elements changed
			TArray< uint8 > & PropMetaData = RepNotifyMetaData.FindOrAdd( Property );
			PropMetaData.Add( ElementIndex );
		} 
		else if ( MetaData.Num() > 0 )
		{
			// For other properties (TArrays only now) the MetaData array is build within ::NetSerialize. Just add it to the RepNotifyMetaData map here.

			//UE_LOG(LogRepTraffic, Verbose, TEXT("Property %s had MetaData: "), *Property->GetName() );
			//for (auto MetaIt = MetaData.CreateIterator(); MetaIt; ++MetaIt)
			//	UE_LOG(LogRepTraffic, Verbose, TEXT("   %d"), *MetaIt );

			// Property included some meta data about what was serialized. 
			TArray< uint8 > & PropMetaData = RepNotifyMetaData.FindOrAdd( Property );
			PropMetaData = MetaData;
		}
	}
}

void FObjectReplicator::WritePropertyHeaderAndPayload(
	UObject*				Object,
	UProperty*				Property,
	FNetFieldExportGroup*	NetFieldExportGroup,
	FNetBitWriter&			Bunch,
	FNetBitWriter&			Payload ) const
{
	// Get class network info cache.
	const FClassNetCache* ClassCache = Connection->Driver->NetCache->GetClassNetCache( ObjectClass );

	check( ClassCache );

	// Get the network friend property index to replicate
	const FFieldNetCache * FieldCache = ClassCache->GetFromField( Property );

	checkSlow( FieldCache );

	// Send property name and optional array index.
	check( FieldCache->FieldNetIndex <= ClassCache->GetMaxIndex() );

	const int32 HeaderBits = OwningChannel->WriteFieldHeaderAndPayload( Bunch, ClassCache, FieldCache, NetFieldExportGroup, Payload );

	NETWORK_PROFILER( GNetworkProfiler.TrackWritePropertyHeader( Property, HeaderBits, nullptr ) );
}
