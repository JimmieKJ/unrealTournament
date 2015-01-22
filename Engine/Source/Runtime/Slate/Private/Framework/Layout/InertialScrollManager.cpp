// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SlatePrivatePCH.h"


FInertialScrollManager::FInertialScrollManager(float InScrollDecceleration, double InSampleTimeout)
	: ScrollVelocity(0.f)
	, ScrollDecceleration(InScrollDecceleration)
	, SampleTimeout(InSampleTimeout)
{
}

void FInertialScrollManager::AddScrollSample(float Delta, double CurrentTime)
{
	new( this->ScrollSamples ) FScrollSample(Delta, CurrentTime);

	float Total = 0;
	double OldestTime = 0;
	for ( int32 VelIdx = this->ScrollSamples.Num() - 1; VelIdx >= 0; --VelIdx )
	{
		const double SampleTime = this->ScrollSamples[VelIdx].Time;
		const float SampleDelta = this->ScrollSamples[VelIdx].Delta;
		if ( CurrentTime - SampleTime > SampleTimeout )
		{
			this->ScrollSamples.RemoveAt(VelIdx);
		}
		else
		{
			if ( SampleTime < OldestTime || OldestTime == 0 )
			{
				OldestTime = SampleTime;
			}

			Total += SampleDelta;
		}
	}

	// Set the current velocity to the average of the previous recent samples
	const double Duration = (OldestTime > 0) ? (CurrentTime - OldestTime) : 0;
	if ( Duration > 0 )
	{
		this->ScrollVelocity = Total / Duration;
	}
	else
	{
		this->ScrollVelocity = 0;
	}
}	

void FInertialScrollManager::UpdateScrollVelocity(const float InDeltaTime)
{
	if ( this->ScrollVelocity > 0 )
	{
		this->ScrollVelocity -= ScrollDecceleration * InDeltaTime;
		this->ScrollVelocity = FMath::Max<float>(0.f, this->ScrollVelocity);
	}
	else if ( this->ScrollVelocity < 0 )
	{
		this->ScrollVelocity += ScrollDecceleration * InDeltaTime;
		this->ScrollVelocity = FMath::Min<float>(0.f, this->ScrollVelocity);
	}
}

void FInertialScrollManager::ClearScrollVelocity()
{
	this->ScrollVelocity = 0;
}