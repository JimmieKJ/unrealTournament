/*
 * Copyright (c) 2008-2015, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.  


#include "GuBarycentricCoordinates.h"

using namespace physx;
using namespace Ps::aos;

void Gu::barycentricCoordinates(const Vec3VArg p, const Vec3VArg a, const Vec3VArg b, FloatV& v)
{
	const Vec3V v0 = V3Sub(a, p);
	const Vec3V v1 = V3Sub(b, p);
	const Vec3V d = V3Sub(v1, v0);
	const FloatV denominator = V3Dot(d, d);
	const FloatV numerator = V3Dot(V3Neg(v0), d);
	v = FDiv(numerator, denominator);
}

void Gu::barycentricCoordinates(const Ps::aos::Vec3VArg p, const Ps::aos::Vec3VArg a, const Ps::aos::Vec3VArg b, const Ps::aos::Vec3VArg c, Ps::aos::FloatV& v, Ps::aos::FloatV& w)
{
	const Vec3V v0 = V3Sub(b, a);
	const Vec3V v1 = V3Sub(c, a);
	const Vec3V v2 = V3Sub(p, a);
	const FloatV d00 = V3Dot(v0, v0);
	const FloatV d01 = V3Dot(v0, v1);
	const FloatV d11 = V3Dot(v1, v1);
	const FloatV d20 = V3Dot(v2, v0);
	const FloatV d21 = V3Dot(v2, v1);
	const FloatV d00d11 = FMul(d00, d11);
	const FloatV d01d01 = FMul(d01, d01);
	const FloatV d11d20 = FMul(d11, d20);
	const FloatV d01d21 = FMul(d01, d21);
	const FloatV d00d21 = FMul(d00, d21);
	const FloatV d01d20 = FMul(d01, d20);
	const FloatV zero = FZero();
	const FloatV dif = FSub(d00d11, d01d01);
	const FloatV denom = FSel(FIsEq(dif, zero), zero, FRecip(dif));
	v = FMul(FSub(d11d20, d01d21), denom);
	w = FMul(FSub(d00d21, d01d20), denom);
}

/*
	v0 = b - a;
	v1 = c - a;
	v2 = p - a;
*/
void Gu::barycentricCoordinates(const Vec3VArg v0, const Vec3VArg v1, const Vec3VArg v2, FloatV& v, FloatV& w)
{
	const FloatV d00 = V3Dot(v0, v0);
	const FloatV d01 = V3Dot(v0, v1);
	const FloatV d11 = V3Dot(v1, v1);
	const FloatV d20 = V3Dot(v2, v0);
	const FloatV d21 = V3Dot(v2, v1);
	const FloatV denom = FRecip(FSub(FMul(d00,d11), FMul(d01, d01)));
	v = FMul(FSub(FMul(d11, d20), FMul(d01, d21)), denom);
	w = FMul(FSub(FMul(d00, d21), FMul(d01, d20)), denom);
}

