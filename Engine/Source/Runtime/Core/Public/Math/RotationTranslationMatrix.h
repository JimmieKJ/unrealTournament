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
#if WITH_DIRECTXMATH	// Currently VectorSinCos() is only vectorized in DirectX implementation. Other platforms become slower if they use this.

	VectorRegister Angles = MakeVectorRegister(Rot.Roll, Rot.Pitch, Rot.Yaw, 0.0f);
	VectorRegister HalfAngles = VectorMultiply(Angles, GlobalVectorConstants::DEG_TO_RAD);

	union { VectorRegister v; float f[4]; } SinAngles, CosAngles;	
	VectorSinCos(&SinAngles.v, &CosAngles.v, &HalfAngles);

	const float	SR	= SinAngles.f[0];
	const float	SP	= SinAngles.f[1];
	const float	SY	= SinAngles.f[2];
	const float	CR	= CosAngles.f[0];
	const float	CP	= CosAngles.f[1];
	const float	CY	= CosAngles.f[2];

#else
	
	float SP, SY, SR;
	float CP, CY, CR;
	FMath::SinCos(&SP, &CP, FMath::DegreesToRadians(Rot.Pitch));
	FMath::SinCos(&SY, &CY, FMath::DegreesToRadians(Rot.Yaw));
	FMath::SinCos(&SR, &CR, FMath::DegreesToRadians(Rot.Roll));

#endif

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
