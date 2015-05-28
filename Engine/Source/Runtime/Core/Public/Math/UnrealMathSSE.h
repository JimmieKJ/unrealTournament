// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#if __cplusplus_cli
// there are compile issues with this file in managed mode, so use the FPU version
#include "UnrealMathFPU.h"
#else

// We require SSE2
#include <emmintrin.h>


/*=============================================================================
 *	Helpers:
 *============================================================================*/

/**
 *	float4 vector register type, where the first float (X) is stored in the lowest 32 bits, and so on.
 */
typedef __m128	VectorRegister;
typedef __m128i VectorRegisterInt;

// for an __m128, we need a single set of braces (for clang)
#define DECLARE_VECTOR_REGISTER(X, Y, Z, W) { X, Y, Z, W }

/**
 * @param A0	Selects which element (0-3) from 'A' into 1st slot in the result
 * @param A1	Selects which element (0-3) from 'A' into 2nd slot in the result
 * @param B2	Selects which element (0-3) from 'B' into 3rd slot in the result
 * @param B3	Selects which element (0-3) from 'B' into 4th slot in the result
 */
#define SHUFFLEMASK(A0,A1,B2,B3) ( (A0) | ((A1)<<2) | ((B2)<<4) | ((B3)<<6) )

/**
 * Returns a bitwise equivalent vector based on 4 DWORDs.
 *
 * @param X		1st uint32 component
 * @param Y		2nd uint32 component
 * @param Z		3rd uint32 component
 * @param W		4th uint32 component
 * @return		Bitwise equivalent vector with 4 floats
 */
FORCEINLINE VectorRegister MakeVectorRegister( uint32 X, uint32 Y, uint32 Z, uint32 W )
{
 	union { VectorRegister v; VectorRegisterInt i; } Tmp;
	Tmp.i = _mm_setr_epi32( X, Y, Z, W );
 	return Tmp.v;
}

/**
 * Returns a vector based on 4 FLOATs.
 *
 * @param X		1st float component
 * @param Y		2nd float component
 * @param Z		3rd float component
 * @param W		4th float component
 * @return		Vector of the 4 FLOATs
 */
FORCEINLINE VectorRegister MakeVectorRegister( float X, float Y, float Z, float W )
{
	return _mm_setr_ps( X, Y, Z, W );
}

/*=============================================================================
 *	Constants:
 *============================================================================*/

#include "UnrealMathVectorConstants.h"

/*=============================================================================
 *	Intrinsics:
 *============================================================================*/

/**
 * Returns a vector with all zeros.
 *
 * @return		VectorRegister(0.0f, 0.0f, 0.0f, 0.0f)
 */
#define VectorZero()					_mm_setzero_ps()

/**
 * Returns a vector with all ones.
 *
 * @return		VectorRegister(1.0f, 1.0f, 1.0f, 1.0f)
 */
#define VectorOne()						(GlobalVectorConstants::FloatOne)

/**
 * Returns an component from a vector.
 *
 * @param Vec				Vector register
 * @param ComponentIndex	Which component to get, X=0, Y=1, Z=2, W=3
 * @return					The component as a float
 */
FORCEINLINE float VectorGetComponent(VectorRegister Vec, uint32 ComponentIndex)
{
	return (((float*)&(Vec))[ComponentIndex]);
}

/**
 * Loads 4 FLOATs from unaligned memory.
 *
 * @param Ptr	Unaligned memory pointer to the 4 FLOATs
 * @return		VectorRegister(Ptr[0], Ptr[1], Ptr[2], Ptr[3])
 */
#define VectorLoad( Ptr )				_mm_loadu_ps( (float*)(Ptr) )

/**
 * Loads 3 FLOATs from unaligned memory and leaves W undefined.
 *
 * @param Ptr	Unaligned memory pointer to the 3 FLOATs
 * @return		VectorRegister(Ptr[0], Ptr[1], Ptr[2], undefined)
 */
#define VectorLoadFloat3( Ptr )			MakeVectorRegister( ((const float*)(Ptr))[0], ((const float*)(Ptr))[1], ((const float*)(Ptr))[2], 0.0f )

/**
 * Loads 3 FLOATs from unaligned memory and sets W=0.
 *
 * @param Ptr	Unaligned memory pointer to the 3 FLOATs
 * @return		VectorRegister(Ptr[0], Ptr[1], Ptr[2], 0.0f)
 */
#define VectorLoadFloat3_W0( Ptr )		MakeVectorRegister( ((const float*)(Ptr))[0], ((const float*)(Ptr))[1], ((const float*)(Ptr))[2], 0.0f )

/**
 * Loads 3 FLOATs from unaligned memory and sets W=1.
 *
 * @param Ptr	Unaligned memory pointer to the 3 FLOATs
 * @return		VectorRegister(Ptr[0], Ptr[1], Ptr[2], 1.0f)
 */
#define VectorLoadFloat3_W1( Ptr )		MakeVectorRegister( ((const float*)(Ptr))[0], ((const float*)(Ptr))[1], ((const float*)(Ptr))[2], 1.0f )

/**
 * Loads 4 FLOATs from aligned memory.
 *
 * @param Ptr	Aligned memory pointer to the 4 FLOATs
 * @return		VectorRegister(Ptr[0], Ptr[1], Ptr[2], Ptr[3])
 */
#define VectorLoadAligned( Ptr )		_mm_load_ps( (float*)(Ptr) )

/**
 * Loads 1 float from unaligned memory and replicates it to all 4 elements.
 *
 * @param Ptr	Unaligned memory pointer to the float
 * @return		VectorRegister(Ptr[0], Ptr[0], Ptr[0], Ptr[0])
 */
#define VectorLoadFloat1( Ptr )			_mm_load1_ps( (float*)(Ptr) )

/**
 * Creates a vector out of three FLOATs and leaves W undefined.
 *
 * @param X		1st float component
 * @param Y		2nd float component
 * @param Z		3rd float component
 * @return		VectorRegister(X, Y, Z, undefined)
 */
#define VectorSetFloat3( X, Y, Z )		MakeVectorRegister( X, Y, Z, 0.0f )

/**
 * Propagates passed in float to all registers
 *
 * @param F		Float to set
 * @return		VectorRegister(F,F,F,F)
 */
#define VectorSetFloat1( F )	_mm_set1_ps( F )


/**
 * Creates a vector out of four FLOATs.
 *
 * @param X		1st float component
 * @param Y		2nd float component
 * @param Z		3rd float component
 * @param W		4th float component
 * @return		VectorRegister(X, Y, Z, W)
 */
#define VectorSet( X, Y, Z, W )			MakeVectorRegister( X, Y, Z, W )

/**
 * Stores a vector to aligned memory.
 *
 * @param Vec	Vector to store
 * @param Ptr	Aligned memory pointer
 */
#define VectorStoreAligned( Vec, Ptr )	_mm_store_ps( (float*)(Ptr), Vec )


/**
 * Performs non-temporal store of a vector to aligned memory without polluting the caches
 *
 * @param Vec	Vector to store
 * @param Ptr	Aligned memory pointer
 */
#define VectorStoreAlignedStreamed( Vec, Ptr )	_mm_stream_ps( (float*)(Ptr), Vec )


/**
 * Stores a vector to memory (aligned or unaligned).
 *
 * @param Vec	Vector to store
 * @param Ptr	Memory pointer
 */
#define VectorStore( Vec, Ptr )			_mm_storeu_ps( (float*)(Ptr), Vec )

/**
 * Stores the XYZ components of a vector to unaligned memory.
 *
 * @param Vec	Vector to store XYZ
 * @param Ptr	Unaligned memory pointer
 */
FORCEINLINE void VectorStoreFloat3( const VectorRegister& Vec, void* Ptr )
{
	union { VectorRegister v; float f[4]; } Tmp;
	Tmp.v = Vec;
	float* FloatPtr = (float*)(Ptr);
	FloatPtr[0] = Tmp.f[0];
	FloatPtr[1] = Tmp.f[1];
	FloatPtr[2] = Tmp.f[2];
}

/**
 * Stores the X component of a vector to unaligned memory.
 *
 * @param Vec	Vector to store X
 * @param Ptr	Unaligned memory pointer
 */
#define VectorStoreFloat1( Vec, Ptr )	_mm_store_ss((float*)(Ptr), Vec)

/**
 * Replicates one element into all four elements and returns the new vector.
 *
 * @param Vec			Source vector
 * @param ElementIndex	Index (0-3) of the element to replicate
 * @return				VectorRegister( Vec[ElementIndex], Vec[ElementIndex], Vec[ElementIndex], Vec[ElementIndex] )
 */
#define VectorReplicate( Vec, ElementIndex )	_mm_shuffle_ps( Vec, Vec, SHUFFLEMASK(ElementIndex,ElementIndex,ElementIndex,ElementIndex) )

/**
 * Returns the absolute value (component-wise).
 *
 * @param Vec			Source vector
 * @return				VectorRegister( abs(Vec.x), abs(Vec.y), abs(Vec.z), abs(Vec.w) )
 */
#define VectorAbs( Vec )				_mm_and_ps(Vec, GlobalVectorConstants::SignMask)

/**
 * Returns the negated value (component-wise).
 *
 * @param Vec			Source vector
 * @return				VectorRegister( -Vec.x, -Vec.y, -Vec.z, -Vec.w )
 */
#define VectorNegate( Vec )				_mm_sub_ps(_mm_setzero_ps(),Vec)

/**
 * Adds two vectors (component-wise) and returns the result.
 *
 * @param Vec1	1st vector
 * @param Vec2	2nd vector
 * @return		VectorRegister( Vec1.x+Vec2.x, Vec1.y+Vec2.y, Vec1.z+Vec2.z, Vec1.w+Vec2.w )
 */
#define VectorAdd( Vec1, Vec2 )			_mm_add_ps( Vec1, Vec2 )

/**
 * Subtracts a vector from another (component-wise) and returns the result.
 *
 * @param Vec1	1st vector
 * @param Vec2	2nd vector
 * @return		VectorRegister( Vec1.x-Vec2.x, Vec1.y-Vec2.y, Vec1.z-Vec2.z, Vec1.w-Vec2.w )
 */
#define VectorSubtract( Vec1, Vec2 )	_mm_sub_ps( Vec1, Vec2 )

/**
 * Multiplies two vectors (component-wise) and returns the result.
 *
 * @param Vec1	1st vector
 * @param Vec2	2nd vector
 * @return		VectorRegister( Vec1.x*Vec2.x, Vec1.y*Vec2.y, Vec1.z*Vec2.z, Vec1.w*Vec2.w )
 */
#define VectorMultiply( Vec1, Vec2 )	_mm_mul_ps( Vec1, Vec2 )

/**
 * Multiplies two vectors (component-wise), adds in the third vector and returns the result.
 *
 * @param Vec1	1st vector
 * @param Vec2	2nd vector
 * @param Vec3	3rd vector
 * @return		VectorRegister( Vec1.x*Vec2.x + Vec3.x, Vec1.y*Vec2.y + Vec3.y, Vec1.z*Vec2.z + Vec3.z, Vec1.w*Vec2.w + Vec3.w )
 */
#define VectorMultiplyAdd( Vec1, Vec2, Vec3 )	_mm_add_ps( _mm_mul_ps(Vec1, Vec2), Vec3 )

/**
 * Calculates the dot3 product of two vectors and returns a vector with the result in all 4 components.
 * Only really efficient on Xbox 360.
 *
 * @param Vec1	1st vector
 * @param Vec2	2nd vector
 * @return		d = dot3(Vec1.xyz, Vec2.xyz), VectorRegister( d, d, d, d )
 */
FORCEINLINE VectorRegister VectorDot3( const VectorRegister& Vec1, const VectorRegister& Vec2 )
{
	VectorRegister Temp = VectorMultiply( Vec1, Vec2 );
	return VectorAdd( VectorReplicate(Temp,0), VectorAdd( VectorReplicate(Temp,1), VectorReplicate(Temp,2) ) );
}

/**
 * Calculates the dot4 product of two vectors and returns a vector with the result in all 4 components.
 * Only really efficient on Xbox 360.
 *
 * @param Vec1	1st vector
 * @param Vec2	2nd vector
 * @return		d = dot4(Vec1.xyzw, Vec2.xyzw), VectorRegister( d, d, d, d )
 */
FORCEINLINE VectorRegister VectorDot4( const VectorRegister& Vec1, const VectorRegister& Vec2 )
{
	VectorRegister Temp1, Temp2;
	Temp1 = VectorMultiply( Vec1, Vec2 );
	Temp2 = _mm_shuffle_ps( Temp1, Temp1, SHUFFLEMASK(2,3,0,1) );	// (Z,W,X,Y).
	Temp1 = VectorAdd( Temp1, Temp2 );								// (X*X + Z*Z, Y*Y + W*W, Z*Z + X*X, W*W + Y*Y)
	Temp2 = _mm_shuffle_ps( Temp1, Temp1, SHUFFLEMASK(1,2,3,0) );	// Rotate left 4 bytes (Y,Z,W,X).
	return VectorAdd( Temp1, Temp2 );								// (X*X + Z*Z + Y*Y + W*W, Y*Y + W*W + Z*Z + X*X, Z*Z + X*X + W*W + Y*Y, W*W + Y*Y + X*X + Z*Z)
}


/**
 * Creates a four-part mask based on component-wise == compares of the input vectors
 *
 * @param Vec1	1st vector
 * @param Vec2	2nd vector
 * @return		VectorRegister( Vec1.x == Vec2.x ? 0xFFFFFFFF : 0, same for yzw )
 */
#define VectorCompareEQ( Vec1, Vec2 )			_mm_cmpeq_ps( Vec1, Vec2 )

/**
 * Creates a four-part mask based on component-wise != compares of the input vectors
 *
 * @param Vec1	1st vector
 * @param Vec2	2nd vector
 * @return		VectorRegister( Vec1.x != Vec2.x ? 0xFFFFFFFF : 0, same for yzw )
 */
#define VectorCompareNE( Vec1, Vec2 )			_mm_cmpneq_ps( Vec1, Vec2 )

/**
 * Creates a four-part mask based on component-wise > compares of the input vectors
 *
 * @param Vec1	1st vector
 * @param Vec2	2nd vector
 * @return		VectorRegister( Vec1.x > Vec2.x ? 0xFFFFFFFF : 0, same for yzw )
 */
#define VectorCompareGT( Vec1, Vec2 )			_mm_cmpgt_ps( Vec1, Vec2 )

/**
 * Creates a four-part mask based on component-wise >= compares of the input vectors
 *
 * @param Vec1	1st vector
 * @param Vec2	2nd vector
 * @return		VectorRegister( Vec1.x >= Vec2.x ? 0xFFFFFFFF : 0, same for yzw )
 */
#define VectorCompareGE( Vec1, Vec2 )			_mm_cmpge_ps( Vec1, Vec2 )

/**
 * Does a bitwise vector selection based on a mask (e.g., created from VectorCompareXX)
 *
 * @param Mask  Mask (when 1: use the corresponding bit from Vec1 otherwise from Vec2)
 * @param Vec1	1st vector
 * @param Vec2	2nd vector
 * @return		VectorRegister( for each bit i: Mask[i] ? Vec1[i] : Vec2[i] )
 *
 */

FORCEINLINE VectorRegister VectorSelect(const VectorRegister& Mask, const VectorRegister& Vec1, const VectorRegister& Vec2 )
{
	return _mm_xor_ps( Vec2, _mm_and_ps( Mask, _mm_xor_ps( Vec1, Vec2 ) ) );
}

/**
 * Combines two vectors using bitwise OR (treating each vector as a 128 bit field)
 *
 * @param Vec1	1st vector
 * @param Vec2	2nd vector
 * @return		VectorRegister( for each bit i: Vec1[i] | Vec2[i] )
 */
#define VectorBitwiseOr(Vec1, Vec2)	_mm_or_ps(Vec1, Vec2)

/**
 * Combines two vectors using bitwise AND (treating each vector as a 128 bit field)
 *
 * @param Vec1	1st vector
 * @param Vec2	2nd vector
 * @return		VectorRegister( for each bit i: Vec1[i] & Vec2[i] )
 */
#define VectorBitwiseAnd(Vec1, Vec2) _mm_and_ps(Vec1, Vec2)

/**
 * Combines two vectors using bitwise XOR (treating each vector as a 128 bit field)
 *
 * @param Vec1	1st vector
 * @param Vec2	2nd vector
 * @return		VectorRegister( for each bit i: Vec1[i] ^ Vec2[i] )
 */
#define VectorBitwiseXor(Vec1, Vec2) _mm_xor_ps(Vec1, Vec2)


/**
 * Calculates the cross product of two vectors (XYZ components). W is set to 0.
 *
 * @param Vec1	1st vector
 * @param Vec2	2nd vector
 * @return		cross(Vec1.xyz, Vec2.xyz). W is set to 0.
 */
FORCEINLINE VectorRegister VectorCross( const VectorRegister& Vec1, const VectorRegister& Vec2 )
{
	VectorRegister A_YZXW = _mm_shuffle_ps( Vec1, Vec1, SHUFFLEMASK(1,2,0,3) );
	VectorRegister B_ZXYW = _mm_shuffle_ps( Vec2, Vec2, SHUFFLEMASK(2,0,1,3) );
	VectorRegister A_ZXYW = _mm_shuffle_ps( Vec1, Vec1, SHUFFLEMASK(2,0,1,3) );
	VectorRegister B_YZXW = _mm_shuffle_ps( Vec2, Vec2, SHUFFLEMASK(1,2,0,3) );
	return VectorSubtract( VectorMultiply(A_YZXW,B_ZXYW), VectorMultiply(A_ZXYW, B_YZXW) );
}

/**
 * Calculates x raised to the power of y (component-wise).
 *
 * @param Base		Base vector
 * @param Exponent	Exponent vector
 * @return			VectorRegister( Base.x^Exponent.x, Base.y^Exponent.y, Base.z^Exponent.z, Base.w^Exponent.w )
 */
FORCEINLINE VectorRegister VectorPow( const VectorRegister& Base, const VectorRegister& Exponent )
{
	//@TODO: Optimize
	union { VectorRegister v; float f[4]; } B, E;
	B.v = Base;
	E.v = Exponent;
	return _mm_setr_ps( powf(B.f[0], E.f[0]), powf(B.f[1], E.f[1]), powf(B.f[2], E.f[2]), powf(B.f[3], E.f[3]) );
}

/**
* Returns an estimate of 1/sqrt(c) for each component of the vector
*
* @param Vector		Vector 
* @return			VectorRegister(1/sqrt(t), 1/sqrt(t), 1/sqrt(t), 1/sqrt(t))
*/
#define VectorReciprocalSqrt(Vec)		_mm_rsqrt_ps( Vec )

/**
 * Computes an estimate of the reciprocal of a vector (component-wise) and returns the result.
 *
 * @param Vec	1st vector
 * @return		VectorRegister( (Estimate) 1.0f / Vec.x, (Estimate) 1.0f / Vec.y, (Estimate) 1.0f / Vec.z, (Estimate) 1.0f / Vec.w )
 */
#define VectorReciprocal(Vec)			_mm_rcp_ps(Vec)

/**
* Return Reciprocal Length of the vector (estimate)
*
* @param Vector		Vector 
* @return			VectorRegister(rlen, rlen, rlen, rlen) when rlen = 1/sqrt(dot4(V))
*/
FORCEINLINE VectorRegister VectorReciprocalLen( const VectorRegister& Vector )
{
	VectorRegister RecipLen = VectorDot4( Vector, Vector );
	return VectorReciprocalSqrt( RecipLen );
}

/**
* Return the reciprocal of the square root of each component
*
* @param Vector		Vector 
* @return			VectorRegister(1/sqrt(Vec.X), 1/sqrt(Vec.Y), 1/sqrt(Vec.Z), 1/sqrt(Vec.W))
*/
FORCEINLINE VectorRegister VectorReciprocalSqrtAccurate(const VectorRegister& Vec)
{
	// Perform a single pass of Newton-Raphson iteration on the hardware estimate
	//    v^-0.5 = x
	// => x^2 = v^-1
	// => 1/(x^2) = v
	// => F(x) = x^-2 - v
	//    F'(x) = -2x^-3

	//    x1 = x0 - F(x0)/F'(x0)
	// => x1 = x0 + 0.5 * (x0^-2 - Vec) * x0^3
	// => x1 = x0 + 0.5 * (x0 - Vec * x0^3)
	// => x1 = x0 + x0 * (0.5 - 0.5 * Vec * x0^2)

	const VectorRegister OneHalf = GlobalVectorConstants::FloatOneHalf;
	const VectorRegister VecDivBy2 = VectorMultiply(Vec, OneHalf);

	// Initial estimate
	const VectorRegister x0 = VectorReciprocalSqrt(Vec);

	// First iteration
	const VectorRegister x0Squared = VectorMultiply(x0, x0);
	const VectorRegister InnerTerm0 = VectorSubtract(OneHalf, VectorMultiply(VecDivBy2, x0Squared));
	const VectorRegister x1 = VectorMultiplyAdd(x0, InnerTerm0, x0);

	return x1;
}

/**
 * Computes the reciprocal of a vector (component-wise) and returns the result.
 *
 * @param Vec	1st vector
 * @return		VectorRegister( 1.0f / Vec.x, 1.0f / Vec.y, 1.0f / Vec.z, 1.0f / Vec.w )
 */
FORCEINLINE VectorRegister VectorReciprocalAccurate(const VectorRegister& Vec)
{
	// Perform two passes of Newton-Raphson iteration on the hardware estimate
	//   x1 = x0 - f(x0) / f'(x0)
	//
	//    1 / Vec = x
	// => x * Vec = 1 
	// => F(x) = x * Vec - 1
	//    F'(x) = Vec
	// => x1 = x0 - (x0 * Vec - 1) / Vec
	//
	// Since 1/Vec is what we're trying to solve, use an estimate for it, x0
	// => x1 = x0 - (x0 * Vec - 1) * x0 = 2 * x0 - Vec * x0^2 

	// Initial estimate
	const VectorRegister x0 = VectorReciprocal(Vec);

	// First iteration
	const VectorRegister x0Squared = VectorMultiply(x0, x0);
	const VectorRegister x0Times2 = VectorAdd(x0, x0);
	const VectorRegister x1 = VectorSubtract(x0Times2, VectorMultiply(Vec, x0Squared));

	// Second iteration
	const VectorRegister x1Squared = VectorMultiply(x1, x1);
	const VectorRegister x1Times2 = VectorAdd(x1, x1);
	const VectorRegister x2 = VectorSubtract(x1Times2, VectorMultiply(Vec, x1Squared));

	return x2;
}

/**
* Normalize vector
*
* @param Vector		Vector to normalize
* @return			Normalized VectorRegister
*/
FORCEINLINE VectorRegister VectorNormalize( const VectorRegister& Vector )
{
	return VectorMultiply( Vector, VectorReciprocalLen( Vector ) );
}

/**
* Loads XYZ and sets W=0
*
* @param Vector	VectorRegister
* @return		VectorRegister(X, Y, Z, 0.0f)
*/
#define VectorSet_W0( Vec )		_mm_and_ps( Vec, GlobalVectorConstants::XYZMask )

/**
* Loads XYZ and sets W=1
*
* @param Vector	VectorRegister
* @return		VectorRegister(X, Y, Z, 1.0f)
*/
FORCEINLINE VectorRegister VectorSet_W1( const VectorRegister& Vector )
{
	// Temp = (Vector[2]. Vector[3], 1.0f, 1.0f)
	VectorRegister Temp = _mm_movehl_ps( VectorOne(), Vector );

	// Return (Vector[0], Vector[1], Vector[2], 1.0f)
	return _mm_shuffle_ps( Vector, Temp, SHUFFLEMASK(0,1,0,3) );
}

/**
 * Multiplies two 4x4 matrices.
 *
 * @param Result	Pointer to where the result should be stored
 * @param Matrix1	Pointer to the first matrix
 * @param Matrix2	Pointer to the second matrix
 */
FORCEINLINE void VectorMatrixMultiply( void *Result, const void* Matrix1, const void* Matrix2 )
{
	const VectorRegister *A	= (const VectorRegister *) Matrix1;
	const VectorRegister *B	= (const VectorRegister *) Matrix2;
	VectorRegister *R		= (VectorRegister *) Result;
	VectorRegister Temp, R0, R1, R2, R3;

	// First row of result (Matrix1[0] * Matrix2).
	Temp	= VectorMultiply( VectorReplicate( A[0], 0 ), B[0] );
	Temp	= VectorMultiplyAdd( VectorReplicate( A[0], 1 ), B[1], Temp );
	Temp	= VectorMultiplyAdd( VectorReplicate( A[0], 2 ), B[2], Temp );
	R0		= VectorMultiplyAdd( VectorReplicate( A[0], 3 ), B[3], Temp );

	// Second row of result (Matrix1[1] * Matrix2).
	Temp	= VectorMultiply( VectorReplicate( A[1], 0 ), B[0] );
	Temp	= VectorMultiplyAdd( VectorReplicate( A[1], 1 ), B[1], Temp );
	Temp	= VectorMultiplyAdd( VectorReplicate( A[1], 2 ), B[2], Temp );
	R1		= VectorMultiplyAdd( VectorReplicate( A[1], 3 ), B[3], Temp );

	// Third row of result (Matrix1[2] * Matrix2).
	Temp	= VectorMultiply( VectorReplicate( A[2], 0 ), B[0] );
	Temp	= VectorMultiplyAdd( VectorReplicate( A[2], 1 ), B[1], Temp );
	Temp	= VectorMultiplyAdd( VectorReplicate( A[2], 2 ), B[2], Temp );
	R2		= VectorMultiplyAdd( VectorReplicate( A[2], 3 ), B[3], Temp );

	// Fourth row of result (Matrix1[3] * Matrix2).
	Temp	= VectorMultiply( VectorReplicate( A[3], 0 ), B[0] );
	Temp	= VectorMultiplyAdd( VectorReplicate( A[3], 1 ), B[1], Temp );
	Temp	= VectorMultiplyAdd( VectorReplicate( A[3], 2 ), B[2], Temp );
	R3		= VectorMultiplyAdd( VectorReplicate( A[3], 3 ), B[3], Temp );

	// Store result
	R[0] = R0;
	R[1] = R1;
	R[2] = R2;
	R[3] = R3;
}

/**
 * Calculate the inverse of an FMatrix.
 *
 * @param DstMatrix		FMatrix pointer to where the result should be stored
 * @param SrcMatrix		FMatrix pointer to the Matrix to be inversed
 */
// TODO: Vector version of this function that doesn't use D3DX
FORCEINLINE void VectorMatrixInverse( void* DstMatrix, const void* SrcMatrix )
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
FORCEINLINE VectorRegister VectorTransformVector(const VectorRegister&  VecP,  const void* MatrixM )
{
	const VectorRegister *M	= (const VectorRegister *) MatrixM;
	VectorRegister VTempX, VTempY, VTempZ, VTempW;

	// Splat x,y,z and w
	VTempX = VectorReplicate(VecP, 0);
	VTempY = VectorReplicate(VecP, 1);
	VTempZ = VectorReplicate(VecP, 2);
	VTempW = VectorReplicate(VecP, 3);
	// Mul by the matrix
	VTempX = VectorMultiply(VTempX, M[0]);
	VTempY = VectorMultiply(VTempY, M[1]);
	VTempZ = VectorMultiply(VTempZ, M[2]);
	VTempW = VectorMultiply(VTempW, M[3]);
	// Add them all together
	VTempX = VectorAdd(VTempX, VTempY);
	VTempZ = VectorAdd(VTempZ, VTempW);
	VTempX = VectorAdd(VTempX, VTempZ);

	return VTempX;
}

/**
 * Returns the minimum values of two vectors (component-wise).
 *
 * @param Vec1	1st vector
 * @param Vec2	2nd vector
 * @return		VectorRegister( min(Vec1.x,Vec2.x), min(Vec1.y,Vec2.y), min(Vec1.z,Vec2.z), min(Vec1.w,Vec2.w) )
 */
#define VectorMin( Vec1, Vec2 )			_mm_min_ps( Vec1, Vec2 )

/**
 * Returns the maximum values of two vectors (component-wise).
 *
 * @param Vec1	1st vector
 * @param Vec2	2nd vector
 * @return		VectorRegister( max(Vec1.x,Vec2.x), max(Vec1.y,Vec2.y), max(Vec1.z,Vec2.z), max(Vec1.w,Vec2.w) )
 */
#define VectorMax( Vec1, Vec2 )			_mm_max_ps( Vec1, Vec2 )

/**
 * Swizzles the 4 components of a vector and returns the result.
 *
 * @param Vec		Source vector
 * @param X			Index for which component to use for X (literal 0-3)
 * @param Y			Index for which component to use for Y (literal 0-3)
 * @param Z			Index for which component to use for Z (literal 0-3)
 * @param W			Index for which component to use for W (literal 0-3)
 * @return			The swizzled vector
 */
#define VectorSwizzle( Vec, X, Y, Z, W )	_mm_shuffle_ps( Vec, Vec, SHUFFLEMASK(X,Y,Z,W) )

/**
 * Creates a vector through selecting two components from each vector via a shuffle mask. 
 *
 * @param Vec1		Source vector1
 * @param Vec2		Source vector2
 * @param X			Index for which component of Vector1 to use for X (literal 0-3)
 * @param Y			Index for which component to Vector1 to use for Y (literal 0-3)
 * @param Z			Index for which component to Vector2 to use for Z (literal 0-3)
 * @param W			Index for which component to Vector2 to use for W (literal 0-3)
 * @return			The swizzled vector
 */
#define VectorShuffle( Vec1, Vec2, X, Y, Z, W )	_mm_shuffle_ps( Vec1, Vec2, SHUFFLEMASK(X,Y,Z,W) )



/**
 * These functions return a vector mask to indicate which components pass the comparison.
 * Each component is 0xffffffff if it passes, 0x00000000 if it fails.
 *
 * @param Vec1			1st source vector
 * @param Vec2			2nd source vector
 * @return				Vector with a mask for each component.
 */
#define VectorMask_LT( Vec1, Vec2 )			_mm_cmplt_ps(Vec1, Vec2)
#define VectorMask_LE( Vec1, Vec2 )			_mm_cmple_ps(Vec1, Vec2)
#define VectorMask_GT( Vec1, Vec2 )			_mm_cmpgt_ps(Vec1, Vec2)
#define VectorMask_GE( Vec1, Vec2 )			_mm_cmpge_ps(Vec1, Vec2)
#define VectorMask_EQ( Vec1, Vec2 )			_mm_cmpeq_ps(Vec1, Vec2)
#define VectorMask_NE( Vec1, Vec2 )			_mm_cmpneq_ps(Vec1, Vec2)

/**
 * Returns an integer bit-mask (0x00 - 0x0f) based on the sign-bit for each component in a vector.
 *
 * @param VecMask		Vector
 * @return				Bit 0 = sign(VecMask.x), Bit 1 = sign(VecMask.y), Bit 2 = sign(VecMask.z), Bit 3 = sign(VecMask.w)
 */
#define VectorMaskBits( VecMask )			_mm_movemask_ps( VecMask )

/**
 * Returns the bitwise AND.
 *
 * @param	Vec1	Vector to AND
 * @param	Vec2	Vector to AND
 * @return	bitwise per component AND operation.
 */
#define VectorBitwiseAND( Vec1, Vec2 )	_mm_and_ps( (Vec1), (Vec2) )

/**
 * Divides two vectors (component-wise) and returns the result.
 *
 * @param Vec1	1st vector
 * @param Vec2	2nd vector
 * @return		VectorRegister( Vec1.x/Vec2.x, Vec1.y/Vec2.y, Vec1.z/Vec2.z, Vec1.w/Vec2.w )
 */
#define VectorDivide( Vec1, Vec2 )		_mm_div_ps( Vec1, Vec2 )

/**
 * Counts the number of trailing zeros in the bit representation of the value,
 * counting from least-significant bit to most.
 *
 * @param Value the value to determine the number of leading zeros for
 * @return the number of zeros before the first "on" bit
 */
#if PLATFORM_WINDOWS
#pragma intrinsic( _BitScanForward )
FORCEINLINE uint32 appCountTrailingZeros(uint32 Value)
{
	if (Value == 0)
	{
		return 32;
	}
	uint32 BitIndex;	// 0-based, where the LSB is 0 and MSB is 31
	_BitScanForward( (::DWORD *)&BitIndex, Value );	// Scans from LSB to MSB
	return BitIndex;
}
#else // PLATFORM_WINDOWS
FORCEINLINE uint32 appCountTrailingZeros(uint32 Value)
{
	if (Value == 0)
	{
		return 32;
	}
	return __builtin_ffs(Value) - 1;
}
#endif // PLATFORM_WINDOWS


/**
 * Merges the XYZ components of one vector with the W component of another vector and returns the result.
 *
 * @param VecXYZ	Source vector for XYZ_
 * @param VecW		Source register for ___W (note: the fourth component is used, not the first)
 * @return			VectorRegister(VecXYZ.x, VecXYZ.y, VecXYZ.z, VecW.w)
 */
#define VectorMergeVecXYZ_VecW( VecXYZ, VecW )	VectorSelect( GlobalVectorConstants::XYZMask, VecXYZ, VecW )

/**
 * Loads 4 BYTEs from unaligned memory and converts them into 4 FLOATs.
 * IMPORTANT: You need to call VectorResetFloatRegisters() before using scalar FLOATs after you've used this intrinsic!
 *
 * @param Ptr			Unaligned memory pointer to the 4 BYTEs.
 * @return				VectorRegister( float(Ptr[0]), float(Ptr[1]), float(Ptr[2]), float(Ptr[3]) )
 */
// Looks complex but is really quite straightforward:
// Load as 32-bit value, unpack 4x unsigned bytes to 4x 16-bit ints, then unpack again into 4x 32-bit ints, then convert to 4x floats
#define VectorLoadByte4( Ptr )			_mm_cvtepi32_ps(_mm_unpacklo_epi16(_mm_unpacklo_epi8(_mm_cvtsi32_si128(*(int32*)Ptr), _mm_setzero_si128()), _mm_setzero_si128()))

/**
 * Loads 4 BYTEs from unaligned memory and converts them into 4 FLOATs in reversed order.
 * IMPORTANT: You need to call VectorResetFloatRegisters() before using scalar FLOATs after you've used this intrinsic!
 *
 * @param Ptr			Unaligned memory pointer to the 4 BYTEs.
 * @return				VectorRegister( float(Ptr[3]), float(Ptr[2]), float(Ptr[1]), float(Ptr[0]) )
 */
FORCEINLINE VectorRegister VectorLoadByte4Reverse( void* Ptr )
{
	VectorRegister Temp = VectorLoadByte4(Ptr);
	return _mm_shuffle_ps( Temp, Temp, SHUFFLEMASK(3,2,1,0) );
}

/**
 * Converts the 4 FLOATs in the vector to 4 BYTEs, clamped to [0,255], and stores to unaligned memory.
 * IMPORTANT: You need to call VectorResetFloatRegisters() before using scalar FLOATs after you've used this intrinsic!
 *
 * @param Vec			Vector containing 4 FLOATs
 * @param Ptr			Unaligned memory pointer to store the 4 BYTEs.
 */
FORCEINLINE void VectorStoreByte4( const VectorRegister& Vec, void* Ptr )
{
	// Looks complex but is really quite straightforward:
	// Convert 4x floats to 4x 32-bit ints, then pack into 4x 16-bit ints, then into 4x 8-bit unsigned ints, then store as a 32-bit value
	*(int32*)Ptr = _mm_cvtsi128_si32(_mm_packus_epi16(_mm_packs_epi32(_mm_cvttps_epi32(Vec), _mm_setzero_si128()), _mm_setzero_si128()));
}

/**
 * Returns non-zero if any element in Vec1 is greater than the corresponding element in Vec2, otherwise 0.
 *
 * @param Vec1			1st source vector
 * @param Vec2			2nd source vector
 * @return				Non-zero integer if (Vec1.x > Vec2.x) || (Vec1.y > Vec2.y) || (Vec1.z > Vec2.z) || (Vec1.w > Vec2.w)
 */
#define VectorAnyGreaterThan( Vec1, Vec2 )		_mm_movemask_ps( _mm_cmpgt_ps(Vec1, Vec2) )

/**
 * Resets the floating point registers so that they can be used again.
 * Some intrinsics use these for MMX purposes (e.g. VectorLoadByte4 and VectorStoreByte4).
 */
// This is no longer necessary now that we don't use MMX instructions
#define VectorResetFloatRegisters()

/**
 * Returns the control register.
 *
 * @return			The uint32 control register
 */
#define VectorGetControlRegister()		_mm_getcsr()

/**
 * Sets the control register.
 *
 * @param ControlStatus		The uint32 control status value to set
 */
#define	VectorSetControlRegister(ControlStatus) _mm_setcsr( ControlStatus )

/**
 * Control status bit to round all floating point math results towards zero.
 */
#define VECTOR_ROUND_TOWARD_ZERO		_MM_ROUND_TOWARD_ZERO

/**
* Multiplies two quaternions; the order matters.
*
* Order matters when composing quaternions: C = VectorQuaternionMultiply2(A, B) will yield a quaternion C = A * B
* that logically first applies B then A to any subsequent transformation (right first, then left).
*
* @param Quat1	Pointer to the first quaternion
* @param Quat2	Pointer to the second quaternion
* @return Quat1 * Quat2
*/
FORCEINLINE VectorRegister VectorQuaternionMultiply2( const VectorRegister& Quat1, const VectorRegister& Quat2 )
{
	VectorRegister Result = VectorMultiply(VectorReplicate(Quat1, 3), Quat2);
	Result = VectorMultiplyAdd( VectorMultiply(VectorReplicate(Quat1, 0), VectorSwizzle(Quat2, 3,2,1,0)), GlobalVectorConstants::QMULTI_SIGN_MASK0, Result);
	Result = VectorMultiplyAdd( VectorMultiply(VectorReplicate(Quat1, 1), VectorSwizzle(Quat2, 2,3,0,1)), GlobalVectorConstants::QMULTI_SIGN_MASK1, Result);
	Result = VectorMultiplyAdd( VectorMultiply(VectorReplicate(Quat1, 2), VectorSwizzle(Quat2, 1,0,3,2)), GlobalVectorConstants::QMULTI_SIGN_MASK2, Result);

	return Result;
}

/**
* Multiplies two quaternions; the order matters.
*
* When composing quaternions: VectorQuaternionMultiply(C, A, B) will yield a quaternion C = A * B
* that logically first applies B then A to any subsequent transformation (right first, then left).
*
* @param Result	Pointer to where the result Quat1 * Quat2 should be stored
* @param Quat1	Pointer to the first quaternion (must not be the destination)
* @param Quat2	Pointer to the second quaternion (must not be the destination)
*/
FORCEINLINE void VectorQuaternionMultiply( void* RESTRICT Result, const void* RESTRICT Quat1, const void* RESTRICT Quat2)
{
	*((VectorRegister *)Result) = VectorQuaternionMultiply2(*((const VectorRegister *)Quat1), *((const VectorRegister *)Quat2));
}


/**
* Computes the sine and cosine of each component of a Vector.
*
* @param VSinAngles	VectorRegister Pointer to where the Sin result should be stored
* @param VCosAngles	VectorRegister Pointer to where the Cos result should be stored
* @param VAngles VectorRegister Pointer to the input angles 
*/
FORCEINLINE void VectorSinCos(  VectorRegister* VSinAngles, VectorRegister* VCosAngles, const VectorRegister* VAngles )
{	
	union { VectorRegister v; float f[4]; } VecSin, VecCos, VecAngles;
	VecAngles.v = *VAngles;

	FMath::SinCos(&VecSin.f[0], &VecCos.f[0], VecAngles.f[0]);
	FMath::SinCos(&VecSin.f[1], &VecCos.f[1], VecAngles.f[1]);
	FMath::SinCos(&VecSin.f[2], &VecCos.f[2], VecAngles.f[2]);
	FMath::SinCos(&VecSin.f[3], &VecCos.f[3], VecAngles.f[3]);

	*VSinAngles = VecSin.v;
	*VCosAngles = VecCos.v;
}

// Returns true if the vector contains a component that is either NAN or +/-infinite.
inline bool VectorContainsNaNOrInfinite(const VectorRegister& Vec)
{
	bool IsNotNAN = _mm_movemask_ps(_mm_cmpeq_ps(Vec, Vec)) != 0; // Test for the fact that NAN != NAN

	// Test for infinity, technique "stolen" from DirectXMathVector.inl

	// Mask off signs
	VectorRegister InfTest = _mm_and_ps(Vec, GlobalVectorConstants::SignMask);
	// Compare to infinity. If any are infinity, the signs are true.
	bool IsNotInf = _mm_movemask_ps(_mm_cmpeq_ps(InfTest, GlobalVectorConstants::FloatInfinity)) == 0;

	return !(IsNotNAN & IsNotInf);
}

FORCEINLINE VectorRegister VectorTruncate(const VectorRegister& X)
{
	return _mm_cvtepi32_ps(_mm_cvttps_epi32(X));
}

FORCEINLINE VectorRegister VectorFractional(const VectorRegister& X)
{
	return VectorSubtract(X, VectorTruncate(X));
}

FORCEINLINE VectorRegister VectorCeil(const VectorRegister& X)
{
	VectorRegister Trunc = VectorTruncate(X);
	VectorRegister PosMask = VectorCompareGE(X, GlobalVectorConstants::FloatZero);
	VectorRegister Add = VectorSelect(PosMask, GlobalVectorConstants::FloatOne, (GlobalVectorConstants::FloatZero));
	return VectorAdd(Trunc, Add);
}

FORCEINLINE VectorRegister VectorFloor(const VectorRegister& X)
{
	VectorRegister Trunc = VectorTruncate(X);
	VectorRegister PosMask = VectorCompareGE(X, (GlobalVectorConstants::FloatZero));
	VectorRegister Sub = VectorSelect(PosMask, (GlobalVectorConstants::FloatZero), (GlobalVectorConstants::FloatOne));
	return VectorSubtract(Trunc, Sub);
}

FORCEINLINE VectorRegister VectorMod(const VectorRegister& X, const VectorRegister& Y)
{
	VectorRegister Temp = VectorFloor(VectorDivide(X, Y));
	return VectorSubtract(X, VectorMultiply(Y, Temp));
}

FORCEINLINE VectorRegister VectorSign(const VectorRegister& X)
{
	VectorRegister Mask = VectorCompareGE(X, (GlobalVectorConstants::FloatZero));
	return VectorSelect(Mask, (GlobalVectorConstants::FloatOne), (GlobalVectorConstants::FloatMinusOne));
}

FORCEINLINE VectorRegister VectorStep(const VectorRegister& X)
{
	VectorRegister Mask = VectorCompareGE(X, (GlobalVectorConstants::FloatZero));
	return VectorSelect(Mask, (GlobalVectorConstants::FloatOne), (GlobalVectorConstants::FloatZero));
}

//TODO: Vectorize
FORCEINLINE VectorRegister VectorExp(const VectorRegister& X)
{
	return MakeVectorRegister((float)exp(VectorGetComponent(X, 0)), (float)exp(VectorGetComponent(X, 1)), (float)exp(VectorGetComponent(X, 2)), (float)exp(VectorGetComponent(X, 3)));
}

//TODO: Vectorize
FORCEINLINE VectorRegister VectorExp2(const VectorRegister& X)
{
	return MakeVectorRegister((float)exp2(VectorGetComponent(X, 0)), (float)exp2(VectorGetComponent(X, 1)), (float)exp2(VectorGetComponent(X, 2)), (float)exp2(VectorGetComponent(X, 3)));
}

//TODO: Vectorize
FORCEINLINE VectorRegister VectorLog(const VectorRegister& X)
{
	return MakeVectorRegister((float)log(VectorGetComponent(X, 0)), (float)log(VectorGetComponent(X, 1)), (float)log(VectorGetComponent(X, 2)), (float)log(VectorGetComponent(X, 3)));
}

//TODO: Vectorize
FORCEINLINE VectorRegister VectorLog2(const VectorRegister& X)
{
	return MakeVectorRegister((float)log2(VectorGetComponent(X, 0)), (float)log2(VectorGetComponent(X, 1)), (float)log2(VectorGetComponent(X, 2)), (float)log2(VectorGetComponent(X, 3)));
}

FORCEINLINE VectorRegister VectorSin(const VectorRegister& X)
{
	//Sine approximation using a squared parabola restrained to f(0) = 0, f(PI) = 0, f(PI/2) = 1.
	//based on a good discussion here http://forum.devmaster.net/t/fast-and-accurate-sine-cosine/9648
	//After approx 2.5 million tests comparing to sin(): 
	//Average error of 0.000128
	//Max error of 0.001091

	static const float p = 0.225f;
	static const VectorRegister P = MakeVectorRegister(p, p, p, p);
	static const float a = 16 * sqrt(p);
	static const VectorRegister A = MakeVectorRegister(a, a, a, a);
	static const float b = (1 - p) / sqrt(p);
	static const VectorRegister B = MakeVectorRegister(b, b, b, b);

	VectorRegister y = VectorMultiply(X, GlobalVectorConstants::OneOverTwoPi);
	y = VectorSubtract(y, VectorFloor(VectorAdd(y, GlobalVectorConstants::FloatOneHalf)));
	y = VectorMultiply(A, VectorMultiply(y, VectorSubtract(GlobalVectorConstants::FloatOneHalf, VectorAbs(y))));
	return VectorMultiply(y, VectorAdd(B, VectorAbs(y)));
}

FORCEINLINE VectorRegister VectorCos(const VectorRegister& X)
{
	return VectorSin(VectorAdd(X, GlobalVectorConstants::PiByTwo));
}

//TODO: Vectorize
FORCEINLINE VectorRegister VectorTan(const VectorRegister& X)
{
	return MakeVectorRegister((float)tan(VectorGetComponent(X, 0)), (float)tan(VectorGetComponent(X, 1)), (float)tan(VectorGetComponent(X, 2)), (float)tan(VectorGetComponent(X, 3)));
}

//TODO: Vectorize
FORCEINLINE VectorRegister VectorASin(const VectorRegister& X)
{
	return MakeVectorRegister((float)asin(VectorGetComponent(X, 0)), (float)asin(VectorGetComponent(X, 1)), (float)asin(VectorGetComponent(X, 2)), (float)asin(VectorGetComponent(X, 3)));
}

//TODO: Vectorize
FORCEINLINE VectorRegister VectorACos(const VectorRegister& X)
{
	return MakeVectorRegister((float)acos(VectorGetComponent(X, 0)), (float)acos(VectorGetComponent(X, 1)), (float)acos(VectorGetComponent(X, 2)), (float)acos(VectorGetComponent(X, 3)));
}

//TODO: Vectorize
FORCEINLINE VectorRegister VectorATan(const VectorRegister& X)
{
	return MakeVectorRegister((float)atan(VectorGetComponent(X, 0)), (float)atan(VectorGetComponent(X, 1)), (float)atan(VectorGetComponent(X, 2)), (float)atan(VectorGetComponent(X, 3)));
}

//TODO: Vectorize
FORCEINLINE VectorRegister VectorATan2(const VectorRegister& X, const VectorRegister& Y)
{
	return MakeVectorRegister(	(float)atan2(VectorGetComponent(X, 0), VectorGetComponent(Y, 0)), 
		(float)atan2(VectorGetComponent(X, 1), VectorGetComponent(Y, 1)),
		(float)atan2(VectorGetComponent(X, 2), VectorGetComponent(Y, 2)),
		(float)atan2(VectorGetComponent(X, 3), VectorGetComponent(Y, 3)));
}

// To be continued...

#endif

