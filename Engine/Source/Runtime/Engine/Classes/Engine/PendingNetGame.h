// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Engine/EngineBaseTypes.h"
#include "PendingNetGame.generated.h"

struct FWorldContext;
class UEngine;

/**
 * Accepting connection response codes
 */
namespace EAcceptConnection
{
	enum Type
	{
		/** Reject the connection */
		Reject,
		/** Accept the connection */
		Accept,
		/** Ignore the connection, sending no reply, while server traveling */
		Ignore
	};

	/** @return the stringified version of the enum passed in */
	inline const TCHAR* ToString(EAcceptConnection::Type EnumVal)
	{
		switch (EnumVal)
		{
			case Reject:
			{
				return TEXT("Reject");
			}
			case Accept:
			{
				return TEXT("Accept");
			}
			case Ignore:
			{
				return TEXT("Ignore");
			}
		}
		return TEXT("");
	}
};

/**
 * The net code uses this to send notifications.
 */
class FNetworkNotify
{
public:
	/** @todo document */
	virtual EAcceptConnection::Type NotifyAcceptingConnection() PURE_VIRTUAL(FNetworkNotify::NotifyAcceptedConnection,return EAcceptConnection::Ignore;);

	/** @todo document */
	virtual void NotifyAcceptedConnection( class UNetConnection* Connection ) PURE_VIRTUAL(FNetworkNotify::NotifyAcceptedConnection,);

	/** @todo document */
	virtual bool NotifyAcceptingChannel( class UChannel* Channel ) PURE_VIRTUAL(FNetworkNotify::NotifyAcceptingChannel,return false;);

	/** handler for messages sent through a remote connection's control channel
	 * not required to handle the message, but if it reads any data from Bunch, it MUST read the ENTIRE data stream for that message (i.e. use FNetControlMessage<TYPE>::Receive())
	 */
	virtual void NotifyControlMessage(UNetConnection* Connection, uint8 MessageType, class FInBunch& Bunch) PURE_VIRTUAL(FNetworkNotify::NotifyReceivedText,);
};

UCLASS(customConstructor, transient)
class UPendingNetGame :
	public UObject,
	public FNetworkNotify
{
	GENERATED_UCLASS_BODY()

	/** 
	 * Net driver created for contacting the new server
	 * Transferred to world on successful connection
	 */
	UPROPERTY()
	class UNetDriver*		NetDriver;

	/** 
	 * Demo Net driver created for loading demos, but we need to go through pending net game
	 * Transferred to world on successful connection
	 */
	UPROPERTY()
	class UDemoNetDriver*	DemoNetDriver;

public:
	/** URL associated with this level. */
	FURL					URL;

	/** @todo document */
	bool					bSuccessfullyConnected;

	/** @todo document */
	bool					bSentJoinRequest;

	/** @todo document */
	FString					ConnectionError;

	// Constructor.
	void Initialize(const FURL& InURL);

	// Constructor.
	UPendingNetGame(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	void	InitNetDriver();

	//~ Begin FNetworkNotify Interface.
	virtual EAcceptConnection::Type NotifyAcceptingConnection() override;
	virtual void NotifyAcceptedConnection( class UNetConnection* Connection ) override;
	virtual bool NotifyAcceptingChannel( class UChannel* Channel ) override;
	virtual void NotifyControlMessage(UNetConnection* Connection, uint8 MessageType, class FInBunch& Bunch) override;
	//~ End FNetworkNotify Interface.

	/**  Update the pending level's status. */
	virtual void Tick( float DeltaTime );

	/** @todo document */
	virtual UNetDriver* GetNetDriver() { return NetDriver; }

	/** Send JOIN to other end */
	virtual void SendJoin();

	//~ Begin UObject Interface.
	virtual void Serialize( FArchive& Ar ) override;

	virtual void FinishDestroy() override
	{
		NetDriver = NULL;
		
		Super::FinishDestroy();
	}
	
	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);
	//~ End UObject Interface.
	

	/** Create the peer net driver and a socket to listen for new client peer connections. */
	void InitPeerListen();

	/** Called by the engine after it calls LoadMap for this PendingNetGame. */
	virtual void LoadMapCompleted(UEngine* Engine, FWorldContext& Context, bool bLoadedMapSuccessfully, const FString& LoadMapError);
};
