// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CorePrivatePCH.h" 


DEFINE_LOG_CATEGORY_STATIC(LogUnrealMathTest, Log, All);

// Create some temporary storage variables
MS_ALIGN( 16 ) static float GScratch[16] GCC_ALIGN( 16 );
static float GSum;
static bool GPassing;


/**
 * Tests if two vectors (xyzw) are bitwise equal
 *
 * @param Vec0 First vector
 * @param Vec1 Second vector
 *
 * @return true if equal
 */
bool TestVectorsEqualBitwise( VectorRegister Vec0, VectorRegister Vec1)
{
	VectorStoreAligned( Vec0, GScratch + 0 );
	VectorStoreAligned( Vec1, GScratch + 4 );

	const bool Passed = (memcmp(GScratch + 0, GScratch + 4, sizeof(float) * 4) == 0);

	GPassing = GPassing && Passed;
	return Passed;
}


/**
 * Tests if two vectors (xyzw) are equal within an optional tolerance
 *
 * @param Vec0 First vector
 * @param Vec1 Second vector
 * @param Tolerance Error allowed for the comparison
 *
 * @return true if equal(ish)
 */
bool TestVectorsEqual( VectorRegister Vec0, VectorRegister Vec1, float Tolerance = 0.0f)
{
	VectorStoreAligned( Vec0, GScratch + 0 );
	VectorStoreAligned( Vec1, GScratch + 4 );
	GSum = 0.f;
	for ( int32 Component = 0; Component < 4; Component++ ) 
	{
		float Diff = GScratch[ Component + 0 ] - GScratch[ Component + 4 ];
		GSum += ( Diff >= 0.0f ) ? Diff : -Diff;
	}
	GPassing = GPassing && GSum <= Tolerance;
	return GSum <= Tolerance;
}


/**
 * Tests if two vectors (xyz) are equal within an optional tolerance
 *
 * @param Vec0 First vector
 * @param Vec1 Second vector
 * @param Tolerance Error allowed for the comparison
 *
 * @return true if equal(ish)
 */
bool TestVectorsEqual3( VectorRegister Vec0, VectorRegister Vec1, float Tolerance = 0.0f)
{
	VectorStoreAligned( Vec0, GScratch + 0 );
	VectorStoreAligned( Vec1, GScratch + 4 );
	GSum = 0.f;
	for ( int32 Component = 0; Component < 3; Component++ ) 
	{
		GSum += FMath::Abs<float>( GScratch[ Component + 0 ] - GScratch[ Component + 4 ] );
	}
	GPassing = GPassing && GSum <= Tolerance;
	return GSum <= Tolerance;
}

/**
 * Tests if two vectors (xyz) are equal within an optional tolerance
 *
 * @param Vec0 First vector
 * @param Vec1 Second vector
 * @param Tolerance Error allowed for the comparison
 *
 * @return true if equal(ish)
 */
bool TestVectorsEqual3_NoVec( const FVector& Vec0, const FVector& Vec1, float Tolerance = 0.0f)
{
	GScratch[0] = Vec0.X;
	GScratch[1] = Vec0.Y;
	GScratch[2] = Vec0.Z;
	GScratch[3] = 0.0f;
	GScratch[4] = Vec1.X;
	GScratch[5] = Vec1.Y;
	GScratch[6] = Vec1.Z;
	GScratch[7] = 0.0f;
	GSum = 0.f;

	for ( int32 Component = 0; Component < 3; Component++ ) 
	{
		GSum += FMath::Abs<float>( GScratch[ Component + 0 ] - GScratch[ Component + 4 ] );
	}
	GPassing = GPassing && GSum <= Tolerance;
	return GSum <= Tolerance;
}

/**
 * Tests if two matrices (4x4 xyzw) are equal within an optional tolerance
 *
 * @param Mat0 First Matrix
 * @param Mat1 Second Matrix
 * @param Tolerance Error per column allowed for the comparison
 *
 * @return true if equal(ish)
 */
bool TestMatricesEqual( FMatrix &Mat0, FMatrix &Mat1, float Tolerance = 0.0f)
{
	for (int32 Row = 0; Row < 4; ++Row ) 
	{
		GSum = 0.f;
		for ( int32 Column = 0; Column < 4; ++Column ) 
		{
			float Diff = Mat0.M[Row][Column] - Mat1.M[Row][Column];
			GSum += ( Diff >= 0.0f ) ? Diff : -Diff;
		}
		if (GSum > Tolerance)
		{
			GPassing = false; 
			return false;
		}
	}
	return true;
}


/**
 * Multiplies two 4x4 matrices.
 *
 * @param Result	Pointer to where the result should be stored
 * @param Matrix1	Pointer to the first matrix
 * @param Matrix2	Pointer to the second matrix
 */
void TestVectorMatrixMultiply( void* Result, const void* Matrix1, const void* Matrix2 )
{
	typedef float Float4x4[4][4];
	const Float4x4& A = *((const Float4x4*) Matrix1);
	const Float4x4& B = *((const Float4x4*) Matrix2);
	Float4x4 Temp;
	Temp[0][0] = A[0][0] * B[0][0] + A[0][1] * B[1][0] + A[0][2] * B[2][0] + A[0][3] * B[3][0];
	Temp[0][1] = A[0][0] * B[0][1] + A[0][1] * B[1][1] + A[0][2] * B[2][1] + A[0][3] * B[3][1];
	Temp[0][2] = A[0][0] * B[0][2] + A[0][1] * B[1][2] + A[0][2] * B[2][2] + A[0][3] * B[3][2];
	Temp[0][3] = A[0][0] * B[0][3] + A[0][1] * B[1][3] + A[0][2] * B[2][3] + A[0][3] * B[3][3];

	Temp[1][0] = A[1][0] * B[0][0] + A[1][1] * B[1][0] + A[1][2] * B[2][0] + A[1][3] * B[3][0];
	Temp[1][1] = A[1][0] * B[0][1] + A[1][1] * B[1][1] + A[1][2] * B[2][1] + A[1][3] * B[3][1];
	Temp[1][2] = A[1][0] * B[0][2] + A[1][1] * B[1][2] + A[1][2] * B[2][2] + A[1][3] * B[3][2];
	Temp[1][3] = A[1][0] * B[0][3] + A[1][1] * B[1][3] + A[1][2] * B[2][3] + A[1][3] * B[3][3];

	Temp[2][0] = A[2][0] * B[0][0] + A[2][1] * B[1][0] + A[2][2] * B[2][0] + A[2][3] * B[3][0];
	Temp[2][1] = A[2][0] * B[0][1] + A[2][1] * B[1][1] + A[2][2] * B[2][1] + A[2][3] * B[3][1];
	Temp[2][2] = A[2][0] * B[0][2] + A[2][1] * B[1][2] + A[2][2] * B[2][2] + A[2][3] * B[3][2];
	Temp[2][3] = A[2][0] * B[0][3] + A[2][1] * B[1][3] + A[2][2] * B[2][3] + A[2][3] * B[3][3];

	Temp[3][0] = A[3][0] * B[0][0] + A[3][1] * B[1][0] + A[3][2] * B[2][0] + A[3][3] * B[3][0];
	Temp[3][1] = A[3][0] * B[0][1] + A[3][1] * B[1][1] + A[3][2] * B[2][1] + A[3][3] * B[3][1];
	Temp[3][2] = A[3][0] * B[0][2] + A[3][1] * B[1][2] + A[3][2] * B[2][2] + A[3][3] * B[3][2];
	Temp[3][3] = A[3][0] * B[0][3] + A[3][1] * B[1][3] + A[3][2] * B[2][3] + A[3][3] * B[3][3];
	memcpy( Result, &Temp, 16*sizeof(float) );
}


/**
 * Calculate the inverse of an FMatrix.
 *
 * @param DstMatrix		FMatrix pointer to where the result should be stored
 * @param SrcMatrix		FMatrix pointer to the Matrix to be inversed
 */
 void TestVectorMatrixInverse( void* DstMatrix, const void* SrcMatrix )
{
	typedef float Float4x4[4][4];
	const Float4x4& M = *((const Float4x4*) SrcMatrix);
	Float4x4 Result;
	float Det[4];
	Float4x4 Tmp;

	Tmp[0][0]	= M[2][2] * M[3][3] - M[2][3] * M[3][2];
	Tmp[0][1]	= M[1][2] * M[3][3] - M[1][3] * M[3][2];
	Tmp[0][2]	= M[1][2] * M[2][3] - M[1][3] * M[2][2];

	Tmp[1][0]	= M[2][2] * M[3][3] - M[2][3] * M[3][2];
	Tmp[1][1]	= M[0][2] * M[3][3] - M[0][3] * M[3][2];
	Tmp[1][2]	= M[0][2] * M[2][3] - M[0][3] * M[2][2];

	Tmp[2][0]	= M[1][2] * M[3][3] - M[1][3] * M[3][2];
	Tmp[2][1]	= M[0][2] * M[3][3] - M[0][3] * M[3][2];
	Tmp[2][2]	= M[0][2] * M[1][3] - M[0][3] * M[1][2];

	Tmp[3][0]	= M[1][2] * M[2][3] - M[1][3] * M[2][2];
	Tmp[3][1]	= M[0][2] * M[2][3] - M[0][3] * M[2][2];
	Tmp[3][2]	= M[0][2] * M[1][3] - M[0][3] * M[1][2];

	Det[0]		= M[1][1]*Tmp[0][0] - M[2][1]*Tmp[0][1] + M[3][1]*Tmp[0][2];
	Det[1]		= M[0][1]*Tmp[1][0] - M[2][1]*Tmp[1][1] + M[3][1]*Tmp[1][2];
	Det[2]		= M[0][1]*Tmp[2][0] - M[1][1]*Tmp[2][1] + M[3][1]*Tmp[2][2];
	Det[3]		= M[0][1]*Tmp[3][0] - M[1][1]*Tmp[3][1] + M[2][1]*Tmp[3][2];

	float Determinant = M[0][0]*Det[0] - M[1][0]*Det[1] + M[2][0]*Det[2] - M[3][0]*Det[3];
	const float	RDet = 1.0f / Determinant;

	Result[0][0] =  RDet * Det[0];
	Result[0][1] = -RDet * Det[1];
	Result[0][2] =  RDet * Det[2];
	Result[0][3] = -RDet * Det[3];
	Result[1][0] = -RDet * (M[1][0]*Tmp[0][0] - M[2][0]*Tmp[0][1] + M[3][0]*Tmp[0][2]);
	Result[1][1] =  RDet * (M[0][0]*Tmp[1][0] - M[2][0]*Tmp[1][1] + M[3][0]*Tmp[1][2]);
	Result[1][2] = -RDet * (M[0][0]*Tmp[2][0] - M[1][0]*Tmp[2][1] + M[3][0]*Tmp[2][2]);
	Result[1][3] =  RDet * (M[0][0]*Tmp[3][0] - M[1][0]*Tmp[3][1] + M[2][0]*Tmp[3][2]);
	Result[2][0] =  RDet * (
					M[1][0] * (M[2][1] * M[3][3] - M[2][3] * M[3][1]) -
					M[2][0] * (M[1][1] * M[3][3] - M[1][3] * M[3][1]) +
					M[3][0] * (M[1][1] * M[2][3] - M[1][3] * M[2][1])
				);
	Result[2][1] = -RDet * (
					M[0][0] * (M[2][1] * M[3][3] - M[2][3] * M[3][1]) -
					M[2][0] * (M[0][1] * M[3][3] - M[0][3] * M[3][1]) +
					M[3][0] * (M[0][1] * M[2][3] - M[0][3] * M[2][1])
				);
	Result[2][2] =  RDet * (
					M[0][0] * (M[1][1] * M[3][3] - M[1][3] * M[3][1]) -
					M[1][0] * (M[0][1] * M[3][3] - M[0][3] * M[3][1]) +
					M[3][0] * (M[0][1] * M[1][3] - M[0][3] * M[1][1])
				);
	Result[2][3] = -RDet * (
					M[0][0] * (M[1][1] * M[2][3] - M[1][3] * M[2][1]) -
					M[1][0] * (M[0][1] * M[2][3] - M[0][3] * M[2][1]) +
					M[2][0] * (M[0][1] * M[1][3] - M[0][3] * M[1][1])
				);
	Result[3][0] = -RDet * (
					M[1][0] * (M[2][1] * M[3][2] - M[2][2] * M[3][1]) -
					M[2][0] * (M[1][1] * M[3][2] - M[1][2] * M[3][1]) +
					M[3][0] * (M[1][1] * M[2][2] - M[1][2] * M[2][1])
				);
	Result[3][1] =  RDet * (
					M[0][0] * (M[2][1] * M[3][2] - M[2][2] * M[3][1]) -
					M[2][0] * (M[0][1] * M[3][2] - M[0][2] * M[3][1]) +
					M[3][0] * (M[0][1] * M[2][2] - M[0][2] * M[2][1])
				);
	Result[3][2] = -RDet * (
					M[0][0] * (M[1][1] * M[3][2] - M[1][2] * M[3][1]) -
					M[1][0] * (M[0][1] * M[3][2] - M[0][2] * M[3][1]) +
					M[3][0] * (M[0][1] * M[1][2] - M[0][2] * M[1][1])
				);
	Result[3][3] =  RDet * (
				M[0][0] * (M[1][1] * M[2][2] - M[1][2] * M[2][1]) -
				M[1][0] * (M[0][1] * M[2][2] - M[0][2] * M[2][1]) +
				M[2][0] * (M[0][1] * M[1][2] - M[0][2] * M[1][1])
			);

	memcpy( DstMatrix, &Result, 16*sizeof(float) );
}


/**
 * Calculate Homogeneous transform.
 *
 * @param VecP			VectorRegister 
 * @param MatrixM		FMatrix pointer to the Matrix to apply transform
 * @return VectorRegister = VecP*MatrixM
 */
VectorRegister TestVectorTransformVector(const VectorRegister&  VecP,  const void* MatrixM )
{
	typedef float Float4x4[4][4];
	union { VectorRegister v; float f[4]; } Tmp, Result;
	Tmp.v = VecP;
	const Float4x4& M = *((const Float4x4*)MatrixM);	

	Result.f[0] = Tmp.f[0] * M[0][0] + Tmp.f[1] * M[1][0] + Tmp.f[2] * M[2][0] + Tmp.f[3] * M[3][0];
	Result.f[1] = Tmp.f[0] * M[0][1] + Tmp.f[1] * M[1][1] + Tmp.f[2] * M[2][1] + Tmp.f[3] * M[3][1];
	Result.f[2] = Tmp.f[0] * M[0][2] + Tmp.f[1] * M[1][2] + Tmp.f[2] * M[2][2] + Tmp.f[3] * M[3][2];
	Result.f[3] = Tmp.f[0] * M[0][3] + Tmp.f[1] * M[1][3] + Tmp.f[2] * M[2][3] + Tmp.f[3] * M[3][3];

	return Result.v;
}

/**
* Get Rotation as a quaternion.
* @param Rotator FRotator 
* @return Rotation as a quaternion.
*/
FORCENOINLINE FQuat TestRotatorToQuaternion( const FRotator& Rotator)
{
	const float CR = FMath::Cos(FMath::DegreesToRadians(Rotator.Roll  * 0.5f));
	const float CP = FMath::Cos(FMath::DegreesToRadians(Rotator.Pitch * 0.5f));
	const float CY = FMath::Cos(FMath::DegreesToRadians(Rotator.Yaw   * 0.5f));
	const float SR = FMath::Sin(FMath::DegreesToRadians(Rotator.Roll  * 0.5f));
	const float SP = FMath::Sin(FMath::DegreesToRadians(Rotator.Pitch * 0.5f));
	const float SY = FMath::Sin(FMath::DegreesToRadians(Rotator.Yaw   * 0.5f));

	FQuat RotationQuat;
	RotationQuat.W = CR*CP*CY + SR*SP*SY;
	RotationQuat.X = CR*SP*SY - SR*CP*CY;
	RotationQuat.Y = -CR*SP*CY - SR*CP*SY;
	RotationQuat.Z = CR*CP*SY - SR*SP*CY;
	return RotationQuat;
}

FORCENOINLINE FVector TestQuaternionRotateVector(const FQuat& Quat, const FVector& Vector)
{
	// (q.W*q.W-qv.qv)v + 2(qv.v)qv + 2 q.W (qv x v)
	const FVector qv(Quat.X, Quat.Y, Quat.Z);
	FVector vOut = (2.f * Quat.W) * (qv ^ Vector);
	vOut += ((Quat.W * Quat.W) - (qv | qv)) * Vector;
	vOut += (2.f * (qv | Vector)) * qv;
	
	return vOut;
}

FORCENOINLINE FVector TestQuaternionMultiplyVector(const FQuat& Quat, const FVector& Vector)
{
	FQuat VQ(Vector.X, Vector.Y, Vector.Z, 0.f);
	FQuat VT, VR;
	FQuat I = Quat.Inverse();
	VectorQuaternionMultiply(&VT, &Quat, &VQ);
	VectorQuaternionMultiply(&VR, &VT, &I);

	return FVector(VR.X, VR.Y, VR.Z);
}

/**
* Multiplies two quaternions: The order matters.
*
* @param Result	Pointer to where the result should be stored
* @param Quat1	Pointer to the first quaternion (must not be the destination)
* @param Quat2	Pointer to the second quaternion (must not be the destination)
*/
void TestVectorQuaternionMultiply( void *Result, const void* Quat1, const void* Quat2)
{
	typedef float Float4[4];
	const Float4& A = *((const Float4*) Quat1);
	const Float4& B = *((const Float4*) Quat2);
	Float4 & R = *((Float4*) Result);

	// store intermediate results in temporaries
	const float TX = A[3]*B[0] + A[0]*B[3] + A[1]*B[2] - A[2]*B[1];
	const float TY = A[3]*B[1] - A[0]*B[2] + A[1]*B[3] + A[2]*B[0];
	const float TZ = A[3]*B[2] + A[0]*B[1] - A[1]*B[0] + A[2]*B[3];
	const float TW = A[3]*B[3] - A[0]*B[0] - A[1]*B[1] - A[2]*B[2];

	// copy intermediate result to *this
	R[0] = TX;
	R[1] = TY;
	R[2] = TZ;
	R[3] = TW;
}

/**
 * Converts a Quaternion to a Rotator.
 */
FORCENOINLINE FRotator TestQuaternionToRotator(const FQuat& Quat)
{
	const float X = Quat.X;
	const float Y = Quat.Y;
	const float Z = Quat.Z;
	const float W = Quat.W;

	const float SingularityTest = Z*X-W*Y;
	const float YawY = 2.f*(W*Z+X*Y);
	const float YawX = (1.f-2.f*(FMath::Square(Y) + FMath::Square(Z)));
	const float SINGULARITY_THRESHOLD = 0.4999995f;

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
		RotatorFromQuat.Pitch = FMath::Asin(2.f*(SingularityTest)) * RAD_TO_DEG;
		RotatorFromQuat.Yaw = FMath::Atan2(YawY, YawX) * RAD_TO_DEG;
		RotatorFromQuat.Roll = FMath::Atan2(-2.f*(W*X+Y*Z), (1.f-2.f*(FMath::Square(X) + FMath::Square(Y)))) * RAD_TO_DEG;
	}

	RotatorFromQuat.Pitch = FRotator::NormalizeAxis(RotatorFromQuat.Pitch);
	RotatorFromQuat.Yaw = FRotator::NormalizeAxis(RotatorFromQuat.Yaw);
	RotatorFromQuat.Roll = FRotator::NormalizeAxis(RotatorFromQuat.Roll);

	return RotatorFromQuat;
}


/**
 * Helper debugf function to print out success or failure information for a test
 *
 * @param TestName Name of the current test
 * @param bHasPassed true if the test has passed
 */
void LogTest( const TCHAR *TestName, bool bHasPassed ) 
{
	if ( bHasPassed == false )
	{
		UE_LOG(LogUnrealMathTest, Log,  TEXT("%s: %s"), bHasPassed ? TEXT("PASSED") : TEXT("FAILED"), TestName );
		UE_LOG(LogUnrealMathTest, Log,  TEXT("Bad(%f): (%f %f %f %f) (%f %f %f %f)"), GSum, GScratch[0], GScratch[1], GScratch[2], GScratch[3], GScratch[4], GScratch[5], GScratch[6], GScratch[7] );
		GPassing = false;
	}
}


/** 
 * Set the contents of the scratch memory
 * 
 * @param X,Y,Z,W,U values to push into GScratch
 */
void SetScratch( float X, float Y, float Z, float W, float U = 0.0f )
{
	GScratch[0] = X;
	GScratch[1] = Y;
	GScratch[2] = Z;
	GScratch[3] = W;
	GScratch[4] = U;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVectorRegisterAbstractionTest, "System.Core.Math.Vector Register Abstraction Test", EAutomationTestFlags::ATF_SmokeTest)

/**
 * Run a suite of vector operations to validate vector intrinsics are working on the platform
 */
bool FVectorRegisterAbstractionTest::RunTest(const FString& Parameters)
{
	float F1 = 1.f;
	uint32 D1 = *(uint32 *)&F1;
	VectorRegister V0, V1, V2, V3;

	GPassing = true;

	V0 = MakeVectorRegister( D1, D1, D1, D1 );
	V1 = MakeVectorRegister( F1, F1, F1, F1 );
	LogTest( TEXT("MakeVectorRegister"), TestVectorsEqual( V0, V1 ) );

	V0 = MakeVectorRegister( 0.f, 0.f, 0.f, 0.f );
	V1 = VectorZero();
	LogTest( TEXT("VectorZero"), TestVectorsEqual( V0, V1 ) );

	V0 = MakeVectorRegister( 1.f, 1.f, 1.f, 1.f );
	V1 = VectorOne();
	LogTest( TEXT("VectorOne"), TestVectorsEqual( V0, V1 ) );

	SetScratch( 1.0f, 2.0f, -0.25f, -0.5f, 5.f );
	V0 = MakeVectorRegister( 1.0f, 2.0f, -0.25f, -0.5f );
	V1 = VectorLoad( GScratch );
	LogTest( TEXT("VectorLoad"), TestVectorsEqual( V0, V1 ) );

	SetScratch( 1.0f, 2.0f, -0.25f, -0.5f, 5.f );
	V0 = MakeVectorRegister( 1.0f, 2.0f, -0.25f, -0.5f );
	V1 = VectorLoad( GScratch );
	LogTest( TEXT("VectorLoad"), TestVectorsEqual( V0, V1 ) );

	SetScratch( 1.0f, 2.0f, -0.25f, -0.5f, 5.f );
	V0 = MakeVectorRegister( 1.0f, 2.0f, -0.25f, -0.5f );
	V1 = VectorLoadAligned( GScratch );
	LogTest( TEXT("VectorLoadAligned"), TestVectorsEqual( V0, V1 ) );

	SetScratch( 1.0f, 2.0f, -0.25f, -0.5f, 5.f );
	V0 = VectorLoad( GScratch + 1 );
	V1 = VectorLoadFloat3( GScratch + 1 );
	LogTest( TEXT("VectorLoadFloat3"), TestVectorsEqual3( V0, V1 ) );

	SetScratch( 1.0f, 2.0f, -0.25f, -0.5f, 5.f );
	V0 = MakeVectorRegister( 1.0f, 2.0f, -0.25f, 0.0f );
	V1 = VectorLoadFloat3_W0( GScratch );
	LogTest( TEXT("VectorLoadFloat3_W0"), TestVectorsEqual( V0, V1 ) );

	SetScratch( 1.0f, 2.0f, -0.25f, -0.5f, 5.f );
	V0 = MakeVectorRegister( 1.0f, 2.0f, -0.25f, 1.0f );
	V1 = VectorLoadFloat3_W1( GScratch );
	LogTest( TEXT("VectorLoadFloat3_W1"), TestVectorsEqual( V0, V1 ) );

	SetScratch( 1.0f, 2.0f, -0.25f, -0.5f, 5.f );
	V0 = MakeVectorRegister( -0.5f, -0.5f, -0.5f, -0.5f );
	V1 = VectorLoadFloat1( GScratch + 3 );
	LogTest( TEXT("VectorLoadFloat1"), TestVectorsEqual( V0, V1 ) );

	SetScratch( 1.0f, 2.0f, -0.25f, -0.5f, 5.f );
	V0 = VectorSetFloat3( GScratch[1], GScratch[2], GScratch[3] );
	V1 = VectorLoadFloat3( GScratch + 1 );
	LogTest( TEXT("VectorSetFloat3"), TestVectorsEqual3( V0, V1 ) );

	SetScratch( 1.0f, 2.0f, -0.25f, -0.5f, 5.f );
	V0 = VectorSet( GScratch[1], GScratch[2], GScratch[3], GScratch[4] );
	V1 = VectorLoad( GScratch + 1 );
	LogTest( TEXT("VectorSet"), TestVectorsEqual( V0, V1 ) );

	V0 = MakeVectorRegister( 1.0f, 2.0f, -0.25f, 1.0f );
	VectorStoreAligned( V0, GScratch + 8 );
	V1 = VectorLoad( GScratch + 8 );
	LogTest( TEXT("VectorStoreAligned"), TestVectorsEqual( V0, V1 ) );

	V0 = MakeVectorRegister( 1.0f, 2.0f, -0.55f, 1.0f );
	VectorStore( V0, GScratch + 7 );
	V1 = VectorLoad( GScratch + 7 );
	LogTest( TEXT("VectorStore"), TestVectorsEqual( V0, V1 ) );

	SetScratch( 1.0f, 2.0f, -0.25f, -0.5f, 5.f );
	V0 = MakeVectorRegister( 5.0f, 3.0f, 1.0f, -1.0f );
	VectorStoreFloat3( V0, GScratch );
	V1 = VectorLoad( GScratch );
	V0 = MakeVectorRegister( 5.0f, 3.0f, 1.0f, -0.5f );
	LogTest( TEXT("VectorStoreFloat3"), TestVectorsEqual( V0, V1 ) );

	SetScratch( 1.0f, 2.0f, -0.25f, -0.5f, 5.f );
	V0 = MakeVectorRegister( 5.0f, 3.0f, 1.0f, -1.0f );
	VectorStoreFloat1( V0, GScratch + 1 );
	V1 = VectorLoad( GScratch );
	V0 = MakeVectorRegister( 1.0f, 5.0f, -0.25f, -0.5f );
	LogTest( TEXT("VectorStoreFloat1"), TestVectorsEqual( V0, V1 ) );

	V0 = MakeVectorRegister( 1.0f, 2.0f, 3.0f, 4.0f );
	V1 = VectorReplicate( V0, 1 );
	V0 = MakeVectorRegister( 2.0f, 2.0f, 2.0f, 2.0f );
	LogTest( TEXT("VectorReplicate"), TestVectorsEqual( V0, V1 ) );

	V0 = MakeVectorRegister( 1.0f, -2.0f, 3.0f, -4.0f );
	V1 = VectorAbs( V0 );
	V0 = MakeVectorRegister( 1.0f, 2.0f, 3.0f, 4.0f );
	LogTest( TEXT("VectorAbs"), TestVectorsEqual( V0, V1 ) );

	V0 = MakeVectorRegister( 1.0f, -2.0f, 3.0f, -4.0f );
	V1 = VectorNegate( V0 );
	V0 = MakeVectorRegister( -1.0f, 2.0f, -3.0f, 4.0f );
	LogTest( TEXT("VectorNegate"), TestVectorsEqual( V0, V1 ) );

	V0 = MakeVectorRegister( 1.0f, 2.0f, 3.0f, 4.0f );
	V1 = MakeVectorRegister( 2.0f, 4.0f, 6.0f, 8.0f );
	V1 = VectorAdd( V0, V1 );
	V0 = MakeVectorRegister( 3.0f, 6.0f, 9.0f, 12.0f );
	LogTest( TEXT("VectorAdd"), TestVectorsEqual( V0, V1 ) );

	V0 = MakeVectorRegister( 2.0f, 4.0f, 6.0f, 8.0f );
	V1 = MakeVectorRegister( 1.0f, 2.0f, 3.0f, 4.0f );
	V1 = VectorSubtract( V0, V1 );
	V0 = MakeVectorRegister( 1.0f, 2.0f, 3.0f, 4.0f );
	LogTest( TEXT("VectorSubtract"), TestVectorsEqual( V0, V1 ) );

	V0 = MakeVectorRegister( 2.0f, 4.0f, 6.0f, 8.0f );
	V1 = MakeVectorRegister( 1.0f, 2.0f, 3.0f, 4.0f );
	V1 = VectorMultiply( V0, V1 );
	V0 = MakeVectorRegister( 2.0f, 8.0f, 18.0f, 32.0f );
	LogTest( TEXT("VectorMultiply"), TestVectorsEqual( V0, V1 ) );

	V0 = MakeVectorRegister( 2.0f, 4.0f, 6.0f, 8.0f );
	V1 = MakeVectorRegister( 1.0f, 2.0f, 3.0f, 4.0f );
	V1 = VectorMultiplyAdd( V0, V1, VectorOne() );
	V0 = MakeVectorRegister( 3.0f, 9.0f, 19.0f, 33.0f );
	LogTest( TEXT("VectorMultiplyAdd"), TestVectorsEqual( V0, V1 ) );

	V0 = MakeVectorRegister( 2.0f, 4.0f, 6.0f, 8.0f );
	V1 = MakeVectorRegister( 1.0f, 2.0f, 3.0f, 4.0f );
	V1 = VectorDot3( V0, V1 );
	V0 = MakeVectorRegister( 28.0f, 28.0f, 28.0f, 28.0f );
	LogTest( TEXT("VectorDot3"), TestVectorsEqual( V0, V1 ) );

	V0 = MakeVectorRegister( 2.0f, 4.0f, 6.0f, 8.0f );
	V1 = MakeVectorRegister( 1.0f, 2.0f, 3.0f, 4.0f );
	V1 = VectorDot4( V0, V1 );
	V0 = MakeVectorRegister( 60.0f, 60.0f, 60.0f, 60.0f );
	LogTest( TEXT("VectorDot4"), TestVectorsEqual( V0, V1 ) );

	V0 = MakeVectorRegister( 1.0f, 0.0f, 0.0f, 8.0f );
	V1 = MakeVectorRegister( 0.0f, 2.0f, 0.0f, 4.0f );
	V1 = VectorCross( V0, V1 );
	V0 = MakeVectorRegister( 0.f, 0.0f, 2.0f, 0.0f );
	LogTest( TEXT("VectorCross"), TestVectorsEqual( V0, V1 ) );

	V0 = MakeVectorRegister( 2.0f, 4.0f, 6.0f, 8.0f );
	V1 = MakeVectorRegister( 4.0f, 3.0f, 2.0f, 1.0f );
	V1 = VectorPow( V0, V1 );
	V0 = MakeVectorRegister( 16.0f, 64.0f, 36.0f, 8.0f );
	LogTest( TEXT("VectorPow"), TestVectorsEqual( V0, V1, 0.001f ) );

	V0 = MakeVectorRegister( 2.0f, -2.0f, 2.0f, -2.0f );
	V1 = VectorReciprocalLen( V0 );
	V0 = MakeVectorRegister( 0.25f, 0.25f, 0.25f, 0.25f );
	LogTest( TEXT("VectorReciprocalLen"), TestVectorsEqual( V0, V1, 0.001f ) );

	V0 = MakeVectorRegister( 2.0f, -2.0f, 2.0f, -2.0f );
	V1 = VectorNormalize( V0 );
	V0 = MakeVectorRegister( 0.5f, -0.5f, 0.5f, -0.5f );
	LogTest( TEXT("VectorNormalize"), TestVectorsEqual( V0, V1, 0.001f ) );

	V0 = MakeVectorRegister( 2.0f, -2.0f, 2.0f, -2.0f );
	V1 = VectorSet_W0( V0 );
	V0 = MakeVectorRegister( 2.0f, -2.0f, 2.0f, 0.0f );
	LogTest( TEXT("VectorSet_W0"), TestVectorsEqual( V0, V1 ) );

	V0 = MakeVectorRegister( 2.0f, -2.0f, 2.0f, -2.0f );
	V1 = VectorSet_W1( V0 );
	V0 = MakeVectorRegister( 2.0f, -2.0f, 2.0f, 1.0f );
	LogTest( TEXT("VectorSet_W1"), TestVectorsEqual( V0, V1 ) );

	V0 = MakeVectorRegister( 2.0f, 4.0f, 6.0f, 8.0f );
	V1 = MakeVectorRegister( 4.0f, 3.0f, 2.0f, 1.0f );
	V1 = VectorMin( V0, V1 );
	V0 = MakeVectorRegister( 2.0f, 3.0f, 2.0f, 1.0f );
	LogTest( TEXT("VectorMin"), TestVectorsEqual( V0, V1 ) );

	V0 = MakeVectorRegister( 2.0f, 4.0f, 6.0f, 8.0f );
	V1 = MakeVectorRegister( 4.0f, 3.0f, 2.0f, 1.0f );
	V1 = VectorMax( V0, V1 );
	V0 = MakeVectorRegister( 4.0f, 4.0f, 6.0f, 8.0f );
	LogTest( TEXT("VectorMax"), TestVectorsEqual( V0, V1 ) );

	V0 = MakeVectorRegister( 4.0f, 3.0f, 2.0f, 1.0f );
	V1 = VectorSwizzle( V0, 1, 0, 3, 2 );
	V0 = MakeVectorRegister( 3.0f, 4.0f, 1.0f, 2.0f );
	LogTest( TEXT("VectorSwizzle1032"), TestVectorsEqual( V0, V1 ) );

	V0 = MakeVectorRegister( 4.0f, 3.0f, 2.0f, 1.0f );
	V1 = VectorSwizzle( V0, 1, 2, 0, 1 );
	V0 = MakeVectorRegister( 3.0f, 2.0f, 4.0f, 3.0f );
	LogTest( TEXT("VectorSwizzle1201"), TestVectorsEqual( V0, V1 ) );

	V0 = MakeVectorRegister( 4.0f, 3.0f, 2.0f, 1.0f );
	V1 = VectorSwizzle( V0, 2, 0, 1, 3 );
	V0 = MakeVectorRegister( 2.0f, 4.0f, 3.0f, 1.0f );
	LogTest( TEXT("VectorSwizzle2013"), TestVectorsEqual( V0, V1 ) );

	V0 = MakeVectorRegister( 4.0f, 3.0f, 2.0f, 1.0f );
	V1 = VectorSwizzle( V0, 2, 3, 0, 1 );
	V0 = MakeVectorRegister( 2.0f, 1.0f, 4.0f, 3.0f );
	LogTest( TEXT("VectorSwizzle2301"), TestVectorsEqual( V0, V1 ) );

	V0 = MakeVectorRegister( 4.0f, 3.0f, 2.0f, 1.0f );
	V1 = VectorSwizzle( V0, 3, 2, 1, 0 );
	V0 = MakeVectorRegister( 1.0f, 2.0f, 3.0f, 4.0f );
	LogTest( TEXT("VectorSwizzle3210"), TestVectorsEqual( V0, V1 ) );

	uint8 Bytes[4] = { 25, 75, 125, 200 };
	V0 = VectorLoadByte4( Bytes );
	V1 = MakeVectorRegister( 25.f, 75.f, 125.f, 200.f );
	LogTest( TEXT("VectorLoadByte4"), TestVectorsEqual( V0, V1 ) );

	V0 = VectorLoadByte4Reverse( Bytes );
	V1 = MakeVectorRegister( 25.f, 75.f, 125.f, 200.f );
	V1 = VectorSwizzle( V1, 3, 2, 1, 0 );
	LogTest( TEXT("VectorLoadByte4Reverse"), TestVectorsEqual( V0, V1 ) );

	V0 = MakeVectorRegister( 4.0f, 3.0f, 2.0f, 1.0f );
	VectorStoreByte4( V0, Bytes );
	V1 = VectorLoadByte4( Bytes );
	LogTest( TEXT("VectorStoreByte4"), TestVectorsEqual( V0, V1 ) );

	V0 = MakeVectorRegister( 2.0f, 4.0f, 6.0f, 8.0f );
	V1 = MakeVectorRegister( 4.0f, 3.0f, 2.0f, 1.0f );
	bool bIsVAGT_TRUE = VectorAnyGreaterThan( V0, V1 ) != 0;
	LogTest( TEXT("VectorAnyGreaterThan-true"), bIsVAGT_TRUE );

	V0 = MakeVectorRegister( 1.0f, 3.0f, 2.0f, 1.0f );
	V1 = MakeVectorRegister( 2.0f, 4.0f, 6.0f, 8.0f );
	bool bIsVAGT_FALSE = VectorAnyGreaterThan( V0, V1 ) == 0;
	LogTest( TEXT("VectorAnyGreaterThan-false"), bIsVAGT_FALSE );

	V0 = MakeVectorRegister( 1.0f, 3.0f, 2.0f, 1.0f );
	V1 = MakeVectorRegister( 2.0f, 4.0f, 6.0f, 8.0f );
	LogTest( TEXT("VectorAnyLesserThan-true"), VectorAnyLesserThan( V0, V1 ) != 0 );

	V0 = MakeVectorRegister( 3.0f, 5.0f, 7.0f, 9.0f );
	V1 = MakeVectorRegister( 2.0f, 4.0f, 6.0f, 8.0f );
	LogTest( TEXT("VectorAnyLesserThan-false"), VectorAnyLesserThan( V0, V1 ) == 0 );

	V0 = MakeVectorRegister( 3.0f, 5.0f, 7.0f, 9.0f );
	V1 = MakeVectorRegister( 2.0f, 4.0f, 6.0f, 8.0f );
	LogTest( TEXT("VectorAllGreaterThan-true"), VectorAllGreaterThan( V0, V1 ) != 0 );

	V0 = MakeVectorRegister( 3.0f, 1.0f, 7.0f, 9.0f );
	V1 = MakeVectorRegister( 2.0f, 4.0f, 6.0f, 8.0f );
	LogTest( TEXT("VectorAllGreaterThan-false"), VectorAllGreaterThan( V0, V1 ) == 0 );

	V0 = MakeVectorRegister( 1.0f, 3.0f, 2.0f, 1.0f );
	V1 = MakeVectorRegister( 2.0f, 4.0f, 6.0f, 8.0f );
	LogTest( TEXT("VectorAllLesserThan-true"), VectorAllLesserThan( V0, V1 ) != 0 );

	V0 = MakeVectorRegister( 3.0f, 3.0f, 2.0f, 1.0f );
	V1 = MakeVectorRegister( 2.0f, 4.0f, 6.0f, 8.0f );
	LogTest( TEXT("VectorAllLesserThan-false"), VectorAllLesserThan( V0, V1 ) == 0 );

	V0 = MakeVectorRegister( 1.0f, 3.0f, 2.0f, 8.0f );
	V1 = MakeVectorRegister( 2.0f, 4.0f, 2.0f, 1.0f );
	V2 = VectorCompareGT( V0, V1 );
	V3 = MakeVectorRegister( (uint32)0, (uint32)0, (uint32)0, (uint32)-1 );
	LogTest( TEXT("VectorCompareGT"), TestVectorsEqualBitwise( V2, V3 ) );

	V0 = MakeVectorRegister( 1.0f, 3.0f, 2.0f, 8.0f );
	V1 = MakeVectorRegister( 2.0f, 4.0f, 2.0f, 1.0f );
	V2 = VectorCompareGE( V0, V1 );
	V3 = MakeVectorRegister( (uint32)0, (uint32)0, (uint32)-1, (uint32)-1 );
	LogTest( TEXT("VectorCompareGE"), TestVectorsEqualBitwise( V2, V3 ) );

	V0 = MakeVectorRegister( 1.0f, 3.0f, 2.0f, 8.0f );
	V1 = MakeVectorRegister( 2.0f, 4.0f, 2.0f, 1.0f );
	V2 = VectorCompareEQ( V0, V1 );
	V3 = MakeVectorRegister( (uint32)0, (uint32)0, (uint32)-1, (uint32)0 );
	LogTest( TEXT("VectorCompareEQ"), TestVectorsEqualBitwise( V2, V3 ) );

	V0 = MakeVectorRegister( 1.0f, 3.0f, 2.0f, 8.0f );
	V1 = MakeVectorRegister( 2.0f, 4.0f, 2.0f, 1.0f );
	V2 = VectorCompareNE( V0, V1 );
	V3 = MakeVectorRegister( (uint32)(0xFFFFFFFFU), (uint32)(0xFFFFFFFFU), (uint32)(0), (uint32)(0xFFFFFFFFU) );
	LogTest( TEXT("VectorCompareNE"), TestVectorsEqualBitwise( V2, V3 ) );
	
	V0 = MakeVectorRegister( 1.0f, 3.0f, 2.0f, 8.0f );
	V1 = MakeVectorRegister( 2.0f, 4.0f, 2.0f, 1.0f );
	V2 = MakeVectorRegister( (uint32)-1, (uint32)0, (uint32)0, (uint32)-1 );
	V2 = VectorSelect( V2, V0, V1 );
	V3 = MakeVectorRegister( 1.0f, 4.0f, 2.0f, 8.0f );
	LogTest( TEXT("VectorSelect"), TestVectorsEqual( V2, V3 ) );

	V0 = MakeVectorRegister( 1.0f, 3.0f, 0.0f, 0.0f );
	V1 = MakeVectorRegister( 0.0f, 0.0f, 2.0f, 1.0f );
	V2 = VectorBitwiseOr( V0, V1 );
	V3 = MakeVectorRegister( 1.0f, 3.0f, 2.0f, 1.0f );
	LogTest( TEXT("VectorBitwiseOr-Float1"), TestVectorsEqual( V2, V3 ) );

	V0 = MakeVectorRegister( 1.0f, 3.0f, 24.0f, 36.0f );
	V1 = MakeVectorRegister( (uint32)(0x80000000U), (uint32)(0x80000000U), (uint32)(0x80000000U), (uint32)(0x80000000U) );
	V2 = VectorBitwiseOr( V0, V1 );
	V3 = MakeVectorRegister( -1.0f, -3.0f, -24.0f, -36.0f );
	LogTest( TEXT("VectorBitwiseOr-Float2"), TestVectorsEqual( V2, V3 ) );

	V0 = MakeVectorRegister( -1.0f, -3.0f, -24.0f, 36.0f );
	V1 = MakeVectorRegister( (uint32)(0xFFFFFFFFU), (uint32)(0x7FFFFFFFU), (uint32)(0x7FFFFFFFU), (uint32)(0xFFFFFFFFU) );
	V2 = VectorBitwiseAnd( V0, V1 );
	V3 = MakeVectorRegister( -1.0f, 3.0f, 24.0f, 36.0f );
	LogTest( TEXT("VectorBitwiseAnd-Float"), TestVectorsEqual( V2, V3 ) );

	V0 = MakeVectorRegister( -1.0f, -3.0f, -24.0f, 36.0f );
	V1 = MakeVectorRegister( (uint32)(0x80000000U), (uint32)(0x00000000U), (uint32)(0x80000000U), (uint32)(0x80000000U) );
	V2 = VectorBitwiseXor( V0, V1 );
	V3 = MakeVectorRegister( 1.0f, -3.0f, 24.0f, -36.0f );
	LogTest( TEXT("VectorBitwiseXor-Float"), TestVectorsEqual( V2, V3 ) );

	V0 = MakeVectorRegister( -1.0f, -3.0f, -24.0f, 36.0f );
	V1 = MakeVectorRegister( 5.0f, 35.0f, 23.0f, 48.0f );
	V2 = VectorMergeVecXYZ_VecW( V0, V1 );
	V3 = MakeVectorRegister( -1.0f, -3.0f, -24.0f, 48.0f );
	LogTest( TEXT("VectorMergeXYZ_VecW-1"), TestVectorsEqual( V2, V3 ) );

	V0 = MakeVectorRegister( -1.0f, -3.0f, -24.0f, 36.0f );
	V1 = MakeVectorRegister( 5.0f, 35.0f, 23.0f, 48.0f );
	V2 = VectorMergeVecXYZ_VecW( V1, V0 );
	V3 = MakeVectorRegister( 5.0f, 35.0f, 23.0f, 36.0f );
	LogTest( TEXT("VectorMergeXYZ_VecW-2"), TestVectorsEqual( V2, V3 ) );

	V0 = MakeVectorRegister( 1.0f, 1.0e6f, 1.3e-8f, 35.0f );
	V1 = VectorReciprocal( V0 );
	V3 = VectorMultiply(V1, V0);
	LogTest( TEXT("VectorReciprocal"), TestVectorsEqual( VectorOne(), V3, 1e-3f ) );

	V0 = MakeVectorRegister( 1.0f, 1.0e6f, 1.3e-8f, 35.0f );
	V1 = VectorReciprocalAccurate( V0 );
	V3 = VectorMultiply(V1, V0);
	LogTest( TEXT("VectorReciprocalAccurate"), TestVectorsEqual( VectorOne(), V3, 1e-7f ) );

	V0 = MakeVectorRegister( 1.0f, 1.0e6f, 1.3e-8f, 35.0f );
	V1 = VectorReciprocalSqrt( V0 );
	V3 = VectorMultiply(VectorMultiply(V1, V1), V0);
	LogTest( TEXT("VectorReciprocalSqrt"), TestVectorsEqual( VectorOne(), V3, 2e-3f ) );

	V0 = MakeVectorRegister( 1.0f, 1.0e6f, 1.3e-8f, 35.0f );
	V1 = VectorReciprocalSqrtAccurate( V0 );
	V3 = VectorMultiply(VectorMultiply(V1, V1), V0);
	LogTest( TEXT("VectorReciprocalSqrtAccurate"), TestVectorsEqual( VectorOne(), V3, 1e-6f ) );

	FMatrix	M0, M1, M2, M3;
	FVector Eye, LookAt, Up;	
	// Create Look at Matrix
	Eye    = FVector(1024.0f, -512.0f, -2048.0f);
	LookAt = FVector(0.0f,		  0.0f,     0.0f);
	Up     = FVector(0.0f,       1.0f,    0.0f);
	M0	= FLookAtMatrix(Eye, LookAt, Up);		

	// Create GL ortho projection matrix
	const float Width = 1920.0f;
	const float Height = 1080.0f;
	const float Left = 0.0f;
	const float Right = Left+Width;
	const float Top = 0.0f;
	const float Bottom = Top+Height;
	const float ZNear = -100.0f;
	const float ZFar = 100.0f;

	M1 = FMatrix(FPlane(2.0f/(Right-Left),	0,							0,					0 ),
		FPlane(0,							2.0f/(Top-Bottom),			0,					0 ),
		FPlane(0,							0,							1/(ZNear-ZFar),		0 ),
		FPlane((Left+Right)/(Left-Right),	(Top+Bottom)/(Bottom-Top),	ZNear/(ZNear-ZFar), 1 ) );

	VectorMatrixMultiply( &M2, &M0, &M1 );
	TestVectorMatrixMultiply( &M3, &M0, &M1 );
	LogTest( TEXT("VectorMatrixMultiply"), TestMatricesEqual( M2, M3 ) );

	VectorMatrixInverse( &M2, &M1 );
	TestVectorMatrixInverse( &M3, &M1 );
	LogTest( TEXT("VectorMatrixInverse"), TestMatricesEqual( M2, M3 ) );

// 	FTransform Transform;
// 	Transform.SetFromMatrix(M1);
// 	FTransform InvTransform = Transform.Inverse();
// 	FTransform InvTransform2 = FTransform(Transform.ToMatrixWithScale().Inverse());
// 	LogTest( TEXT("FTransform Inverse"), InvTransform.Equals(InvTransform2, 1e-3f ) );

	V0 = MakeVectorRegister( 100.0f, -100.0f, 200.0f, 1.0f );
	V1 = VectorTransformVector(V0, &M0);
	V2 = TestVectorTransformVector(V0, &M0);
	LogTest( TEXT("VectorTransformVector"), TestVectorsEqual( V1, V2 ) );

	V0 = MakeVectorRegister( 32768.0f,131072.0f, -8096.0f, 1.0f );
	V1 = VectorTransformVector(V0, &M1);
	V2 = TestVectorTransformVector(V0, &M1);
	LogTest( TEXT("VectorTransformVector"), TestVectorsEqual( V1, V2 ) );

	FQuat Q0, Q1, Q2, Q3;

	{
		FRotator Rotator0;
		Rotator0 = FRotator(30.0f, -45.0f, 90.0f);
		Q0 = Rotator0.Quaternion();
		Q1 = TestRotatorToQuaternion(Rotator0);
		LogTest( TEXT("TestRotatorToQuaternion"), Q0.Equals( Q1, 1e-6f ) );

		FVector FV0, FV1;
		FV0 = Rotator0.Vector();
		FV1 = FRotationMatrix( Rotator0 ).GetScaledAxis( EAxis::X );
		LogTest( TEXT("Test0 Rotator::Vector()"), FV0.Equals( FV1, 1e-6f ) );
		
		FV0 = FRotationMatrix( Rotator0 ).GetScaledAxis( EAxis::X );
		FV1 = FQuatRotationMatrix( Q0 ).GetScaledAxis( EAxis::X );
		LogTest( TEXT("Test0 FQuatRotationMatrix"), FV0.Equals( FV1, 1e-5f ) );

		Rotator0 = FRotator(45.0f,  60.0f, 120.0f);
		Q0 = Rotator0.Quaternion();
		Q1 = TestRotatorToQuaternion(Rotator0);
		LogTest( TEXT("TestRotatorToQuaternion"), Q0.Equals( Q1, 1e-6f ) );

		FV0 = Rotator0.Vector();
		FV1 = FRotationMatrix( Rotator0 ).GetScaledAxis( EAxis::X );
		LogTest( TEXT("Test1 Rotator::Vector()"), FV0.Equals( FV1, 1e-6f ) );

		FV0 = FRotationMatrix( Rotator0 ).GetScaledAxis( EAxis::X );
		FV1 = FQuatRotationMatrix( Q0 ).GetScaledAxis( EAxis::X );
		LogTest(TEXT("Test1 FQuatRotationMatrix"), FV0.Equals(FV1, 1e-5f));

		FV0 = FRotationMatrix(FRotator::ZeroRotator).GetScaledAxis(EAxis::X);
		FV1 = FQuatRotationMatrix(FQuat::Identity).GetScaledAxis(EAxis::X);
		LogTest(TEXT("Test2 FQuatRotationMatrix"), FV0.Equals(FV1, 1e-6f));
	}

	{
		const FRotator Rotator0(30.0f, -45.0f, 90.0f);
		const FRotator Rotator1(45.0f,  60.0f, 120.0f);
		const FQuat Quat0(Rotator0);
		const FQuat Quat1(Rotator1);
		const float Tolerance = 1e-4f;

		LogTest( TEXT("Test0 FQuat::RotateVector(const FVector& Vec)"),		TestVectorsEqual3_NoVec(Quat0.RotateVector(FVector::ForwardVector), TestQuaternionRotateVector(Quat0, FVector::ForwardVector),	Tolerance ));
		LogTest( TEXT("Test0 FQuat::operator*(const FVector& Vec)"),		TestVectorsEqual3_NoVec(Quat0 * FVector::ForwardVector, TestQuaternionMultiplyVector(Quat0, FVector::ForwardVector), Tolerance ));
		LogTest( TEXT("Test1 FQuat::RotateVector(const FVector& Vec)"),		TestVectorsEqual3_NoVec(Quat0.RotateVector(FVector::RightVector), TestQuaternionRotateVector(Quat0, FVector::RightVector),	Tolerance ));
		LogTest( TEXT("Test1 FQuat::operator*(const FVector& Vec)"),		TestVectorsEqual3_NoVec(Quat0 * FVector::RightVector, TestQuaternionMultiplyVector(Quat0, FVector::RightVector), Tolerance ));
		LogTest( TEXT("Test2 FQuat::RotateVector(const FVector& Vec)"),		TestVectorsEqual3_NoVec(Quat0.RotateVector(FVector::UpVector), TestQuaternionRotateVector(Quat0, FVector::UpVector),	Tolerance ));
		LogTest( TEXT("Test2 FQuat::operator*(const FVector& Vec)"),		TestVectorsEqual3_NoVec(Quat0 * FVector::UpVector, TestQuaternionMultiplyVector(Quat0, FVector::UpVector), Tolerance ));
		LogTest( TEXT("Test3 FQuat::RotateVector(const FVector& Vec)"),		TestVectorsEqual3_NoVec(Quat0.RotateVector(FVector(45.0f, -60.0f, 120.0f)), TestQuaternionRotateVector(Quat0, FVector(45.0f, -60.0f, 120.0f)),	Tolerance ));
		LogTest( TEXT("Test3 FQuat::operator*(const FVector& Vec)"),		TestVectorsEqual3_NoVec(Quat0 * FVector(45.0f, -60.0f, 120.0f), TestQuaternionMultiplyVector(Quat0, FVector(45.0f, -60.0f, 120.0f)), Tolerance ));
		LogTest( TEXT("Test4 FQuat::RotateVector(const FVector& Vec)"),		TestVectorsEqual3_NoVec(Quat1.RotateVector(FVector::ForwardVector), TestQuaternionRotateVector(Quat1, FVector::ForwardVector),	Tolerance ));
		LogTest( TEXT("Test4 FQuat::operator*(const FVector& Vec)"),		TestVectorsEqual3_NoVec(Quat1 * FVector::ForwardVector, TestQuaternionMultiplyVector(Quat1, FVector::ForwardVector), Tolerance ));
		LogTest( TEXT("Test5 FQuat::RotateVector(const FVector& Vec)"),		TestVectorsEqual3_NoVec(Quat1.RotateVector(FVector::RightVector), TestQuaternionRotateVector(Quat1, FVector::RightVector),	Tolerance ));
		LogTest( TEXT("Test5 FQuat::operator*(const FVector& Vec)"),		TestVectorsEqual3_NoVec(Quat1 * FVector::RightVector, TestQuaternionMultiplyVector(Quat1, FVector::RightVector), Tolerance ));
		LogTest( TEXT("Test6 FQuat::RotateVector(const FVector& Vec)"),		TestVectorsEqual3_NoVec(Quat1.RotateVector(FVector::UpVector), TestQuaternionRotateVector(Quat1, FVector::UpVector),	Tolerance ));
		LogTest( TEXT("Test6 FQuat::operator*(const FVector& Vec)"),		TestVectorsEqual3_NoVec(Quat1 * FVector::UpVector, TestQuaternionMultiplyVector(Quat1, FVector::UpVector), Tolerance ));
		LogTest( TEXT("Test7 FQuat::RotateVector(const FVector& Vec)"),		TestVectorsEqual3_NoVec(Quat1.RotateVector(FVector(45.0f, -60.0f, 120.0f)), TestQuaternionRotateVector(Quat1, FVector(45.0f, -60.0f, 120.0f)),	Tolerance ));
		LogTest( TEXT("Test7 FQuat::operator*(const FVector& Vec)"),		TestVectorsEqual3_NoVec(Quat1 * FVector(45.0f, -60.0f, 120.0f), TestQuaternionMultiplyVector(Quat1, FVector(45.0f, -60.0f, 120.0f)), Tolerance ));

	}

	{
		FRotator Rotator0, Rotator1, Rotator2;
		Rotator0 = FRotator(30.0f, -45.0f, 90.0f);
		Q0 = TestRotatorToQuaternion(Rotator0);
		Rotator1 = Q0.Rotator();
		Rotator2 = TestQuaternionToRotator(Q0);
		LogTest(TEXT("TestQuaternionToRotator"), Rotator1.Equals(Rotator2, 1e-5f));

		Rotator0 = FRotator(45.0f, 60.0f, 120.0f);
		Q0 = TestRotatorToQuaternion(Rotator0);
		Rotator1 = Q0.Rotator();
		Rotator2 = TestQuaternionToRotator(Q0);
		LogTest(TEXT("TestQuaternionToRotator"), Rotator1.Equals(Rotator2, 1e-5f));
	}

	Q0 = FQuat(FRotator(30.0f, -45.0f, 90.0f));
	Q1 = FQuat(FRotator(45.0f,  60.0f, 120.0f));
	VectorQuaternionMultiply(&Q2, &Q0, &Q1);
	TestVectorQuaternionMultiply(&Q3, &Q0, &Q1);
	LogTest( TEXT("VectorQuaternionMultiply"), Q2.Equals( Q3, 1e-6f ) );
	V0 = VectorLoadAligned(&Q0);
	V1 = VectorLoadAligned(&Q1);
	V2 = VectorQuaternionMultiply2(V0, V1);
	V3 = VectorLoadAligned(&Q3);
	LogTest( TEXT("VectorQuaternionMultiply2"), TestVectorsEqual( V2, V3, 1e-6f ) );

	Q0 = FQuat(FRotator(0.0f, 180.0f, 45.0f));
	Q1 = FQuat(FRotator(-120.0f, -90.0f, 0.0f));
	VectorQuaternionMultiply(&Q2, &Q0, &Q1);
	TestVectorQuaternionMultiply(&Q3, &Q0, &Q1);
	LogTest( TEXT("VectorMatrixInverse"), Q2.Equals( Q3, 1e-6f ) );
	V0 = VectorLoadAligned(&Q0);
	V1 = VectorLoadAligned(&Q1);
	V2 = VectorQuaternionMultiply2(V0, V1);
	V3 = VectorLoadAligned(&Q3);
	LogTest( TEXT("VectorQuaternionMultiply2"), TestVectorsEqual(V2, V3, 1e-6f) );

	if (!GPassing)
	{
		UE_LOG(LogUnrealMathTest, Fatal,TEXT("VectorIntrinsics Failed."));
	}

	return true;
}
