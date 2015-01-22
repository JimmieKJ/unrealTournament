// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ADPCMAudioInfo.h: Unreal audio ADPCM decompression interface object.
=============================================================================*/

#pragma once

#include "AudioDecompress.h"

class FADPCMAudioInfo : public ICompressedAudioInfo
{
public:
	ENGINE_API FADPCMAudioInfo(void);
	ENGINE_API virtual ~FADPCMAudioInfo(void);

	// ICompressedAudioInfo Interface
	ENGINE_API virtual bool ReadCompressedInfo(const uint8* InSrcBufferData, uint32 InSrcBufferDataSize, struct FSoundQualityInfo* QualityInfo);
	ENGINE_API virtual bool ReadCompressedData(uint8* Destination, bool bLooping, uint32 BufferSize);
	ENGINE_API virtual void SeekToTime(const float SeekTime) {};
	ENGINE_API virtual void ExpandFile(uint8* DstBuffer, struct FSoundQualityInfo* QualityInfo);
	ENGINE_API virtual void EnableHalfRate(bool HalfRate) {};
	virtual uint32 GetSourceBufferSize() const { return SrcBufferDataSize; }
	virtual bool UsesVorbisChannelOrdering() const { return false; }
	virtual int GetStreamBufferSize() const;
	// End of ICompressedAudioInfo Interface

	FWaveModInfo WaveInfo;
	const uint8*	SrcBufferData;
	uint32			SrcBufferDataSize;
	uint32			SrcBufferOffset;

	uint32 UncompressedBlockSize;
	uint32 CompressedBlockSize;
	uint32 BlockSize;
	int32 StreamBufferSize;
	uint32 StreamBufferSizeInBlocks;
	uint32 TotalDecodedSize;

private:
	bool DecodeStereoData(uint8* Destination, bool bLooping, uint32 BufferSize);
	bool DecodeMonoData(uint8* Destination, bool bLooping, uint32 BufferSize);
};
