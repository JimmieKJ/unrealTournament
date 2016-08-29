// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "UnrealAudioTypes.h"
#include "UnrealAudioHandles.h"
#include "UnrealAudioSoundFileManager.h"
#include "UnrealAudioTypes.h"
#include "UnrealAudioSoundFile.h"
#include "UnrealAudioSoundFileInternal.h"

#if ENABLE_UNREAL_AUDIO

namespace UAudio
{
	struct FSoundFileDecoderSettings
	{
		int32 NumDecodeBuffers;
		int32 DecodeBufferFrames;
	};

	class FSoundFileDecoder : public FRunnable
	{
	public:
		FSoundFileDecoder(FUnrealAudioModule* InAudioModule);
		~FSoundFileDecoder();

		// FRunnable
		bool Init() override;
		uint32 Run() override;
		void Stop() override;
		void Exit() override;

		bool Init(const FSoundFileDecoderSettings& Settings, int32 NumVoices);
		void InitalizeEntry(uint32 Index, TSharedPtr<ISoundFile> InSoundFileData);
		void ClearEntry(uint32 Index);

		bool GetDecodedAudioData(uint32 Index, TArray<float>& AudioData);

	private:

		void Signal();

		struct FSoundFileDecodeData
		{
			FSoundFileHandle SoundFileHandle;
			TSharedPtr<FSoundFileReader> SoundFileReader;
			TArray<TArray<float>> DecodedBuffers;
			FThreadSafeBool bIsActive;
			int32 CurrentReadSampleIndex;
			int32 LastSampleIndex;
			int32 NumChannels;
			bool bIsLooping;
			FThreadSafeCounter CurrentWriteBufferIndex;
			FThreadSafeCounter CurrentReadBufferIndex;

			FSoundFileDecodeData(int32 NumDecodeBuffers)
				: bIsActive(false)
				, CurrentReadSampleIndex(0)
				, LastSampleIndex(0)
				, NumChannels(1)
				, bIsLooping(false)
				, CurrentWriteBufferIndex(0)
				, CurrentReadBufferIndex(0)
			{
				// Initialize this entries decode buffers
				DecodedBuffers.Init(TArray<float>(), NumDecodeBuffers);
			}
		};

		FUnrealAudioModule* AudioModule;
		FEvent* ThreadDecodeEvent;
		FSoundFileDecoderSettings Settings;
		TArray<FSoundFileDecodeData> DecodeData;
		FThreadSafeBool bIsDecoding;
		FRunnableThread* DecodeThread;
	};
}



#endif // #if ENABLE_UNREAL_AUDIO

