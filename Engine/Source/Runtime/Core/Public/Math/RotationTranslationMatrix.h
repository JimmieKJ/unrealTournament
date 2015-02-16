// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


/** Combined rotation and translation matrix */
class FRotationTranslationMatrix
	: public FMatrix
{
public:

	/**
	 * Constructor.
	 *
	 * @param Rot rotation
	 * @param Origin translation to apply
	 */
	FRotationTranslationMatrix(const FRotator& Rot, const FVector& Origin);

	/** Matrix factory. Return an FMatrix so we don't have type conversion issues in expressions. */
	static FMatrix Make(const FRotator& Rot, const FVector& Origin)
	{
		return FRotationTranslationMatrix(Rot, Origin);
	}
};


FORCEINLINE FRotationTranslationMatrix::FRotationTranslationMatrix(const FRotator& Rot, const FVector& Origin)
{
	const float	SR	= FMath::Sin(Rot.Roll * PI / 180.f);
	const float	SP	= FMath::Sin(Rot.Pitch * PI / 180.f);
	const float	SY	= FMath::Sin(Rot.Yaw * PI / 180.f);
	const float	CR	= FMath::Cos(Rot.Roll * PI / 180.f);
	const float	CP	= FMath::Cos(Rot.Pitch * PI / 180.f);
	const float	CY	= FMath::Cos(Rot.Yaw * PI / 180.f);

	M[0][0]	= CP * CY;
	M[0][1]	= CP * SY;
	M[0][2]	= SP;
	M[0][3]	= 0.f;

	M[1][0]	= SR * SP * CY - CR * SY;
	M[1][1]	= SR * SP * SY + CR * CY;
	M[1][2]	= - SR * CP;
	M[1][3]	= 0.f;

	M[2][0]	= -( CR * SP * CY + SR * SY );
	M[2][1]	= CY * SR - CR * SP * SY;
	M[2][2]	= CR * CP;
	M[2][3]	= 0.f;

	M[3][0]	= Origin.X;
	M[3][1]	= Origin.Y;
	M[3][2]	= Origin.Z;
	M[3][3]	= 1.f;
}
