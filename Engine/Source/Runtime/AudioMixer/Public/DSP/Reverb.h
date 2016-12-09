// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DSP/Delay.h"
#include "DSP/OnePole.h"

namespace Audio
{
	/** 
	* FreeVerb/Schroeder verb implementation
	* https://ccrma.stanford.edu/~jos/pasp/Freeverb.html
	*/
	class FFreeVerb
	{
	public:
		FFreeVerb(const float InSampleRate);

		void SetWetLevel(const float InWetLevel);
		void SetRoomSize(const float InRoomSize);
		float GetRoomSize() const;
		void SetDamping(const float InDamping);
		float GetDamping() const;
		void SetStereoWidth(float InWidth);
		float GetStereoWidth() const;
		void SetFreeze(const bool bInIsFrozen);
		bool IsFrozen() const;
		void Clear();

		void operator()(const TArray<float>& InAudio, TArray<float>& OutAudio);

	protected:
		void UpdateParameters();

		static const int32 NumCombFilters = 8;
		static const int32 NumAllPassFilters = 4;
		static const int32 StereoSpread = 23;

		static const float FixedGain;
		static const float ScaleWet;
		static const float ScaleDry;
		static const float ScaleDamp;
		static const float ScaleRoom;
		static const float OffsetRoom;

		TArray<float> CombFilterDelayLengths;
		TArray<float> AllpassFilterDelayLengths;

		float AllPassCoefficientG;
		float Gain;
		float RoomSizeSetting;
		float RoomSize;
		float DampSetting;
		float Damp;
		float WetLevel;
		float Wet1; 
		float Wet2;
		float Dry;
		float Width;
		bool bIsFrozen;

		TArray<FDelay> CombDelayLeft;
		TArray<FDelay> CombDelayRight;
		TArray<FOnePoleLPF> CombLowPassLeft;
		TArray<FOnePoleLPF> CombLowPassRight;

		TArray<FDelay> AllPassDelayLeft;
		TArray<FDelay> AllPassDelayRight;
	};


}
