// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

/* Public dependencies
*****************************************************************************/

#include "CoreMinimal.h"

#include "AudioDecompress.h"
#include "AudioMixer.h"

namespace Audio
{
	namespace EBufferType
	{
		enum Type
		{
			PCM,
			PCMPreview,
			PCMRealTime,
			Streaming,
			Invalid,
		};
	}

	class FMixerDevice;
	class FMixerBuffer;

	typedef FAsyncRealtimeAudioTaskProxy<FMixerBuffer> FAsyncRealtimeAudioTask;

	class FMixerBuffer : public FSoundBuffer
	{
	public:
		FMixerBuffer(FAudioDevice* InAudioDevice, USoundWave* InWave, EBufferType::Type InBufferType);
		~FMixerBuffer();

		//~ Begin FSoundBuffer Interface
		int32 GetSize() override;
		int32 GetCurrentChunkIndex() const override;
		int32 GetCurrentChunkOffset() const override;
		bool IsRealTimeSourceReady() override;
		bool ReadCompressedInfo(USoundWave* SoundWave) override;
		bool ReadCompressedData(uint8* Destination, bool bLooping) override;
		void Seek(const float SeekTime) override;

		//~ End FSoundBuffer Interface

		static FMixerBuffer* Init(FAudioDevice* AudioDevice, USoundWave* InWave, bool bForceRealtime);
		static FMixerBuffer* CreatePreviewBuffer(FMixerDevice* InMixer, USoundWave* InWave);
		static FMixerBuffer* CreateProceduralBuffer(FMixerDevice* InMixer, USoundWave* InWave);
		static FMixerBuffer* CreateNativeBuffer(FMixerDevice* InMixer, USoundWave* InWave);
		static FMixerBuffer* CreateStreamingBuffer(FMixerDevice* InMixer, USoundWave* InWave);
		static FMixerBuffer* CreateRealTimeBuffer(FMixerDevice* InMixer, USoundWave* InWave);

		/** Returns the buffer's format */
		EBufferType::Type GetType() const;
		bool IsRealTimeBuffer() const;

		/** Returns the contained raw PCM data and data size */
		void GetPCMData(uint8** OutData, uint32* OutDataSize);

		void EnsureHeaderParseTaskFinished();

		float GetSampleRate() const { return SampleRate; }

	private:

		/** Async task for parsing real-time decompressed compressed info headers. */
		FAsyncRealtimeAudioTask* RealtimeAsyncHeaderParseTask;

		/** Wrapper to handle the decompression of audio codecs. */
		ICompressedAudioInfo* DecompressionState;

		/** Format of the sound referenced by this buffer */
		EBufferType::Type BufferType;

		/** Sample rate of the audio buffer. */
		int32 SampleRate;

		/** Number of bits per sample. */
		int16 BitsPerSample;

		/** Ptr to raw PCM data. */
		uint8* Data;

		/** The raw PCM data size. */
		uint32 DataSize;

		/** Bool indicating the that the real-time source is ready for real-time decoding. */
		FThreadSafeBool bIsRealTimeSourceReady;

		/** Set to true when the PCM data should be freed when the buffer is destroyed */
		bool bIsDynamicResource;
	};
}

