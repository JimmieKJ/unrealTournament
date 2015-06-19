// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealEd.h"
#include "Sound/SoundWave.h"
#include "CanvasItem.h"
#include "Audio.h"

USoundWaveThumbnailRenderer::USoundWaveThumbnailRenderer(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void USoundWaveThumbnailRenderer::Draw(UObject* Object, int32 X, int32 Y, uint32 Width, uint32 Height, FRenderTarget*, FCanvas* Canvas)
{
	static bool bDrawChannels = true;
	static bool bDrawAsCurve = false;

	USoundWave* SoundWave = Cast<USoundWave>(Object);
	if (SoundWave != nullptr && SoundWave->NumChannels > 0)
	{
		// check if there is any raw sound data
		if( SoundWave->RawData.GetBulkDataSize() > 0 )
		{
			FCanvasLineItem LineItem;
			LineItem.SetColor( FLinearColor::White );
			// Lock raw wave data.
			uint8* RawWaveData = ( uint8* )SoundWave->RawData.Lock( LOCK_READ_ONLY );
			int32 RawDataSize = SoundWave->RawData.GetBulkDataSize();
			FWaveModInfo WaveInfo;

			// parse the wave data
			if( WaveInfo.ReadWaveHeader( RawWaveData, RawDataSize, 0 ) )
			{
				const float SampleYScale = Height / (2.f * 32767 * (bDrawChannels ? SoundWave->NumChannels : 1));

				int16* SamplePtr = reinterpret_cast<int16*>(WaveInfo.SampleDataStart);

				uint32 SampleCount = 0;
				uint32 SampleCounts[10] = {0};

				if (SoundWave->NumChannels <= 2)
				{
					SampleCount = WaveInfo.SampleDataSize / (2 * SoundWave->NumChannels);
				}
				else
				{
					for (int32 ChannelIndex = 0; ChannelIndex < SoundWave->NumChannels; ++ChannelIndex)
					{
						SampleCounts[ChannelIndex] = SoundWave->ChannelSizes[ChannelIndex] / 2;
						SampleCount = FMath::Max(SampleCount, SampleCounts[ChannelIndex]);
					}
				}
				const uint32 SamplesPerX = (SampleCount / Width) + 1;
				float LastScaledSample[10] = {0.f};

				for (uint32 XOffset = 0; XOffset < Width-1; ++XOffset )
				{
					int64 SampleSum[10] = {0};
					if (SoundWave->NumChannels <= 2)
					{
						for (uint32 PerXSampleIndex = 0; PerXSampleIndex < SamplesPerX; ++PerXSampleIndex)
						{
							for (int32 ChannelIndex = 0; ChannelIndex < SoundWave->NumChannels; ++ChannelIndex)
							{
								const int16 SampleValue = (bDrawAsCurve ? *SamplePtr : FMath::Abs(*SamplePtr));
								SampleSum[ChannelIndex] += SampleValue;
								++SamplePtr;
							}
						}
					}
					else
					{
						for (int32 ChannelIndex = 0; ChannelIndex < SoundWave->NumChannels; ++ChannelIndex)
						{
							if (SampleCounts[ChannelIndex] >= SamplesPerX)
							{
								for (uint32 PerXSampleIndex = 0; PerXSampleIndex < SamplesPerX; ++PerXSampleIndex)
								{
									int16 SampleValue =*(SamplePtr + (SamplesPerX * XOffset) + PerXSampleIndex + SoundWave->ChannelOffsets[ChannelIndex] / 2);
									if (!bDrawAsCurve)
									{
										SampleValue = FMath::Abs(SampleValue);
									}
									SampleSum[ChannelIndex] += SampleValue;
								}
								SampleCounts[ChannelIndex] -= SamplesPerX;
							}
						}
					}
					if (bDrawChannels)
					{
						for (int32 ChannelIndex = 0; ChannelIndex < SoundWave->NumChannels; ++ChannelIndex)
						{
							const float ScaledSample = static_cast<float>(SampleSum[ChannelIndex]) / SamplesPerX * SampleYScale;
							if (bDrawAsCurve)
							{
								if (XOffset > 0)
								{
									const float YCenter = Y + ((2 * ChannelIndex) + 1) * Height / (2.f * SoundWave->NumChannels);
									LineItem.Draw( Canvas,  FVector2D(X + XOffset - 1, YCenter + LastScaledSample[ChannelIndex]), FVector2D(X + XOffset, YCenter + ScaledSample ) );
								}
								LastScaledSample[ChannelIndex] = ScaledSample;
							}
							else if (ScaledSample > 0.001f)
							{
								const float YCenter = Y + ((2 * ChannelIndex) + 1) * Height / (2.f * SoundWave->NumChannels);
								LineItem.Draw( Canvas, FVector2D(X + XOffset, YCenter - ScaledSample), FVector2D(X + XOffset, YCenter + ScaledSample) );
							}
						}
					}
					else
					{
						if (bDrawAsCurve)
						{
							float ScaledSampleSum = 0.f;
							int32 ActiveChannelCount = 0;
							for (int32 ChannelIndex = 0; ChannelIndex < SoundWave->NumChannels; ++ChannelIndex)
							{
								const float ScaledSample = static_cast<float>(SampleSum[ChannelIndex]) / SamplesPerX * SampleYScale;
								if (FMath::Abs(ScaledSample) > 0.001f)
								{
									ScaledSampleSum += ScaledSample;
									++ActiveChannelCount;
								}
							}
							const float ScaledSample = (ActiveChannelCount > 0 ? ScaledSampleSum / ActiveChannelCount : 0.f);
							if (XOffset > 0)
							{
								const float YCenter = Y + 0.5f * Height;
								LineItem.Draw( Canvas, FVector2D(X + XOffset - 1, YCenter + LastScaledSample[0]), FVector2D(X + XOffset, YCenter + ScaledSample) );
							}
							LastScaledSample[0] = ScaledSample;
						}
						else
						{
							float MaxScaledSample = 0.f;
							for (int32 ChannelIndex = 0; ChannelIndex < SoundWave->NumChannels; ++ChannelIndex)
							{
								const float ScaledSample = static_cast<float>(SampleSum[ChannelIndex]) / SamplesPerX * SampleYScale;
								MaxScaledSample = FMath::Max(MaxScaledSample, ScaledSample);
							}
							if (MaxScaledSample > 0.001f)
							{
								const float YCenter = Y + 0.5f * Height;
								LineItem.Draw( Canvas, FVector2D(X + XOffset, YCenter - MaxScaledSample), FVector2D(X + XOffset, YCenter + MaxScaledSample) );
							}
						}
					}
				}
			}

			SoundWave->RawData.Unlock();
		}
	}
}
