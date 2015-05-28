// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "LiveEditorListenServerPrivatePCH.h"
#include "Networking.h"
#include "DefaultValueHelper.h"
#include "LiveEditorTransactionHistory.h"

namespace nLiveEditorListenServer
{
	static UProperty *GetPropertyByNameRecurse( UStruct *InStruct, const FString &TokenString, void ** hContainerPtr, int32 &OutArrayIndex )
	{
		FString FirstToken;
		FString RemainingTokens;
		int32 SplitIndex;
		if ( TokenString.FindChar( '.', SplitIndex ) )
		{
			FirstToken = TokenString.LeftChop( TokenString.Len()-SplitIndex );
			RemainingTokens = TokenString.RightChop(SplitIndex+1);
		}
		else
		{
			FirstToken = TokenString;
			RemainingTokens = FString(TEXT(""));
		}

		//get the array index if there is any
		int32 ArrayIndex = 0;
		if ( FirstToken.FindChar( '[', SplitIndex ) )
		{
			FString ArrayIndexString = FirstToken.RightChop( SplitIndex+1 );
			ArrayIndexString = ArrayIndexString.LeftChop( 1 );
			FDefaultValueHelper::ParseInt( ArrayIndexString, ArrayIndex );

			FirstToken = FirstToken.LeftChop( FirstToken.Len()-SplitIndex );
		}

		for (TFieldIterator<UProperty> PropertyIt(InStruct, EFieldIteratorFlags::IncludeSuper); PropertyIt; ++PropertyIt)
		{
			UProperty* Property = *PropertyIt;
			FName PropertyName = Property->GetFName();
			if ( FirstToken == PropertyName.ToString() )
			{
				if ( RemainingTokens.Len() == 0 )
				{
					check( *hContainerPtr != NULL );
					OutArrayIndex = ArrayIndex;
					return Property;
				}
				else
				{
					UStructProperty *StructProp = Cast<UStructProperty>(Property);
					if ( StructProp )
					{
						check( *hContainerPtr != NULL );
						*hContainerPtr = Property->ContainerPtrToValuePtr<void>( *hContainerPtr, ArrayIndex );
						return GetPropertyByNameRecurse( StructProp->Struct, RemainingTokens, hContainerPtr, OutArrayIndex );
					}
				}
			}
		}

		return NULL;
	}
	static UProperty *GetPropertyByName( UObject *Target, UStruct *InStruct, const FString &PropertyName, void ** hContainerPtr, int32 &OutArrayIndex )
	{
		UProperty *Prop = GetPropertyByNameRecurse( Target->GetClass(), PropertyName, hContainerPtr, OutArrayIndex );
		if ( Prop == NULL )
		{
			AActor *AsActor = Cast<AActor>(Target);
			if ( AsActor != NULL )
			{
				FString ComponentPropertyName = PropertyName;
				int32 SplitIndex = 0;
				if ( ComponentPropertyName.FindChar( '.', SplitIndex ) )
				{
					//FString ComponentName = ComponentPropertyName.LeftChop(SplitIndex);
					ComponentPropertyName = ComponentPropertyName.RightChop(SplitIndex+1);

					TInlineComponentArray<UActorComponent*> ActorComponents;
					AsActor->GetComponents(ActorComponents);
					for ( auto ComponentIt = ActorComponents.CreateIterator(); ComponentIt && !Prop; ++ComponentIt )
					{
						UActorComponent *Component = *ComponentIt;
						check( Component != NULL );
						/*
						if ( Component->GetName() != ComponentName )
						{
							continue;
						}
						*/

						*hContainerPtr = Component;
						Prop = GetPropertyByNameRecurse( Component->GetClass(), ComponentPropertyName, hContainerPtr, OutArrayIndex );
					}
				}
			}
		}

		return Prop;
	}

	static void SetPropertyValue( UObject *Target, const FString &PropertyName, const FString &PropertyValue )
	{

		if ( Target == NULL )
			return;

		void *ContainerPtr = Target;
		int32 ArrayIndex;
		UProperty *Prop = nLiveEditorListenServer::GetPropertyByName( Target, Target->GetClass(), PropertyName, &ContainerPtr, ArrayIndex );
		if ( !Prop || !Prop->IsA( UNumericProperty::StaticClass() ) )
		{
			return;
		}

		check( ContainerPtr != NULL );

		if ( Prop->IsA( UByteProperty::StaticClass() ) )
		{
			UByteProperty *NumericProp = CastChecked<UByteProperty>(Prop);
			uint8 Value = FCString::Atoi( *PropertyValue );
			NumericProp->SetPropertyValue_InContainer(ContainerPtr, Value, ArrayIndex);
		}
		else if ( Prop->IsA( UInt8Property::StaticClass() ) )
		{
			UInt8Property *NumericProp = CastChecked<UInt8Property>(Prop);
			int32 Value = FCString::Atoi( *PropertyValue );
			NumericProp->SetPropertyValue_InContainer(ContainerPtr, Value, ArrayIndex);
		}
		else if ( Prop->IsA( UInt16Property::StaticClass() ) )
		{
			UInt16Property *NumericProp = CastChecked<UInt16Property>(Prop);
			int16 Value = FCString::Atoi( *PropertyValue );
			NumericProp->SetPropertyValue_InContainer(ContainerPtr, Value, ArrayIndex);
		}
		else if ( Prop->IsA( UIntProperty::StaticClass() ) )
		{
			UIntProperty *NumericProp = CastChecked<UIntProperty>(Prop);
			int32 Value = FCString::Atoi( *PropertyValue );
			NumericProp->SetPropertyValue_InContainer(ContainerPtr, Value, ArrayIndex);
		}
		else if ( Prop->IsA( UInt64Property::StaticClass() ) )
		{
			UInt64Property *NumericProp = CastChecked<UInt64Property>(Prop);
			int64 Value = FCString::Atoi64( *PropertyValue );
			NumericProp->SetPropertyValue_InContainer(ContainerPtr, Value, ArrayIndex);
		}
		else if ( Prop->IsA( UUInt16Property::StaticClass() ) )
		{
			UUInt16Property *NumericProp = CastChecked<UUInt16Property>(Prop);
			uint16 Value = FCString::Atoi( *PropertyValue );
			NumericProp->SetPropertyValue_InContainer(ContainerPtr, Value, ArrayIndex);
		}
		else if ( Prop->IsA( UUInt32Property::StaticClass() ) )
		{
			UUInt32Property *NumericProp = CastChecked<UUInt32Property>(Prop);
			uint32 Value = FCString::Atoi( *PropertyValue );
			NumericProp->SetPropertyValue_InContainer(ContainerPtr, Value, ArrayIndex);
		}
		else if ( Prop->IsA( UInt64Property::StaticClass() ) )
		{
			UInt64Property *NumericProp = CastChecked<UInt64Property>(Prop);
			uint64 Value = FCString::Atoi64( *PropertyValue );
			NumericProp->SetPropertyValue_InContainer(ContainerPtr, Value, ArrayIndex);
		}
		else if ( Prop->IsA( UFloatProperty::StaticClass() ) )
		{
			UFloatProperty *NumericProp = CastChecked<UFloatProperty>(Prop);
			float Value = FCString::Atof( *PropertyValue );
			NumericProp->SetPropertyValue_InContainer(ContainerPtr, Value, ArrayIndex);
		}
		else if ( Prop->IsA( UDoubleProperty::StaticClass() ) )
		{
			UDoubleProperty *NumericProp = CastChecked<UDoubleProperty>(Prop);
			double Value = FCString::Atod( *PropertyValue );
			NumericProp->SetPropertyValue_InContainer(ContainerPtr, Value, ArrayIndex);
		}
	}
}


DEFINE_LOG_CATEGORY_STATIC(LiveEditorListenServer, Log, All);
#define DEFAULT_LISTEN_ENDPOINT FIPv4Endpoint(FIPv4Address(127, 0, 0, 1), LIVEEDITORLISTENSERVER_DEFAULT_PORT)

IMPLEMENT_MODULE( FLiveEditorListenServer, LiveEditorListenServer )

FLiveEditorListenServer::FLiveEditorListenServer()
:	ObjectCreateListener(NULL),
	ObjectDeleteListener(NULL),
	TickObject(NULL),
	Listener(NULL),
	TransactionHistory(NULL)
{
}

void FLiveEditorListenServer::StartupModule()
{
#if !UE_BUILD_SHIPPING
	//FLiveEditorListenServer does nothing in the Editor
	if ( GIsEditor )
	{
		return;
	}

	TransactionHistory = new FLiveEditorTransactionHistory();

	Listener = new FTcpListener( DEFAULT_LISTEN_ENDPOINT );
	Listener->OnConnectionAccepted().BindRaw(this, &FLiveEditorListenServer::HandleListenerConnectionAccepted);

	InstallHooks();
#endif
}

void FLiveEditorListenServer::ShutdownModule()
{
#if !UE_BUILD_SHIPPING
	RemoveHooks();
#endif
}

void FLiveEditorListenServer::InstallHooks()
{
	ObjectCreateListener = new nLiveEditorListenServer::FCreateListener(this);
	GetUObjectArray().AddUObjectCreateListener(ObjectCreateListener);

	ObjectDeleteListener = new nLiveEditorListenServer::FDeleteListener(this);
	GetUObjectArray().AddUObjectDeleteListener(ObjectDeleteListener);

	TickObject = new nLiveEditorListenServer::FTickObject(this);

	MapLoadObserver = MakeShareable( new nLiveEditorListenServer::FMapLoadObserver(this) );
	FCoreUObjectDelegates::PostLoadMap.AddSP(MapLoadObserver.Get(), &nLiveEditorListenServer::FMapLoadObserver::OnPostLoadMap);
	FCoreUObjectDelegates::PreLoadMap.AddSP(MapLoadObserver.Get(), &nLiveEditorListenServer::FMapLoadObserver::OnPreLoadMap);
}

void FLiveEditorListenServer::RemoveHooks()
{
	if ( Listener )
	{
		Listener->Stop();
		delete Listener;
		Listener = NULL;
	}

	if ( TransactionHistory )
	{
		delete TransactionHistory;
		TransactionHistory = NULL;
	}

	if ( !PendingClients.IsEmpty() )
	{
		FSocket *Client = NULL;
		while( PendingClients.Dequeue(Client) )
		{
			Client->Close();
		}
	}
	for ( TArray<class FSocket*>::TIterator ClientIt(Clients); ClientIt; ++ClientIt )
	{
		(*ClientIt)->Close();
	}

	delete TickObject;
	TickObject = NULL;

	delete ObjectCreateListener;
	ObjectCreateListener = NULL;

	delete ObjectDeleteListener;
	ObjectDeleteListener = NULL;

	FCoreUObjectDelegates::PostLoadMap.RemoveAll(MapLoadObserver.Get());
	FCoreUObjectDelegates::PreLoadMap.RemoveAll(MapLoadObserver.Get());
	MapLoadObserver = NULL;
}

void FLiveEditorListenServer::OnObjectCreation( const class UObjectBase *NewObject )
{
	ObjectCache.OnObjectCreation( NewObject );
}

void FLiveEditorListenServer::OnObjectDeletion( const class UObjectBase *NewObject )
{
	ObjectCache.OnObjectDeletion( NewObject );
}

void FLiveEditorListenServer::OnPreLoadMap()
{
	ObjectCache.EndCache();
}
void FLiveEditorListenServer::OnPostLoadMap()
{
	ObjectCache.StartCache();
}

bool FLiveEditorListenServer::HandleListenerConnectionAccepted( FSocket* ClientSocket, const FIPv4Endpoint& ClientEndpoint )
{
	PendingClients.Enqueue(ClientSocket);
	return true;
}

void FLiveEditorListenServer::Tick( float DeltaTime )
{
#if !UE_BUILD_SHIPPING
	if ( !PendingClients.IsEmpty() )
	{
		FSocket *Client = NULL;
		while( PendingClients.Dequeue(Client) )
		{
			Clients.Add(Client);
		}
	}

	// remove closed connections
	for (int32 ClientIndex = Clients.Num() - 1; ClientIndex >= 0; --ClientIndex)
	{
		if ( Clients[ClientIndex]->GetConnectionState() != SCS_Connected )
		{
			Clients.RemoveAtSwap(ClientIndex);
		}
	}

	//poll for data
	for ( TArray<class FSocket*>::TIterator ClientIt(Clients); ClientIt; ++ClientIt )
	{
		FSocket *Client = *ClientIt;
		uint32 DataSize = 0;
		while ( Client->HasPendingData(DataSize) )
		{
			FArrayReaderPtr Datagram = MakeShareable(new FArrayReader(true));
			Datagram->SetNumUninitialized(FMath::Min(DataSize, 65507u));

			int32 BytesRead = 0;
			if ( Client->Recv(Datagram->GetData(), Datagram->Num(), BytesRead) )
			{
				nLiveEditorListenServer::FNetworkMessage Message;
				*Datagram << Message;

				ReplicateChanges( Message );
				TransactionHistory->AppendTransaction( Message.Payload.ClassName, Message.Payload.PropertyName, Message.Payload.PropertyValue );
			}
		}
	}

	//Check for any newly Tracked objects
	TArray<UObject*> NewTrackedObjects;
	ObjectCache.EvaluatePendingCreations( NewTrackedObjects );

	//If there are any newly tracked objects, stuff them with the list of latest changes we've accumulated so far
	for ( TArray<UObject*>::TIterator NewObjectIt(NewTrackedObjects); NewObjectIt; ++NewObjectIt )
	{
		UObject *NewObject = *NewObjectIt;
		TMap<FString,FString> Transactions;
		TransactionHistory->FindTransactionsForObject( NewObject, Transactions );

		for ( TMap<FString,FString>::TConstIterator TransactionIt(Transactions); TransactionIt; ++TransactionIt )
		{
			const FString &PropertyName = (*TransactionIt).Key;
			const FString &PropertyValue = (*TransactionIt).Value;
			nLiveEditorListenServer::SetPropertyValue( NewObject, PropertyName, PropertyValue );
		}
	}
	
#endif
}

void FLiveEditorListenServer::ReplicateChanges( const nLiveEditorListenServer::FNetworkMessage &Message )
{
	if ( Message.Type == nLiveEditorListenServer::CLASSDEFAULT_OBJECT_PROPERTY )
	{
		//UClass *BaseClass = LoadClass<UObject>( NULL, *Message.Payload.ClassName, NULL, 0, NULL );
		UClass *BaseClass = FindObject<UClass>(ANY_PACKAGE, *Message.Payload.ClassName);
		if ( BaseClass )
		{
			nLiveEditorListenServer::SetPropertyValue( BaseClass->ClassDefaultObject, Message.Payload.PropertyName, Message.Payload.PropertyValue );

			TArray< TWeakObjectPtr<UObject> > Replicants;
			ObjectCache.FindObjectDependants( BaseClass->ClassDefaultObject, Replicants );
			for ( TArray< TWeakObjectPtr<UObject> >::TIterator It(Replicants); It; ++It )
			{
				if ( !(*It).IsValid() )
					continue;

				UObject *Object = (*It).Get();
				check( Object->IsA(BaseClass) );
				nLiveEditorListenServer::SetPropertyValue( Object, Message.Payload.PropertyName, Message.Payload.PropertyValue );
			}
		}
	}
}
