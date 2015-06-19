// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "Core.h"
#include "ModuleInterface.h"
#include "ModuleManager.h"
#include "TargetPlatform.h"

DEFINE_LOG_CATEGORY_STATIC(LogAudioFormatADPCM, Log, All);

#define UE_MAKEFOURCC(ch0, ch1, ch2, ch3)\
	((uint32)(uint8)(ch0) | ((uint32)(uint8)(ch1) << 8) |\
	((uint32)(uint8)(ch2) << 16) | ((uint32)(uint8)(ch3) << 24 ))

#define NUM_ADAPTATION_TABLE 16
#define NUM_ADAPTATION_COEFF 7

#define WAVE_FORMAT_LPCM  1
#define WAVE_FORMAT_ADPCM 2

static FName NAME_ADPCM(TEXT("ADPCM"));

namespace
{
	struct RiffDataChunk
	{
		uint32 ID;
		uint32 DataSize;
		uint8* Data;
	};

#if PLATFORM_SUPPORTS_PRAGMA_PACK
#pragma pack(push, 2)
#endif
	struct WaveFormatHeader
	{
		uint16 wFormatTag;      // Format type: 1 = PCM, 2 = ADPCM
		uint16 nChannels;       // Number of channels (i.e. mono, stereo...).
		uint32 nSamplesPerSec;  // Sample rate. 44100 or 22050 or 11025  Hz.
		uint32 nAvgBytesPerSec; // For buffer estimation  = sample rate * BlockAlign.
		uint16 nBlockAlign;     // Block size of data = Channels times BYTES per sample.
		uint16 wBitsPerSample;  // Number of bits per sample of mono data.
		uint16 cbSize;          // The count in bytes of the size of extra information (after cbSize).
	};
#if PLATFORM_SUPPORTS_PRAGMA_PACK
#pragma pack(pop)
#endif

	template <uint32 N>
	void GenerateWaveFile(RiffDataChunk(& RiffDataChunks)[N], TArray<uint8>& CompressedDataStore)
	{
		// 'WAVE'
		uint32 RiffDataSize = sizeof(uint32);

		// Determine the size of the wave file to be generated
		for (uint32 Scan = 0; Scan < N; ++Scan)
		{
			const RiffDataChunk& Chunk = RiffDataChunks[Scan];
			RiffDataSize += (sizeof(Chunk.ID) + sizeof(Chunk.DataSize) + Chunk.DataSize);
		}

		// Allocate space for the output data + 'RIFF' + ChunkSize
		uint32 OutputDataSize = RiffDataSize + sizeof(uint32) + sizeof(uint32);
		
		CompressedDataStore.Empty(OutputDataSize);
		FMemoryWriter CompressedData(CompressedDataStore);
		
		uint32 rID = UE_MAKEFOURCC('R','I','F','F');
		CompressedData.Serialize(&rID, sizeof(rID));
		CompressedData.Serialize(&RiffDataSize, sizeof(RiffDataSize));

		rID = UE_MAKEFOURCC('W','A','V','E');
		CompressedData.Serialize(&rID, sizeof(rID));

		// Write each sub-chunk to the output data
		for (uint32 Scan = 0; Scan < N; ++Scan)
		{
			RiffDataChunk& Chunk = RiffDataChunks[Scan];
			
			CompressedData.Serialize(&Chunk.ID, sizeof(Chunk.ID));
			CompressedData.Serialize(&Chunk.DataSize, sizeof(Chunk.DataSize));
			CompressedData.Serialize(Chunk.Data, Chunk.DataSize);
		}
	}

	template <typename T, uint32 B>
	inline T SignExtend(const T ValueToExtend)
	{
		struct { T ExtendedValue:B; } SignExtender;
		return SignExtender.ExtendedValue = ValueToExtend;
	}

	template <typename T>
	inline T ReadFromByteStream(const uint8* ByteStream, int32& ReadIndex, bool bLittleEndian = true)
	{
		T ValueRaw = 0;

		if (bLittleEndian)
		{
#if PLATFORM_LITTLE_ENDIAN
			for (int32 ByteIndex = 0; ByteIndex < sizeof(T); ++ByteIndex)
#else
			for (int32 ByteIndex = sizeof(T) - 1; ByteIndex >= 0; --ByteIndex)
#endif // PLATFORM_LITTLE_ENDIAN
			{
				ValueRaw |= ByteStream[ReadIndex++] << 8 * ByteIndex;
			}
		}
		else
		{
#if PLATFORM_LITTLE_ENDIAN
			for (int32 ByteIndex = sizeof(T) - 1; ByteIndex >= 0; --ByteIndex)
#else
			for (int32 ByteIndex = 0; ByteIndex < sizeof(T); ++ByteIndex)
#endif // PLATFORM_LITTLE_ENDIAN
			{
				ValueRaw |= ByteStream[ReadIndex++] << 8 * ByteIndex;
			}
		}

		return ValueRaw;
	}

	template <typename T>
	inline void WriteToByteStream(T Value, uint8* ByteStream, int32& WriteIndex, bool bLittleEndian = true)
	{
		if (bLittleEndian)
		{
#if PLATFORM_LITTLE_ENDIAN
			for (int32 ByteIndex = 0; ByteIndex < sizeof(T); ++ByteIndex)
#else
			for (int32 ByteIndex = sizeof(T) - 1; ByteIndex >= 0; --ByteIndex)
#endif // PLATFORM_LITTLE_ENDIAN
			{
				ByteStream[WriteIndex++] = (Value >> (8 * ByteIndex)) & 0xFF;
			}
		}
		else
		{
#if PLATFORM_LITTLE_ENDIAN
			for (int32 ByteIndex = sizeof(T) - 1; ByteIndex >= 0; --ByteIndex)
#else
			for (int32 ByteIndex = 0; ByteIndex < sizeof(T); ++ByteIndex)
#endif // PLATFORM_LITTLE_ENDIAN
			{
				ByteStream[WriteIndex++] = (Value >> (8 * ByteIndex)) & 0xFF;
			}
		}
	}

	template <typename T>
	inline T ReadFromArray(const T* ElementArray, int32& ReadIndex, int32 NumElements, int32 IndexStride = 1)
	{
		T OutputValue = 0;

		if (ReadIndex >= 0 && ReadIndex < NumElements)
		{
			OutputValue = ElementArray[ReadIndex];
			ReadIndex += IndexStride;
		}

		return OutputValue;
	}
} // end namespace

namespace LPCM
{
	void Encode(const TArray<uint8>& InputPCMData, TArray<uint8>& CompressedDataStore, const FSoundQualityInfo& QualityInfo)
	{
		WaveFormatHeader Format;
		Format.nChannels = static_cast<uint16>(QualityInfo.NumChannels);
		Format.nSamplesPerSec = QualityInfo.SampleRate;
		Format.nBlockAlign = static_cast<uint16>(Format.nChannels * sizeof(int16));
		Format.nAvgBytesPerSec = Format.nBlockAlign * QualityInfo.SampleRate;
		Format.wBitsPerSample = 16;
		Format.wFormatTag = WAVE_FORMAT_LPCM;

		RiffDataChunk RiffDataChunks[2];
		RiffDataChunks[0].ID = UE_MAKEFOURCC('f','m','t',' ');
		RiffDataChunks[0].DataSize = sizeof(Format);
		RiffDataChunks[0].Data = reinterpret_cast<uint8*>(&Format);

		RiffDataChunks[1].ID = UE_MAKEFOURCC('d','a','t','a');
		RiffDataChunks[1].DataSize = InputPCMData.Num();
		RiffDataChunks[1].Data = const_cast<uint8*>(InputPCMData.GetData());

		GenerateWaveFile(RiffDataChunks, CompressedDataStore);
	}
} // end namespace LPCM

namespace ADPCM
{
	template <typename T>
	static void GetAdaptationTable(T(& OutAdaptationTable)[NUM_ADAPTATION_TABLE])
	{
		// Magic values as specified by standard
		static T AdaptationTable[] =
		{
			230, 230, 230, 230, 307, 409, 512, 614,
			768, 614, 512, 409, 307, 230, 230, 230
		};

		FMemory::Memcpy(&OutAdaptationTable, AdaptationTable, sizeof(AdaptationTable));
	}

	template <typename T>
	static void GetAdaptationCoefficients(T(& OutAdaptationCoefficient1)[NUM_ADAPTATION_COEFF], T(& OutAdaptationCoefficient2)[NUM_ADAPTATION_COEFF])
	{
		// Magic values as specified by standard
		static T AdaptationCoefficient1[] =
		{
			256, 512, 0, 192, 240, 460, 392
		};
		static T AdaptationCoefficient2[] =
		{
			0, -256, 0,  64, 0, -208, -232
		};

		FMemory::Memcpy(&OutAdaptationCoefficient1, AdaptationCoefficient1, sizeof(AdaptationCoefficient1));
		FMemory::Memcpy(&OutAdaptationCoefficient2, AdaptationCoefficient2, sizeof(AdaptationCoefficient2));
	}

	struct FAdaptationContext
	{
	public:
		// Adaptation constants
		int32 AdaptationTable[NUM_ADAPTATION_TABLE];
		int32 AdaptationCoefficient1[NUM_ADAPTATION_COEFF];
		int32 AdaptationCoefficient2[NUM_ADAPTATION_COEFF];

		int32 AdaptationDelta;
		int32 Coefficient1;
		int32 Coefficient2;
		int32 Sample1;
		int32 Sample2;

		FAdaptationContext() :
			AdaptationDelta(0),
			Coefficient1(0),
			Coefficient2(0),
			Sample1(0),
			Sample2(0)
		{
			GetAdaptationTable(AdaptationTable);
			GetAdaptationCoefficients(AdaptationCoefficient1, AdaptationCoefficient2);
		}
	};

#if PLATFORM_SUPPORTS_PRAGMA_PACK
#pragma pack(push, 2)
#endif
	struct ADPCMFormatHeader
	{
		WaveFormatHeader BaseFormat;
		uint16           wSamplesPerBlock;
		uint16           wNumCoef;
		int16            aCoef[2 * NUM_ADAPTATION_COEFF];

		ADPCMFormatHeader()
		{
			int16 AdaptationCoefficient1[NUM_ADAPTATION_COEFF];
			int16 AdaptationCoefficient2[NUM_ADAPTATION_COEFF];
			GetAdaptationCoefficients(AdaptationCoefficient1, AdaptationCoefficient2);

			// Interlace the coefficients as pairs
			for (int32 Coeff = 0, WriteIndex = 0; Coeff < NUM_ADAPTATION_COEFF; Coeff++)
			{
				aCoef[WriteIndex++] = AdaptationCoefficient1[Coeff];
				aCoef[WriteIndex++] = AdaptationCoefficient2[Coeff];
			}
		}
	};
#if PLATFORM_SUPPORTS_PRAGMA_PACK
#pragma pack(pop)
#endif

	/**
	* Encodes a 16-bit PCM sample to a 4-bit ADPCM sample.
	**/
	uint8 EncodeNibble(FAdaptationContext& Context, int16 NextSample)
	{
		int32 PredictedSample = (Context.Sample1 * Context.Coefficient1 + Context.Sample2 * Context.Coefficient2) / 256;
		int32 ErrorDelta = (NextSample - PredictedSample) / Context.AdaptationDelta;
		ErrorDelta = FMath::Clamp(ErrorDelta, -8, 7);

		// Predictor must be clamped within the 16-bit range
		PredictedSample += (Context.AdaptationDelta * ErrorDelta);
		PredictedSample = FMath::Clamp(PredictedSample, -32768, 32767);

		int8 SmallDelta = static_cast<int8>(ErrorDelta);
		uint8 EncodedNibble = reinterpret_cast<uint8&>(SmallDelta) & 0x0F;

		// Shuffle samples for the next iteration
		Context.Sample2 = Context.Sample1;
		Context.Sample1 = static_cast<int16>(PredictedSample);
		Context.AdaptationDelta = (Context.AdaptationDelta * Context.AdaptationTable[EncodedNibble]) / 256;
		Context.AdaptationDelta = FMath::Max(Context.AdaptationDelta, 16);

		return EncodedNibble;
	}

	int32 EncodeBlock(const int16* InputPCMSamples, int32 SampleStride, int32 NumSamples, int32 BlockSize, uint8* EncodedADPCMData)
	{
		FAdaptationContext Context;
		int32 ReadIndex = 0;
		int32 WriteIndex = 0;

		/* TODO::JTM - Dec 10, 2012 05:30PM - Calculate the optimal starting coefficient */
		uint8 CoefficientIndex = 0;
		Context.AdaptationDelta = Context.AdaptationTable[0];
		Context.Sample1 = ReadFromArray<int16>(InputPCMSamples, ReadIndex, NumSamples, SampleStride);
		Context.Sample2 = ReadFromArray<int16>(InputPCMSamples, ReadIndex, NumSamples, SampleStride);
		Context.Coefficient1 = Context.AdaptationCoefficient1[CoefficientIndex];
		Context.Coefficient2 = Context.AdaptationCoefficient2[CoefficientIndex];

		// Populate the block preamble
		//   [0]: Block Predictor
		// [1-2]: Initial Adaptation Delta
		// [3-4]: First Sample
		// [5-6]: Second Sample
		WriteToByteStream<uint8>(CoefficientIndex, EncodedADPCMData, WriteIndex);
		WriteToByteStream<int16>(Context.AdaptationDelta, EncodedADPCMData, WriteIndex);
		WriteToByteStream<int16>(Context.Sample1, EncodedADPCMData, WriteIndex);
		WriteToByteStream<int16>(Context.Sample2, EncodedADPCMData, WriteIndex);

		// Process all the nibble pairs after the preamble.
		while (WriteIndex < BlockSize)
		{
			EncodedADPCMData[WriteIndex] = EncodeNibble(Context, ReadFromArray<int16>(InputPCMSamples, ReadIndex, NumSamples, SampleStride)) << 4;
			EncodedADPCMData[WriteIndex++] |= EncodeNibble(Context, ReadFromArray<int16>(InputPCMSamples, ReadIndex, NumSamples, SampleStride));
		}

		return WriteIndex;
	}

	void Encode(const TArray<uint8>& InputPCMData, TArray<uint8>& CompressedDataStore, const FSoundQualityInfo& QualityInfo)
	{
		const int32 SourceSampleStride = QualityInfo.NumChannels;

		// Input source samples are 2-bytes
		const int32 SourceNumSamples = QualityInfo.SampleDataSize / 2;
		const int32 SourceNumSamplesPerChannel = SourceNumSamples / QualityInfo.NumChannels;

		// Output samples are 4-bits
		const int32 CompressedNumSamplesPerByte = 2;
		const int32 PreambleSamples = 2;
		const int32 BlockSize = 512;
		const int32 PreambleSize = 2 * PreambleSamples + 3;
		const int32 CompressedSamplesPerBlock = (BlockSize - PreambleSize) * CompressedNumSamplesPerByte + PreambleSamples;
		int32 NumBlocksPerChannel = SourceNumSamplesPerChannel / CompressedSamplesPerBlock;

		// If our storage didn't exactly line up with the number of samples, we need an extra block
		if (NumBlocksPerChannel * CompressedSamplesPerBlock != SourceNumSamplesPerChannel)
		{
			NumBlocksPerChannel++;
		}

		const uint32 EncodedADPCMDataSize = NumBlocksPerChannel * BlockSize * QualityInfo.NumChannels;
		uint8* EncodedADPCMData = static_cast<uint8*>(FMemory::Malloc(EncodedADPCMDataSize));
		FMemory::Memzero(EncodedADPCMData, EncodedADPCMDataSize);

		const int16* InputPCMSamples = reinterpret_cast<const int16*>(InputPCMData.GetData());
		uint8* EncodedADPCMChannelData = EncodedADPCMData;

		// Encode each channel, appending channel output as we go.
		for (uint32 ChannelIndex = 0; ChannelIndex < QualityInfo.NumChannels; ++ChannelIndex)
		{
			const int16* ChannelPCMSamples = InputPCMSamples + ChannelIndex;
			int32 SourceSampleOffset = 0;
			int32 DestDataOffset = 0;
			
			for (int32 BlockIndex = 0; BlockIndex < NumBlocksPerChannel; ++BlockIndex)
			{
				EncodeBlock(ChannelPCMSamples + SourceSampleOffset, SourceSampleStride, SourceNumSamples - SourceSampleOffset, BlockSize, EncodedADPCMChannelData + DestDataOffset);
				
				SourceSampleOffset += CompressedSamplesPerBlock * SourceSampleStride;
				DestDataOffset += BlockSize;
			}

			EncodedADPCMChannelData += DestDataOffset;
		}

		ADPCMFormatHeader Format;
		Format.BaseFormat.nChannels = static_cast<uint16>(QualityInfo.NumChannels);
		Format.BaseFormat.nSamplesPerSec = QualityInfo.SampleRate;
		Format.BaseFormat.nBlockAlign = static_cast<uint16>(BlockSize);
		Format.BaseFormat.wBitsPerSample = 4;
		Format.BaseFormat.wFormatTag = WAVE_FORMAT_ADPCM;
		Format.wSamplesPerBlock = static_cast<uint16>(CompressedSamplesPerBlock);
		Format.BaseFormat.nAvgBytesPerSec = ((Format.BaseFormat.nSamplesPerSec / Format.wSamplesPerBlock) * Format.BaseFormat.nBlockAlign);
		Format.wNumCoef = NUM_ADAPTATION_COEFF;
		Format.BaseFormat.cbSize = sizeof(Format) - sizeof(Format.BaseFormat);

		RiffDataChunk RiffDataChunks[2];
		RiffDataChunks[0].ID = UE_MAKEFOURCC('f','m','t',' ');
		RiffDataChunks[0].DataSize = sizeof(Format);
		RiffDataChunks[0].Data = reinterpret_cast<uint8*>(&Format);

		RiffDataChunks[1].ID = UE_MAKEFOURCC('d','a','t','a');
		RiffDataChunks[1].DataSize = EncodedADPCMDataSize;
		RiffDataChunks[1].Data = EncodedADPCMData;

		GenerateWaveFile(RiffDataChunks, CompressedDataStore);
		FMemory::Free(EncodedADPCMData);
	}
} // end namespace ADPCM

class FAudioFormatADPCM : public IAudioFormat
{
	enum
	{
		/** Version for ADPCM format, this becomes part of the DDC key. */
		UE_AUDIO_ADPCM_VER = 0,
	};

public:
	virtual bool AllowParallelBuild() const
	{
		return false;
	}

	virtual uint16 GetVersion(FName Format) const override
	{
		check(Format == NAME_ADPCM);
		return UE_AUDIO_ADPCM_VER;
	}


	virtual void GetSupportedFormats(TArray<FName>& OutFormats) const
	{
		OutFormats.Add(NAME_ADPCM);
	}

	virtual bool Cook(FName Format, const TArray<uint8>& SrcBuffer, FSoundQualityInfo& QualityInfo, TArray<uint8>& CompressedDataStore) const
	{
		check(Format == NAME_ADPCM);

		if (QualityInfo.Quality == 100)
		{
			LPCM::Encode(SrcBuffer, CompressedDataStore, QualityInfo);
		}
		else
		{
			ADPCM::Encode(SrcBuffer, CompressedDataStore, QualityInfo);
		}
		
		return CompressedDataStore.Num() > 0;
	}

	virtual bool CookSurround(FName Format, const TArray<TArray<uint8> >& SrcBuffers, FSoundQualityInfo& QualityInfo, TArray<uint8>& CompressedDataStore) const
	{
		check(Format == NAME_ADPCM);

		// Unsupported for iOS devices
		return false;
	}

	virtual int32 Recompress(FName Format, const TArray<uint8>& SrcBuffer, FSoundQualityInfo& QualityInfo, TArray<uint8>& OutBuffer) const
	{
		check(Format == NAME_ADPCM);

		// Recompress is only necessary during editor previews
		return 0;
	}
};


/**
 * Module for ADPCM audio compression
 */

static IAudioFormat* Singleton = NULL;

class FAudioPlatformADPCMModule : public IAudioFormatModule
{
public:
	virtual ~FAudioPlatformADPCMModule()
	{
		delete Singleton;
		Singleton = NULL;
	}
	virtual IAudioFormat* GetAudioFormat()
	{
		if (!Singleton)
		{
			/* TODO::JTM - Dec 10, 2012 09:55AM - Library initialization */
			Singleton = new FAudioFormatADPCM();
		}
		return Singleton;
	}
};

IMPLEMENT_MODULE(FAudioPlatformADPCMModule, AudioFormatADPCM);
