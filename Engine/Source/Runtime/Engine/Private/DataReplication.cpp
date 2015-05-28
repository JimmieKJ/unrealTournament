// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

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
				UE_LOG(LogNet, Warning, TEXT("NetSerializeStruct: Native NetSerialize %s failed."), *Struct->GetFullName());
			}
		}
		else
		{
			TSharedPtr<FRepLayout> RepLayout = Driver->GetStructRepLayout(Struct);
			RepLayout->SerializePropertiesForStruct(Struct, Ar, Map, Data, bHasUnmapped);
		}
	}

	UNetDriver * Driver;
};

bool IsCustomDeltaProperty( UProperty * Property )
{
	UStructProperty * StructProperty = Cast< UStructProperty >( Property );

	if ( StructProperty != NULL && StructProperty->Struct->StructFlags & STRUCT_NetDeltaSerializeNative )
	{
		return true;
	}

	return false;
}

bool FObjectReplicator::SerializeCustomDeltaProperty( UNetConnection * Connection, void* Src, UProperty * Property, int32 ArrayDim, FNetBitWriter & OutBunch, TSharedPtr<INetDeltaBaseState> &NewFullState, TSharedPtr<INetDeltaBaseState> & OldState )
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

	Parms.Writer			= &OutBunch;
	Parms.Map				= Connection->PackageMap;
	Parms.OldState			= OldState.Get();
	Parms.NewState			= &NewFullState;
	Parms.NetSerializeCB	= &NetSerializeCB;

	UScriptStruct::ICppStructOps * CppStructOps = StructProperty->Struct->GetCppStructOps();

	check(CppStructOps); // else should not have STRUCT_NetSerializeNative
	check(!StructProperty->Struct->InheritedCppStructOps()); // else should not have STRUCT_NetSerializeNative

	Parms.Struct = StructProperty->Struct;

	return CppStructOps->NetDeltaSerialize( Parms, Property->ContainerPtrToValuePtr<void>( Src, ArrayDim ) );
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
		UE_LOG( LogNet, Warning, TEXT( "ValidateAgainstState: RepLayout.IsValid() == false" ) );
		return false;
	}

	if ( RepState == NULL )
	{
		UE_LOG( LogNet, Warning, TEXT( "ValidateAgainstState: RepState == NULL" ) );
		return false;
	}

	if ( RepLayout->DiffProperties( RepState, ObjectState, false ) )
	{
		UE_LOG( LogNet, Warning, TEXT( "ValidateAgainstState: Properties changed for %s" ), *ObjectState->GetName() );
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
		UE_LOG( LogNet, Error, TEXT( "InitWithObject: Object == NULL" ) );
		return;
	}

	ObjectClass					= InObject->GetClass();
	Connection					= InConnection;
	RemoteFunctions				= NULL;
	bHasReplicatedProperties	= false;
	bOpenAckCalled				= false;
	RepState					= NULL;
	OwningChannel				= NULL;		// Initially NULL until StartReplicating is called

	RepLayout = Connection->Driver->GetObjectClassRepLayout( ObjectClass );

	// Make a copy of the net properties
	uint8* Source = bUseDefaultState ? (uint8*)GetObject()->GetClass()->GetDefaultObject() : (uint8*)InObject;

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
		UE_LOG( LogNet, Error, TEXT( "StartReplicating: Object == NULL" ) );
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

static FORCEINLINE void ValidateRetirementHistory( FPropertyRetirement & Retire )
{
	FPropertyRetirement * Rec = Retire.Next;	// Note the first element is 'head' that we dont actually use

	FPacketIdRange LastRange;

	while ( Rec != NULL )
	{
		check( Rec->OutPacketIdRange.Last >= Rec->OutPacketIdRange.First );
		check( Rec->OutPacketIdRange.First >= LastRange.Last );		// Bunch merging and queuing can cause this overlap

		LastRange = Rec->OutPacketIdRange;

		Rec = Rec->Next;
	}
}

void FObjectReplicator::StopReplicating( class UActorChannel * InActorChannel )
{
	check( OwningChannel != NULL );
	check( OwningChannel == InActorChannel );

	OwningChannel = NULL;

	// Cleanup retirement records
	for ( int32 i = Retirement.Num() - 1; i >= 0; i-- )
	{
		ValidateRetirementHistory( Retirement[i] );

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
	UObject* Object = GetObject();

	if ( Object == NULL )
	{
		UE_LOG( LogNet, Verbose, TEXT( "ReceivedNak: Object == NULL" ) );
		return;
	}

	if ( Object != NULL && ObjectClass != NULL )
	{
		RepLayout->ReceivedNak( RepState, NakPacketId );

		for ( int32 i = Retirement.Num() - 1; i >= 0; i-- )
		{
			ValidateRetirementHistory( Retirement[i] );

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
					UE_LOG( LogNet, Verbose, TEXT( "Restoring Previous Base State of dynamic property Channel %d. NakId: %d  (%d -%d)" ), OwningChannel->ChIndex, NakPacketId, Rec->OutPacketIdRange.First, Rec->OutPacketIdRange.Last  );

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
				
			ValidateRetirementHistory( Retirement[i] );
		}
	}
}

const FFieldNetCache* FObjectReplicator::ReadField( const FClassNetCache* ClassCache, FInBunch& Bunch ) const
{
	if ( Connection->InternalAck )
	{
		// Replays use checksum rather than index
		uint32 Checksum = 0;
		Bunch << Checksum;

		if ( Bunch.IsError() )
		{
			UE_LOG( LogNet, Error, TEXT( "ReadField: Error reading checksum: %s" ), *GetObject()->GetFullName() );
			return NULL;
		}

		if ( Checksum == 0 )
		{
			return NULL;	// We're done
		}

		const FFieldNetCache * FieldNetCache = ClassCache->GetFromChecksum( Checksum );

		if ( FieldNetCache == NULL )
		{
			UE_LOG( LogNet, Error, TEXT( "ReadField: GetFromChecksum failed: %s" ), *GetObject()->GetFullName() );
			Bunch.SetError();
			return NULL;
		}

		return FieldNetCache;
	}

	const int32 RepIndex = Bunch.ReadInt( ClassCache->GetMaxIndex() + 1 );

	if ( Bunch.IsError() )
	{
		UE_LOG( LogNet, Error, TEXT( "ReadField: Error reading RepIndex: %s" ), *GetObject()->GetFullName() );
		return NULL;
	}

	if ( RepIndex == ClassCache->GetMaxIndex() )
	{
		return NULL;	// We're done
	}

	if ( RepIndex > ClassCache->GetMaxIndex() )
	{
		// We shouldn't be receiving this bunch of this object has no properties or RPC functions to process
		UE_LOG( LogNet, Error, TEXT( "ReadField: RepIndex too large: %s" ), *GetObject()->GetFullName() );
		Bunch.SetError();
		return NULL;
	}
	
	const FFieldNetCache * FieldNetCache = ClassCache->GetFromIndex( RepIndex );

	if ( FieldNetCache == NULL )
	{
		UE_LOG( LogNet, Error, TEXT( "ReadField: GetFromIndex failed: %s" ), *GetObject()->GetFullName() );
		Bunch.SetError();
		return NULL;
	}

	return FieldNetCache;
}

bool FObjectReplicator::ReceivedBunch( FInBunch& Bunch, const FReplicationFlags& RepFlags, bool& bOutHasUnmapped )
{
	UObject* Object = GetObject();

	if ( Object == NULL )
	{
		UE_LOG( LogNet, Verbose, TEXT( "ReceivedBunch: Object == NULL" ) );
		return false;
	}

	UPackageMap * PackageMap = OwningChannel->Connection->PackageMap;

	const bool bIsServer = ( OwningChannel->Connection->Driver->ServerConnection == NULL );

	const FClassNetCache * ClassCache = OwningChannel->Connection->Driver->NetCache->GetClassNetCache( ObjectClass );

	if ( ClassCache == NULL )
	{
		UE_LOG( LogNet, Error, TEXT( "ReceivedBunch: ClassCache == NULL: %s" ), *Object->GetFullName() );
		return false;
	}

	bool bThisBunchReplicatedProperties = false;

	// Read first field
	const FFieldNetCache * FieldCache = ReadField( ClassCache, Bunch );

	if ( Bunch.IsError() )
	{
		UE_LOG( LogNet, Error, TEXT( "ReceivedBunch: Error reading field 1: %s" ), *Object->GetFullName() );
		return false;
	}

	if ( FieldCache == NULL )
	{
		// There are no actual replicated properties or functions in this bunch. That is ok - we may have gotten this
		// actor/sub-object because we want the client to spawn one (but we aren't actually replicating properties on it)
		return true;
	}

	while ( FieldCache )
	{
		// Receive properties from the net.
		UProperty* ReplicatedProp = NULL;

		while ( FieldCache && ( ReplicatedProp = Cast< UProperty >( FieldCache->Field ) ) != NULL )
		{
			NET_CHECKSUM( Bunch );

			// Server shouldn't receive properties.
			if ( bIsServer )
			{
				UE_LOG( LogNet, Error, TEXT( "Server received unwanted property value %s in %s" ), *ReplicatedProp->GetName(), *Object->GetFullName() );
				return false;
			}
		
			bThisBunchReplicatedProperties = true;

			if ( !bHasReplicatedProperties )
			{
				bHasReplicatedProperties = true;		// Persistent, not reset until PostNetReceive is called
				PreNetReceive();
			}

			bool DebugProperty = false;
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			{
				static IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("net.Replication.DebugProperty"));
				if (CVar && !CVar->GetString().IsEmpty() && ReplicatedProp->GetName().Contains(CVar->GetString()) )
				{
					UE_LOG(LogNet, Log, TEXT("Replicating Property[%d] %s on %s"), ReplicatedProp->RepIndex, *ReplicatedProp->GetName(), *Object->GetName());
					DebugProperty = true;
				}
			}
#endif
			if ( !Retirement[ ReplicatedProp->RepIndex ].CustomDelta )
			{
				// We hijack a non custom delta property to signify we are using FRepLayout to read the entire property block
				if ( !RepLayout->ReceiveProperties( ObjectClass, RepState, (void*)Object, Bunch, bOutHasUnmapped ) )
				{
					UE_LOG( LogNet, Error, TEXT( "ReceiveProperties FAILED %s in %s" ), *ReplicatedProp->GetName(), *Object->GetFullName() );
					return false;
				}
			}
			else
			{
				// Receive array index.
				uint32 Element = 0;
				if ( ReplicatedProp->ArrayDim != 1 )
				{
					check( ReplicatedProp->ArrayDim >= 2 );

					Bunch.SerializeIntPacked( Element );

					if ( Element >= (uint32)ReplicatedProp->ArrayDim )
					{
						UE_LOG( LogNet, Error, TEXT( "Element index too large %s in %s" ), *ReplicatedProp->GetName(), *Object->GetFullName() );
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
					UE_LOG( LogNetTraffic, Error, TEXT( "Property isn't custom delta %s" ), *ReplicatedProp->GetName() );
					return false;
				}

				UScriptStruct * InnerStruct = StructProperty->Struct;

				if ( !( InnerStruct->StructFlags & STRUCT_NetDeltaSerializeNative ) )
				{
					// This property isn't custom delta
					UE_LOG( LogNetTraffic, Error, TEXT( "Property isn't custom delta %s" ), *ReplicatedProp->GetName() );
					return false;
				}

				UScriptStruct::ICppStructOps * CppStructOps = InnerStruct->GetCppStructOps();

				check( CppStructOps );
				check( !InnerStruct->InheritedCppStructOps() );

				FNetDeltaSerializeInfo Parms;

				FNetSerializeCB NetSerializeCB( OwningChannel->Connection->Driver );

				Parms.DebugName			= StructProperty->GetName();
				Parms.Struct			= InnerStruct;
				Parms.Map				= PackageMap;
				Parms.Reader			= &Bunch;
				Parms.NetSerializeCB	= &NetSerializeCB;

				// Call the custom delta serialize function to handle it
				CppStructOps->NetDeltaSerialize( Parms, Data );

				if ( Bunch.IsError() )
				{
					UE_LOG( LogNet, Error, TEXT( "ReceivedBunch: NetDeltaSerialize - Bunch.IsError() == true: %s" ), *Object->GetFullName() );
					return false;
				}

				if ( Parms.bOutHasMoreUnmapped )
				{
					UnmappedCustomProperties.Add( DataOffset, StructProperty );
				}

				// Successfully received it.
				UE_LOG( LogNetTraffic, Log, TEXT( " %s - %s" ), *Object->GetName(), *ReplicatedProp->GetName());

				// Notify the Object if this var is RepNotify
				QueuePropertyRepNotify( Object, ReplicatedProp, Element, MetaData );
			}	
			
			// Read next field
			FieldCache = ReadField( ClassCache, Bunch );

			if ( Bunch.IsError() )
			{
				UE_LOG( LogNet, Error, TEXT( "ReceivedBunch: Error reading field 2: %s" ), *Object->GetFullName() );
				return false;
			}
		}

		// Handle function calls.
		if ( FieldCache && Cast< UFunction >( FieldCache->Field ) )
		{
			FName Message = FieldCache->Field->GetFName();
			UFunction * Function = Object->FindFunction( Message );

			if ( Function == NULL )
			{
				UE_LOG( LogNet, Error, TEXT( "ReceivedBunch: Function == NULL: %s" ), *Object->GetFullName() );
				return false;
			}

			if ( ( Function->FunctionFlags & FUNC_Net ) == 0 )
			{
				UE_LOG( LogNet, Error, TEXT( "Rejected non RPC function %s in %s" ), *Message.ToString(), *Object->GetFullName() );
				return false;
			}

			if ( ( Function->FunctionFlags & ( bIsServer ? FUNC_NetServer : ( FUNC_NetClient | FUNC_NetMulticast ) ) ) == 0 )
			{
				UE_LOG( LogNet, Error, TEXT( "Rejected RPC function due to access rights %s in %s" ), *Message.ToString(), *Object->GetFullName() );
				return false;
			}

			UE_LOG( LogNetTraffic, Log, TEXT( "      Received RPC: %s" ), *Message.ToString() );

			// Get the parameters.
			FMemMark Mark(FMemStack::Get());
			uint8* Parms = new(FMemStack::Get(),MEM_Zeroed,Function->ParmsSize)uint8;

			// Use the replication layout to receive the rpc parameter values
			TSharedPtr<FRepLayout> FuncRepLayout = OwningChannel->Connection->Driver->GetFunctionRepLayout( Function );

			FuncRepLayout->ReceivePropertiesForRPC( Object, Function, OwningChannel, Bunch, Parms );

			if ( Bunch.IsError() )
			{
				UE_LOG( LogNet, Error, TEXT( "ReceivedBunch: ReceivePropertiesForRPC - Bunch.IsError() == true: Function: %s, Object: %s" ), *Message.ToString(), *Object->GetFullName() );
				return false;
			}

			// validate that the function is callable here
			const bool bCanExecute = ( !bIsServer || RepFlags.bNetOwner );		// we are client or net owner

			if ( bCanExecute )
			{
				// Call the function.
				RPC_ResetLastFailedReason();

				Object->ProcessEvent( Function, Parms );

				if ( RPC_GetLastFailedReason() != NULL )
				{
					UE_LOG( LogNet, Error, TEXT( "ReceivedBunch: RPC_GetLastFailedReason: %s" ), RPC_GetLastFailedReason() );
					return false;
				}
			}
			else
			{
				UE_LOG( LogNet, Verbose, TEXT( "Rejected unwanted function %s in %s" ), *Message.ToString(), *Object->GetFullName() );

				if ( !OwningChannel->Connection->TrackLogsPerSecond() )	// This will disconnect the client if we get here too often
				{
					UE_LOG( LogNet, Error, TEXT( "Rejected too many unwanted functions %s in %s" ), *Message.ToString(), *Object->GetFullName() );
					return false;
				}
			}

			// Destroy the parameters.
			//warning: highly dependent on UObject::ProcessEvent freeing of parms!
			for ( UProperty * Destruct=Function->DestructorLink; Destruct; Destruct=Destruct->DestructorLinkNext )
			{
				if( Destruct->IsInContainer(Function->ParmsSize) )
				{
					Destruct->DestroyValue_InContainer(Parms);
				}
			}

			Mark.Pop();

			if ( Object == NULL || Object->IsPendingKill() )
			{
				// replicated function destroyed Object
				return true;		// FIXME: Should this return false to kick connection?  Seems we'll cause a read misalignment here if we don't
			}

			// Next.
			FieldCache = ReadField( ClassCache, Bunch );

			if ( Bunch.IsError() )
			{
				UE_LOG( LogNet, Error, TEXT( "ReceivedBunch: Error reading field 3: %s" ), *Object->GetFullName() );
				return false;
			}
		}
		else if ( FieldCache )
		{
			UE_LOG( LogNet, Error, TEXT( "ReceivedBunch: Invalid replicated field %i in %s" ), FieldCache->FieldNetIndex, *Object->GetFullName() );
			return false;
		}
	}

	return true;
}

void FObjectReplicator::PostReceivedBunch()
{
	if ( GetObject() == NULL )
	{
		UE_LOG( LogNet, Verbose, TEXT( "PostReceivedBunch: Object == NULL" ) );
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
	CallRepNotifies();
}

static FORCEINLINE FPropertyRetirement ** UpdateAckedRetirements( FPropertyRetirement &	Retire, int32 OutAckPacketId )
{
	ValidateRetirementHistory( Retire );

	FPropertyRetirement ** Rec = &Retire.Next;	// Note the first element is 'head' that we dont actually use

	while ( *Rec != NULL )
	{
		if ( OutAckPacketId >= (*Rec)->OutPacketIdRange.Last )
		{
			UE_LOG( LogNetTraffic, Verbose, TEXT("Deleting Property Record (%d >= %d)"), OutAckPacketId, (*Rec)->OutPacketIdRange.Last );

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

void FObjectReplicator::ReplicateCustomDeltaProperties( FOutBunch & Bunch, FReplicationFlags RepFlags, bool & bContentBlockWritten )
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

	ConditionMap[COND_None] = true;
	ConditionMap[COND_InitialOnly] = bIsInitial;
	ConditionMap[COND_OwnerOnly] = bIsOwner;
	ConditionMap[COND_SkipOwner] = !bIsOwner;
	ConditionMap[COND_SimulatedOnly] = bIsSimulated;
	ConditionMap[COND_AutonomousOnly] = !bIsSimulated;
	ConditionMap[COND_SimulatedOrPhysics] = bIsSimulated || bIsPhysics;
	ConditionMap[COND_InitialOrOwner] = bIsInitial || bIsOwner;
	ConditionMap[COND_Custom] = true;

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

		const int32 BitsWrittenBeforeThis = Bunch.GetNumBits();

		// If this is a dynamic array, we do the delta here
		TSharedPtr<INetDeltaBaseState> & OldState = RecentCustomDeltaState.FindOrAdd( RetireIndex );
		TSharedPtr<INetDeltaBaseState> NewState;

		// Update Retirement records with this new state so we can handle packet drops.
		// LastNext will be pointer to the last "Next" pointer in the list (so pointer to a pointer)
		FPropertyRetirement ** LastNext = UpdateAckedRetirements( Retire, OwningChannelConnection->OutAckPacketId );

		check( LastNext != NULL );
		check( *LastNext == NULL );

		ValidateRetirementHistory( Retire );

		FNetBitWriter TempBitWriter( OwningChannel->Connection->PackageMap, 0 );

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

		// Write header, and data to send to the actual bunch
		RepLayout->WritePropertyHeader( Object, ObjectClass, OwningChannel, It, Bunch, Index, bContentBlockWritten );

		const int NumStartingBits = Bunch.GetNumBits();

		// Send property.
		Bunch.SerializeBits( TempBitWriter.GetData(), TempBitWriter.GetNumBits() );

		NETWORK_PROFILER(GNetworkProfiler.TrackReplicateProperty(It, Bunch.GetNumBits() - NumStartingBits));
	}
}

/** Replicates properties to the Bunch. Returns true if it wrote anything */
bool FObjectReplicator::ReplicateProperties( FOutBunch & Bunch, FReplicationFlags RepFlags )
{
	UObject* Object = GetObject();

	if ( Object == NULL )
	{
		UE_LOG( LogNet, Verbose, TEXT( "ReplicateProperties: Object == NULL" ) );
		return false;
	}

	check( Object );
	check( OwningChannel );
	check( RepLayout.IsValid() );
	check( RepState )
	check( RepState->StaticBuffer.Num() );

	UNetConnection* OwningChannelConnection = OwningChannel->Connection;

	const int32 StartingBitNum = Bunch.GetNumBits();

	bool bContentBlockWritten = false;

	// Replicate all the custom delta properties (fast arrays, etc)
	ReplicateCustomDeltaProperties( Bunch, RepFlags, bContentBlockWritten );

	// Replicate properties in the layout
	RepLayout->ReplicateProperties( RepState, (uint8*)Object, ObjectClass, OwningChannel, Bunch, RepFlags, bContentBlockWritten );

	// LastUpdateEmpty - this is done before dequeing the multicasted unreliable functions on purpose as they should not prevent
	// an actor channel from going dormant.
	bLastUpdateEmpty = ( Bunch.GetNumBits() == StartingBitNum );

	// Replicate Queued (unreliable functions)
	if ( RemoteFunctions != NULL && RemoteFunctions->GetNumBits() > 0 )
	{
		static const auto* CVar = IConsoleManager::Get().FindTConsoleVariableDataInt( TEXT( "net.RPC.Debug" ) );

		if ( CVar && CVar->GetValueOnGameThread() == 1 )
		{
			UE_LOG( LogNetTraffic, Warning,	TEXT("      Sending queued RPCs: %s. Channel[%d] [%.1f bytes]"), *Object->GetName(), OwningChannel->ChIndex, RemoteFunctions->GetNumBits() / 8.f );
		}

		if ( !bContentBlockWritten )
		{
			OwningChannel->BeginContentBlock( Object, Bunch );
			bContentBlockWritten = true;
		}

		Bunch.SerializeBits( RemoteFunctions->GetData(), RemoteFunctions->GetNumBits() );
		RemoteFunctions->Reset();
		RemoteFuncInfo.Empty();

		NETWORK_PROFILER(GNetworkProfiler.FlushQueuedRPCs(OwningChannelConnection, Object));
	}

	// See if we wrote something important (anything but the 'end' int below).
	// Note that queued unreliable functions are considered important (WroteImportantData) but not for bLastUpdateEmpty. LastUpdateEmpty
	// is used for dormancy purposes. WroteImportantData is for determining if we should not include a component in replication.
	const bool WroteImportantData = ( Bunch.GetNumBits() != StartingBitNum );

	if ( WroteImportantData )
	{
		OwningChannel->EndContentBlock( Object, Bunch, OwningChannelConnection->Driver->NetCache->GetClassNetCache( ObjectClass ) );
	}

	return WroteImportantData;
}

void FObjectReplicator::ForceRefreshUnreliableProperties()
{
	if ( GetObject() == NULL )
	{
		UE_LOG( LogNet, Verbose, TEXT( "ForceRefreshUnreliableProperties: Object == NULL" ) );
		return;
	}

	check( !bOpenAckCalled );

	RepLayout->OpenAcked( RepState );

	bOpenAckCalled = true;
}

void FObjectReplicator::PostSendBunch( FPacketIdRange & PacketRange, uint8 bReliable )
{
	if ( GetObject() == NULL )
	{
		UE_LOG( LogNet, Verbose, TEXT( "PostSendBunch: Object == NULL" ) );
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

		ValidateRetirementHistory( Retire );
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
		UE_LOG(LogNet, Log, TEXT("Too many calls to RPC %s within a single netupdate. Skipping. %s.  LastCallTime: %.2f. CurrentTime: %.2f. LastRelevantTime: %.2f. LastUpdateTime: %.2f "), 
			*Func->GetName(), *GetObject()->GetName(), RemoteFuncInfo[InfoIdx].LastCallTime, OwningChannel->Connection->Driver->Time, OwningChannel->RelevantTime, OwningChannel->LastUpdateTime );
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
		UE_LOG( LogNet, Verbose, TEXT( "ReadyForDormancy: Object == NULL" ) );
		return true;		// Technically, we don't want to hold up dormancy, but the owner needs to clean us up, so we warn
	}

	// Can't go dormant until last update produced no new property updates
	if ( !bLastUpdateEmpty )
	{
		if ( !suppressLogs )
		{
			UE_LOG( LogNetTraffic, Verbose, TEXT( "    [%d] Not ready for dormancy. bLastUpdateEmpty = false" ), OwningChannel->ChIndex );
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
				UE_LOG( LogNetTraffic, Verbose, TEXT( "    [%d] OutAckPacketId: %d First: %d Last: %d " ), OwningChannel->ChIndex, OwningChannel->Connection->OutAckPacketId, Retirement[i].OutPacketIdRange.First, Retirement[i].OutPacketIdRange.Last );
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
		UE_LOG( LogNet, Verbose, TEXT( "StartBecomingDormant: Object == NULL" ) );
		return;
	}

	bLastUpdateEmpty = false; // Ensure we get one more attempt to update properties
}

void FObjectReplicator::CallRepNotifies()
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

	if ( OwningChannel != NULL && OwningChannel->QueuedBunches.Num() > 0 )
	{
		return;
	}

	RepLayout->CallRepNotifies( RepState, Object );

	if ( RepNotifies.Num() > 0 )
	{
		for (int32 RepNotifyIdx = 0; RepNotifyIdx < RepNotifies.Num(); RepNotifyIdx++)
		{
			//UE_LOG(LogNet, Log,  TEXT("Calling Object->%s with %s"), *RepNotifies(RepNotifyIdx)->RepNotifyFunc.ToString(), *RepNotifies(RepNotifyIdx)->GetName()); 						
			UProperty* RepProperty = RepNotifies[RepNotifyIdx];
			UFunction* RepNotifyFunc = Object->FindFunctionChecked(RepProperty->RepNotifyFunc);

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
		UE_LOG( LogNet, Warning, TEXT( "FObjectReplicator::UpdateUnmappedObjects: Connection->State == USOCK_Closed" ) );
		return;
	}

	check( RepState->RepNotifies.Num() == 0 );
	check( RepNotifies.Num() == 0 );

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
	CallRepNotifies();

	if ( bSomeObjectsWereMapped )
	{
		// If we mapped some objects, make sure to call PostNetReceive (some game code will need to think this was actually replicated to work)
		PostNetReceive();
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
			UE_LOG( LogNetTraffic, Verbose, TEXT("Property %s had ArrayDim: %d change"), *Property->GetName(), ElementIndex );

			// Property is multi dimensional, keep track of what elements changed
			TArray< uint8 > & PropMetaData = RepNotifyMetaData.FindOrAdd( Property );
			PropMetaData.Add( ElementIndex );
		} 
		else if ( MetaData.Num() > 0 )
		{
			// For other properties (TArrays only now) the MetaData array is build within ::NetSerialize. Just add it to the RepNotifyMetaData map here.

			//UE_LOG(LogNetTraffic, Verbose, TEXT("Property %s had MetaData: "), *Property->GetName() );
			//for (auto MetaIt = MetaData.CreateIterator(); MetaIt; ++MetaIt)
			//	UE_LOG(LogNetTraffic, Verbose, TEXT("   %d"), *MetaIt );

			// Property included some meta data about what was serialized. 
			TArray< uint8 > & PropMetaData = RepNotifyMetaData.FindOrAdd( Property );
			PropMetaData = MetaData;
		}
	}
}
