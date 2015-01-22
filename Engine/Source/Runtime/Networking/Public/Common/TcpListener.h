// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


/**
 * Delegate type for accepted TCP connections.
 *
 * The first parameter is the socket for the accepted connection.
 * The second parameter is the remote IP endpoint of the accepted connection.
 * The return value indicates whether the connection should be accepted.
 */
DECLARE_DELEGATE_RetVal_TwoParams(bool, FOnTcpListenerConnectionAccepted, FSocket*, const FIPv4Endpoint&)


/**
 * Implements a runnable that listens for incoming TCP connections.
 */
class FTcpListener
	: public FRunnable
{
public:

	/**
	 * Creates and initializes a new instance from the specified IP endpoint.
	 *
	 * @param LocalEndpoint The local IP endpoint to listen on.
	 * @param SleepTime The time to sleep between checking for pending connections (default = 0 seconds).
	 */
	FTcpListener( const FIPv4Endpoint& LocalEndpoint, const FTimespan& InSleepTime = FTimespan::Zero())
		: DeleteSocket(true)
		, Endpoint(LocalEndpoint)
		, SleepTime(InSleepTime)
		, Socket(NULL)
		, Stopping(false)
	{
		Thread = FRunnableThread::Create(this, TEXT("FTcpListener"), 8 * 1024, TPri_Normal);
	}

	/**
	 * Creates and initializes a new instance from the specified socket.
	 *
	 * @param InSocket The socket to listen on.
	 * @param SleepTime The time to sleep between checking for pending connections (default = 0 seconds).
	 */
	FTcpListener( FSocket& InSocket, const FTimespan& InSleepTime = FTimespan::Zero() )
		: DeleteSocket(false)
		, SleepTime(InSleepTime)
		, Socket(&InSocket)
		, Stopping(false)
	{
		TSharedRef<FInternetAddr> LocalAddress = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateInternetAddr();
		Socket->GetAddress(*LocalAddress);
		Endpoint = FIPv4Endpoint(LocalAddress);

		Thread = FRunnableThread::Create(this, TEXT("FTcpListener"), 8 * 1024, TPri_Normal);
	}

	/**
	 * Destructor.
	 */
	~FTcpListener( )
	{
		if (Thread != NULL)
		{
			Thread->Kill(true);
			delete Thread;
		}

		if (DeleteSocket && (Socket != NULL))
		{
			ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(Socket);
			Socket = NULL;
		}
	}

public:

	/**
	 * Gets the listener's local IP endpoint.
	 *
	 * @return IP endpoint.
	 */
	const FIPv4Endpoint& GetLocalEndpoint( ) const
	{
		return Endpoint;
	}

	/**
	 * Gets the listener's network socket.
	 *
	 * @return Network socket.
	 */
	FSocket* GetSocket( ) const
	{
		return Socket;
	}

	/**
	 * Checks whether the listener is listening for incoming connections.
	 *
	 * @return true if it is listening, false otherwise.
	 */
	bool IsActive( ) const
	{
		return ((Socket != NULL) && !Stopping);
	}

public:

	/**
	 * Gets a delegate to be invoked when an incoming connection has been accepted.
	 *
	 * If this delegate is not bound, the listener will reject all incoming connections.
	 * To temporarily disable accepting connections, use the Enable() and Disable() methods.
	 *
	 * @return The delegate.
	 * @see Enable, Disable
	 */
	FOnTcpListenerConnectionAccepted& OnConnectionAccepted( )
	{
		return ConnectionAcceptedDelegate;
	}

public:

	// FRunnable interface

	virtual bool Init( ) override
	{
		if (Socket == NULL)
		{
			Socket = FTcpSocketBuilder(TEXT("FTcpListener server"))
				.AsReusable()
				.BoundToEndpoint(Endpoint)
				.Listening(8);

			int32 NewSize = 0;
			Socket->SetReceiveBufferSize(2*1024*1024, NewSize);
		}

		return (Socket != NULL);
	}

	virtual uint32 Run( ) override
	{
		TSharedRef<FInternetAddr> RemoteAddress = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateInternetAddr();

		while (!Stopping)
		{
			bool Pending;

			// handle incoming connections
			if (Socket->HasPendingConnection(Pending) && Pending)
			{
				FSocket* ConnectionSocket = Socket->Accept(*RemoteAddress, TEXT("FTcpListener client"));

				if (ConnectionSocket != NULL)
				{
					bool Accepted = false;

					if (ConnectionAcceptedDelegate.IsBound())
					{
						Accepted = ConnectionAcceptedDelegate.Execute(ConnectionSocket, FIPv4Endpoint(RemoteAddress));
					}

					if (!Accepted)
					{
						ConnectionSocket->Close();
						ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(ConnectionSocket);
					}
				}
			}

			FPlatformProcess::Sleep(SleepTime.GetSeconds());
		}

		return 0;
	}

	virtual void Stop( ) override
	{
		Stopping = true;
	}

	virtual void Exit( ) override { }

private:

	// Holds a flag indicating whether the socket should be deleted in the destructor.
	bool DeleteSocket;

	// Holds the server endpoint.
	FIPv4Endpoint Endpoint;

	// Holds the time to sleep between checking for pending connections.
	FTimespan SleepTime;

	// Holds the server socket.
	FSocket* Socket;

	// Holds a flag indicating that the thread is stopping.
	bool Stopping;

	// Holds the thread object.
	FRunnableThread* Thread;

private:

	// Holds a delegate to be invoked when an incoming connection has been accepted.
	FOnTcpListenerConnectionAccepted ConnectionAcceptedDelegate;
};