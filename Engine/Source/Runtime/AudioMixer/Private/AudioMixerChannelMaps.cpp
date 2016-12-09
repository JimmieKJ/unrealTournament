// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "AudioMixer.h"
#include "AudioMixerDevice.h"
#include "Misc/ConfigCacheIni.h"

namespace Audio
{
	// Tables based on Ac-3 down-mixing
	// Rows: output speaker configuration
	// Cols: input source channels

	static float ToMonoMatrix[8 * 1] =
	{
		// FrontLeft	FrontRight	Center		LowFrequency	SideLeft	SideRight	BackLeft	BackRight  
		0.707f,			0.707f,		1.0f,		0.0f,			0.5f,		0.5f,		0.5f,		0.5f,		// FrontLeft
	};

	static float ToStereoMatrix[8 * 2] =
	{
		// FrontLeft	FrontRight	Center		LowFrequency	SideLeft	SideRight	BackLeft	BackRight  
		1.0f,			0.0f,		0.707f,		0.0f,			0.707f,		0.0f,		0.707f,		0.0f,		// FrontLeft
		0.0f,			1.0f,		0.707f,		0.0f,			0.0f,		0.707f,		0.0f,		0.707f,		// FrontRight
	};

	static float ToQuadMatrix[8 * 4] =
	{
		// FrontLeft	FrontRight	Center		LowFrequency	SideLeft	SideRight	BackLeft	BackRight	
		1.0f,			0.0f,		0.707f,		0.0f,			0.0f,		0.0f,		0.0f,		0.0f,		// FrontLeft
		0.0f,			1.0f,		0.707f,		0.0f,			0.0f,		0.0f,		0.0f,		0.0f,		// FrontRight
		0.0f,			0.0f,		0.0f,		0.0f,			1.0f,		0.0f,		1.0f,		0.0f,		// SideLeft
		0.0f,			0.0f,		0.0f,		0.0f,			0.0f,		1.0f,		0.0f,		1.0f,		// SideRight
	};

	static float To5Point1Matrix[6 * 8] =
	{
		// FrontLeft	FrontRight	Center		LowFrequency	SideLeft	SideRight	BackLeft	BackRight	
		1.0f,			0.0f,		0.0f,		0.0f,			0.0f,		0.0f,		0.0f,		0.0f,		// FrontLeft
		0.0f,			1.0f,		0.0f,		0.0f,			0.0f,		0.0f,		0.0f,		0.0f,		// FrontRight
		0.0f,			0.0f,		1.0f,		0.0f,			0.0f,		0.0f,		0.0f,		0.0f,		// Center
		0.0f,			0.0f,		0.0f,		1.0f,			0.0f,		0.0f,		0.0f,		0.0f,		// LowFrequency
		0.0f,			0.0f,		0.0f,		0.0f,			1.0f,		0.0f,		1.0f,		0.0f,		// SideLeft
		0.0f,			0.0f,		0.0f,		0.0f,			0.0f,		1.0f,		0.0f,		1.0f,		// SideRight
	};

	static float ToHexMatrix[7 * 8] =
	{
		// FrontLeft	FrontRight	Center		LowFrequency	SideLeft	SideRight	BackLeft	BackRight	
		1.0f,			0.0f,		0.707f,		0.0f,			0.0f,		0.0f,		0.0f,		0.0f,		// FrontLeft
		0.0f,			1.0f,		0.707f,		0.0f,			0.0f,		0.0f,		0.0f,		0.0f,		// FrontRight
		0.0f,			0.0f,		0.0f,		0.0f,			1.0f,		0.0f,		0.0f,		0.0f,		// SideLeft
		0.0f,			0.0f,		0.0f,		0.0f,			0.0f,		1.0f,		0.0f,		0.0f,		// SideRight
		0.0f,			0.0f,		0.0f,		0.0f,			0.0f,		0.0f,		1.0f,		0.0f,		// BackLeft
		0.0f,			0.0f,		0.0f,		0.0f,			0.0f,		0.0f,		0.0f,		1.0f,		// BackRight
	};

	static float To7Point1Matrix[8 * 8] =
	{
		// FrontLeft	FrontRight	Center		LowFrequency	SideLeft	SideRight	BackLeft	BackRight
		1.0f,			0.0f,		0.0f,		0.0f,			0.0f,		0.0f,		0.0f,		0.0f,		// FrontLeft
		0.0f,			1.0f,		0.0f,		0.0f,			0.0f,		0.0f,		0.0f,		0.0f,		// FrontRight
		0.0f,			0.0f,		1.0f,		0.0f,			0.0f,		0.0f,		0.0f,		0.0f,		// FrontCenter
		0.0f,			0.0f,		0.0f,		1.0f,			0.0f,		0.0f,		0.0f,		0.0f,		// LowFrequency
		0.0f,			0.0f,		0.0f,		0.0f,			1.0f,		0.0f,		0.0f,		0.0f,		// SideLeft
		0.0f,			0.0f,		0.0f,		0.0f,			0.0f,		1.0f,		0.0f,		0.0f,		// SideRight
		0.0f,			0.0f,		0.0f,		0.0f,			0.0f,		0.0f,		1.0f,		0.0f,		// BackLeft
		0.0f,			0.0f,		0.0f,		0.0f,			0.0f,		0.0f,		0.0f,		1.0f,		// BackRight
	};

	static float* OutputChannelMaps[7] =
	{
		ToMonoMatrix,
		ToStereoMatrix,
		nullptr,
		ToQuadMatrix,
		To5Point1Matrix,
		ToHexMatrix,
		To7Point1Matrix
	};

	void FMixerDevice::Get2DChannelMap(const int32 NumSourceChannels, TArray<float>& OutChannelMap)
	{
		if (NumSourceChannels > 8 || PlatformInfo.NumChannels == 3)
		{
			// Return a zero'd channel map buffer in the case of an unsupported channel configuration
			OutChannelMap.AddZeroed(NumSourceChannels * PlatformInfo.NumChannels);
			UE_LOG(LogAudioMixer, Warning, TEXT("Unsupported source channel count or output channels %d"), NumSourceChannels);
			return;
		}

		// Check the cache.
		const int32 CacheID = NumSourceChannels + 10 * PlatformInfo.NumChannels;
		TArray<float>* CachedChannelMap = ChannelMapCache.Find(CacheID);
		if (!CachedChannelMap)
		{
			float* Matrix = OutputChannelMaps[PlatformInfo.NumChannels - 1];
			check(Matrix != nullptr);

			int32 ChannelMapIndex = 0;
			for (int32 SourceChannel = 0; SourceChannel < NumSourceChannels; ++SourceChannel)
			{
				for (int32 OutputChannel = 0; OutputChannel < PlatformInfo.NumChannels; ++OutputChannel)
				{
					OutChannelMap.Add(Matrix[ChannelMapIndex++]);
				}
			}
			ChannelMapCache.Add(CacheID, OutChannelMap);
		}
		else
		{
			OutChannelMap = *CachedChannelMap;
		}
	}

	void FMixerDevice::InitializeChannelAzimuthMap(const int32 NumChannels)
	{
		// Now setup the hard-coded values
		if (NumChannels == 2)
		{
			DefaultChannelAzimuthPosition[EAudioMixerChannel::FrontLeft] = { EAudioMixerChannel::FrontLeft, 270 };
			DefaultChannelAzimuthPosition[EAudioMixerChannel::FrontRight] = { EAudioMixerChannel::FrontRight, 90 };
		}
		else
		{
			DefaultChannelAzimuthPosition[EAudioMixerChannel::FrontLeft] = { EAudioMixerChannel::FrontLeft, 330 };
			DefaultChannelAzimuthPosition[EAudioMixerChannel::FrontRight] = { EAudioMixerChannel::FrontRight, 30 };
		}

		// Ignore front cent and low-frequency for azimuth computations. Both of these channels are manual opt-ins from data and not directly used in 3d audio calculations
		DefaultChannelAzimuthPosition[EAudioMixerChannel::FrontCenter] = { EAudioMixerChannel::FrontCenter, -1 };
		DefaultChannelAzimuthPosition[EAudioMixerChannel::LowFrequency] = { EAudioMixerChannel::LowFrequency, -1 };

		DefaultChannelAzimuthPosition[EAudioMixerChannel::BackLeft] = { EAudioMixerChannel::BackLeft, 210 };
		DefaultChannelAzimuthPosition[EAudioMixerChannel::BackRight] = { EAudioMixerChannel::BackRight, 150 };
		DefaultChannelAzimuthPosition[EAudioMixerChannel::FrontLeftOfCenter] = { EAudioMixerChannel::FrontLeftOfCenter, 15 };
		DefaultChannelAzimuthPosition[EAudioMixerChannel::FrontRightOfCenter] = { EAudioMixerChannel::FrontRightOfCenter, 345 };
		DefaultChannelAzimuthPosition[EAudioMixerChannel::BackCenter] = { EAudioMixerChannel::BackCenter, 180 };
		DefaultChannelAzimuthPosition[EAudioMixerChannel::SideLeft] = { EAudioMixerChannel::SideLeft, 270 };
		DefaultChannelAzimuthPosition[EAudioMixerChannel::SideRight] = { EAudioMixerChannel::SideRight, 90 };

		// Check any engine ini overrides for these default positions
		if (NumChannels != 2)
		{
			int32 AzimuthPositionOverride = 0;
			for (int32 ChannelOverrideIndex = 0; ChannelOverrideIndex < EAudioMixerChannel::MaxSupportedChannel; ++ChannelOverrideIndex)
			{
				EAudioMixerChannel::Type MixerChannelType = EAudioMixerChannel::Type(ChannelOverrideIndex);
				const TCHAR* ChannelName = EAudioMixerChannel::ToString(MixerChannelType);
				if (GConfig->GetInt(TEXT("AudioChannelAzimuthMap"), ChannelName, AzimuthPositionOverride, GEngineIni))
				{
					if (AzimuthPositionOverride >= 0 && AzimuthPositionOverride < 360)
					{
						// Make sure no channels have this azimuth angle first, otherwise we'll get some bad math later
						bool bIsUnique = true;
						for (int32 ExistingChannelIndex = 0; ExistingChannelIndex < EAudioMixerChannel::MaxSupportedChannel; ++ExistingChannelIndex)
						{
							if (DefaultChannelAzimuthPosition[ExistingChannelIndex].Azimuth == AzimuthPositionOverride)
							{
								bIsUnique = false;

								// If the override is setting the same value as our default, don't print a warning
								if (ExistingChannelIndex != ChannelOverrideIndex)
								{
									const TCHAR* ExistingChannelName = EAudioMixerChannel::ToString(EAudioMixerChannel::Type(ExistingChannelIndex));
									UE_LOG(LogAudioMixer, Warning, TEXT("Azimuth value '%d' for audio mixer channel '%s' is already used by '%s'. Azimuth values must be unique."),
										AzimuthPositionOverride, ChannelName, ExistingChannelName);
								}
								break;
							}
						}

						if (bIsUnique)
						{
							DefaultChannelAzimuthPosition[MixerChannelType].Azimuth = AzimuthPositionOverride;
						}
					}
					else
					{
						UE_LOG(LogAudioMixer, Warning, TEXT("Azimuth value, %d, for audio mixer channel %s out of range. Must be [0, 360)."), AzimuthPositionOverride, ChannelName);
					}
				}
			}
		}

		// Build a map of azimuth positions of only the current audio device's output channels
		CurrentChannelAzimuthPositions.Reset();
		for (EAudioMixerChannel::Type Channel : PlatformInfo.OutputChannelArray)
		{
			// Only track non-LFE and non-Center channel azimuths for use with 3d channel mappings
			if (Channel != EAudioMixerChannel::LowFrequency && Channel != EAudioMixerChannel::FrontCenter)
			{
				++NumSpatialChannels;
				CurrentChannelAzimuthPositions.Add(DefaultChannelAzimuthPosition[Channel]);
			}
		}

		check(NumSpatialChannels > 0);
		OmniPanFactor = 1.0f / FMath::Sqrt(NumSpatialChannels);

		// Sort the current mapping by azimuth
		struct FCompareByAzimuth
		{
			FORCEINLINE bool operator()(const FChannelPositionInfo& A, const FChannelPositionInfo& B) const
			{
				return A.Azimuth < B.Azimuth;
			}
		};

		CurrentChannelAzimuthPositions.Sort(FCompareByAzimuth());
	}
}