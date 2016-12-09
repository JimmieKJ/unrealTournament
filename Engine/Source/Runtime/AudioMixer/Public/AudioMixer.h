// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AudioMixerLog.h"
#include "AudioDecompress.h"
#include "AudioEffect.h"
#include "AudioMixerTypes.h"
#include "HAL/Runnable.h"

// defines used for AudioMixer.h
#define AUDIO_PLATFORM_ERROR(INFO)			(OnAudioMixerPlatformError(INFO, FString(__FILE__), __LINE__))

#ifndef AUDIO_MIXER_ENABLE_DEBUG_MODE
// This define enables a bunch of more expensive debug checks and logging capabilities that are intended to be off most of the time even in debug builds of game/editor.
#define AUDIO_MIXER_ENABLE_DEBUG_MODE 0
#endif

// Enable debug checking for audio mixer

#if AUDIO_MIXER_ENABLE_DEBUG_MODE
#define AUDIO_MIXER_CHECK(expr) check(expr)
#define AUDIO_MIXER_CHECK_GAME_THREAD(_MixerDevice)			(_MixerDevice->CheckGameThread())
#define AUDIO_MIXER_CHECK_AUDIO_PLAT_THREAD(_MixerDevice)	(_MixerDevice->CheckAudioPlatformThread())
#else
#define AUDIO_MIXER_CHECK(expr)
#define AUDIO_MIXER_CHECK_GAME_THREAD(_MixerDevice)
#define AUDIO_MIXER_CHECK_AUDIO_PLAT_THREAD(_MixerDevice)
#endif

#define AUDIO_MIXER_MIN_PITCH						0.1f
#define AUDIO_MIXER_MAX_PITCH						4.0f

namespace Audio
{

	/** Structure to hold platform device information **/
	struct FAudioPlatformDeviceInfo
	{
		/** The name of the audio device */
		FString Name;

		/** The number of channels supported by the audio device */
		int32 NumChannels;

		/** The sample rate of the audio device */
		int32 SampleRate;

		/** The default sample of the audio device */
		int32 DefaultSampleRate;

		/** The number of frames processed by audio device */
		int32 NumFrames;

		/** The number of samples processed by the audio device (NumChannels * NumFrames) */
		int32 NumSamples;

		/** The data format of the audio stream */
		EAudioMixerStreamDataFormat::Type Format;

		/** The output channel array of the audio device */
		TArray<EAudioMixerChannel::Type> OutputChannelArray;

		/** The estimated latency of the audio device */
		uint32 Latency;

		/** Whether or not this device is the system default */
		uint8 bIsSystemDefault : 1;

		FAudioPlatformDeviceInfo()
		{
			Reset();
		}

		void Reset()
		{
			Name = TEXT("Unknown");
			NumChannels = 0;
			SampleRate = 0;
			DefaultSampleRate = 0;
			NumFrames = 0;
			NumSamples = 0;
			Format = EAudioMixerStreamDataFormat::Unknown;
			OutputChannelArray.Reset();
			Latency = 0;
			bIsSystemDefault = false;
		}

	};

	/** Platform independent audio mixer interface. */
	class IAudioMixer
	{
	public:
		/** Callback to generate a new audio stream buffer. */
		virtual bool OnProcessAudioStream(TArray<float>& OutputBuffer) = 0;
	};

	/** Defines parameters needed for opening a new audio stream to device. */
	struct FAudioMixerOpenStreamParams
	{
		/** The audio device index to open. */
		uint32 OutputDeviceIndex;

		/** The number of desired audio frames in audio callback. */
		uint32 NumFrames;
		
		/** Owning platform independent audio mixer ptr.*/
		IAudioMixer* AudioMixer;
		
		/** The desired sample rate */
		uint32 SampleRate;

		FAudioMixerOpenStreamParams()
			: OutputDeviceIndex(INDEX_NONE)
			, NumFrames(1024)
			, AudioMixer(nullptr)
			, SampleRate(44100)
		{}
	};

	struct FAudioOutputStreamInfo
	{
		/** The index of the output device for the audio stream. */
		uint32 OutputDeviceIndex;

		FAudioPlatformDeviceInfo DeviceInfo;

		/** The requested sample rate. */
		uint32 RequestedSampleRate;

		/** The state of the output audio stream. */
		EAudioOutputStreamState::Type StreamState;

		/** The callback to use for platform-independent layer. */
		IAudioMixer* AudioMixer;

		/** The number of frames used in each audio callback. */
		uint32 NumOutputFrames;

		/** Whether or not we need to perform a format conversion of the audio for this output device. */
		uint8 bPerformFormatConversion : 1;

		/** Whether or not we need to perform a byte swap for this output device. */
		uint8 bPerformByteSwap : 1;

		FAudioOutputStreamInfo()
		{
			Reset();
		}

		~FAudioOutputStreamInfo()
		{

		}

		void Reset()
		{
			OutputDeviceIndex = 0;
			DeviceInfo.Reset();
			StreamState = EAudioOutputStreamState::Closed;
			AudioMixer = nullptr;
			NumOutputFrames = 0;
			bPerformFormatConversion = false;
			bPerformByteSwap = false;
		}
	};


	/** Abstract interface for mixer platform. */
	class AUDIOMIXER_API IAudioMixerPlatformInterface : public FRunnable
	{

	public: // Virtual functions

		/** Virtual destructor. */
		virtual ~IAudioMixerPlatformInterface() {}

		/** Returns the platform API enumeration. */
		virtual EAudioMixerPlatformApi::Type GetPlatformApi() const = 0;

		/** Initialize the hardware. */
		virtual bool InitializeHardware() = 0;

		/** Teardown the hardware. */
		virtual bool TeardownHardware() = 0;
		
		/** Is the hardware initialized. */
		virtual bool IsInitialized() const = 0;

		/** Returns the number of output devices. */
		virtual bool GetNumOutputDevices(uint32& OutNumOutputDevices) = 0;

		/** Gets the device information of the given device index. */
		virtual bool GetOutputDeviceInfo(const uint32 InDeviceIndex, FAudioPlatformDeviceInfo& OutInfo) = 0;

		/** Returns the default device index. */
		virtual bool GetDefaultOutputDeviceIndex(uint32& OutDefaultDeviceIndex) const = 0;

		/** Opens up a new audio stream with the given parameters. */
		virtual bool OpenAudioStream(const FAudioMixerOpenStreamParams& Params) = 0;

		/** Closes the audio stream (if it's open). */
		virtual bool CloseAudioStream() = 0;

		/** Starts the audio stream processing and generating audio. */
		virtual bool StartAudioStream() = 0;

		/** Stops the audio stream (but keeps the audio stream open). */
		virtual bool StopAudioStream() = 0;

		/** Returns the platform device info of the currently open audio stream. */
		virtual FAudioPlatformDeviceInfo GetPlatformDeviceInfo() const = 0;

		/** Submit the given buffer to the platform's output audio device. */
		virtual void SubmitBuffer(const TArray<float>& Buffer) = 0;

		/** Returns the name of the format of the input sound wave. */
		virtual FName GetRuntimeFormat(USoundWave* InSoundWave) = 0;

		/** Checks if the platform has a compressed audio format for sound waves. */
		virtual bool HasCompressedAudioInfoClass(USoundWave* InSoundWave) = 0;

		/** Creates a Compressed audio info class suitable for decompressing this SoundWave. */
		virtual ICompressedAudioInfo* CreateCompressedAudioInfo(USoundWave* SoundWave) = 0;

	public: // Public Functions
		//~ Begin FRunnable
		uint32 Run() override;
		//~ End FRunnable

		/** Constructor. */
		IAudioMixerPlatformInterface();

		/** Generate the next audio buffer into the given buffer array. */
		void GenerateBuffer(TArray<float>& Buffer);

		/** Retrieves the next generated buffer and feeds it to the platform mixer output stream. */
		void ReadNextBuffer();

		/** Returns the last error generated. */
		FString GetLastError() const { return LastError; }

	protected:

		/** Is called when an error is generated. */
		inline void OnAudioMixerPlatformError(const FString& ErrorDetails, const FString& FileName, int32 LineNumber)
		{
			LastError = FString::Printf(TEXT("Audio Platform Device Error: %s (File %s, Line %d)"), *ErrorDetails, *FileName, LineNumber);
			UE_LOG(LogAudioMixer, Error, TEXT("%s"), *LastError);
		}

		/** Returns the number of bytes for the given audio stream format. */
		uint32 GetNumBytesForFormat(const EAudioMixerStreamDataFormat::Type DataFormat);

		/** Start generating audio from our mixer. */
		void BeginGeneratingAudio();

		/** Stops the render thread from generating audio. */
		void StopGeneratingAudio();

	protected:

		/** The audio device stream info. */
		FAudioOutputStreamInfo AudioStreamInfo;

		/** The number of mixer buffers. */
		static const int32 NumMixerBuffers = 3;

		/** List of generated output buffers. */
		TArray<float> OutputBuffers[NumMixerBuffers];

		/** The audio render thread. */
		FRunnableThread* AudioRenderThread;
		/** The render thread sync event. */
		FEvent* AudioRenderEvent;
		/** The render thread critical section. */
		FCriticalSection AudioRenderCritSect;

		/** The current buffer index. */
		int32 CurrentBufferIndex;

		/** String containing the last generated error. */
		FString LastError;
	};




}
