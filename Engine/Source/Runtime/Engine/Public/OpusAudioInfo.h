// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	OpusAudioInfo.h: Unreal audio opus decompression interface object.
=============================================================================*/

#pragma once

#include "AudioDecompress.h"

#define OPUS_ID_STRING "UE4OPUS"

struct FOpusDecoderWrapper;

/**
* Helper class to parse opus data
*/
class FOpusAudioInfo : public ICompressedAudioInfo
{
public:
	ENGINE_API FOpusAudioInfo(void);
	ENGINE_API virtual ~FOpusAudioInfo(void);

	// ICompressedAudioInfo Interface
	ENGINE_API virtual bool ReadCompressedInfo(const uint8* InSrcBufferData, uint32 InSrcBufferDataSize, struct FSoundQualityInfo* QualityInfo) override;
	ENGINE_API virtual bool ReadCompressedData(uint8* Destination, bool bLooping, uint32 BufferSize) override;
	ENGINE_API virtual void SeekToTime(const float SeekTime) override {};
	ENGINE_API virtual void ExpandFile(uint8* DstBuffer, struct FSoundQualityInfo* QualityInfo) override;
	ENGINE_API virtual void EnableHalfRate(bool HalfRate) override {};
	virtual uint32 GetSourceBufferSize() const override { return SrcBufferDataSize;}
	virtual bool UsesVorbisChannelOrdering() const override { return false; }
	virtual int GetStreamBufferSize() const override { return  MONO_PCM_BUFFER_SIZE; }

	virtual bool SupportsStreaming() const override {return true;}
	virtual bool StreamCompressedInfo(USoundWave* Wave, struct FSoundQualityInfo* QualityInfo) override;
	virtual bool StreamCompressedData(uint8* Destination, bool bLooping, uint32 BufferSize) override;
	virtual int32 GetCurrentChunkIndex() const override {return CurrentChunkIndex;}
	virtual int32 GetCurrentChunkOffset() const override {return SrcBufferOffset;}
	// End of ICompressedAudioInfo Interface

protected:
	/** Emulate read from memory functionality */
	size_t			Read(void *ptr, uint32 size);

	/**
	 * Decompresses a frame of Opus data to PCM buffer
	 *
	 * @param FrameSize Size of the frame in bytes
	 * @return The amount of samples that were decompressed (< 0 indicates error)
	 */
	int32 DecompressToPCMBuffer(uint16 FrameSize);

	/**
	 * Adds to the count of samples that have currently been decoded
	 *
	 * @param NewSamples	How many samples have been decoded
	 * @return How many samples were actually part of the true sample count
	 */
	uint32 IncrementCurrentSampleCount(uint32 NewSamples);

	/**
	 * Writes data from decoded PCM buffer, taking into account whether some PCM has been written before
	 *
	 * @param Destination	Where to place the decoded sound
	 * @param BufferSize	Size of the destination buffer in bytes
	 * @return				How many bytes were written
	 */
	uint32	WriteFromDecodedPCM(uint8* Destination, uint32 BufferSize);

	/**
	 * Zeroes the contents of a buffer
	 *
	 * @param Destination	Buffer to zero
	 * @param BufferSize	Size of the destination buffer in bytes
	 * @return				How many bytes were zeroed
	 */
	uint32	ZeroBuffer(uint8* Destination, uint32 BufferSize);

	FOpusDecoderWrapper*	OpusDecoderWrapper;
	const uint8*	SrcBufferData;
	uint32			SrcBufferDataSize;
	uint32			SrcBufferOffset;
	uint32			AudioDataOffset;

	uint16			SampleRate;
	uint32			TrueSampleCount;
	uint32			CurrentSampleCount;
	uint8			NumChannels;
	uint32			MaxFrameSizeSamples;
	uint32			SampleStride;

	TArray<uint8>	LastDecodedPCM;
	uint32			LastPCMByteSize;
	uint32			LastPCMOffset;
	bool			bStoringEndOfFile;

	// Streaming specific
	USoundWave*		StreamingSoundWave;
	int32			CurrentChunkIndex;
};
