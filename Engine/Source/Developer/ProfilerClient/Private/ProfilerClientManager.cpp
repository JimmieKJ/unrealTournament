// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ProfilerClientModule.cpp: Implements the FProfilerClientModule class.
=============================================================================*/

#include "ProfilerClientPrivatePCH.h"
#include "ISessionInstanceInfo.h"
#include "SecureHash.h"
#include "StatsData.h"
#include "StatsFile.h"

DEFINE_LOG_CATEGORY_STATIC( LogProfilerClient, Log, All );

DECLARE_CYCLE_STAT( TEXT("HandleDataReceived"),	STAT_PC_HandleDataReceived,			STATGROUP_Profiler );
DECLARE_CYCLE_STAT( TEXT("ReadStatMessages"),	STAT_PC_ReadStatMessages,			STATGROUP_Profiler );
DECLARE_CYCLE_STAT( TEXT("AddStatMessages"),	STAT_PC_AddStatMessages,			STATGROUP_Profiler );
DECLARE_CYCLE_STAT( TEXT("GenerateDataFrame"),	STAT_PC_GenerateDataFrame,			STATGROUP_Profiler );
DECLARE_CYCLE_STAT( TEXT("AddStatFName"),		STAT_PC_AddStatFName,				STATGROUP_Profiler );
DECLARE_CYCLE_STAT( TEXT("AddGroupFName"),		STAT_PC_AddGroupFName,				STATGROUP_Profiler );
DECLARE_CYCLE_STAT( TEXT("GenerateCycleGraph"),	STAT_PC_GenerateCycleGraph,			STATGROUP_Profiler );
DECLARE_CYCLE_STAT( TEXT("GenerateAccumulator"),STAT_PC_GenerateAccumulator,		STATGROUP_Profiler );
DECLARE_CYCLE_STAT( TEXT("FindOrAddStat"),		STAT_PC_FindOrAddStat,				STATGROUP_Profiler );
DECLARE_CYCLE_STAT( TEXT("FindOrAddThread"),	STAT_PC_FindOrAddThread,			STATGROUP_Profiler );

/* FProfilerClientManager structors
 *****************************************************************************/

FProfilerClientManager::FProfilerClientManager( const IMessageBusRef& InMessageBus )
{
#if STATS
	MessageBus = InMessageBus;
	MessageEndpoint = FMessageEndpoint::Builder("FProfilerClientModule", InMessageBus)
		.Handling<FProfilerServiceAuthorize>(this, &FProfilerClientManager::HandleServiceAuthorizeMessage)
		.Handling<FProfilerServiceData2>(this, &FProfilerClientManager::HandleProfilerServiceData2Message)
		.Handling<FProfilerServicePreviewAck>(this, &FProfilerClientManager::HandleServicePreviewAckMessage)
		.Handling<FProfilerServiceFileChunk>(this, &FProfilerClientManager::HandleServiceFileChunk)  
		.Handling<FProfilerServicePing>(this, &FProfilerClientManager::HandleServicePingMessage);

	if (MessageEndpoint.IsValid())
	{
		OnShutdownMessageBusDelegateHandle = InMessageBus->OnShutdown().AddRaw( this, &FProfilerClientManager::HandleMessageBusShutdown );
		MessageEndpoint->Subscribe<FProfilerServicePing>();
	}

	TickDelegate = FTickerDelegate::CreateRaw(this, &FProfilerClientManager::HandleTicker);
	MessageDelegate = FTickerDelegate::CreateRaw(this, &FProfilerClientManager::HandleMessagesTicker);
	LastPingTime = FDateTime::Now();
	RetryTime = 5.f;

#if PROFILER_THREADED_LOAD
	LoadTask = nullptr;
#endif
	LoadConnection = nullptr;
	MessageDelegateHandle = FTicker::GetCoreTicker().AddTicker(MessageDelegate, 0.1f);
#endif
}

FProfilerClientManager::~FProfilerClientManager()
{
#if STATS
	// Delete all active file writers and remove temporary files.
	for( auto It = ActiveTransfers.CreateIterator(); It; ++It )
	{
		FReceivedFileInfo& ReceivedFileInfo = It.Value();
		
		delete ReceivedFileInfo.FileWriter;
		ReceivedFileInfo.FileWriter = nullptr;

		IFileManager::Get().Delete( *ReceivedFileInfo.DestFilepath );

		UE_LOG( LogProfilerClient, Log, TEXT( "File service-client transfer aborted: %s" ), *It.Key() );
	}

	FTicker::GetCoreTicker().RemoveTicker(MessageDelegateHandle);
	FTicker::GetCoreTicker().RemoveTicker(TickDelegateHandle);

	Unsubscribe();

	if (MessageBus.IsValid())
	{
		MessageBus->OnShutdown().Remove( OnShutdownMessageBusDelegateHandle );
	}

	LoadConnection = nullptr;
#endif
}


/* IProfilerClient interface
 *****************************************************************************/

void FProfilerClientManager::Subscribe( const FGuid& Session )
{
#if STATS
	FGuid OldSessionId = ActiveSessionId;
	PendingSessionId = Session;
	if (MessageEndpoint.IsValid())
	{
		if (OldSessionId.IsValid())
		{
			TArray<FGuid> Instances;
			Connections.GenerateKeyArray(Instances);
			for (int32 i = 0; i < Instances.Num(); ++i)
			{
				MessageEndpoint->Publish(new FProfilerServiceUnsubscribe(OldSessionId, Instances[i]), EMessageScope::Network);

				// fire the disconnection delegate
				ProfilerClientDisconnectedDelegate.Broadcast(ActiveSessionId, Instances[i]);
			}

			ActiveSessionId.Invalidate();
		}
		ActiveSessionId = PendingSessionId;
	}

	Connections.Reset();

	UE_LOG( LogProfilerClient, Log, TEXT( "Subscribe Session: %s" ), *Session.ToString() );
#endif
}

void FProfilerClientManager::Track( const FGuid& Instance )
{
#if STATS
	if (MessageEndpoint.IsValid() && ActiveSessionId.IsValid() && !PendingInstances.Contains(Instance))
	{
		PendingInstances.Add(Instance);

		MessageEndpoint->Publish(new FProfilerServiceSubscribe(ActiveSessionId, Instance), EMessageScope::Network);

		RetryTime = 5.f;
		TickDelegateHandle = FTicker::GetCoreTicker().AddTicker(TickDelegate, RetryTime);

		UE_LOG( LogProfilerClient, Verbose, TEXT( "Track Session: %s, Instance: %s" ), *ActiveSessionId.ToString(), *Instance.ToString() );
	}
#endif
}

void FProfilerClientManager::Track( const TArray<TSharedPtr<ISessionInstanceInfo>>& Instances )
{
#if STATS
	if (MessageEndpoint.IsValid() && ActiveSessionId.IsValid())
	{
		TArray<FGuid> ActiveInstances;
		Connections.GenerateKeyArray(ActiveInstances);

		for (int32 i = 0; i < Instances.Num(); ++i)
		{
			if (Connections.Find(Instances[i]->GetInstanceId()) == nullptr)
			{
				Track(Instances[i]->GetInstanceId());
			}
			else
			{
				ActiveInstances.Remove(Instances[i]->GetInstanceId());
			}
		}

		for (int32 i = 0; i < ActiveInstances.Num(); ++i)
		{
			Untrack(ActiveInstances[i]);
		}
	}
#endif
}

void FProfilerClientManager::Untrack( const FGuid& Instance )
{
#if STATS
	if (MessageEndpoint.IsValid() && ActiveSessionId.IsValid())
	{
		MessageEndpoint->Publish(new FProfilerServiceUnsubscribe(ActiveSessionId, Instance), EMessageScope::Network);
		Connections.Remove(Instance);

		// fire the disconnection delegate
		ProfilerClientDisconnectedDelegate.Broadcast(ActiveSessionId, Instance);

		UE_LOG( LogProfilerClient, Verbose, TEXT( "Untrack Session: %s, Instance: %s" ), *ActiveSessionId.ToString(), *Instance.ToString() );
	}
#endif
}

void FProfilerClientManager::Unsubscribe()
{
#if STATS
	PendingSessionId.Invalidate();
	Subscribe(PendingSessionId);
#endif
}

void FProfilerClientManager::SetCaptureState( const bool bRequestedCaptureState, const FGuid& InstanceId /*= FGuid()*/  )
{
#if STATS
	if (MessageEndpoint.IsValid() && ActiveSessionId.IsValid())
	{
		if( !InstanceId.IsValid() )
		{
			TArray<FMessageAddress> Instances;
			for (auto It = Connections.CreateConstIterator(); It; ++It)
			{
				Instances.Add( It.Value().ProfilerServiceAddress );
			}
			MessageEndpoint->Send(new FProfilerServiceCapture(bRequestedCaptureState), Instances);
			UE_LOG( LogProfilerClient, Verbose, TEXT( "SetCaptureState Session: %s, Instance: %s, State: %i" ), *ActiveSessionId.ToString(), *InstanceId.ToString(), (int32)bRequestedCaptureState );
		}
		else
		{
			const FMessageAddress* MessageAddress = &Connections.Find(InstanceId)->ProfilerServiceAddress;
			if( MessageAddress )
			{
				MessageEndpoint->Send(new FProfilerServiceCapture(bRequestedCaptureState), *MessageAddress);
			}

			UE_LOG( LogProfilerClient, Verbose, TEXT( "SetCaptureState Session: %s, Instance: %s, State: %i" ), *ActiveSessionId.ToString(), *InstanceId.ToString(), (int32)bRequestedCaptureState );
		}
	}
#endif
}

void FProfilerClientManager::SetPreviewState( const bool bRequestedPreviewState, const FGuid& InstanceId /*= FGuid()*/ )
{
#if STATS
	if (MessageEndpoint.IsValid() && ActiveSessionId.IsValid())
	{
		if( !InstanceId.IsValid() )
		{
			TArray<FMessageAddress> Instances;
			for (auto It = Connections.CreateConstIterator(); It; ++It)
			{
				Instances.Add( It.Value().ProfilerServiceAddress );
			}
			MessageEndpoint->Send(new FProfilerServicePreview(bRequestedPreviewState), Instances);
			UE_LOG( LogProfilerClient, Verbose, TEXT( "SetPreviewState Session: %s, Instance: %s, State: %i" ), *ActiveSessionId.ToString(), *InstanceId.ToString(), (int32)bRequestedPreviewState );
		}
		else
		{
			const FMessageAddress* MessageAddress = &Connections.Find(InstanceId)->ProfilerServiceAddress;
			if( MessageAddress )
			{
				MessageEndpoint->Send(new FProfilerServicePreview(bRequestedPreviewState), *MessageAddress);
			}
			UE_LOG( LogProfilerClient, Verbose, TEXT( "SetPreviewState Session: %s, Instance: %s, State: %i" ), *ActiveSessionId.ToString(), *InstanceId.ToString(), (int32)bRequestedPreviewState );
		}
	}
#endif
}

void FProfilerClientManager::LoadCapture( const FString& DataFilepath, const FGuid& ProfileId )
{
#if STATS
	// start an async load
	LoadConnection = &Connections.FindOrAdd(ProfileId);
	LoadConnection->InstanceId = ProfileId;
	LoadConnection->MetaData.CriticalSection = &LoadConnection->CriticalSection;
	LoadConnection->MetaData.SecondsPerCycle = FPlatformTime::GetSecondsPerCycle(); // fix this by adding a message which specifies this

	const int64 Size = IFileManager::Get().FileSize( *DataFilepath );
	if( Size < 4 )
	{
		UE_LOG( LogProfilerClient, Error, TEXT( "Could not open: %s" ), *DataFilepath );
		return;
	}

#if PROFILER_THREADED_LOAD
	FArchive* FileReader = IFileManager::Get().CreateFileReader(*DataFilepath);
#else
	FileReader = IFileManager::Get().CreateFileReader( *DataFilepath );
#endif

	if( !FileReader )
	{
		UE_LOG( LogProfilerClient, Error, TEXT( "Could not open: %s" ), *DataFilepath );
		return;
	}

	if( !LoadConnection->Stream.ReadHeader( *FileReader ) )
	{
		UE_LOG( LogProfilerClient, Error, TEXT( "Could not open, bad magic: %s" ), *DataFilepath );
		delete FileReader;
		return;
	}

	// This shouldn't happen.
	if( LoadConnection->Stream.Header.bRawStatsFile )
	{
		delete FileReader;
		return;
	}

	const bool bIsFinalized = LoadConnection->Stream.Header.IsFinalized();
	if( bIsFinalized )
	{
		// Read metadata.
		TArray<FStatMessage> MetadataMessages;
		LoadConnection->Stream.ReadFNamesAndMetadataMessages( *FileReader, MetadataMessages );
		LoadConnection->CurrentThreadState.ProcessMetaDataOnly( MetadataMessages );

		// Read frames offsets.
		LoadConnection->Stream.ReadFramesOffsets( *FileReader );
		FileReader->Seek( LoadConnection->Stream.FramesInfo[0].FrameFileOffset );
	}

	if( LoadConnection->Stream.Header.HasCompressedData() )
	{
		UE_CLOG( !bIsFinalized, LogProfilerClient, Fatal, TEXT( "Compressed stats file has to be finalized" ) );
	}

#if PROFILER_THREADED_LOAD
	LoadTask = new FAsyncTask<FAsyncReadWorker>(LoadConnection, FileReader);
	LoadTask->StartBackgroundTask();
#endif

	RetryTime = 0.05f;
	TickDelegateHandle = FTicker::GetCoreTicker().AddTicker(TickDelegate, RetryTime);
	ProfilerLoadStartedDelegate.Broadcast(ProfileId);
#endif
}


void FProfilerClientManager::RequestLastCapturedFile( const FGuid& InstanceId /*= FGuid()*/ )
{
#if STATS
	if (MessageEndpoint.IsValid() && ActiveSessionId.IsValid())
	{
		if( !InstanceId.IsValid() )
		{
			TArray<FMessageAddress> Instances;
			for (auto It = Connections.CreateConstIterator(); It; ++It)
			{
				Instances.Add( It.Value().ProfilerServiceAddress );
			}
			MessageEndpoint->Send(new FProfilerServiceRequest(EProfilerRequestType::PRT_SendLastCapturedFile), Instances);
		}
		else
		{
			const FMessageAddress* MessageAddress = &Connections.Find(InstanceId)->ProfilerServiceAddress;
			if( MessageAddress )
			{
				MessageEndpoint->Send(new FProfilerServiceRequest(EProfilerRequestType::PRT_SendLastCapturedFile), *MessageAddress);
			}
		}
	}
#endif
}

/* FProfilerClientManager event handlers
 *****************************************************************************/

void FProfilerClientManager::HandleMessageBusShutdown()
{
#if STATS
	Unsubscribe();

	MessageEndpoint.Reset();
	MessageBus.Reset();
#endif
}

void FProfilerClientManager::HandleServiceAuthorizeMessage( const FProfilerServiceAuthorize& Message, const IMessageContextRef& Context )
{
#if STATS
	if (ActiveSessionId == Message.SessionId && PendingInstances.Contains(Message.InstanceId))
	{
		PendingInstances.Remove(Message.InstanceId);
		FServiceConnection& Connection = Connections.FindOrAdd(Message.InstanceId);
		Connection.Initialize(Message, Context);

		// Fire a meta data update message
		ProfilerMetaDataUpdatedDelegate.Broadcast(Message.InstanceId);

		// Fire the client connection event
		ProfilerClientConnectedDelegate.Broadcast(ActiveSessionId, Message.InstanceId);

		UE_LOG( LogProfilerClient, Verbose, TEXT( "Authorize SessionId: %s, InstanceId: %s" ), *Message.SessionId.ToString(), *Message.InstanceId.ToString() );
	}
#endif
}

void FServiceConnection::Initialize( const FProfilerServiceAuthorize& Message, const IMessageContextRef& Context )
{
#if STATS
	ProfilerServiceAddress = Context->GetSender();
	InstanceId = Message.InstanceId;
	CurrentData.Frame = 0;

	// Add the supplied meta data.
	FMemoryReader MemoryReader( Message.Data );

	MetaData.CriticalSection = &CriticalSection;
	int64 Size = MemoryReader.TotalSize();

	const bool bVerifyHeader = Stream.ReadHeader( MemoryReader );
	//check( bVerifyHeader );

	// Read in the data.
	TArray<FStatMessage> StatMessages;
	{
		SCOPE_CYCLE_COUNTER(STAT_PC_ReadStatMessages);
		while(MemoryReader.Tell() < Size)
		{
			// read the message
			new (StatMessages)FStatMessage( Stream.ReadMessage( MemoryReader ) );
		}
		static FStatNameAndInfo Adv(NAME_AdvanceFrame, "", "", TEXT(""), EStatDataType::ST_int64, true, false);
		new (StatMessages) FStatMessage(Adv.GetEncodedName(), EStatOperation::AdvanceFrameEventGameThread, 1LL, false);
	}

	// generate a thread state from the data
	{
		SCOPE_CYCLE_COUNTER(STAT_PC_AddStatMessages);
		CurrentThreadState.AddMessages(StatMessages);
	}

	UpdateMetaData();
#endif
}

bool FProfilerClientManager::CheckHashAndWrite( const FProfilerServiceFileChunk& FileChunk, const FProfilerFileChunkHeader& FileChunkHeader, FArchive* Writer )
{
#if STATS
	const int32 HashSize = 20;
	uint8 LocalHash[HashSize]={0};
	
	// Hash file chunk data. 
	FSHA1 Sha;
	Sha.Update( FileChunk.Data.GetData(), FileChunkHeader.ChunkSize );
	// Hash file chunk header.
	Sha.Update( FileChunk.Header.GetData(), FileChunk.Header.Num() );
	Sha.Final();
	Sha.GetHash( LocalHash );

	const int32 MemDiff = FMemory::Memcmp( FileChunk.ChunkHash.GetData(), LocalHash, HashSize );

	bool bResult = false;

	if( MemDiff == 0 )
	{
		// Write the data to the archive.
		Writer->Seek( FileChunkHeader.ChunkOffset );
		Writer->Serialize( (void*)FileChunk.Data.GetData(), FileChunkHeader.ChunkSize );

		bResult = true;
	}

	return bResult;
#else
	return false;
#endif
}

void FProfilerClientManager::HandleServiceFileChunk( const FProfilerServiceFileChunk& FileChunk, const IMessageContextRef& Context )
{
#if STATS
	const TCHAR* StrTmp = TEXT(".tmp");

	// Read file chunk header.
	FMemoryReader Reader( FileChunk.Header );
	FProfilerFileChunkHeader FileChunkHeader;
	Reader << FileChunkHeader;
	FileChunkHeader.Validate();

	const bool bValidFileChunk = !FailedTransfer.Contains( FileChunk.Filename );

	// @TODO yrx 2014-03-24 At this moment received file chunks are handled on the main thread, asynchronous file receiving is planned for the future release.
	if (ActiveSessionId.IsValid() && Connections.Find(FileChunk.InstanceId) != nullptr && bValidFileChunk)
	{
		FReceivedFileInfo* ReceivedFileInfo = ActiveTransfers.Find( FileChunk.Filename );
		if( !ReceivedFileInfo )
		{
			const FString PathName = FPaths::ProfilingDir() + TEXT("UnrealStats/Received/");
			const FString StatFilepath = PathName + FileChunk.Filename + StrTmp;

			UE_LOG( LogProfilerClient, Log, TEXT( "Opening stats file for service-client sending: %s" ), *StatFilepath );

			FArchive* FileWriter = IFileManager::Get().CreateFileWriter(*StatFilepath);
			if( !FileWriter )
			{
				UE_LOG( LogProfilerClient, Error, TEXT( "Could not open: %s" ), *StatFilepath );
				return;
			}

			ReceivedFileInfo = &ActiveTransfers.Add( FileChunk.Filename, FReceivedFileInfo(FileWriter,0,StatFilepath) );
			ProfilerFileTransferDelegate.Broadcast( FileChunk.Filename, ReceivedFileInfo->Progress, FileChunkHeader.FileSize );
		}

		const bool bSimulateBadFileChunk = true;//FMath::Rand() % 10 != 0;
		const bool bSuccess = CheckHashAndWrite( FileChunk, FileChunkHeader, ReceivedFileInfo->FileWriter ) && bSimulateBadFileChunk;	
		if( bSuccess )
		{
			ReceivedFileInfo->Progress += FileChunkHeader.ChunkSize;
			ReceivedFileInfo->Update();

			if( ReceivedFileInfo->Progress == FileChunkHeader.FileSize )
			{
				// File has been successfully sent, so send this information to the profiler service.
				if( MessageEndpoint.IsValid() )
				{
					MessageEndpoint->Send( new FProfilerServiceFileChunk( FGuid(),FileChunk.Filename,FProfilerFileChunkHeader(0,0,0,EProfilerFileChunkType::FinalizeFile).AsArray() ), Context->GetSender() );
					ProfilerFileTransferDelegate.Broadcast( FileChunk.Filename, ReceivedFileInfo->Progress, FileChunkHeader.FileSize );
				}
				
				// Delete the file writer.
				delete ReceivedFileInfo->FileWriter;
				ReceivedFileInfo->FileWriter = nullptr;

				// Rename the stats file.
				IFileManager::Get().Move( *ReceivedFileInfo->DestFilepath.Replace( StrTmp, TEXT("") ), *ReceivedFileInfo->DestFilepath );

				ActiveTransfers.Remove( FileChunk.Filename );

				UE_LOG( LogProfilerClient, Log, TEXT( "File service-client received successfully: %s" ), *FileChunk.Filename );
			}
			else
			{
				ProfilerFileTransferDelegate.Broadcast( FileChunk.Filename, ReceivedFileInfo->Progress, FileChunkHeader.FileSize );			
			}
		}
		else
		{
			// This chunk is a bad chunk, so ask for resending it.
			if( MessageEndpoint.IsValid() )
			{
				MessageEndpoint->Send( new FProfilerServiceFileChunk(FileChunk,FProfilerServiceFileChunk::FNullTag()), Context->GetSender() );
				UE_LOG( LogProfilerClient, Log, TEXT( "Received a bad chunk of file, resending: %5i, %6u, %10u, %s" ), FileChunk.Data.Num(), ReceivedFileInfo->Progress, FileChunkHeader.FileSize, *FileChunk.Filename );
			}
		}
	}
#endif
}

void FProfilerClientManager::HandleServicePingMessage( const FProfilerServicePing& Message, const IMessageContextRef& Context )
{
#if STATS
	if (MessageEndpoint.IsValid())
	{
		TArray<FMessageAddress> Instances;
		for (auto It = Connections.CreateConstIterator(); It; ++It)
		{
			Instances.Add(It.Value().ProfilerServiceAddress);
		}
		MessageEndpoint->Send(new FProfilerServicePong(), Instances);

		UE_LOG( LogProfilerClient, Verbose, TEXT( "Ping GetSender: %s" ), *Context->GetSender().ToString() );
	}
#endif
}

#if PROFILER_THREADED_LOAD
bool FProfilerClientManager::AsyncLoad()
{
#if STATS
	BroadcastMetadataUpdate();

	if (LoadTask->IsDone())
	{
		FinalizeLoading();
		return false;
	}
#endif
	return true;
}
#else
bool FProfilerClientManager::SyncLoad()
{
#if STATS
	const bool bFinalize = LoadConnection->ReadAndConvertStatMessages( *FileReader, false );
	BroadcastMetadataUpdate();

	if( bFinalize )
	{
		FinalizeLoading();
		return false;
	}
#endif
	return true;
}
#endif

bool FProfilerClientManager::HandleTicker( float DeltaTime )
{
#if STATS
	if (PendingInstances.Num() > 0 && FDateTime::Now() > LastPingTime + DeltaTime)
	{
		TArray<FGuid> Instances;
		Instances.Append(PendingInstances);

		PendingInstances.Reset();

		for (int32 i = 0; i < Instances.Num(); ++i)
		{
			Track(Instances[i]);
		}
		LastPingTime = FDateTime::Now();
	}
	else if (LoadConnection)
	{
#if PROFILER_THREADED_LOAD
		return AsyncLoad();
#else
		return SyncLoad();
#endif
	}
#endif
	return false;
}

bool FProfilerClientManager::HandleMessagesTicker( float DeltaTime )
{
#if STATS
	for (auto It = Connections.CreateIterator(); It; ++It)
	{
		FServiceConnection& Connection = It.Value();

		TArray<int64> Frames;
		Connection.PendingMessages.GenerateKeyArray( Frames );
		Frames.Sort();

		// MessageBus sends all data in out of order fashion.
		// We buffer frame to make sure that all frames are received in the proper order.
		const int32 NUM_BUFFERED_FRAMES = 15;

		for( int32 Index = 0; Index < Frames.Num(); Index++ )
		{
			if (Connection.PendingMessages.Num() < NUM_BUFFERED_FRAMES)
			{
				break;
			}

			int64 FrameNum = Frames[Index];
			TArray<uint8>& Data = *Connection.PendingMessages.Find( FrameNum );

			// Pass the data to the visualization code.
			FMemoryReader MemoryReader( Data );

			int64 Size = MemoryReader.TotalSize();
			FStatsReadStream& Stream = Connection.Stream;

			UE_LOG( LogProfilerClient, VeryVerbose, TEXT( "Frame=%i/%i, FNamesIndexMap=%i, CurrentMetadataSize=%i" ), FrameNum, Frames.Num(), Connection.Stream.FNamesIndexMap.Num(), Connection.CurrentThreadState.ShortNameToLongName.Num() );

			// read in the data and post if we reach a 
			{
				SCOPE_CYCLE_COUNTER(STAT_PC_ReadStatMessages);
				while(MemoryReader.Tell() < Size)
				{
					// read the message
					FStatMessage Message(Stream.ReadMessage(MemoryReader));

					if (Message.NameAndInfo.GetField<EStatOperation>() == EStatOperation::AdvanceFrameEventGameThread)
					{
						Connection.AddCollectedStatMessages( Message );
					}

					new (Connection.Messages) FStatMessage(Message);
				}
			}

			// create an old format data frame from the data
			Connection.GenerateProfilerDataFrame();

			// Fire a meta data update message
			if (Connection.CurrentData.MetaDataUpdated)
			{
				ProfilerMetaDataUpdatedDelegate.Broadcast(Connection.InstanceId);
			}

			// send the data out
			ProfilerDataDelegate.Broadcast( Connection.InstanceId, Connection.CurrentData, 0.0f );

			Connection.PendingMessages.Remove( FrameNum );
		}
	}

	// Remove any active transfer that timed out.
	for( auto It = ActiveTransfers.CreateIterator(); It; ++It )
	{
		FReceivedFileInfo& ReceivedFileInfo = It.Value();
		const FString& Filename = It.Key();

		if( ReceivedFileInfo.IsTimedOut() )
		{
			UE_LOG( LogProfilerClient, Log, TEXT( "File service-client timed out, aborted: %s" ), *Filename );
			FailedTransfer.Add( Filename );

			delete ReceivedFileInfo.FileWriter;
			ReceivedFileInfo.FileWriter = nullptr;

			IFileManager::Get().Delete( *ReceivedFileInfo.DestFilepath );
			ProfilerFileTransferDelegate.Broadcast( Filename, -1, -1 );
			It.RemoveCurrent();	
		}
	}
#endif

	return true;
}

void FProfilerClientManager::HandleServicePreviewAckMessage( const FProfilerServicePreviewAck& Message, const IMessageContextRef& Context )
{
#if STATS
	if (ActiveSessionId.IsValid() && Connections.Find(Message.InstanceId) != NULL)
	{
		FServiceConnection& Connection = *Connections.Find(Message.InstanceId);

		UE_LOG( LogProfilerClient, Verbose, TEXT( "PreviewAck InstanceId: %s, GetSender: %s" ), *Message.InstanceId.ToString(), *Context->GetSender().ToString() );
	}
#endif
}

void FProfilerClientManager::HandleProfilerServiceData2Message( const FProfilerServiceData2& Message, const IMessageContextRef& Context )
{
#if STATS
	SCOPE_CYCLE_COUNTER(STAT_PC_HandleDataReceived);
	if (ActiveSessionId.IsValid() && Connections.Find(Message.InstanceId) != nullptr)
	{
		// Create a temporary profiler data and prepare all data.
		FProfilerServiceData2* ToProcess = new FProfilerServiceData2( Message.InstanceId, Message.Frame, Message.HexData, Message.CompressedSize, Message.UncompressedSize );

		// Decompression and decoding is done on the task graph.
		FSimpleDelegateGraphTask::CreateAndDispatchWhenReady
		(
			FSimpleDelegateGraphTask::FDelegate::CreateRaw( this, &FProfilerClientManager::DecompressDataAndSendToGame, ToProcess ), 
			TStatId()
		);
	}
#endif
}

void FProfilerClientManager::DecompressDataAndSendToGame( FProfilerServiceData2* ToProcess )
{
	DECLARE_SCOPE_CYCLE_COUNTER( TEXT( "FProfilerClientManager::DecompressDataAndSendToGame" ), STAT_FProfilerClientManager_DecompressDataAndSendToGame, STATGROUP_Profiler );

	// De-hex string into TArray<uint8>
	TArray<uint8> CompressedData;
	CompressedData.Reset( ToProcess->CompressedSize );
	CompressedData.AddUninitialized( ToProcess->CompressedSize );
	FString::ToHexBlob( ToProcess->HexData, CompressedData.GetData(), ToProcess->CompressedSize );

	// Decompress data.
	TArray<uint8> UncompressedData;
	UncompressedData.Reset( ToProcess->UncompressedSize );
	UncompressedData.AddUninitialized( ToProcess->UncompressedSize );

	bool bResult = FCompression::UncompressMemory( COMPRESS_ZLIB, UncompressedData.GetData(), ToProcess->UncompressedSize, CompressedData.GetData(), ToProcess->CompressedSize );
	check( bResult );

	// Send to the game thread. Connections is not thread-safe, so we cannot add the data here.
	TArray<uint8>* DateToGame = new TArray<uint8>( MoveTemp( UncompressedData ) );

	FSimpleDelegateGraphTask::CreateAndDispatchWhenReady
	(
		FSimpleDelegateGraphTask::FDelegate::CreateRaw( this, &FProfilerClientManager::SendToGame, DateToGame, ToProcess->Frame, ToProcess->InstanceId ), 
		TStatId(), nullptr, ENamedThreads::GameThread
	);

	delete ToProcess;
}

void FProfilerClientManager::SendToGame( TArray<uint8>* DataToGame, int64 Frame, const FGuid InstanceId )
{
	if (ActiveSessionId.IsValid() && Connections.Find( InstanceId ) != nullptr)
	{
		FServiceConnection& Connection = *Connections.Find( InstanceId );

		// Add the message to the connections queue.
		UE_LOG( LogProfilerClient, VeryVerbose, TEXT( "Frame: %i, UncompressedSize: %i, InstanceId: %s" ), Frame, DataToGame->Num(), *InstanceId.ToString() );
		Connection.PendingMessages.Add( Frame, MoveTemp( *DataToGame ) );
	}

	delete DataToGame;
}

void FProfilerClientManager::BroadcastMetadataUpdate()
{
	while( LoadConnection->DataFrames.Num() > 0 )
	{
		FScopeLock ScopeLock( &(LoadConnection->CriticalSection) );

		// Fire a meta data update message
		if( LoadConnection->DataFrames[0].MetaDataUpdated )
		{
			ProfilerMetaDataUpdatedDelegate.Broadcast( LoadConnection->InstanceId );
			ProfilerLoadedMetaDataDelegate.Broadcast( LoadConnection->InstanceId );
		}

		FProfilerDataFrame& DataFrame = LoadConnection->DataFrames[0];
		ProfilerDataDelegate.Broadcast( LoadConnection->InstanceId, DataFrame, LoadConnection->DataLoadingProgress );
		LoadConnection->DataFrames.RemoveAt( 0 );
	}
}

void FProfilerClientManager::FinalizeLoading()
{
	ProfilerLoadCompletedDelegate.Broadcast( LoadConnection->InstanceId );
	LoadConnection = nullptr;
#if	PROFILER_THREADED_LOAD
	delete LoadTask;
	LoadTask = nullptr;
#else
	delete FileReader;
	FileReader = nullptr;
#endif // PROFILER_THREADED_LOAD
	RetryTime = 5.f;
}

void FProfilerClientManager::FAsyncReadWorker::DoWork()
{
#if STATS
	const bool bFinalize = LoadConnection->ReadAndConvertStatMessages( *FileReader, true );
	if( bFinalize )
	{
		FileReader->Close();
		delete FileReader;
	}
#endif
}

#if STATS
void FServiceConnection::UpdateMetaData()
{
	// loop through the stats meta data messages
	for (auto It = CurrentThreadState.ShortNameToLongName.CreateConstIterator(); It; ++It)
	{
		FStatMessage const& LongName = It.Value();
		const FName GroupName = LongName.NameAndInfo.GetGroupName();

		uint32 StatType = STATTYPE_Error;
		if (LongName.NameAndInfo.GetField<EStatDataType>() == EStatDataType::ST_int64)
		{
			if( LongName.NameAndInfo.GetFlag( EStatMetaFlags::IsCycle ) )
			{
				StatType = STATTYPE_CycleCounter;
			}
			else if( LongName.NameAndInfo.GetFlag( EStatMetaFlags::IsMemory ) )
			{
				StatType = STATTYPE_MemoryCounter;
			}
			else
			{
				StatType = STATTYPE_AccumulatorDWORD;
			}
		}
		else if (LongName.NameAndInfo.GetField<EStatDataType>() == EStatDataType::ST_double)
		{
			StatType = STATTYPE_AccumulatorFLOAT;
		}
		if (StatType != STATTYPE_Error)
		{
			FindOrAddStat(LongName.NameAndInfo, StatType);
		}

		// Threads metadata.
		const bool bIsThread = FStatConstants::NAME_ThreadGroup == GroupName;
		if( bIsThread )
		{
			FindOrAddThread( LongName.NameAndInfo );
		}	
	}
}

int32 FServiceConnection::FindOrAddStat( const FStatNameAndInfo& StatNameAndInfo, uint32 StatType)
{
	SCOPE_CYCLE_COUNTER(STAT_PC_FindOrAddStat);
	const FName LongName = StatNameAndInfo.GetRawName();
	int32* const StatIDPtr = LongNameToStatID.Find( LongName );
	int32 StatID = StatIDPtr != nullptr ? *StatIDPtr : -1;

	if (!StatIDPtr)
	{
		// meta data has been updated
		CurrentData.MetaDataUpdated = true;

		const FName StatName = StatNameAndInfo.GetShortName();
		FName GroupName = StatNameAndInfo.GetGroupName();
		const FString Description = StatNameAndInfo.GetDescription();

		// do some special stats first
		if (StatName == TEXT("STAT_FrameTime"))
		{
			StatID = LongNameToStatID.Add(LongName, 2);
		}
		else if (StatName == FStatConstants::NAME_ThreadRoot)
		{
			StatID = LongNameToStatID.Add(LongName, 1);
			GroupName = TEXT( "NoGroup" );
		}
		else
		{
			StatID = LongNameToStatID.Add(LongName, LongNameToStatID.Num()+10);
		}
		check(StatID != -1);

		// add a new stat description to the meta data
		FStatDescription StatDescription;
		StatDescription.ID = StatID;
		StatDescription.Name = !Description.IsEmpty() ? Description : StatName.ToString();
		if( StatDescription.Name.Contains( TEXT("STAT_") ) )
		{
			StatDescription.Name = StatDescription.Name.RightChop(FString(TEXT("STAT_")).Len());
		}
		StatDescription.StatType = StatType;

		if( GroupName == NAME_None && Stream.Header.Version == EStatMagicNoHeader::NO_VERSION )
		{	
			// @todo Add more ways to group the stats.
			const int32 Thread_Pos = StatDescription.Name.Find( TEXT("Thread_") );
			const int32 _0Pos = StatDescription.Name.Find( TEXT("_0") );
			const bool bIsThread = Thread_Pos != INDEX_NONE && _0Pos > Thread_Pos;
			// Add a special group for all threads.
			if( bIsThread )
			{
				GroupName = TEXT("Threads");
			}
			// Add a special group for all objects.
			else
			{
				GroupName = TEXT("Objects");
			}
		}

		int32* const GroupIDPtr = GroupNameArray.Find( GroupName );
		int32 GroupID = GroupIDPtr != nullptr ? *GroupIDPtr : -1;
		if( !GroupIDPtr )
		{
			// add a new group description to the meta data
			GroupID = GroupNameArray.Add(GroupName, GroupNameArray.Num()+10);
			check( GroupID != -1 );

			FStatGroupDescription GroupDescription;
			GroupDescription.ID = GroupID;
			GroupDescription.Name = GroupName.ToString();
			GroupDescription.Name.RemoveFromStart(TEXT("STATGROUP_"));

			// add to the meta data
			FScopeLock ScopeLock(&CriticalSection);
			MetaData.GroupDescriptions.Add(GroupDescription.ID, GroupDescription);
		}
		{
			StatDescription.GroupID = GroupID;
			FScopeLock ScopeLock(&CriticalSection);
			MetaData.StatDescriptions.Add(StatDescription.ID, StatDescription);
		}
	}
	// return the stat id
	return StatID;
}

int32 FServiceConnection::FindOrAddThread(const FStatNameAndInfo& Thread)
{
	SCOPE_CYCLE_COUNTER(STAT_PC_FindOrAddThread);

	// The description of a thread group contains the thread id
	const FString Desc = Thread.GetDescription();
	const uint32 ThreadID = FStatsUtils::ParseThreadID( Desc );

	const FName ShortName = Thread.GetShortName();

	// add to the meta data
	FScopeLock ScopeLock( &CriticalSection );
	const int32 OldNum = MetaData.ThreadDescriptions.Num();
	MetaData.ThreadDescriptions.Add( ThreadID, ShortName.ToString() );
	const int32 NewNum = MetaData.ThreadDescriptions.Num();

	// meta data has been updated
	CurrentData.MetaDataUpdated = CurrentData.MetaDataUpdated || OldNum != NewNum;

	return ThreadID;
}

void FServiceConnection::GenerateAccumulators(TArray<FStatMessage>& Stats, TArray<FProfilerCountAccumulator>& CountAccumulators, TArray<FProfilerFloatAccumulator>& FloatAccumulators)
{
	SCOPE_CYCLE_COUNTER(STAT_PC_GenerateAccumulator)
	for (int32 Index = 0; Index < Stats.Num(); ++Index)
	{
		FStatMessage& Stat = Stats[Index];
		if (Stat.NameAndInfo.GetField<EStatDataType>() == EStatDataType::ST_int64)
		{
			// add a count accumulator
			FProfilerCountAccumulator Data;
			Data.StatId = FindOrAddStat(Stat.NameAndInfo, STATTYPE_AccumulatorDWORD);
			Data.Value = Stat.GetValue_int64();
			CountAccumulators.Add(Data);
		}
		else if (Stat.NameAndInfo.GetField<EStatDataType>() == EStatDataType::ST_double)
		{
			// add a float accumulator
			FProfilerFloatAccumulator Data;
			Data.StatId = FindOrAddStat(Stat.NameAndInfo, STATTYPE_AccumulatorFLOAT);
			Data.Value = Stat.GetValue_double();
			FloatAccumulators.Add(Data);

			const FName StatName = Stat.NameAndInfo.GetShortName();
			if (StatName == TEXT("STAT_SecondsPerCycle"))
			{
				FScopeLock ScopeLock( &CriticalSection );
				MetaData.SecondsPerCycle = Stat.GetValue_double();
			}
		}
	}
}

void FServiceConnection::CreateGraphRecursively(const FRawStatStackNode* Root, FProfilerCycleGraph& Graph, uint32 InStartCycles)
{
	Graph.FrameStart = InStartCycles;
	Graph.StatId = FindOrAddStat(Root->Meta.NameAndInfo, STATTYPE_CycleCounter);

	// add the data
	if (Root->Meta.NameAndInfo.GetField<EStatDataType>() == EStatDataType::ST_int64)
	{
		if (Root->Meta.NameAndInfo.GetFlag(EStatMetaFlags::IsPackedCCAndDuration))
		{
			Graph.CallsPerFrame = FromPackedCallCountDuration_CallCount(Root->Meta.GetValue_int64());
			Graph.Value = FromPackedCallCountDuration_Duration(Root->Meta.GetValue_int64());
		}
		else
		{
			Graph.CallsPerFrame = 1;
			Graph.Value = Root->Meta.GetValue_int64();
		}
	}

	uint32 ChildStartCycles = InStartCycles;
	TArray<FRawStatStackNode*> ChildArray;
	Root->Children.GenerateValueArray(ChildArray);
	ChildArray.Sort( FStatDurationComparer<FRawStatStackNode>() );
	for( int32 Index = 0; Index < ChildArray.Num(); ++Index )
	{
		const FRawStatStackNode* ChildStat = ChildArray[Index];

		// create the child graph
		FProfilerCycleGraph ChildGraph;
		ChildGraph.ThreadId = Graph.ThreadId;
		CreateGraphRecursively(ChildStat, ChildGraph, ChildStartCycles);

		// add to the graph
		Graph.Children.Add(ChildGraph);

		// update the start cycles
		ChildStartCycles += ChildGraph.Value;
	}
}

void FServiceConnection::GenerateCycleGraphs(const FRawStatStackNode& Root, TMap<uint32, FProfilerCycleGraph>& CycleGraphs)
{
	SCOPE_CYCLE_COUNTER(STAT_PC_GenerateCycleGraph);

	// Initialize the root stat.
	FindOrAddStat(Root.Meta.NameAndInfo, STATTYPE_CycleCounter);

	// get the cycle graph from each child of the stack root
	TArray<FRawStatStackNode*> ChildArray;
	Root.Children.GenerateValueArray(ChildArray);
	for (int32 Index = 0; Index < ChildArray.Num(); ++Index)
	{
		FRawStatStackNode* ThreadNode = ChildArray[Index];
		FProfilerCycleGraph Graph;

		// determine the thread id
		Graph.ThreadId = FindOrAddThread(ThreadNode->Meta.NameAndInfo);

		// create the thread graph
		CreateGraphRecursively(ThreadNode, Graph, 0);

		// add to the map
		CycleGraphs.Add(Graph.ThreadId, Graph);
	}
}

bool FServiceConnection::ReadAndConvertStatMessages( FArchive& Reader, bool bUseInAsync )
{
	// Buffer used to store the compressed and decompressed data.
	TArray<uint8> SrcData;
	TArray<uint8> DestData;

	const bool bHasCompressedData = Stream.Header.HasCompressedData();
	const bool bIsFinalized = Stream.Header.IsFinalized();

	SCOPE_CYCLE_COUNTER( STAT_PC_ReadStatMessages );
	if( bHasCompressedData )
	{
		while( Reader.Tell() < Reader.TotalSize() )
		{
			// Read the compressed data.
			FCompressedStatsData UncompressedData( SrcData, DestData );
			Reader << UncompressedData;
			if( UncompressedData.HasReachedEndOfCompressedData() )
			{
				return true;
			}

			FMemoryReader MemoryReader( DestData );

			while( MemoryReader.Tell() < MemoryReader.TotalSize() )
			{
				// read the message
				FStatMessage Message( Stream.ReadMessage( MemoryReader, bIsFinalized ) );
				ReadMessages++;

				if( Message.NameAndInfo.GetShortName() != TEXT( "Unknown FName" ) )
				{
					if( Message.NameAndInfo.GetField<EStatOperation>() == EStatOperation::AdvanceFrameEventGameThread && ReadMessages > 2 )
					{
						AddCollectedStatMessages( Message );

						// create an old format data frame from the data
						GenerateProfilerDataFrame();

						{
							// add the frame to the work list
							FScopeLock ScopeLock( &CriticalSection );
							DataFrames.Add( CurrentData );
							DataLoadingProgress = (double)Reader.Tell() / (double)Reader.TotalSize();
						}

						if( bUseInAsync )
						{
							if( DataFrames.Num() > FProfilerClientManager::MaxFramesPerTick )
							{
								FPlatformProcess::Sleep( 0.1f );
							}
						}


					}

					new (Messages)FStatMessage( Message );			
				}
				else
				{
					break;
				}	
			}

			if( !bUseInAsync && DataFrames.Num() < FProfilerClientManager::MaxFramesPerTick )
			{
				return false;
			}
		}
	}
	else
	{
		while( Reader.Tell() < Reader.TotalSize() )
		{
			// read the message
			FStatMessage Message( Stream.ReadMessage( Reader, bIsFinalized ) );
			ReadMessages++;

			if( Message.NameAndInfo.GetShortName() != TEXT( "Unknown FName" ) )
			{
				if( Message.NameAndInfo.GetField<EStatOperation>() == EStatOperation::SpecialMessageMarker )
				{
					// Simply break the loop.
					// The profiler supports more advanced handling of this message.
					return true;
				}
				else if( Message.NameAndInfo.GetField<EStatOperation>() == EStatOperation::AdvanceFrameEventGameThread && ReadMessages > 2 )
				{
					AddCollectedStatMessages( Message );

					// create an old format data frame from the data
					GenerateProfilerDataFrame();

					{
						// add the frame to the work list
						FScopeLock ScopeLock( &CriticalSection );
						DataFrames.Add( CurrentData );
						DataLoadingProgress = (double)Reader.Tell() / (double)Reader.TotalSize();
					}

					if( bUseInAsync )
					{
						if( DataFrames.Num() > FProfilerClientManager::MaxFramesPerTick )
						{
							while( DataFrames.Num() )
							{
								FPlatformProcess::Sleep( 0.001f );
							}
						}
					}
				}

				new (Messages)FStatMessage( Message );
			}
			else
			{
				break;
			}

			if( !bUseInAsync && DataFrames.Num() < FProfilerClientManager::MaxFramesPerTick )
			{
				return false;
			}
		}
	}
	
	if( Reader.Tell() >= Reader.TotalSize() )
	{
		return true;
	}

	return false;
}

void FServiceConnection::AddCollectedStatMessages( FStatMessage Message )
{
	SCOPE_CYCLE_COUNTER( STAT_PC_AddStatMessages );
	new (Messages)FStatMessage( Message );
	CurrentThreadState.AddMessages( Messages );
	Messages.Reset();
}

void FServiceConnection::GenerateProfilerDataFrame()
{
	SCOPE_CYCLE_COUNTER( STAT_PC_GenerateDataFrame );
	FProfilerDataFrame& DataFrame = CurrentData;
	DataFrame.Frame = CurrentThreadState.CurrentGameFrame;
	DataFrame.FrameStart = 0.0;
	DataFrame.CountAccumulators.Reset();
	DataFrame.CycleGraphs.Reset();
	DataFrame.FloatAccumulators.Reset();
	DataFrame.MetaDataUpdated = false;

	// get the stat stack root and the non frame stats
	FRawStatStackNode Stack;
	TArray<FStatMessage> NonFrameStats;
	CurrentThreadState.UncondenseStackStats( CurrentThreadState.CurrentGameFrame, Stack, nullptr, &NonFrameStats );

	// cycle graphs
	GenerateCycleGraphs( Stack, DataFrame.CycleGraphs );

	// accumulators
	GenerateAccumulators( NonFrameStats, DataFrame.CountAccumulators, DataFrame.FloatAccumulators );
}

#endif
