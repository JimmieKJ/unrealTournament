// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AudioMixer.h"
#include "Sound/SoundSubmix.h"

class USoundEffectSubmix;

namespace Audio
{
	class IAudioMixerEffect;
	class FMixerSourceVoice;
	class FMixerDevice;

	class FMixerSubmix
	{
	public:
		FMixerSubmix(USoundSubmix* InSoundSubmix, FMixerDevice* InMixerDevice);
		~FMixerSubmix();

		// Queries if this submix is the master submix.
		bool IsMasterSubmix() const;

		// Returns the number of source voices currently a part of this submix.
		int32 GetNumSourceVoices() const;

		// Returns the number of wet effects in this submix.
		int32 GetNumEffects() const;

		// Adds the given source voice to the submix.
		void AddSourceVoice(FMixerSourceVoice* InSourceVoice);

		/** Removes the given source voice from the submix. */
		void RemoveSourceVoice(FMixerSourceVoice* InSourceVoice);

		// Function which processes audio.
		void ProcessAudio(TArray<float>& OutAudio);

		// Sets the wet-level to use when outputting the submix's audio to it's parent submixes (does not apply for master submix).
		void SetOutputWetLevel(const float InWetLevel);

		// Returns the submix's wet level, used by parent submixes to determine where to route output audio of children.
		float GetOutputWetLevel() const;

		// Returns the device sample rate this submix is rendering to
		int32 GetSampleRate() const;

		// Returns the output channels this submix is rendering to
		int32 GetNumOutputChannels() const;

		// Updates the submix from the main thread.
		void Update();

	private:

		// UObject static data with graph information.
		USoundSubmix* SoundSubmix;

		// The effect chain of this submix, based on the sound submix preset chain
		TArray<USoundEffectSubmix*> EffectSubmixChain;

		// Owning mixer device. 
		FMixerDevice* MixerDevice;

		// Parent submix. 
		FMixerSubmix* Parent;

		// The source voices to be mixed by this submix. 
		TArray<FMixerSourceVoice*> MixerSourceVoices;

		TArray<float> DryBuffer;
		TArray<float> WetBuffer;
		TArray<float> ScratchBuffer;

		// The output wet level of this submix. Used by parent submix to determine how to route the output of this submix. 
		float WetLevel;
	};



}
