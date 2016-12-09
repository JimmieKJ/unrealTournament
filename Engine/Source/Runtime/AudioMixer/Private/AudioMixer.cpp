// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "AudioMixer.h"
#include "AudioMixerDevice.h"
#include "HAL/RunnableThread.h"

namespace Audio
{
	/**
	 * IAudioMixerPlatformInterface
	 */

	IAudioMixerPlatformInterface::IAudioMixerPlatformInterface()
		: AudioRenderThread(nullptr)
		, AudioRenderEvent(nullptr)
		, CurrentBufferIndex(0)
		, LastError(TEXT("None"))
	{
	}

	void IAudioMixerPlatformInterface::GenerateBuffer(TArray<float>& Buffer)
	{
		// Call into platform independent code to process next stream
		AudioStreamInfo.AudioMixer->OnProcessAudioStream(Buffer);
	}

	void IAudioMixerPlatformInterface::ReadNextBuffer()
	{
		AUDIO_MIXER_CHECK(AudioStreamInfo.StreamState == EAudioOutputStreamState::Running);

		{
			FScopeLock Lock(&AudioRenderCritSect);

			SubmitBuffer(OutputBuffers[CurrentBufferIndex]);

			// Increment the buffer index
			CurrentBufferIndex = (CurrentBufferIndex + 1) % NumMixerBuffers;
		}

		AudioRenderEvent->Trigger();
	}

	void IAudioMixerPlatformInterface::BeginGeneratingAudio()
	{
		FAudioPlatformDeviceInfo & DeviceInfo = AudioStreamInfo.DeviceInfo;

		// Setup the output buffers
		for (int32 Index = 0; Index < NumMixerBuffers; ++Index)
		{
			OutputBuffers[Index].SetNumZeroed(DeviceInfo.NumSamples);
		}

		// Submit the first empty buffer. This will begin audio callback notifications on some platforms.
		SubmitBuffer(OutputBuffers[CurrentBufferIndex++]);

		AudioStreamInfo.StreamState = EAudioOutputStreamState::Running;
		check(AudioRenderEvent == nullptr);
		AudioRenderEvent = FPlatformProcess::GetSynchEventFromPool();
		check(AudioRenderThread == nullptr);
		AudioRenderThread = FRunnableThread::Create(this, TEXT("AudioMixerRenderThread"), 0, TPri_AboveNormal);
	}

	void IAudioMixerPlatformInterface::StopGeneratingAudio()
	{
		// Stop the FRunnable thread
		{
			FScopeLock Lock(&AudioRenderCritSect);

			if (AudioStreamInfo.StreamState != EAudioOutputStreamState::Stopped)
			{
				AudioStreamInfo.StreamState = EAudioOutputStreamState::Stopping;
			}

			// Make sure the thread wakes up
			AudioRenderEvent->Trigger();
		}

		AudioRenderThread->WaitForCompletion();
		check(AudioStreamInfo.StreamState == EAudioOutputStreamState::Stopped);

		delete AudioRenderThread;
		AudioRenderThread = nullptr;

		FPlatformProcess::ReturnSynchEventToPool(AudioRenderEvent);
		AudioRenderEvent = nullptr;
	}

	uint32 IAudioMixerPlatformInterface::Run()
	{
		while (AudioStreamInfo.StreamState != EAudioOutputStreamState::Stopping)
		{
			{
				FScopeLock Lock(&AudioRenderCritSect);

				// Zero the current output buffer
				FPlatformMemory::Memzero(OutputBuffers[CurrentBufferIndex].GetData(), OutputBuffers[CurrentBufferIndex].Num() * sizeof(float));
				AudioStreamInfo.AudioMixer->OnProcessAudioStream(OutputBuffers[CurrentBufferIndex]);
			}

			// Note that this wait has to be outside the scope lock to avoid deadlocking
			AudioRenderEvent->Wait();
		}

		AudioStreamInfo.StreamState = EAudioOutputStreamState::Stopped;
		return 0;
	}

	uint32 IAudioMixerPlatformInterface::GetNumBytesForFormat(const EAudioMixerStreamDataFormat::Type DataFormat)
	{
		switch (DataFormat)
		{
			case EAudioMixerStreamDataFormat::Float:	return 4;
			case EAudioMixerStreamDataFormat::Double:	return 8;
			case EAudioMixerStreamDataFormat::Int16:	return 2;
			case EAudioMixerStreamDataFormat::Int24:	return 3;
			case EAudioMixerStreamDataFormat::Int32:	return 4;

			default:
			checkf(false, TEXT("Unknown or unsupported data format."))
				return 0;
		}
	}
}