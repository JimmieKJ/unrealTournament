// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "ProfilerPrivatePCH.h"

/*-----------------------------------------------------------------------------
	FFPSAnalyzer
-----------------------------------------------------------------------------*/

void FFPSAnalyzer::Reset()
{
	Samples.Reset();
	Histogram.Reset();
	Histogram.AddZeroed((MaxVal-MinVal)/Interval+1);
	MinFPS = 9999;
	MaxFPS = 0;
	AveFPS = 0;
	FPS30 = 0;
	FPS25 = 0;
	FPS20 = 0;
}

void FFPSAnalyzer::AddSample( float FPSSample )
{
	Samples.Add(FPSSample);

	float DeltaSeconds = 1.0f / FPSSample;
	int32 Index = FPlatformMath::FloorToInt(FPSSample / Interval);
	Index = Index >= Histogram.Num() ? Histogram.Num()-1 : Index;
	Histogram[Index].Count++;
	Histogram[Index].CummulativeTime += DeltaSeconds;
	if (FPSSample >= 30)
	{
		FPS30++;
	}
	if (FPSSample >= 25)
	{
		FPS25++;
	}
	if (FPSSample >= 20)
	{
		FPS20++;
	}

	if (FPSSample > MaxFPS)
	{
		MaxFPS = FPSSample;
	}
	if (FPSSample < MinFPS)
	{
		MinFPS = FPSSample;
	}
	AveFPS = (FPSSample + (float)(Samples.Num()-1) * AveFPS) / (float)Samples.Num();
}

int32 FFPSAnalyzer::GetTotalCount()
{
	return Samples.Num();
}

int32 FFPSAnalyzer::GetCount(float InMinVal, float InMaxVal)
{
	int32 Index = FPlatformMath::FloorToInt(InMinVal / Interval);
	Index = Index >= Histogram.Num() ? Histogram.Num()-1 : Index;
	return Histogram[Index].Count;
}