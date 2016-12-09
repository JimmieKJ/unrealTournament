// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	OpusAudioInfo.h: Unreal audio opus decompression interface object.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "AudioDecompress.h"

#define OPUS_ID_STRING "UE4OPUS"

struct FOpusDecoderWrapper;
struct FSoundQualityInfo;

/**
* Helper class to parse opus data
*/
class FOpusAudioInfo : public IStreamedCompressedInfo
{
public:
	ENGINE_API FOpusAudioInfo();
	ENGINE_API virtual ~FOpusAudioInfo();

	//~ Begin IStreamedCompressedInfo Interface
	virtual bool ParseHeader(const uint8* InSrcBufferData, uint32 InSrcBufferDataSize, FSoundQualityInfo* QualityInfo) override;
	virtual uint32 GetFrameSize() override;
	virtual uint32 GetMaxFrameSizeSamples() const override;
	virtual bool CreateDecoder() override;
	virtual int32 Decode(const uint8* FrameData, uint16 FrameSize, int16* OutPCMData, int32 SampleSize) override;
	//~ End IStreamedCompressedInfo Interface

protected:
	/** Wrapper around Opus-specific decoding state and APIs */
	FOpusDecoderWrapper*	OpusDecoderWrapper;
};
