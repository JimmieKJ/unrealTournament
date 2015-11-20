// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MediaAssetsPrivatePCH.h"
#include "IMediaAudioTrack.h"
#include "MediaSampleQueue.h"
#include "MediaSoundWave.h"


/* UMediaSoundWave structors
 *****************************************************************************/

UMediaSoundWave::UMediaSoundWave( const FObjectInitializer& ObjectInitializer )
	: Super(ObjectInitializer)
	, AudioTrackIndex(INDEX_NONE)
	, AudioQueue(MakeShareable(new FMediaSampleQueue))
{
	bSetupDelegates = false;
	bLooping = false;
	bProcedural = true;
	Duration = INDEFINITELY_LOOPING_DURATION;
}

/* UMediaSoundWave interface
 *****************************************************************************/

TSharedPtr<IMediaPlayer> UMediaSoundWave::GetPlayer() const
{
	if (MediaPlayer == nullptr)
	{
		return nullptr;
	}

	return MediaPlayer->GetPlayer();
}


void UMediaSoundWave::SetMediaPlayer( UMediaPlayer* InMediaPlayer )
{
	MediaPlayer = InMediaPlayer;

	InitializeTrack();
}


/* USoundWave overrides
 *****************************************************************************/

int32 UMediaSoundWave::GeneratePCMData( uint8* PCMData, const int32 SamplesNeeded )
{
	// drain media sample queue
	TSharedPtr<TArray<uint8>, ESPMode::ThreadSafe> Sample;

	while (AudioQueue->Dequeue(Sample))
	{
		QueuedAudio.Append(*Sample);
	}

	// get requested samples
	if (SamplesNeeded <= 0)
	{
		return 0;
	}

	const int32 SamplesAvailable = QueuedAudio.Num() / sizeof(int16);
	const int32 BytesToCopy = FMath::Min<int32>(SamplesNeeded, SamplesAvailable) * sizeof(int16);

	if (BytesToCopy > 0)
	{
		FMemory::Memcpy((void*)PCMData, &QueuedAudio[0], BytesToCopy);
		QueuedAudio.RemoveAt(0, BytesToCopy);
	}

	return BytesToCopy;
}


FByteBulkData* UMediaSoundWave::GetCompressedData( FName Format )
{
	return nullptr;
}


int32 UMediaSoundWave::GetResourceSizeForFormat( FName Format )
{
	return 0;
}


void UMediaSoundWave::InitAudioResource( FByteBulkData& CompressedData )
{
	check(false); // should never be pushing compressed data to this class
}


bool UMediaSoundWave::InitAudioResource( FName Format )
{
	return true;
}


/* UObject overrides
 *****************************************************************************/

void UMediaSoundWave::GetAssetRegistryTags( TArray<FAssetRegistryTag>& OutTags ) const
{
}


SIZE_T UMediaSoundWave::GetResourceSize(EResourceSizeMode::Type Mode)
{
	return 0;
}


void UMediaSoundWave::Serialize( FArchive& Ar )
{
	// do not call the USoundWave version of serialize
	USoundBase::Serialize(Ar);
}

void UMediaSoundWave::PostLoad()
{
	Super::PostLoad();

	if (!HasAnyFlags(RF_ClassDefaultObject) && !GIsBuildMachine)
	{
		InitializeTrack();
	}
}

void UMediaSoundWave::BeginDestroy()
{
	Super::BeginDestroy();

	if (AudioTrack.IsValid())
	{
		AudioTrack->GetStream().RemoveSink(AudioQueue);
		AudioTrack.Reset();
	}

	if (CurrentMediaPlayer.IsValid())
	{
		CurrentMediaPlayer->OnTracksChanged().RemoveAll(this);
		CurrentMediaPlayer.Reset();
	}
}


/* UMediaSoundWave implementation
 *****************************************************************************/

void UMediaSoundWave::InitializeTrack()
{
	// assign new media player asset
	if (CurrentMediaPlayer != MediaPlayer || !bSetupDelegates)
	{
		if (CurrentMediaPlayer != nullptr)
		{
			CurrentMediaPlayer->OnTracksChanged().RemoveAll(this);
		}

		CurrentMediaPlayer = MediaPlayer;

		if (MediaPlayer != nullptr)
		{
			MediaPlayer->OnTracksChanged().AddUObject(this, &UMediaSoundWave::HandleMediaPlayerTracksChanged);
		}
		bSetupDelegates = true;
	}

	// disconnect from current track
	if (AudioTrack.IsValid())
	{
		AudioTrack->GetStream().RemoveSink(AudioQueue);
	}

	AudioTrack.Reset();

	// initialize from new track
	if (MediaPlayer != nullptr)
	{
		IMediaPlayerPtr Player = MediaPlayer->GetPlayer();

		if (Player.IsValid())
		{
			auto AudioTracks = Player->GetAudioTracks();

			if (AudioTracks.IsValidIndex(AudioTrackIndex))
			{
				AudioTrack = AudioTracks[AudioTrackIndex];
			}
			else if (AudioTracks.Num() > 0)
			{
				AudioTrack = AudioTracks[0];
				AudioTrackIndex = 0;
			}
		}
	}

	if (AudioTrack.IsValid())
	{
		NumChannels = AudioTrack->GetNumChannels();
		SampleRate = AudioTrack->GetSamplesPerSecond();
	}
	else
	{
		NumChannels = 0;
		SampleRate = 0;
	}

	// connect to new track
	if (AudioTrack.IsValid())
	{
		AudioTrack->GetStream().AddSink(AudioQueue);
	}
}


/* UMediaSoundWave callbacks
 *****************************************************************************/

void UMediaSoundWave::HandleMediaPlayerTracksChanged()
{
	InitializeTrack();
}
