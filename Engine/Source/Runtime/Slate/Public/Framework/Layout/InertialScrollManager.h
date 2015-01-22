// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

/**
 * A helper class to calculate inertial scrolling
 */
class SLATE_API FInertialScrollManager
{
public:
	/**
	  * Constructor
	  * @param ScrollDecceleration	The acceleration against the velocity causing it to decay.
	  * @param SampleTimeout		Samples older than this amount of time will be discarded.
	  */
	FInertialScrollManager(float ScrollDecceleration = 2048.f, double SampleTimeout = 0.1f);

	/** Adds a scroll velocity sample to help calculate a smooth velocity */
	void AddScrollSample(float Delta, double CurrentTime);

	/** Updates the current scroll velocity. Call every frame. */
	void UpdateScrollVelocity(const float InDeltaTime);

	/** Instantly end the inertial scroll */
	void ClearScrollVelocity();

	/** Gets the calculated velocity of the scroll. */
	float GetScrollVelocity() const { return ScrollVelocity; }

private:
	struct FScrollSample
	{
		double Time;
		float Delta;

		FScrollSample(float InDelta, double InTime)
			: Time(InTime)
			, Delta(InDelta)
		{}
	};

	/** Used to calculate the appropriate scroll velocity over the last few frames while inertial scrolling */
	TArray<FScrollSample> ScrollSamples;

	/** The current velocity of the scroll */
	float ScrollVelocity;

	/** The acceleration against the velocity causing it to decay. */
	float ScrollDecceleration;

	/** Samples older than this amount of time will be discarded. */
	double SampleTimeout;
};
