// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#include "EnginePrivate.h"
#include "OpusAudioInfo.h"
#include "IAudioFormat.h"
#include "Sound/SoundWave.h"

#define WITH_OPUS (PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX)

#if WITH_OPUS
#include "opus_multistream.h"
#endif

#define USE_UE4_MEM_ALLOC 1
#define OPUS_MAX_FRAME_SIZE_MS 120
#define SAMPLE_SIZE			( ( uint32 )sizeof( short ) )

///////////////////////////////////////////////////////////////////////////////////////
// Followed pattern used in opus_multistream_encoder.c - this will allow us to setup //
// a multistream decoder without having to save extra information for every asset.   //
///////////////////////////////////////////////////////////////////////////////////////
struct UnrealChannelLayout{
	int32 NumStreams;
	int32 NumCoupledStreams;
	uint8 Mapping[8];
};

/* Index is NumChannels-1*/
static const UnrealChannelLayout UnrealMappings[8] = {
	{ 1, 0, { 0 } },                      /* 1: mono */
	{ 1, 1, { 0, 1 } },                   /* 2: stereo */
	{ 2, 1, { 0, 1, 2 } },                /* 3: 1-d surround */
	{ 2, 2, { 0, 1, 2, 3 } },             /* 4: quadraphonic surround */
	{ 3, 2, { 0, 1, 4, 2, 3 } },          /* 5: 5-channel surround */
	{ 4, 2, { 0, 1, 4, 5, 2, 3 } },       /* 6: 5.1 surround */
	{ 4, 3, { 0, 1, 4, 6, 2, 3, 5 } },    /* 7: 6.1 surround */
	{ 5, 3, { 0, 1, 6, 7, 2, 3, 4, 5 } }, /* 8: 7.1 surround */
};

/*------------------------------------------------------------------------------------
FOpusDecoderWrapper
------------------------------------------------------------------------------------*/
struct FOpusDecoderWrapper
{
	FOpusDecoderWrapper(uint16 SampleRate, uint8 NumChannels)
	{
#if WITH_OPUS
		check(NumChannels <= 8);
		const UnrealChannelLayout& Layout = UnrealMappings[NumChannels-1];
	#if USE_UE4_MEM_ALLOC
		int32 DecSize = opus_multistream_decoder_get_size(Layout.NumStreams, Layout.NumCoupledStreams);
		Decoder = (OpusMSDecoder*)FMemory::Malloc(DecSize);
		DecError = opus_multistream_decoder_init(Decoder, SampleRate, NumChannels, Layout.NumStreams, Layout.NumCoupledStreams, Layout.Mapping);
	#else
		Decoder = opus_multistream_decoder_create(SampleRate, NumChannels, Layout.NumStreams, Layout.NumCoupledStreams, Layout.Mapping, &DecError);
	#endif
#endif
	}

	~FOpusDecoderWrapper()
	{
#if WITH_OPUS
	#if USE_UE4_MEM_ALLOC
		FMemory::Free(Decoder);
	#else
		opus_multistream_encoder_destroy(Decoder);
	#endif
#endif
	}

	int32 Decode(const uint8* FrameData, uint16 FrameSize, int16* OutPCMData, int32 SampleSize)
	{
#if WITH_OPUS
		return opus_multistream_decode(Decoder, FrameData, FrameSize, OutPCMData, SampleSize, 0);
#else
		return -1;
#endif
	}

	bool WasInitialisedSuccessfully() const
	{
#if WITH_OPUS
		return DecError == OPUS_OK;
#else
		return false;
#endif
	}

private:
#if WITH_OPUS
	OpusMSDecoder* Decoder;
	int32 DecError;
#endif
};

/*------------------------------------------------------------------------------------
FOpusAudioInfo.
------------------------------------------------------------------------------------*/
FOpusAudioInfo::FOpusAudioInfo(void)
	: OpusDecoderWrapper(NULL)
	, SrcBufferData(NULL)
	, SrcBufferDataSize(0)
	, SrcBufferOffset(0)
	, AudioDataOffset(0)
	, SampleRate(0)
	, TrueSampleCount(0)
	, CurrentSampleCount(0)
	, NumChannels(0)
	, MaxFrameSizeSamples(0)
	, SampleStride(0)
	, LastPCMByteSize(0)
	, LastPCMOffset(0)
	, bStoringEndOfFile(false)
	, StreamingSoundWave(NULL)
	, CurrentChunkIndex(0)
{
}

FOpusAudioInfo::~FOpusAudioInfo(void)
{
	if (OpusDecoderWrapper != NULL)
	{
		delete OpusDecoderWrapper;
	}
}

size_t FOpusAudioInfo::Read(void *ptr, uint32 size)
{
	size_t BytesToRead = FMath::Min(size, SrcBufferDataSize - SrcBufferOffset);
	if (BytesToRead > 0)
	{
		FMemory::Memcpy(ptr, SrcBufferData + SrcBufferOffset, BytesToRead);
		SrcBufferOffset += BytesToRead;
	}
	return(BytesToRead);
}

bool FOpusAudioInfo::ReadCompressedInfo(const uint8* InSrcBufferData, uint32 InSrcBufferDataSize, struct FSoundQualityInfo* QualityInfo)
{
	SrcBufferData = InSrcBufferData;
	SrcBufferDataSize = InSrcBufferDataSize;
	SrcBufferOffset = 0;
	CurrentSampleCount = 0;

	// Read Identifier, True Sample Count, Number of channels and Frames to Encode first
	if (FCStringAnsi::Strcmp((char*)InSrcBufferData, OPUS_ID_STRING) != 0)
	{
		return false;
	}
	else
	{
		SrcBufferOffset += FCStringAnsi::Strlen(OPUS_ID_STRING) + 1;
	}
	Read(&SampleRate, sizeof(uint16));
	Read(&TrueSampleCount, sizeof(uint32));
	Read(&NumChannels, sizeof(uint8));
	uint16 SerializedFrames = 0;
	Read(&SerializedFrames, sizeof(uint16));
	AudioDataOffset = SrcBufferOffset;
	SampleStride = SAMPLE_SIZE * NumChannels;

	check(OpusDecoderWrapper == NULL);
	OpusDecoderWrapper = new FOpusDecoderWrapper(SampleRate, NumChannels);
	if (!OpusDecoderWrapper->WasInitialisedSuccessfully())
	{
		delete OpusDecoderWrapper;
		OpusDecoderWrapper = NULL;
		return false;
	}

	MaxFrameSizeSamples = (SampleRate * OPUS_MAX_FRAME_SIZE_MS) / 1000;

	LastDecodedPCM.Empty(MaxFrameSizeSamples * SampleStride);
	LastDecodedPCM.AddUninitialized(MaxFrameSizeSamples * SampleStride);

	if (QualityInfo)
	{
		QualityInfo->SampleRate = SampleRate;
		QualityInfo->NumChannels = NumChannels;
		QualityInfo->SampleDataSize = TrueSampleCount * QualityInfo->NumChannels * SAMPLE_SIZE;
		QualityInfo->Duration = (float)TrueSampleCount / QualityInfo->SampleRate;
	}

	return true;
}

bool FOpusAudioInfo::ReadCompressedData(uint8* Destination, bool bLooping, uint32 BufferSize /*= 0*/)
{
	check(OpusDecoderWrapper);
	check(Destination);

	SCOPE_CYCLE_COUNTER(STAT_OpusDecompressTime);

	// Write out any PCM data that was decoded during the last request
	uint32 RawPCMOffset = WriteFromDecodedPCM(Destination, BufferSize);

	bool bLooped = false;

	if (bStoringEndOfFile && LastPCMByteSize > 0)
	{
		// delayed returning looped because we hadn't read the entire buffer
		bLooped = true;
		bStoringEndOfFile = false;
	}

	while (RawPCMOffset < BufferSize)
	{
		uint16 FrameSize = 0;
		Read(&FrameSize, sizeof(uint16));
		int32 DecodedSamples = DecompressToPCMBuffer(FrameSize);
		if (DecodedSamples < 0)
		{
			LastPCMByteSize = 0;
			ZeroBuffer(Destination + RawPCMOffset, BufferSize - RawPCMOffset);
			return false;
		}
		else
		{
			LastPCMByteSize = IncrementCurrentSampleCount(DecodedSamples) * SampleStride;
			RawPCMOffset += WriteFromDecodedPCM(Destination + RawPCMOffset, BufferSize - RawPCMOffset);

			if (SrcBufferOffset >= SrcBufferDataSize)
			{
				// check whether all decoded PCM was written
				if (LastPCMByteSize == 0)
				{
					bLooped = true;
				}
				else
				{
					bStoringEndOfFile = true;
				}
				if (bLooping)
				{
					SrcBufferOffset = AudioDataOffset;
					CurrentSampleCount = 0;
				}
				else
				{
					RawPCMOffset += ZeroBuffer(Destination + RawPCMOffset, BufferSize - RawPCMOffset);
				}
			}
		}
	}

	return bLooped;
}

void FOpusAudioInfo::ExpandFile(uint8* DstBuffer, struct FSoundQualityInfo* QualityInfo)
{
	check(OpusDecoderWrapper);
	check(DstBuffer);
	check(QualityInfo);
	check(QualityInfo->SampleDataSize <= SrcBufferDataSize);

	// Ensure we're at the start of the audio data
	SrcBufferOffset = AudioDataOffset;

	uint32 RawPCMOffset = 0;

	while (RawPCMOffset < QualityInfo->SampleDataSize)
	{
		uint16 FrameSize = 0;
		Read(&FrameSize, sizeof(uint16));
		int32 DecodedSamples = DecompressToPCMBuffer(FrameSize);

		if (DecodedSamples < 0)
		{
			RawPCMOffset += ZeroBuffer(DstBuffer + RawPCMOffset, QualityInfo->SampleDataSize - RawPCMOffset);
		}
		else
		{
			LastPCMByteSize = IncrementCurrentSampleCount(DecodedSamples) * SampleStride;
			RawPCMOffset += WriteFromDecodedPCM(DstBuffer + RawPCMOffset, QualityInfo->SampleDataSize - RawPCMOffset);
		}
	}
}

bool FOpusAudioInfo::StreamCompressedInfo(USoundWave* Wave, struct FSoundQualityInfo* QualityInfo)
{
	StreamingSoundWave = Wave;

	// Get the first chunk of audio data (should always be loaded)
	CurrentChunkIndex = 0;
	const uint8* FirstChunk = IStreamingManager::Get().GetAudioStreamingManager().GetLoadedChunk(StreamingSoundWave, CurrentChunkIndex);

	if (FirstChunk)
	{
		return ReadCompressedInfo(FirstChunk, Wave->RunningPlatformData->Chunks[0].DataSize, QualityInfo);
	}

	return false;
}

bool FOpusAudioInfo::StreamCompressedData(uint8* Destination, bool bLooping, uint32 BufferSize)
{
	check(OpusDecoderWrapper);
	check(Destination);

	SCOPE_CYCLE_COUNTER(STAT_OpusDecompressTime);

	UE_LOG(LogAudio, Log, TEXT("Streaming compressed data from SoundWave'%s' - Chunk %d, Offset %d"), *StreamingSoundWave->GetName(), CurrentChunkIndex, SrcBufferOffset);

	// Write out any PCM data that was decoded during the last request
	uint32 RawPCMOffset = WriteFromDecodedPCM(Destination, BufferSize);

	// If next chunk wasn't loaded when last one finished reading, try to get it again now
	if (SrcBufferData == NULL)
	{
		SrcBufferData = IStreamingManager::Get().GetAudioStreamingManager().GetLoadedChunk(StreamingSoundWave, CurrentChunkIndex);
		if (SrcBufferData)
		{
			SrcBufferDataSize = StreamingSoundWave->RunningPlatformData->Chunks[CurrentChunkIndex].DataSize;
			SrcBufferOffset = CurrentChunkIndex == 0 ? AudioDataOffset : 0;
		}
		else
		{
			// Still not loaded, zero remainder of current buffer
			UE_LOG(LogAudio, Warning, TEXT("Unable to read from chunk %d of SoundWave'%s'"), CurrentChunkIndex, *StreamingSoundWave->GetName());
			ZeroBuffer(Destination + RawPCMOffset, BufferSize - RawPCMOffset);
			return false;
		}
	}

	bool bLooped = false;

	if (bStoringEndOfFile && LastPCMByteSize > 0)
	{
		// delayed returning looped because we hadn't read the entire buffer
		bLooped = true;
		bStoringEndOfFile = false;
	}

	while (RawPCMOffset < BufferSize)
	{
		uint16 FrameSize = 0;
		Read(&FrameSize, sizeof(uint16));
		int32 DecodedSamples = DecompressToPCMBuffer(FrameSize);

		if (DecodedSamples < 0)
		{
			LastPCMByteSize = 0;
			ZeroBuffer(Destination + RawPCMOffset, BufferSize - RawPCMOffset);
			return false;
		}
		else
		{
			LastPCMByteSize = IncrementCurrentSampleCount(DecodedSamples) * SampleStride;

			RawPCMOffset += WriteFromDecodedPCM(Destination + RawPCMOffset, BufferSize - RawPCMOffset);

			// Have we reached the end of buffer
			if (SrcBufferOffset >= SrcBufferDataSize)
			{
				// Special case for the last chunk of audio
				if (CurrentChunkIndex == StreamingSoundWave->RunningPlatformData->NumChunks - 1)
				{
					// check whether all decoded PCM was written
					if (LastPCMByteSize == 0)
					{
						bLooped = true;
					}
					else
					{
						bStoringEndOfFile = true;
					}
					if (bLooping)
					{
						CurrentChunkIndex = 0;
						SrcBufferOffset = AudioDataOffset;
						CurrentSampleCount = 0;
					}
					else
					{
						RawPCMOffset += ZeroBuffer(Destination + RawPCMOffset, BufferSize - RawPCMOffset);
					}
				}
				else
				{
					CurrentChunkIndex++;
					SrcBufferOffset = 0;
				}

				SrcBufferData = IStreamingManager::Get().GetAudioStreamingManager().GetLoadedChunk(StreamingSoundWave, CurrentChunkIndex);
				if (SrcBufferData)
				{
					UE_LOG(LogAudio, Log, TEXT("Incremented current chunk from SoundWave'%s' - Chunk %d, Offset %d"), *StreamingSoundWave->GetName(), CurrentChunkIndex, SrcBufferOffset);
					SrcBufferDataSize = StreamingSoundWave->RunningPlatformData->Chunks[CurrentChunkIndex].DataSize;
				}
				else
				{
					SrcBufferDataSize = 0;
					RawPCMOffset += ZeroBuffer(Destination + RawPCMOffset, BufferSize - RawPCMOffset);
				}
			}
		}
	}

	return bLooped;
}

int32 FOpusAudioInfo::DecompressToPCMBuffer(uint16 FrameSize)
{
	if (SrcBufferOffset + FrameSize > SrcBufferDataSize)
	{
		// if frame size is too large, something has gone wrong
		return -1;
	}

	const uint8* SrcPtr = SrcBufferData + SrcBufferOffset;
	SrcBufferOffset += FrameSize;
	LastPCMOffset = 0;
	return OpusDecoderWrapper->Decode(SrcPtr, FrameSize, (int16*)LastDecodedPCM.GetData(), MaxFrameSizeSamples);
}

uint32 FOpusAudioInfo::IncrementCurrentSampleCount(uint32 NewSamples)
{
	if (CurrentSampleCount + NewSamples > TrueSampleCount)
	{
		NewSamples = TrueSampleCount - CurrentSampleCount;
		CurrentSampleCount = TrueSampleCount;
	}
	else
	{
		CurrentSampleCount += NewSamples;
	}
	return NewSamples;
}

uint32 FOpusAudioInfo::WriteFromDecodedPCM(uint8* Destination, uint32 BufferSize)
{
	uint32 BytesToCopy = FMath::Min(BufferSize, LastPCMByteSize - LastPCMOffset);
	if (BytesToCopy > 0)
	{
		FMemory::Memcpy(Destination, LastDecodedPCM.GetData() + LastPCMOffset, BytesToCopy);
		LastPCMOffset += BytesToCopy;
		if (LastPCMOffset >= LastPCMByteSize)
		{
			LastPCMOffset = 0;
			LastPCMByteSize = 0;
		}
	}
	return BytesToCopy;
}

uint32	FOpusAudioInfo::ZeroBuffer(uint8* Destination, uint32 BufferSize)
{
	check(Destination);

	if (BufferSize > 0)
	{
		FMemory::Memzero(Destination, BufferSize);
		return BufferSize;
	}
	return 0;
}
