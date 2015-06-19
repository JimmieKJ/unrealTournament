// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "ProfilerServicePrivatePCH.h"

#include "StatsData.h"
#include "StatsFile.h"
#include "SecureHash.h"


DEFINE_LOG_CATEGORY_STATIC( LogProfilerService, Log, All );

/** 
 * Thread used to read, prepare and send files through the message bus.
 * Supports resending bad file chunks and basic synchronization between service and client.
 */
class FFileTransferRunnable : public FRunnable
{
	/** Archive used to read a captured stats file. Created on the main thread, destroyed on the runnable thread once finalized. */
	// FArchive* Reader;
	/** Where this file chunk should be sent. */
	// FMessageAddress RecipientAddress;
	typedef TKeyValuePair<FArchive*,FMessageAddress> FReaderAndAddress;

public:	

	/** Default constructor. */
	FFileTransferRunnable( FMessageEndpointPtr& InMessageEndpoint )
		: Runnable( nullptr )
		, WorkEvent( FPlatformProcess::GetSynchEventFromPool( true ) )
		, MessageEndpoint( InMessageEndpoint )
		, StopTaskCounter( 0 )
	{
		Runnable = FRunnableThread::Create(this, TEXT("FFileTransferRunnable"), 128 * 1024, TPri_BelowNormal);
	}

	/** Destructor. */
	~FFileTransferRunnable()
	{
		if( Runnable != nullptr )
		{
			Stop();
			Runnable->WaitForCompletion();
			delete Runnable;
			Runnable = nullptr;
		}

		// Delete all active file readers.
		for( auto It = ActiveTransfers.CreateIterator(); It; ++It )
		{
			FReaderAndAddress& ReaderAndAddress = It.Value();
			DeleteFileReader( ReaderAndAddress );

			UE_LOG( LogProfilerService, Log, TEXT( "File service-client sending aborted (srv): %s" ), *It.Key() );
		}

		FPlatformProcess::ReturnSynchEventToPool( WorkEvent );
		WorkEvent = nullptr;
	}

	// Begin FRunnable interface.
	virtual bool Init()
	{
		return true;
	}

	virtual uint32 Run()
	{
		while( !ShouldStop() )
		{
			if( WorkEvent->Wait( 250 ) )
			{
				FProfilerServiceFileChunk* FileChunk;
				while( !ShouldStop() && SendQueue.Dequeue( FileChunk ) )
				{
					FMemoryReader MemoryReader(FileChunk->Header);
					FProfilerFileChunkHeader FileChunkHeader;
					MemoryReader << FileChunkHeader;
					FileChunkHeader.Validate();

					if( FileChunkHeader.ChunkType == EProfilerFileChunkType::SendChunk )
					{
						// Find the corresponding file archive reader and recipient.
						FArchive* ReaderArchive = nullptr;
						FMessageAddress Recipient;
						{
							FScopeLock Lock(&SyncActiveTransfers);
							const FReaderAndAddress* ReaderAndAddress = ActiveTransfers.Find( FileChunk->Filename );
							if( ReaderAndAddress )
							{
								ReaderArchive = ReaderAndAddress->Key;
								Recipient = ReaderAndAddress->Value;
							}
						}

						// If there is no reader and recipient is invalid, it means that the file transfer is no longer valid, because client disconnected or exited.
						if( ReaderArchive && Recipient.IsValid() )
						{
							ReadAndSetHash( FileChunk, FileChunkHeader, ReaderArchive );

							if( MessageEndpoint.IsValid() )
							{
								MessageEndpoint->Send( FileChunk, Recipient );
							}
						}		
					}
					else if( FileChunkHeader.ChunkType == EProfilerFileChunkType::PrepareFile )
					{
						PrepareFileForSending( FileChunk );
					}
				}

				WorkEvent->Reset();
			}
		}

		return 0;
	}

	virtual void Stop()
	{
		StopTaskCounter.Increment();
	}

	virtual void Exit()
	{}
	// End FRunnable interface

	void EnqueueFileToSend( const FString& StatFilename, const FMessageAddress& RecipientAddress, const FGuid& ServiceInstanceId )
	{
		const FString PathName = FPaths::ProfilingDir() + TEXT("UnrealStats/");
		FString StatFilepath = PathName + StatFilename;

		UE_LOG( LogProfilerService, Log, TEXT( "Opening stats file for service-client sending: %s" ), *StatFilepath );

		const int64 FileSize = IFileManager::Get().FileSize(*StatFilepath);
		if( FileSize < 4 )
		{
			UE_LOG( LogProfilerService, Error, TEXT( "Could not open: %s" ), *StatFilepath );
			return;
		}

		FArchive* FileReader = IFileManager::Get().CreateFileReader(*StatFilepath);
		if( !FileReader )
		{
			UE_LOG( LogProfilerService, Error, TEXT( "Could not open: %s" ), *StatFilepath );
			return;
		}

		{
			FScopeLock Lock(&SyncActiveTransfers);
			check( !ActiveTransfers.Contains( StatFilename ) );
			ActiveTransfers.Add( StatFilename, FReaderAndAddress(FileReader,RecipientAddress) );
		}

		// This is not a real file chunk, but helper to prepare file for sending on the runnable.
		EnqueueFileChunkToSend( new FProfilerServiceFileChunk( ServiceInstanceId,StatFilename,FProfilerFileChunkHeader(0,0,FileReader->TotalSize(),EProfilerFileChunkType::PrepareFile).AsArray() ), true );
	}

	/** Enqueues a file chunk. */
	void EnqueueFileChunkToSend( FProfilerServiceFileChunk* FileChunk, bool bTriggerWorkEvent = false )
	{
		SendQueue.Enqueue( FileChunk );

		if( bTriggerWorkEvent )
		{
			// Trigger the runnable.
			WorkEvent->Trigger();
		}
	}

	/** Prepare the chunks to be sent through the message bus. */
	void PrepareFileForSending( FProfilerServiceFileChunk*& FileChunk )
	{
		// Find the corresponding file archive and recipient.
		FArchive* Reader = nullptr;
		FMessageAddress Recipient;
		{
			FScopeLock Lock(&SyncActiveTransfers);
			const FReaderAndAddress& ReaderAndAddress = ActiveTransfers.FindChecked( FileChunk->Filename );
			Reader = ReaderAndAddress.Key;
			Recipient = ReaderAndAddress.Value;
		}

		int64 ChunkOffset = 0;
		int64 RemainingSizeToSend = Reader->TotalSize();

		while( RemainingSizeToSend > 0 )
		{
			const int64 SizeToCopy = FMath::Min( FProfilerFileChunkHeader::DefChunkSize, RemainingSizeToSend );

			EnqueueFileChunkToSend( new FProfilerServiceFileChunk( FileChunk->InstanceId,FileChunk->Filename,FProfilerFileChunkHeader(ChunkOffset,SizeToCopy,Reader->TotalSize(),EProfilerFileChunkType::SendChunk).AsArray() ) );

			ChunkOffset += SizeToCopy;
			RemainingSizeToSend -= SizeToCopy;
		}

		// Trigger the runnable.
		WorkEvent->Trigger();

		// Delete this file chunk.
		delete FileChunk;
		FileChunk = nullptr;	
	}

	/** Removes file from the list of the active transfers, must be confirmed by the profiler client. */
	void FinalizeFileSending( const FString& Filename )
	{
		FScopeLock Lock(&SyncActiveTransfers);

		check( ActiveTransfers.Contains( Filename ) );
		FReaderAndAddress ReaderAndAddress = ActiveTransfers.FindAndRemoveChecked( Filename );
		DeleteFileReader( ReaderAndAddress );

		UE_LOG( LogProfilerService, Log, TEXT( "File service-client sent successfully : %s" ), *Filename );
	}

	/** Aborts file sending to the specified client, probably client disconnected or exited. */
	void AbortFileSending( const FMessageAddress& Recipient )
	{
		FScopeLock Lock(&SyncActiveTransfers);

		for( auto It = ActiveTransfers.CreateIterator(); It; ++It )
		{
			FReaderAndAddress& ReaderAndAddress = It.Value();
			if( ReaderAndAddress.Value == Recipient )
			{
				UE_LOG( LogProfilerService, Log, TEXT( "File service-client sending aborted (cl): %s" ), *It.Key() );
				FReaderAndAddress ActiveReaderAndAddress = ActiveTransfers.FindAndRemoveChecked( It.Key() );
				DeleteFileReader( ActiveReaderAndAddress );
			}
		}
	}

	/** Checks if there has been any stop requests. */
	FORCEINLINE bool ShouldStop() const
	{
		return StopTaskCounter.GetValue() > 0;
	}

protected:
	/** Deletes the file reader. */
	void DeleteFileReader( FReaderAndAddress& ReaderAndAddress )
	{
		delete ReaderAndAddress.Key;
		ReaderAndAddress.Key = nullptr;
	}

	/** Reads the data from the archive and generates hash. */
	void ReadAndSetHash( FProfilerServiceFileChunk* FileChunk, const FProfilerFileChunkHeader& FileChunkHeader, FArchive* Reader )
	{
		FileChunk->Data.AddUninitialized( FileChunkHeader.ChunkSize );

		Reader->Seek( FileChunkHeader.ChunkOffset );
		Reader->Serialize( FileChunk->Data.GetData(), FileChunkHeader.ChunkSize );

		const int32 HashSize = 20;
		uint8 LocalHash[HashSize]={0};

		// Hash file chunk data. 
		FSHA1 Sha;
		Sha.Update( FileChunk->Data.GetData(), FileChunkHeader.ChunkSize );
		// Hash file chunk header.
		Sha.Update( FileChunk->Header.GetData(), FileChunk->Header.Num() );
		Sha.Final();
		Sha.GetHash( LocalHash );

		FileChunk->ChunkHash.AddUninitialized( HashSize );
		FMemory::Memcpy( FileChunk->ChunkHash.GetData(), LocalHash, HashSize );

		// Limit transfer per second, otherwise we will probably hang the message bus.
		static int64 TotalReadBytes = 0;
#if	_DEBUG
		static const int64 NumBytesPerTick = 128*1024;
#else
		static const int64 NumBytesPerTick = 256*1024;
#endif // _DEBUG

		TotalReadBytes += FileChunkHeader.ChunkSize;
		if( TotalReadBytes > NumBytesPerTick )
		{
			FPlatformProcess::Sleep( 0.1f );
			TotalReadBytes = 0;
		}
	}

	/** Thread that is running this task. */
	FRunnableThread* Runnable;

	/** Event used to signaling that work is available. */
	FEvent* WorkEvent;

	/** Holds the messaging endpoint. */
	FMessageEndpointPtr& MessageEndpoint;

	/** > 0 if we have been asked to abort work in progress at the next opportunity. */
	FThreadSafeCounter StopTaskCounter;

	/** Added on the main thread, processed on the async thread. */
	TQueue<FProfilerServiceFileChunk*,EQueueMode::Mpsc> SendQueue;

	/** Critical section used to synchronize. */
	FCriticalSection SyncActiveTransfers;

	/** Active transfers, stored as a filename -> reader and destination address. Assumes that filename is unique and never will be the same. */
	TMap<FString,FReaderAndAddress> ActiveTransfers;
};

void FProfilerServiceManager::HandleServiceFileChunkMessage( const FProfilerServiceFileChunk& Message, const IMessageContextRef& Context )
{
	FMemoryReader Reader( Message.Header );
	FProfilerFileChunkHeader Header;
	Reader << Header;
	Header.Validate();

	if (Header.ChunkType == EProfilerFileChunkType::SendChunk)
	{
		// Send this file chunk again.
		FileTransferRunnable->EnqueueFileChunkToSend( new FProfilerServiceFileChunk( Message, FProfilerServiceFileChunk::FNullTag() ), true );
	}
	else if (Header.ChunkType == EProfilerFileChunkType::FinalizeFile)
	{
		// Finalize file.
		FileTransferRunnable->FinalizeFileSending( Message.Filename );
	}
}


/* FProfilerServiceManager structors
 *****************************************************************************/

FProfilerServiceManager::FProfilerServiceManager()
	: FileTransferRunnable( nullptr )
	, MetadataSize( 0 )
{
	PingDelegate = FTickerDelegate::CreateRaw(this, &FProfilerServiceManager::HandlePing);
}

/* IProfilerServiceManager interface
 *****************************************************************************/

void FProfilerServiceManager::StartCapture()
{
#if STATS
	DirectStatsCommand(TEXT("stat startfile"));
#endif
}


void FProfilerServiceManager::StopCapture()
{
#if STATS
	DirectStatsCommand(TEXT("stat stopfile"),true);
	// Not thread-safe, but in this case it is ok, because we are waiting for completion.
	LastStatsFilename = FCommandStatsFile::Get().LastFileSaved;
#endif
}

/* FProfilerServiceManager implementation
 *****************************************************************************/

void FProfilerServiceManager::Init()
{
	// get the instance id
	SessionId = FApp::GetSessionId();
	InstanceId = FApp::GetInstanceId();

	// connect to message bus
	MessageEndpoint = FMessageEndpoint::Builder("FProfilerServiceModule")
		.Handling<FProfilerServiceCapture>(this, &FProfilerServiceManager::HandleServiceCaptureMessage)	
		.Handling<FProfilerServicePong>(this, &FProfilerServiceManager::HandleServicePongMessage)
		.Handling<FProfilerServicePreview>(this, &FProfilerServiceManager::HandleServicePreviewMessage)
		.Handling<FProfilerServiceRequest>(this, &FProfilerServiceManager::HandleServiceRequestMessage)
		.Handling<FProfilerServiceFileChunk>(this, &FProfilerServiceManager::HandleServiceFileChunkMessage)
		.Handling<FProfilerServiceSubscribe>(this, &FProfilerServiceManager::HandleServiceSubscribeMessage)
		.Handling<FProfilerServiceUnsubscribe>(this, &FProfilerServiceManager::HandleServiceUnsubscribeMessage);

	if (MessageEndpoint.IsValid())
	{
		MessageEndpoint->Subscribe<FProfilerServiceSubscribe>();
		MessageEndpoint->Subscribe<FProfilerServiceUnsubscribe>();
	}

	FileTransferRunnable = new FFileTransferRunnable( MessageEndpoint );
}


void FProfilerServiceManager::Shutdown()
{
	delete FileTransferRunnable;
	FileTransferRunnable = nullptr;

	MessageEndpoint.Reset();
}


IProfilerServiceManagerPtr FProfilerServiceManager::CreateSharedServiceManager()
{
	static IProfilerServiceManagerPtr ProfilerServiceManager;

	if (!ProfilerServiceManager.IsValid())
	{
		ProfilerServiceManager = MakeShareable(new FProfilerServiceManager());
	}

	return ProfilerServiceManager;
}

void FProfilerServiceManager::AddNewFrameHandleStatsThread()
{
#if	STATS
	const FStatsThreadState& Stats = FStatsThreadState::GetLocalState();
	NewFrameDelegateHandle = Stats.NewFrameDelegate.AddRaw( this, &FProfilerServiceManager::HandleNewFrame );
	StatsMasterEnableAdd();
	MetadataSize = 0;
#endif // STATS
}

void FProfilerServiceManager::RemoveNewFrameHandleStatsThread()
{
#if	STATS
	const FStatsThreadState& Stats = FStatsThreadState::GetLocalState();
	Stats.NewFrameDelegate.Remove( NewFrameDelegateHandle );
	StatsMasterEnableSubtract();
	MetadataSize = 0;
#endif // STATS
}

void FProfilerServiceManager::SetPreviewState( const FMessageAddress& ClientAddress, const bool bRequestedPreviewState )
{
#if STATS
	FClientData* Client = ClientData.Find( ClientAddress );
	if (MessageEndpoint.IsValid() && Client)
	{
		const bool bIsPreviewing = Client->Preview;

		if( bRequestedPreviewState != bIsPreviewing )
		{
			if( bRequestedPreviewState )
			{
				// Enable stat capture.
				if (PreviewClients.Num() == 0)
				{
					FSimpleDelegateGraphTask::CreateAndDispatchWhenReady
					(
						FSimpleDelegateGraphTask::FDelegate::CreateRaw( this, &FProfilerServiceManager::AddNewFrameHandleStatsThread ),
						TStatId(), nullptr,
						FPlatformProcess::SupportsMultithreading() ? ENamedThreads::StatsThread : ENamedThreads::GameThread
					);
				}
				PreviewClients.Add(ClientAddress);
				Client->Preview = true;

				MessageEndpoint->Send( new FProfilerServicePreviewAck( InstanceId ), ClientAddress );
			}
			else
			{
				PreviewClients.Remove(ClientAddress);
				Client->Preview = false;

				// Disable stat capture.
				if (PreviewClients.Num() == 0)
				{
					FSimpleDelegateGraphTask::CreateAndDispatchWhenReady
					(
						FSimpleDelegateGraphTask::FDelegate::CreateRaw( this, &FProfilerServiceManager::RemoveNewFrameHandleStatsThread ),
						TStatId(), nullptr,
						FPlatformProcess::SupportsMultithreading() ? ENamedThreads::StatsThread : ENamedThreads::GameThread
					);
					
				}	
			}
		}

		UE_LOG( LogProfilerService, Verbose, TEXT( "SetPreviewState: %i, InstanceId: %s, ClientAddress: %s" ), (int32)bRequestedPreviewState, *InstanceId.ToString(), *ClientAddress.ToString() );
	}
#endif
}


/* FProfilerServiceManager callbacks
 *****************************************************************************/

bool FProfilerServiceManager::HandlePing( float DeltaTime )
{
#if STATS
	// check the active flags and reset if true, remove the client if false
	TArray<FMessageAddress> Clients;
	for (auto Iter = ClientData.CreateIterator(); Iter; ++Iter)
	{
		FMessageAddress ClientAddress = Iter.Key();
		if (Iter.Value().Active)
		{
			Iter.Value().Active = false;
			Clients.Add(Iter.Key());
			UE_LOG( LogProfilerService, Verbose, TEXT( "Ping Active 0: %s, InstanceId: %s, ClientAddress: %s" ), *Iter.Key().ToString(), *InstanceId.ToString(), *ClientAddress.ToString() );
		}
		else
		{
			UE_LOG( LogProfilerService, Verbose, TEXT( "Ping Remove: %s, InstanceId: %s, ClientAddress: %s" ), *Iter.Key().ToString(), *InstanceId.ToString(), *ClientAddress.ToString() );
			SetPreviewState( ClientAddress, false );

			Iter.RemoveCurrent();
			FileTransferRunnable->AbortFileSending( ClientAddress );
		}
	}

	// send the ping message
	if (MessageEndpoint.IsValid() && Clients.Num() > 0)
	{
		MessageEndpoint->Send(new FProfilerServicePing(), Clients);
	}
	return (ClientData.Num() > 0);
#endif
	return false;
}


void FProfilerServiceManager::HandleServiceCaptureMessage( const FProfilerServiceCapture& Message, const IMessageContextRef& Context )
{
#if STATS
	const bool bRequestedCaptureState = Message.bRequestedCaptureState;
	const bool bIsCapturing = FCommandStatsFile::Get().IsStatFileActive();

	if( bRequestedCaptureState != bIsCapturing )
	{
		if( bRequestedCaptureState && !bIsCapturing )
		{
			UE_LOG( LogProfilerService, Verbose, TEXT( "StartCapture, InstanceId: %s, GetSender: %s" ), *InstanceId.ToString(), *Context->GetSender().ToString() );
			StartCapture();
		}
		else if( !bRequestedCaptureState && bIsCapturing )
		{
			UE_LOG( LogProfilerService, Verbose, TEXT( "StopCapture, InstanceId: %s, GetSender: %s" ), *InstanceId.ToString(), *Context->GetSender().ToString() );
			StopCapture();
		}
	}
#endif
}


void FProfilerServiceManager::HandleServicePongMessage( const FProfilerServicePong& Message, const IMessageContextRef& Context )
{
#if STATS
	FClientData* Data = ClientData.Find(Context->GetSender());
	
	if (Data != nullptr)
	{
		Data->Active = true;
		UE_LOG( LogProfilerService, Verbose, TEXT( "Pong InstanceId: %s, GetSender: %s" ), *InstanceId.ToString(), *Context->GetSender().ToString() );
	}
#endif
}


void FProfilerServiceManager::HandleServicePreviewMessage( const FProfilerServicePreview& Message, const IMessageContextRef& Context )
{
	SetPreviewState( Context->GetSender(), Message.bRequestedPreviewState );
}


void FProfilerServiceManager::HandleServiceRequestMessage( const FProfilerServiceRequest& Message, const IMessageContextRef& Context )
{
	if( Message.Request == EProfilerRequestType::PRT_SendLastCapturedFile )
	{
		if( LastStatsFilename.IsEmpty() == false )
		{
			FileTransferRunnable->EnqueueFileToSend( LastStatsFilename, Context->GetSender(), InstanceId );
			LastStatsFilename.Empty();
		}
	}
}



void FProfilerServiceManager::HandleServiceSubscribeMessage( const FProfilerServiceSubscribe& Message, const IMessageContextRef& Context )
{
#if STATS
	const FMessageAddress& SenderAddress = Context->GetSender();
	if( MessageEndpoint.IsValid() && Message.SessionId == SessionId && Message.InstanceId == InstanceId && !ClientData.Contains( SenderAddress ) )
	{
		UE_LOG( LogProfilerService, Log, TEXT( "Subscribe Session: %s, Instance: %s" ), *SessionId.ToString(), *InstanceId.ToString() );

		FClientData Data;
		Data.Active = true;
		Data.Preview = false;
		//Data.StatsWriteFile.WriteHeader();

		// Add to the client list.
		ClientData.Add( SenderAddress, Data );

		// Send authorized and stat descriptions.
		const TArray<uint8> OutData;// = ClientData.Find( SenderAddress )->StatsWriteFile.GetOutData();
		MessageEndpoint->Send( new FProfilerServiceAuthorize( SessionId, InstanceId, OutData ), SenderAddress );

		// Initiate the ping callback
		if (ClientData.Num() == 1)
		{
			PingDelegateHandle = FTicker::GetCoreTicker().AddTicker(PingDelegate, 5.0f);
		}
	}
#endif
}


void FProfilerServiceManager::HandleServiceUnsubscribeMessage( const FProfilerServiceUnsubscribe& Message, const IMessageContextRef& Context )
{
#if	STATS
	const FMessageAddress SenderAddress = Context->GetSender();
	if (Message.SessionId == SessionId && Message.InstanceId == InstanceId)
	{
		UE_LOG( LogProfilerService, Log, TEXT( "Unsubscribe Session: %s, Instance: %s" ), *SessionId.ToString(), *InstanceId.ToString() );

		// clear out any previews
		while (PreviewClients.Num() > 0)
		{
			SetPreviewState( SenderAddress, false );
		}

		// remove from the client list
		ClientData.Remove( SenderAddress );
		FileTransferRunnable->AbortFileSending( SenderAddress );

		// stop the ping messages if we have no clients
		if (ClientData.Num() == 0)
		{
			FTicker::GetCoreTicker().RemoveTicker(PingDelegateHandle);
		}
	}
#endif // STATS
}

void FProfilerServiceManager::HandleNewFrame(int64 Frame)
{
	// Called from the stats thread.
#if STATS
	DECLARE_SCOPE_CYCLE_COUNTER( TEXT( "FProfilerServiceManager::HandleNewFrame" ), STAT_FProfilerServiceManager_HandleNewFrame, STATGROUP_Profiler );
	
	const FStatsThreadState& Stats = FStatsThreadState::GetLocalState();
	const int32 CurrentMetadataSize = Stats.ShortNameToLongName.Num();

	bool bNeedFullMetadata = false;
	if (MetadataSize < CurrentMetadataSize)
	{
		// Write the whole metadata.
		bNeedFullMetadata = true;
		MetadataSize = CurrentMetadataSize;
	}

	// Write frame.
	FStatsWriteFile StatsWriteFile;
	StatsWriteFile.WriteFrame( Frame, bNeedFullMetadata );

	// Task graph
	TArray<uint8>* DataToTask = new TArray<uint8>( MoveTemp( StatsWriteFile.GetOutData() ) );

	// Compression and encoding is done on the task graph
	FSimpleDelegateGraphTask::CreateAndDispatchWhenReady
	(
		FSimpleDelegateGraphTask::FDelegate::CreateRaw( this, &FProfilerServiceManager::CompressDataAndSendToGame, DataToTask, Frame ),
		TStatId()
	);
#endif
}

#if STATS

void FProfilerServiceManager::CompressDataAndSendToGame( TArray<uint8>* DataToTask, int64 Frame )
{
	DECLARE_SCOPE_CYCLE_COUNTER( TEXT( "FProfilerServiceManager::CompressDataAndSendToGame" ), STAT_FProfilerServiceManager_CompressDataAndSendToGame, STATGROUP_Profiler );

	const uint8* UncompressedPtr = DataToTask->GetData();
	const int32 UncompressedSize = DataToTask->Num();

	TArray<uint8> CompressedBuffer;
	CompressedBuffer.Reserve( UncompressedSize );
	int32 CompressedSize = UncompressedSize;

	// We assume that compression cannot fail.
	const bool bResult = FCompression::CompressMemory( COMPRESS_ZLIB, CompressedBuffer.GetData(), CompressedSize, UncompressedPtr, UncompressedSize );
	check( bResult );

	// Convert to hex.
	FString HexData = FString::FromHexBlob( CompressedBuffer.GetData(), CompressedSize );

	// Create a temporary profiler data and prepare all data.
	FProfilerServiceData2* ToGameThread = new FProfilerServiceData2( InstanceId, Frame, HexData, CompressedSize, UncompressedSize );

	const float CompressionRatio = (float)UncompressedSize / (float)CompressedSize;
	UE_LOG( LogProfilerService, VeryVerbose, TEXT( "Frame: %i, UncompressedSize: %i/%f, InstanceId: %i" ), ToGameThread->Frame, UncompressedSize, CompressionRatio, *InstanceId.ToString() );

	// Send to the game thread. PreviewClients is not thread-safe, so we cannot send the data here.
	FSimpleDelegateGraphTask::CreateAndDispatchWhenReady
	(
		FSimpleDelegateGraphTask::FDelegate::CreateRaw( this, &FProfilerServiceManager::HandleNewFrameGT, ToGameThread ),
		TStatId(), nullptr, ENamedThreads::GameThread
	);

	delete DataToTask;
}

void FProfilerServiceManager::HandleNewFrameGT( FProfilerServiceData2* ToGameThread )
{
	if (MessageEndpoint.IsValid())
	{
		// Send through the Message Bus.
		MessageEndpoint->Send( ToGameThread, PreviewClients );
	}
}
#endif
