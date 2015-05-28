// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "NullNetworkReplayStreaming.h"
#include "Paths.h"
#include "EngineVersion.h"

DEFINE_LOG_CATEGORY_STATIC( LogNullReplay, Log, All );

/* Class to hold stream event information */
class FNullCheckpointListItem : public FOnlineJsonSerializable
{
public:
	FNullCheckpointListItem() : Time1(0), Time2(0) {}

	FString		Group;
	FString		Metadata;
	uint32		Time1;
	uint32		Time2;

	// FOnlineJsonSerializable
	BEGIN_ONLINE_JSON_SERIALIZER
		ONLINE_JSON_SERIALIZE( "group",			Group );
		ONLINE_JSON_SERIALIZE( "meta",			Metadata );
		ONLINE_JSON_SERIALIZE( "time1",			Time1 );
		ONLINE_JSON_SERIALIZE( "time2",			Time2 );
	END_ONLINE_JSON_SERIALIZER
};

/**
 * Very basic implementation of network replay streaming using the file system
 * As of now, there is just simple opening and closing of the stream, and handing off the stream for direct use
 * Eventually, we'll want to expand this interface to allow enumerating demos, support for live spectating on local machine
 * (which will require support for writing/reading at the same time)
 */

static FString GetStreamBaseFilename(const FString& StreamName)
{
	int32 Year, Month, DayOfWeek, Day, Hour, Min, Sec, MSec;
	FPlatformTime::SystemTime( Year, Month, DayOfWeek, Day, Hour, Min, Sec, MSec );

	FString DemoName = StreamName;

	DemoName.ReplaceInline( TEXT( "%td" ), *FDateTime::Now().ToString() );
	DemoName.ReplaceInline( TEXT( "%d" ), *FString::Printf( TEXT( "%i-%i-%i" ), Month, Day, Year ) );
	DemoName.ReplaceInline( TEXT( "%t" ), *FString::Printf( TEXT( "%i" ), ( ( Hour * 3600 ) + ( Min * 60 ) + Sec ) * 1000 + MSec ) );
	DemoName.ReplaceInline( TEXT( "%v" ), *FString::Printf( TEXT( "%i" ), GEngineVersion.GetChangelist() ) );

	// replace bad characters with underscores
	DemoName.ReplaceInline( TEXT( "\\" ),	TEXT( "_" ) );
	DemoName.ReplaceInline( TEXT( "/" ),	TEXT( "_" ) );
	DemoName.ReplaceInline( TEXT( "." ),	TEXT( "_" ) );
	DemoName.ReplaceInline( TEXT( " " ),	TEXT( "_" ) );
	DemoName.ReplaceInline( TEXT( "%" ),	TEXT( "_" ) );

	return DemoName;
}

static FString GetDemoPath()
{
	return FPaths::Combine(*FPaths::GameSavedDir(), TEXT( "Demos/" ));
}

static FString GetStreamDirectory(const FString& StreamName)
{
	const FString DemoName = GetStreamBaseFilename(StreamName);

	// Directory for this demo
	const FString DemoDir  = FPaths::Combine(*GetDemoPath(), *DemoName);
	
	return DemoDir;
}

static FString GetStreamFullBaseFilename(const FString& StreamName)
{
	return FPaths::Combine(*GetStreamDirectory(StreamName), *GetStreamBaseFilename(StreamName));
}

static FString GetHeaderFilename(const FString& StreamName)
{
	return GetStreamFullBaseFilename(StreamName) + TEXT(".header");
}

static FString GetDemoFilename(const FString& StreamName)
{
	return GetStreamFullBaseFilename(StreamName) + TEXT(".demo");
}

static FString GetMetadataFilename(const FString& StreamName)
{
	return GetStreamFullBaseFilename(StreamName) + TEXT(".metadata");
}

static FString GetCheckpointFilename( const FString& StreamName, int32 Index )
{
	return FPaths::Combine(*GetStreamDirectory(StreamName), TEXT("checkpoints"), *FString::Printf( TEXT("checkpoint%d"), Index ) );
}

static FString GetEventFilename( const FString& StreamName, int32 Index )
{
	return FPaths::Combine(*GetStreamDirectory(StreamName), TEXT("events"), *FString::Printf( TEXT("event%d"), Index ) );
}

static FString GetInfoFilename( const FString& StreamName )
{
	return GetStreamFullBaseFilename(StreamName) + TEXT(".replayinfo");
}

static FNullReplayInfo ReadReplayInfo( const FString& StreamName )
{
	FNullReplayInfo Info;

	const FString InfoFilename = GetInfoFilename(StreamName);
	TUniquePtr<FArchive> InfoFileArchive( IFileManager::Get().CreateFileReader( *InfoFilename ) );

	if ( InfoFileArchive.IsValid() )
	{
		FString JsonString;
		*InfoFileArchive << JsonString;

		Info.FromJson(JsonString);
	}

	return Info;
}

// Returns a name formatted as "demoX", where X is 0-9.
// Returns the first value that doesn't yet exist, or if they all exist, returns the oldest one
// (it will be overwritten).
static FString GetAutomaticDemoName()
{
	FString FinalDemoName;
	FDateTime BestDateTime = FDateTime::MaxValue();

	const int MAX_DEMOS = 10;

	for (int32 i = 0; i < MAX_DEMOS; i++)
	{
		const FString DemoName = FString::Printf(TEXT("demo%i"), i + 1);
		
		const FString FullDemoName = GetDemoFilename(DemoName);
		
		FDateTime DateTime = IFileManager::Get().GetTimeStamp(*FullDemoName);

		if (DateTime == FDateTime::MinValue())
		{
			// If we don't find this file, we can early out now
			FinalDemoName = DemoName;
			break;
		}
		else if (DateTime < BestDateTime)
		{
			// Use the oldest file
			FinalDemoName = DemoName;
			BestDateTime = DateTime;
		}
	}

	return FinalDemoName;
}

void FNullNetworkReplayStreamer::StartStreaming( const FString& CustomName, const FString& FriendlyName, bool bRecord, const FNetworkReplayVersion& ReplayVersion, const FOnStreamReadyDelegate& Delegate )
{
	FString FinalDemoName = CustomName;

	if ( CustomName.IsEmpty() )
	{
		if ( bRecord )
		{
			// If we're recording and the caller didn't provide a name, generate one automatically
			FinalDemoName = GetAutomaticDemoName();
		}
		else
		{
			// Can't play a replay if the user didn't provide a name!
			Delegate.ExecuteIfBound( false, bRecord );
			return;
		}
	}
	
	const FString DemoDir = GetStreamDirectory(FinalDemoName);

	const FString FullHeaderFilename = GetHeaderFilename(FinalDemoName);
	const FString FullDemoFilename = GetDemoFilename(FinalDemoName);
	
	CurrentStreamName = FinalDemoName;

	if ( !bRecord )
	{
		// Load metadata if it exists
		ReplayInfo = ReadReplayInfo( CurrentStreamName );

		// Open file for reading
		FileAr.Reset( IFileManager::Get().CreateFileReader( *FullDemoFilename ) );
		HeaderAr.Reset( IFileManager::Get().CreateFileReader( *FullHeaderFilename ) );
		StreamerState = EStreamerState::Playback;
	}
	else
	{
		// Delete any existing demo with this name
		IFileManager::Get().DeleteDirectory( *DemoDir, false, true );
		
		// Create a directory for this demo
		IFileManager::Get().MakeDirectory( *DemoDir, true );

		// Open file for writing
		FileAr.Reset( IFileManager::Get().CreateFileWriter( *FullDemoFilename, FILEWRITE_AllowRead ) );
		HeaderAr.Reset( IFileManager::Get().CreateFileWriter( *FullHeaderFilename ) );
		StreamerState = EStreamerState::Recording;

		CurrentCheckpointIndex = 0;

		// Set up replay info
		ReplayInfo.NetworkVersion = ReplayVersion.NetworkVersion;
		ReplayInfo.Changelist = ReplayVersion.Changelist;
		ReplayInfo.FriendlyName = FriendlyName;
	}

	// Notify immediately
	Delegate.ExecuteIfBound( FileAr.Get() != nullptr && HeaderAr.Get() != nullptr, bRecord );
}

void FNullNetworkReplayStreamer::StopStreaming()
{
	if (StreamerState == EStreamerState::Recording)
	{
		// Update metadata file with latest info
		TUniquePtr<FArchive> ReplayInfoFileAr(IFileManager::Get().CreateFileWriter(*GetInfoFilename(CurrentStreamName)));

		if (ReplayInfoFileAr.IsValid())
		{
			FString JsonString = ReplayInfo.ToJson();
			*ReplayInfoFileAr << JsonString; 
		}
	}

	HeaderAr.Reset();
	FileAr.Reset();
	MetadataFileAr.Reset();

	CurrentStreamName.Empty();
	StreamerState = EStreamerState::Idle;
}

FArchive* FNullNetworkReplayStreamer::GetHeaderArchive()
{
	return HeaderAr.Get();
}

FArchive* FNullNetworkReplayStreamer::GetStreamingArchive()
{
	return FileAr.Get();
}

FArchive* FNullNetworkReplayStreamer::GetMetadataArchive()
{
	check( StreamerState != EStreamerState::Idle );

	// Create the metadata archive on-demand
	if (!MetadataFileAr)
	{
		switch (StreamerState)
		{
			case EStreamerState::Recording:
				MetadataFileAr.Reset( IFileManager::Get().CreateFileWriter( *GetMetadataFilename(CurrentStreamName) ) );
				break;

			case EStreamerState::Playback:
				MetadataFileAr.Reset( IFileManager::Get().CreateFileReader( *GetMetadataFilename(CurrentStreamName) ) );
				break;

			default:
				break;
		}
	}

	return MetadataFileAr.Get();
}

void FNullNetworkReplayStreamer::UpdateTotalDemoTime(uint32 TimeInMS)
{
	ReplayInfo.LengthInMS = TimeInMS;
}

bool FNullNetworkReplayStreamer::IsLive() const
{
	return IsNamedStreamLive(CurrentStreamName);
}

bool FNullNetworkReplayStreamer::IsNamedStreamLive( const FString& StreamName ) const
{
	// If the directory for this stream doesn't exist, it can't possibly be live.
	if (!IFileManager::Get().DirectoryExists(*GetStreamDirectory(StreamName)))
	{
		return false;
	}

	// If the metadata file doesn't exist, this is a live stream.
	const int64 MetadataFileSize = IFileManager::Get().FileSize(*GetMetadataFilename(StreamName));
	return MetadataFileSize == INDEX_NONE;
}

void FNullNetworkReplayStreamer::DeleteFinishedStream( const FString& StreamName, const FOnDeleteFinishedStreamComplete& Delegate ) const
{
	// Live streams can't be deleted
	if (IsNamedStreamLive(StreamName))
	{
		UE_LOG(LogNullReplay, Log, TEXT("Can't delete network replay stream %s because it is live!"), *StreamName);
		Delegate.ExecuteIfBound(false);
		return;
	}

	// Delete the directory with the specified name in the Saved/Demos directory
	const FString DemoName = GetStreamDirectory(StreamName);

	const bool DeleteSucceeded = IFileManager::Get().DeleteDirectory( *DemoName, false, true );

	Delegate.ExecuteIfBound(DeleteSucceeded);
}

void FNullNetworkReplayStreamer::EnumerateStreams( const FNetworkReplayVersion& ReplayVersion, const FOnEnumerateStreamsComplete& Delegate )
{
	// Simply returns a stream for each folder in the Saved/Demos directory
	const FString WildCardPath = GetDemoPath() + TEXT( "*" );

	TArray<FString> DirectoryNames;
	IFileManager::Get().FindFiles( DirectoryNames, *WildCardPath, false, true );

	TArray<FNetworkReplayStreamInfo> Results;

	for (const FString& Directory : DirectoryNames)
	{
		// Assume there will be one file with a .demo extension in the directory
		const FString FullDemoFilePath = GetDemoFilename(Directory);

		FNetworkReplayStreamInfo Info;
		Info.SizeInBytes = IFileManager::Get().FileSize( *FullDemoFilePath );
		
		if (Info.SizeInBytes != INDEX_NONE)
		{
			// Read stored info for this replay
			FNullReplayInfo StoredReplayInfo = ReadReplayInfo( Directory );
			
			// Check version. NetworkVersion and changelist of 0 will ignore version check.
			const bool NetworkVersionMatches = ReplayVersion.NetworkVersion == StoredReplayInfo.NetworkVersion;
			const bool ChangelistMatches = ReplayVersion.Changelist == StoredReplayInfo.Changelist;

			const bool NetworkVersionPasses = ReplayVersion.NetworkVersion == 0 || NetworkVersionMatches;
			const bool ChangelistPasses = ReplayVersion.Changelist == 0 || ChangelistMatches;

			if ( NetworkVersionPasses && ChangelistPasses )
			{
				Info.Name = Directory;
				Info.Timestamp = IFileManager::Get().GetTimeStamp( *FullDemoFilePath );
				Info.bIsLive = IsNamedStreamLive( Directory );
				Info.LengthInMS = StoredReplayInfo.LengthInMS;
				Info.FriendlyName = StoredReplayInfo.FriendlyName;

				// Live streams not supported yet
				if (!Info.bIsLive)
				{
					Results.Add(Info);
				}
			}
		}
	}

	Delegate.ExecuteIfBound(Results);
}

FArchive* FNullNetworkReplayStreamer::GetCheckpointArchive()
{
	// If the archive is null, and the API is being used properly, the caller is writing a checkpoint...
	if ( CheckpointAr.Get() == nullptr )
	{
		// Create a file writer for the next checkpoint index.
		check(StreamerState != EStreamerState::Playback);

		FString NextCheckpointFileName = FString::Printf( TEXT( "checkpoint%d" ), CurrentCheckpointIndex );

		UE_LOG(LogNullReplay, Log, TEXT("FNullNetworkReplayStreamer::GetCheckpointArchive. Creating new checkpoint file."));

		CheckpointAr.Reset( IFileManager::Get().CreateFileWriter( *GetCheckpointFilename(CurrentStreamName, CurrentCheckpointIndex) ) );
	}

	return CheckpointAr.Get();
}

void FNullNetworkReplayStreamer::FlushCheckpoint(const uint32 TimeInMS)
{
	UE_LOG(LogNullReplay, Log, TEXT("FNullNetworkReplayStreamer::FlushCheckpoint. TimeInMS: %u"), TimeInMS);

	check(FileAr.Get() != nullptr);

	// The file writer archive will finalize the file on disk on destruction. The new file will be created
	// next time the driver calls GetCheckpointArchive.
	CheckpointAr.Reset();

	// Also write the event description file to disk with a corresponding checkpoint index, so they can be correlated later.
	TUniquePtr<FArchive> EventFileAr(IFileManager::Get().CreateFileWriter(*GetEventFilename(CurrentStreamName, CurrentCheckpointIndex)));


	if (EventFileAr.Get() != nullptr)
	{
		FNullCheckpointListItem CheckpointEvent;
		CheckpointEvent.Group = TEXT("checkpoint");
		CheckpointEvent.Metadata = FString::Printf( TEXT("%ld"), FileAr->Tell() );
		CheckpointEvent.Time1 = TimeInMS;
		CheckpointEvent.Time2 = TimeInMS;

		FString EventJsonString = CheckpointEvent.ToJson();
		*EventFileAr << EventJsonString;
	}

	++CurrentCheckpointIndex;
}

void FNullNetworkReplayStreamer::GotoCheckpointIndex(const int32 CheckpointIndex, const FOnCheckpointReadyDelegate& Delegate)
{
	GotoCheckpointIndexInternal(CheckpointIndex, Delegate, -1);
}

void FNullNetworkReplayStreamer::GotoCheckpointIndexInternal(int32 CheckpointIndex, const FOnCheckpointReadyDelegate& Delegate, int32 TimeInMS)
{
	check( FileAr.Get() != nullptr);

	if ( CheckpointIndex == -1 )
	{
		// Create a dummy checkpoint archive to indicate this is the first checkpoint
		CheckpointAr.Reset(new FArchive);

		FileAr->Seek(0);
		
		Delegate.ExecuteIfBound( true, TimeInMS );
		return;
	}

	// Attempt to open the checkpoint file for the given index. Will fail if file doesn't exist.
	const FString CheckpointFilename = GetCheckpointFilename(CurrentStreamName, CheckpointIndex);
	CheckpointAr.Reset( IFileManager::Get().CreateFileReader( *CheckpointFilename ) );

	if ( CheckpointAr.Get() == nullptr )
	{
		UE_LOG(LogNullReplay, Log, TEXT("FNullNetworkReplayStreamer::GotoCheckpointIndex. Index: %i. Couldn't open checkpoint file %s"), CheckpointIndex, *CheckpointFilename);
		Delegate.ExecuteIfBound( false, TimeInMS );
		return;
	}

	// Open and deserialize the corresponding event, this tells us where we need to seek to
	// in the main replay file to sync up with the checkpoint we're loading.
	const FString EventFilename = GetEventFilename(CurrentStreamName, CheckpointIndex);
	TUniquePtr<FArchive> EventFile( IFileManager::Get().CreateFileReader(*EventFilename));
	if (EventFile.Get() != nullptr)
	{
		FString JsonString;
		*EventFile << JsonString;

		FNullCheckpointListItem Item;
		Item.FromJson(JsonString);

		FileAr->Seek( FCString::Atoi64( *Item.Metadata ) );
	}

	Delegate.ExecuteIfBound( true, TimeInMS );
}

void FNullNetworkReplayStreamer::GotoTimeInMS(const uint32 TimeInMS, const FOnCheckpointReadyDelegate& Delegate)
{
	// Enumerate all the events in the events folder, since we need to know what times the checkpoints correlate with
	TArray<FNullCheckpointListItem> Checkpoints;

	const FString EventBaseName = FPaths::Combine( *GetStreamDirectory(CurrentStreamName), TEXT( "events" ), TEXT( "event" ) );

	int CurrentEventIndex = 0;

	// Try to load every event in order until one is missing
	while ( true )
	{
		const FString CheckEventName = EventBaseName + FString::FromInt(CurrentEventIndex);

		TUniquePtr<FArchive> EventFile( IFileManager::Get().CreateFileReader(*CheckEventName) );

		if ( EventFile.Get() != nullptr )
		{
			FString JsonString;
			*EventFile << JsonString;

			FNullCheckpointListItem Item;
			Item.FromJson(JsonString);

			Checkpoints.Add(Item);
		}
		else
		{
			break;
		}

		CurrentEventIndex++;
	}

	int32 CheckpointIndex = -1;

	if ( Checkpoints.Num() > 0 && TimeInMS >= Checkpoints[ Checkpoints.Num() - 1 ].Time1 )
	{
		// If we're after the very last checkpoint, that's the one we want
		CheckpointIndex = Checkpoints.Num() - 1;
	}
	else
	{
		// Checkpoints should be sorted by time, return the checkpoint that exists right before the current time
		// For fine scrubbing, we'll fast forward the rest of the way
		// NOTE - If we're right before the very first checkpoint, we'll return -1, which is what we want when we want to start from the very beginning
		for ( int i = 0; i < Checkpoints.Num(); i++ )
		{
			if ( TimeInMS < Checkpoints[i].Time1 )
			{
				CheckpointIndex = i - 1;
				break;
			}
		}
	}

	int32 ExtraSkipTimeInMS = TimeInMS;

	if ( CheckpointIndex >= 0 )
	{
		// Subtract off checkpoint time so we pass in the leftover to the engine to fast forward through for the fine scrubbing part
		ExtraSkipTimeInMS = TimeInMS - Checkpoints[ CheckpointIndex ].Time1;
	}

	GotoCheckpointIndexInternal( CheckpointIndex, Delegate, ExtraSkipTimeInMS );
}

IMPLEMENT_MODULE( FNullNetworkReplayStreamingFactory, NullNetworkReplayStreaming )

TSharedPtr< INetworkReplayStreamer > FNullNetworkReplayStreamingFactory::CreateReplayStreamer() 
{
	return TSharedPtr< INetworkReplayStreamer >( new FNullNetworkReplayStreamer );
}
