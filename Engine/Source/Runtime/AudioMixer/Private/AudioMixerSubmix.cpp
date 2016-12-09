// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "AudioMixerSubmix.h"
#include "AudioMixerDevice.h"
#include "AudioMixerSourceVoice.h"
#include "Sound/SoundSubmix.h"
#include "Sound/SoundEffectSubmix.h"

namespace Audio
{

	FMixerSubmix::FMixerSubmix(USoundSubmix* InSoundSubmix, FMixerDevice* InMixerDevice)
		: SoundSubmix(nullptr)
		, MixerDevice(InMixerDevice)
		, Parent(nullptr)
		, WetLevel(0.5f)
	{
		if (InSoundSubmix != nullptr)
		{
			SoundSubmix = InSoundSubmix;

			// Loop through the submix's presets and make new instances of effects in the same order as the presets
			EffectSubmixChain.Reset();

			// If this is the master submix, we have some default effects we're going to apply (like master EQ, master compression, master verb, etc)
			if (IsMasterSubmix())
			{

			}


			for (USoundEffectSubmixPreset* EffectPreset : SoundSubmix->SubmixEffectChain)
			{
				// Create a new effect instance using the preset
				USoundEffectSubmix* SubmixEffect = EffectPreset->CreateNewEffect();

				// Now set the preset
				SubmixEffect->SetPreset(EffectPreset);

				// Add the effect to this submix's chain
				EffectSubmixChain.Add(SubmixEffect);
			}
		}
	}

	FMixerSubmix::~FMixerSubmix()
	{
	}

	bool FMixerSubmix::IsMasterSubmix() const
	{
		// If we were given a nullptr sound submix, then it is the master sound mix
		return SoundSubmix == nullptr;
	}

	int32 FMixerSubmix::GetNumSourceVoices() const
	{
		return MixerSourceVoices.Num();
	}

	int32 FMixerSubmix::GetNumEffects() const
	{
		return EffectSubmixChain.Num();
	}

	void FMixerSubmix::AddSourceVoice(FMixerSourceVoice* InSourceVoice)
	{
		AUDIO_MIXER_CHECK_AUDIO_PLAT_THREAD(MixerDevice);
		MixerSourceVoices.Add(InSourceVoice);
	}

	void FMixerSubmix::RemoveSourceVoice(FMixerSourceVoice* InSourceVoice)
	{
		AUDIO_MIXER_CHECK_AUDIO_PLAT_THREAD(MixerDevice);
		int32 NumRemoved = MixerSourceVoices.Remove(InSourceVoice);
		AUDIO_MIXER_CHECK(NumRemoved == 1);
	}

	void FMixerSubmix::ProcessAudio(TArray<float>& OutAudioBuffer)
	{
		AUDIO_MIXER_CHECK_AUDIO_PLAT_THREAD(MixerDevice);

		// Create a zero'd scratch buffer to get the audio from this submix's children
		const int32 NumSamples = OutAudioBuffer.Num();

		WetBuffer.Reset(NumSamples);
		WetBuffer.AddZeroed(NumSamples);

		DryBuffer.Reset(NumSamples);
		DryBuffer.AddZeroed(NumSamples);

		if (SoundSubmix != nullptr)
		{
			// First loop this submix's child submixes mixing in their output into this submix's dry/wet buffers.
			for (USoundSubmix* ChildSubmix : SoundSubmix->ChildSubmixes)
			{
				FMixerSubmix* ChildMixerSubmix = MixerDevice->GetSubmixInstance(ChildSubmix);
				AUDIO_MIXER_CHECK(ChildMixerSubmix != nullptr);

				ScratchBuffer.Reset(NumSamples);

				ChildMixerSubmix->ProcessAudio(ScratchBuffer);

				// Get this child submix's wet level
				float ChildWetLevel = ChildSubmix->OutputWetLevel;
				float ChildDryLevel = 1.0f - WetLevel;

				// Get a nice constant-power x-fade of the dry/wet levels
				ChildWetLevel = FMath::Sin(WetLevel);
				ChildDryLevel = FMath::Cos(WetLevel);

				// Mix the child's submix results into this submix's dry and wet levels
				for (int32 i = 0; i < NumSamples; ++i)
				{
					WetBuffer[i] += ChildDryLevel * ScratchBuffer[i];
					DryBuffer[i] += ChildDryLevel* ScratchBuffer[i];
				}
			}
		}

		// Loop through this submix's sound sources, removing any sound sources that have finished
		for (int32 i = MixerSourceVoices.Num() - 1; i >= 0; --i)
		{
			MixerSourceVoices[i]->MixOutputBuffers(DryBuffer, WetBuffer);
		}

		// Setup the input data buffer
		FSoundEffectSubmixInputData InputData;
		InputData.AudioBuffer = &WetBuffer;
		InputData.NumChannels = MixerDevice->GetNumDeviceChannels();
		InputData.AudioClock = MixerDevice->GetAudioTime();

		FSoundEffectSubmixOutputData OutputData;
		OutputData.AudioBuffer = &ScratchBuffer;

		for (USoundEffectSubmix* SubmixEffect : EffectSubmixChain)
		{
			SubmixEffect->ProcessAudio(InputData, OutputData);

			// Copy the output audio buffer to the old wet buffer
			WetBuffer = *OutputData.AudioBuffer;
		}

		// Mix the dry and wet buffers together for the final output
		for (int32 i = 0; i < NumSamples; ++i)
		{
			OutAudioBuffer[i] = (DryBuffer[i] + WetBuffer[i]);
		}
	}

	void FMixerSubmix::SetOutputWetLevel(const float InWetLevel)
	{
		WetLevel = InWetLevel;
	}

	float FMixerSubmix::GetOutputWetLevel() const
	{
		return WetLevel;
	}

	int32 FMixerSubmix::GetSampleRate() const
	{
		return MixerDevice->GetDeviceSampleRate();
	}

	int32 FMixerSubmix::GetNumOutputChannels() const
	{
		return MixerDevice->GetNumDeviceChannels();
	}

	void FMixerSubmix::Update()
	{
	}
}