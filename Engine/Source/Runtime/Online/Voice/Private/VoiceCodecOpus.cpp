// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "VoicePrivatePCH.h"
#include "VoiceCodecOpus.h"

#if PLATFORM_SUPPORTS_VOICE_CAPTURE

#include "opus.h"

/** Turn on extra logging for codec */
#define DEBUG_OPUS 0
/** Turn on entropy data in packets */
#define ADD_ENTROPY_TO_PACKET 0
/** Use UE4 memory allocation or Opus */
#define USE_UE4_MEM_ALLOC 1
/** Maximum number of frames in a single Opus packet */
#define MAX_OPUS_FRAMES_PER_PACKET 48
/** Number of max frames for buffer calculation purposes */
#define MAX_OPUS_FRAMES 6

/**
 * Number of samples per channel of available space in PCM during decompression.  
 * If this is less than the maximum packet duration (120ms; 5760 for 48kHz), opus will not be capable of decoding some packets.
 */
#define MAX_OPUS_FRAME_SIZE MAX_OPUS_FRAMES * 320
/** Hypothetical maximum for buffer validation */
#define MAX_OPUS_UNCOMPRESSED_BUFFER_SIZE 48 * 1024
/** 20ms frame sizes are a good choice for most applications (1000ms / 20ms = 50) */
#define NUM_OPUS_FRAMES_PER_SEC 50

/**
 * Output debug information regarding the state of the Opus encoder
 */
void DebugEncoderInfo(OpusEncoder* Encoder)
{
	int32 ErrCode = 0;

	int32 BitRate = 0;
	ErrCode = opus_encoder_ctl(Encoder, OPUS_GET_BITRATE(&BitRate));

	int32 Vbr = 0;
	ErrCode = opus_encoder_ctl(Encoder, OPUS_GET_VBR(&Vbr));

	int32 SampleRate = 0;
	ErrCode = opus_encoder_ctl(Encoder, OPUS_GET_SAMPLE_RATE(&SampleRate));

	int32 Application = 0;
	ErrCode = opus_encoder_ctl(Encoder, OPUS_GET_APPLICATION(&Application));

	int32 Complexity = 0;
	ErrCode = opus_encoder_ctl(Encoder, OPUS_GET_COMPLEXITY(&Complexity));

	UE_LOG(LogVoiceEncode, Display, TEXT("Opus Encoder Details:\n Application: %d\n Bitrate: %d\n SampleRate: %d\n VBR: %d\n Complexity: %d"),
		Application, BitRate, SampleRate, Vbr, Complexity);
}

/**
 * Output debug information regarding the state of a single Opus packet
 */
void DebugFrameInfoInternal(const uint8* PacketData, uint32 PacketLength, uint32 SampleRate, bool bEncode)
{
	int32 NumFrames = opus_packet_get_nb_frames(PacketData, PacketLength);
	if (NumFrames == OPUS_BAD_ARG || NumFrames == OPUS_INVALID_PACKET)
	{
		UE_LOG(LogVoice, Warning, TEXT("opus_packet_get_nb_frames: Invalid voice packet data!"));
	}

	int32 NumSamples = opus_packet_get_nb_samples(PacketData, PacketLength, SampleRate);
	if (NumSamples == OPUS_BAD_ARG || NumSamples == OPUS_INVALID_PACKET)
	{
		UE_LOG(LogVoice, Warning, TEXT("opus_packet_get_nb_samples: Invalid voice packet data!"));
	}

	int32 NumSamplesPerFrame = opus_packet_get_samples_per_frame(PacketData, SampleRate);
	int32 Bandwidth = opus_packet_get_bandwidth(PacketData);

	const TCHAR* BandwidthStr = NULL;
	switch (Bandwidth)
	{
	case OPUS_BANDWIDTH_NARROWBAND: // Narrowband (4kHz bandpass)
		BandwidthStr = TEXT("NB");
		break;
	case OPUS_BANDWIDTH_MEDIUMBAND: // Mediumband (6kHz bandpass)
		BandwidthStr = TEXT("MB");
		break;
	case OPUS_BANDWIDTH_WIDEBAND: // Wideband (8kHz bandpass)
		BandwidthStr = TEXT("WB");
		break;
	case OPUS_BANDWIDTH_SUPERWIDEBAND: // Superwideband (12kHz bandpass)
		BandwidthStr = TEXT("SWB");
		break;
	case OPUS_BANDWIDTH_FULLBAND: // Fullband (20kHz bandpass)
		BandwidthStr = TEXT("FB");
		break;
	case OPUS_INVALID_PACKET: 
	default:
		BandwidthStr = TEXT("Invalid");
		break;
	}

	/*
	 *	0
	 *	0 1 2 3 4 5 6 7
	 *	+-+-+-+-+-+-+-+-+
	 *	| config  |s| c |
	 *	+-+-+-+-+-+-+-+-+
	 */
	uint8 TOC;
	// (max 48 x 2.5ms frames in a packet = 120ms)
	const uint8* frames[48];
	int16 size[48];
	int32 payload_offset = 0;
	int32 NumFramesParsed = opus_packet_parse(PacketData, PacketLength, &TOC, frames, size, &payload_offset);

	// Frame Encoding see http://tools.ietf.org/html/rfc6716#section-3.1
	int32 TOCEncoding = (TOC & 0xf8) >> 3;

	// Number of channels
	bool TOCStereo = (TOC & 0x4) != 0 ? true : false;

	// Number of frames and their configuration
	// 0: 1 frame in the packet
	// 1: 2 frames in the packet, each with equal compressed size
    // 2: 2 frames in the packet, with different compressed sizes
	// 3: an arbitrary number of frames in the packet
	int32 TOCMode = TOC & 0x3;

	if (bEncode)
	{
		UE_LOG(LogVoiceEncode, Verbose, TEXT("PacketLength: %d NumFrames: %d NumSamples: %d Bandwidth: %s Encoding: %d Stereo: %d FrameDesc: %d"),
			PacketLength, NumFrames, NumSamples, BandwidthStr, TOCEncoding, TOCStereo, TOCMode);
	}
	else
	{
		UE_LOG(LogVoiceDecode, Verbose, TEXT("PacketLength: %d NumFrames: %d NumSamples: %d Bandwidth: %s Encoding: %d Stereo: %d FrameDesc: %d"),
			PacketLength, NumFrames, NumSamples, BandwidthStr, TOCEncoding, TOCStereo, TOCMode);
	}
}

inline void DebugFrameEncodeInfo(const uint8* PacketData, uint32 PacketLength, uint32 SampleRate)
{
	DebugFrameInfoInternal(PacketData, PacketLength, SampleRate, true);
}

inline void DebugFrameDecodeInfo(const uint8* PacketData, uint32 PacketLength, uint32 SampleRate)
{
	DebugFrameInfoInternal(PacketData, PacketLength, SampleRate, false);
}

FVoiceEncoderOpus::FVoiceEncoderOpus() :
	SampleRate(0),
	NumChannels(0),
	FrameSize(0),
	Encoder(NULL),
	LastEntropyIdx(0),
	Generation(0)
{
	FMemory::Memzero(Entropy, NUM_ENTROPY_VALUES * sizeof(uint32));
}

FVoiceEncoderOpus::~FVoiceEncoderOpus()
{
	Destroy();
}

bool FVoiceEncoderOpus::Init(int32 InSampleRate, int32 InNumChannels)
{
	UE_LOG(LogVoiceEncode, Display, TEXT("EncoderVersion: %s"), ANSI_TO_TCHAR(opus_get_version_string()));

	SampleRate = InSampleRate;
	NumChannels = InNumChannels;

	// 20ms frame sizes are a good choice for most applications (1000ms / 20ms = 50)
	FrameSize = SampleRate / NUM_OPUS_FRAMES_PER_SEC;
	//MaxFrameSize = FrameSize * MAX_OPUS_FRAMES;

	int32 EncError = 0;

#if USE_UE4_MEM_ALLOC
	int32 EncSize = opus_encoder_get_size(NumChannels);
	Encoder = (OpusEncoder*)FMemory::Malloc(EncSize);
	EncError = opus_encoder_init(Encoder, SampleRate, NumChannels, OPUS_APPLICATION_VOIP);
#else
	Encoder = opus_encoder_create(SampleRate, NumChannels, OPUS_APPLICATION_VOIP, &EncError);
#endif

	if (EncError != OPUS_OK)
	{
		UE_LOG(LogVoiceEncode, Warning, TEXT("Failed to init Opus Encoder: %s"), ANSI_TO_TCHAR(opus_strerror(EncError)));
		Destroy();
	}

	// Turn on variable bit rate encoding
	int32 UseVbr = 1;
	opus_encoder_ctl(Encoder, OPUS_SET_VBR(UseVbr));

	// Turn off constrained VBR
	int32 UseCVbr = 0;
	opus_encoder_ctl(Encoder, OPUS_SET_VBR_CONSTRAINT(UseCVbr));

	// Complexity (1-10)
	int32 Complexity = 1;
	opus_encoder_ctl(Encoder, OPUS_SET_COMPLEXITY(Complexity));

	// Forward error correction
	int32 InbandFEC = 0;
	opus_encoder_ctl(Encoder, OPUS_SET_INBAND_FEC(InbandFEC));

#if DEBUG_OPUS
	DebugEncoderInfo(Encoder);
#endif // DEBUG_OPUS
	return EncError == OPUS_OK;
}

int32 FVoiceEncoderOpus::Encode(const uint8* RawPCMData, uint32 RawDataSize, uint8* OutCompressedData, uint32& OutCompressedDataSize)
{
	check(Encoder);
	SCOPE_CYCLE_COUNTER(STAT_Voice_Encoding);

	int32 HeaderSize = 0;
	int32 BytesPerFrame = FrameSize * NumChannels * sizeof(opus_int16);
	int32 MaxFramesEncoded = MAX_OPUS_UNCOMPRESSED_BUFFER_SIZE / BytesPerFrame;

	// total bytes / bytes per frame
	int32 NumFramesToEncode = FMath::Min((int32)RawDataSize / BytesPerFrame, MaxFramesEncoded); 
	int32 DataRemainder = RawDataSize % BytesPerFrame;
	int32 RawDataStride = BytesPerFrame;

	// Store the number of frames to be encoded
	check(NumFramesToEncode < MAX_uint8);
	OutCompressedData[0] = NumFramesToEncode;
	OutCompressedData[1] = Generation;
	HeaderSize += 2 * sizeof(uint8);
	
	// Store the offset to each encoded frame
	uint16* CompressedOffsets = (uint16*)(OutCompressedData + HeaderSize);
	uint32 LengthOfCompressedOffsets = NumFramesToEncode * sizeof(uint16);
	HeaderSize += LengthOfCompressedOffsets;

	// Store the entropy to each encoded frame
	uint32 LengthOfEntropyOffsets = 0;
#if ADD_ENTROPY_TO_PACKET
	uint32* EntropyOffsets = (uint32*)(OutCompressedData + HeaderSize);
	LengthOfEntropyOffsets = NumFramesToEncode * sizeof(uint32);
#endif
	HeaderSize += LengthOfEntropyOffsets;

	// Space available after overhead
	int32 AvailableBufferSize = OutCompressedDataSize - HeaderSize;
	
	// Start of the actual compressed data
	uint8* CompressedDataStart = OutCompressedData + HeaderSize;
	int32 CompressedBufferOffset = 0;
	for (int32 i = 0; i < NumFramesToEncode; i++)
	{
		int32 CompressedLength = 0;
			CompressedLength = opus_encode(Encoder, (const opus_int16*)(RawPCMData + (i * RawDataStride)), FrameSize, CompressedDataStart + CompressedBufferOffset, AvailableBufferSize);

		if (CompressedLength < 0)
		{
			const char* ErrorStr = opus_strerror(CompressedLength);
			UE_LOG(LogVoiceEncode, Warning, TEXT("Failed to encode: [%d] %s"), CompressedLength, ANSI_TO_TCHAR(ErrorStr));

			OutCompressedData[0] = 0;
			OutCompressedDataSize = 0;
			return 0;
		}
		else if (CompressedLength != 1)
		{
			opus_encoder_ctl(Encoder, OPUS_GET_FINAL_RANGE(&Entropy[LastEntropyIdx]));

#if ADD_ENTROPY_TO_PACKET
			UE_LOG(LogVoiceEncode, VeryVerbose, TEXT("Entropy[%d]=%d"), i, Entropy[LastEntropyIdx]);
			EntropyOffsets[i] = Entropy[LastEntropyIdx];
#endif
			LastEntropyIdx = (LastEntropyIdx + 1) % NUM_ENTROPY_VALUES;

#if DEBUG_OPUS
			DebugFrameEncodeInfo(CompressedDataStart + CompressedBufferOffset, CompressedLength, SampleRate);
#endif // DEBUG_OPUS

			AvailableBufferSize -= CompressedLength;
			CompressedBufferOffset += CompressedLength;

			check(CompressedBufferOffset < MAX_uint16); 
			CompressedOffsets[i] = (uint16)CompressedBufferOffset;
		}
		else
		{
			UE_LOG(LogVoiceEncode, Warning, TEXT("Nothing to encode!"));
		}
	}

	// End of buffer
	OutCompressedDataSize = HeaderSize + CompressedBufferOffset;

	UE_LOG(LogVoiceEncode, VeryVerbose, TEXT("OpusEncode[%d]: RawSize: %d CompressedSize: %d NumFramesEncoded: %d Remains: %d"), Generation, RawDataSize, OutCompressedDataSize, NumFramesToEncode, DataRemainder);

	Generation = (Generation + 1) % MAX_uint8;
	return DataRemainder;
}

void FVoiceEncoderOpus::Destroy()
{
#if USE_UE4_MEM_ALLOC
	FMemory::Free(Encoder);
#else
	opus_encoder_destroy(Encoder);
#endif
	Encoder = NULL;
}

FVoiceDecoderOpus::FVoiceDecoderOpus() :
	SampleRate(0),
	NumChannels(0),
	FrameSize(0),
	Decoder(NULL),
	LastEntropyIdx(0),
	LastGeneration(0)
{
	FMemory::Memzero(Entropy, NUM_ENTROPY_VALUES * sizeof(uint32));
}

FVoiceDecoderOpus::~FVoiceDecoderOpus()
{
	Destroy();
}

bool FVoiceDecoderOpus::Init(int32 InSampleRate, int32 InNumChannels)
{
	UE_LOG(LogVoiceDecode, Display, TEXT("DecoderVersion: %s"), ANSI_TO_TCHAR(opus_get_version_string()));

	SampleRate = InSampleRate;
	NumChannels = InNumChannels;

	FrameSize = SampleRate / 50;

	int32 DecSize = 0;
	int32 DecError = 0;

#if USE_UE4_MEM_ALLOC
	DecSize = opus_decoder_get_size(NumChannels);
	Decoder = (OpusDecoder*)FMemory::Malloc(DecSize);
	DecError = opus_decoder_init(Decoder, SampleRate, NumChannels);
#else
	Decoder = opus_decoder_create(SampleRate, NumChannels, &DecError);
#endif
	if (DecError != OPUS_OK)
	{
		UE_LOG(LogVoiceDecode, Warning, TEXT("Failed to init Opus Decoder: %s"), ANSI_TO_TCHAR(opus_strerror(DecError)));
		Destroy();
	}

	return DecError == OPUS_OK;
}

void FVoiceDecoderOpus::Destroy()
{
#if USE_UE4_MEM_ALLOC
	FMemory::Free(Decoder);
#else
	opus_decoder_destroy(Decoder);
#endif 
	Decoder = NULL;
}

void FVoiceDecoderOpus::Decode(const uint8* InCompressedData, uint32 CompressedDataSize, uint8* OutRawPCMData, uint32& OutRawDataSize)
{
	SCOPE_CYCLE_COUNTER(STAT_Voice_Decoding);

	int32 HeaderSize = 0;
	int32 BytesPerFrame = FrameSize * NumChannels * sizeof(opus_int16);
	int32 MaxFramesEncoded = MAX_OPUS_UNCOMPRESSED_BUFFER_SIZE / BytesPerFrame;

	int32 NumFramesToDecode = InCompressedData[0];
	int32 PacketGeneration = InCompressedData[1];
	HeaderSize += 2 * sizeof(uint8);

	if (PacketGeneration != LastGeneration + 1)
	{
		UE_LOG(LogVoiceDecode, Verbose, TEXT("Packet generation skipped from %d to %d"), LastGeneration, PacketGeneration);
	}

	if (NumFramesToDecode > 0 && NumFramesToDecode <= MaxFramesEncoded)
	{
		// Start of compressed data offsets
		const uint16* CompressedOffsets = (const uint16*)(InCompressedData + HeaderSize);
		uint32 LengthOfCompressedOffsets = NumFramesToDecode * sizeof(uint16);
		HeaderSize += LengthOfCompressedOffsets;

		uint32 LengthOfEntropyOffsets = 0;
#if ADD_ENTROPY_TO_PACKET
		// Start of the entropy to each encoded frame
		const uint32* EntropyOffsets = (uint32*)(InCompressedData + HeaderSize);
		LengthOfEntropyOffsets = NumFramesToDecode * sizeof(uint32);
		HeaderSize += LengthOfEntropyOffsets;
#endif

		// Start of compressed data
		const uint8* CompressedDataStart = (InCompressedData + HeaderSize);
		
		int32 CompressedBufferOffset = 0;
		int32 DecompressedBufferOffset = 0;
		uint16 LastCompressedOffset = 0;

		for (int32 i=0; i < NumFramesToDecode; i++)
		{
			int32 UncompressedBufferAvail = OutRawDataSize - DecompressedBufferOffset;

			if (UncompressedBufferAvail >= (MAX_OPUS_FRAMES * BytesPerFrame))
			{
				int32 CompressedBufferSize = CompressedOffsets[i] - LastCompressedOffset;

				check(Decoder);
				int32 NumDecompressedSamples = opus_decode(Decoder, 
					CompressedDataStart + CompressedBufferOffset, CompressedBufferSize, 
					(opus_int16*)(OutRawPCMData + DecompressedBufferOffset), MAX_OPUS_FRAME_SIZE, 0);

#if DEBUG_OPUS
				DebugFrameDecodeInfo(CompressedDataStart + CompressedBufferOffset, CompressedBufferSize, SampleRate);
#endif // DEBUG_OPUS

				if (NumDecompressedSamples < 0)
				{
					const char* ErrorStr = opus_strerror(NumDecompressedSamples);
					UE_LOG(LogVoiceDecode, Warning, TEXT("Failed to decode: [%d] %s"), NumDecompressedSamples, ANSI_TO_TCHAR(ErrorStr));
					break;
				}
				else
				{
					if (NumDecompressedSamples != FrameSize)
					{
						UE_LOG(LogVoiceDecode, Warning, TEXT("Unexpected decode result NumSamplesDecoded %d != FrameSize %d"), NumDecompressedSamples, FrameSize);
					}

					opus_decoder_ctl(Decoder, OPUS_GET_FINAL_RANGE(&Entropy[LastEntropyIdx]));

#if ADD_ENTROPY_TO_PACKET
					if (Entropy[LastEntropyIdx] != EntropyOffsets[i])
					{
						UE_LOG(LogVoiceDecode, Verbose, TEXT("Decoder Entropy[%d/%d] = %d expected %d"), i, NumFramesToDecode-1, Entropy[LastEntropyIdx], EntropyOffsets[i]);
					}
#endif

					LastEntropyIdx = (LastEntropyIdx + 1) % NUM_ENTROPY_VALUES;

					DecompressedBufferOffset += NumDecompressedSamples * sizeof(opus_int16);
					CompressedBufferOffset += CompressedBufferSize;
					LastCompressedOffset = CompressedOffsets[i];
				}
			}
			else
			{
				UE_LOG(LogVoiceDecode, Warning, TEXT("Decompression buffer too small to decode voice"));
				break;
			}
		}

		OutRawDataSize = DecompressedBufferOffset;
	}
	else
	{
		UE_LOG(LogVoiceDecode, Warning, TEXT("Failed to decode: buffer corrupted"));
		OutRawDataSize = 0;
	}

	UE_LOG(LogVoiceDecode, VeryVerbose, TEXT("OpusDecode[%d]: RawSize: %d CompressedSize: %d NumFramesEncoded: %d "), PacketGeneration, OutRawDataSize, CompressedDataSize, NumFramesToDecode);
	LastGeneration = PacketGeneration;
}

#endif // PLATFORM_SUPPORTS_VOICE_CAPTURE