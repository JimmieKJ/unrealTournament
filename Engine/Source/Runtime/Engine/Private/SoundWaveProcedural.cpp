// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "Sound/SoundWaveProcedural.h"

USoundWaveProcedural::USoundWaveProcedural(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bProcedural = true;
}

void USoundWaveProcedural::QueueAudio( const uint8* AudioData, const int32 BufferSize )
{
	if (BufferSize == 0 || !ensure( ( BufferSize % sizeof( int16 ) ) == 0 ))
	{
		return;
	}
	const int32 Position = QueuedAudio.AddUninitialized( BufferSize );
	FMemory::Memcpy( &QueuedAudio[ Position ], AudioData, BufferSize );
}

int32 USoundWaveProcedural::GeneratePCMData( uint8* PCMData, const int32 SamplesNeeded )
{
	int32 SamplesAvailable = QueuedAudio.Num() / sizeof( int16 );

	// if delegate is bound and we don't have enough samples, call it so system can supply more
	if (SamplesNeeded > SamplesAvailable && OnSoundWaveProceduralUnderflow.IsBound())
	{
		OnSoundWaveProceduralUnderflow.Execute(this, SamplesNeeded);
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

void USoundWaveProcedural::ResetAudio()
{
	QueuedAudio.Empty();
}

int32 USoundWaveProcedural::GetAvailableAudioByteCount()
{
	return QueuedAudio.Num();
}

SIZE_T USoundWaveProcedural::GetResourceSize(EResourceSizeMode::Type Mode)
{
	return 0;
}

int32 USoundWaveProcedural::GetResourceSizeForFormat(FName Format)
{
	return 0;
}

void USoundWaveProcedural::GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const
{
	// SoundWaveProcedural should never be in the asset registry
	check(false);
}

FByteBulkData* USoundWaveProcedural::GetCompressedData(FName Format)
{
	// SoundWaveProcedural does not have compressed data and should generally not be asked about it
	return nullptr;
}

void USoundWaveProcedural::Serialize( FArchive& Ar )
{
	// Do not call the USoundWave version of serialize
	USoundBase::Serialize( Ar );
}

void USoundWaveProcedural::InitAudioResource( FByteBulkData& CompressedData )
{
	// Should never be pushing compressed data to a SoundWaveProcedural
	check(false);
}

bool USoundWaveProcedural::InitAudioResource(FName Format)
{
	// Nothing to be done to initialize a USoundWaveProcedural
	return true;
}