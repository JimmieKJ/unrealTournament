// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MediaAssetsPrivatePCH.h"
#include "IMediaTrackAudioDetails.h"
#include "MediaSampleQueue.h"
#include "MediaSoundWave.h"


/* UMediaSoundWave structors
 *****************************************************************************/

UMediaSoundWave::UMediaSoundWave( const FObjectInitializer& ObjectInitializer )
	: Super(ObjectInitializer)
	, AudioQueue(MakeShareable(new FMediaSampleQueue))
{
	bLooping = false;
	bProcedural = true;
	Duration = INDEFINITELY_LOOPING_DURATION;
}


UMediaSoundWave::~UMediaSoundWave()
{
	if (AudioTrack.IsValid())
	{
		AudioTrack->RemoveSink(AudioQueue);
	}
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


/* UMediaSoundWave implementation
 *****************************************************************************/

void UMediaSoundWave::InitializeTrack()
{
	// assign new media player asset
	if (CurrentMediaPlayer != MediaPlayer)
	{
		if (CurrentMediaPlayer != nullptr)
		{
			CurrentMediaPlayer->OnMediaChanged().RemoveAll(this);
		}

		CurrentMediaPlayer = MediaPlayer;

		if (MediaPlayer != nullptr)
		{
			MediaPlayer->OnMediaChanged().AddUObject(this, &UMediaSoundWave::HandleMediaPlayerMediaChanged);
		}	
	}

	// disconnect from current track
	if (AudioTrack.IsValid())
	{
		AudioTrack->RemoveSink(AudioQueue);
	}

	AudioTrack.Reset();

	// initialize from new track
	if (MediaPlayer != nullptr)
	{
		IMediaPlayerPtr Player = MediaPlayer->GetPlayer();

		if (Player .IsValid())
		{
			AudioTrack = Player ->GetTrack(AudioTrackIndex, EMediaTrackTypes::Audio);

			if (!AudioTrack.IsValid())
			{
				AudioTrack = Player->GetFirstTrack(EMediaTrackTypes::Audio);

				if (AudioTrack.IsValid())
				{
					AudioTrackIndex = AudioTrack->GetIndex();
				}
			}
		}
	}

	if (AudioTrack.IsValid())
	{
		NumChannels = AudioTrack->GetAudioDetails().GetNumChannels();
		SampleRate = AudioTrack->GetAudioDetails().GetSamplesPerSecond();
	}
	else
	{
		NumChannels = 0;
		SampleRate = 0;
	}

	// connect to new track
	if (AudioTrack.IsValid())
	{
		AudioTrack->AddSink(AudioQueue);
	}
}


/* UMediaSoundWave callbacks
 *****************************************************************************/

void UMediaSoundWave::HandleMediaPlayerMediaChanged()
{
	InitializeTrack();
}
