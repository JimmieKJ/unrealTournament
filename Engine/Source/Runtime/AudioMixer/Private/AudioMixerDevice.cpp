// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "AudioMixerDevice.h"
#include "AudioMixerSource.h"
#include "AudioMixerSubmix.h"
#include "AudioMixerSourceVoice.h"
#include "UObject/UObjectHash.h"
#include "DSP/Noise.h"
#include "DSP/SinOsc.h"
#include "UObject/UObjectIterator.h"

namespace Audio
{
	FMixerDevice::FMixerDevice(IAudioMixerPlatformInterface* InAudioMixerPlatform)
		: AudioMixerPlatform(InAudioMixerPlatform)
		, NumSpatialChannels(0)
		, OmniPanFactor(0.0f)
		, AudioClockDelta(0.0)
		, AudioClock(0.0)
		, MasterSubmix(nullptr)
		, SourceManager(this)
		, GameOrAudioThreadId(INDEX_NONE)
		, AudioPlatformThreadId(INDEX_NONE)
		, bDebugOutputEnabled(false)
	{
		// This audio device is the audio mixer
		bAudioMixerModuleLoaded = true;
	}

	FMixerDevice::~FMixerDevice()
	{
		AUDIO_MIXER_CHECK_GAME_THREAD(this);

		if (AudioMixerPlatform != nullptr)
		{
			delete AudioMixerPlatform;
		}
	}

	void FMixerDevice::CheckGameThread()
	{
		if (GameOrAudioThreadId == INDEX_NONE)
		{
			GameOrAudioThreadId = FPlatformTLS::GetCurrentThreadId();
		}
		int32 CurrentThreadId = FPlatformTLS::GetCurrentThreadId();
		AUDIO_MIXER_CHECK(CurrentThreadId == GameOrAudioThreadId);
	}

	void FMixerDevice::ResetAudioPlatformThreadId()
	{
#if AUDIO_MIXER_ENABLE_DEBUG_MODE
		AudioPlatformThreadId = INDEX_NONE;
		CheckAudioPlatformThread();
#endif // AUDIO_MIXER_ENABLE_DEBUG_MODE
	}

	void FMixerDevice::CheckAudioPlatformThread()
	{
		if (AudioPlatformThreadId == INDEX_NONE)
		{
			AudioPlatformThreadId = FPlatformTLS::GetCurrentThreadId();
		}
		int32 CurrentThreadId = FPlatformTLS::GetCurrentThreadId();
		AUDIO_MIXER_CHECK(CurrentThreadId == AudioPlatformThreadId);
	}

	void FMixerDevice::GetAudioDeviceList(TArray<FString>& OutAudioDeviceNames) const
	{
		if (AudioMixerPlatform && AudioMixerPlatform->IsInitialized())
		{
			uint32 NumOutputDevices;
			if (AudioMixerPlatform->GetNumOutputDevices(NumOutputDevices))
			{
				for (uint32 i = 0; i < NumOutputDevices; ++i)
				{
					FAudioPlatformDeviceInfo DeviceInfo;
					if (AudioMixerPlatform->GetOutputDeviceInfo(i, DeviceInfo))
					{
						OutAudioDeviceNames.Add(DeviceInfo.Name);
					}
				}
			}
		}
	}

	bool FMixerDevice::InitializeHardware()
	{
		AUDIO_MIXER_CHECK_GAME_THREAD(this);

		if (AudioMixerPlatform && AudioMixerPlatform->InitializeHardware())
		{
			AUDIO_MIXER_CHECK(SampleRate != 0.0f);
			AUDIO_MIXER_CHECK(BufferLength != 0);

			OpenStreamParams.NumFrames = BufferLength;
			OpenStreamParams.OutputDeviceIndex = 0; // Default device (for now)
			OpenStreamParams.SampleRate = SampleRate;
			OpenStreamParams.AudioMixer = this;

			if (AudioMixerPlatform->OpenAudioStream(OpenStreamParams))
			{
				// Get the platform device info we're using
				PlatformInfo = AudioMixerPlatform->GetPlatformDeviceInfo();

				// Initialize some data that depends on speaker configuration, etc.
				InitializeChannelAzimuthMap(PlatformInfo.NumChannels);

				SourceManager.Init(MaxChannels);

				AudioClock = 0.0;
				AudioClockDelta = (double)OpenStreamParams.NumFrames / OpenStreamParams.SampleRate;

				// Get the master submix UObject

				check(MasterSubmix == nullptr);
				MasterSubmix = new FMixerSubmix(nullptr, this);

				// Start streaming audio
				return AudioMixerPlatform->StartAudioStream();
			}
		}
		return false;
	}

	void FMixerDevice::TeardownHardware()
	{
		if (AudioMixerPlatform)
		{
			AudioMixerPlatform->StopAudioStream();
			AudioMixerPlatform->CloseAudioStream();

			AudioMixerPlatform->TeardownHardware();

			delete MasterSubmix;
			MasterSubmix = nullptr;
		}
	}

	void FMixerDevice::UpdateHardware()
	{
		SourceManager.Update();
	}

	double FMixerDevice::GetAudioTime() const
	{
		return AudioClock;
	}

	FAudioEffectsManager* FMixerDevice::CreateEffectsManager()
	{
		return nullptr;
	}

	FSoundSource* FMixerDevice::CreateSoundSource()
	{
		return new FMixerSource(this);
	}

	FName FMixerDevice::GetRuntimeFormat(USoundWave* InSoundWave)
	{
		check(AudioMixerPlatform);
		return AudioMixerPlatform->GetRuntimeFormat(InSoundWave);
	}

	bool FMixerDevice::HasCompressedAudioInfoClass(USoundWave* InSoundWave)
	{
		check(InSoundWave);
		check(AudioMixerPlatform);
		return AudioMixerPlatform->HasCompressedAudioInfoClass(InSoundWave);
	}

	bool FMixerDevice::SupportsRealtimeDecompression() const
	{
		// TODO: Test and implement realt-time decompression on all other platforms
#if PLATFORM_WINDOWS
		return true;
#else
		return false;
#endif
		
	}

	class ICompressedAudioInfo* FMixerDevice::CreateCompressedAudioInfo(USoundWave* InSoundWave)
	{
		check(InSoundWave);
		check(AudioMixerPlatform);
		return AudioMixerPlatform->CreateCompressedAudioInfo(InSoundWave);
	}

	bool FMixerDevice::ValidateAPICall(const TCHAR* Function, uint32 ErrorCode)
	{
		return false;
	}

	bool FMixerDevice::Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar)
	{
		return false;
	}

	void FMixerDevice::CountBytes(FArchive& InArchive)
	{
		FAudioDevice::CountBytes(InArchive);
	}

	bool FMixerDevice::IsExernalBackgroundSoundActive()
	{
		return false;
	}

	void FMixerDevice::Precache(class USoundWave* InSoundWave, bool bSynchronous, bool bTrackMemory)
	{
		FAudioDevice::Precache(InSoundWave, bSynchronous, bTrackMemory);
	}

	void FMixerDevice::ResumeContext()
	{
	}

	void FMixerDevice::SetMaxChannels(int InMaxChannels)
	{
	}

	void FMixerDevice::StopAllSounds(bool InStop)
	{
	}

	void FMixerDevice::SuspendContext()
	{
	}

	void FMixerDevice::EnableDebugAudioOutput()
	{
		bDebugOutputEnabled = true;
	}

	bool FMixerDevice::OnProcessAudioStream(TArray<float>& Output)
	{
		// This function could be called in a task manager, which means the thread ID may change between calls.
		ResetAudioPlatformThreadId();

		// Compute the next block of audio in the source manager
		SourceManager.ComputeNextBlockOfSamples();

		if (MasterSubmix)
		{
			// Process the audio output from the master submix
			MasterSubmix->ProcessAudio(Output);
		}

		// Do any debug output performing
 		if (bDebugOutputEnabled)
		{
			SineOscTest(Output);
		}

		// Update the audio clock
		AudioClock += AudioClockDelta;

		return true;
	}

	void FMixerDevice::InitSoundSubmixes()
	{
		// Reset existing submixes if they exist
		Submixes.Reset();

		// Reset the maps of sound instances properties
		for (TObjectIterator<USoundSubmix> It; It; ++It)
		{
			USoundSubmix* SoundSubmix = *It;
			FMixerSubmix* MixerSubmix = new FMixerSubmix(SoundSubmix, this);
			Submixes.Add(SoundSubmix, MixerSubmix);
		}

	}

	void FMixerDevice::RegisterSoundSubmix(USoundSubmix* SoundSubmix)
	{
		if (SoundSubmix)
		{
			if (!Submixes.Contains(SoundSubmix))
			{
				FMixerSubmix* MixerSubmix = new FMixerSubmix(SoundSubmix, this);
				Submixes.Add(SoundSubmix, MixerSubmix);
			}
		}
	}

	void FMixerDevice::UnregisterSoundSubmix(USoundSubmix* SoundSubmix)
	{
		if (SoundSubmix)
		{
			Submixes.Remove(SoundSubmix);
		}
	}

	FMixerSubmix* FMixerDevice::GetSubmixInstance(USoundSubmix* SoundSubmix)
	{
		FMixerSubmix** Result = Submixes.Find(SoundSubmix);
		if (Result)
		{
			return *Result;
		}
		return nullptr;
	}

	FMixerSourceVoice* FMixerDevice::GetMixerSourceVoice(const FWaveInstance* InWaveInstance, ISourceBufferQueueListener* InBufferQueueListener, bool bUseHRTFSpatialization)
	{
		// Create a new mixer source voice using our source manager
		FMixerSourceVoice* NewMixerSourceVoice = new FMixerSourceVoice(this, &SourceManager);

		return NewMixerSourceVoice;
	}

	int32 FMixerDevice::GetNumSources() const
	{
		return Sources.Num();
	}

	int32 FMixerDevice::GetNumActiveSources() const
	{
		return SourceManager.GetNumActiveSources();
	}

	void FMixerDevice::Get3DChannelMap(const FWaveInstance* InWaveInstance, float EmitterAzimith, float NormalizedOmniRadius, TArray<float>& OutChannelMap)
	{
		// If we're center-channel only, then no need for spatial calculations, but need to build a channel map
		if (InWaveInstance->bCenterChannelOnly)
		{
			// If we only have stereo channels
			if (NumSpatialChannels == 2)
			{
				// Equal volume in left + right channel with equal power panning
				static const float Pan = 1.0f / FMath::Sqrt(2.0f);
				OutChannelMap.Add(Pan);
				OutChannelMap.Add(Pan);
			}
			else
			{
				for (EAudioMixerChannel::Type Channel : PlatformInfo.OutputChannelArray)
				{
					float Pan = (Channel == EAudioMixerChannel::FrontCenter) ? 1.0f : 0.0f;
					OutChannelMap.Add(Pan);
				}
			}

			return;
		}

		float Azimuth = EmitterAzimith;

		const FChannelPositionInfo* PrevChannelInfo = nullptr;
		const FChannelPositionInfo* NextChannelInfo = nullptr;

		for (int32 i = 0; i < CurrentChannelAzimuthPositions.Num(); ++i)
		{
			const FChannelPositionInfo& ChannelPositionInfo = CurrentChannelAzimuthPositions[i];

			if (Azimuth <= ChannelPositionInfo.Azimuth)
			{
				NextChannelInfo = &CurrentChannelAzimuthPositions[i];

				int32 PrevIndex = i - 1;
				if (PrevIndex < 0)
				{
					PrevIndex = CurrentChannelAzimuthPositions.Num() - 1;
				}

				PrevChannelInfo = &CurrentChannelAzimuthPositions[PrevIndex];
				break;
			}
		}

		// If we didn't find anything, that means our azimuth position is at the top of the mapping
		if (PrevChannelInfo == nullptr)
		{
			PrevChannelInfo = &CurrentChannelAzimuthPositions[CurrentChannelAzimuthPositions.Num() - 1];
			NextChannelInfo = &CurrentChannelAzimuthPositions[0];
			AUDIO_MIXER_CHECK(PrevChannelInfo != NextChannelInfo);
		}

		float NextChannelAzimuth = NextChannelInfo->Azimuth;
		float PrevChannelAzimuth = PrevChannelInfo->Azimuth;

		if (NextChannelAzimuth < PrevChannelAzimuth)
		{
			NextChannelAzimuth += 360.0f;
		}

		if (Azimuth < PrevChannelAzimuth)
		{
			Azimuth += 360.0f;
		}

		AUDIO_MIXER_CHECK(NextChannelAzimuth > PrevChannelAzimuth);
		AUDIO_MIXER_CHECK(Azimuth > PrevChannelAzimuth);
		float Fraction = (Azimuth - PrevChannelAzimuth) / (NextChannelAzimuth - PrevChannelAzimuth);
		AUDIO_MIXER_CHECK(Fraction >= 0.0f && Fraction <= 1.0f);

		// Compute the panning values using equal-power panning law
		float PrevChannelPan = FMath::Cos(Fraction * 0.5f * PI);
		float NextChannelPan = FMath::Sin(Fraction * 0.5f * PI);

		float NormalizedOmniRadSquared = NormalizedOmniRadius * NormalizedOmniRadius;
		float OmniAmount = 0.0f;

		if (NormalizedOmniRadSquared > 0.0f)
		{
			OmniAmount = 1.0f - FMath::Max(1.0f / NormalizedOmniRadSquared, 1.0f);
		}

		// OmniPan is the amount of pan to use if fully omni-directional
		AUDIO_MIXER_CHECK(NumSpatialChannels > 0);

		// Build the output channel map based on the current platform device output channel array 

		float DefaultEffectivePan = !OmniAmount ? 0.0f : FMath::Lerp(0.0f, OmniPanFactor, OmniAmount);

		for (EAudioMixerChannel::Type Channel : PlatformInfo.OutputChannelArray)
		{
			float EffectivePan = DefaultEffectivePan;

			// Check for manual channel mapping parameters (LFE and Front Center)
			if (Channel == EAudioMixerChannel::LowFrequency)
			{
				EffectivePan = InWaveInstance->LFEBleed;
			}
			else if (Channel == EAudioMixerChannel::FrontCenter)
			{
				EffectivePan = InWaveInstance->VoiceCenterChannelVolume;
			}
			else if (Channel == PrevChannelInfo->Channel)
			{
				EffectivePan = !OmniAmount ? PrevChannelPan : FMath::Lerp(PrevChannelPan, OmniPanFactor, OmniAmount);
			}
			else if (Channel == NextChannelInfo->Channel)
			{
				EffectivePan = !OmniAmount ? NextChannelPan : FMath::Lerp(NextChannelPan, OmniPanFactor, OmniAmount);
			}

			AUDIO_MIXER_CHECK(EffectivePan >= 0.0f && EffectivePan <= 1.0f);
			OutChannelMap.Add(EffectivePan);

		}
	}

	void FMixerDevice::SetChannelAzimuth(EAudioMixerChannel::Type ChannelType, int32 Azimuth)
	{
		if (ChannelType >= EAudioMixerChannel::TopCenter)
		{
			UE_LOG(LogAudioMixer, Warning, TEXT("Unsupported mixer channel type: %s"), EAudioMixerChannel::ToString(ChannelType));
			return;
		}

		if (Azimuth < 0 || Azimuth >= 360)
		{
			UE_LOG(LogAudioMixer, Warning, TEXT("Supplied azimuth is out of range: %d [0, 360)"), Azimuth);
			return;
		}

		DefaultChannelAzimuthPosition[ChannelType].Azimuth = Azimuth;
	}



	int32 FMixerDevice::GetAzimuthForChannelType(EAudioMixerChannel::Type ChannelType)
	{
		if (ChannelType >= EAudioMixerChannel::TopCenter)
		{
			UE_LOG(LogAudioMixer, Warning, TEXT("Unsupported mixer channel type: %s"), EAudioMixerChannel::ToString(ChannelType));
			return 0;
		}

		return DefaultChannelAzimuthPosition[ChannelType].Azimuth;
	}

	int32 FMixerDevice::GetDeviceSampleRate() const
	{
		return PlatformInfo.SampleRate;
	}

	int32 FMixerDevice::GetDeviceOutputChannels() const
	{
		return PlatformInfo.NumChannels;
	}

	FMixerSourceManager* FMixerDevice::GetSourceManager()
	{
		return &SourceManager;
	}

	void FMixerDevice::WhiteNoiseTest(TArray<float>& Output)
	{
		int32 NumFrames = PlatformInfo.NumFrames;
		int32 NumChannels = PlatformInfo.NumChannels;

		static FWhiteNoise WhiteNoise(0.2f);

		for (int32 FrameIndex = 0; FrameIndex < NumFrames; ++FrameIndex)
		{
			for (int32 ChannelIndex = 0; ChannelIndex < NumChannels; ++ChannelIndex)
			{
				int32 Index = FrameIndex * NumChannels + ChannelIndex;
				Output[Index] += WhiteNoise();
			}
		}
	}

	void FMixerDevice::SineOscTest(TArray<float>& Output)
	{
		int32 NumFrames = PlatformInfo.NumFrames;
		int32 NumChannels = PlatformInfo.NumChannels;

		check(NumChannels > 0);

		static FSineOsc SineOscLeft(PlatformInfo.SampleRate, 440.0f, 0.2f);
		static FSineOsc SineOscRight(PlatformInfo.SampleRate, 220.0f, 0.2f);

		for (int32 FrameIndex = 0; FrameIndex < NumFrames; ++FrameIndex)
		{
			int32 Index = FrameIndex * NumChannels;

			Output[Index] += SineOscLeft();

			if (NumChannels > 1)
			{
				Output[Index + 1] += SineOscRight();
			}
		}
	}

}