// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


/** Various ways to interpolate TAlphaBlend. */
enum EAlphaBlendType
{
    ABT_Linear              =0,
    ABT_Cubic               =1,
    ABT_Sinusoidal          =2,
    ABT_EaseInOutExponent2  =3,
    ABT_EaseInOutExponent3  =4,
    ABT_EaseInOutExponent4  =5,
    ABT_EaseInOutExponent5  =6,
    ABT_MAX                 =7,
};


/** Turn a linear interpolated alpha into the corresponding AlphaBlendType */
FORCEINLINE float AlphaToBlendType(float InAlpha, uint8 BlendType)
{
	switch( BlendType )
	{
	case ABT_Sinusoidal         : return FMath::Clamp<float>((FMath::Sin(InAlpha * PI - HALF_PI) + 1.f) / 2.f, 0.f, 1.f);
	case ABT_Cubic              : return FMath::Clamp<float>(FMath::CubicInterp<float>(0.f, 0.f, 1.f, 0.f, InAlpha), 0.f, 1.f);
	case ABT_EaseInOutExponent2 : return FMath::Clamp<float>(FMath::InterpEaseInOut<float>(0.f, 1.f, InAlpha, 2), 0.f, 1.f);
	case ABT_EaseInOutExponent3 : return FMath::Clamp<float>(FMath::InterpEaseInOut<float>(0.f, 1.f, InAlpha, 3), 0.f, 1.f);
	case ABT_EaseInOutExponent4 : return FMath::Clamp<float>(FMath::InterpEaseInOut<float>(0.f, 1.f, InAlpha, 4), 0.f, 1.f);
	case ABT_EaseInOutExponent5 : return FMath::Clamp<float>(FMath::InterpEaseInOut<float>(0.f, 1.f, InAlpha, 5), 0.f, 1.f);
	}

	return InAlpha;
}


/**
 * Alpha Blend Type
 */
struct 
DEPRECATED(4.9, "FTAlphaBlend is deprecated, please use FAlphaBlend instead")
FTAlphaBlend
{
	/** Internal Lerped value for Alpha */
	float	AlphaIn;
	/** Resulting Alpha value, between 0.f and 1.f */
	float	AlphaOut;
	/** Target to reach */
	float	AlphaTarget;
	/** Default blend time */
	float	BlendTime;
	/** Time left to reach target */
	float	BlendTimeToGo;
	/** Type of blending used (Linear, Cubic, etc.) */
	uint8	BlendType;

	// Constructor
	FTAlphaBlend();

	/**
	 * Constructor
	 *
	 * @param InAlphaIn Internal Lerped value for Alpha
	 * @param InAlphaOut Resulting Alpha value, between 0.f and 1.f
	 * @param InAlphaTarget Target to reach
	 * @param InBlendTime Default blend time
	 * @param InBlendTimeToGo Time left to reach target
	 * @param InBlendType Type of blending used (Linear, Cubic, etc.)
	 */
	 FTAlphaBlend(float InAlphaIn, float InAlphaOut, float InAlphaTarget, float InBlendTime, float InBlendTimeToGo, uint8 InBlendType);

	/**
	 * Serializes the Alpha Blend.
	 *
	 * @param Ar Reference to the serialization archive.
	 * @param AlphaBlend Reference to the alpha blend being serialized.
	 *
	 * @return Reference to the Archive after serialization.
	 */
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	friend FArchive& operator<<(FArchive& Ar, FTAlphaBlend& AlphaBlend)
	{
		return Ar << AlphaBlend.AlphaIn << AlphaBlend.AlphaOut << AlphaBlend.AlphaTarget << AlphaBlend.BlendTime << AlphaBlend.BlendTimeToGo << AlphaBlend.BlendType;
	}
PRAGMA_ENABLE_DEPRECATION_WARNINGS

	/** Update transition blend time */
	FORCEINLINE void SetBlendTime(float InBlendTime);

	/** Reset to zero */
	void Reset();

	/** Returns true if Target is > 0.f, or false otherwise */
	FORCEINLINE bool GetToggleStatus();

	/** Enable (1.f) or Disable (0.f) */
	FORCEINLINE void Toggle(bool bEnable);

	/** SetTarget, but check that we're actually setting a different target */
	FORCEINLINE void ConditionalSetTarget(float InAlphaTarget);

	/** Set Target for interpolation */
	void SetTarget(float InAlphaTarget);

	/** Update interpolation, has to be called once every frame */
	void Update(float InDeltaTime);
};

// We're disabling deprecation warnings here as we get a bunch because we're using
// methods and members from the deprecated struct, we're only really concerned with
// uses outside of this struct
PRAGMA_DISABLE_DEPRECATION_WARNINGS
FORCEINLINE FTAlphaBlend::FTAlphaBlend() {}


FORCEINLINE FTAlphaBlend::FTAlphaBlend(float InAlphaIn, float InAlphaOut, float InAlphaTarget, float InBlendTime, float InBlendTimeToGo, uint8 InBlendType): 
	AlphaIn(InAlphaIn), AlphaOut(InAlphaOut), AlphaTarget(InAlphaTarget), BlendTime(InBlendTime), BlendTimeToGo(InBlendTimeToGo), BlendType(InBlendType) {}


FORCEINLINE void FTAlphaBlend::SetBlendTime(float InBlendTime)
{
	BlendTime = FMath::Max(InBlendTime, 0.f);
}


FORCEINLINE void FTAlphaBlend::Reset()
{
	AlphaIn = 0.f;
	AlphaOut = 0.f;
	AlphaTarget = 0.f;
	BlendTimeToGo = 0.f;
}


FORCEINLINE bool FTAlphaBlend::GetToggleStatus()
{
	return (AlphaTarget > 0.f);
}


FORCEINLINE void FTAlphaBlend::Toggle(bool bEnable)
{
	ConditionalSetTarget(bEnable ? 1.f : 0.f);
}


FORCEINLINE void FTAlphaBlend::ConditionalSetTarget(float InAlphaTarget)
{
	if( AlphaTarget != InAlphaTarget )
	{
		SetTarget(InAlphaTarget);
	}
}


FORCEINLINE void FTAlphaBlend::SetTarget(float InAlphaTarget)
{
	// Clamp parameters to valid range
	AlphaTarget = FMath::Clamp<float>(InAlphaTarget, 0.f, 1.f);

	// if blend time is zero, transition now, don't wait to call update.
	if( BlendTime <= 0.f )
	{
		AlphaIn = AlphaTarget;
		AlphaOut = AlphaToBlendType(AlphaIn, BlendType);
		BlendTimeToGo = 0.f;
	}
	else
	{
		// Blend time is to go all the way, so scale that by how much we have to travel
		BlendTimeToGo = BlendTime * FMath::Abs(AlphaTarget - AlphaIn);
	}
}


FORCEINLINE void FTAlphaBlend::Update(float InDeltaTime)
{
	// Make sure passed in delta time is positive
	check(InDeltaTime >= 0.f);

	if( AlphaIn != AlphaTarget )
	{
		if( BlendTimeToGo >= InDeltaTime )
		{
			const float BlendDelta = AlphaTarget - AlphaIn; 
			AlphaIn += (BlendDelta / BlendTimeToGo) * InDeltaTime;
			BlendTimeToGo -= InDeltaTime;
		}
		else
		{
			BlendTimeToGo = 0.f; 
			AlphaIn = AlphaTarget;
		}

		// Convert Lerped alpha to corresponding blend type.
		AlphaOut = AlphaToBlendType(AlphaIn, BlendType);
	}
}
PRAGMA_ENABLE_DEPRECATION_WARNINGS
