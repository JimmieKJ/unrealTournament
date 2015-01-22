// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#pragma once

/** A normal vector, quantized and packed into 32-bits. */
struct FPackedNormal
{
	union
	{
		struct
		{
#if PLATFORM_LITTLE_ENDIAN
			uint8	X,
			Y,
			Z,
			W;
#else
			uint8	W,
			Z,
			Y,
			X;
#endif
		};
		uint32		Packed;
	}				Vector;

	// Constructors.

	FPackedNormal() { Vector.Packed = 0; }
	FPackedNormal(uint32 InPacked) { Vector.Packed = InPacked; }
	FPackedNormal(const FVector& InVector) { *this = InVector; }
	FPackedNormal(uint8 InX, uint8 InY, uint8 InZ, uint8 InW) { Vector.X = InX; Vector.Y = InY; Vector.Z = InZ; Vector.W = InW; }

	// Conversion operators.

	void operator=(const FVector& InVector);
	void operator=(const FVector4& InVector);
	operator FVector() const;
	VectorRegister GetVectorRegister() const;

	// Set functions.
	void Set(const FVector& InVector) { *this = InVector; }

	// Equality operator.

	bool operator==(const FPackedNormal& B) const;
	bool operator!=(const FPackedNormal& B) const;

	// Serializer.

	friend RENDERCORE_API FArchive& operator<<(FArchive& Ar, FPackedNormal& N);

	FString ToString() const
	{
		return FString::Printf(TEXT("X=%d Y=%d Z=%d W=%d"), Vector.X, Vector.Y, Vector.Z, Vector.W);
	}

	// Zero Normal
	static RENDERCORE_API FPackedNormal ZeroNormal;
};

/** X=127.5, Y=127.5, Z=1/127.5f, W=-1.0 */
extern RENDERCORE_API const VectorRegister GVectorPackingConstants;

FORCEINLINE void FPackedNormal::operator=(const FVector& InVector)
{
	Vector.X = FMath::Clamp(FMath::TruncToInt(InVector.X * 127.5f + 127.5f),0,255);
	Vector.Y = FMath::Clamp(FMath::TruncToInt(InVector.Y * 127.5f + 127.5f),0,255);
	Vector.Z = FMath::Clamp(FMath::TruncToInt(InVector.Z * 127.5f + 127.5f),0,255);
	Vector.W = 128;
}

FORCEINLINE void FPackedNormal::operator=(const FVector4& InVector)
{
	Vector.X = FMath::Clamp(FMath::TruncToInt(InVector.X * 127.5f + 127.5f),0,255);
	Vector.Y = FMath::Clamp(FMath::TruncToInt(InVector.Y * 127.5f + 127.5f),0,255);
	Vector.Z = FMath::Clamp(FMath::TruncToInt(InVector.Z * 127.5f + 127.5f),0,255);
	Vector.W = FMath::Clamp(FMath::TruncToInt(InVector.W * 127.5f + 127.5f),0,255);
}

FORCEINLINE bool FPackedNormal::operator==(const FPackedNormal& B) const
{
	if(Vector.Packed != B.Vector.Packed)
		return 0;

	FVector	V1 = *this,
			V2 = B;

	if(FMath::Abs(V1.X - V2.X) > THRESH_NORMALS_ARE_SAME * 4.0f)
		return 0;

	if(FMath::Abs(V1.Y - V2.Y) > THRESH_NORMALS_ARE_SAME * 4.0f)
		return 0;

	if(FMath::Abs(V1.Z - V2.Z) > THRESH_NORMALS_ARE_SAME * 4.0f)
		return 0;

	return 1;
}

FORCEINLINE bool FPackedNormal::operator!=(const FPackedNormal& B) const
{
	if(Vector.Packed == B.Vector.Packed)
		return 0;

	FVector	V1 = *this,
			V2 = B;

	if(FMath::Abs(V1.X - V2.X) > THRESH_NORMALS_ARE_SAME * 4.0f)
		return 1;

	if(FMath::Abs(V1.Y - V2.Y) > THRESH_NORMALS_ARE_SAME * 4.0f)
		return 1;

	if(FMath::Abs(V1.Z - V2.Z) > THRESH_NORMALS_ARE_SAME * 4.0f)
		return 1;

	return 0;
}

FORCEINLINE FPackedNormal::operator FVector() const
{
	VectorRegister VectorToUnpack = GetVectorRegister();
	// Write to FVector and return it.
	FVector UnpackedVector;
	VectorStoreFloat3( VectorToUnpack, &UnpackedVector );
	return UnpackedVector;
}

FORCEINLINE VectorRegister FPackedNormal::GetVectorRegister() const
{
	// Rescale [0..255] range to [-1..1]
	VectorRegister VectorToUnpack		= VectorLoadByte4( this );
	VectorToUnpack						= VectorMultiplyAdd( VectorToUnpack, VectorReplicate(GVectorPackingConstants,2), VectorReplicate(GVectorPackingConstants,3) );
	VectorResetFloatRegisters();
	// Return unpacked vector register.
	return VectorToUnpack;
}
