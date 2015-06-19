// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

/**
 * This class wraps the server thread and network connection
 */
class FNetworkFileServer
	: public FRunnable
	, public INetworkFileServer
{
public:

	/**
	 * Creates and initializes a new instance.
	 *
	 * @param InPort The port number to bind to (0 = any available port).
	 * @param InFileRequestDelegate 
	 */
	FNetworkFileServer( int32 InPort, const FFileRequestDelegate* InFileRequestDelegate, 
		const FRecompileShadersDelegate* InRecompileShadersDelegate, const TArray<ITargetPlatform*>& InActiveTargetPlatforms );

	/**
	 * Destructor.
	 */
	~FNetworkFileServer( );

public:

	// FRunnable Interface

	virtual bool Init( ) override
	{
		return true;
	}

	virtual uint32 Run( ) override;

	virtual void Stop( ) override
	{
		StopRequested.Set(true);
	}

	virtual void Exit( ) override;

public:

	// INetworkFileServer interface

	virtual bool IsItReadyToAcceptConnections(void) const override;
	virtual bool GetAddressList(TArray<TSharedPtr<FInternetAddr> >& OutAddresses) const override;
	virtual FString GetSupportedProtocol() const override;
	virtual int32 NumConnections() const override;
	virtual void Shutdown() override;

private:

	// Holds the server (listening) socket.
	FSocket* Socket;

	// Holds the server thread object.
	FRunnableThread* Thread;

	// Holds the list of all client connections.
	TArray< class FNetworkFileServerClientConnectionThreaded*> Connections;

	// Holds a flag indicating whether the thread should stop executing
	FThreadSafeCounter StopRequested;

	// Is the Listner thread up and running. 
	FThreadSafeCounter Running;

public:

	// Holds a delegate to be invoked on every sync request.
	FFileRequestDelegate FileRequestDelegate;

	// Holds a delegate to be invoked when a client requests a shader recompile.
	FRecompileShadersDelegate RecompileShadersDelegate;

	// cached copy of the active target platforms (if any)
	const TArray<ITargetPlatform*> ActiveTargetPlatforms;

	// Holds the address that the server is bound to.
	TSharedPtr<FInternetAddr> ListenAddr;
};
