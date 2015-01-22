// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "Sound/SoundWaveStreaming.h"

USoundWaveStreaming::USoundWaveStreaming(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bProcedural = true;

}

void USoundWaveStreaming::QueueAudio( const uint8* AudioData, const int32 BufferSize )
{
	if (BufferSize == 0 || !ensure( ( BufferSize % sizeof( int16 ) ) == 0 ))
	{
		return;
	}
	const int32 Position = QueuedAudio.AddUninitialized( BufferSize );
	FMemory::Memcpy( &QueuedAudio[ Position ], AudioData, BufferSize );
}

int32 USoundWaveStreaming::GeneratePCMData( uint8* PCMData, const int32 SamplesNeeded )
{
	int32 SamplesAvailable = QueuedAudio.Num() / sizeof( int16 );

	// if delegate is bound and we don't have enough samples, call it so system can supply more
	if (SamplesNeeded > SamplesAvailable && OnSoundWaveStreamingUnderflow.IsBound())
	{
		OnSoundWaveStreamingUnderflow.Execute(this, SamplesNeeded);
		// Update available samples
		SamplesAvailable = QueuedAudio.Num() / sizeof( int16 );
	}

	if (SamplesAvailable > 0 && SamplesNeeded > 0)
	{
		const int32 SamplesToCopy = FMath::Min<int32>( SamplesNeeded, SamplesAvailable );
		const int32 BytesToCopy = SamplesToCopy * sizeof( int16 );

		FMemory::Memcpy( ( void* )PCMData,  &QueuedAudio[ 0 ], BytesToCopy );
		QueuedAudio.RemoveAt( 0, BytesToCopy );
		return BytesToCopy;
	}

	return 0;
}

void USoundWaveStreaming::ResetAudio()
{
	QueuedAudio.Empty();
}

int32 USoundWaveStreaming::GetAvailableAudioByteCount()
{
	return QueuedAudio.Num();
}

SIZE_T USoundWaveStreaming::GetResourceSize(EResourceSizeMode::Type Mode)
{
	return 0;
}

int32 USoundWaveStreaming::GetResourceSizeForFormat(FName Format)
{
	return 0;
}

void USoundWaveStreaming::GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const
{
	// SoundWaveStreaming should never be in the asset registry
	check(false);
}

FByteBulkData* USoundWaveStreaming::GetCompressedData(FName Format)
{
	// SoundWaveStreaming does not have compressed data and should generally not be asked about it
	return NULL;
}

void USoundWaveStreaming::Serialize( FArchive& Ar )
{
	// Do not call the USoundWave version of serialize
	USoundBase::Serialize( Ar );
}

void USoundWaveStreaming::InitAudioResource( FByteBulkData& CompressedData )
{
	// Should never be pushing compressed data to a SoundWaveStreaming
	check(false);
}

bool USoundWaveStreaming::InitAudioResource(FName Format)
{
	// Nothing to be done to initialize a USoundWaveStreaming
	return true;
}