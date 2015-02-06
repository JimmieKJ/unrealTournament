// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "ProfilerServicePrivatePCH.h"

#include "StatsData.h"
#include "SecureHash.h"


DEFINE_LOG_CATEGORY_STATIC(LogProfile, Log, All);


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
		, WorkEvent( FPlatformProcess::CreateSynchEvent( true ) )
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

			UE_LOG(LogProfile, Log, TEXT( "File service-client sending aborted (srv): %s" ), *It.Key() );
		}

		delete WorkEvent;
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

		UE_LOG(LogProfile, Log, TEXT( "Opening stats file for service-client sending: %s" ), *StatFilepath );

		const int64 FileSize = IFileManager::Get().FileSize(*StatFilepath);
		if( FileSize < 4 )
		{
			UE_LOG(LogProfile, Error, TEXT( "Could not open: %s" ), *StatFilepath );
			return;
		}

		FArchive* FileReader = IFileManager::Get().CreateFileReader(*StatFilepath);
		if( !FileReader )
		{
			UE_LOG(LogProfile, Error, TEXT( "Could not open: %s" ), *StatFilepath );
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

		UE_LOG(LogProfile, Log, TEXT( "File service-client sent successfully : %s" ), *Filename );
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
				UE_LOG(LogProfile, Log, TEXT( "File service-client sending aborted (cl): %s" ), *It.Key() );
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

/* FProfilerServiceManager structors
 *****************************************************************************/

FProfilerServiceManager::FProfilerServiceManager()
{
	MetaData.SecondsPerCycle = FPlatformTime::GetSecondsPerCycle();
	NewMetaData.SecondsPerCycle = FPlatformTime::GetSecondsPerCycle();
	PingDelegate = FTickerDelegate::CreateRaw(this, &FProfilerServiceManager::HandlePing);
	DataFrame.Frame = 0;
}


FProfilerServiceManager::~FProfilerServiceManager()
{}


/* IProfilerServiceManager interface
 *****************************************************************************/

void FProfilerServiceManager::SendData(FProfilerCycleCounter& Data)
{
	// add the data to the data frame
	DataFrame.CycleCounters.FindOrAdd(Data.ThreadId).Add(Data);
}


void FProfilerServiceManager::SendData(FProfilerFloatAccumulator& Data)
{
	// add the data to the data frame
	DataFrame.FloatAccumulators.Add(Data);
}


void FProfilerServiceManager::SendData(FProfilerCountAccumulator& Data)
{
	// add the data to the data frame
	DataFrame.CountAccumulators.Add(Data);
}


void FProfilerServiceManager::SendData(FProfilerCycleGraph& Data)
{
	// add the data to the data frame
	DataFrame.CycleGraphs.FindOrAdd(Data.ThreadId) = Data;
}

void FProfilerServiceManager::StartCapture()
{
#if STATS
	// fire off the equivalent of the stat startfile command
	if (!Archive.IsValid())
	{
		// @TODO yrx 2014-06-05 Needs to be done on the stats thread via the task graph task.
		FString Filename = CreateProfileFilename( FStatConstants::StatsFileExtension, true );
		LastStatsFilename = FApp::GetInstanceName() + TEXT("_") + Filename;
		TSharedPtr<FStatsWriteFile, ESPMode::ThreadSafe> ArchivePtr = MakeShareable(new FStatsWriteFile());
		Archive = ArchivePtr;
		Archive->Start( LastStatsFilename, false );
		if (!Archive->IsValid())
		{
			Archive = nullptr;
		}
	}
#endif
}


void FProfilerServiceManager::StopCapture()
{
#if STATS
	if (Archive.IsValid())
	{
		Archive->Stop();
	}
	Archive = nullptr;
#endif
}

void FProfilerServiceManager::UpdateMetaData()
{
	// @TODO yrx 2014-04-13 Obsolete, remove later.
	// update the thread descriptions only if there has been a change
	int32 OldCount = FRunnableThread::GetThreadRegistry().GetThreadCount();
	if (FRunnableThread::GetThreadRegistry().IsUpdated())
	{
		FRunnableThread::GetThreadRegistry().Lock();
		for( auto Iter = FRunnableThread::GetThreadRegistry().CreateConstIterator(); Iter; ++Iter)
		{
			if (!MetaData.ThreadDescriptions.Contains(Iter.Key()))
			{
				MetaData.ThreadDescriptions.Add(Iter.Key(), Iter.Value()->GetThreadName());
				NewMetaData.ThreadDescriptions.Add(Iter.Key(), Iter.Value()->GetThreadName());
			}
		}
		FRunnableThread::GetThreadRegistry().ClearUpdated();
		FRunnableThread::GetThreadRegistry().Unlock();
	}

	if (MessageEndpoint.IsValid() && PreviewClients.Num() > 0 && (NewMetaData.GroupDescriptions.Num() > 0 || NewMetaData.StatDescriptions.Num() > 0 || NewMetaData.ThreadDescriptions.Num() > 0))
	{
		FArrayWriter ArrayWriter(true);
		ArrayWriter << NewMetaData;
		MessageEndpoint->Send(new FProfilerServiceMetaData(InstanceId, ArrayWriter), PreviewClients);
	}

	NewMetaData.GroupDescriptions.Reset();
	NewMetaData.StatDescriptions.Reset();
	NewMetaData.ThreadDescriptions.Reset();
}


void FProfilerServiceManager::SendMetaData(const FMessageAddress& client)
{
	if (MessageEndpoint.IsValid())
	{
		FArrayWriter ArrayWriter(true);
		ArrayWriter << MetaData;
		MessageEndpoint->Send(new FProfilerServiceMetaData(InstanceId, ArrayWriter), client);
	}
}

void FProfilerServiceManager::StartFrame(uint32 FrameNumber, double FrameStart)
{
	// send data to the clients
	if (DataFrame.Frame > 0)
	{
		if (MessageEndpoint.IsValid() && PreviewClients.Num() > 0)
		{
			FArrayWriter ArrayWriter(true);
			ArrayWriter << DataFrame;
			MessageEndpoint->Send(new FProfilerServiceData(InstanceId, ArrayWriter), PreviewClients );
		}

		ProfilerDataDelegate.Broadcast(FGuid(), DataFrame);
	}

	// update the data frame
	DataFrame.CycleCounters.Reset();
	DataFrame.CycleGraphs.Reset();
	DataFrame.CountAccumulators.Reset();
	DataFrame.FloatAccumulators.Reset();
	DataFrame.Frame = FrameNumber;
	DataFrame.FrameStart = FrameStart;
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

#if STATS
	// check to see if we need to capture, specified from the command line
	Archive = nullptr;
#endif
	if (FParse::Param(FCommandLine::Get(), TEXT("StartCapture")))
	{
		StartCapture();
	}

	FileTransferRunnable = new FFileTransferRunnable( MessageEndpoint );
}


void FProfilerServiceManager::Shutdown()
{
#if STATS
	if (Archive.IsValid())
	{
		Archive = nullptr;
	}
#endif

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

void FProfilerServiceManager::SetPreviewState( const FMessageAddress& ClientAddress, const bool bRequestedPreviewState )
{
#if STATS
	const FStatsThreadState& Stats = FStatsThreadState::GetLocalState();
	FClientData* Client = ClientData.Find( ClientAddress );
	if( Client )
	{
		const bool bIsPreviewing = Client->Preview;

		if( bRequestedPreviewState != bIsPreviewing )
		{
			if( bRequestedPreviewState )
			{
				// enable stat capture
				if (PreviewClients.Num() == 0)
				{
					HandleNewFrameDelegateHandle = Stats.NewFrameDelegate.AddRaw( this, &FProfilerServiceManager::HandleNewFrame );
					StatsMasterEnableAdd();
				}
				PreviewClients.Add(ClientAddress);
				Client->Preview = true;
				if (MessageEndpoint.IsValid())
				{
					Client->CurrentFrame = Stats.CurrentGameFrame;
					MessageEndpoint->Send( new FProfilerServicePreviewAck( InstanceId, Stats.CurrentGameFrame ), ClientAddress );
				}
				SendMetaData(ClientAddress);
			}
			else
			{
				PreviewClients.Remove(ClientAddress);
				Client->Preview = false;

				// stop the ping messages if we have no clients
				if (PreviewClients.Num() == 0)
				{
					// disable stat capture
					Stats.NewFrameDelegate.Remove( HandleNewFrameDelegateHandle );
					StatsMasterEnableAdd();
				}
			}
		}
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
		}
		else
		{
			if (PreviewClients.Contains(ClientAddress))
			{
				PreviewClients.Remove(ClientAddress);	
			}
			Iter.RemoveCurrent();
			FileTransferRunnable->AbortFileSending( ClientAddress );
		}
	}

	if (PreviewClients.Num() == 0)
	{
		// disable stat capture
		FStatsThreadState::GetLocalState().NewFrameDelegate.Remove(HandleNewFrameDelegateHandle);
		StatsMasterEnableAdd();
	}

	// send the ping message
	if (MessageEndpoint.IsValid() && Clients.Num() > 0)
	{
		MessageEndpoint->Send(new FProfilerServicePing(), Clients);
	}
#endif
	return (ClientData.Num() > 0);
}


void FProfilerServiceManager::HandleServiceCaptureMessage( const FProfilerServiceCapture& Message, const IMessageContextRef& Context )
{
#if STATS
	const bool bRequestedCaptureState = Message.bRequestedCaptureState;
	const bool bIsCapturing = Archive.IsValid();

	if( bRequestedCaptureState != bIsCapturing )
	{
		if( bRequestedCaptureState && !Archive.IsValid() )
		{
			UE_LOG(LogProfile, Log, TEXT("StartCapture") );
			StartCapture();
		}
		else if( !bRequestedCaptureState && Archive.IsValid() )
		{
			StopCapture();
		}
	}
#endif
}


void FProfilerServiceManager::HandleServicePongMessage( const FProfilerServicePong& Message, const IMessageContextRef& Context )
{
	FClientData* Data = ClientData.Find(Context->GetSender());
	
	if (Data != nullptr)
	{
		Data->Active = true;
	}
}


void FProfilerServiceManager::HandleServicePreviewMessage( const FProfilerServicePreview& Message, const IMessageContextRef& Context )
{
	SetPreviewState( Context->GetSender(), Message.bRequestedPreviewState );
}


void FProfilerServiceManager::HandleServiceRequestMessage( const FProfilerServiceRequest& Message, const IMessageContextRef& Context )
{
	if (Message.Request == EProfilerRequestType::PRT_MetaData)
	{
		SendMetaData(Context->GetSender());
	}
	else if( Message.Request == EProfilerRequestType::PRT_SendLastCapturedFile )
	{
		if( LastStatsFilename.IsEmpty() == false )
		{
			FileTransferRunnable->EnqueueFileToSend( LastStatsFilename, Context->GetSender(), InstanceId );
			LastStatsFilename.Empty();
		}
	}
}

void FProfilerServiceManager::HandleServiceFileChunkMessage( const FProfilerServiceFileChunk& Message, const IMessageContextRef& Context )
{
	FMemoryReader Reader(Message.Header);
	FProfilerFileChunkHeader Header;
	Reader << Header;
	Header.Validate();

	if( Header.ChunkType == EProfilerFileChunkType::SendChunk )
	{
		// Send this file chunk again.
		FileTransferRunnable->EnqueueFileChunkToSend( new FProfilerServiceFileChunk(Message,FProfilerServiceFileChunk::FNullTag()), true );
	}
	else if( Header.ChunkType == EProfilerFileChunkType::FinalizeFile )
	{
		// Finalize file.
		FileTransferRunnable->FinalizeFileSending( Message.Filename );
	}
}

void FProfilerServiceManager::HandleServiceSubscribeMessage( const FProfilerServiceSubscribe& Message, const IMessageContextRef& Context )
{
#if STATS
	const FMessageAddress& MsgAddress = Context->GetSender();
	if( Message.SessionId == SessionId && Message.InstanceId == InstanceId && !ClientData.Contains( MsgAddress ) )
	{
		UE_LOG(LogProfile, Log, TEXT("Added a client" ));

		FClientData Data;
		Data.Active = true;
		Data.Preview = false;
		Data.StatsWriteFile.WriteHeader( false );

		// add to the client list
		ClientData.Add( MsgAddress, Data );
		// send authorized and stat descriptions
		const TArray<uint8>& OutData = ClientData.Find( MsgAddress )->StatsWriteFile.GetOutData();
		MessageEndpoint->Send( new FProfilerServiceAuthorize2( SessionId, InstanceId, OutData ), MsgAddress );

		const FStatsThreadState& Stats = FStatsThreadState::GetLocalState();
		ClientData.Find( MsgAddress )->MetadataSize = Stats.ShortNameToLongName.Num();

		// initiate the ping callback
		if (ClientData.Num() == 1)
		{
			PingDelegateHandle = FTicker::GetCoreTicker().AddTicker(PingDelegate, 15.0f);
		}
	}
#endif
}


void FProfilerServiceManager::HandleServiceUnsubscribeMessage( const FProfilerServiceUnsubscribe& Message, const IMessageContextRef& Context )
{
	const FMessageAddress SenderAddress = Context->GetSender();
	if (Message.SessionId == SessionId && Message.InstanceId == InstanceId)
	{
		UE_LOG(LogProfile, Log, TEXT("Removed a client"));

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
}

void FProfilerServiceManager::HandleNewFrame(int64 Frame)
{
#if STATS
	// package it up and send to the clients
	if (MessageEndpoint.IsValid())
	{
		const FStatsThreadState& Stats = FStatsThreadState::GetLocalState();
		const int32 CurrentMetadataSize = Stats.ShortNameToLongName.Num();

		// update preview clients with the current data
		for (auto It = PreviewClients.CreateConstIterator(); It; ++It)
		{
			FClientData& Client = *ClientData.Find(*It);

			while(Client.CurrentFrame < Frame)
			{
				Client.CurrentFrame++;
				Client.StatsWriteFile.ResetData();

				bool bNeedFullMetadata = false;
				if( Client.MetadataSize < CurrentMetadataSize )
				{
					// Write the whole metadata.
					bNeedFullMetadata = true;
					Client.MetadataSize = CurrentMetadataSize;
				}

				Client.StatsWriteFile.WriteFrame( Client.CurrentFrame, bNeedFullMetadata );
				MessageEndpoint->Send( new FProfilerServiceData2( InstanceId, Client.CurrentFrame, Client.StatsWriteFile.GetOutData() ), PreviewClients );
			}
		}
	}
#endif
}
