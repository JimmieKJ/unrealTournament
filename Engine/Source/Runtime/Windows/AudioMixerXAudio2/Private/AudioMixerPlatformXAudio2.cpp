// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/**
	Concrete implementation of FAudioDevice for XAudio2

	See https://msdn.microsoft.com/en-us/library/windows/desktop/hh405049%28v=vs.85%29.aspx
*/

#include "AudioMixerPlatformXAudio2.h"
#include "AudioMixer.h"
#include "AudioMixerDevice.h"
#include "HAL/PlatformAffinity.h"
#include "OpusAudioInfo.h"
#include "VorbisAudioInfo.h"

// Macro to check result code for XAudio2 failure, get the string version, log, and goto a cleanup
#define XAUDIO2_CLEANUP_ON_FAIL(Result)						\
	if (FAILED(Result))										\
	{														\
		const TCHAR* ErrorString = GetErrorString(Result);	\
		AUDIO_PLATFORM_ERROR(ErrorString);					\
		goto Cleanup;										\
	}

// Macro to check result for XAudio2 failure, get string version, log, and return false
#define XAUDIO2_RETURN_ON_FAIL(Result)						\
	if (FAILED(Result))										\
	{														\
		const TCHAR* ErrorString = GetErrorString(Result);	\
		AUDIO_PLATFORM_ERROR(ErrorString);					\
		return false;										\
	}

namespace Audio
{
	void FXAudio2VoiceCallback::OnBufferEnd(void* BufferContext)
	{
		check(BufferContext);
		IAudioMixerPlatformInterface* MixerPlatform = (IAudioMixerPlatformInterface*)BufferContext;
		MixerPlatform->ReadNextBuffer();
	}

	FMixerPlatformXAudio2::FMixerPlatformXAudio2()
		: XAudio2System(nullptr)
		, OutputAudioStreamMasteringVoice(nullptr)
		, OutputAudioStreamSourceVoice(nullptr)
		, bIsComInitialized(false)
		, bIsInitialized(false)
		, bIsDeviceOpen(false)
	{
		// Build the channel map. Index corresponds to unreal audio mixer channel enumeration.
		ChannelTypeMap.Add(SPEAKER_FRONT_LEFT);
		ChannelTypeMap.Add(SPEAKER_FRONT_RIGHT);
		ChannelTypeMap.Add(SPEAKER_FRONT_CENTER);
		ChannelTypeMap.Add(SPEAKER_LOW_FREQUENCY);
		ChannelTypeMap.Add(SPEAKER_BACK_LEFT);
		ChannelTypeMap.Add(SPEAKER_BACK_RIGHT);
		ChannelTypeMap.Add(SPEAKER_FRONT_LEFT_OF_CENTER);
		ChannelTypeMap.Add(SPEAKER_FRONT_RIGHT_OF_CENTER);
		ChannelTypeMap.Add(SPEAKER_BACK_CENTER);
		ChannelTypeMap.Add(SPEAKER_SIDE_LEFT);
		ChannelTypeMap.Add(SPEAKER_SIDE_RIGHT);
		ChannelTypeMap.Add(SPEAKER_TOP_CENTER);
		ChannelTypeMap.Add(SPEAKER_TOP_FRONT_LEFT);
		ChannelTypeMap.Add(SPEAKER_TOP_FRONT_CENTER);
		ChannelTypeMap.Add(SPEAKER_TOP_FRONT_RIGHT);
		ChannelTypeMap.Add(SPEAKER_TOP_BACK_LEFT);
		ChannelTypeMap.Add(SPEAKER_TOP_BACK_CENTER);
		ChannelTypeMap.Add(SPEAKER_TOP_BACK_RIGHT);
	}

	FMixerPlatformXAudio2::~FMixerPlatformXAudio2()
	{
	}

	const TCHAR* FMixerPlatformXAudio2::GetErrorString(HRESULT Result)
	{
		switch (Result)
		{
			case HRESULT(XAUDIO2_E_INVALID_CALL):			return TEXT("XAUDIO2_E_INVALID_CALL");
			case HRESULT(XAUDIO2_E_XMA_DECODER_ERROR):		return TEXT("XAUDIO2_E_XMA_DECODER_ERROR");
			case HRESULT(XAUDIO2_E_XAPO_CREATION_FAILED):	return TEXT("XAUDIO2_E_XAPO_CREATION_FAILED");
			case HRESULT(XAUDIO2_E_DEVICE_INVALIDATED):		return TEXT("XAUDIO2_E_DEVICE_INVALIDATED");
			case REGDB_E_CLASSNOTREG:						return TEXT("REGDB_E_CLASSNOTREG");
			case CLASS_E_NOAGGREGATION:						return TEXT("CLASS_E_NOAGGREGATION");
			case E_NOINTERFACE:								return TEXT("E_NOINTERFACE");
			case E_POINTER:									return TEXT("E_POINTER");
			case E_INVALIDARG:								return TEXT("E_INVALIDARG");
			case E_OUTOFMEMORY:								return TEXT("E_OUTOFMEMORY");
			default:										return TEXT("UKNOWN");
		}
	}

	bool FMixerPlatformXAudio2::InitializeHardware()
	{
		if (bIsInitialized)
		{
			AUDIO_PLATFORM_ERROR(TEXT("XAudio2 already initialized."));
			return false;

		}

		// Load ogg and vorbis dlls if they haven't been loaded yet
		LoadVorbisLibraries();

		bIsComInitialized = FWindowsPlatformMisc::CoInitialize();

		XAUDIO2_RETURN_ON_FAIL(XAudio2Create(&XAudio2System, 0, (XAUDIO2_PROCESSOR)FPlatformAffinity::GetAudioThreadMask()));

		bIsInitialized = true;

		return true;
	}

	bool FMixerPlatformXAudio2::TeardownHardware()
	{
		if (!bIsInitialized)
		{
			AUDIO_PLATFORM_ERROR(TEXT("XAudio2 was already tore down."));
			return false;
		}

		SAFE_RELEASE(XAudio2System);

		if (bIsComInitialized)
		{
			FWindowsPlatformMisc::CoUninitialize();
		}
		bIsInitialized = false;

		return true;
	}

	bool FMixerPlatformXAudio2::IsInitialized() const
	{
		return bIsInitialized;
	}

	bool FMixerPlatformXAudio2::GetNumOutputDevices(uint32& OutNumOutputDevices)
	{
		if (!bIsInitialized)
		{
			AUDIO_PLATFORM_ERROR(TEXT("XAudio2 was not initialized."));
			return false;
		}

		check(XAudio2System);
		XAUDIO2_RETURN_ON_FAIL(XAudio2System->GetDeviceCount(&OutNumOutputDevices));
		return true;
	}

	bool FMixerPlatformXAudio2::GetOutputDeviceInfo(const uint32 InDeviceIndex, FAudioPlatformDeviceInfo& OutInfo)
	{
		if (!bIsInitialized)
		{
			AUDIO_PLATFORM_ERROR(TEXT("XAudio2 was not initialized."));
			return false;
		}

		check(XAudio2System);

		XAUDIO2_DEVICE_DETAILS DeviceDetails;
		XAUDIO2_RETURN_ON_FAIL(XAudio2System->GetDeviceDetails(InDeviceIndex, &DeviceDetails));

		OutInfo.Name = FString(DeviceDetails.DisplayName);
		OutInfo.bIsSystemDefault = (InDeviceIndex == 0);

		// Get the wave format to parse there rest of the device details
		const WAVEFORMATEX& WaveFormatEx = DeviceDetails.OutputFormat.Format;
		OutInfo.SampleRate = WaveFormatEx.nSamplesPerSec;
		OutInfo.NumChannels = WaveFormatEx.nChannels;

		// XAudio2 automatically converts the audio format to output device us so we don't need to do any format conversions
		OutInfo.Format = EAudioMixerStreamDataFormat::Float;

		OutInfo.OutputChannelArray.Reset();

		// Extensible format supports surround sound so we need to parse the channel configuration to build our channel output array
		if (WaveFormatEx.wFormatTag == WAVE_FORMAT_EXTENSIBLE)
		{
			// Cast to the extensible format to get access to extensible data
			const WAVEFORMATEXTENSIBLE* WaveFormatExtensible = (WAVEFORMATEXTENSIBLE*)&WaveFormatEx;

			// Loop through the extensible format channel flags in the standard order and build our output channel array
			// From https://msdn.microsoft.com/en-us/library/windows/hardware/dn653308(v=vs.85).aspx
			// The channels in the interleaved stream corresponding to these spatial positions must appear in the order specified above. This holds true even in the 
			// case of a non-contiguous subset of channels. For example, if a stream contains left, bass enhance and right, then channel 1 is left, channel 2 is right, 
			// and channel 3 is bass enhance. This enables the linkage of multi-channel streams to well-defined multi-speaker configurations.

			uint32 ChanCount = 0;
			for (uint32 ChannelTypeIndex = 0; ChannelTypeIndex < EAudioMixerChannel::ChannelTypeCount && ChanCount < WaveFormatEx.nChannels; ++ChannelTypeIndex)
			{
				if (WaveFormatExtensible->dwChannelMask & ChannelTypeMap[ChannelTypeIndex])
				{
					++ChanCount;
					OutInfo.OutputChannelArray.Add((EAudioMixerChannel::Type)ChannelTypeIndex);
				}
			}

			// Make sure we got the right number of output channels for the device
			checkf(ChanCount == OutInfo.NumChannels, TEXT("Unknown channel type or channel not in proper order."));
		}
		else
		{
			// Non-extensible formats only support mono or stereo channel output
			OutInfo.OutputChannelArray.Add(EAudioMixerChannel::FrontLeft);
			if (OutInfo.NumChannels == 2)
			{
				OutInfo.OutputChannelArray.Add(EAudioMixerChannel::FrontRight);
			}
		}

		UE_LOG(LogAudioMixerDebug, Log, TEXT("Audio Device Output Speaker Info:"), OutInfo.NumChannels);
		UE_LOG(LogAudioMixerDebug, Log, TEXT("Name: %s"), *OutInfo.Name);
		UE_LOG(LogAudioMixerDebug, Log, TEXT("Is Default: %s"), OutInfo.bIsSystemDefault ? TEXT("Yes") : TEXT("No"));
		UE_LOG(LogAudioMixerDebug, Log, TEXT("Sample Rate: %d"), OutInfo.SampleRate);
		UE_LOG(LogAudioMixerDebug, Log, TEXT("Channel Count: %d"), OutInfo.NumChannels);
		UE_LOG(LogAudioMixerDebug, Log, TEXT("Channel Order:"));
		for (int32 i = 0; i < OutInfo.NumChannels; ++i)
		{
			UE_LOG(LogAudioMixerDebug, Log, TEXT("%d: %s"), i, EAudioMixerChannel::ToString(OutInfo.OutputChannelArray[i]));
		}

		return true;
	}

	bool FMixerPlatformXAudio2::GetDefaultOutputDeviceIndex(uint32& OutDefaultDeviceIndex) const
	{
		OutDefaultDeviceIndex = 0;
		return true;
	}

	bool FMixerPlatformXAudio2::OpenAudioStream(const FAudioMixerOpenStreamParams& Params)
	{
		if (!bIsInitialized)
		{
			AUDIO_PLATFORM_ERROR(TEXT("XAudio2 was not initialized."));
			return false;
		}

		if (bIsDeviceOpen)
		{
			AUDIO_PLATFORM_ERROR(TEXT("XAudio2 audio stream already opened."));
			return false;
		}

		checkf(Params.OutputDeviceIndex != INDEX_NONE, TEXT("OpenAudioStream params must specify a output device index."));
		check(XAudio2System);
		check(OutputAudioStreamMasteringVoice == nullptr);

		WAVEFORMATEX Format = { 0 };

		AudioStreamInfo.Reset();

		AudioStreamInfo.OutputDeviceIndex = Params.OutputDeviceIndex;
		AudioStreamInfo.NumOutputFrames = Params.NumFrames;
		AudioStreamInfo.RequestedSampleRate = Params.SampleRate;
		AudioStreamInfo.OutputDeviceIndex = Params.OutputDeviceIndex;
		AudioStreamInfo.AudioMixer = Params.AudioMixer;

		if (!GetOutputDeviceInfo(AudioStreamInfo.OutputDeviceIndex, AudioStreamInfo.DeviceInfo))
		{
			return false;
		}

		AudioStreamInfo.DeviceInfo.NumFrames = Params.NumFrames;
		AudioStreamInfo.DeviceInfo.NumSamples = AudioStreamInfo.DeviceInfo.NumFrames * AudioStreamInfo.DeviceInfo.NumChannels;
		AudioStreamInfo.DeviceInfo.SampleRate = AudioStreamInfo.RequestedSampleRate;

		HRESULT Result = XAudio2System->CreateMasteringVoice(&OutputAudioStreamMasteringVoice, AudioStreamInfo.DeviceInfo.NumChannels, AudioStreamInfo.RequestedSampleRate, 0, AudioStreamInfo.OutputDeviceIndex, nullptr);
		XAUDIO2_CLEANUP_ON_FAIL(Result);

		// Xaudio2 on windows, no need for byte swap
		AudioStreamInfo.bPerformByteSwap = false;

		// Start the xaudio2 engine running, which will now allow us to start feeding audio to it
		XAudio2System->StartEngine();

		// Setup the format of the output source voice
		Format.nChannels = AudioStreamInfo.DeviceInfo.NumChannels;
		Format.nSamplesPerSec = AudioStreamInfo.DeviceInfo.SampleRate;
		Format.wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
		Format.nAvgBytesPerSec = Format.nSamplesPerSec * sizeof(float) * Format.nChannels;
		Format.nBlockAlign = sizeof(float) * Format.nChannels;
		Format.wBitsPerSample = sizeof(float) * 8;

		// Create the output source voice
		Result = XAudio2System->CreateSourceVoice(&OutputAudioStreamSourceVoice, &Format, XAUDIO2_VOICE_NOSRC | XAUDIO2_VOICE_NOPITCH, 2.0f, &OutputVoiceCallback);
		XAUDIO2_RETURN_ON_FAIL(Result);
		
		AudioStreamInfo.StreamState = EAudioOutputStreamState::Open;
		bIsDeviceOpen = true;

	Cleanup:
		if (FAILED(Result))
		{
			CloseAudioStream();
		}
		return SUCCEEDED(Result);
	}

	FAudioPlatformDeviceInfo FMixerPlatformXAudio2::GetPlatformDeviceInfo() const
	{
		return AudioStreamInfo.DeviceInfo;
	}

	bool FMixerPlatformXAudio2::CloseAudioStream()
	{
		if (!bIsInitialized)
		{
			AUDIO_PLATFORM_ERROR(TEXT("XAudio2 was not initialized."));
			return false;
		}

		if (bIsDeviceOpen)
		{
			if (!StopAudioStream())
			{
				return false;
			}
		}

		check(XAudio2System);
		XAudio2System->StopEngine();
		check(OutputAudioStreamSourceVoice);
		OutputAudioStreamSourceVoice->DestroyVoice();
		OutputAudioStreamSourceVoice = nullptr;

		check(OutputAudioStreamMasteringVoice);
		OutputAudioStreamMasteringVoice->DestroyVoice();
		OutputAudioStreamMasteringVoice = nullptr;

		bIsDeviceOpen = false;

		AudioStreamInfo.StreamState = EAudioOutputStreamState::Closed;

		return true;
	}

	bool FMixerPlatformXAudio2::StartAudioStream()
	{
		// Start generating audio with our output source voice
		BeginGeneratingAudio();

		// If we already have a source voice, we can just restart it
		if (OutputAudioStreamSourceVoice)
		{
			AudioStreamInfo.StreamState = EAudioOutputStreamState::Running;
			OutputAudioStreamSourceVoice->Start();
			return true;
		}

		return false;
	}

	bool FMixerPlatformXAudio2::StopAudioStream()
	{
		if (!bIsInitialized)
		{
			AUDIO_PLATFORM_ERROR(TEXT("XAudio2 was not initialized."));
			return false;
		}

		check(XAudio2System);

		if (AudioStreamInfo.StreamState != EAudioOutputStreamState::Stopped)
		{
			// Signal that the thread that is running the update that we're stopping
			if (OutputAudioStreamSourceVoice)
			{
				OutputAudioStreamSourceVoice->FlushSourceBuffers();
				OutputAudioStreamSourceVoice->Stop(0);
			}

			if (AudioStreamInfo.StreamState == EAudioOutputStreamState::Running)
			{
				StopGeneratingAudio();
			}

			check(AudioStreamInfo.StreamState == EAudioOutputStreamState::Stopped);
		}

		return true;
	}

	void FMixerPlatformXAudio2::SubmitBuffer(const TArray<float>& Buffer)
	{
		// Create a new xaudio2 buffer submission
		XAUDIO2_BUFFER XAudio2Buffer = { 0 };
		XAudio2Buffer.AudioBytes = AudioStreamInfo.DeviceInfo.NumSamples * sizeof(float);
		XAudio2Buffer.pAudioData = (const BYTE*)Buffer.GetData();
		XAudio2Buffer.pContext = this;

		// Submit buffer to the output streaming voice
		OutputAudioStreamSourceVoice->SubmitSourceBuffer(&XAudio2Buffer);
	}

	FName FMixerPlatformXAudio2::GetRuntimeFormat(USoundWave* InSoundWave)
	{
		if (InSoundWave->IsStreaming())
		{
			return FName(TEXT("OPUS"));
		}

		static FName NAME_OGG(TEXT("OGG"));
		return NAME_OGG;
	}

	bool FMixerPlatformXAudio2::HasCompressedAudioInfoClass(USoundWave* InSoundWave)
	{
#if WITH_OGGVORBIS
		return true;
#else
		return false;
#endif
	}

	ICompressedAudioInfo* FMixerPlatformXAudio2::CreateCompressedAudioInfo(USoundWave* InSoundWave)
	{
		check(InSoundWave);

		if (InSoundWave->IsStreaming())
		{
			return new FOpusAudioInfo();
		}

		ICompressedAudioInfo* CompressedInfo = new FVorbisAudioInfo();
		if (!CompressedInfo)
		{
			UE_LOG(LogAudioMixer, Error, TEXT("Failed to create new FVorbisAudioInfo for SoundWave %s: out of memory."), *InSoundWave->GetName());
			return nullptr;
		}
		return CompressedInfo;
	}

}