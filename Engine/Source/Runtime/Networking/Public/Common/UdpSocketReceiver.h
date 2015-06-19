// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


/**
 * Temporary fix for concurrency crashes. This whole class will be redesigned.
 */
typedef TSharedPtr<FArrayReader, ESPMode::ThreadSafe> FArrayReaderPtr;

/**
 * Delegate type for received data.
 *
 * The first parameter is the received data.
 * The second parameter is sender's IP endpoint.
 */
DECLARE_DELEGATE_TwoParams(FOnSocketDataReceived, const FArrayReaderPtr&, const FIPv4Endpoint&);


/**
 * Asynchronously receives data from an UDP socket.
 */
class FUdpSocketReceiver
	: public FRunnable
{
public:

	/**
	 * Creates and initializes a new socket receiver.
	 *
	 * @param InSocket - The UDP socket to receive data from.
	 * @param InWaitTime - The amount of time to wait for the socket to be readable.
	 * @param ThreadDescription - The thread description text (for debugging).
	 */
	FUdpSocketReceiver( FSocket* InSocket, const FTimespan& InWaitTime, const TCHAR* ThreadDescription )
		: Socket(InSocket)
		, Stopping(false)
		, WaitTime(InWaitTime)
	{
		check(Socket != NULL);
		check(Socket->GetSocketType() == SOCKTYPE_Datagram);

		SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
		Thread = FRunnableThread::Create(this, ThreadDescription, 128 * 1024, TPri_AboveNormal, FPlatformAffinity::GetPoolThreadMask());
	}

	/**
	 * Destructor.
	 */
	~FUdpSocketReceiver( )
	{
		if (Thread != NULL)
		{
			Thread->Kill(true);
			delete Thread;
		}
	}


public:

	/**
	 * Returns a delegate that is executed when data has been received.
	 *
	 * @return The delegate.
	 */
	FOnSocketDataReceived& OnDataReceived( )
	{
		return DataReceivedDelegate;
	}


public:

	virtual bool Init( ) override
	{
		return true;
	}

	virtual uint32 Run( ) override
	{
		TSharedRef<FInternetAddr> Sender = SocketSubsystem->CreateInternetAddr();

		while (!Stopping)
		{
			if (!Socket->Wait(ESocketWaitConditions::WaitForRead, WaitTime))
			{
				continue;
			}

			uint32 Size;

			while (Socket->HasPendingData(Size))
			{
				FArrayReaderPtr Reader = MakeShareable(new FArrayReader(true));
				Reader->SetNumUninitialized(FMath::Min(Size, 65507u));

				int32 Read = 0;
				Socket->RecvFrom(Reader->GetData(), Reader->Num(), Read, *Sender);

				DataReceivedDelegate.ExecuteIfBound(Reader, FIPv4Endpoint(Sender));
			}
		}

		return 0;
	}

	virtual void Stop( ) override
	{
		Stopping = true;
	}

	virtual void Exit( ) override { }


private:

	// Holds the network socket.
	FSocket* Socket;

	// Holds a pointer to the socket sub-system.
	ISocketSubsystem* SocketSubsystem;

	// Holds a flag indicating that the thread is stopping.
	bool Stopping;

	// Holds the thread object.
	FRunnableThread* Thread;

	// Holds the amount of time to wait for inbound packets.
	FTimespan WaitTime;


private:

	// Holds the data received delegate.
	FOnSocketDataReceived DataReceivedDelegate;
};
