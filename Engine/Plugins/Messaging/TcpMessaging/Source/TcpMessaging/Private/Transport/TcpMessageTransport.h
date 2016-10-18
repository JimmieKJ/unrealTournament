// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once


/** Entry specfying addition or removal to/from the NodeConnectionMap */
struct FNodeConnectionMapUpdate
{
	FNodeConnectionMapUpdate(bool bInNewNode, const FGuid& InNodeId, const TWeakPtr<FTcpMessageTransportConnection>& InConnection)
	:	bNewNode(bInNewNode)
	,	NodeId(InNodeId)
	,	Connection(InConnection)
	{}

	FNodeConnectionMapUpdate()
	:	bNewNode(false)
	{}

	bool bNewNode;
	FGuid NodeId;
	TWeakPtr<FTcpMessageTransportConnection> Connection;
};

/**
 * Implements a message transport technology using an Tcp network connection.
 *
 * On platforms that support multiple processes, the transport is using two sockets,
 * one for per-process unicast sending/receiving, and one for multicast receiving.
 * Console and mobile platforms use a single multicast socket for all sending and receiving.
 */
class FTcpMessageTransport
	: FRunnable
	, public IMessageTransport
{
public:

	/**
	 * Creates and initializes a new instance.
	 *
	 */
	FTcpMessageTransport(const FIPv4Endpoint& InListenEndpoint, const TArray<FIPv4Endpoint>& InConnectToEndpoints, int32 InConnectionRetryDelay);

	/** Virtual destructor. */
	virtual ~FTcpMessageTransport();

public:

	// FTcpMessageTransport interface

	void AddOutgoingConnection(const FIPv4Endpoint& Endpoint);

	void RemoveOutgoingConnection(const FIPv4Endpoint& Endpoint);


	//~ IMessageTransport interface

	virtual FName GetDebugName() const override
	{
		return "TcpMessageTransport";
	}

	virtual FOnMessageReceived& OnMessageReceived() override
	{
		return MessageReceivedDelegate;
	}

	virtual FOnNodeDiscovered& OnNodeDiscovered() override
	{
		return NodeDiscoveredDelegate;
	}

	virtual FOnNodeLost& OnNodeLost() override
	{
		return NodeLostDelegate;
	}

	virtual bool StartTransport() override;
	virtual void StopTransport() override;
	virtual bool TransportMessage(const IMessageContextRef& Context, const TArray<FGuid>& Recipients) override;

	//~ FRunnable interface

	virtual bool Init() override;
	virtual uint32 Run() override;
	virtual void Stop() override;
	virtual void Exit() override;

private:

	/** Callback for accepted connections to the local server. */
	bool HandleListenerConnectionAccepted(FSocket* ClientSocket, const FIPv4Endpoint& ClientEndpoint);

	/** Callback from connections for node discovery/loss */
	void HandleConnectionStateChanged(TSharedPtr<FTcpMessageTransportConnection> Connection);

private:

	/** Holds a delegate to be invoked when a message was received on the transport channel. */
	FOnMessageReceived MessageReceivedDelegate;

	/** Holds a delegate to be invoked when a network node was discovered. */
	FOnNodeDiscovered NodeDiscoveredDelegate;

	/** Holds a delegate to be invoked when a network node was lost. */
	FOnNodeLost NodeLostDelegate;

	/** Current settings */
	FIPv4Endpoint ListenEndpoint;
	TArray<FIPv4Endpoint> ConnectToEndpoints;
	int32 ConnectionRetryDelay;

	/** For the thread */
	bool bStopping;
	
	/** Holds a pointer to the socket sub-system. */
	ISocketSubsystem* SocketSubsystem;

	/** Holds the local listener for incoming tunnel connections. */
	FTcpListener* Listener;

	/** Current connections */
	TArray<TSharedPtr<FTcpMessageTransportConnection>> Connections;

	/** Map nodes to connections. We do not transport unicast messages for unknown nodes. */
	TMap<FGuid, TSharedPtr<FTcpMessageTransportConnection>> NodeConnectionMap;

	/** Holds a queue of changes to NodeConnectionMap. */
	TQueue<FNodeConnectionMapUpdate, EQueueMode::Mpsc> NodeConnectionMapUpdates;
	
	/** Holds a queue of pending connections. */
	TQueue<TSharedPtr<FTcpMessageTransportConnection>, EQueueMode::Mpsc> PendingConnections;

	/** Queue of enpoints describing connection we want to remove */
	TQueue<FIPv4Endpoint, EQueueMode::Mpsc> ConnectionEndpointsToRemove;

	/** Holds the thread object. */
	FRunnableThread* Thread;
};
