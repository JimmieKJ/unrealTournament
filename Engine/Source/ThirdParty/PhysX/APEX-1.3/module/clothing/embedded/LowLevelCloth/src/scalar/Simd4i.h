// This code contains NVIDIA Confidential Information and is disclosed to you
// under a form of NVIDIA software license agreement provided separately to you.
//
// Notice
// NVIDIA Corporation and its licensors retain all intellectual property and
// proprietary rights in and to this software and related documentation and
// any modifications thereto. Any use, reproduction, disclosure, or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA Corporation is strictly prohibited.
//
// ALL NVIDIA DESIGN SPECIFICATIONS, CODE ARE PROVIDED "AS IS.". NVIDIA MAKES
// NO WARRANTIES, EXPRESSED, IMPLIED, STATUTORY, OR OTHERWISE WITH RESPECT TO
// THE MATERIALS, AND EXPRESSLY DISCLAIMS ALL IMPLIED WARRANTIES OF NONINFRINGEMENT,
// MERCHANTABILITY, AND FITNESS FOR A PARTICULAR PURPOSE.
//
// Information and code furnished is believed to be accurate and reliable.
// However, NVIDIA Corporation assumes no responsibility for the consequences of use of such
// information or for any infringement of patents or other rights of third parties that may
// result from its use. No license is granted by implication or otherwise under any patent
// or patent rights of NVIDIA Corporation. Details are subject to change without notice.
// This code supersedes and replaces all information previously supplied.
// NVIDIA Corporation products are not authorized for use as critical
// components in life support devices or systems without express written approval of
// NVIDIA Corporation.
//
// Copyright (c) 2008-2013 NVIDIA Corporation. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.  

#pragma once

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
// factory implementation
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

template <>
inline Simd4iFactory<const int&>::operator Scalar4i() const
{
	return Scalar4i(v, v, v, v);
}

inline Simd4iFactory<detail::FourTuple>::operator Scalar4i() const
{
	return reinterpret_cast<const Scalar4i&>(v);
}

template <int i>
inline Simd4iFactory<detail::IntType<i> >::operator Scalar4i() const
{
	return Scalar4i(i, i, i, i);
}

template <>
inline Simd4iFactory<const int*>::operator Scalar4i() const
{
	return Scalar4i(v[0], v[1], v[2], v[3]);
}

template <>
inline Simd4iFactory<detail::AlignedPointer<int> >::operator Scalar4i() const
{
	return Scalar4i(v.ptr[0], v.ptr[1], v.ptr[2], v.ptr[3]);
}

template <>
inline Simd4iFactory<detail::OffsetPointer<int> >::operator Scalar4i() const
{
	const int* ptr = reinterpret_cast<const int*>(
		reinterpret_cast<const char*>(v.ptr) + v.offset);
	return Scalar4i(ptr[0], ptr[1], ptr[2], ptr[3]);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
// operator implementations
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

namespace simdi
{

inline Scalar4i operator==( const Scalar4i& v0, const Scalar4i& v1 )
{
	return Scalar4i(v0.i4[0] == v1.i4[0], v0.i4[1] == v1.i4[1], 
		v0.i4[2] == v1.i4[2], v0.i4[3] == v1.i4[3]);
}

inline Scalar4i operator<( const Scalar4i& v0, const Scalar4i& v1 )
{
	return Scalar4i(v0.i4[0] < v1.i4[0], v0.i4[1] < v1.i4[1], 
		v0.i4[2] < v1.i4[2], v0.i4[3] < v1.i4[3]);
}

inline Scalar4i operator>( const Scalar4i& v0, const Scalar4i& v1 )
{
	return Scalar4i(v0.i4[0] > v1.i4[0], v0.i4[1] > v1.i4[1], 
		v0.i4[2] > v1.i4[2], v0.i4[3] > v1.i4[3]);
}

inline Scalar4i operator+( const Scalar4i& v0, const Scalar4i& v1 )
{
	return Scalar4i(v0.i4[0] + v1.i4[0], v0.i4[1] + v1.i4[1], 
		v0.i4[2] + v1.i4[2], v0.i4[3] + v1.i4[3]);
}

inline Scalar4i operator-( const Scalar4i& v )
{
	return Scalar4i(-v.i4[0], -v.i4[1], -v.i4[2], -v.i4[3]);
}

inline Scalar4i operator-( const Scalar4i& v0, const Scalar4i& v1 )
{
	return Scalar4i(v0.i4[0] - v1.i4[0], v0.i4[1] - v1.i4[1], 
		v0.i4[2] - v1.i4[2], v0.i4[3] - v1.i4[3]);
}

} // namespace simd

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
// function implementations
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

inline Scalar4i simd4i( const Scalar4f& v )
{
	return v;
}

namespace simdi
{

inline int (&array( Scalar4i& v ))[4]
{
	return v.i4;
}

inline const int (&array( const Scalar4i& v ))[4]
{
	return v.i4;
}

} // namespace simdi

inline void store( int* ptr, const Scalar4i& v )
{
	ptr[0] = v.i4[0];
	ptr[1] = v.i4[1];
	ptr[2] = v.i4[2];
	ptr[3] = v.i4[3];
}

inline void storeAligned( int* ptr, const Scalar4i& v )
{
	store(ptr, v);
}

inline void storeAligned( int* ptr, int offset, const Scalar4i& v )
{
	store( reinterpret_cast<int*>(
		reinterpret_cast<char*>(ptr) + offset), v );
}

namespace simdi
{

inline int allEqual( const Scalar4i& v0, const Scalar4i& v1 )
{
	return v0.i4[0] == v1.i4[0] && v0.i4[1] == v1.i4[1] && 
		v0.i4[2] == v1.i4[2] && v0.i4[3] == v1.i4[3];
}

inline int allEqual( const Scalar4i& v0, const Scalar4i& v1, Scalar4i& outMask )
{
	bool b0 = v0.i4[0] == v1.i4[0], b1 = v0.i4[1] == v1.i4[1],
		b2 = v0.i4[2] == v1.i4[2], b3 = v0.i4[3] == v1.i4[3];
	outMask = Scalar4f(b0, b1, b2, b3);
	return b0 && b1 && b2 && b3;
}

inline int anyEqual( const Scalar4i& v0, const Scalar4i& v1 )
{
	return v0.i4[0] == v1.i4[0] || v0.i4[1] == v1.i4[1] || 
		v0.i4[2] == v1.i4[2] || v0.i4[3] == v1.i4[3];
}

inline int anyEqual( const Scalar4i& v0, const Scalar4i& v1, Scalar4i& outMask )
{
	bool b0 = v0.i4[0] == v1.i4[0], b1 = v0.i4[1] == v1.i4[1],
		b2 = v0.i4[2] == v1.i4[2], b3 = v0.i4[3] == v1.i4[3];
	outMask = Scalar4f(b0, b1, b2, b3);
	return b0 || b1 || b2 || b3;
}

inline int allGreater( const Scalar4i& v0, const Scalar4i& v1 )
{
	return v0.i4[0] > v1.i4[0] && v0.i4[1] > v1.i4[1] && 
		v0.i4[2] > v1.i4[2] && v0.i4[3] > v1.i4[3];
}

inline int allGreater( const Scalar4i& v0, const Scalar4i& v1, Scalar4i& outMask )
{
	bool b0 = v0.i4[0] > v1.i4[0], b1 = v0.i4[1] > v1.i4[1],
		b2 = v0.i4[2] > v1.i4[2], b3 = v0.i4[3] > v1.i4[3];
	outMask = Scalar4f(b0, b1, b2, b3);
	return b0 && b1 && b2 && b3;
}

inline int anyGreater( const Scalar4i& v0, const Scalar4i& v1 )
{
	return v0.i4[0] > v1.i4[0] || v0.i4[1] > v1.i4[1] || 
		v0.i4[2] > v1.i4[2] || v0.i4[3] > v1.i4[3];
}

inline int anyGreater( const Scalar4i& v0, const Scalar4i& v1, Scalar4i& outMask )
{
	bool b0 = v0.i4[0] > v1.i4[0], b1 = v0.i4[1] > v1.i4[1],
		b2 = v0.i4[2] > v1.i4[2], b3 = v0.i4[3] > v1.i4[3];
	outMask = Scalar4f(b0, b1, b2, b3);
	return b0 || b1 || b2 || b3;
}

} // namespace simd
