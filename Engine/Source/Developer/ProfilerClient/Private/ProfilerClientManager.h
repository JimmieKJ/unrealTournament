// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

// #YRX_Profiler: 2015-05-13 Remove it later.
#define PROFILER_THREADED_LOAD 1


/** Helper struct containing all of the data and operations associated with a service connection */
struct FServiceConnection
{
	FServiceConnection()
		: DataLoadingProgress( 0.0f )
		, ReadMessages(0)
	{
		MetaData.CriticalSection = &CriticalSection;
	}

	~FServiceConnection()
	{
		MetaData.CriticalSection = NULL;
	}

	FServiceConnection(const FServiceConnection& InConnnection)
	{
		MetaData.CriticalSection = &CriticalSection;
	}

	/** Instance Id */
	FGuid InstanceId;

	/** Service endpoint */
	FMessageAddress ProfilerServiceAddress;

	/** Descriptions for the stats */
	FStatMetaData MetaData;

	/** Current frame worth of data */
	FProfilerDataFrame CurrentData;

	/** Initialize the connection from the given authorization message and context */
	void Initialize( const FProfilerServiceAuthorize& Message, const IMessageContextRef& Context );

#if STATS
	/** Current stats data */
	FStatsThreadState CurrentThreadState;
#endif

	/** Provides an FName to GroupId mapping */
	TMap<FName, int32> GroupNameArray;

	/** Provides the long stat name to StatId mapping. */
	TMap<FName,int32> LongNameToStatID;

#if STATS
	/** Stream reader */
	FStatsReadStream Stream;

	/** Pending stat messages */
	TArray<FStatMessage> Messages;
#endif

	/** Pending Data frames on a load */
	TArray<FProfilerDataFrame> DataFrames;

	/** Current data loading progress. */
	float DataLoadingProgress;

	/** Current number of read messages. */
	uint64 ReadMessages;

	/** Messages received and pending process, they are stored in a map as we can receive them out of order */
	TMap<int64, TArray<uint8> > PendingMessages;

	/** Critical Section needed to synchronize */
	FCriticalSection CriticalSection;

#if STATS
	// generates the old style cycle graph
	void GenerateCycleGraphs(const FRawStatStackNode& Root, TMap<uint32, FProfilerCycleGraph>& Graphs);

	// recursive call to generate the old cycle graph
	void CreateGraphRecursively(const FRawStatStackNode* Root, FProfilerCycleGraph& Graph, uint32 InStartCycles);

	// generates the old style accumulators
	void GenerateAccumulators(TArray<FStatMessage>& Stats, TArray<FProfilerCountAccumulator>& CountAccumulators, TArray<FProfilerFloatAccumulator>& FloatAccumulators);

	// adds a new style stat FName to the list of stats and generates an old style id and description
	int32 FindOrAddStat( const FStatNameAndInfo& StatNameAndInfo, uint32 StatType );

	// adds a new style stat FName to the list of threads and generates an old style id and description
	int32 FindOrAddThread(const FStatNameAndInfo& Thread);

	// updates the meta date for the given instance
	void UpdateMetaData();

	
	/**
	 * Reads stat message from the archive and converts them to be usable by the profiler..
	 *
	 * @return true if read a special marker that indicates end of file
	 */
	bool ReadAndConvertStatMessages( FArchive& Reader, bool bUseInAsync );

	/** Generates a new frame for the profiler. */
	void GenerateNewFrame( FStatMessage Message, FArchive &Reader, bool bUseInAsync );

	/** Adds all collected stat messages to the current stats thread state. */
	void AddCollectedStatMessages( FStatMessage Message );

	/** Generates profiler data frame based on the collected stat messages. */
	void GenerateProfilerDataFrame();
#endif
};

/**
 * Implements the ProfileClient manager.
 */
class FProfilerClientManager
	: public IProfilerClient
{
	class FAsyncReadWorker : public FNonAbandonableTask
	{
	public:
		FServiceConnection* LoadConnection;
		FArchive* FileReader;

		/** Constructor */
		FAsyncReadWorker(FServiceConnection* InConnection, FArchive* InReader)
			: LoadConnection(InConnection)
			, FileReader(InReader)
		{}

		void DoWork();

		FORCEINLINE TStatId GetStatId() const
		{
			RETURN_QUICK_DECLARE_CYCLE_STAT(FAsyncReadWorker, STATGROUP_ThreadPoolAsyncTasks);
		}
	};

public:

	/**
	 * Default constructor
	 *
	 * @param InMessageBus The message bus to use.
	 */
	FProfilerClientManager( const IMessageBusRef& InMessageBus );

	/** Default destructor */
	~FProfilerClientManager();

public:

	// IProfilerClient Interface

	virtual void Subscribe( const FGuid& Session ) override;
	virtual void Track( const FGuid& Instance ) override;
	virtual void Track( const TArray<TSharedPtr<ISessionInstanceInfo>>& Instances ) override;
	virtual void Untrack( const FGuid& Instance ) override;
	virtual void Unsubscribe() override;
	virtual void SetCaptureState( const bool bRequestedCaptureState, const FGuid& InstanceId = FGuid() ) override;
	virtual void SetPreviewState( const bool bRequestedPreviewState, const FGuid& InstanceId = FGuid() ) override;
	virtual void LoadCapture( const FString& DataFilepath, const FGuid& ProfileId ) override;
	virtual void RequestLastCapturedFile( const FGuid& InstanceId = FGuid() ) override;

	virtual const FStatMetaData& GetStatMetaData( const FGuid& InstanceId ) const override
	{
		return Connections.Find(InstanceId)->MetaData;
	}

	virtual FProfilerClientDataDelegate& OnProfilerData() override
	{
		return ProfilerDataDelegate;
	}

	virtual FProfilerFileTransferDelegate& OnProfilerFileTransfer() override
	{
		return ProfilerFileTransferDelegate;
	}

	virtual FProfilerClientConnectedDelegate& OnProfilerClientConnected() override
	{
		return ProfilerClientConnectedDelegate;
	}

	virtual FProfilerClientDisconnectedDelegate& OnProfilerClientDisconnected() override
	{
		return ProfilerClientDisconnectedDelegate;
	}

	virtual FProfilerMetaDataUpdateDelegate& OnMetaDataUpdated() override
	{
		return ProfilerMetaDataUpdatedDelegate;
	}

	virtual FProfilerLoadStartedDelegate& OnLoadStarted() override
	{
		return ProfilerLoadStartedDelegate;
	}

	virtual FProfilerLoadCompletedDelegate& OnLoadCompleted() override
	{

		return ProfilerLoadCompletedDelegate;
	}

	virtual FProfilerLoadedMetaDataDelegate& OnLoadedMetaData() override
	{
		return ProfilerLoadedMetaDataDelegate;
	}

	static const int32 MaxFramesPerTick = 30;

private:

	// Handles message bus shutdowns.
	void HandleMessageBusShutdown();

	// Handles FProfilerServiceAuthorize2 messages.
	void HandleServiceAuthorizeMessage( const FProfilerServiceAuthorize& Message, const IMessageContextRef& Context );

	// Handles FProfilerServiceFileChunk messages.
	void HandleServiceFileChunk( const FProfilerServiceFileChunk& FileChunk, const IMessageContextRef& Context );

	// Handles FProfilerServicePing messages.
	void HandleServicePingMessage( const FProfilerServicePing& Message, const IMessageContextRef& Context );

	// Handles ticker callbacks (used to retry connection with a profiler service).
	bool HandleTicker( float DeltaTime );

	// Handles FProfilerServiceData2 messages.
	void HandleProfilerServiceData2Message( const FProfilerServiceData2& Message, const IMessageContextRef& Context );

	// Handles FProfilerServicePreviewAck messages
	void HandleServicePreviewAckMessage( const FProfilerServicePreviewAck& Messsage, const IMessageContextRef& Context );

	// Handles ticker callbacks for sending out the received messages to subscribed clients
	bool HandleMessagesTicker( float DeltaTime );

	/** If hash is ok, writes the data to the archive and returns true, otherwise only returns false. */
	bool CheckHashAndWrite( const FProfilerServiceFileChunk& FileChunk, const FProfilerFileChunkHeader& FileChunkHeader, FArchive* Writer );

	/** Broadcast that meta data has been updated. */
	void BroadcastMetadataUpdate();

	/** Broadcast that loading has completed and cleans internal structures. */
	void FinalizeLoading();

	/** Decompress all stats data and send to the game thread. */
	void DecompressDataAndSendToGame( FProfilerServiceData2* ToProcess );

	/** Sends decompressed data to the game thread. */
	void SendToGame( TArray<uint8>* DataToGame, int64 Frame, const FGuid InstanceId );

private:

	/** Session this client is currently communicating with */
	FGuid ActiveSessionId;

	/** Session this client is trying to communicate with */
	FGuid PendingSessionId;
	TArray<FGuid> PendingInstances;

	/** Service connections */
	TMap<FGuid, FServiceConnection> Connections;

	struct FReceivedFileInfo
	{
		FReceivedFileInfo( FArchive* InFileWriter, const int64 InProgress, const FString& InDestFilepath)
			: FileWriter( InFileWriter )
			, Progress( InProgress )
			, DestFilepath( InDestFilepath )
			, LastReceivedChunkTime( FPlatformTime::Seconds() )
		{}

		bool IsTimedOut() const
		{
			static const double TimeOut = 15.0f;
			return LastReceivedChunkTime+TimeOut < FPlatformTime::Seconds();
		}

		void Update()
		{
			LastReceivedChunkTime = FPlatformTime::Seconds();
		}

		FArchive* FileWriter;
		int64 Progress;
		FString DestFilepath;
		double LastReceivedChunkTime;
	};

	/** Active transfers, stored as a filename -> received file information. Assumes that filename is unique and never will be the same. */
	TMap<FString,FReceivedFileInfo> ActiveTransfers;
	
	/** List of failed transfers, so discard any new file chunks. */
	TSet<FString> FailedTransfer;

	/** Holds the messaging endpoint. */
	FMessageEndpointPtr MessageEndpoint;

	/** Holds a pointer to the message bus. */
	IMessageBusPtr MessageBus;

	/** Delegate for notifying clients of received data */
	FProfilerClientDataDelegate ProfilerDataDelegate;

	/** Delegate for notifying clients of received data through file transfer. */
	FProfilerFileTransferDelegate ProfilerFileTransferDelegate;

	/** Delegate for notifying clients of a session connection */
	FProfilerClientConnectedDelegate ProfilerClientConnectedDelegate;

	/** Delegate for notifying clients of a session disconnect */
	FProfilerClientDisconnectedDelegate ProfilerClientDisconnectedDelegate;

	/** Delegate for notifying clients of a meta data update */
	FProfilerMetaDataUpdateDelegate ProfilerMetaDataUpdatedDelegate;

	/** Delegate for notifying clients of a load start */
	FProfilerLoadStartedDelegate ProfilerLoadStartedDelegate;

	/** Delegate for notifying clients of a load completion */
	FProfilerLoadCompletedDelegate ProfilerLoadCompletedDelegate;

	/** Delegate for notifying clients of initial meta data load */
	FProfilerLoadedMetaDataDelegate ProfilerLoadedMetaDataDelegate;

	/** Holds a delegate to be invoked when the widget ticks. */
	FTickerDelegate TickDelegate;

	/** Handle to the registered TickDelegate. */
	FDelegateHandle TickDelegateHandle;

	/** Amount of time between connection retries */
	float RetryTime;

	/** Fake Connection used for loading a file */
	FServiceConnection* LoadConnection;

	/** Holds a delegate to be invoked when the widget ticks. */
	FTickerDelegate MessageDelegate;

	/** Handle to the registered MessageDelegate. */
	FDelegateHandle MessageDelegateHandle;

	/** Handle to the registered OnShutdown for Message Bus. */
	FDelegateHandle OnShutdownMessageBusDelegateHandle;

#if PROFILER_THREADED_LOAD
	/** Loads a file asynchronously */
	bool AsyncLoad();

	/** Load Task */
	FAsyncTask<FAsyncReadWorker>* LoadTask;
#else
	/** Loads a file synchronously */
	bool SyncLoad();
	/** File reader */
	FArchive* FileReader;
#endif

	/** Holds the last time a ping was made to instances */
	FDateTime LastPingTime;
};
