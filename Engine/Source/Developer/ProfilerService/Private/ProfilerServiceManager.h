// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


/**
 * struct that holds the client information
 */
struct FClientData
{
	/** Connection is active */
	bool Active;
	/** Connection is previewing */
	bool Preview;

#if STATS
	FStatsWriteFile StatsWriteFile;

	/** Current frame of data sent */
	int64 CurrentFrame;

	/** Stats metadata size. */
	int32 MetadataSize;
#endif
};


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

	/**
	 * Default destructor
	 */
	~FProfilerServiceManager();

public:

	// Begin IProfilerServiceManager interface
	virtual void SendData(FProfilerCycleCounter& Data) override;

	virtual void SendData(FProfilerFloatAccumulator& Data) override;

	virtual void SendData(FProfilerCountAccumulator& Data) override;

	virtual void SendData(FProfilerCycleGraph& Data) override;

	virtual bool IsCapturing() const override
	{ 
#if STATS
		return (PreviewClients.Num() > 0 || Archive.IsValid());
#else
		return false;
#endif
	}

	virtual void StartCapture();
	
	virtual void StopCapture();

	virtual void UpdateMetaData() override;

	virtual void StartFrame(uint32 FrameNumber, double FrameStart) override;

	virtual FStatMetaData& GetStatMetaData() override
	{
		return MetaData;
	}

	virtual FProfilerDataDelegate& OnProfilerData() override
	{
		return ProfilerDataDelegate;
	}
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

	void SendMetaData(const FMessageAddress& client);

private:

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

	// Handle a new frame from the stats system
	void HandleNewFrame(int64 Frame);

private:

	/** Holds the messaging endpoint. */
	FMessageEndpointPtr MessageEndpoint;

	/** Holds the session and instance identifier. */
	FGuid SessionId;
	FGuid InstanceId;

	/** Holds the message addresses for registered clients */
	TArray<FMessageAddress> PreviewClients;

	/** Holds the client data for registered clients */
	TMap<FMessageAddress, FClientData> ClientData;

	/** Data writer for recording the data to disk */
#if STATS
	TSharedPtr<FStatsWriteFile, ESPMode::ThreadSafe> Archive;
#endif

	/** Thread used to read, prepare and send file chunks through the message bus. */
	class FFileTransferRunnable* FileTransferRunnable;

	/** Filename of last capture file. */
	FString LastStatsFilename;

	/** Stat meta data */
	FStatMetaData MetaData;
	FStatMetaData NewMetaData;

	/** Delegate for notifying clients of received data */
	FProfilerDataDelegate ProfilerDataDelegate;

	/** Frame of data */
	FProfilerDataFrame DataFrame;

	/** Holds a delegate to be invoked for client pings */
	FTickerDelegate PingDelegate;
};
