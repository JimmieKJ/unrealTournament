/************************************************************************************

Filename    :   OVR_Math.h
Content     :   Implementation of 3D primitives such as vectors, matrices.
Created     :   September 4, 2012
Authors     :   Andrew Reisse, Michael Antonov, Anna Yershova

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#include "OVR_Math.h"
#include "OVR_Log.h"

#include <float.h>


namespace OVR {


//-------------------------------------------------------------------------------------
// ***** Math


// Single-precision Math constants class.
const float Math<float>::Pi      = 3.1415926f;
const float Math<float>::TwoPi   = 3.1415926f * 2;
const float Math<float>::PiOver2 = 3.1415926f / 2.0f;
const float Math<float>::PiOver4 = 3.1415926f / 4.0f;
const float Math<float>::E       = 2.7182818f;

const float Math<float>::MaxValue				= FLT_MAX;
const float Math<float>::MinPositiveValue		= FLT_MIN;

const float Math<float>::RadToDegreeFactor		= 360.0f / Math<float>::TwoPi;
const float Math<float>::DegreeToRadFactor		= Math<float>::TwoPi / 360.0f;

const float Math<float>::Tolerance				= 0.00001f;
const float Math<float>::SingularityRadius		= 0.0000001f; // Use for Gimbal lock numerical problems

const float Math<float>::SmallestNonDenormal	= float( 1.1754943508222875e-038f );	// ( 1U << 23 )
const float Math<float>::HugeNumber				= float( 1.8446742974197924e+019f );	// ( ( ( 127U * 3 / 2 ) << 23 ) | ( ( 1 << 23 ) - 1 ) )

// Double-precision Math constants class.
const double Math<double>::Pi      = 3.14159265358979;
const double Math<double>::TwoPi   = 3.14159265358979 * 2;
const double Math<double>::PiOver2 = 3.14159265358979 / 2.0;
const double Math<double>::PiOver4 = 3.14159265358979 / 4.0;
const double Math<double>::E       = 2.71828182845905;

const double Math<double>::MaxValue				= DBL_MAX;
const double Math<double>::MinPositiveValue		= DBL_MIN;

const double Math<double>::RadToDegreeFactor	= 360.0 / Math<double>::TwoPi;
const double Math<double>::DegreeToRadFactor	= Math<double>::TwoPi / 360.0;

const double Math<double>::Tolerance			= 0.00001;
const double Math<double>::SingularityRadius	= 0.000000000001; // Use for Gimbal lock numerical problems

const double Math<double>::SmallestNonDenormal	= double( 2.2250738585072014e-308 );	// ( 1ULL << 52 )
const double Math<double>::HugeNumber			= double( 1.3407807929942596e+154 );	// ( ( ( 1023ULL * 3 / 2 ) << 52 ) | ( ( 1 << 52 ) - 1 ) )


//-------------------------------------------------------------------------------------
// ***** Constants

template<>
const Vector2<float> Vector2<float>::ZERO = Vector2<float>();

template<>
const Vector2<double> Vector2<double>::ZERO = Vector2<double>();

template<>
const Vector3<float> Vector3<float>::ZERO = Vector3<float>();

template<>
const Vector3<double> Vector3<double>::ZERO = Vector3<double>();

template<>
const Vector4<float> Vector4<float>::ZERO = Vector4<float>();

template<>
const Vector4<double> Vector4<double>::ZERO = Vector4<double>();

template<>
const Matrix4<float> Matrix4<float>::IdentityValue = Matrix4<float>(1.0f, 0.0f, 0.0f, 0.0f, 
                                                                    0.0f, 1.0f, 0.0f, 0.0f, 
                                                                    0.0f, 0.0f, 1.0f, 0.0f,
                                                                    0.0f, 0.0f, 0.0f, 1.0f);

template<>
const Matrix4<double> Matrix4<double>::IdentityValue = Matrix4<double>(1.0, 0.0, 0.0, 0.0, 
                                                                       0.0, 1.0, 0.0, 0.0, 
                                                                       0.0, 0.0, 1.0, 0.0,
                                                                       0.0, 0.0, 0.0, 1.0);



} // Namespace OVR
