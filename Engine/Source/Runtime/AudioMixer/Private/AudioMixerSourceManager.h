// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

/* Public dependencies
*****************************************************************************/

#include "CoreMinimal.h"
#include "AudioMixerBuffer.h"
#include "DSP/OnePole.h"
#include "IAudioExtensionPlugin.h"
#include "Containers/Queue.h"

#define ENABLE_AUDIO_OUTPUT_DEBUGGING 0

#if AUDIO_MIXER_ENABLE_DEBUG_MODE

// Macro which checks if the source id is in debug mode, avoids having a bunch of #ifdefs in code
#define AUDIO_MIXER_DEBUG_LOG(SourceId, Format, ...)																							\
	if (bIsDebugMode[SourceId])																													\
	{																																			\
		FString CustomMessage = FString::Printf(Format, ##__VA_ARGS__);																			\
		FString LogMessage = FString::Printf(TEXT("<Debug Sound Log> [Id=%d][Name=%s]: %s"), SourceId, *DebugName[SourceId], *CustomMessage);	\
		UE_LOG(LogAudioMixer, Log, TEXT("%s"), *LogMessage);																								\
	}

#else

#define AUDIO_MIXER_DEBUG_LOG(SourceId, Message)

#endif


namespace Audio
{
	class FMixerSubmix;
	class FMixerDevice;
	class FMixerSourceVoice;

	struct FMixerSourceVoiceBuffer;

	typedef TSharedPtr<FMixerSourceVoiceBuffer, ESPMode::ThreadSafe> FMixerSourceBufferPtr;

	class ISourceBufferQueueListener
	{
	public:
		// Called when the current buffer is finished and a new one needs to be queued
		virtual void OnSourceBufferEnd() = 0;
	};

	struct FMixerSourceVoiceInitParams
	{
		ISourceBufferQueueListener* BufferQueueListener;
		FMixerSubmix* OwningSubmix;
		FMixerSourceVoice* SourceVoice;
		int32 NumInputChannels;
		int32 NumInputFrames;
		FString DebugName;
		bool bUseHRTFSpatialization;
		bool bIsDebugMode;

		FMixerSourceVoiceInitParams()
			: BufferQueueListener(nullptr)
			, OwningSubmix(nullptr)
			, SourceVoice(nullptr)
			, NumInputChannels(0)
			, NumInputFrames(0)
			, bUseHRTFSpatialization(false)
			, bIsDebugMode(false)
		{}
	};

	class FSourceParam
	{
	public:
		FSourceParam(float InNumInterpFrames);

		void Reset();

		void SetValue(float InValue);
		float operator()();

	private:
		float StartValue;
		float EndValue;
		float CurrentValue;
		float NumInterpFrames;
		float Frame;
		bool bIsInit;
	};


	class FSourceChannelMap
	{
	public:
		FSourceChannelMap(float InNumInterpFrames)
			: NumInterpFrames(InNumInterpFrames)
		{}

		void Reset()
		{
			ChannelValues.Reset();
		}

		void SetChannelMap(const TArray<float>& ChannelMap)
		{
			if (!ChannelValues.Num())
			{
				for (int32 i = 0; i < ChannelMap.Num(); ++i)
				{
					ChannelValues.Add(FSourceParam(NumInterpFrames));
					ChannelValues[i].SetValue(ChannelMap[i]);
				}
			}
			else
			{
				for (int32 i = 0; i < ChannelMap.Num(); ++i)
				{
					ChannelValues[i].SetValue(ChannelMap[i]);
				}
			}
		}

		void GetChannelMap(TArray<float>& OutChannelMap)
		{
			OutChannelMap.Reset();
			for (int32 i = 0; i < ChannelValues.Num(); ++i)
			{
				OutChannelMap.Add(ChannelValues[i]());
			}
		}

	private:
		TArray<FSourceParam> ChannelValues;
		float NumInterpFrames;
	};

	class FMixerSourceManager
	{
	public:
		FMixerSourceManager(FMixerDevice* InMixerDevice);
		~FMixerSourceManager();

		void Init(const int32 InNumSources);
		void Update();

		bool GetFreeSourceId(int32& OutSourceId);
		int32 GetNumActiveSources() const;

		void ReleaseSourceId(const int32 SourceId);
		void InitSource(const int32 SourceId, const FMixerSourceVoiceInitParams& InitParams);

		void Play(const int32 SourceId);
		void Stop(const int32 SourceId);
		void Pause(const int32 SourceId);
		void SetPitch(const int32 SourceId, const float Pitch);
		void SetVolume(const int32 SourceId, const float Volume);
		void SetSpatializationParams(const int32 SourceId, const FSpatializationParams& InParams);
		void SetWetLevel(const int32 SourceId, const float WetLevel);
		void SetChannelMap(const int32 SourceId, const TArray<float>& InChannelMap);
		void SetLPFFrequency(const int32 SourceId, const float Frequency);
		
		void SubmitBuffer(const int32 SourceId, FMixerSourceBufferPtr InSourceVoiceBuffer);
		void SubmitBufferAudioThread(const int32 SourceId, FMixerSourceBufferPtr InSourceVoiceBuffer);

		int64 GetNumFramesPlayed(const int32 SourceId) const;
		bool IsDone(const int32 SourceId) const;

		void ComputeNextBlockOfSamples();
		void MixOutputBuffers(const int32 SourceId, TArray<float>& OutDryBuffer, TArray<float>& OutWetBuffer) const;

	private:

		void ReleaseSource(const int32 SourceId);
		void ReadSourceFrame(const int32 SourceId);
		void ComputeSourceBuffers();
		void ComputePostSourceEffectBuffers();
		void ComputeOutputBuffers();

		void AudioMixerThreadCommand(TFunction<void()> InFunction);
		void PumpCommandQueue();

		static const int32 NUM_BYTES_PER_SAMPLE = 2;

		FMixerDevice* MixerDevice;

		// Array of pointers to game thread audio source objects
		TArray<FMixerSourceVoice*> MixerSources;

		// A command queue to execute commands from audio thread (or game thread) to audio mixer device thread.
		TQueue<TFunction<void()>> SourceCommandQueue;

		/////////////////////////////////////////////////////////////////////////////
		// Vectorized source voice info
		/////////////////////////////////////////////////////////////////////////////

		// Raw PCM buffer data
		TArray<TQueue<FMixerSourceBufferPtr>> BufferQueue;
		TArray<ISourceBufferQueueListener*> BufferQueueListener;
		TArray<FThreadSafeCounter> NumBuffersQueued;

		// Debugging source generators
#if ENABLE_AUDIO_OUTPUT_DEBUGGING
		TArray<FSineOsc> DebugOutputGenerators;
#endif
		// Array of sources which are currently debug solo'd
		TArray<int32> DebugSoloSources;
		TArray<bool> bIsDebugMode;
		TArray<FString> DebugName;

		// Data used for rendering sources
		TArray<FMixerSourceBufferPtr> CurrentPCMBuffer;
		TArray<int32> CurrentAudioChunkNumFrames;
		TArray<TArray<float>> SourceBuffer;
		TArray<TArray<float>> CurrentFrameValues;
		TArray<TArray<float>> NextFrameValues;
		TArray<float> CurrentFrameAlpha;
		TArray<int32> CurrentFrameIndex;
		TArray<int64> NumFramesPlayed;

		TArray<FSourceParam> PitchSourceParam;
		TArray<FSourceParam> VolumeSourceParam;
		TArray<FSourceParam> WetLevelSourceParam;
		TArray<FSourceParam> LPFCutoffFrequencyParam;

		// Simple LPFs for all sources (all channels of all sources)
		TArray<TArray<FOnePoleLPF>> LowPassFilters;

		TArray<FSourceChannelMap> ChannelMapParam;
		TArray<FSpatializationParams> SpatParams;
		TArray<float> ScratchChannelMap;

		// Output data, after computing a block of sample data, this is read back from mixers
		TArray<TArray<float>> PostEffectBuffers;
		TArray<TArray<float>> DryBuffer;
		TArray<TArray<float>> WetBuffer;

		// Buffer used as an intermediate buffer between effects
		TArray<float> ScratchBuffer;

		// State management
		TArray<bool> bIsActive;
		TArray<bool> bIsPlaying;
		TArray<bool> bIsPaused;
		TArray<bool> bHasStarted;
		TArray<bool> bIsBusy;
		TArray<bool> bUseHRTFSpatializer;
		TArray<bool> bIsDone;

		// Source format info
		TArray<int32> NumInputChannels;
		TArray<int32> NumPostEffectChannels;
		TArray<int32> NumInputFrames;

		// General information about sources in source manager accessible from game thread
		struct FGameThreadInfo
		{
			TArray<int32> FreeSourceIndices;
			TArray<bool> bIsBusy;
			TArray<FThreadSafeBool> bIsDone;
			TArray<bool> bIsDebugMode;
		} GameThreadInfo;

		int32 NumActiveSources;
		int32 NumTotalSources;
		uint8 bInitialized : 1;

		friend class FMixerSourceVoice;

		// Set to true when the audio source manager should pump the command queue
		FThreadSafeBool bPumpQueue;
	};
}
