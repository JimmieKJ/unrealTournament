// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

/* Public dependencies
*****************************************************************************/

#include "CoreMinimal.h"

#include "AudioMixerBuffer.h"
#include "AudioMixerSourceManager.h"

namespace Audio
{
	class FMixerDevice;
	class FMixerSourceVoice;
	class FMixerSource;
	class FMixerBuffer;
	class ISourceBufferQueueListener;

	static const int32 MAX_BUFFERS_QUEUED = 3;
	static const int32 LOOP_FOREVER = -1;


	/** Struct defining a source voice buffer. */
	struct FMixerSourceVoiceBuffer
	{
		/** Raw audio PCM data. */
		TArray<uint8> AudioData;

		/** The amount of real audio data in the byte array (may not be the size of the array in the case of procedural audio. */
		uint32 AudioBytes;

		/** How many times this buffer will loop. */
		int32 LoopCount;

		/** If this buffer is from real-time decoding and needs to make callbacks for more data. */
		uint32 bRealTimeBuffer : 1;

		FMixerSourceVoiceBuffer()
		{
			FMemory::Memzero(this, sizeof(this));
		}
	};

	typedef FAsyncRealtimeAudioTaskProxy<FMixerBuffer> FAsyncRealtimeAudioTask;

	/** 
	 * FMixerSource
	 * Class which implements a sound source object for the audio mixer module.
	 */
	class FMixerSource :	public FSoundSource, 
							public ISourceBufferQueueListener
	{
	public:
		/** Constructor. */
		FMixerSource(FAudioDevice* InAudioDevice);

		/** Destructor. */
		~FMixerSource();

		//~ Begin FSoundSource Interface
		bool Init(FWaveInstance* InWaveInstance) override;
		void Update() override;
		bool PrepareForInitialization(FWaveInstance* InWaveInstance) override;
		bool IsPreparedToInit() override;
		void Play() override;
		void Stop() override;
		void Pause() override;
		bool IsFinished() override;
		FString Describe(bool bUseLongName) override;
		//~ End FSoundSource Interface

		//~Begin ISourceBufferQueueListener
		void OnSourceBufferEnd() override;
		//~End ISourceBufferQueueListener

	private:

		/** Enum describing the data-read mode of an audio buffer. */
		enum class EBufferReadMode : uint8
		{
			/** Read the next buffer synchronously. */
			Synchronous,

			/** Read the next buffer asynchronously. */
			Asynchronous,

			/** Read the next buffer asynchronously but skip the first chunk of audio. */
			AsynchronousSkipFirstFrame
		};

		/** Submit the current decoded PCM buffer to the contained source voice. */
		void SubmitPCMBuffers();

		/** Submit the current decoded PCMRT (PCM RealTime) buffer to the contained source voice. */
		void SubmitPCMRTBuffers();

		/** Reads more PCMRT data for the real-time decoded audio buffers. */
		bool ReadMorePCMRTData(const int32 BufferIndex, EBufferReadMode BufferReadMode, bool* OutLooped = nullptr);

		/** Called when a buffer finishes for a real-time source and more buffers need to be read and submitted. */
		void ProcessRealTimeSource(bool bBlockForData);

		/** Submits new real-time decoded buffers to a source voice. */
		void SubmitRealTimeSourceData(bool bLooped);

		/** Frees any resources for this sound source. */
		void FreeResources();

		/** Updates the pitch parameter set from the game thread. */
		void UpdatePitch();
		
		/** Updates the volume parameter set from the game thread. */
		void UpdateVolume();

		/** Gets updated spatialization information for the voice. */
		void UpdateSpatialization();

		/** Updates and source effect on this voice. */
		void UpdateEffects();

		/** Updates the channel map of the sound if its a 3d sound.*/
		void UpdateChannelMaps();

		/** Computes the mono-channel map. */
		bool ComputeMonoChannelMap();

		/** Computes the stereo-channel map. */
		bool ComputeStereoChannelMap();

		/** Compute the channel map based on the number of channels. */
		bool ComputeChannelMap(const int32 NumChannels);

		/** Whether or not we should create the source voice with the HRTF spatializer. */
		bool UseHRTSpatialization() const;

	private:

		FMixerDevice* MixerDevice;
		FMixerBuffer* MixerBuffer;
		FMixerSourceVoice* MixerSourceVoice;
		FAsyncRealtimeAudioTask* AsyncRealtimeAudioTask;

		TArray<float> ChannelMap;
		TArray<float> StereoChannelMap;

		int32 CurrentBuffer;
		float PreviousAzimuth;

		// The decoded source buffers are using a shared pointer because the audio mixer thread 
		// will need to have a ref while playing back the sound. There is a small window where a 
		// flushed/destroyed sound can destroy the buffer before the sound finishes using the buffer.
		TArray<FMixerSourceBufferPtr> SourceVoiceBuffers;

		FSpatializationParams SpatializationParams;

		FThreadSafeBool bPlayedCachedBuffer;
		FThreadSafeBool bPlaying;
		FThreadSafeBool bLoopCallback;
		FThreadSafeBool bIsFinished;
		FThreadSafeBool bBuffersToFlush;

		uint32 bResourcesNeedFreeing : 1;
		uint32 bEditorWarnedChangedSpatialization : 1;
		uint32 bUsingHRTFSpatialization : 1;
		uint32 bDebugMode : 1;
	};
}
