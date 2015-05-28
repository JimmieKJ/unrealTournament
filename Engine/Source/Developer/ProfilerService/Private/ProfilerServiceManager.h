// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


#if STATS

/**
 * struct that holds the client information
 */
struct FClientData
{
	/** Connection is active. */
	bool Active;

	/** Connection is previewing. */
	bool Preview;

	/** Default constructor. */
	FClientData()
		: Active( false )
		, Preview( false )
	{}
};

#endif //STATS


/**
 * Implements the Profile Service Manager
 */
class FProfilerServiceManager
	: public TSharedFromThis<FProfilerServiceManager>
	, public IProfilerServiceManager
{
public:

	/**
	 * Default constructor
	 */
	FProfilerServiceManager();

public:

	// Begin IProfilerServiceManager interface
	virtual void StartCapture() override;
	virtual void StopCapture() override;
	// End IProfilerServiceManager interface

	/**
	 * Creates a profiler service manager for shared use
	 */
	static IProfilerServiceManagerPtr CreateSharedServiceManager();


	/**
	 * Initializes the manager
	 */
	void Init();

	/**
	 * Shuts down the manager
	 */
	void Shutdown();

private:
	/**
	 * Changes the data preview state for the given client to the specified value.
	 */
	void SetPreviewState( const FMessageAddress& ClientAddress, const bool bRequestedPreviewState );

	/** Callback for a tick, used to ping the clients */
	bool HandlePing( float DeltaTime );


	// Handles FProfilerServiceCapture messages.
	void HandleServiceCaptureMessage( const FProfilerServiceCapture& Message, const IMessageContextRef& Context );

	// Handles FProfilerServicePong messages.
	void HandleServicePongMessage( const FProfilerServicePong& Message, const IMessageContextRef& Context );

	// Handles FProfilerServicePreview messages.
	void HandleServicePreviewMessage( const FProfilerServicePreview& Message, const IMessageContextRef& Context );

	// Handles FProfilerServiceRequest messages.
	void HandleServiceRequestMessage( const FProfilerServiceRequest& Message, const IMessageContextRef& Context );

	// Handles FProfilerServiceFileChunk messages.
	void HandleServiceFileChunkMessage( const FProfilerServiceFileChunk& Message, const IMessageContextRef& Context );

	// Handles FProfilerServiceSubscribe messages.
	void HandleServiceSubscribeMessage( const FProfilerServiceSubscribe& Message, const IMessageContextRef& Context );

	// Handles FProfilerServiceUnsubscribe messages.
	void HandleServiceUnsubscribeMessage( const FProfilerServiceUnsubscribe& Message, const IMessageContextRef& Context );

	/** Handles a new frame from the stats system. Called from the stats thread. */
	void HandleNewFrame(int64 Frame);

#if STATS
	/** Compresses all stats data and send to the game thread. */
	void CompressDataAndSendToGame( TArray<uint8>* DataToTask, int64 Frame );

	/** Handles a new frame from the stats system. Called from the game thread. */
	void HandleNewFrameGT( FProfilerServiceData2* ToGameThread );
#endif // STATS

	void AddNewFrameHandleStatsThread();

	void RemoveNewFrameHandleStatsThread();

	/** Holds the messaging endpoint. */
	FMessageEndpointPtr MessageEndpoint;

	/** Holds the session and instance identifier. */
	FGuid SessionId;
	FGuid InstanceId;

	/** Holds the message addresses for registered clients */
	TArray<FMessageAddress> PreviewClients;

#if	STATS
	/** Holds the client data for registered clients */
	TMap<FMessageAddress, FClientData> ClientData;
#endif // STATS

	/** Thread used to read, prepare and send file chunks through the message bus. */
	class FFileTransferRunnable* FileTransferRunnable;

	/** Filename of last capture file. */
	FString LastStatsFilename;

	/** Size of the stats metadata. */
	int32 MetadataSize;

	/** Holds a delegate to be invoked for client pings */
	FTickerDelegate PingDelegate;

	/** Handle to the registered PingDelegate */
	FDelegateHandle PingDelegateHandle;

	/** Handle to the registered HandleNewFrame delegate */
	FDelegateHandle NewFrameDelegateHandle;
};
