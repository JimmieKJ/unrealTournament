// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "InMemoryNetworkReplayStreaming.h"
#include "Paths.h"
#include "EngineVersion.h"
#include "Guid.h"
#include "DateTime.h"

DEFINE_LOG_CATEGORY_STATIC( LogMemoryReplay, Log, All );

static FString GetAutomaticDemoName()
{
	return FGuid::NewGuid().ToString();
}

void FInMemoryNetworkReplayStreamer::StartStreaming( const FString& CustomName, const FString& FriendlyName, const TArray< FString >& UserNames, bool bRecord, const FNetworkReplayVersion& ReplayVersion, const FOnStreamReadyDelegate& Delegate )
{
	if ( CustomName.IsEmpty() )
	{
		if ( bRecord )
		{
			// If we're recording and the caller didn't provide a name, generate one automatically
			CurrentStreamName = GetAutomaticDemoName();
		}
		else
		{
			// Can't play a replay if the user didn't provide a name!
			Delegate.ExecuteIfBound( false, bRecord );
			return;
		}
	}
	else
	{
		CurrentStreamName = CustomName;
	}

	if ( !bRecord )
	{
		FInMemoryReplay* FoundReplay = GetCurrentReplay();
		if (FoundReplay == nullptr)
		{
			Delegate.ExecuteIfBound(false, bRecord);
			return;
		}

		FileAr.Reset(new FMemoryReader(FoundReplay->Stream));
		HeaderAr.Reset(new FMemoryReader(FoundReplay->Header));
		StreamerState = EStreamerState::Playback;
	}
	else
	{
		// Add or overwrite a demo with this name
		TUniquePtr<FInMemoryReplay> NewReplay(new FInMemoryReplay);

		NewReplay->StreamInfo.Name = CurrentStreamName;
		NewReplay->StreamInfo.FriendlyName = FriendlyName;
		NewReplay->StreamInfo.Timestamp = FDateTime::Now();
		NewReplay->StreamInfo.bIsLive = true;
		NewReplay->StreamInfo.Changelist = ReplayVersion.Changelist;
		NewReplay->NetworkVersion = ReplayVersion.NetworkVersion;

		// Open archives for writing
		FileAr.Reset(new FMemoryWriter(NewReplay->Stream));
		HeaderAr.Reset(new FMemoryWriter(NewReplay->Header));

		OwningFactory->Replays.Add(CurrentStreamName, MoveTemp(NewReplay));
		
		StreamerState = EStreamerState::Recording;
	}

	// Notify immediately
	Delegate.ExecuteIfBound( FileAr.Get() != nullptr && HeaderAr.Get() != nullptr, bRecord );
}

void FInMemoryNetworkReplayStreamer::StopStreaming()
{
	if (StreamerState == EStreamerState::Recording)
	{
		FInMemoryReplay* FoundReplay = GetCurrentReplayChecked();

		FoundReplay->StreamInfo.SizeInBytes = FoundReplay->Header.Num() + FoundReplay->Stream.Num() + FoundReplay->Metadata.Num();
		for(const auto& Checkpoint : FoundReplay->Checkpoints)
		{
			FoundReplay->StreamInfo.SizeInBytes += Checkpoint.Data.Num();
		}

		FoundReplay->StreamInfo.bIsLive = false;
	}

	HeaderAr.Reset();
	FileAr.Reset();
	MetadataFileAr.Reset();

	CurrentStreamName.Empty();
	StreamerState = EStreamerState::Idle;
}

FArchive* FInMemoryNetworkReplayStreamer::GetHeaderArchive()
{
	return HeaderAr.Get();
}

FArchive* FInMemoryNetworkReplayStreamer::GetStreamingArchive()
{
	return FileAr.Get();
}

FArchive* FInMemoryNetworkReplayStreamer::GetMetadataArchive()
{
	check( StreamerState != EStreamerState::Idle );

	// Create the metadata archive on-demand
	if (!MetadataFileAr)
	{
		FInMemoryReplay* FoundReplay = GetCurrentReplayChecked();

		switch (StreamerState)
		{
			case EStreamerState::Recording:
			{
				MetadataFileAr.Reset(new FMemoryWriter(FoundReplay->Metadata));
				break;
			}
			case EStreamerState::Playback:
				MetadataFileAr.Reset(new FMemoryReader(FoundReplay->Metadata));
				break;

			default:
				break;
		}
	}

	return MetadataFileAr.Get();
}

void FInMemoryNetworkReplayStreamer::UpdateTotalDemoTime(uint32 TimeInMS)
{
	check(StreamerState == EStreamerState::Recording);

	FInMemoryReplay* FoundReplay = GetCurrentReplayChecked();

	FoundReplay->StreamInfo.LengthInMS = TimeInMS;
}

uint32 FInMemoryNetworkReplayStreamer::GetTotalDemoTime() const
{
	check(StreamerState != EStreamerState::Idle);

	const FInMemoryReplay* FoundReplay = GetCurrentReplayChecked();

	return FoundReplay->StreamInfo.LengthInMS;
}

bool FInMemoryNetworkReplayStreamer::IsDataAvailable() const
{
	// Assumptions:
	// 1. All streamer instances run on the same thread, not simultaneously
	// 2. A recording DemoNetDriver will write either no frames or entire frames each time it ticks
	check(StreamerState == EStreamerState::Playback);

	return FileAr.IsValid() && FileAr->Tell() < FileAr->TotalSize();
}

bool FInMemoryNetworkReplayStreamer::IsLive() const
{
	return IsNamedStreamLive(CurrentStreamName);
}

bool FInMemoryNetworkReplayStreamer::IsNamedStreamLive( const FString& StreamName ) const
{
	TUniquePtr<FInMemoryReplay>* FoundReplay = OwningFactory->Replays.Find(StreamName);
	if (FoundReplay != nullptr)
	{
		return (*FoundReplay)->StreamInfo.bIsLive;
	}

	return false;
}

void FInMemoryNetworkReplayStreamer::DeleteFinishedStream( const FString& StreamName, const FOnDeleteFinishedStreamComplete& Delegate ) const
{
	// Danger! Deleting a stream that is still being read by another streaming instance is not supported!

	// Live streams can't be deleted
	if (IsNamedStreamLive(StreamName))
	{
		UE_LOG(LogMemoryReplay, Log, TEXT("Can't delete network replay stream %s because it is live!"), *StreamName);
		Delegate.ExecuteIfBound(false);
		return;
	}

	const int32 NumRemoved = OwningFactory->Replays.Remove(StreamName);

	Delegate.ExecuteIfBound(NumRemoved > 0);
}

void FInMemoryNetworkReplayStreamer::EnumerateStreams( const FNetworkReplayVersion& ReplayVersion, const FString& UserString, const FString& MetaString, const FOnEnumerateStreamsComplete& Delegate )
{
	EnumerateStreams( ReplayVersion, UserString, MetaString, TArray< FString >(), Delegate );
}

void FInMemoryNetworkReplayStreamer::EnumerateStreams( const FNetworkReplayVersion& ReplayVersion, const FString& UserString, const FString& MetaString, const TArray< FString >& ExtraParms, const FOnEnumerateStreamsComplete& Delegate )
{
	TArray<FNetworkReplayStreamInfo> Results;

	for ( const auto& StreamPair : OwningFactory->Replays )
	{
		// Check version. NetworkVersion and changelist of 0 will ignore version check.
		const bool NetworkVersionMatches = ReplayVersion.NetworkVersion == StreamPair.Value->NetworkVersion;
		const bool ChangelistMatches = ReplayVersion.Changelist == StreamPair.Value->StreamInfo.Changelist;

		const bool NetworkVersionPasses = ReplayVersion.NetworkVersion == 0 || NetworkVersionMatches;
		const bool ChangelistPasses = ReplayVersion.Changelist == 0 || ChangelistMatches;

		if ( NetworkVersionPasses && ChangelistPasses )
		{
			Results.Add( StreamPair.Value->StreamInfo );
		}
	}

	Delegate.ExecuteIfBound( Results );
}

void FInMemoryNetworkReplayStreamer::AddUserToReplay( const FString& UserString )
{
	UE_LOG(LogMemoryReplay, Log, TEXT("FInMemoryNetworkReplayStreamer::AddUserToReplay is currently unsupported."));
}

void FInMemoryNetworkReplayStreamer::AddEvent( const uint32 TimeInMS, const FString& Group, const FString& Meta, const TArray<uint8>& Data )
{
	UE_LOG(LogMemoryReplay, Log, TEXT("FInMemoryNetworkReplayStreamer::AddEvent is currently unsupported."));
}

void FInMemoryNetworkReplayStreamer::EnumerateEvents( const FString& Group, const FEnumerateEventsCompleteDelegate& EnumerationCompleteDelegate )
{
	UE_LOG(LogMemoryReplay, Log, TEXT("FInMemoryNetworkReplayStreamer::EnumerateEvents is currently unsupported."));
}

void FInMemoryNetworkReplayStreamer::EnumerateEvents(const FString& ReplayName, const FString& Group, const FEnumerateEventsCompleteDelegate& EnumerationCompleteDelegate)
{
	UE_LOG(LogMemoryReplay, Log, TEXT("FInMemoryNetworkReplayStreamer::EnumerateEvents is currently unsupported."));
}

void FInMemoryNetworkReplayStreamer::RequestEventData(const FString& EventID, const FOnRequestEventDataComplete& RequestEventDataComplete)
{
	UE_LOG(LogMemoryReplay, Log, TEXT("FInMemoryNetworkReplayStreamer::RequestEventData is currently unsupported."));
}

void FInMemoryNetworkReplayStreamer::SearchEvents(const FString& EventGroup, const FOnEnumerateStreamsComplete& Delegate)
{
	UE_LOG(LogMemoryReplay, Log, TEXT("FInMemoryNetworkReplayStreamer::SearchEvents is currently unsupported."));
}

void FInMemoryNetworkReplayStreamer::KeepReplay(const FString& ReplayName, const bool bKeep)
{
	UE_LOG(LogMemoryReplay, Log, TEXT("FInMemoryNetworkReplayStreamer::KeepReplay is currently unsupported."));
}

FArchive* FInMemoryNetworkReplayStreamer::GetCheckpointArchive()
{
	// If the archive is null, and the API is being used properly, the caller is writing a checkpoint...
	if ( CheckpointAr.Get() == nullptr )
	{
		check(StreamerState != EStreamerState::Playback);

		UE_LOG(LogMemoryReplay, Log, TEXT("FInMemoryNetworkReplayStreamer::GetCheckpointArchive. Creating new checkpoint."));

		FInMemoryReplay* FoundReplay = GetCurrentReplayChecked();

		const int32 NewCheckpointIndex = FoundReplay->Checkpoints.Add(FInMemoryReplay::FCheckpoint());
		CheckpointAr.Reset(new FMemoryWriter(FoundReplay->Checkpoints[NewCheckpointIndex].Data));
	}

	return CheckpointAr.Get();
}

void FInMemoryNetworkReplayStreamer::FlushCheckpoint(const uint32 TimeInMS)
{
	UE_LOG(LogMemoryReplay, Log, TEXT("FInMemoryNetworkReplayStreamer::FlushCheckpoint. TimeInMS: %u"), TimeInMS);

	check(FileAr.Get() != nullptr);

	// Finalize the checkpoint data.
	CheckpointAr.Reset();
	
	FInMemoryReplay* FoundReplay = GetCurrentReplayChecked();

	FoundReplay->Checkpoints.Last().TimeInMS = TimeInMS;
	FoundReplay->Checkpoints.Last().StreamByteOffset = FileAr->Tell();
}

void FInMemoryNetworkReplayStreamer::GotoCheckpointIndex(const int32 CheckpointIndex, const FOnCheckpointReadyDelegate& Delegate)
{
	GotoCheckpointIndexInternal(CheckpointIndex, Delegate, -1);
}

void FInMemoryNetworkReplayStreamer::GotoCheckpointIndexInternal(int32 CheckpointIndex, const FOnCheckpointReadyDelegate& Delegate, int32 TimeInMS)
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

	FInMemoryReplay* FoundReplay = GetCurrentReplayChecked();

	if (!FoundReplay->Checkpoints.IsValidIndex(CheckpointIndex))
	{
		UE_LOG(LogMemoryReplay, Log, TEXT("FNullNetworkReplayStreamer::GotoCheckpointIndex. Index %i is out of bounds."), CheckpointIndex);
		Delegate.ExecuteIfBound( false, TimeInMS );
		return;
	}

	CheckpointAr.Reset(new FMemoryReader(FoundReplay->Checkpoints[CheckpointIndex].Data));

	FileAr->Seek( FoundReplay->Checkpoints[CheckpointIndex].StreamByteOffset );

	Delegate.ExecuteIfBound( true, TimeInMS );
}

FInMemoryReplay* FInMemoryNetworkReplayStreamer::GetCurrentReplay()
{
	TUniquePtr<FInMemoryReplay>* FoundReplay = OwningFactory->Replays.Find(CurrentStreamName);
	if (FoundReplay != nullptr)
	{
		return FoundReplay->Get();
	}

	return nullptr;
}

FInMemoryReplay* FInMemoryNetworkReplayStreamer::GetCurrentReplay() const
{
	TUniquePtr<FInMemoryReplay>* FoundReplay = OwningFactory->Replays.Find(CurrentStreamName);
	if (FoundReplay != nullptr)
	{
		return FoundReplay->Get();
	}

	return nullptr;
}

FInMemoryReplay* FInMemoryNetworkReplayStreamer::GetCurrentReplayChecked()
{
	FInMemoryReplay* FoundReplay = GetCurrentReplay();
	check(FoundReplay != nullptr);
	
	return FoundReplay;
}

FInMemoryReplay* FInMemoryNetworkReplayStreamer::GetCurrentReplayChecked() const
{
	FInMemoryReplay* FoundReplay = GetCurrentReplay();
	check(FoundReplay != nullptr);
	
	return FoundReplay;
}

void FInMemoryNetworkReplayStreamer::GotoTimeInMS(const uint32 TimeInMS, const FOnCheckpointReadyDelegate& Delegate)
{
	int32 CheckpointIndex = -1;

	FInMemoryReplay* FoundReplay = GetCurrentReplayChecked();

	if ( FoundReplay->Checkpoints.Num() > 0 && TimeInMS >= FoundReplay->Checkpoints.Last().TimeInMS )
	{
		// If we're after the very last checkpoint, that's the one we want
		CheckpointIndex = FoundReplay->Checkpoints.Num() - 1;
	}
	else
	{
		// Checkpoints should be sorted by time, return the checkpoint that exists right before the current time
		// For fine scrubbing, we'll fast forward the rest of the way
		// NOTE - If we're right before the very first checkpoint, we'll return -1, which is what we want when we want to start from the very beginning
		for ( int i = 0; i < FoundReplay->Checkpoints.Num(); i++ )
		{
			if ( TimeInMS < FoundReplay->Checkpoints[i].TimeInMS )
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
		ExtraSkipTimeInMS = TimeInMS - FoundReplay->Checkpoints[ CheckpointIndex ].TimeInMS;
	}

	GotoCheckpointIndexInternal( CheckpointIndex, Delegate, ExtraSkipTimeInMS );
}

void FInMemoryNetworkReplayStreamer::Tick(float DeltaSeconds)
{
	
}

TStatId FInMemoryNetworkReplayStreamer::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(FInMemoryNetworkReplayStreamer, STATGROUP_Tickables);
}

IMPLEMENT_MODULE(FInMemoryNetworkReplayStreamingFactory, InMemoryNetworkReplayStreaming)

TSharedPtr<INetworkReplayStreamer> FInMemoryNetworkReplayStreamingFactory::CreateReplayStreamer() 
{
	return TSharedPtr<INetworkReplayStreamer>(new FInMemoryNetworkReplayStreamer(this));
}
