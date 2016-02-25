// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "VoicePackage.h"

/** Stats for voice codec */
DECLARE_CYCLE_STAT_EXTERN(TEXT("VoiceEncode"), STAT_Voice_Encoding, STATGROUP_Net, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("VoiceDecode"), STAT_Voice_Decoding, STATGROUP_Net, );

/**
 * Interface for encoding raw voice for transmission over the wire
 */
class IVoiceEncoder : public TSharedFromThis<IVoiceEncoder>
{

protected:

	IVoiceEncoder() {}

public:

	virtual ~IVoiceEncoder() {}

	/**
	 * Initialize the encoder
	 *
	 * @param SampleRate requested sample rate of the encoding
	 * @param NumChannel number of channels in the raw audio stream
	 * 
	 * @return true if initialization was successful, false otherwise
	 */
	virtual bool Init(int32 SampleRate, int32 NumChannels) = 0;

	/**
	 * Encode a raw audio stream (expects 16bit PCM audio)
	 *
	 * @param RawPCMData array of raw PCM data to encode
	 * @param RawDataSize amount of raw PCM data in the buffer
	 * @param OutCompressedData buffer to contain encoded/compressed audio stream
	 * @param OutCompressedDataSize [in/out] amount of buffer used to encode the audio stream
	 *
	 * @return number of bytes at the end of the stream that wasn't encoded (some interfaces can only encode at a certain frame slice)
	 */
	virtual int32 Encode(const uint8* RawPCMData, uint32 RawDataSize, uint8* OutCompressedData, uint32& OutCompressedDataSize) = 0;

	/**
	 * Cleanup the encoder
	 */
	virtual void Destroy() = 0;
};

/**
 * Interface for decoding voice passed over the wire 
 */
class IVoiceDecoder : public TSharedFromThis<IVoiceDecoder>
{

protected:

	IVoiceDecoder() {}

public:

	virtual ~IVoiceDecoder() {}

	/**
	 * Initialize the decoder
	 *
	 * @param SampleRate requested sample rate of the decoding (may be up sampled depending on the source data)
	 * @param NumChannel number of channels in the output decoded stream (mono streams can encode to stereo)
	 * 
	 * @return true if initialization was successful, false otherwise
	 */
	virtual bool Init(int32 SampleRate, int32 NumChannels) = 0;

	/**
	 * Decode an encoded audio stream (outputs 16bit PCM audio)
	 *
	 * @param CompressedData encoded/compressed audio stream
	 * @param CompressedDataSize amount of data in the buffer
	 * @param OutRawPCMData buffer to contain the decoded raw PCM data
	 * @param OutRawDataSize [in/out] amount of buffer used for the decoded raw PCM data
	 */
	virtual void Decode(const uint8* CompressedData, uint32 CompressedDataSize, uint8* OutRawPCMData, uint32& OutRawDataSize) = 0;

	/**
	 * Cleanup the decoder
	 */
	virtual void Destroy() = 0;
};