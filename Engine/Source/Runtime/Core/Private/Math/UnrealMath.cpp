// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	UnMath.cpp: Unreal math routines
=============================================================================*/

#include "CorePrivatePCH.h"

DEFINE_LOG_CATEGORY(LogUnrealMath);

/**
* Math stats
*/

DECLARE_CYCLE_STAT( TEXT( "Convert Rotator to Quat" ), STAT_MathConvertRotatorToQuat, STATGROUP_Math );
DECLARE_CYCLE_STAT( TEXT( "Convert Quat to Rotator" ), STAT_MathConvertQuatToRotator, STATGROUP_Math );

/*-----------------------------------------------------------------------------
	Globals
-----------------------------------------------------------------------------*/

CORE_API const FVector FVector::ZeroVector(0.0f, 0.0f, 0.0f);
CORE_API const FVector FVector::UpVector(0.0f, 0.0f, 1.0f);
CORE_API const FVector FVector::ForwardVector(1.0f, 0.0f, 0.0f);
CORE_API const FVector FVector::RightVector(0.0f, 1.0f, 0.0f);
CORE_API const FVector2D FVector2D::ZeroVector(0.0f, 0.0f);
CORE_API const FVector2D FVector2D::UnitVector(1.0f, 1.0f);
CORE_API const FRotator FRotator::ZeroRotator(0.f,0.f,0.f);

CORE_API const VectorRegister VECTOR_INV_255 = DECLARE_VECTOR_REGISTER(1.f/255.f, 1.f/255.f, 1.f/255.f, 1.f/255.f);

CORE_API const uint32 FMath::BitFlag[32] =
{
	(1U << 0),	(1U << 1),	(1U << 2),	(1U << 3),
	(1U << 4),	(1U << 5),	(1U << 6),	(1U << 7),
	(1U << 8),	(1U << 9),	(1U << 10),	(1U << 11),
	(1U << 12),	(1U << 13),	(1U << 14),	(1U << 15),
	(1U << 16),	(1U << 17),	(1U << 18),	(1U << 19),
	(1U << 20),	(1U << 21),	(1U << 22),	(1U << 23),
	(1U << 24),	(1U << 25),	(1U << 26),	(1U << 27),
	(1U << 28),	(1U << 29),	(1U << 30),	(1U << 31),
};

CORE_API const FIntPoint FIntPoint::ZeroValue(0,0);
CORE_API const FIntPoint FIntPoint::NoneValue(INDEX_NONE,INDEX_NONE);
CORE_API const FIntVector FIntVector::ZeroValue(0,0,0);
CORE_API const FIntVector FIntVector::NoneValue(INDEX_NONE,INDEX_NONE,INDEX_NONE);

/** FVectors NetSerialize without quantization. Use the FVectors_NetQuantize etc (NetSerialization.h) instead. */
bool FVector::NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
{
	Ar << X;
	Ar << Y;
	Ar << Z;
	return true;
}

bool FVector2D::NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
{
	Ar << X;
	Ar << Y;
	return true;
}

bool FRotator::NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
{
	SerializeCompressedShort( Ar );
	bOutSuccess = true;
	return true;
}

void FRotator::SerializeCompressed( FArchive& Ar )
{
	uint8 BytePitch = FRotator::CompressAxisToByte(Pitch);
	uint8 ByteYaw = FRotator::CompressAxisToByte(Yaw);
	uint8 ByteRoll = FRotator::CompressAxisToByte(Roll);

	uint8 B = (BytePitch!=0);
	Ar.SerializeBits( &B, 1 );
	if( B )
	{
		Ar << BytePitch;
	}
	else
	{
		BytePitch = 0;
	}
	
	B = (ByteYaw!=0);
	Ar.SerializeBits( &B, 1 );
	if( B )
	{
		Ar << ByteYaw;
	}
	else
	{
		ByteYaw = 0;
	}
	
	B = (ByteRoll!=0);
	Ar.SerializeBits( &B, 1 );
	if( B )
	{
		Ar << ByteRoll;
	}
	else
	{
		ByteRoll = 0;
	}
	
	if( Ar.IsLoading() )
	{
		Pitch = FRotator::DecompressAxisFromByte(BytePitch);
		Yaw	= FRotator::DecompressAxisFromByte(ByteYaw);
		Roll = FRotator::DecompressAxisFromByte(ByteRoll);
	}
}

void FRotator::SerializeCompressedShort( FArchive& Ar )
{
	uint16 ShortPitch = FRotator::CompressAxisToShort(Pitch);
	uint16 ShortYaw = FRotator::CompressAxisToShort(Yaw);
	uint16 ShortRoll = FRotator::CompressAxisToShort(Roll);

	uint8 B = (ShortPitch!=0);
	Ar.SerializeBits( &B, 1 );
	if( B )
	{
		Ar << ShortPitch;
	}
	else
	{
		ShortPitch = 0;
	}

	B = (ShortYaw!=0);
	Ar.SerializeBits( &B, 1 );
	if( B )
	{
		Ar << ShortYaw;
	}
	else
	{
		ShortYaw = 0;
	}

	B = (ShortRoll!=0);
	Ar.SerializeBits( &B, 1 );
	if( B )
	{
		Ar << ShortRoll;
	}
	else
	{
		ShortRoll = 0;
	}

	if( Ar.IsLoading() )
	{
		Pitch = FRotator::DecompressAxisFromShort(ShortPitch);
		Yaw	= FRotator::DecompressAxisFromShort(ShortYaw);
		Roll = FRotator::DecompressAxisFromShort(ShortRoll);
	}
}

CORE_API FRotator FVector::Rotation() const
{
	FRotator R;

	// Find yaw.
	R.Yaw = FMath::Atan2(Y,X) * 180.f / PI;

	// Find pitch.
	R.Pitch = FMath::Atan2(Z,FMath::Sqrt(X*X+Y*Y)) * 180.f / PI;

	// Find roll.
	R.Roll = 0;

	return R;
}

FRotator FVector4::Rotation() const
{
	FRotator R;

	// Find yaw.
	R.Yaw = FMath::Atan2(Y,X) * 180.f / PI;

	// Find pitch.
	R.Pitch = FMath::Atan2(Z,FMath::Sqrt(X*X+Y*Y)) * 180.f / PI;

	// Find roll.
	R.Roll = 0;

	return R;
}

void FVector::FindBestAxisVectors( FVector& Axis1, FVector& Axis2 ) const
{
	const float NX = FMath::Abs(X);
	const float NY = FMath::Abs(Y);
	const float NZ = FMath::Abs(Z);

	// Find best basis vectors.
	if( NZ>NX && NZ>NY )	Axis1 = FVector(1,0,0);
	else					Axis1 = FVector(0,0,1);

	Axis1 = (Axis1 - *this * (Axis1 | *this)).GetSafeNormal();
	Axis2 = Axis1 ^ *this;
}

FVector FMath::ClosestPointOnLine(const FVector& LineStart, const FVector& LineEnd, const FVector& Point)
{
	// Solve to find alpha along line that is closest point
	// Weisstein, Eric W. "Point-Line Distance--3-Dimensional." From MathWorld--A Wolfram Web Resource. http://mathworld.wolfram.com/Point-LineDistance3-Dimensional.html 
	const float A = (LineStart-Point) | (LineEnd - LineStart);
	const float B = (LineEnd - LineStart).SizeSquared();
	const float T = FMath::Clamp(-A/B, 0.f, 1.f);

	// Generate closest point
	FVector ClosestPoint = LineStart + (T * (LineEnd - LineStart));

	return ClosestPoint;
}

void FVector::CreateOrthonormalBasis(FVector& XAxis,FVector& YAxis,FVector& ZAxis)
{
	// Project the X and Y axes onto the plane perpendicular to the Z axis.
	XAxis -= (XAxis | ZAxis) / (ZAxis | ZAxis) * ZAxis;
	YAxis -= (YAxis | ZAxis) / (ZAxis | ZAxis) * ZAxis;

	// If the X axis was parallel to the Z axis, choose a vector which is orthogonal to the Y and Z axes.
	if(XAxis.SizeSquared() < DELTA*DELTA)
	{
		XAxis = YAxis ^ ZAxis;
	}

	// If the Y axis was parallel to the Z axis, choose a vector which is orthogonal to the X and Z axes.
	if(YAxis.SizeSquared() < DELTA*DELTA)
	{
		YAxis = XAxis ^ ZAxis;
	}

	// Normalize the basis vectors.
	XAxis.Normalize();
	YAxis.Normalize();
	ZAxis.Normalize();
}

void FVector::UnwindEuler()
{
	X = FMath::UnwindDegrees(X);
	Y = FMath::UnwindDegrees(Y);
	Z = FMath::UnwindDegrees(Z);
}


FRotator::FRotator(const FQuat& Quat)
{
	*this = Quat.Rotator();
}


CORE_API FVector FRotator::Vector() const
{
	float CP, SP, CY, SY;
	FMath::SinCos( &SP, &CP, FMath::DegreesToRadians(Pitch) );
	FMath::SinCos( &SY, &CY, FMath::DegreesToRadians(Yaw) );
	FVector V = FVector( CP*CY, CP*SY, SP );

	return V;
}


FQuat FRotator::Quaternion() const
{
	SCOPE_CYCLE_COUNTER(STAT_MathConvertRotatorToQuat);

#if USE_MATRIX_ROTATOR 
	FQuat RotationMatrix = FQuat( FRotationMatrix( *this ) );
#endif

#if WITH_DIRECTXMATH	// Currently VectorSinCos() is only vectorized in DirectX implementation. Other platforms become slower if they use this.
	VectorRegister Angles = MakeVectorRegister(Roll, Pitch, Yaw, 0.0f);
	VectorRegister HalfAngles = VectorMultiply(Angles, GlobalVectorConstants::DEG_TO_RAD_HALF);

	union { VectorRegister v; float f[4]; } SinAngles, CosAngles;	
	VectorSinCos(&SinAngles.v, &CosAngles.v, &HalfAngles);

	const float	SR	= SinAngles.f[0];
	const float	SP	= SinAngles.f[1];
	const float	SY	= SinAngles.f[2];
	const float	CR	= CosAngles.f[0];
	const float	CP	= CosAngles.f[1];
	const float	CY	= CosAngles.f[2];

#else
	static const float DEG_TO_RAD = PI/(180.f);
	static const float DIVIDE_BY_2 = DEG_TO_RAD/2.f;

	float SP, SY, SR;
	float CP, CY, CR;

	FMath::SinCos(&SP, &CP, Pitch*DIVIDE_BY_2);
	FMath::SinCos(&SY, &CY, Yaw*DIVIDE_BY_2);
	FMath::SinCos(&SR, &CR, Roll*DIVIDE_BY_2);
#endif

	FQuat RotationQuat;
	RotationQuat.W = CR*CP*CY + SR*SP*SY;
	RotationQuat.X = CR*SP*SY - SR*CP*CY;
	RotationQuat.Y = -CR*SP*CY - SR*CP*SY;
	RotationQuat.Z = CR*CP*SY - SR*SP*CY;

#if USE_MATRIX_ROTATOR 
	if (!RotationMatrix.Equals(RotationQuat, KINDA_SMALL_NUMBER) && !RotationMatrix.Equals(RotationQuat*-1.f, KINDA_SMALL_NUMBER))
	{
		UE_LOG(LogUnrealMath, Log, TEXT("RotationMatrix (%s), RotationQuat (%s)"), *RotationMatrix.ToString(), *RotationQuat.ToString());
	}
	return RotationMatrix;

#else
	return RotationQuat;
#endif
}

FVector FRotator::Euler() const
{
	return FVector( Roll, Pitch, Yaw );
}

FRotator FRotator::MakeFromEuler(const FVector& Euler)
{
	return FRotator(Euler.Y, Euler.Z, Euler.X);
}

FVector FRotator::UnrotateVector(const FVector& V) const
{
	return FRotationMatrix(*this).GetTransposed().TransformVector( V );
}	

FVector FRotator::RotateVector(const FVector& V) const
{
	return FRotationMatrix(*this).TransformVector( V );
}	


void FRotator::GetWindingAndRemainder(FRotator& Winding, FRotator& Remainder) const
{
	//// YAW
	Remainder.Yaw = NormalizeAxis(Yaw);

	Winding.Yaw = Yaw - Remainder.Yaw;

	//// PITCH
	Remainder.Pitch = NormalizeAxis(Pitch);

	Winding.Pitch = Pitch - Remainder.Pitch;

	//// ROLL
	Remainder.Roll = NormalizeAxis(Roll);

	Winding.Roll = Roll - Remainder.Roll;
}



FRotator FMatrix::Rotator() const
{
	const FVector		XAxis	= GetScaledAxis( EAxis::X );
	const FVector		YAxis	= GetScaledAxis( EAxis::Y );
	const FVector		ZAxis	= GetScaledAxis( EAxis::Z );

	FRotator	Rotator	= FRotator( 
									FMath::Atan2( XAxis.Z, FMath::Sqrt(FMath::Square(XAxis.X)+FMath::Square(XAxis.Y)) ) * 180.f / PI, 
									FMath::Atan2( XAxis.Y, XAxis.X ) * 180.f / PI, 
									0 
								);
	
	const FVector		SYAxis	= FRotationMatrix( Rotator ).GetScaledAxis( EAxis::Y );
	Rotator.Roll		= FMath::Atan2( ZAxis | SYAxis, YAxis | SYAxis ) * 180.f / PI;

	return Rotator;
}


FQuat FMatrix::ToQuat() const
{
	FQuat Result(*this);
	return Result;
}

const FMatrix FMatrix::Identity(FPlane(1,0,0,0),FPlane(0,1,0,0),FPlane(0,0,1,0),FPlane(0,0,0,1));

CORE_API const FQuat FQuat::Identity(0,0,0,1);

FString FMatrix::ToString() const
{
	FString Output;

	Output += FString::Printf(TEXT("[%f %f %f %f] "), M[0][0], M[0][1], M[0][2], M[0][3]);
	Output += FString::Printf(TEXT("[%f %f %f %f] "), M[1][0], M[1][1], M[1][2], M[1][3]);
	Output += FString::Printf(TEXT("[%f %f %f %f] "), M[2][0], M[2][1], M[2][2], M[2][3]);
	Output += FString::Printf(TEXT("[%f %f %f %f] "), M[3][0], M[3][1], M[3][2], M[3][3]);

	return Output;
}

void FMatrix::DebugPrint() const
{
	UE_LOG(LogUnrealMath, Log, TEXT("%s"), *ToString());
}

//////////////////////////////////////////////////////////////////////////
// FQuat

FRotator FQuat::Rotator() const
{
	SCOPE_CYCLE_COUNTER(STAT_MathConvertQuatToRotator);

#if USE_MATRIX_ROTATOR 
	// if you think this function is problem, you can undo previous matrix rotator by returning RotatorFromMatrix
	FRotator RotatorFromMatrix = FQuatRotationTranslationMatrix(*this, FVector::ZeroVector).Rotator();
	checkSlow(IsNormalized());
#endif

	const float SingularityTest = Z*X-W*Y;
	const float YawY = 2.f*(W*Z+X*Y);
	const float YawX = (1.f-2.f*(FMath::Square(Y) + FMath::Square(Z)));

	// reference 
	// http://en.wikipedia.org/wiki/Conversion_between_quaternions_and_Euler_angles
	// http://www.euclideanspace.com/maths/geometry/rotations/conversions/quaternionToEuler/

	// this value was found from experience, the above websites recommend different values
	// but that isn't the case for us, so I went through different testing, and finally found the case 
	// where both of world lives happily. 
	const float SINGULARITY_THRESHOLD = 0.4999995f;

#if PLATFORM_ENABLE_VECTORINTRINSICS

	FRotator RotatorFromQuat;
	union { VectorRegister v; float f[4]; } VRotatorFromQuat;

	if (SingularityTest < -SINGULARITY_THRESHOLD)
	{
		// Pitch
		VRotatorFromQuat.f[0] = 3.0f*HALF_PI;	// 270 deg
		// Yaw
		VRotatorFromQuat.f[1] = FMath::Atan2(YawY, YawX);
		// Roll
		VRotatorFromQuat.f[2] = -VRotatorFromQuat.f[1] - (2.f * FMath::Atan2(X, W));
	}
	else if (SingularityTest > SINGULARITY_THRESHOLD)
	{
		// Pitch
		VRotatorFromQuat.f[0] = HALF_PI;	// 90 deg
		// Yaw
		VRotatorFromQuat.f[1] = FMath::Atan2(YawY, YawX);
		//Roll
		VRotatorFromQuat.f[2] = VRotatorFromQuat.f[1] - (2.f * FMath::Atan2(X, W));
	}
	else
	{
		//Pitch
		VRotatorFromQuat.f[0] = FMath::FastAsin(2.f*(SingularityTest));
		// Yaw
		VRotatorFromQuat.f[1] = FMath::Atan2(YawY, YawX);
		// Roll
		VRotatorFromQuat.f[2] = FMath::Atan2(-2.f*(W*X+Y*Z), (1.f-2.f*(FMath::Square(X) + FMath::Square(Y))));
	}

	VRotatorFromQuat.f[3] = 0.f; // We should initialize this, otherwise the value can be denormalized which is bad for floating point perf.
	VRotatorFromQuat.v = VectorMultiply(VRotatorFromQuat.v, GlobalVectorConstants::RAD_TO_DEG);
	VRotatorFromQuat.v = VectorNormalizeRotator(VRotatorFromQuat.v);
	VectorStoreFloat3(VRotatorFromQuat.v, &RotatorFromQuat);

#else // PLATFORM_ENABLE_VECTORINTRINSICS

	static const float RAD_TO_DEG = (180.f)/PI;
	FRotator RotatorFromQuat;

	if (SingularityTest < -SINGULARITY_THRESHOLD)
	{
		RotatorFromQuat.Pitch = 270.f;
		RotatorFromQuat.Yaw = FMath::Atan2(YawY, YawX) * RAD_TO_DEG;
		RotatorFromQuat.Roll = -RotatorFromQuat.Yaw - (2.f * FMath::Atan2(X, W) * RAD_TO_DEG);
	}
	else if (SingularityTest > SINGULARITY_THRESHOLD)
	{
		RotatorFromQuat.Pitch = 90.f;
		RotatorFromQuat.Yaw = FMath::Atan2(YawY, YawX) * RAD_TO_DEG;
		RotatorFromQuat.Roll = RotatorFromQuat.Yaw - (2.f * FMath::Atan2(X, W) * RAD_TO_DEG);
	}
	else
	{
		RotatorFromQuat.Pitch = FMath::FastAsin(2.f*(SingularityTest)) * RAD_TO_DEG;
		RotatorFromQuat.Yaw = FMath::Atan2(YawY, YawX) * RAD_TO_DEG;
		RotatorFromQuat.Roll = FMath::Atan2(-2.f*(W*X+Y*Z), (1.f-2.f*(FMath::Square(X) + FMath::Square(Y)))) * RAD_TO_DEG;
	}

	RotatorFromQuat.Normalize();

#endif // PLATFORM_ENABLE_VECTORINTRINSICS


#if USE_MATRIX_ROTATOR
	RotatorFromMatrix = RotatorFromMatrix.Clamp();

	// this Euler is degree, so less 1 is negligible
	if (!DebugRotatorEquals(RotatorFromQuat, RotatorFromMatrix, 0.1f))
	{
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("WRONG: (Singularity: %.9f) RotationMatrix (%s), RotationQuat(%s)"), SingularityTest, *RotatorFromMatrix.ToString(), *RotatorFromQuat.ToString());
	}

	return RotatorFromMatrix;
#else
	return RotatorFromQuat;
#endif
}

FQuat FQuat::MakeFromEuler(const FVector& Euler)
{
	return FRotator::MakeFromEuler(Euler).Quaternion();
}

FMatrix FRotationAboutPointMatrix::Make(const FQuat& Rot, const FVector& Origin)
{
	return FRotationAboutPointMatrix(Rot.Rotator(), Origin);
}

FMatrix FRotationMatrix::Make(FQuat const& Rot)
{
	return FQuatRotationTranslationMatrix(Rot, FVector::ZeroVector);
}

FMatrix FRotationMatrix::MakeFromX(FVector const& XAxis)
{
	FVector const NewX = XAxis.GetSafeNormal();

	// try to use up if possible
	FVector const UpVector = ( FMath::Abs(NewX.Z) < (1.f - KINDA_SMALL_NUMBER) ) ? FVector(0,0,1.f) : FVector(1.f,0,0);

	const FVector NewY = (UpVector ^ NewX).GetSafeNormal();
	const FVector NewZ = NewX ^ NewY;

	return FMatrix(NewX, NewY, NewZ, FVector::ZeroVector);
}

FMatrix FRotationMatrix::MakeFromY(FVector const& YAxis)
{
	FVector const NewY = YAxis.GetSafeNormal();

	// try to use up if possible
	FVector const UpVector = ( FMath::Abs(NewY.Z) < (1.f - KINDA_SMALL_NUMBER) ) ? FVector(0,0,1.f) : FVector(1.f,0,0);

	const FVector NewZ = (UpVector ^ NewY).GetSafeNormal();
	const FVector NewX = NewY ^ NewZ;

	return FMatrix(NewX, NewY, NewZ, FVector::ZeroVector);
}

FMatrix FRotationMatrix::MakeFromZ(FVector const& ZAxis)
{
	FVector const NewZ = ZAxis.GetSafeNormal();

	// try to use up if possible
	FVector const UpVector = ( FMath::Abs(NewZ.Z) < (1.f - KINDA_SMALL_NUMBER) ) ? FVector(0,0,1.f) : FVector(1.f,0,0);

	const FVector NewX = (UpVector ^ NewZ).GetSafeNormal();
	const FVector NewY = NewZ ^ NewX;

	return FMatrix(NewX, NewY, NewZ, FVector::ZeroVector);
}

FMatrix FRotationMatrix::MakeFromXY(FVector const& XAxis, FVector const& YAxis)
{
	FVector NewX = XAxis.GetSafeNormal();
	FVector Norm = YAxis.GetSafeNormal();

	// if they're almost same, we need to find arbitrary vector
	if ( FMath::IsNearlyEqual(FMath::Abs(NewX | Norm), 1.f) )
	{
		// make sure we don't ever pick the same as NewX
		Norm = ( FMath::Abs(NewX.Z) < (1.f - KINDA_SMALL_NUMBER) ) ? FVector(0,0,1.f) : FVector(1.f,0,0);
	}

	const FVector NewZ = (NewX ^ Norm).GetSafeNormal();
	const FVector NewY = NewZ ^ NewX;

	return FMatrix(NewX, NewY, NewZ, FVector::ZeroVector);
}

FMatrix FRotationMatrix::MakeFromXZ(FVector const& XAxis, FVector const& ZAxis)
{
	FVector const NewX = XAxis.GetSafeNormal();
	FVector Norm = ZAxis.GetSafeNormal();

	// if they're almost same, we need to find arbitrary vector
	if ( FMath::IsNearlyEqual(FMath::Abs(NewX | Norm), 1.f) )
	{
		// make sure we don't ever pick the same as NewX
		Norm = ( FMath::Abs(NewX.Z) < (1.f - KINDA_SMALL_NUMBER) ) ? FVector(0,0,1.f) : FVector(1.f,0,0);
	}

	const FVector NewY = (Norm ^ NewX).GetSafeNormal();
	const FVector NewZ = NewX ^ NewY;

	return FMatrix(NewX, NewY, NewZ, FVector::ZeroVector);
}

FMatrix FRotationMatrix::MakeFromYX(FVector const& YAxis, FVector const& XAxis)
{
	FVector const NewY = YAxis.GetSafeNormal();
	FVector Norm = XAxis.GetSafeNormal();

	// if they're almost same, we need to find arbitrary vector
	if ( FMath::IsNearlyEqual(FMath::Abs(NewY | Norm), 1.f) )
	{
		// make sure we don't ever pick the same as NewX
		Norm = ( FMath::Abs(NewY.Z) < (1.f - KINDA_SMALL_NUMBER) ) ? FVector(0,0,1.f) : FVector(1.f,0,0);
	}
	
	const FVector NewZ = (Norm ^ NewY).GetSafeNormal();
	const FVector NewX = NewY ^ NewZ;

	return FMatrix(NewX, NewY, NewZ, FVector::ZeroVector);
}

FMatrix FRotationMatrix::MakeFromYZ(FVector const& YAxis, FVector const& ZAxis)
{
	FVector const NewY = YAxis.GetSafeNormal();
	FVector Norm = ZAxis.GetSafeNormal();

	// if they're almost same, we need to find arbitrary vector
	if ( FMath::IsNearlyEqual(FMath::Abs(NewY | Norm), 1.f) )
	{
		// make sure we don't ever pick the same as NewX
		Norm = ( FMath::Abs(NewY.Z) < (1.f - KINDA_SMALL_NUMBER) ) ? FVector(0,0,1.f) : FVector(1.f,0,0);
	}

	const FVector NewX = (NewY ^ Norm).GetSafeNormal();
	const FVector NewZ = NewX ^ NewY;

	return FMatrix(NewX, NewY, NewZ, FVector::ZeroVector);
}

FMatrix FRotationMatrix::MakeFromZX(FVector const& ZAxis, FVector const& XAxis)
{
	FVector const NewZ = ZAxis.GetSafeNormal();
	FVector Norm = XAxis.GetSafeNormal();

	// if they're almost same, we need to find arbitrary vector
	if ( FMath::IsNearlyEqual(FMath::Abs(NewZ | Norm), 1.f) )
	{
		// make sure we don't ever pick the same as NewX
		Norm = ( FMath::Abs(NewZ.Z) < (1.f - KINDA_SMALL_NUMBER) ) ? FVector(0,0,1.f) : FVector(1.f,0,0);
	}

	const FVector NewY = (NewZ ^ Norm).GetSafeNormal();
	const FVector NewX = NewY ^ NewZ;

	return FMatrix(NewX, NewY, NewZ, FVector::ZeroVector);
}

FMatrix FRotationMatrix::MakeFromZY(FVector const& ZAxis, FVector const& YAxis)
{
	FVector const NewZ = ZAxis.GetSafeNormal();
	FVector Norm = YAxis.GetSafeNormal();

	// if they're almost same, we need to find arbitrary vector
	if ( FMath::IsNearlyEqual(FMath::Abs(NewZ | Norm), 1.f) )
	{
		// make sure we don't ever pick the same as NewX
		Norm = ( FMath::Abs(NewZ.Z) < (1.f - KINDA_SMALL_NUMBER) ) ? FVector(0,0,1.f) : FVector(1.f,0,0);
	}

	const FVector NewX = (Norm ^ NewZ).GetSafeNormal();
	const FVector NewY = NewZ ^ NewX;

	return FMatrix(NewX, NewY, NewZ, FVector::ZeroVector);
}

FVector FQuat::Euler() const
{
	return Rotator().Euler();
}

bool FQuat::NetSerialize(FArchive& Ar, class UPackageMap*, bool& bOutSuccess)
{
	FQuat &Q = *this;

	if (Ar.IsSaving())
	{
		// Make sure we have a non null SquareSum. It shouldn't happen with a quaternion, but better be safe.
		if(Q.SizeSquared() <= SMALL_NUMBER)
		{
			Q = FQuat::Identity;
		}
		else
		{
			// All transmitted quaternions *MUST BE* unit quaternions, in which case we can deduce the value of W.
			Q.Normalize();
			// force W component to be non-negative
			if (Q.W < 0.f)
			{
				Q.X *= -1.f;
				Q.Y *= -1.f;
				Q.Z *= -1.f;
				Q.W *= -1.f;
			}
		}
	}

	Ar << Q.X << Q.Y << Q.Z;
	if ( Ar.IsLoading() )
	{
		const float XYZMagSquared = (Q.X*Q.X + Q.Y*Q.Y + Q.Z*Q.Z);
		const float WSquared = 1.0f - XYZMagSquared;
		// If mag of (X,Y,Z) <= 1.0, then we calculate W to make magnitude of Q 1.0
		if (WSquared >= 0.f)
		{
			Q.W = FMath::Sqrt(WSquared);
		}
		// If mag of (X,Y,Z) > 1.0, we set W to zero, and then renormalize 
		else
		{
			Q.W = 0.f;

			const float XYZInvMag = FMath::InvSqrt(XYZMagSquared);
			Q.X *= XYZInvMag;
			Q.Y *= XYZInvMag;
			Q.Z *= XYZInvMag;
		}
	}

	bOutSuccess = true;
	return true;
}

FQuat FQuat::FindBetween(const FVector& vec1, const FVector& vec2)
{
	const FVector cross = vec1 ^ vec2;
	const float crossMag = cross.Size();

	// See if vectors are parallel or anti-parallel
	if(crossMag < KINDA_SMALL_NUMBER)
	{
		// If these vectors are parallel - just return identity quaternion (ie no rotation).
		const float Dot = vec1 | vec2;
		if(Dot > -KINDA_SMALL_NUMBER)
		{
			return FQuat::Identity; // no rotation
		}
		// Exactly opposite..
		else
		{
			// ..rotation by 180 degrees around a vector orthogonal to vec1 & vec2
			FVector Vec = vec1.SizeSquared() > vec2.SizeSquared() ? vec1 : vec2;
			Vec.Normalize();

			FVector AxisA, AxisB;
			Vec.FindBestAxisVectors(AxisA, AxisB);

			return FQuat(AxisA.X, AxisA.Y, AxisA.Z, 0.f); // (axis*sin(pi/2), cos(pi/2)) = (axis, 0)
		}
	}

	// Not parallel, so use normal code
	float angle = FMath::Asin(crossMag);

	const float dot = vec1 | vec2;
	if(dot < 0.0f)
	{
		angle = PI - angle;
	}

	float sinHalfAng, cosHalfAng;
	FMath::SinCos(&sinHalfAng, &cosHalfAng, 0.5f * angle);
	const FVector axis = cross / crossMag;

	return FQuat(
		sinHalfAng * axis.X,
		sinHalfAng * axis.Y,
		sinHalfAng * axis.Z,
		cosHalfAng );
}

FQuat FQuat::Log() const
{
	FQuat Result;
	Result.W = 0.f;

	if ( FMath::Abs(W) < 1.f )
	{
		const float Angle = FMath::Acos(W);
		const float SinAngle = FMath::Sin(Angle);

		if ( FMath::Abs(SinAngle) >= SMALL_NUMBER )
		{
			const float Scale = Angle/SinAngle;
			Result.X = Scale*X;
			Result.Y = Scale*Y;
			Result.Z = Scale*Z;

			return Result;
		}
	}

	Result.X = X;
	Result.Y = Y;
	Result.Z = Z;

	return Result;
}

FQuat FQuat::Exp() const
{
	const float Angle = FMath::Sqrt(X*X + Y*Y + Z*Z);
	const float SinAngle = FMath::Sin(Angle);

	FQuat Result;
	Result.W = FMath::Cos(Angle);

	if ( FMath::Abs(SinAngle) >= SMALL_NUMBER )
	{
		const float Scale = SinAngle/Angle;
		Result.X = Scale*X;
		Result.Y = Scale*Y;
		Result.Z = Scale*Z;
	}
	else
	{
		Result.X = X;
		Result.Y = Y;
		Result.Z = Z;
	}

	return Result;
}

/*-----------------------------------------------------------------------------
	Swept-Box vs Box test.
-----------------------------------------------------------------------------*/

/* Line-extent/Box Test Util */
bool FMath::LineExtentBoxIntersection(const FBox& inBox, 
								 const FVector& Start, 
								 const FVector& End,
								 const FVector& Extent,
								 FVector& HitLocation,
								 FVector& HitNormal,
								 float& HitTime)
{
	FBox box = inBox;
	box.Max.X += Extent.X;
	box.Max.Y += Extent.Y;
	box.Max.Z += Extent.Z;
	
	box.Min.X -= Extent.X;
	box.Min.Y -= Extent.Y;
	box.Min.Z -= Extent.Z;

	const FVector Dir = (End - Start);
	
	FVector	Time;
	bool	Inside = 1;
	float   faceDir[3] = {1, 1, 1};
	
	/////////////// X
	if(Start.X < box.Min.X)
	{
		if(Dir.X <= 0.0f)
			return 0;
		else
		{
			Inside = 0;
			faceDir[0] = -1;
			Time.X = (box.Min.X - Start.X) / Dir.X;
		}
	}
	else if(Start.X > box.Max.X)
	{
		if(Dir.X >= 0.0f)
			return 0;
		else
		{
			Inside = 0;
			Time.X = (box.Max.X - Start.X) / Dir.X;
		}
	}
	else
		Time.X = 0.0f;
	
	/////////////// Y
	if(Start.Y < box.Min.Y)
	{
		if(Dir.Y <= 0.0f)
			return 0;
		else
		{
			Inside = 0;
			faceDir[1] = -1;
			Time.Y = (box.Min.Y - Start.Y) / Dir.Y;
		}
	}
	else if(Start.Y > box.Max.Y)
	{
		if(Dir.Y >= 0.0f)
			return 0;
		else
		{
			Inside = 0;
			Time.Y = (box.Max.Y - Start.Y) / Dir.Y;
		}
	}
	else
		Time.Y = 0.0f;
	
	/////////////// Z
	if(Start.Z < box.Min.Z)
	{
		if(Dir.Z <= 0.0f)
			return 0;
		else
		{
			Inside = 0;
			faceDir[2] = -1;
			Time.Z = (box.Min.Z - Start.Z) / Dir.Z;
		}
	}
	else if(Start.Z > box.Max.Z)
	{
		if(Dir.Z >= 0.0f)
			return 0;
		else
		{
			Inside = 0;
			Time.Z = (box.Max.Z - Start.Z) / Dir.Z;
		}
	}
	else
		Time.Z = 0.0f;
	
	// If the line started inside the box (ie. player started in contact with the fluid)
	if(Inside)
	{
		HitLocation = Start;
		HitNormal = FVector(0, 0, 1);
		HitTime = 0;
		return 1;
	}
	// Otherwise, calculate when hit occured
	else
	{	
		if(Time.Y > Time.Z)
		{
			HitTime = Time.Y;
			HitNormal = FVector(0, faceDir[1], 0);
		}
		else
		{
			HitTime = Time.Z;
			HitNormal = FVector(0, 0, faceDir[2]);
		}
		
		if(Time.X > HitTime)
		{
			HitTime = Time.X;
			HitNormal = FVector(faceDir[0], 0, 0);
		}
		
		if(HitTime >= 0.0f && HitTime <= 1.0f)
		{
			HitLocation = Start + Dir * HitTime;
			const float BOX_SIDE_THRESHOLD = 0.1f;
			if(	HitLocation.X > box.Min.X - BOX_SIDE_THRESHOLD && HitLocation.X < box.Max.X + BOX_SIDE_THRESHOLD &&
				HitLocation.Y > box.Min.Y - BOX_SIDE_THRESHOLD && HitLocation.Y < box.Max.Y + BOX_SIDE_THRESHOLD &&
				HitLocation.Z > box.Min.Z - BOX_SIDE_THRESHOLD && HitLocation.Z < box.Max.Z + BOX_SIDE_THRESHOLD)
			{				
				return 1;
			}
		}
		
		return 0;
	}
}

float FVector::EvaluateBezier(const FVector* ControlPoints, int32 NumPoints, TArray<FVector>& OutPoints)
{
	check( ControlPoints );
	check( NumPoints >= 2 );

	// var q is the change in t between successive evaluations.
	const float q = 1.f/(NumPoints-1); // q is dependent on the number of GAPS = POINTS-1

	// recreate the names used in the derivation
	const FVector& P0 = ControlPoints[0];
	const FVector& P1 = ControlPoints[1];
	const FVector& P2 = ControlPoints[2];
	const FVector& P3 = ControlPoints[3];

	// coefficients of the cubic polynomial that we're FDing -
	const FVector a = P0;
	const FVector b = 3*(P1-P0);
	const FVector c = 3*(P2-2*P1+P0);
	const FVector d = P3-3*P2+3*P1-P0;

	// initial values of the poly and the 3 diffs -
	FVector S  = a;						// the poly value
	FVector U  = b*q + c*q*q + d*q*q*q;	// 1st order diff (quadratic)
	FVector V  = 2*c*q*q + 6*d*q*q*q;	// 2nd order diff (linear)
	FVector W  = 6*d*q*q*q;				// 3rd order diff (constant)

	// Path length.
	float Length = 0.f;

	FVector OldPos = P0;
	OutPoints.Add( P0 );	// first point on the curve is always P0.

	for( int32 i = 1 ; i < NumPoints ; ++i )
	{
		// calculate the next value and update the deltas
		S += U;			// update poly value
		U += V;			// update 1st order diff value
		V += W;			// update 2st order diff value
		// 3rd order diff is constant => no update needed.

		// Update Length.
		Length += FVector::Dist( S, OldPos );
		OldPos  = S;

		OutPoints.Add( S );
	}

	// Return path length as experienced in sequence (linear interpolation between points).
	return Length;
}

float FLinearColor::EvaluateBezier(const FLinearColor* ControlPoints, int32 NumPoints, TArray<FLinearColor>& OutPoints)
{
	check( ControlPoints );
	check( NumPoints >= 2 );

	// var q is the change in t between successive evaluations.
	const float q = 1.f/(NumPoints-1); // q is dependent on the number of GAPS = POINTS-1

	// recreate the names used in the derivation
	const FLinearColor& P0 = ControlPoints[0];
	const FLinearColor& P1 = ControlPoints[1];
	const FLinearColor& P2 = ControlPoints[2];
	const FLinearColor& P3 = ControlPoints[3];

	// coefficients of the cubic polynomial that we're FDing -
	const FLinearColor a = P0;
	const FLinearColor b = 3*(P1-P0);
	const FLinearColor c = 3*(P2-2*P1+P0);
	const FLinearColor d = P3-3*P2+3*P1-P0;

	// initial values of the poly and the 3 diffs -
	FLinearColor S  = a;						// the poly value
	FLinearColor U  = b*q + c*q*q + d*q*q*q;	// 1st order diff (quadratic)
	FLinearColor V  = 2*c*q*q + 6*d*q*q*q;	// 2nd order diff (linear)
	FLinearColor W  = 6*d*q*q*q;				// 3rd order diff (constant)

	// Path length.
	float Length = 0.f;

	FLinearColor OldPos = P0;
	OutPoints.Add( P0 );	// first point on the curve is always P0.

	for( int32 i = 1 ; i < NumPoints ; ++i )
	{
		// calculate the next value and update the deltas
		S += U;			// update poly value
		U += V;			// update 1st order diff value
		V += W;			// update 2st order diff value
		// 3rd order diff is constant => no update needed.

		// Update Length.
		Length += FLinearColor::Dist( S, OldPos );
		OldPos  = S;

		OutPoints.Add( S );
	}

	// Return path length as experienced in sequence (linear interpolation between points).
	return Length;
}



FQuat FQuat::Slerp(const FQuat& Quat1,const FQuat& Quat2, float Slerp)
{
	// Get cosine of angle between quats.
	const float RawCosom = 
		    Quat1.X * Quat2.X +
			Quat1.Y * Quat2.Y +
			Quat1.Z * Quat2.Z +
			Quat1.W * Quat2.W;
	// Unaligned quats - compensate, results in taking shorter route.
	const float Cosom = FMath::FloatSelect( RawCosom, RawCosom, -RawCosom );
	
	float Scale0, Scale1;

	if( Cosom < 0.9999f )
	{	
		const float Omega = FMath::Acos(Cosom);
		const float InvSin = 1.f/FMath::Sin(Omega);
		Scale0 = FMath::Sin( (1.f - Slerp) * Omega ) * InvSin;
		Scale1 = FMath::Sin( Slerp * Omega ) * InvSin;
	}
	else
	{
		// Use linear interpolation.
		Scale0 = 1.0f - Slerp;
		Scale1 = Slerp;	
	}

	// In keeping with our flipped Cosom:
	Scale1 = FMath::FloatSelect( RawCosom, Scale1, -Scale1 );

	FQuat Result;
		
	Result.X = Scale0 * Quat1.X + Scale1 * Quat2.X;
	Result.Y = Scale0 * Quat1.Y + Scale1 * Quat2.Y;
	Result.Z = Scale0 * Quat1.Z + Scale1 * Quat2.Z;
	Result.W = Scale0 * Quat1.W + Scale1 * Quat2.W;

	return Result;
}

FQuat FQuat::SlerpFullPath(const FQuat &quat1, const FQuat &quat2, float Alpha )
{
	const float CosAngle = FMath::Clamp(quat1 | quat2, -1.f, 1.f);
	const float Angle = FMath::Acos(CosAngle);

	//UE_LOG(LogUnrealMath, Log,  TEXT("CosAngle: %f Angle: %f"), CosAngle, Angle );

	if ( FMath::Abs(Angle) < KINDA_SMALL_NUMBER )
	{
		return quat1;
	}

	const float SinAngle = FMath::Sin(Angle);
	const float InvSinAngle = 1.f/SinAngle;

	const float Scale0 = FMath::Sin((1.0f-Alpha)*Angle)*InvSinAngle;
	const float Scale1 = FMath::Sin(Alpha*Angle)*InvSinAngle;

	return quat1*Scale0 + quat2*Scale1;
}

FQuat FQuat::Squad(const FQuat& quat1, const FQuat& tang1, const FQuat& quat2, const FQuat& tang2, float Alpha)
{
	const FQuat Q1 = FQuat::SlerpFullPath(quat1, quat2, Alpha);
	//UE_LOG(LogUnrealMath, Log, TEXT("Q1: %f %f %f %f"), Q1.X, Q1.Y, Q1.Z, Q1.W);

	const FQuat Q2 = FQuat::SlerpFullPath(tang1, tang2, Alpha);
	//UE_LOG(LogUnrealMath, Log, TEXT("Q2: %f %f %f %f"), Q2.X, Q2.Y, Q2.Z, Q2.W);

	const FQuat Result = FQuat::SlerpFullPath(Q1, Q2, 2.f * Alpha * (1.f - Alpha));
	//FQuat Result = FQuat::Slerp(Q1, Q2, 2.f * Alpha * (1.f - Alpha));
	//UE_LOG(LogUnrealMath, Log, TEXT("Result: %f %f %f %f"), Result.X, Result.Y, Result.Z, Result.W);

	return Result;
}

void FQuat::CalcTangents( const FQuat& PrevP, const FQuat& P, const FQuat& NextP, float Tension, FQuat& OutTan )
{
	const FQuat InvP = P.Inverse();
	const FQuat Part1 = (InvP * PrevP).Log();
	const FQuat Part2 = (InvP * NextP).Log();

	const FQuat PreExp = (Part1 + Part2) * -0.5f;

	OutTan = P * PreExp.Exp();
}

static void FindBounds( float& OutMin, float& OutMax,  float Start, float StartLeaveTan, float StartT, float End, float EndArriveTan, float EndT, bool bCurve )
{
	OutMin = FMath::Min( Start, End );
	OutMax = FMath::Max( Start, End );

	// Do we need to consider extermeties of a curve?
	if(bCurve)
	{
		// Scale tangents based on time interval, so this code matches the behaviour in FInterpCurve::Eval
		float Diff = EndT - StartT;
		StartLeaveTan *= Diff;
		EndArriveTan *= Diff;

		const float a = 6.f*Start + 3.f*StartLeaveTan + 3.f*EndArriveTan - 6.f*End;
		const float b = -6.f*Start - 4.f*StartLeaveTan - 2.f*EndArriveTan + 6.f*End;
		const float c = StartLeaveTan;

		const float Discriminant = (b*b) - (4.f*a*c);
		if(Discriminant > 0.f && !FMath::IsNearlyZero(a)) // Solving doesn't work if a is zero, which usually indicates co-incident start and end, and zero tangents anyway
		{
			const float SqrtDisc = FMath::Sqrt( Discriminant );

			const float x0 = (-b + SqrtDisc)/(2.f*a); // x0 is the 'Alpha' ie between 0 and 1
			const float t0 = StartT + x0*(EndT - StartT); // Then t0 is the actual 'time' on the curve
			if(t0 > StartT && t0 < EndT)
			{
				const float Val = FMath::CubicInterp( Start, StartLeaveTan, End, EndArriveTan, x0 );

				OutMin = FMath::Min( OutMin, Val );
				OutMax = FMath::Max( OutMax, Val );
			}

			const float x1 = (-b - SqrtDisc)/(2.f*a);
			const float t1 = StartT + x1*(EndT - StartT);
			if(t1 > StartT && t1 < EndT)
			{
				const float Val = FMath::CubicInterp( Start, StartLeaveTan, End, EndArriveTan, x1 );

				OutMin = FMath::Min( OutMin, Val );
				OutMax = FMath::Max( OutMax, Val );
			}
		}
	}
}

void CORE_API CurveFloatFindIntervalBounds( const FInterpCurvePoint<float>& Start, const FInterpCurvePoint<float>& End, float& CurrentMin, float& CurrentMax )
{
	const bool bIsCurve = Start.IsCurveKey();

	float OutMin, OutMax;

	FindBounds(OutMin, OutMax, Start.OutVal, Start.LeaveTangent, Start.InVal, End.OutVal, End.ArriveTangent, End.InVal, bIsCurve);

	CurrentMin = FMath::Min( CurrentMin, OutMin );
	CurrentMax = FMath::Max( CurrentMax, OutMax );
}

void CORE_API CurveVector2DFindIntervalBounds( const FInterpCurvePoint<FVector2D>& Start, const FInterpCurvePoint<FVector2D>& End, FVector2D& CurrentMin, FVector2D& CurrentMax )
{
	const bool bIsCurve = Start.IsCurveKey();

	float OutMin, OutMax;

	FindBounds(OutMin, OutMax, Start.OutVal.X, Start.LeaveTangent.X, Start.InVal, End.OutVal.X, End.ArriveTangent.X, End.InVal, bIsCurve);
	CurrentMin.X = FMath::Min( CurrentMin.X, OutMin );
	CurrentMax.X = FMath::Max( CurrentMax.X, OutMax );

	FindBounds(OutMin, OutMax, Start.OutVal.Y, Start.LeaveTangent.Y, Start.InVal, End.OutVal.Y, End.ArriveTangent.Y, End.InVal, bIsCurve);
	CurrentMin.Y = FMath::Min( CurrentMin.Y, OutMin );
	CurrentMax.Y = FMath::Max( CurrentMax.Y, OutMax );
}

void CORE_API CurveVectorFindIntervalBounds( const FInterpCurvePoint<FVector>& Start, const FInterpCurvePoint<FVector>& End, FVector& CurrentMin, FVector& CurrentMax )
{
	const bool bIsCurve = Start.IsCurveKey();

	float OutMin, OutMax;

	FindBounds(OutMin, OutMax, Start.OutVal.X, Start.LeaveTangent.X, Start.InVal, End.OutVal.X, End.ArriveTangent.X, End.InVal, bIsCurve);
	CurrentMin.X = FMath::Min( CurrentMin.X, OutMin );
	CurrentMax.X = FMath::Max( CurrentMax.X, OutMax );

	FindBounds(OutMin, OutMax, Start.OutVal.Y, Start.LeaveTangent.Y, Start.InVal, End.OutVal.Y, End.ArriveTangent.Y, End.InVal, bIsCurve);
	CurrentMin.Y = FMath::Min( CurrentMin.Y, OutMin );
	CurrentMax.Y = FMath::Max( CurrentMax.Y, OutMax );

	FindBounds(OutMin, OutMax, Start.OutVal.Z, Start.LeaveTangent.Z, Start.InVal, End.OutVal.Z, End.ArriveTangent.Z, End.InVal, bIsCurve);
	CurrentMin.Z = FMath::Min( CurrentMin.Z, OutMin );
	CurrentMax.Z = FMath::Max( CurrentMax.Z, OutMax );
}

void CORE_API CurveTwoVectorsFindIntervalBounds(const FInterpCurvePoint<FTwoVectors>& Start, const FInterpCurvePoint<FTwoVectors>& End, FTwoVectors& CurrentMin, FTwoVectors& CurrentMax)
{
	const bool bIsCurve = Start.IsCurveKey();

	float OutMin;
	float OutMax;

	// Do the first curve
	FindBounds(OutMin, OutMax, Start.OutVal.v1.X, Start.LeaveTangent.v1.X, Start.InVal, End.OutVal.v1.X, End.ArriveTangent.v1.X, End.InVal, bIsCurve);
	CurrentMin.v1.X = FMath::Min( CurrentMin.v1.X, OutMin );
	CurrentMax.v1.X = FMath::Max( CurrentMax.v1.X, OutMax );

	FindBounds(OutMin, OutMax, Start.OutVal.v1.Y, Start.LeaveTangent.v1.Y, Start.InVal, End.OutVal.v1.Y, End.ArriveTangent.v1.Y, End.InVal, bIsCurve);
	CurrentMin.v1.Y = FMath::Min( CurrentMin.v1.Y, OutMin );
	CurrentMax.v1.Y = FMath::Max( CurrentMax.v1.Y, OutMax );

	FindBounds(OutMin, OutMax, Start.OutVal.v1.Z, Start.LeaveTangent.v1.Z, Start.InVal, End.OutVal.v1.Z, End.ArriveTangent.v1.Z, End.InVal, bIsCurve);
	CurrentMin.v1.Z = FMath::Min( CurrentMin.v1.Z, OutMin );
	CurrentMax.v1.Z = FMath::Max( CurrentMax.v1.Z, OutMax );

	// Do the second curve
	FindBounds(OutMin, OutMax, Start.OutVal.v2.X, Start.LeaveTangent.v2.X, Start.InVal, End.OutVal.v2.X, End.ArriveTangent.v2.X, End.InVal, bIsCurve);
	CurrentMin.v2.X = FMath::Min( CurrentMin.v2.X, OutMin );
	CurrentMax.v2.X = FMath::Max( CurrentMax.v2.X, OutMax );

	FindBounds(OutMin, OutMax, Start.OutVal.v2.Y, Start.LeaveTangent.v2.Y, Start.InVal, End.OutVal.v2.Y, End.ArriveTangent.v2.Y, End.InVal, bIsCurve);
	CurrentMin.v2.Y = FMath::Min( CurrentMin.v2.Y, OutMin );
	CurrentMax.v2.Y = FMath::Max( CurrentMax.v2.Y, OutMax );

	FindBounds(OutMin, OutMax, Start.OutVal.v2.Z, Start.LeaveTangent.v2.Z, Start.InVal, End.OutVal.v2.Z, End.ArriveTangent.v2.Z, End.InVal, bIsCurve);
	CurrentMin.v2.Z = FMath::Min( CurrentMin.v2.Z, OutMin );
	CurrentMax.v2.Z = FMath::Max( CurrentMax.v2.Z, OutMax );
}

void CORE_API CurveLinearColorFindIntervalBounds( const FInterpCurvePoint<FLinearColor>& Start, const FInterpCurvePoint<FLinearColor>& End, FLinearColor& CurrentMin, FLinearColor& CurrentMax )
{
	const bool bIsCurve = Start.IsCurveKey();

	float OutMin, OutMax;

	FindBounds(OutMin, OutMax, Start.OutVal.R, Start.LeaveTangent.R, Start.InVal, End.OutVal.R, End.ArriveTangent.R, End.InVal, bIsCurve);
	CurrentMin.R = FMath::Min( CurrentMin.R, OutMin );
	CurrentMax.R = FMath::Max( CurrentMax.R, OutMax );

	FindBounds(OutMin, OutMax, Start.OutVal.G, Start.LeaveTangent.G, Start.InVal, End.OutVal.G, End.ArriveTangent.G, End.InVal, bIsCurve);
	CurrentMin.G = FMath::Min( CurrentMin.G, OutMin );
	CurrentMax.G = FMath::Max( CurrentMax.G, OutMax );

	FindBounds(OutMin, OutMax, Start.OutVal.B, Start.LeaveTangent.B, Start.InVal, End.OutVal.B, End.ArriveTangent.B, End.InVal, bIsCurve);
	CurrentMin.B = FMath::Min( CurrentMin.B, OutMin );
	CurrentMax.B = FMath::Max( CurrentMax.B, OutMax );

	FindBounds(OutMin, OutMax, Start.OutVal.A, Start.LeaveTangent.A, Start.InVal, End.OutVal.A, End.ArriveTangent.A, End.InVal, bIsCurve);
	CurrentMin.A = FMath::Min( CurrentMin.A, OutMin );
	CurrentMax.A = FMath::Max( CurrentMax.A, OutMax );
}

CORE_API float FMath::PointDistToLine(const FVector &Point, const FVector &Direction, const FVector &Origin, FVector &OutClosestPoint)
{
	const FVector SafeDir = Direction.GetSafeNormal();
	OutClosestPoint = Origin + (SafeDir * ((Point-Origin) | SafeDir));
	return (OutClosestPoint-Point).Size();
}

CORE_API float FMath::PointDistToLine(const FVector &Point, const FVector &Direction, const FVector &Origin)
{
	const FVector SafeDir = Direction.GetSafeNormal();
	const FVector OutClosestPoint = Origin + (SafeDir * ((Point-Origin) | SafeDir));
	return (OutClosestPoint-Point).Size();
}

FVector FMath::ClosestPointOnSegment(const FVector &Point, const FVector &StartPoint, const FVector &EndPoint)
{
	const FVector Segment = EndPoint - StartPoint;
	const FVector VectToPoint = Point - StartPoint;

	// See if closest point is before StartPoint
	const float Dot1 = VectToPoint | Segment;
	if( Dot1 <= 0 )
	{
		return StartPoint;
	}

	// See if closest point is beyond EndPoint
	const float Dot2 = Segment | Segment;
	if( Dot2 <= Dot1 )
	{
		return EndPoint;
	}

	// Closest Point is within segment
	return StartPoint + Segment * (Dot1 / Dot2);
}

FVector2D FMath::ClosestPointOnSegment2D(const FVector2D &Point, const FVector2D &StartPoint, const FVector2D &EndPoint)
{
	const FVector2D Segment = EndPoint - StartPoint;
	const FVector2D VectToPoint = Point - StartPoint;

	// See if closest point is before StartPoint
	const float Dot1 = VectToPoint | Segment;
	if (Dot1 <= 0)
	{
		return StartPoint;
	}

	// See if closest point is beyond EndPoint
	const float Dot2 = Segment | Segment;
	if (Dot2 <= Dot1)
	{
		return EndPoint;
	}

	// Closest Point is within segment
	return StartPoint + Segment * (Dot1 / Dot2);
}

float FMath::PointDistToSegment(const FVector &Point, const FVector &StartPoint, const FVector &EndPoint) 
{
	const FVector ClosestPoint = ClosestPointOnSegment(Point, StartPoint, EndPoint);
	return (Point - ClosestPoint).Size();
}

float FMath::PointDistToSegmentSquared(const FVector &Point, const FVector &StartPoint, const FVector &EndPoint) 
{
	const FVector ClosestPoint = ClosestPointOnSegment(Point, StartPoint, EndPoint);
	return (Point - ClosestPoint).SizeSquared();
}

void FMath::SegmentDistToSegmentSafe(FVector A1, FVector B1, FVector A2, FVector B2, FVector& OutP1, FVector& OutP2)
{
	// Segments
	const	FVector	S1		= B1 - A1;
	const	FVector	S2		= B2 - A2;
	const	FVector	S3		= A1 - A2;


	const	FVector	S1_norm		= S1.GetSafeNormal();
	const	FVector	S2_norm		= S2.GetSafeNormal();

	const	float	Dot11	= S1 | S1;	// always >= 0
	const	float	Dot22	= S2 | S2;	// always >= 0
	const	float	Dot12	= S1 | S2;
	const	float	Dot13	= S1 | S3;
	const	float	Dot23	= S2 | S3;

	const	float	Dot11_norm	= S1_norm | S1_norm;	// always >= 0
	const	float	Dot22_norm	= S2_norm | S2_norm;	// always >= 0
	const	float	Dot12_norm	= S1_norm | S2_norm;

	// Numerator
	float	N1, N2;

	// Denominator
	const	float	D		= Dot11*Dot22 - Dot12*Dot12;	// always >= 0
	const	float	D_norm	= Dot11_norm*Dot22_norm - Dot12_norm*Dot12_norm;	// always >= 0

	float	D1		= D;		// T1 = N1 / D1, default D1 = D >= 0
	float	D2		= D;		// T2 = N2 / D2, default D2 = D >= 0

	// compute the line parameters of the two closest points
	if( D < KINDA_SMALL_NUMBER || D_norm < KINDA_SMALL_NUMBER ) 
	{ 
		// the lines are almost parallel
		N1 = 0.f;	// force using point A on segment S1
		D1 = 1.f;	// to prevent possible division by 0 later
		N2 = Dot23;
		D2 = Dot22;
	}
	else 
	{                
		// get the closest points on the infinite lines
		N1 = (Dot12*Dot23 - Dot22*Dot13);
		N2 = (Dot11*Dot23 - Dot12*Dot13);

		if( N1 < 0.f ) 
		{
			// t1 < 0.f => the s==0 edge is visible
			N1 = 0.f;
			N2 = Dot23;
			D2 = Dot22;
		}
		else if( N1 > D1 ) 
		{
			// t1 > 1 => the t1==1 edge is visible
			N1 = D1;
			N2 = Dot23 + Dot12;
			D2 = Dot22;
		}
	}

	if( N2 < 0.f ) 
	{           
		// t2 < 0 => the t2==0 edge is visible
		N2 = 0.f;

		// recompute t1 for this edge
		if( -Dot13 < 0.f )
		{
			N1 = 0.f;
		}
		else if( -Dot13 > Dot11 )
		{
			N1 = D1;
		}
		else 
		{
			N1 = -Dot13;
			D1 = Dot11;
		}
	}
	else if( N2 > D2 ) 
	{      
		// t2 > 1 => the t2=1 edge is visible
		N2 = D2;

		// recompute t1 for this edge
		if( (-Dot13 + Dot12) < 0.f )
		{
			N1 = 0.f;
		}
		else if( (-Dot13 + Dot12) > Dot11 )
		{
			N1 = D1;
		}
		else 
		{
			N1 = (-Dot13 + Dot12);
			D1 = Dot11;
		}
	}

	// finally do the division to get the points' location
	const float T1 = (FMath::Abs(N1) < KINDA_SMALL_NUMBER ? 0.f : N1 / D1);
	const float T2 = (FMath::Abs(N2) < KINDA_SMALL_NUMBER ? 0.f : N2 / D2);

	// return the closest points
	OutP1 = A1 + T1 * S1;
	OutP2 = A2 + T2 * S2;
}

void FMath::SegmentDistToSegment(FVector A1, FVector B1, FVector A2, FVector B2, FVector& OutP1, FVector& OutP2)
{
	// Segments
	const	FVector	S1		= B1 - A1;
	const	FVector	S2		= B2 - A2;
	const	FVector	S3		= A1 - A2;

	const	float	Dot11	= S1 | S1;	// always >= 0
	const	float	Dot22	= S2 | S2;	// always >= 0
	const	float	Dot12	= S1 | S2;
	const	float	Dot13	= S1 | S3;
	const	float	Dot23	= S2 | S3;

	// Numerator
			float	N1, N2;

	// Denominator
	const	float	D		= Dot11*Dot22 - Dot12*Dot12;	// always >= 0
			float	D1		= D;		// T1 = N1 / D1, default D1 = D >= 0
			float	D2		= D;		// T2 = N2 / D2, default D2 = D >= 0

	// compute the line parameters of the two closest points
	if( D < KINDA_SMALL_NUMBER ) 
	{ 
		// the lines are almost parallel
		N1 = 0.f;	// force using point A on segment S1
		D1 = 1.f;	// to prevent possible division by 0 later
		N2 = Dot23;
		D2 = Dot22;
	}
	else 
	{                
		// get the closest points on the infinite lines
		N1 = (Dot12*Dot23 - Dot22*Dot13);
		N2 = (Dot11*Dot23 - Dot12*Dot13);

		if( N1 < 0.f ) 
		{
			// t1 < 0.f => the s==0 edge is visible
			N1 = 0.f;
			N2 = Dot23;
			D2 = Dot22;
		}
		else if( N1 > D1 ) 
		{
			// t1 > 1 => the t1==1 edge is visible
			N1 = D1;
			N2 = Dot23 + Dot12;
			D2 = Dot22;
		}
	}

	if( N2 < 0.f ) 
	{           
		// t2 < 0 => the t2==0 edge is visible
		N2 = 0.f;

		// recompute t1 for this edge
		if( -Dot13 < 0.f )
		{
			N1 = 0.f;
		}
		else if( -Dot13 > Dot11 )
		{
			N1 = D1;
		}
		else 
		{
			N1 = -Dot13;
			D1 = Dot11;
		}
	}
	else if( N2 > D2 ) 
	{      
		// t2 > 1 => the t2=1 edge is visible
		N2 = D2;

		// recompute t1 for this edge
		if( (-Dot13 + Dot12) < 0.f )
		{
			N1 = 0.f;
		}
		else if( (-Dot13 + Dot12) > Dot11 )
		{
			N1 = D1;
		}
		else 
		{
			N1 = (-Dot13 + Dot12);
			D1 = Dot11;
		}
	}

	// finally do the division to get the points' location
	const float T1 = (FMath::Abs(N1) < KINDA_SMALL_NUMBER ? 0.f : N1 / D1);
	const float T2 = (FMath::Abs(N2) < KINDA_SMALL_NUMBER ? 0.f : N2 / D2);

	// return the closest points
	OutP1 = A1 + T1 * S1;
	OutP2 = A2 + T2 * S2;
}

float FMath::GetTForSegmentPlaneIntersect(const FVector& StartPoint, const FVector& EndPoint, const FPlane& Plane)
{
	return ( Plane.W - (StartPoint|Plane) ) / ( (EndPoint - StartPoint)|Plane);	
}

bool FMath::SegmentPlaneIntersection(const FVector& StartPoint, const FVector& EndPoint, const FPlane& Plane, FVector& out_IntersectPoint)
{
	float T = FMath::GetTForSegmentPlaneIntersect(StartPoint,EndPoint,Plane);
	// If the parameter value is not between 0 and 1, there is no intersection
	if( T > -KINDA_SMALL_NUMBER && T < 1.f+KINDA_SMALL_NUMBER )
	{
		out_IntersectPoint = StartPoint + T * (EndPoint - StartPoint);
		return true;
	}
	return false;
}


/**
 * Compute the screen bounds of a point light along one axis.
 * Based on http://www.gamasutra.com/features/20021011/lengyel_06.htm
 * and http://sourceforge.net/mailarchive/message.php?msg_id=10501105
 */
static bool ComputeProjectedSphereShaft(
	float LightX,
	float LightZ,
	float Radius,
	const FMatrix& ProjMatrix,
	const FVector& Axis,
	float AxisSign,
	int32& InOutMinX,
	int32& InOutMaxX
	)
{
	float ViewX = InOutMinX;
	float ViewSizeX = InOutMaxX - InOutMinX;

	// Vertical planes: T = <Nx, 0, Nz, 0>
	float Discriminant = (FMath::Square(LightX) - FMath::Square(Radius) + FMath::Square(LightZ)) * FMath::Square(LightZ);
	if(Discriminant >= 0)
	{
		float SqrtDiscriminant = FMath::Sqrt(Discriminant);
		float InvLightSquare = 1.0f / (FMath::Square(LightX) + FMath::Square(LightZ));

		float Nxa = (Radius * LightX - SqrtDiscriminant) * InvLightSquare;
		float Nxb = (Radius * LightX + SqrtDiscriminant) * InvLightSquare;
		float Nza = (Radius - Nxa * LightX) / LightZ;
		float Nzb = (Radius - Nxb * LightX) / LightZ;
		float Pza = LightZ - Radius * Nza;
		float Pzb = LightZ - Radius * Nzb;

		// Tangent a
		if(Pza > 0)
		{
			float Pxa = -Pza * Nza / Nxa;
			FVector4 P = ProjMatrix.TransformFVector4(FVector4(Axis.X * Pxa,Axis.Y * Pxa,Pza,1));
			float X = (Dot3(P,Axis) / P.W + 1.0f * AxisSign) / 2.0f * AxisSign;
			if(FMath::IsNegativeFloat(Nxa) ^ FMath::IsNegativeFloat(AxisSign))
			{
				InOutMaxX = FMath::Min<int64>(FMath::CeilToInt(ViewSizeX * X + ViewX),InOutMaxX);
			}
			else
			{
				InOutMinX = FMath::Max<int64>(FMath::FloorToInt(ViewSizeX * X + ViewX),InOutMinX);
			}
		}

		// Tangent b
		if(Pzb > 0)
		{
			float Pxb = -Pzb * Nzb / Nxb;
			FVector4 P = ProjMatrix.TransformFVector4(FVector4(Axis.X * Pxb,Axis.Y * Pxb,Pzb,1));
			float X = (Dot3(P,Axis) / P.W + 1.0f * AxisSign) / 2.0f * AxisSign;
			if(FMath::IsNegativeFloat(Nxb) ^ FMath::IsNegativeFloat(AxisSign))
			{
				InOutMaxX = FMath::Min<int64>(FMath::CeilToInt(ViewSizeX * X + ViewX),InOutMaxX);
			}
			else
			{
				InOutMinX = FMath::Max<int64>(FMath::FloorToInt(ViewSizeX * X + ViewX),InOutMinX);
			}
		}
	}

	return InOutMinX <= InOutMaxX;
}

uint32 FMath::ComputeProjectedSphereScissorRect(FIntRect& InOutScissorRect, FVector SphereOrigin, float Radius, FVector ViewOrigin, const FMatrix& ViewMatrix, const FMatrix& ProjMatrix)
{
	// Calculate a scissor rectangle for the light's radius.
	if((SphereOrigin - ViewOrigin).Size() > Radius)
	{
		FVector LightVector = ViewMatrix.TransformPosition(SphereOrigin);

		if(!ComputeProjectedSphereShaft(
			LightVector.X,
			LightVector.Z,
			Radius,
			ProjMatrix,
			FVector(+1,0,0),
			+1,
			InOutScissorRect.Min.X,
			InOutScissorRect.Max.X))
		{
			return 0;
		}

		if(!ComputeProjectedSphereShaft(
			LightVector.Y,
			LightVector.Z,
			Radius,
			ProjMatrix,
			FVector(0,+1,0),
			-1,
			InOutScissorRect.Min.Y,
			InOutScissorRect.Max.Y))
		{
			return 0;
		}

		return 1;
	}
	else
	{
		return 2;
	}
}

bool FMath::PlaneAABBIntersection(const FPlane& P, const FBox& AABB)
{
	// find diagonal most closely aligned with normal of plane
	FVector Vmin, Vmax;

	// Bypass the slow FVector[] operator. Not RESTRICT because it won't update Vmin, Vmax
	float* VminPtr = (float*)&Vmin;
	float* VmaxPtr = (float*)&Vmax;

	// Use restrict to get better instruction scheduling and to bypass the slow FVector[] operator
	const float* RESTRICT AABBMinPtr = (const float*)&AABB.Min;
	const float* RESTRICT AABBMaxPtr = (const float*)&AABB.Max;
	const float* RESTRICT PlanePtr = (const float*)&P;

	for(int32 Idx=0;Idx<3;++Idx)
	{
		if(PlanePtr[Idx] >= 0.f)
		{
			VminPtr[Idx] = AABBMinPtr[Idx];
			VmaxPtr[Idx] = AABBMaxPtr[Idx];
		}
		else
		{
			VminPtr[Idx] = AABBMaxPtr[Idx];
			VmaxPtr[Idx] = AABBMinPtr[Idx]; 
		}
	}

	// if either diagonal is right on the plane, or one is on either side we have an interesection
	float dMax = P.PlaneDot(Vmax);
	float dMin = P.PlaneDot(Vmin);

	// if Max is below plane, or Min is above we know there is no intersection.. otherwise there must be one
	return (dMax >= 0.f && dMin <= 0.f);
}

bool FMath::SphereConeIntersection(const FVector& SphereCenter, float SphereRadius, const FVector& ConeAxis, float ConeAngleSin, float ConeAngleCos)
{
	/**
	 * from http://www.geometrictools.com/Documentation/IntersectionSphereCone.pdf
	 * (Copyright c 1998-2008. All Rights Reserved.) http://www.geometrictools.com (boost license)
	 */

	// the following code assumes the cone tip is at 0,0,0 (means the SphereCenter is relative to the cone tip)

	FVector U = ConeAxis * (-SphereRadius / ConeAngleSin);
	FVector D = SphereCenter - U;
	float dsqr = D | D;
	float e = ConeAxis | D;

	if(e > 0 && e * e >= dsqr * FMath::Square(ConeAngleCos))
	{
		dsqr = SphereCenter |SphereCenter;
		e = -ConeAxis | SphereCenter;
		if(e > 0 && e*e >= dsqr * FMath::Square(ConeAngleSin))
		{
			return dsqr <= FMath::Square(SphereRadius);
		}
		else
		{
			return true;
		}
	}
	return false;
}

FVector FMath::ClosestPointOnTriangleToPoint(const FVector& Point, const FVector& A, const FVector& B, const FVector& C)
{
	//Figure out what region the point is in and compare against that "point" or "edge"
	const FVector BA = A - B;
	const FVector AC = C - A;
	const FVector CB = B - C;
	const FVector TriNormal = BA ^ CB;

	// Get the planes that define this triangle
	// edges BA, AC, BC with normals perpendicular to the edges facing outward
	const FPlane Planes[3] = { FPlane(B, TriNormal ^ BA), FPlane(A, TriNormal ^ AC), FPlane(C, TriNormal ^ CB) };
	int32 PlaneHalfspaceBitmask = 0;

	//Determine which side of each plane the test point exists
	for (int32 i=0; i<3; i++)
	{
		if (Planes[i].PlaneDot(Point) > 0.0f)
		{
			PlaneHalfspaceBitmask |= (1 << i);
		}
	}

	FVector Result(Point.X, Point.Y, Point.Z);
	switch (PlaneHalfspaceBitmask)
	{
	case 0: //000 Inside
		return FVector::PointPlaneProject(Point, A, B, C);
	case 1:	//001 Segment BA
		Result = FMath::ClosestPointOnSegment(Point, B, A);
		break;
	case 2:	//010 Segment AC
		Result = FMath::ClosestPointOnSegment(Point, A, C);
		break;
	case 3:	//011 point A
		return A;
	case 4: //100 Segment BC
		Result = FMath::ClosestPointOnSegment(Point, B, C);
		break;
	case 5: //101 point B
		return B;
	case 6: //110 point C
		return C;
	default:
		UE_LOG(LogUnrealMath, Log, TEXT("Impossible result in FMath::ClosestPointOnTriangleToPoint"));
		break;
	}

	return Result;
}

FVector FMath::GetBaryCentric2D(const FVector& Point, const FVector& A, const FVector& B, const FVector& C)
{
	float a = ((B.Y-C.Y)*(Point.X-C.X) + (C.X-B.X)*(Point.Y-C.Y)) / ((B.Y-C.Y)*(A.X-C.X) + (C.X-B.X)*(A.Y-C.Y));
	float b = ((C.Y-A.Y)*(Point.X-C.X) + (A.X-C.X)*(Point.Y-C.Y)) / ((B.Y-C.Y)*(A.X-C.X) + (C.X-B.X)*(A.Y-C.Y));

	return FVector(a, b, 1.0f - a - b);	
}

FVector FMath::ComputeBaryCentric2D(const FVector& Point, const FVector& A, const FVector& B, const FVector& C)
{
	// Compute the normal of the triangle
	const FVector TriNorm = (B-A) ^ (C-A);

	//check collinearity of A,B,C
	check(TriNorm.SizeSquared() > SMALL_NUMBER && "Collinear points in FMath::ComputeBaryCentric2D()");

	const FVector N = TriNorm.GetSafeNormal();

	// Compute twice area of triangle ABC
	const float AreaABCInv = 1.0f / (N | TriNorm);

	// Compute a contribution
	const float AreaPBC = N | ((B-Point) ^ (C-Point));
	const float a = AreaPBC * AreaABCInv;

	// Compute b contribution
	const float AreaPCA = N | ((C-Point) ^ (A-Point));
	const float b = AreaPCA * AreaABCInv;

	// Compute c contribution
	return FVector(a, b, 1.0f - a - b);
}

FVector4 FMath::ComputeBaryCentric3D(const FVector& Point, const FVector& A, const FVector& B, const FVector& C, const FVector& D)
{	
	//http://www.devmaster.net/wiki/Barycentric_coordinates
	//Pick A as our origin and
	//Setup three basis vectors AB, AC, AD
	const FVector B1 = (B-A);
	const FVector B2 = (C-A);
	const FVector B3 = (D-A);

	//check co-planarity of A,B,C,D
	check( fabsf(B1 | (B2 ^ B3)) > SMALL_NUMBER && "Coplanar points in FMath::ComputeBaryCentric3D()");

	//Transform Point into this new space
	const FVector V = (Point - A);

	//Create a matrix of linearly independent vectors
	const FMatrix SolvMat(B1, B2, B3, FVector::ZeroVector);

	//The point V can be expressed as Ax=v where x is the vector containing the weights {w1...wn}
	//Solve for x by multiplying both sides by AInv   (AInv * A)x = AInv * v ==> x = AInv * v
	const FMatrix InvSolvMat = SolvMat.Inverse();
	const FPlane BaryCoords = InvSolvMat.TransformVector(V);	 

	//Reorder the weights to be a, b, c, d
	return FVector4(1.0f - BaryCoords.X - BaryCoords.Y - BaryCoords.Z, BaryCoords.X, BaryCoords.Y, BaryCoords.Z);
}

FVector FMath::ClosestPointOnTetrahedronToPoint(const FVector& Point, const FVector& A, const FVector& B, const FVector& C, const FVector& D)
{
	//Check for coplanarity of all four points
	check( fabsf((C-A) | ((B-A)^(D-C))) > 0.0001f && "Coplanar points in FMath::ComputeBaryCentric3D()");

	//http://osdir.com/ml/games.devel.algorithms/2003-02/msg00394.html
	//     D
	//    /|\		  C-----------B
	//   / | \		   \         /
	//  /  |  \	   or	\  \A/	/
    // C   |   B		 \	|  /
	//  \  |  /			  \	| /
    //   \ | /			   \|/
	//     A				D
	
	// Figure out the ordering (is D in the direction of the CCW triangle ABC)
	FVector Pt1(A),Pt2(B),Pt3(C),Pt4(D);
 	const FPlane ABC(A,B,C);
 	if (ABC.PlaneDot(D) < 0.0f)
 	{
 		//Swap two points to maintain CCW orders
 		Pt3 = D;
 		Pt4 = C;
 	}
		
	//Tetrahedron made up of 4 CCW faces - DCA, DBC, DAB, ACB
	const FPlane Planes[4] = {FPlane(Pt4,Pt3,Pt1), FPlane(Pt4,Pt2,Pt3), FPlane(Pt4,Pt1,Pt2), FPlane(Pt1,Pt3,Pt2)};

	//Determine which side of each plane the test point exists
	int32 PlaneHalfspaceBitmask = 0;
	for (int32 i=0; i<4; i++)
	{
		if (Planes[i].PlaneDot(Point) > 0.0f)
		{
			PlaneHalfspaceBitmask |= (1 << i);
		}
	}

	//Verts + Faces - Edges = 2	(Euler)
	FVector Result(Point.X, Point.Y, Point.Z);
	switch (PlaneHalfspaceBitmask)
	{
	case 0:	 //inside (0000)
		//@TODO - could project point onto any face
		break;
	case 1:	 //0001 Face	DCA
		return FMath::ClosestPointOnTriangleToPoint(Point, Pt4, Pt3, Pt1);
	case 2:	 //0010 Face	DBC
		return FMath::ClosestPointOnTriangleToPoint(Point, Pt4, Pt2, Pt3);
	case 3:  //0011	Edge	DC
		Result = FMath::ClosestPointOnSegment(Point, Pt4, Pt3);
		break;
	case 4:	 //0100 Face	DAB
		return FMath::ClosestPointOnTriangleToPoint(Point, Pt4, Pt1, Pt2);
	case 5:  //0101	Edge	DA
		Result = FMath::ClosestPointOnSegment(Point, Pt4, Pt1);
		break;
	case 6:  //0110	Edge	DB
		Result = FMath::ClosestPointOnSegment(Point, Pt4, Pt2);
		break;
	case 7:	 //0111 Point	D
		return Pt4;
	case 8:	 //1000 Face	ACB
		return FMath::ClosestPointOnTriangleToPoint(Point, Pt1, Pt3, Pt2);
	case 9:  //1001	Edge	AC	
		Result = FMath::ClosestPointOnSegment(Point, Pt1, Pt3);
		break;
	case 10: //1010	Edge	BC
		Result = FMath::ClosestPointOnSegment(Point, Pt2, Pt3);
		break;
	case 11: //1011 Point	C
		return Pt3;
	case 12: //1100	Edge	BA
		Result = FMath::ClosestPointOnSegment(Point, Pt2, Pt1);
		break;
	case 13: //1101 Point	A
		return Pt1;
	case 14: //1110 Point	B
		return Pt2;
	default: //impossible (1111)
		UE_LOG(LogUnrealMath, Log, TEXT("FMath::ClosestPointOnTetrahedronToPoint() : impossible result"));
		break;
	}

	return Result;
}

void FMath::SphereDistToLine(FVector SphereOrigin, float SphereRadius, FVector LineOrigin, FVector LineDir, FVector& OutClosestPoint)
{
	const float A	= LineDir | LineDir;
	const float B	= 2.f * (LineDir | (LineOrigin - SphereOrigin));
	const float C	= (SphereOrigin|SphereOrigin) + (LineOrigin|LineOrigin) - 2.f *(SphereOrigin|LineOrigin) - FMath::Square(SphereRadius);
	const float D	= FMath::Square(B) - 4.f * A * C;

	if( D <= KINDA_SMALL_NUMBER )
	{
		// line is not intersecting sphere (or is tangent at one point if D == 0 )
		const FVector PointOnLine = LineOrigin + ( -B / 2.f * A ) * LineDir;
		OutClosestPoint = SphereOrigin + (PointOnLine - SphereOrigin).GetSafeNormal() * SphereRadius;
	}
	else
	{
		// Line intersecting sphere in 2 points. Pick closest to line origin.
		const float	E	= FMath::Sqrt(D);
		const float T1	= (-B + E) / (2.f * A);
		const float T2	= (-B - E) / (2.f * A);
		const float T	= FMath::Abs(T1) < FMath::Abs(T2) ? T1 : T2;

		OutClosestPoint	= LineOrigin + T * LineDir;
	}
}

bool FMath::GetDistanceWithinConeSegment(FVector Point, FVector ConeStartPoint, FVector ConeLine, float RadiusAtStart, float RadiusAtEnd, float &PercentageOut)
{
	check(RadiusAtStart >= 0.0f && RadiusAtEnd >= 0.0f && ConeLine.SizeSquared() > 0);
	// -- First we'll draw out a line from the ConeStartPoint down the ConeLine. We'll find the closest point on that line to Point.
	//    If we're outside the max distance, or behind the StartPoint, we bail out as that means we've no chance to be in the cone.

	FVector PointOnCone; // Stores the point on the cone's center line closest to our target point.

	const float Distance = FMath::PointDistToLine(Point, ConeLine, ConeStartPoint, PointOnCone); // distance is how far from the viewline we are

	PercentageOut = 0.0; // start assuming we're outside cone until proven otherwise.

	const FVector VectToStart = ConeStartPoint - PointOnCone;
	const FVector VectToEnd = (ConeStartPoint + ConeLine) - PointOnCone;
	
	const float ConeLengthSqr = ConeLine.SizeSquared();
	const float DistToStartSqr = VectToStart.SizeSquared();
	const float DistToEndSqr = VectToEnd.SizeSquared();

	if (DistToStartSqr > ConeLengthSqr || DistToEndSqr > ConeLengthSqr)
	{
		//Outside cone
		return false;
	}

	const float PercentAlongCone = FMath::Sqrt(DistToStartSqr) / FMath::Sqrt(ConeLengthSqr); // don't have to catch outside 0->1 due to above code (saves 2 sqrts if outside)
	const float RadiusAtPoint = RadiusAtStart + ((RadiusAtEnd - RadiusAtStart) * PercentAlongCone);

	if(Distance > RadiusAtPoint) // target is farther from the line than the radius at that distance)
		return false;

	PercentageOut = RadiusAtPoint > 0.0f ? (RadiusAtPoint - Distance) / RadiusAtPoint : 1.0f;

	return true;
}

bool FMath::PointsAreCoplanar(const TArray<FVector>& Points, const float Tolerance)
{
	//less than 4 points = coplanar
	if (Points.Num() < 4)
	{
		return true;
	}

	//Get the Normal for plane determined by first 3 points
	const FVector Normal = FVector::CrossProduct(Points[2] - Points[0], Points[1] - Points[0]).GetSafeNormal();

	const int32 Total = Points.Num();
	for (int32 v = 3; v < Total; v++)
	{
		//Abs of PointPlaneDist, dist should be 0
		if (FMath::Abs(FVector::PointPlaneDist(Points[v], Points[0], Normal)) > Tolerance)
		{
			return false;
		}
	}

	return true;
}

bool FMath::GetDotDistance
( 
			FVector2D	&OutDotDist, 
	const	FVector		&Direction, 
	const	FVector		&AxisX, 
	const	FVector		&AxisY, 
	const	FVector		&AxisZ 	
)
{
	const FVector NormalDir = Direction.GetSafeNormal();

	// Find projected point (on AxisX and AxisY, remove AxisZ component)
	const FVector NoZProjDir = (NormalDir - (NormalDir | AxisZ) * AxisZ).GetSafeNormal();
	
	// Figure out if projection is on right or left.
	const float AzimuthSign = ( (NoZProjDir | AxisY) < 0.f ) ? -1.f : 1.f;

	OutDotDist.Y = NormalDir | AxisZ;
	const float DirDotX	= NoZProjDir | AxisX;
	OutDotDist.X = AzimuthSign * FMath::Abs(DirDotX);

	return (DirDotX >= 0.f );
}

FVector2D FMath::GetAzimuthAndElevation
(
	const FVector &Direction, 
	const FVector &AxisX, 
	const FVector &AxisY, 
	const FVector &AxisZ 	
)
{
	const FVector NormalDir = Direction.GetSafeNormal();
	// Find projected point (on AxisX and AxisY, remove AxisZ component)
	const FVector NoZProjDir = (NormalDir - (NormalDir | AxisZ) * AxisZ).GetSafeNormal();
	// Figure out if projection is on right or left.
	const float AzimuthSign = ((NoZProjDir | AxisY) < 0.f) ? -1.f : 1.f;
	const float ElevationSin = NormalDir | AxisZ;
	const float AzimuthCos = NoZProjDir | AxisX;

	// Convert to Angles in Radian.
	return FVector2D(FMath::Acos(AzimuthCos) * AzimuthSign, FMath::Asin(ElevationSin));
}

CORE_API FVector FMath::VInterpNormalRotationTo(const FVector& Current, const FVector& Target, float DeltaTime, float RotationSpeedDegrees)
{
	// Find delta rotation between both normals.
	FQuat DeltaQuat = FQuat::FindBetween(Current, Target);

	// Decompose into an axis and angle for rotation
	FVector DeltaAxis(0.f);
	float DeltaAngle = 0.f;
	DeltaQuat.ToAxisAndAngle(DeltaAxis, DeltaAngle);

	// Find rotation step for this frame
	const float RotationStepRadians = RotationSpeedDegrees * (PI / 180) * DeltaTime;

	if( FMath::Abs(DeltaAngle) > RotationStepRadians )
	{
		DeltaAngle = FMath::Clamp(DeltaAngle, -RotationStepRadians, RotationStepRadians);
		DeltaQuat = FQuat(DeltaAxis, DeltaAngle);
		return FQuatRotationTranslationMatrix( DeltaQuat, FVector::ZeroVector ).TransformVector(Current);
	}
	return Target;
}

CORE_API FVector FMath::VInterpConstantTo(const FVector Current, const FVector& Target, float DeltaTime, float InterpSpeed)
{
	const FVector Delta = Target - Current;
	const float DeltaM = Delta.Size();
	const float MaxStep = InterpSpeed * DeltaTime;

	if( DeltaM > MaxStep )
	{
		if( MaxStep > 0.f )
		{
			const FVector DeltaN = Delta / DeltaM;
			return Current + DeltaN * MaxStep;
		}
		else
		{
			return Current;
		}
	}

	return Target;
}

CORE_API FVector FMath::VInterpTo( const FVector& Current, const FVector& Target, float DeltaTime, float InterpSpeed )
{
	// If no interp speed, jump to target value
	if( InterpSpeed <= 0.f )
	{
		return Target;
	}

	// Distance to reach
	const FVector Dist = Target - Current;

	// If distance is too small, just set the desired location
	if( Dist.SizeSquared() < KINDA_SMALL_NUMBER )
	{
		return Target;
	}

	// Delta Move, Clamp so we do not over shoot.
	const FVector	DeltaMove = Dist * FMath::Clamp<float>(DeltaTime * InterpSpeed, 0.f, 1.f);

	return Current + DeltaMove;
}

CORE_API FVector2D FMath::Vector2DInterpConstantTo( const FVector2D& Current, const FVector2D& Target, float DeltaTime, float InterpSpeed )
{
	const FVector2D Delta = Target - Current;
	const float DeltaM = Delta.Size();
	const float MaxStep = InterpSpeed * DeltaTime;

	if( DeltaM > MaxStep )
	{
		if( MaxStep > 0.f )
		{
			const FVector2D DeltaN = Delta / DeltaM;
			return Current + DeltaN * MaxStep;
		}
		else
		{
			return Current;
		}
	}

	return Target;
}

CORE_API FVector2D FMath::Vector2DInterpTo( const FVector2D& Current, const FVector2D& Target, float DeltaTime, float InterpSpeed )
{
	if( InterpSpeed <= 0.f )
	{
		return Target;
	}

	const FVector2D Dist = Target - Current;
	if( Dist.SizeSquared() < KINDA_SMALL_NUMBER )
	{
		return Target;
	}

	const FVector2D DeltaMove = Dist * FMath::Clamp<float>(DeltaTime * InterpSpeed, 0.f, 1.f);
	return Current + DeltaMove;
}

CORE_API FRotator FMath::RInterpConstantTo( const FRotator& Current, const FRotator& Target, float DeltaTime, float InterpSpeed )
{
	// if DeltaTime is 0, do not perform any interpolation (Location was already calculated for that frame)
	if( DeltaTime == 0.f || Current == Target )
	{
		return Current;
	}

	// If no interp speed, jump to target value
	if( InterpSpeed <= 0.f )
	{
		return Target;
	}

	const float DeltaInterpSpeed = InterpSpeed * DeltaTime;
	
	const FRotator DeltaMove = (Target - Current).GetNormalized();
	FRotator Result = Current;
	Result.Pitch += FMath::Clamp(DeltaMove.Pitch, -DeltaInterpSpeed, DeltaInterpSpeed);
	Result.Yaw += FMath::Clamp(DeltaMove.Yaw, -DeltaInterpSpeed, DeltaInterpSpeed);
	Result.Roll += FMath::Clamp(DeltaMove.Roll, -DeltaInterpSpeed, DeltaInterpSpeed);
	return Result.GetNormalized();
}

CORE_API FRotator FMath::RInterpTo( const FRotator& Current, const FRotator& Target, float DeltaTime, float InterpSpeed)
{
	// if DeltaTime is 0, do not perform any interpolation (Location was already calculated for that frame)
	if( DeltaTime == 0.f || Current == Target )
	{
		return Current;
	}

	// If no interp speed, jump to target value
	if( InterpSpeed <= 0.f )
	{
		return Target;
	}

	const float DeltaInterpSpeed = InterpSpeed * DeltaTime;

	const FRotator Delta = (Target - Current).GetNormalized();
	
	// If steps are too small, just return Target and assume we have reached our destination.
	if (Delta.IsNearlyZero())
	{
		return Target;
	}

	// Delta Move, Clamp so we do not over shoot.
	const FRotator DeltaMove = Delta * FMath::Clamp<float>(DeltaInterpSpeed, 0.f, 1.f);
	return (Current + DeltaMove).GetNormalized();
}

CORE_API float FMath::FInterpTo( float Current, float Target, float DeltaTime, float InterpSpeed )
{
	// If no interp speed, jump to target value
	if( InterpSpeed == 0.f )
	{
		return Target;
	}

	// Distance to reach
	const float Dist = Target - Current;

	// If distance is too small, just set the desired location
	if( FMath::Square(Dist) < SMALL_NUMBER )
	{
		return Target;
	}

	// Delta Move, Clamp so we do not over shoot.
	const float DeltaMove = Dist * FMath::Clamp<float>(DeltaTime * InterpSpeed, 0.f, 1.f);

	return Current + DeltaMove;
}

CORE_API float FMath::FInterpConstantTo( float Current, float Target, float DeltaTime, float InterpSpeed )
{
	const float Dist = Target - Current;

	// If distance is too small, just set the desired location
	if( FMath::Square(Dist) < SMALL_NUMBER )
	{
		return Target;
	}

	const float Step = InterpSpeed * DeltaTime;
	return Current + FMath::Clamp<float>(Dist, -Step, Step);
}

/** Interpolate Linear Color from Current to Target. Scaled by distance to Target, so it has a strong start speed and ease out. */
CORE_API FLinearColor FMath::CInterpTo(const FLinearColor& Current, const FLinearColor& Target, float DeltaTime, float InterpSpeed)
{
	// If no interp speed, jump to target value
	if (InterpSpeed <= 0.f)
	{
		return Target;
	}

	// Difference between colors
	const float Dist = FLinearColor::Dist(Target, Current);

	// If distance is too small, just set the desired color
	if (Dist < KINDA_SMALL_NUMBER)
	{
		return Target;
	}

	// Delta change, Clamp so we do not over shoot.
	const FLinearColor DeltaMove = (Target - Current) * FMath::Clamp<float>(DeltaTime * InterpSpeed, 0.f, 1.f);

	return Current + DeltaMove;
}

CORE_API float ClampFloatTangent( float PrevPointVal, float PrevTime, float CurPointVal, float CurTime, float NextPointVal, float NextTime )
{
	const float PrevToNextTimeDiff = FMath::Max< double >( KINDA_SMALL_NUMBER, NextTime - PrevTime );
	const float PrevToCurTimeDiff = FMath::Max< double >( KINDA_SMALL_NUMBER, CurTime - PrevTime );
	const float CurToNextTimeDiff = FMath::Max< double >( KINDA_SMALL_NUMBER, NextTime - CurTime );

	float OutTangentVal = 0.0f;

	const float PrevToNextHeightDiff = NextPointVal - PrevPointVal;
	const float PrevToCurHeightDiff = CurPointVal - PrevPointVal;
	const float CurToNextHeightDiff = NextPointVal - CurPointVal;

	// Check to see if the current point is crest
	if( ( PrevToCurHeightDiff >= 0.0f && CurToNextHeightDiff <= 0.0f ) ||
		( PrevToCurHeightDiff <= 0.0f && CurToNextHeightDiff >= 0.0f ) )
	{
		// Neighbor points are both both on the same side, so zero out the tangent
		OutTangentVal = 0.0f;
	}
	else
	{
		// The three points form a slope

		// Constants
		const float ClampThreshold = 0.333f;

		// Compute height deltas
		const float CurToNextTangent = CurToNextHeightDiff / CurToNextTimeDiff;
		const float PrevToCurTangent = PrevToCurHeightDiff / PrevToCurTimeDiff;
		const float PrevToNextTangent = PrevToNextHeightDiff / PrevToNextTimeDiff;

		// Default to not clamping
		const float UnclampedTangent = PrevToNextTangent;
		float ClampedTangent = UnclampedTangent;

		const float LowerClampThreshold = ClampThreshold;
		const float UpperClampThreshold = 1.0f - ClampThreshold;

		// @todo: Would we get better results using percentange of TIME instead of HEIGHT?
		const float CurHeightAlpha = PrevToCurHeightDiff / PrevToNextHeightDiff;

		if( PrevToNextHeightDiff > 0.0f )
		{
			if( CurHeightAlpha < LowerClampThreshold )
			{
				// 1.0 = maximum clamping (flat), 0.0 = minimal clamping (don't touch)
				const float ClampAlpha = 1.0f - CurHeightAlpha / ClampThreshold;
				const float LowerClamp = FMath::Lerp( PrevToNextTangent, PrevToCurTangent, ClampAlpha );
				ClampedTangent = FMath::Min( ClampedTangent, LowerClamp );
			}

			if( CurHeightAlpha > UpperClampThreshold )
			{
				// 1.0 = maximum clamping (flat), 0.0 = minimal clamping (don't touch)
				const float ClampAlpha = ( CurHeightAlpha - UpperClampThreshold ) / ClampThreshold;
				const float UpperClamp = FMath::Lerp( PrevToNextTangent, CurToNextTangent, ClampAlpha );
				ClampedTangent = FMath::Min( ClampedTangent, UpperClamp );
			}
		}
		else
		{

			if( CurHeightAlpha < LowerClampThreshold )
			{
				// 1.0 = maximum clamping (flat), 0.0 = minimal clamping (don't touch)
				const float ClampAlpha = 1.0f - CurHeightAlpha / ClampThreshold;
				const float LowerClamp = FMath::Lerp( PrevToNextTangent, PrevToCurTangent, ClampAlpha );
				ClampedTangent = FMath::Max( ClampedTangent, LowerClamp );
			}

			if( CurHeightAlpha > UpperClampThreshold )
			{
				// 1.0 = maximum clamping (flat), 0.0 = minimal clamping (don't touch)
				const float ClampAlpha = ( CurHeightAlpha - UpperClampThreshold ) / ClampThreshold;
				const float UpperClamp = FMath::Lerp( PrevToNextTangent, CurToNextTangent, ClampAlpha );
				ClampedTangent = FMath::Max( ClampedTangent, UpperClamp );
			}
		}

		OutTangentVal = ClampedTangent;
	}

	return OutTangentVal;
}

FVector FMath::VRandCone(FVector const& Dir, float ConeHalfAngleRad)
{
	if (ConeHalfAngleRad > 0.f)
	{
		float const RandU = FMath::FRand();
		float const RandV = FMath::FRand();

		// Get spherical coords that have an even distribution over the unit sphere
		// Method described at http://mathworld.wolfram.com/SpherePointPicking.html	
		float Theta = 2.f * PI * RandU;
		float Phi = FMath::Acos((2.f * RandV) - 1.f);

		// restrict phi to [0, ConeHalfAngleRad]
		// this gives an even distribution of points on the surface of the cone
		// centered at the origin, pointing upward (z), with the desired angle
		Phi = FMath::Fmod(Phi, ConeHalfAngleRad);

		// get axes we need to rotate around
		FMatrix const DirMat = FRotationMatrix(Dir.Rotation());
		// note the axis translation, since we want the variation to be around X
		FVector const DirZ = DirMat.GetScaledAxis( EAxis::X );		
		FVector const DirY = DirMat.GetScaledAxis( EAxis::Y );

		FVector Result = Dir.RotateAngleAxis(Phi * 180.f / PI, DirY);
		Result = Result.RotateAngleAxis(Theta * 180.f / PI, DirZ);

		// ensure it's a unit vector (might not have been passed in that way)
		Result = Result.GetSafeNormal();
		
		return Result;
	}
	else
	{
		return Dir.GetSafeNormal();
	}
}

FVector FMath::VRandCone(FVector const& Dir, float HorizontalConeHalfAngleRad, float VerticalConeHalfAngleRad)
{
	if ( (VerticalConeHalfAngleRad > 0.f) && (HorizontalConeHalfAngleRad > 0.f) )
	{
		float const RandU = FMath::FRand();
		float const RandV = FMath::FRand();

		// Get spherical coords that have an even distribution over the unit sphere
		// Method described at http://mathworld.wolfram.com/SpherePointPicking.html	
		float Theta = 2.f * PI * RandU;
		float Phi = FMath::Acos((2.f * RandV) - 1.f);

		// restrict phi to [0, ConeHalfAngleRad]
		// where ConeHalfAngleRad is now a function of Theta
		// (specifically, radius of an ellipse as a function of angle)
		// function is ellipse function (x/a)^2 + (y/b)^2 = 1, converted to polar coords
		float ConeHalfAngleRad = FMath::Square(FMath::Cos(Theta) / VerticalConeHalfAngleRad) + FMath::Square(FMath::Sin(Theta) / HorizontalConeHalfAngleRad);
		ConeHalfAngleRad = FMath::Sqrt(1.f / ConeHalfAngleRad);

		// clamp to make a cone instead of a sphere
		Phi = FMath::Fmod(Phi, ConeHalfAngleRad);

		// get axes we need to rotate around
		FMatrix const DirMat = FRotationMatrix(Dir.Rotation());
		// note the axis translation, since we want the variation to be around X
		FVector const DirZ = DirMat.GetScaledAxis( EAxis::X );		
		FVector const DirY = DirMat.GetScaledAxis( EAxis::Y );

		FVector Result = Dir.RotateAngleAxis(Phi * 180.f / PI, DirY);
		Result = Result.RotateAngleAxis(Theta * 180.f / PI, DirZ);

		// ensure it's a unit vector (might not have been passed in that way)
		Result = Result.GetSafeNormal();

		return Result;
	}
	else
	{
		return Dir.GetSafeNormal();
	}
}

FVector FMath::RandPointInBox(const FBox& Box)
{
	return FVector(	FRandRange(Box.Min.X, Box.Max.X),
					FRandRange(Box.Min.Y, Box.Max.Y),
					FRandRange(Box.Min.Z, Box.Max.Z) );
}

FVector FMath::GetReflectionVector(const FVector& Direction, const FVector& SurfaceNormal)
{
	return Direction - 2 * (Direction | SurfaceNormal.GetSafeNormal()) * SurfaceNormal.GetSafeNormal();
}

struct FClusterMovedHereToMakeCompile
{
	FVector ClusterPosAccum;
	int32 ClusterSize;
};

void FVector::GenerateClusterCenters(TArray<FVector>& Clusters, const TArray<FVector>& Points, int32 NumIterations, int32 NumConnectionsToBeValid)
{
	// Check we have >0 points and clusters
	if(Points.Num() == 0 || Clusters.Num() == 0)
	{
		return;
	}

	// Temp storage for each cluster that mirrors the order of the passed in Clusters array
	TArray<FClusterMovedHereToMakeCompile> ClusterData;
	ClusterData.AddZeroed( Clusters.Num() );

	// Then iterate
	for(int32 ItCount=0; ItCount<NumIterations; ItCount++)
	{
		// Classify each point - find closest cluster center
		for(int32 i=0; i<Points.Num(); i++)
		{
			const FVector& Pos = Points[i];

			// Iterate over all clusters to find closes one
			int32 NearestClusterIndex = INDEX_NONE;
			float NearestClusterDist = BIG_NUMBER;
			for(int32 j=0; j<Clusters.Num() ; j++)
			{
				const float Dist = (Pos - Clusters[j]).Size();
				if(Dist < NearestClusterDist)
				{
					NearestClusterDist = Dist;
					NearestClusterIndex = j;
				}
			}
			// Update its info with this point
			if( NearestClusterIndex != INDEX_NONE )
			{
				ClusterData[NearestClusterIndex].ClusterPosAccum += Pos;
				ClusterData[NearestClusterIndex].ClusterSize++;
			}
		}

		// All points classified - update cluster center as average of membership
		for(int32 i=0; i<Clusters.Num(); i++)
		{
			if(ClusterData[i].ClusterSize > 0)
			{
				Clusters[i] = ClusterData[i].ClusterPosAccum / (float)ClusterData[i].ClusterSize;
			}
		}
	}

	// so now after we have possible cluster centers we want to remove the ones that are outliers and not part of the main cluster
	for(int32 i=0; i<ClusterData.Num(); i++)
	{
		if(ClusterData[i].ClusterSize < NumConnectionsToBeValid)
		{
			Clusters.RemoveAt(i);
		}
	}
}

bool FMath::MemoryTest( void* BaseAddress, uint32 NumBytes )
{
	volatile uint32* Ptr;
	uint32 NumDwords = NumBytes / 4;
	uint32 TestWords[2] = { 0xdeadbeef, 0x1337c0de };
	bool bSucceeded = true;

	for ( int32 TestIndex=0; TestIndex < 2; ++TestIndex )
	{
		// Fill the memory with a pattern.
		Ptr = (uint32*) BaseAddress;
		for ( uint32 Index=0; Index < NumDwords; ++Index )
		{
			*Ptr = TestWords[TestIndex];
			Ptr++;
		}

		// Check that each uint32 is still ok and overwrite it with the complement.
		Ptr = (uint32*) BaseAddress;
		for ( uint32 Index=0; Index < NumDwords; ++Index )
		{
			if ( *Ptr != TestWords[TestIndex] )
			{
				FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Failed memory test at 0x%08x, wrote: 0x%08x, read: 0x%08x\n"), Ptr, TestWords[TestIndex], *Ptr );
				bSucceeded = false;
			}
			*Ptr = ~TestWords[TestIndex];
			Ptr++;
		}

		// Check again, now going backwards in memory.
		Ptr = ((uint32*) BaseAddress) + NumDwords;
		for ( uint32 Index=0; Index < NumDwords; ++Index )
		{
			Ptr--;
			if ( *Ptr != ~TestWords[TestIndex] )
			{
				FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Failed memory test at 0x%08x, wrote: 0x%08x, read: 0x%08x\n"), Ptr, ~TestWords[TestIndex], *Ptr );
				bSucceeded = false;
			}
			*Ptr = TestWords[TestIndex];
		}
	}

	return bSucceeded;
}

/**
 * Converts a string to it's numeric equivalent, ignoring whitespace.
 * "123  45" - becomes 12,345
 *
 * @param	Value	The string to convert.
 * @return			The converted value.
 */
float Val(const FString& Value)
{
	float RetValue = 0;

	for( int32 x = 0 ; x < Value.Len() ; x++ )
	{
		FString Char = Value.Mid(x, 1);

		if( Char >= TEXT("0") && Char <= TEXT("9") )
		{
			RetValue *= 10;
			RetValue += FCString::Atoi( *Char );
		}
		else 
		{
			if( Char != TEXT(" ") )
			{
				break;
			}
		}
	}

	return RetValue;
}

FString GrabChar( FString* pStr )
{
	FString GrabChar;
	if( pStr->Len() )
	{
		do
		{		
			GrabChar = pStr->Left(1);
			*pStr = pStr->Mid(1);
		} while( GrabChar == TEXT(" ") );
	}
	else
	{
		GrabChar = TEXT("");
	}

	return GrabChar;
}

bool SubEval( FString* pStr, float* pResult, int32 Prec )
{
	FString c;
	float V, W, N;

	V = W = N = 0.0f;

	c = GrabChar(pStr);

	if( (c >= TEXT("0") && c <= TEXT("9")) || c == TEXT(".") )	// Number
	{
		V = 0;
		while(c >= TEXT("0") && c <= TEXT("9"))
		{
			V = V * 10 + Val(c);
			c = GrabChar(pStr);
		}

		if( c == TEXT(".") )
		{
			N = 0.1f;
			c = GrabChar(pStr);

			while(c >= TEXT("0") && c <= TEXT("9"))
			{
				V = V + N * Val(c);
				N = N / 10.0f;
				c = GrabChar(pStr);
			}
		}
	}
	else if( c == TEXT("("))									// Opening parenthesis
	{
		if( !SubEval(pStr, &V, 0) )
		{
			return 0;
		}
		c = GrabChar(pStr);
	}
	else if( c == TEXT("-") )									// Negation
	{
		if( !SubEval(pStr, &V, 1000) )
		{
			return 0;
		}
		V = -V;
		c = GrabChar(pStr);
	}
	else if( c == TEXT("+"))									// Positive
	{
		if( !SubEval(pStr, &V, 1000) )
		{
			return 0;
		}
		c = GrabChar(pStr);
	}
	else if( c == TEXT("@") )									// Square root
	{
		if( !SubEval(pStr, &V, 1000) )
		{
			return 0;
		}

		if( V < 0 )
		{
			UE_LOG(LogUnrealMath, Log, TEXT("Expression Error : Can't take square root of negative number"));
			return 0;
		}
		else
		{
			V = FMath::Sqrt(V);
		}

		c = GrabChar(pStr);
	}
	else														// Error
	{
		UE_LOG(LogUnrealMath, Log, TEXT("Expression Error : No value recognized"));
		return 0;
	}
PrecLoop:
	if( c == TEXT("") )
	{
		*pResult = V;
		return 1;
	}
	else if( c == TEXT(")") )
	{
		*pStr = FString(TEXT(")")) + *pStr;
		*pResult = V;
		return 1;
	}
	else if( c == TEXT("+") )
	{
		if( Prec > 1 )
		{
			*pResult = V;
			*pStr = c + *pStr;
			return 1;
		}
		else
		{
			if( SubEval(pStr, &W, 2) )
			{
				V = V + W;
				c = GrabChar(pStr);
				goto PrecLoop;
			}
			else
			{
				return 0;
			}
		}
	}
	else if( c == TEXT("-") )
	{
		if( Prec > 1 )
		{
			*pResult = V;
			*pStr = c + *pStr;
			return 1;
		}
		else
		{
			if( SubEval(pStr, &W, 2) )
			{
				V = V - W;
				c = GrabChar(pStr);
				goto PrecLoop;
			}
			else
			{
				return 0;
			}
		}
	}
	else if( c == TEXT("/") )
	{
		if( Prec > 2 )
		{
			*pResult = V;
			*pStr = c + *pStr;
			return 1;
		}
		else
		{
			if( SubEval(pStr, &W, 3) )
			{
				if( W == 0 )
				{
					UE_LOG(LogUnrealMath, Log, TEXT("Expression Error : Division by zero isn't allowed"));
					return 0;
				}
				else
				{
					V = V / W;
					c = GrabChar(pStr);
					goto PrecLoop;
				}
			}
			else
			{
				return 0;
			}
		}
	}
	else if( c == TEXT("%") )
	{
		if( Prec > 2 )
		{
			*pResult = V;
			*pStr = c + *pStr;
			return 1;
		}
		else
		{
			if( SubEval(pStr, &W, 3) )
			{
				if( W == 0 )
				{
					UE_LOG(LogUnrealMath, Log, TEXT("Expression Error : Modulo zero isn't allowed"));
					return 0;
				}
				else
				{
					V = (int32)V % (int32)W;
					c = GrabChar(pStr);
					goto PrecLoop;
				}
			}
			else
			{
				return 0;
			}
		}
	}
	else if( c == TEXT("*") )
	{
		if( Prec > 3 )
		{
			*pResult = V;
			*pStr = c + *pStr;
			return 1;
		}
		else
		{
			if( SubEval(pStr, &W, 4) )
			{
				V = V * W;
				c = GrabChar(pStr);
				goto PrecLoop;
			}
			else
			{
				return 0;
			}
		}
	}
	else
	{
		UE_LOG(LogUnrealMath, Log, TEXT("Expression Error : Unrecognized Operator"));
	}

	*pResult = V;
	return 1;
}

bool FMath::Eval( FString Str, float& OutValue )
{
	bool bResult = true;

	// Check for a matching number of brackets right up front.
	int32 Brackets = 0;
	for( int32 x = 0 ; x < Str.Len() ; x++ )
	{
		if( Str.Mid(x,1) == TEXT("(") )
		{
			Brackets++;
		}
		if( Str.Mid(x,1) == TEXT(")") )
		{
			Brackets--;
		}
	}

	if( Brackets != 0 )
	{
		UE_LOG(LogUnrealMath, Log, TEXT("Expression Error : Mismatched brackets"));
		bResult = false;
	}

	else
	{
		if( !SubEval( &Str, &OutValue, 0 ) )
		{
			UE_LOG(LogUnrealMath, Log, TEXT("Expression Error : Error in expression"));
			bResult = false;
		}
	}

	return bResult;
}

float FMath::FixedTurn(float InCurrent, float InDesired, float InDeltaRate)
{
	if (InDeltaRate == 0.f)
	{
		return FRotator::ClampAxis(InCurrent);
	}

	float result = FRotator::ClampAxis(InCurrent);
	InCurrent = result;
	InDesired = FRotator::ClampAxis(InDesired);

	if (InCurrent > InDesired)
	{
		if (InCurrent - InDesired < 180.f)
			result -= FMath::Min((InCurrent - InDesired), FMath::Abs(InDeltaRate));
		else
			result += FMath::Min((InDesired + 360.f - InCurrent), FMath::Abs(InDeltaRate));
	}
	else
	{
		if (InDesired - InCurrent < 180.f)
			result += FMath::Min((InDesired - InCurrent), FMath::Abs(InDeltaRate));
		else
			result -= FMath::Min((InCurrent + 360.f - InDesired), FMath::Abs(InDeltaRate));
	}
	return FRotator::ClampAxis(result);
}

float FMath::ClampAngle(float AngleDegrees, float MinAngleDegrees, float MaxAngleDegrees)
{
	float const MaxDelta = FRotator::ClampAxis(MaxAngleDegrees - MinAngleDegrees) * 0.5f;			// 0..180
	float const RangeCenter = FRotator::ClampAxis(MinAngleDegrees + MaxDelta);						// 0..360
	float const DeltaFromCenter = FRotator::NormalizeAxis(AngleDegrees - RangeCenter);				// -180..180

	// maybe clamp to nearest edge
	if (DeltaFromCenter > MaxDelta)
	{
		return FRotator::NormalizeAxis(RangeCenter + MaxDelta);
	}
	else if (DeltaFromCenter < -MaxDelta)
	{
		return FRotator::NormalizeAxis(RangeCenter - MaxDelta);
	}

	// already in range, just return it
	return FRotator::NormalizeAxis(AngleDegrees);
}

void FMath::ApplyScaleToFloat(float& Dst, const FVector& DeltaScale, float Magnitude)
{
	const float Multiplier = ( DeltaScale.X > 0.0f || DeltaScale.Y > 0.0f || DeltaScale.Z > 0.0f ) ? Magnitude : -Magnitude;
	Dst += Multiplier * DeltaScale.Size();
	Dst = FMath::Max( 0.0f, Dst );
}