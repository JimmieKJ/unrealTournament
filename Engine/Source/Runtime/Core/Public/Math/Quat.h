// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


/**
 * Floating point quaternion that can represent a rotation about an axis in 3-D space.
 * The X, Y, Z, W components also double as the Axis/Angle format.
 *
 * Order matters when composing quaternions: C = A * B will yield a quaternion C that logically
 * first applies B then A to any subsequent transformation (right first, then left).
 * Note that this is the opposite order of FTransform multiplication.
 *
 * Example: LocalToWorld = (LocalToWorld * DeltaRotation) will change rotation in local space by DeltaRotation.
 * Example: LocalToWorld = (DeltaRotation * LocalToWorld) will change rotation in world space by DeltaRotation.
 */
MS_ALIGN(16) struct FQuat 
{
public:

	/** Holds the quaternion's X-component. */
	float X;

	/** Holds the quaternion's Y-component. */
	float Y;

	/** Holds the quaternion's Z-component. */
	float Z;

	/** Holds the quaternion's W-component. */
	float W;

public:

	//Identity quaternion
	static CORE_API const FQuat Identity;

public:

	/**
	 * Default constructor (no initialization).
	 */
	FORCEINLINE FQuat() { }

	/**
	 * Creates and initializes a new quaternion, with the W component either 0 or 1.
	 *
	 * @param EForceInit Force init enum: if equal to ForceInitToZero then W is 0, otherwise W = 1 (creating an identity transform)
	 */
	explicit FORCEINLINE FQuat( EForceInit );

	/**
	 * Constructor.
	 *
	 * @param InX X component of the quaternion
	 * @param InY Y component of the quaternion
	 * @param InZ Z component of the quaternion
	 * @param InW W component of the quaternion
	 */
	FORCEINLINE FQuat( float InX, float InY, float InZ, float InW );

	/**
	 * Copy constructor.
	 *
	 * @param Q A FQuat object to use to create new quaternion from.
	 */
	FORCEINLINE FQuat( const FQuat& Q );
	
	/**
	 * Creates and initializes a new quaternion from the given matrix.
	 *
	 * @param M The rotation matrix to initialize from.
	 */
	explicit FQuat( const FMatrix& M );	

	/**
	 * Creates and initializes a new quaternion from the given rotator.
	 *
	 * @param R The rotator to initialize from.
	 */
	explicit FQuat( const FRotator& R);

	/**
	 * Creates and initializes a new quaternion from the a rotation around the given axis.
	 *
	 * @param Axis assumed to be a normalized vector
	 * @param Angle angle to rotate above the given axis (in radians)
	 */
	FQuat( FVector Axis, float AngleRad );

public:

#ifdef IMPLEMENT_ASSIGNMENT_OPERATOR_MANUALLY
	/**
	 * Copy another FQuat into this one
	 *
	 * @return reference to this FQuat
	 */
	FORCEINLINE FQuat& operator=(const FQuat& Other);
#endif

	/**
	 * Gets the result of adding a Quaternion to this.
	 * This is a component-wise addition; composing quaternions should be done via multiplication.
	 *
	 * @param Q The Quaternion to add.
	 * @return The result of addition.
	 */
	FORCEINLINE FQuat operator+( const FQuat& Q ) const;

	/**
	 * Adds to this quaternion.
	 * This is a component-wise addition; composing quaternions should be done via multiplication.
	 *
	 * @param Other The quaternion to add to this.
	 * @return Result after addition.
	 */
	FORCEINLINE FQuat operator+=(const FQuat& Q);

	/**
	 * Gets the result of subtracting a Quaternion to this.
	 * This is a component-wise subtraction; composing quaternions should be done via multiplication.
	 *
	 * @param Q The Quaternion to subtract.
	 * @return The result of subtraction.
	 */
	FORCEINLINE FQuat operator-( const FQuat& Q ) const;

	/**
	 * Checks whether another Quaternion is equal to this, within specified tolerance.
	 *
	 * @param Q The other Quaternion.
	 * @param Tolerance Error Tolerance.
	 * @return true if two Quaternion are equal, within specified tolerance, otherwise false.
	 */
	FORCEINLINE bool Equals(const FQuat& Q, float Tolerance=KINDA_SMALL_NUMBER) const;

	/**
	 * Subtracts another quaternion from this.
	 * This is a component-wise subtraction; composing quaternions should be done via multiplication.
	 *
	 * @param Q The other quaternion.
	 * @return reference to this after subtraction.
	 */
	FORCEINLINE FQuat operator-=(const FQuat& Q);

	/**
	 * Gets the result of multiplying this by another quaternion (this * Q).
	 *
	 * Order matters when composing quaternions: C = A * B will yield a quaternion C that logically
	 * first applies B then A to any subsequent transformation (right first, then left).
	 *
	 * @param Q The Quaternion to multiply this by.
	 * @return The result of multiplication (this * Q).
	 */
	FORCEINLINE FQuat operator*( const FQuat& Q ) const;

	/**
	 * Multiply this by a quaternion (this = this * Q).
	 *
	 * Order matters when composing quaternions: C = A * B will yield a quaternion C that logically
	 * first applies B then A to any subsequent transformation (right first, then left).
	 *
	 * @param Q the quaternion to multiply this by.
	 * @return The result of multiplication (this * Q).
	 */
	FORCEINLINE FQuat operator*=(const FQuat& Q);

	/**
	 * Rotate a vector by this quaternion.
	 *
	 * @param V the vector to be rotated
	 * @return vector after rotation
	 * @see RotateVector()
	 */
	FVector operator*( const FVector& V ) const;

	/** 
	 * Multiply this by a matrix.
	 * This matrix conversion came from
	 * http://www.m-hikari.com/ija/ija-password-2008/ija-password17-20-2008/aristidouIJA17-20-2008.pdf
	 * used for non-uniform scaling transform.
	 *
	 * @param M Matrix to multiply by.
	 * @return Matrix result after multiplication.
	 */	
	FMatrix operator*( const FMatrix& M ) const;
	
	/**
	 * Multiply this quaternion by a scaling factor.
	 *
	 * @param Scale The scaling factor.
	 * @return a reference to this after scaling.
	 */
	FORCEINLINE FQuat operator*=( const float Scale );

	/**
	 * Get the result of scaling this rotator.
	 *
	 * @param Scale The scaling factor.
	 * @return The result of scaling.
	 */
	FORCEINLINE FQuat operator*( const float Scale ) const;
	
	/**
	 * Divide this quaternion by scale.
	 *
	 * @param Scale What to divide by.
	 * @return new Quaternion of this after division by scale.
	 */
	FORCEINLINE FQuat operator/( const float Scale ) const;

 	/**
	 * Checks whether two quaternions are identical.
	 * This is an exact comparison per-component;see Equals() for a comparison
	 * that allows for a small error tolerance and flipped axes of rotation.
	 *
	 * @param Q The other quaternion.
	 * @return true if two quaternion are identical, otherwise false.
	 * @see Equals()
	 */
	bool operator==( const FQuat& Q ) const;

 	/**
	 * Checks whether two quaternions are not identical.
	 *
	 * @param Q The other quaternion.
	 * @return true if two quaternion are not identical, otherwise false.
	 */
	bool operator!=( const FQuat& Q ) const;

	/**
	 * Calculates dot product of two quaternions.
	 *
	 * @param Q The other quaternions.
	 * @return The dot product.
	 */
	float operator|( const FQuat& Q ) const;

public:

	/**
	 * Convert a vector of floating-point Euler angles (in degrees) into a Quaternion.
	 * 
	 * @param Euler the Euler angles
	 * @return constructed FQuat
	 */
	static CORE_API FQuat MakeFromEuler( const FVector& Euler );

	/** Convert a Quaternion into floating-point Euler angles (in degrees). */
	CORE_API FVector Euler() const;

	/**
	 * Normalize this quaternion if it is large enough.
	 *
	 * @param Tolerance Minimum squared length of vector for normalization.
	 */
	FORCEINLINE void Normalize( float Tolerance=SMALL_NUMBER );

	// Return true if this quaternion is normalized
	bool IsNormalized() const;

	/**
	 * Get the length of this quaternion.
	 *
	 * @return The length of this quaternion.
	 */
	FORCEINLINE float Size() const;

	/**
	 * Get the length squared of this quaternion.
	 *
	 * @return The length of this quaternion.
	 */
	FORCEINLINE float SizeSquared() const;

	/** 
	 * get the axis and angle of rotation of this quaternion
	 *
	 * @param Axis{out] vector of axis of the quaternion
	 * @param Angle{out] angle of the quaternion
	 * @warning : assumes normalized quaternions.
	 */
	void ToAxisAndAngle( FVector& Axis, float& Angle ) const;

	/**
	 * Rotate a vector by this quaternion.
	 *
	 * @param V the vector to be rotated
	 * @return vector after rotation
	 */
	FVector RotateVector( FVector V ) const;
	
	/**
	 * Rotate a vector by the inverse of this quaternion.
	 *
	 * @param V the vector to be rotated
	 * @return vector after rotation by the inverse of this quaternion.
	 */
	FVector UnrotateVector( FVector V ) const;

	/**
	 * @return quaternion with W=0 and V=theta*v.
	 */
	CORE_API FQuat Log() const;

	/**
	 * @note Exp should really only be used after Log.
	 * Assumes a quaternion with W=0 and V=theta*v (where |v| = 1).
	 * Exp(q) = (sin(theta)*v, cos(theta))
	 */
	CORE_API FQuat Exp() const;

	/**
	 * @return inverse of this quaternion
	 */
	FORCEINLINE FQuat Inverse() const;

	/**
	 * Enforce that the delta between this Quaternion and another one represents
	 * the shortest possible rotation angle
	 */
	void EnforceShortestArcWith( const FQuat& OtherQuat );
	
	/** Get X Rotation Axis. */
	FORCEINLINE FVector GetAxisX() const;

	/** Get Y Rotation Axis. */
	FORCEINLINE FVector GetAxisY() const;

	/** Get Z Rotation Axis. */
	FORCEINLINE FVector GetAxisZ() const;

	/** @return rotator representation of this quaternion */
	CORE_API FRotator Rotator() const;

	/** @return Vector of the axis of the quaternion */
	FORCEINLINE FVector GetRotationAxis() const;

	/**
	 * Serializes the vector compressed for e.g. network transmission.
	 * @param Ar Archive to serialize to/ from.
	 * @return false to allow the ordinary struct code to run (this never happens).
	 */
	CORE_API bool NetSerialize( FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess );

	/**
	 * Utility to check if there are any NaNs in this Quaternion.
	 *
	 * @return true if there are any NaNs in this Quaternion, otherwise false.
	 */
	bool ContainsNaN() const;

	/**
	 * Get a textual representation of the vector.
	 *
	 * @return Text describing the vector.
	 */
	FString ToString() const;

public:

#if ENABLE_NAN_DIAGNOSTIC
	FORCEINLINE void DiagnosticCheckNaN() const
	{
		checkf(!ContainsNaN(), TEXT("FQuat contains NaN: %s"), *ToString());
	}
#else
	FORCEINLINE void DiagnosticCheckNaN() const {}
#endif

public:

	/**
	 * Generates the 'smallest' (geodesic) rotation between these two vectors.
	 */
	static CORE_API FQuat FindBetween( const FVector& vec1, const FVector& vec2 );

	/**
	 * Error measure (angle) between two quaternions, ranged [0..1].
	 * Returns the hypersphere-angle between two quaternions; alignment shouldn't matter, though 
	 * @note normalized input is expected.
	 */
	static FORCEINLINE float Error( const FQuat& Q1, const FQuat& Q2 );

	/**
	 * FQuat::Error with auto-normalization.
	 */
	static FORCEINLINE float ErrorAutoNormalize( const FQuat& A, const FQuat& B );

	/** 
	 * Fast Linear Quaternion Interpolation.
	 * Result is NOT normalized.
	 */
	static FORCEINLINE FQuat FastLerp( const FQuat& A, const FQuat& B, const float Alpha );

	/** 
	 * Bi-Linear Quaternion interpolation.
	 * Result is NOT normalized.
	 */
	static FORCEINLINE FQuat FastBilerp( const FQuat& P00, const FQuat& P10, const FQuat& P01, const FQuat& P11, float FracX, float FracY );


	/** Spherical interpolation. Will correct alignment. Output is not normalized. */
	static CORE_API FQuat Slerp( const FQuat &Quat1,const FQuat &Quat2, float Slerp );

	/**
	 * Simpler Slerp that doesn't do any checks for 'shortest distance' etc.
	 * We need this for the cubic interpolation stuff so that the multiple Slerps dont go in different directions.
	 */
	static CORE_API FQuat SlerpFullPath( const FQuat &quat1, const FQuat &quat2, float Alpha );
	
	/** Given start and end quaternions of quat1 and quat2, and tangents at those points tang1 and tang2, calculate the point at Alpha (between 0 and 1) between them. */
	static CORE_API FQuat Squad( const FQuat& quat1, const FQuat& tang1, const FQuat& quat2, const FQuat& tang2, float Alpha );

	/** 
	 * Calculate tangents between given points
	 *
	 * @param PrevP quaternion at P-1
	 * @param P quaternion to return the tangent
	 * @param NextP quaternion P+1
	 * @param Tension @todo document
	 * @param OutTan Out control point
	 */
	static CORE_API void CalcTangents( const FQuat& PrevP, const FQuat& P, const FQuat& NextP, float Tension, FQuat& OutTan );

public:

	/**
	 * Serializes the quaternion.
	 *
	 * @param Ar Reference to the serialization archive.
	 * @param F Reference to the quaternion being serialized.
	 * @return Reference to the Archive after serialization.
	 */
	friend FArchive& operator<<( FArchive& Ar, FQuat& F )
	{
		return Ar << F.X << F.Y << F.Z << F.W;
	}

} GCC_ALIGN(16);


/* FQuat inline functions
 *****************************************************************************/

inline FQuat::FQuat( const FMatrix& M )
{
	// If Matrix is NULL, return Identity quaternion. If any of them is 0, you won't be able to construct rotation
	// if you have two plane at least, we can reconstruct the frame using cross product, but that's a bit expensive op to do here
	// for now, if you convert to matrix from 0 scale and convert back, you'll lose rotation. Don't do that. 
	if (M.GetScaledAxis(EAxis::X).IsNearlyZero() || M.GetScaledAxis(EAxis::Y).IsNearlyZero() || M.GetScaledAxis(EAxis::Z).IsNearlyZero())
	{
		*this = FQuat::Identity;
		return;
	}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	// Make sure the Rotation part of the Matrix is unit length.
	// Changed to this (same as RemoveScaling) from RotDeterminant as using two different ways of checking unit length matrix caused inconsistency. 
	if (!ensure((FMath::Abs(1.f - M.GetScaledAxis(EAxis::X).SizeSquared()) <= KINDA_SMALL_NUMBER) && (FMath::Abs(1.f - M.GetScaledAxis(EAxis::Y).SizeSquared()) <= KINDA_SMALL_NUMBER) && (FMath::Abs(1.f - M.GetScaledAxis(EAxis::Z).SizeSquared()) <= KINDA_SMALL_NUMBER)))
	{
		*this = FQuat::Identity;
		return;
	}
#endif

	//const MeReal *const t = (MeReal *) tm;
	float	s;

	// Check diagonal (trace)
	const float tr = M.M[0][0] + M.M[1][1] + M.M[2][2];

	if (tr > 0.0f) 
	{
		float InvS = FMath::InvSqrt(tr + 1.f);
		this->W = 0.5f * (1.f / InvS);
		s = 0.5f * InvS;

		this->X = (M.M[1][2] - M.M[2][1]) * s;
		this->Y = (M.M[2][0] - M.M[0][2]) * s;
		this->Z = (M.M[0][1] - M.M[1][0]) * s;
	} 
	else 
	{
		// diagonal is negative
		int32 i = 0;

		if (M.M[1][1] > M.M[0][0])
			i = 1;

		if (M.M[2][2] > M.M[i][i])
			i = 2;

		static const int32 nxt[3] = { 1, 2, 0 };
		const int32 j = nxt[i];
		const int32 k = nxt[j];
 
		s = M.M[i][i] - M.M[j][j] - M.M[k][k] + 1.0f;

		float InvS = FMath::InvSqrt(s);

		float qt[4];
		qt[i] = 0.5f * (1.f / InvS);

		s = 0.5f * InvS;

		qt[3] = (M.M[j][k] - M.M[k][j]) * s;
		qt[j] = (M.M[i][j] + M.M[j][i]) * s;
		qt[k] = (M.M[i][k] + M.M[k][i]) * s;

		this->X = qt[0];
		this->Y = qt[1];
		this->Z = qt[2];
		this->W = qt[3];

		DiagnosticCheckNaN();
	}
}


FORCEINLINE FQuat::FQuat( const FRotator& R )
{
	*this = R.Quaternion();
	DiagnosticCheckNaN();
}


FORCEINLINE FVector FQuat::operator*( const FVector& V ) const
{
	return RotateVector(V);
}


inline FMatrix FQuat::operator*( const FMatrix& M ) const
{
	FMatrix Result;
	FQuat VT, VR;
	FQuat Inv = Inverse();
	for (int32 I=0; I<4; ++I)
	{
		FQuat VQ(M.M[I][0], M.M[I][1], M.M[I][2], M.M[I][3]);
		VectorQuaternionMultiply(&VT, this, &VQ);
		VectorQuaternionMultiply(&VR, &VT, &Inv);
		Result.M[I][0] = VR.X;
		Result.M[I][1] = VR.Y;
		Result.M[I][2] = VR.Z;
		Result.M[I][3] = VR.W;
	}

	return Result;
}


/** now we directly convert from Rotator to Quaternion and vice versa. If you see issue with rotator, you can contact @LH or  
 *  you can undo this to see if this fixes issue. Feel free to enable this and see if that fixes the issue.
 */
#define USE_MATRIX_ROTATOR 0

#if USE_MATRIX_ROTATOR

#include "QuatRotationTranslationMatrix.h"

/** 
 * this is not right representation of if both rotation is equal or not
 * if you really like to equal, use matrix form. 
 * @warning DO NOT USE THIS To VERIFY if rotation is same. I'm only using this for the debug purpose
 */
FORCEINLINE bool DebugRotatorEquals(const FRotator& R1, const FRotator& R2, float Tolerance=1.f) 
{
	// also 0 and 360 should be considered same 
	return ( FMath::Abs(R1.Pitch-R2.Pitch) < Tolerance || FMath::Abs(R1.Pitch+R2.Pitch-360) < Tolerance )
		&& ( FMath::Abs(R1.Yaw-R2.Yaw) < Tolerance || FMath::Abs(R1.Yaw+R2.Yaw-360) < Tolerance )
		&& ( FMath::Abs(R1.Roll-R2.Roll) < Tolerance || FMath::Abs(R1.Roll+R2.Roll-360) < Tolerance );
}
#endif



/* FQuat inline functions
 *****************************************************************************/

FORCEINLINE FQuat::FQuat( EForceInit ZeroOrNot )
	:	X(0), Y(0), Z(0), W(ZeroOrNot == ForceInitToZero ? 0.0f : 1.0f)
{ }


FORCEINLINE FQuat::FQuat( float InX, float InY, float InZ, float InW )
	: X(InX)
	, Y(InY)
	, Z(InZ)
	, W(InW)
{
	DiagnosticCheckNaN();
}


FORCEINLINE FQuat::FQuat( const FQuat& Q )
	: X(Q.X)
	, Y(Q.Y)
	, Z(Q.Z)
	, W(Q.W)
{ }


FORCEINLINE FString FQuat::ToString() const
{
	return FString::Printf(TEXT("X=%3.3f Y=%3.3f Z=%3.3f W=%3.3f"), X, Y, Z, W);
}


#ifdef IMPLEMENT_ASSIGNMENT_OPERATOR_MANUALLY
FORCEINLINE FQuat& FQuat::operator=( const FQuat& Other )
{
	this->X = Other.X;
	this->Y = Other.Y;
	this->Z = Other.Z;
	this->W = Other.W;

	return *this;
}
#endif


FORCEINLINE FQuat::FQuat( FVector Axis, float AngleRad )
{
	const float half_a = 0.5f * AngleRad;
	float s, c;
	FMath::SinCos(&s, &c, half_a);

	X = s * Axis.X;
	Y = s * Axis.Y;
	Z = s * Axis.Z;
	W = c;

	DiagnosticCheckNaN();
}


FORCEINLINE FQuat FQuat::operator+( const FQuat& Q ) const
{		
	return FQuat( X + Q.X, Y + Q.Y, Z + Q.Z, W + Q.W );
}


FORCEINLINE FQuat FQuat::operator+=(const FQuat& Q)
{
	this->X += Q.X;
	this->Y += Q.Y;
	this->Z += Q.Z;
	this->W += Q.W;

	DiagnosticCheckNaN();

	return *this;
}


FORCEINLINE FQuat FQuat::operator-( const FQuat& Q ) const
{
	return FQuat( X - Q.X, Y - Q.Y, Z - Q.Z, W - Q.W );
}


FORCEINLINE bool FQuat::Equals(const FQuat& Q, float Tolerance) const
{
	return (FMath::Abs(X - Q.X) < Tolerance && FMath::Abs(Y - Q.Y) < Tolerance && FMath::Abs(Z - Q.Z) < Tolerance && FMath::Abs(W - Q.W) < Tolerance)
		|| (FMath::Abs(X + Q.X) < Tolerance && FMath::Abs(Y + Q.Y) < Tolerance && FMath::Abs(Z + Q.Z) < Tolerance && FMath::Abs(W + Q.W) < Tolerance);
}


FORCEINLINE FQuat FQuat::operator-=(const FQuat& Q)
{
	this->X -= Q.X;
	this->Y -= Q.Y;
	this->Z -= Q.Z;
	this->W -= Q.W;

	DiagnosticCheckNaN();

	return *this;
}


FORCEINLINE FQuat FQuat::operator*( const FQuat& Q ) const
{
	FQuat Result;
	VectorQuaternionMultiply(&Result, this, &Q);
	
	Result.DiagnosticCheckNaN();
	
	return Result;
}


FORCEINLINE FQuat FQuat::operator*=( const FQuat& Q )
{
	/**
	 * Now this uses VectorQuaternionMultiply that is optimized per platform.
	 */
	VectorRegister A = VectorLoadAligned(this);
	VectorRegister B = VectorLoadAligned(&Q);
	VectorRegister Result;
	VectorQuaternionMultiply(&Result, &A, &B);
	VectorStoreAligned(Result, this);

	DiagnosticCheckNaN();

	return *this; 
}


FORCEINLINE FQuat FQuat::operator*=( const float Scale )
{
	X *= Scale;
	Y *= Scale;
	Z *= Scale;
	W *= Scale;

	DiagnosticCheckNaN();

	return *this;
}


FORCEINLINE FQuat FQuat::operator*( const float Scale ) const
{
	return FQuat(Scale * X, Scale * Y, Scale * Z, Scale * W);
}


FORCEINLINE FQuat FQuat::operator/( const float Scale ) const
{
	return FQuat(X / Scale, Y / Scale, Z / Scale, W / Scale);
}


FORCEINLINE bool FQuat::operator==( const FQuat& Q ) const
{
	return X == Q.X && Y == Q.Y && Z == Q.Z && W == Q.W;
}


FORCEINLINE bool FQuat::operator!=( const FQuat& Q ) const
{
	return X != Q.X || Y != Q.Y || Z != Q.Z || W != Q.W;
}


FORCEINLINE float FQuat::operator|( const FQuat& Q ) const
{
	return X * Q.X + Y * Q.Y + Z * Q.Z + W * Q.W;
}


FORCEINLINE void FQuat::Normalize(float Tolerance)
{
	const float SquareSum = X * X + Y * Y + Z * Z + W * W;

	if (SquareSum > Tolerance)
	{
		const float Scale = FMath::InvSqrt(SquareSum);

		X *= Scale; 
		Y *= Scale; 
		Z *= Scale;
		W *= Scale;
	}
	else
	{
		*this = FQuat::Identity;
	}
}


FORCEINLINE bool FQuat::IsNormalized() const
{
	return (FMath::Abs(1.f - SizeSquared()) < THRESH_QUAT_NORMALIZED);
}


FORCEINLINE float FQuat::Size() const
{
	return FMath::Sqrt(X * X + Y * Y + Z * Z + W * W);
}


FORCEINLINE float FQuat::SizeSquared() const
{
	return (X * X + Y * Y + Z * Z + W * W);
}


FORCEINLINE void FQuat::ToAxisAndAngle( FVector& Axis, float& Angle ) const
{
	Angle = 2.f * FMath::Acos(W);
	Axis = GetRotationAxis();
}


FORCEINLINE FVector FQuat::GetRotationAxis() const
{
	// Ensure we never try to sqrt a neg number
	const float S = FMath::Sqrt(FMath::Max(1.f - (W * W), 0.f));

	if (S >= 0.0001f) 
	{ 
		return FVector(X / S, Y / S, Z / S);
	} 

	return FVector(1.f, 0.f, 0.f);
}


FORCEINLINE FVector FQuat::RotateVector( FVector V ) const
{	
#if WITH_DIRECTXMATH
	FVector Result;
	VectorQuaternionVector3Rotate(&Result, &V, this);
	return Result;

	/*
	// In unit testing this appears to be slower than the non-vectorized version.
#elif PLATFORM_ENABLE_VECTORINTRINSICS
	FQuat VQ(V.X, V.Y, V.Z, 0.f);
	FQuat VT, VR;
	FQuat I = Inverse();
	VectorQuaternionMultiply(&VT, this, &VQ);
	VectorQuaternionMultiply(&VR, &VT, &I);

	return FVector(VR.X, VR.Y, VR.Z);
	*/

#else
	// (q.W*q.W-qv.qv)v + 2(qv.v)qv + 2 q.W (qv x v)

	const FVector qv(X, Y, Z);
	FVector vOut = (2.f * W) * (qv ^ V);
	vOut += ((W * W) - (qv | qv)) * V;
	vOut += (2.f * (qv | V)) * qv;

	return vOut;
#endif
}

FORCEINLINE FVector FQuat::UnrotateVector( FVector V ) const
{	
#if WITH_DIRECTXMATH
	FVector Result;
	VectorQuaternionVector3InverseRotate(&Result, &V, this);
	return Result;
#else
	return Inverse().RotateVector(V);
#endif
}


FORCEINLINE FQuat FQuat::Inverse() const
{
	checkSlow(IsNormalized());

	return FQuat(-X, -Y, -Z, W);
}


FORCEINLINE void FQuat::EnforceShortestArcWith( const FQuat& OtherQuat )
{
	const float DotResult = (OtherQuat | *this);
	const float Bias = FMath::FloatSelect(DotResult, 1.0f, -1.0f);

	X *= Bias;
	Y *= Bias;
	Z *= Bias;
	W *= Bias;
}

	
FORCEINLINE FVector FQuat::GetAxisX() const
{
	return RotateVector(FVector(1.f, 0.f, 0.f));
}


FORCEINLINE FVector FQuat::GetAxisY() const
{
	return RotateVector(FVector(0.f, 1.f, 0.f));
}


FORCEINLINE FVector FQuat::GetAxisZ() const
{
	return RotateVector(FVector(0.f, 0.f, 1.f));
}


FORCEINLINE float FQuat::Error( const FQuat& Q1, const FQuat& Q2 )
{
	const float cosom = FMath::Abs(Q1.X * Q2.X + Q1.Y * Q2.Y + Q1.Z * Q2.Z + Q1.W * Q2.W);
	return (FMath::Abs(cosom) < 0.9999999f) ? FMath::Acos(cosom)*(1.f / PI) : 0.0f;
}


FORCEINLINE float FQuat::ErrorAutoNormalize( const FQuat& A, const FQuat& B )
{
	FQuat Q1 = A;
	Q1.Normalize();

	FQuat Q2 = B;
	Q2.Normalize();

	return FQuat::Error(Q1, Q2);
}


FORCEINLINE FQuat FQuat::FastLerp( const FQuat& A, const FQuat& B, const float Alpha )
{
	// To ensure the 'shortest route', we make sure the dot product between the both rotations is positive.
	const float DotResult = (A | B);
	const float Bias = FMath::FloatSelect(DotResult, 1.0f, -1.0f);
	return (B * Alpha) + (A * (Bias * (1.f - Alpha)));
}


FORCEINLINE FQuat FQuat::FastBilerp( const FQuat& P00, const FQuat& P10, const FQuat& P01, const FQuat& P11, float FracX, float FracY )
{
	return FQuat::FastLerp(
		FQuat::FastLerp(P00,P10,FracX),
		FQuat::FastLerp(P01,P11,FracX),
		FracY
	);
}


FORCEINLINE bool FQuat::ContainsNaN() const
{
	return (FMath::IsNaN(X) || !FMath::IsFinite(X) ||
			FMath::IsNaN(Y) || !FMath::IsFinite(Y) ||
			FMath::IsNaN(Z) || !FMath::IsFinite(Z) ||
			FMath::IsNaN(W) || !FMath::IsFinite(W)
	);
}


template<> struct TIsPODType<FQuat> { enum { Value = true }; };
