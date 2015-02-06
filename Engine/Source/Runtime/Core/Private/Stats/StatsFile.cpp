// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	StatsFile.cpp: Implements stats file related functionality.
=============================================================================*/

#include "CorePrivatePCH.h"

#if	STATS

#include "StatsData.h"
#include "StatsFile.h"

DECLARE_CYCLE_STAT( TEXT( "Stream File" ), STAT_StreamFile, STATGROUP_StatSystem );
DECLARE_CYCLE_STAT( TEXT( "Wait For Write" ), STAT_StreamFileWaitForWrite, STATGROUP_StatSystem );

/*-----------------------------------------------------------------------------
	FAsyncWriteWorker
-----------------------------------------------------------------------------*/

/**
*	Helper class used to save the capture stats data via the background thread.
*	CAUTION!! Can exist only one instance at the same time. Synchronized via EnsureCompletion.
*/
class FAsyncWriteWorker : public FNonAbandonableTask
{
public:
	/**
	 *	Pointer to the instance of the stats write file.
	 *	Generally speaking accessing this pointer by a different thread is not thread-safe.
	 *	But in this specific case it is.
	 *	@see SendTask
	 */
	FStatsWriteFile* Outer;

	/** Data for the file. Moved via Exchange. */
	TArray<uint8> Data;

	/** Thread cycles for the last frame. Moved via Exchange. */
	TMap<uint32, int64> ThreadCycles;


	/** Constructor. */
	FAsyncWriteWorker( FStatsWriteFile* InStatsWriteFile )
		: Outer( InStatsWriteFile )
	{
		Exchange( Data, InStatsWriteFile->OutData );
		Exchange( ThreadCycles, InStatsWriteFile->ThreadCycles );
	}

	/** Write compressed data to the file. */
	void DoWork()
	{
		check( Data.Num() );
		FArchive& Ar = *Outer->File;

		// Seek to the end of the file.
		Ar.Seek( Ar.TotalSize() );
		const int64 FrameFileOffset = Ar.Tell();

		FCompressedStatsData CompressedData( Data, Outer->CompressedData );
		Ar << CompressedData;

		Outer->FramesInfo.Add( FStatsFrameInfo( FrameFileOffset, ThreadCycles ) );
	}

	/**
	* @return	the name to display in external event viewers
	*/
	static const TCHAR *Name()
	{
		return TEXT( "FAsyncStatsWriteWorker" );
	}
};

/*-----------------------------------------------------------------------------
	FStatsThreadState
-----------------------------------------------------------------------------*/

FStatsThreadState::FStatsThreadState( FString const& Filename )
	: HistoryFrames( MAX_int32 )
	, MaxFrameSeen( -1 )
	, MinFrameSeen( -1 )
	, LastFullFrameMetaAndNonFrame( -1 )
	, LastFullFrameProcessed( -1 )
	, bWasLoaded( true )
	, bFindMemoryExtensiveStats( false )
	, CurrentGameFrame( -1 )
	, CurrentRenderFrame( -1 )
{
	const int64 Size = IFileManager::Get().FileSize( *Filename );
	if( Size < 4 )
	{
		UE_LOG( LogStats, Error, TEXT( "Could not open: %s" ), *Filename );
		return;
	}
	TAutoPtr<FArchive> FileReader( IFileManager::Get().CreateFileReader( *Filename ) );
	if( !FileReader )
	{
		UE_LOG( LogStats, Error, TEXT( "Could not open: %s" ), *Filename );
		return;
	}

	FStatsReadStream Stream;
	if( !Stream.ReadHeader( *FileReader ) )
	{
		UE_LOG( LogStats, Error, TEXT( "Could not open, bad magic: %s" ), *Filename );
		return;
	}

	// Test version only works for the finalized stats files.
	const bool bIsFinalized = Stream.Header.IsFinalized();
	check( bIsFinalized );
	
	TArray<FStatMessage> Messages;
	if( Stream.Header.bRawStatsFile )
	{
		const int64 CurrentFilePos = FileReader->Tell();

		// Read metadata.
		TArray<FStatMessage> MetadataMessages;
		Stream.ReadFNamesAndMetadataMessages( *FileReader, MetadataMessages );
		ProcessMetaDataForLoad( MetadataMessages );

		// Read frames offsets.
		Stream.ReadFramesOffsets( *FileReader );

		// Verify frames offsets.
		for( int32 FrameIndex = 0; FrameIndex < Stream.FramesInfo.Num(); ++FrameIndex )
		{
			const int64 FrameFileOffset = Stream.FramesInfo[FrameIndex].FrameFileOffset;
			FileReader->Seek( FrameFileOffset );

			int64 TargetFrame;
			*FileReader << TargetFrame;
		}
		FileReader->Seek( Stream.FramesInfo[0].FrameFileOffset );

		// Read the raw stats messages.
		FStatPacketArray IncomingData;
		for( int32 FrameIndex = 0; FrameIndex < Stream.FramesInfo.Num(); ++FrameIndex )
		{
			int64 TargetFrame;
			*FileReader << TargetFrame;

			int32 NumPackets;
			*FileReader << NumPackets;

			for( int32 PacketIndex = 0; PacketIndex < NumPackets; PacketIndex++ )
			{
				FStatPacket* ToRead = new FStatPacket();
				Stream.ReadStatPacket( *FileReader, *ToRead, bIsFinalized );
				IncomingData.Packets.Add( ToRead );
			}
	
			FStatPacketArray NowData;
			// This is broken, do not use.
// 			Exchange( NowData.Packets, IncomingData.Packets );
// 			ScanForAdvance( NowData );
// 			AddToHistoryAndEmpty( NowData );
// 			check( !NowData.Packets.Num() );
		}
	}
	else
	{
		// Read the condensed stats messages.
		while( FileReader->Tell() < Size )
		{
			FStatMessage Read( Stream.ReadMessage( *FileReader ) );
			if( Read.NameAndInfo.GetField<EStatOperation>() == EStatOperation::SpecialMessageMarker )
			{
				// Simply break the loop.
				// The profiler supports more advanced handling of this message.
				break;
			}
			else if( Read.NameAndInfo.GetField<EStatOperation>() == EStatOperation::AdvanceFrameEventGameThread )
			{
				ProcessMetaDataForLoad( Messages );
				if( CurrentGameFrame > 0 && Messages.Num() )
				{
					check( !CondensedStackHistory.Contains( CurrentGameFrame ) );
					TArray<FStatMessage>* Save = new TArray<FStatMessage>();
					Exchange( *Save, Messages );
					CondensedStackHistory.Add( CurrentGameFrame, Save );
					GoodFrames.Add( CurrentGameFrame );
				}
			}

			new (Messages)FStatMessage( Read );
		}
		// meh, we will discard the last frame, but we will look for meta data
	}
}

void FStatsThreadState::AddMessages( TArray<FStatMessage>& InMessages )
{
	bWasLoaded = true;
	TArray<FStatMessage> Messages;
	for( int32 Index = 0; Index < InMessages.Num(); ++Index )
	{
		if( InMessages[Index].NameAndInfo.GetField<EStatOperation>() == EStatOperation::AdvanceFrameEventGameThread )
		{
			ProcessMetaDataForLoad( Messages );
			if( !CondensedStackHistory.Contains( CurrentGameFrame ) && Messages.Num() )
			{
				TArray<FStatMessage>* Save = new TArray<FStatMessage>();
				Exchange( *Save, Messages );
				if( CondensedStackHistory.Num() >= HistoryFrames )
				{
					for( auto It = CondensedStackHistory.CreateIterator(); It; ++It )
					{
						delete It.Value();
					}
					CondensedStackHistory.Reset();
				}
				CondensedStackHistory.Add( CurrentGameFrame, Save );
				GoodFrames.Add( CurrentGameFrame );
			}
		}

		new (Messages)FStatMessage( InMessages[Index] );
	}
	bWasLoaded = false;
}

void FStatsThreadState::ProcessMetaDataForLoad(TArray<FStatMessage>& Data)
{
	check(bWasLoaded);
	for (int32 Index = 0; Index < Data.Num() ; Index++)
	{
		FStatMessage& Item = Data[Index];
		EStatOperation::Type Op = Item.NameAndInfo.GetField<EStatOperation>();
		if (Op == EStatOperation::SetLongName)
		{
			FindOrAddMetaData(Item);
		}
		else if (Op == EStatOperation::AdvanceFrameEventGameThread)
		{
			check(Item.NameAndInfo.GetField<EStatDataType>() == EStatDataType::ST_int64);
			if (Item.GetValue_int64() > 0)
			{
				CurrentGameFrame = Item.GetValue_int64();
				if (CurrentGameFrame > MaxFrameSeen)
				{
					MaxFrameSeen = CurrentGameFrame;
				}
				if (MinFrameSeen < 0)
				{
					MinFrameSeen = CurrentGameFrame;
				}
			}
		}
		else if (Op == EStatOperation::AdvanceFrameEventRenderThread)
		{
			check(Item.NameAndInfo.GetField<EStatDataType>() == EStatDataType::ST_int64);
			if (Item.GetValue_int64() > 0)
			{
				CurrentRenderFrame = Item.GetValue_int64();
				if (CurrentGameFrame > MaxFrameSeen)
				{
					MaxFrameSeen = CurrentGameFrame;
				}
				if (MinFrameSeen < 0)
				{
					MinFrameSeen = CurrentGameFrame;
				}
			}
		}
	}
}

/*-----------------------------------------------------------------------------
	FStatsWriteFile
-----------------------------------------------------------------------------*/

FStatsWriteFile::FStatsWriteFile()
	: File( nullptr )
	, AsyncTask( nullptr )
{
	// Reserve 1MB.
	CompressedData.Reserve( EStatsFileConstants::MAX_COMPRESSED_SIZE );
}


void FStatsWriteFile::Start( FString const& InFilename, bool bIsRawStatsFile )
{
	const FString PathName = *(FPaths::ProfilingDir() + TEXT( "UnrealStats/" ));

	FString Filename = PathName + InFilename;
	FString Path = FPaths::GetPath( Filename );
	IFileManager::Get().MakeDirectory( *Path, true );

	UE_LOG( LogStats, Log, TEXT( "Opening stats file: %s" ), *Filename );

	File = IFileManager::Get().CreateFileWriter( *Filename );
	if( !File )
	{
		UE_LOG( LogStats, Error, TEXT( "Could not open: %s" ), *Filename );
	}
	else
	{
		ArchiveFilename = Filename;
		AddNewFrameDelegate();
		WriteHeader( bIsRawStatsFile );
		StatsMasterEnableAdd();
	}
}

void FStatsWriteFile::Stop()
{
	if( IsValid() )
	{
		StatsMasterEnableSubtract();
		RemoveNewFrameDelegate();
		SendTask();
		SendTask();
		Finalize();

		File->Close();
		delete File;
		File = nullptr;

		UE_LOG( LogStats, Log, TEXT( "Wrote stats file: %s" ), *ArchiveFilename );
		FCommandStatsFile::LastFileSaved = ArchiveFilename;
	}
}

void FStatsWriteFile::WriteHeader( bool bIsRawStatsFile )
{
	FMemoryWriter MemoryWriter( OutData, false, true );
	FArchive& Ar = File ? *File : MemoryWriter;

	uint32 Magic = EStatMagicWithHeader::MAGIC;
	// Serialize magic value.
	Ar << Magic;

	// Serialize dummy header, overwritten in Finalize.
	Header.Version = EStatMagicWithHeader::VERSION_4;
	Header.PlatformName = FPlatformProperties::PlatformName();
	Header.bRawStatsFile = bIsRawStatsFile;
	Ar << Header;

	// Serialize metadata.
	WriteMetadata( Ar );
	Ar.Flush();
}

void FStatsWriteFile::WriteFrame( int64 TargetFrame, bool bNeedFullMetadata /*= false*/ )
{
	FMemoryWriter Ar( OutData, false, true );
	const int64 PrevArPos = Ar.Tell();

	if( bNeedFullMetadata )
	{
		WriteMetadata( Ar );
	}

	FStatsThreadState const& Stats = FStatsThreadState::GetLocalState();
	TArray<FStatMessage> const& Data = Stats.GetCondensedHistory( TargetFrame );
	for( auto It = Data.CreateConstIterator(); It; ++It )
	{
		WriteMessage( Ar, *It );
	}

	// Get cycles for all threads, so we can use that data to generate the mini-view.
	for( auto It = Stats.Threads.CreateConstIterator(); It; ++It )
	{
		const int64 Cycles = Stats.GetFastThreadFrameTime( TargetFrame, It.Key() );
		ThreadCycles.Add( It.Key(), Cycles );
	}

	// Serialize thread cycles. Disabled for now.
	//Ar << ThreadCycles;
}

void FStatsWriteFile::Finalize()
{
	FArchive& Ar = *File;

	// Write dummy compression size, so we can detect the end of the file.
	FCompressedStatsData::WriteEndOfCompressedData( Ar );

	// Real header, written at start of the file, but written out right before we close the file.

	// Write out frame table and update header with offset and count.
	Header.FrameTableOffset = Ar.Tell();
	Ar << FramesInfo;

	const FStatsThreadState& Stats = FStatsThreadState::GetLocalState();

	// Add FNames from the stats metadata.
	for( const auto& It : Stats.ShortNameToLongName )
	{
		const FStatMessage& StatMessage = It.Value;
		FNamesSent.Add( StatMessage.NameAndInfo.GetRawName().GetComparisonIndex() );
	}

	// Create a copy of names.
	TSet<int32> FNamesToSent = FNamesSent;
	FNamesSent.Empty( FNamesSent.Num() );

	// Serialize FNames.
	Header.FNameTableOffset = Ar.Tell();
	Header.NumFNames = FNamesToSent.Num();
	for( const int32 It : FNamesToSent )
	{
		WriteFName( Ar, FStatNameAndInfo(FName(It, It, 0),false) );
	}

	// Serialize metadata messages.
	Header.MetadataMessagesOffset = Ar.Tell();
	Header.NumMetadataMessages = Stats.ShortNameToLongName.Num();
	WriteMetadata( Ar );

	// Verify data.
	TSet<int32> BMinA = FNamesSent.Difference( FNamesToSent );
	struct FLocal
	{
		static TArray<FName> GetFNameArray( const TSet<int32>& NameIndices )
		{
			TArray<FName> Result;
			for( const int32 NameIndex : NameIndices )
			{
				new(Result) FName( NameIndex, NameIndex, 0 );
			}
			return Result;
		}
	};
	auto BMinANames = FLocal::GetFNameArray( BMinA );

	// Seek to the position just after a magic value of the file and write out proper header.
	Ar.Seek( sizeof(uint32) );
	Ar << Header;
}

void FStatsWriteFile::NewFrame( int64 TargetFrame )
{
	SCOPE_CYCLE_COUNTER( STAT_StreamFile );

	// @TODO yrx 2014-11-25 Add stat startfile -num=number of frames to capture
	/*
	enum
	{
		MAX_NUM_RAWFRAMES = 120,
	};
	if( Header.bRawStatsFile )
	{
		if( FCommandStatsFile::FirstFrame == -1 )
		{
			FCommandStatsFile::FirstFrame = TargetFrame;
		}
		else if( TargetFrame > FCommandStatsFile::FirstFrame + MAX_NUM_RAWFRAMES )
		{
			FCommandStatsFile::Stop();
			FCommandStatsFile::FirstFrame = -1;
			return;
		}
	}
	}*/

	WriteFrame( TargetFrame );
	SendTask();
}

void FStatsWriteFile::SendTask()
{
	if( AsyncTask )
	{
		SCOPE_CYCLE_COUNTER( STAT_StreamFileWaitForWrite );
		AsyncTask->EnsureCompletion();
		delete AsyncTask;
		AsyncTask = NULL;
	}
	if( OutData.Num() )
	{
		AsyncTask = new FAsyncTask<FAsyncWriteWorker>( this );
		check( !OutData.Num() );
		check( !ThreadCycles.Num() );
		AsyncTask->StartBackgroundTask();
	}
}

bool FStatsWriteFile::IsValid() const
{
	return !!File;
}

void FStatsWriteFile::AddNewFrameDelegate()
{
	FStatsThreadState const& Stats = FStatsThreadState::GetLocalState();
	NewFrameDelegateHandle = Stats.NewFrameDelegate.AddThreadSafeSP( this->AsShared(), &FStatsWriteFile::NewFrame );
}

void FStatsWriteFile::RemoveNewFrameDelegate()
{
	FStatsThreadState const& Stats = FStatsThreadState::GetLocalState();
	Stats.NewFrameDelegate.Remove( NewFrameDelegateHandle );
}

/*-----------------------------------------------------------------------------
	FRawStatsWriteFile
-----------------------------------------------------------------------------*/

void FRawStatsWriteFile::WriteFrame( int64 TargetFrame, bool bNeedFullMetadata /*= false */)
{
	FMemoryWriter Ar( OutData, false, true );
	const FStatsThreadState& Stats = FStatsThreadState::GetLocalState();

	check( Stats.IsFrameValid( TargetFrame ) );

	Ar << TargetFrame;

	// Write stat packet.
	const FStatPacketArray& Frame = Stats.GetStatPacketArray( TargetFrame );
	int32 NumPackets = Frame.Packets.Num();
	Ar << NumPackets;
	for( int32 PacketIndex = 0; PacketIndex < Frame.Packets.Num(); PacketIndex++ )
	{
		FStatPacket& StatPacket = *Frame.Packets[PacketIndex];
		WriteStatPacket( Ar, StatPacket );
	}

	// Get cycles for all threads, so we can use that data to generate the mini-view.
	for( auto It = Stats.Threads.CreateConstIterator(); It; ++It )
	{
		const int64 Cycles = Stats.GetFastThreadFrameTime( TargetFrame, It.Key() );
		ThreadCycles.Add( It.Key(), Cycles );
	}
	
	// Serialize thread cycles.
	Ar << ThreadCycles;
}

/*-----------------------------------------------------------------------------
	Commands functionality
-----------------------------------------------------------------------------*/

FString FCommandStatsFile::LastFileSaved;
int64 FCommandStatsFile::FirstFrame = -1;
TSharedPtr<FStatsWriteFile, ESPMode::ThreadSafe> FCommandStatsFile::CurrentStatsFile = nullptr;

void FCommandStatsFile::Start( const TCHAR* Cmd )
{
	CurrentStatsFile = NULL;
	FString File;
	FParse::Token( Cmd, File, false );
	TSharedPtr<FStatsWriteFile, ESPMode::ThreadSafe> StatFile = MakeShareable( new FStatsWriteFile() );
	CurrentStatsFile = StatFile;
	CurrentStatsFile->Start( File, false );
	if( !CurrentStatsFile->IsValid() )
	{
		CurrentStatsFile = nullptr;
	}
}

void FCommandStatsFile::StartRaw( const TCHAR* Cmd )
{
	CurrentStatsFile = NULL;
	FString File;
	FParse::Token( Cmd, File, false );
	TSharedPtr<FStatsWriteFile, ESPMode::ThreadSafe> StatFile = MakeShareable( new FRawStatsWriteFile() );
	CurrentStatsFile = StatFile;
	CurrentStatsFile->Start( File, true );
	if( !CurrentStatsFile->IsValid() )
	{
		CurrentStatsFile = nullptr;
	}
}

void FCommandStatsFile::Stop()
{
	if( CurrentStatsFile.IsValid() )
	{
		CurrentStatsFile->Stop();
	}
	CurrentStatsFile = nullptr;
}

#endif // STATS
