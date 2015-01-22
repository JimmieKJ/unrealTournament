/* Copyright (c) <2003-2011> <Julio Jerez, Newton Game Dynamics>
* Modifications copyright (c) 2014-2015 Epic Games, Inc. All rights reserved.
* 
* This software is provided 'as-is', without any express or implied
* warranty. In no event will the authors be held liable for any damages
* arising from the use of this software.
* 
* Permission is granted to anyone to use this software for any purpose,
* including commercial applications, and to alter it and redistribute it
* freely, subject to the following restrictions:
* 
* 1. The origin of this software must not be misrepresented; you must not
* claim that you wrote the original software. If you use this software
* in a product, an acknowledgment in the product documentation would be
* appreciated but is not required.
* 
* 2. Altered source versions must be plainly marked as such, and must not be
* misrepresented as being the original software.
* 
* 3. This notice may not be removed or altered from any source distribution.
*/

#if !defined(AFX_SIMD_INTRUCTION_4563GFJK9R_H)
#define AFX_SIMD_INTRUCTION_4563GFJK9R_H


#include "dgTypes.h"

#ifdef DG_BUILD_SIMD_CODE
	#ifdef __ppc__
	
		#define PURMUT_MASK(w, z, y, x)   (vUInt8)       \
			(x * 4 + 0, x * 4 + 1, x * 4 + 2, x * 4 + 3, \
			 y * 4 + 0, y * 4 + 1, y * 4 + 2, y * 4 + 3, \
			 z * 4 + 0, z * 4 + 1, z * 4 + 2, z * 4 + 3, \
			 w * 4 + 0, w * 4 + 1, w * 4 + 2, w * 4 + 3) 

		union vFloatTuple
		{
			vFloat v;
			hacd::HaF32 f[4];
		};

/*
		#define simd_type					vFloat
		#define simd_char					vUInt8
		#define simd_env					vUInt16


		#define simd_set1(a)				(vFloat) ((hacd::HaF32)a, (hacd::HaF32)a, (hacd::HaF32)a, (hacd::HaF32)a)
		#define simd_load_s(a)				(vFloat) ((hacd::HaF32)a, (hacd::HaF32)a, (hacd::HaF32)a, (hacd::HaF32)a)
		#define simd_load1_v(a)				(vFloat) ((hacd::HaF32)a, (hacd::HaF32)a, (hacd::HaF32)a, (hacd::HaF32)a)

		#define simd_permut_v(a,b,mask)		vec_perm (a, b, mask)

		#define simd_or_v(a,b)				vec_or (a, b)	
		#define simd_and_v(a,b)				vec_and (a, b)	
		#define simd_add_v(a,b)				vec_add (a, b)	
		#define simd_sub_v(a,b)				vec_sub (a, b)	
		#define simd_min_v(a,b)				vec_min (a, b)
		#define simd_max_v(a,b)				vec_max (a, b)	
		#define simd_mul_v(a,b)				vec_madd (a, b, (simd_type) (0.0f, 0.0f, 0.0f, 0.0f))	
		#define simd_mul_add_v(a,b,c)		vec_madd (b, c, a)	
		#define simd_mul_sub_v(a,b,c)		vec_nmsub (b, c, a)	
		#define simd_cmpgt_v(a,b)			xxxxxx_mm_cmpgt_ps (a, b)	
		#define simd_rsqrt_v(a)				xxxx(a)	

		#define simd_add_s(a,b)				simd_add_v (a, b)	
		#define simd_sub_s(a,b)				simd_sub_v (a, b)	
		#define simd_mul_s(a,b)				simd_mul_v (a, b)	
		#define simd_min_s(a,b)				simd_min_v (a, b)	
		#define simd_max_s(a,b)				simd_max_v (a, b)	
		#define simd_mul_add_s(a,b,c)		simd_mul_add_v (a, b, c)	
		#define simd_mul_sub_s(a,b,c)		simd_mul_sub_v (a, b, c)	
		#define simd_cmpgt_s(a,b)			(simd_type) vec_cmpgt(a, b)
		#define simd_store_s(a,x)			{vFloatTuple __tmp; __tmp.v = x; a = __tmp.f[0];}
*/


		DG_MSC_VECTOR_ALIGMENT
		class simd_128
		{
			public:
			HACD_FORCE_INLINE simd_128 () {}
			HACD_FORCE_INLINE simd_128 (vFloat type): m_type(type) {}
			HACD_FORCE_INLINE simd_128 (hacd::HaF32 a): m_type((vFloat) (a, a, a, a)) {}
			HACD_FORCE_INLINE simd_128 (const simd_128& data): m_type(data.m_type) {}
			HACD_FORCE_INLINE simd_128 (hacd::HaI32 a) : m_type ((vFloat) (hacd::HaF32 (*(hacd::HaF32*)&a), hacd::HaF32 (*(hacd::HaF32*)&a), hacd::HaF32 (*(hacd::HaF32*)&a), hacd::HaF32 (*(hacd::HaF32*)&a))) {}
			HACD_FORCE_INLINE simd_128 (const hacd::HaF32* const ptr): m_type((vFloat) (hacd::HaF32 (ptr[0]), hacd::HaF32 (ptr[1]), hacd::HaF32 (ptr[2]), hacd::HaF32 (ptr[3]))) {}
			HACD_FORCE_INLINE simd_128 (hacd::HaF32 x, hacd::HaF32 y, hacd::HaF32 z, hacd::HaF32 w): m_type((vFloat) (x, y, z, w)) {}
			HACD_FORCE_INLINE simd_128 (hacd::HaI32 ix, hacd::HaI32 iy, hacd::HaI32 iz, hacd::HaI32 iw): m_type((vFloat) (hacd::HaF32 (*(hacd::HaF32*)&ix), hacd::HaF32 (*(hacd::HaF32*)&iy), hacd::HaF32 (*(hacd::HaF32*)&iz), hacd::HaF32 (*(hacd::HaF32*)&iw))) {}

/*
			HACD_FORCE_INLINE hacd::HaI32 GetInt () const
			{
				return _mm_cvtss_si32(m_type);
			}
*/

			HACD_FORCE_INLINE void StoreScalar(float* const scalar) const
			{
				//_mm_store_ss (scalar, m_type);
				vFloatTuple tmp;
				tmp.v = m_type; 
				*scalar = tmp.f[0];
			}
			
/*
			HACD_FORCE_INLINE void StoreVector(float* const array) const
			{
				_mm_storeu_ps (array, m_type);
			}

			HACD_FORCE_INLINE simd_128 operator= (const simd_128& data)
			{
				m_type = data.m_type;
				return (*this);
			}
*/

			HACD_FORCE_INLINE simd_128 operator+ (const simd_128& data) const
			{
				//return _mm_add_ps (m_type, data.m_type);
				return vec_add (m_type, data.m_type);	
			}

			HACD_FORCE_INLINE simd_128 operator- (const simd_128& data) const
			{
				//return _mm_sub_ps (m_type, data.m_type);
				return vec_sub (m_type, data.m_type);	
			}

			HACD_FORCE_INLINE simd_128 operator* (const simd_128& data) const
			{
				//return _mm_mul_ps (m_type, data.m_type);
				return vec_madd (m_type, data.m_type, (vFloat) (hacd::HaF32 (0.0f), hacd::HaF32 (0.0f), hacd::HaF32 (0.0f), hacd::HaF32 (0.0f)));	
			}

/*
			HACD_FORCE_INLINE simd_128 operator/ (const simd_128& data) const
			{
				return _mm_div_ps (m_type, data.m_type);	
			}



			HACD_FORCE_INLINE simd_128 operator<= (const simd_128& data) const
			{
				return _mm_cmple_ps (m_type, data.m_type);	
			}

			HACD_FORCE_INLINE simd_128 operator>= (const simd_128& data) const
			{
				return _mm_cmpge_ps (m_type, data.m_type);	
			}

			HACD_FORCE_INLINE simd_128 operator< (const simd_128& data) const
			{
				return _mm_cmplt_ps (m_type, data.m_type);	
			}

			HACD_FORCE_INLINE simd_128 operator> (const simd_128& data) const
			{
				return _mm_cmpgt_ps (m_type, data.m_type);	
			}


			HACD_FORCE_INLINE simd_128 operator& (const simd_128& data) const
			{
				return _mm_and_ps (m_type, data.m_type);	
			}

			HACD_FORCE_INLINE simd_128 operator| (const simd_128& data) const
			{
				return _mm_or_ps (m_type, data.m_type);	
			}

			HACD_FORCE_INLINE simd_128 AndNot (const simd_128& data) const
			{
				return _mm_andnot_ps (data.m_type, m_type);	
			}
*/		
			HACD_FORCE_INLINE simd_128 AddHorizontal () const
			{
				//simd_128 tmp (_mm_add_ps (m_type, _mm_shuffle_ps(m_type, m_type, PURMUT_MASK(2, 3, 0, 1))));
				//return _mm_add_ps (tmp.m_type, _mm_shuffle_ps(tmp.m_type, tmp.m_type, PURMUT_MASK(1, 0, 3, 2)));
				simd_128 tmp (vec_add (m_type, vec_perm (m_type, m_type,  PURMUT_MASK(2, 3, 0, 1))));
				return vec_add (tmp.m_type, vec_perm (tmp.m_type, tmp.m_type,  PURMUT_MASK(1, 0, 3, 2)));
			}


			HACD_FORCE_INLINE simd_128 DotProduct (const simd_128& data) const
			{
				simd_128 dot ((*this) * data);
				return dot.AddHorizontal();
			}


			HACD_FORCE_INLINE simd_128 CrossProduct (const simd_128& data) const
			{
//				return _mm_sub_ps (_mm_mul_ps (_mm_shuffle_ps (m_type, m_type, PURMUT_MASK(3, 0, 2, 1)), _mm_shuffle_ps (data.m_type, data.m_type, PURMUT_MASK(3, 1, 0, 2))),
//								   _mm_mul_ps (_mm_shuffle_ps (m_type, m_type, PURMUT_MASK(3, 1, 0, 2)), _mm_shuffle_ps (data.m_type, data.m_type, PURMUT_MASK(3, 0, 2, 1))));

				
				vFloat zero ((vFloat) (hacd::HaF32 (0.0f), hacd::HaF32 (0.0f), hacd::HaF32 (0.0f), hacd::HaF32 (0.0f)));		
				simd_128 a (vec_madd (zero, vec_perm (m_type, m_type, PURMUT_MASK(3, 0, 2, 1)), vec_perm (data.m_type, data.m_type, PURMUT_MASK(3, 1, 0, 2))));
				simd_128 b (vec_madd (zero, vec_perm (m_type, m_type, PURMUT_MASK(3, 1, 0, 2)), vec_perm (data.m_type, data.m_type, PURMUT_MASK(3, 0, 2, 1))));
				return vec_sub (a.m_type, b.m_type);
			}

/*

			HACD_FORCE_INLINE simd_128 Abs () const
			{
				__m128i shitSign = _mm_srli_epi32 (_mm_slli_epi32 (*((__m128i*) &m_type), 1), 1);
				return *(__m128*)&shitSign;
			}

			HACD_FORCE_INLINE simd_128 Floor () const
			{
				//const hacd::HaF32 magicConst = ;
				simd_128 mask ((hacd::HaF32 (1.5f) * hacd::HaF32 (1<<23)));
				simd_128 ret (_mm_sub_ps(_mm_add_ps(m_type, mask.m_type), mask.m_type));
				simd_128 adjust (_mm_cmplt_ps (m_type, ret.m_type));
				ret = _mm_sub_ps (ret.m_type, _mm_and_ps(_mm_set_ps1(1.0), adjust.m_type));
				HACD_ASSERT (ret.m_type.m128_f32[0] == dgFloor(m_type.m128_f32[0]));
				HACD_ASSERT (ret.m_type.m128_f32[1] == dgFloor(m_type.m128_f32[1]));
				HACD_ASSERT (ret.m_type.m128_f32[2] == dgFloor(m_type.m128_f32[2]));
				HACD_ASSERT (ret.m_type.m128_f32[3] == dgFloor(m_type.m128_f32[3]));
				return ret;
			}
			
			HACD_FORCE_INLINE hacd::HaI32 GetSignMask() const
			{
				return _mm_movemask_ps(m_type);
			} 

			HACD_FORCE_INLINE simd_128 InvSqrt () const
			{
				simd_128 half (hacd::HaF32 (0.5f));
				simd_128 three (hacd::HaF32 (3.0f));
				simd_128 tmp0 (_mm_rsqrt_ps(m_type));
				return half * tmp0 * (three - (*this) * tmp0 * tmp0);
			}

			HACD_FORCE_INLINE simd_128 GetMin (const simd_128& data) const
			{
				return _mm_min_ps (m_type, data.m_type);
			} 

			HACD_FORCE_INLINE simd_128 GetMax (const simd_128& data) const
			{
				return _mm_max_ps (m_type, data.m_type);
			} 

			HACD_FORCE_INLINE simd_128 MaximunValue() const
			{
				simd_128 tmp (GetMax (_mm_movehl_ps (m_type, m_type)));
				return tmp.GetMax (_mm_shuffle_ps(tmp.m_type, tmp.m_type, PURMUT_MASK(0, 0, 0, 1)));
			}

*/
			HACD_FORCE_INLINE simd_128 MoveHighToLow (const simd_128& data) const
			{
				//return _mm_movehl_ps (m_type, data.m_type);
				return vec_perm (m_type, data.m_type, PURMUT_MASK(7, 6, 3, 2));
			}

			HACD_FORCE_INLINE simd_128 MoveLowToHigh (const simd_128& data) const
			{
				//return _mm_movelh_ps (m_type, data.m_type);
				return vec_perm (m_type, data.m_type, PURMUT_MASK(5, 4, 1, 0));
			}

			HACD_FORCE_INLINE simd_128 PackLow (const simd_128& data) const
			{
				return vec_perm (m_type, data.m_type, PURMUT_MASK(5, 1, 4, 0));
			}

			HACD_FORCE_INLINE simd_128 PackHigh (const simd_128& data) const
			{
				//return _mm_unpackhi_ps (m_type, data.m_type);
				return vec_perm (m_type, data.m_type, PURMUT_MASK(5, 6, 3, 2));
			}

/*
			HACD_FORCE_INLINE simd_128 ShiftTripleRight () const
			{
				return _mm_shuffle_ps(m_type, m_type, PURMUT_MASK(3, 1, 0, 2));
			}

			HACD_FORCE_INLINE simd_128 ShiftTripleLeft () const
			{
				return _mm_shuffle_ps(m_type, m_type, PURMUT_MASK(3, 0, 2, 1));
			}

*/
			vFloat m_type;
		}DG_GCC_VECTOR_ALIGMENT;


	#else
	
		#define simd_env					hacd::HaU32
		#define simd_get_ctrl()				_mm_getcsr ()
		#define simd_set_ctrl(a)			_mm_setcsr (a)
		#define simd_set_FZ_mode()			_MM_SET_FLUSH_ZERO_MODE (_MM_FLUSH_ZERO_ON)
		#define PURMUT_MASK(w, z, y, x)		_MM_SHUFFLE (w, z, y, x)

		DG_MSC_VECTOR_ALIGMENT
		class simd_128
		{
			public:
			HACD_FORCE_INLINE simd_128 () {}
			HACD_FORCE_INLINE simd_128 (__m128 type): m_type(type) {}
			HACD_FORCE_INLINE simd_128 (hacd::HaF32 a): m_type(_mm_set_ps1(a)) {}
			HACD_FORCE_INLINE simd_128 (const simd_128& data): m_type(data.m_type) {}
			HACD_FORCE_INLINE simd_128 (hacd::HaI32 a): m_type (_mm_set_ps1 (*(hacd::HaF32*)&a)){}
			HACD_FORCE_INLINE simd_128 (const hacd::HaF32* const ptr): m_type(_mm_loadu_ps (ptr)) {}
			HACD_FORCE_INLINE simd_128 (hacd::HaF32 x, hacd::HaF32 y, hacd::HaF32 z, hacd::HaF32 w): m_type(_mm_set_ps(w, z, y, x)) {}
			HACD_FORCE_INLINE simd_128 (hacd::HaI32 ix, hacd::HaI32 iy, hacd::HaI32 iz, hacd::HaI32 iw): m_type(_mm_set_ps(*(hacd::HaF32*)&iw, *(hacd::HaF32*)&iz, *(hacd::HaF32*)&iy, *(hacd::HaF32*)&ix)) {}
			
			HACD_FORCE_INLINE hacd::HaI32 GetInt () const
			{
				return _mm_cvtss_si32(m_type);
			}

			HACD_FORCE_INLINE void StoreScalar(float* const scalar) const
			{
				_mm_store_ss (scalar, m_type);
			}

			HACD_FORCE_INLINE void StoreVector(float* const array) const
			{
				_mm_storeu_ps (array, m_type);
			}

			HACD_FORCE_INLINE simd_128 operator= (const simd_128& data)
			{
				m_type = data.m_type;
				return (*this);
			}

			HACD_FORCE_INLINE simd_128 operator+ (const simd_128& data) const
			{
				return _mm_add_ps (m_type, data.m_type);	
			}

			HACD_FORCE_INLINE simd_128 operator- (const simd_128& data) const
			{
				return _mm_sub_ps (m_type, data.m_type);	
			}

			HACD_FORCE_INLINE simd_128 operator* (const simd_128& data) const
			{
				return _mm_mul_ps (m_type, data.m_type);	
			}

			HACD_FORCE_INLINE simd_128 operator/ (const simd_128& data) const
			{
				return _mm_div_ps (m_type, data.m_type);	
			}



			HACD_FORCE_INLINE simd_128 operator<= (const simd_128& data) const
			{
				return _mm_cmple_ps (m_type, data.m_type);	
			}

			HACD_FORCE_INLINE simd_128 operator>= (const simd_128& data) const
			{
				return _mm_cmpge_ps (m_type, data.m_type);	
			}

			HACD_FORCE_INLINE simd_128 operator< (const simd_128& data) const
			{
				return _mm_cmplt_ps (m_type, data.m_type);	
			}

			HACD_FORCE_INLINE simd_128 operator> (const simd_128& data) const
			{
				return _mm_cmpgt_ps (m_type, data.m_type);	
			}


			HACD_FORCE_INLINE simd_128 operator& (const simd_128& data) const
			{
				return _mm_and_ps (m_type, data.m_type);	
			}

			HACD_FORCE_INLINE simd_128 operator| (const simd_128& data) const
			{
				return _mm_or_ps (m_type, data.m_type);	
			}

			HACD_FORCE_INLINE simd_128 AndNot (const simd_128& data) const
			{
				return _mm_andnot_ps (data.m_type, m_type);	
			}
		
			HACD_FORCE_INLINE simd_128 AddHorizontal () const
			{
				simd_128 tmp (_mm_add_ps (m_type, _mm_shuffle_ps(m_type, m_type, PURMUT_MASK(2, 3, 0, 1))));
				return _mm_add_ps (tmp.m_type, _mm_shuffle_ps(tmp.m_type, tmp.m_type, PURMUT_MASK(1, 0, 3, 2)));
			}

			HACD_FORCE_INLINE simd_128 DotProduct (const simd_128& data) const
			{
				simd_128 dot ((*this) * data);
				return dot.AddHorizontal();
			}

			HACD_FORCE_INLINE simd_128 CrossProduct (const simd_128& data) const
			{
				return _mm_sub_ps (_mm_mul_ps (_mm_shuffle_ps (m_type, m_type, PURMUT_MASK(3, 0, 2, 1)), _mm_shuffle_ps (data.m_type, data.m_type, PURMUT_MASK(3, 1, 0, 2))),
								   _mm_mul_ps (_mm_shuffle_ps (m_type, m_type, PURMUT_MASK(3, 1, 0, 2)), _mm_shuffle_ps (data.m_type, data.m_type, PURMUT_MASK(3, 0, 2, 1))));
			}

			HACD_FORCE_INLINE simd_128 Abs () const
			{
				__m128i shitSign = _mm_srli_epi32 (_mm_slli_epi32 (*((__m128i*) &m_type), 1), 1);
				return *(__m128*)&shitSign;
			}

			HACD_FORCE_INLINE simd_128 Floor () const
			{
				//const hacd::HaF32 magicConst = ;
				simd_128 mask ((hacd::HaF32 (1.5f) * hacd::HaF32 (1<<23)));
				simd_128 ret (_mm_sub_ps(_mm_add_ps(m_type, mask.m_type), mask.m_type));
				simd_128 adjust (_mm_cmplt_ps (m_type, ret.m_type));
				ret = _mm_sub_ps (ret.m_type, _mm_and_ps(_mm_set_ps1(1.0), adjust.m_type));
#if !PLATFORM_MAC || !PLATFORM_LINUX // @todo Mac
				HACD_ASSERT (ret.m_type.m128_f32[0] == dgFloor(m_type.m128_f32[0]));
				HACD_ASSERT (ret.m_type.m128_f32[1] == dgFloor(m_type.m128_f32[1]));
				HACD_ASSERT (ret.m_type.m128_f32[2] == dgFloor(m_type.m128_f32[2]));
				HACD_ASSERT (ret.m_type.m128_f32[3] == dgFloor(m_type.m128_f32[3]));
#endif
				return ret;
			}
			
			HACD_FORCE_INLINE hacd::HaI32 GetSignMask() const
			{
				return _mm_movemask_ps(m_type);
			} 

			HACD_FORCE_INLINE simd_128 InvSqrt () const
			{
				simd_128 half (hacd::HaF32 (0.5f));
				simd_128 three (hacd::HaF32 (3.0f));
				simd_128 tmp0 (_mm_rsqrt_ps(m_type));
				return half * tmp0 * (three - (*this) * tmp0 * tmp0);
			}

			HACD_FORCE_INLINE simd_128 GetMin (const simd_128& data) const
			{
				return _mm_min_ps (m_type, data.m_type);
			} 

			HACD_FORCE_INLINE simd_128 GetMax (const simd_128& data) const
			{
				return _mm_max_ps (m_type, data.m_type);
			} 

			HACD_FORCE_INLINE simd_128 MaximunValue() const
			{
				simd_128 tmp (GetMax (_mm_movehl_ps (m_type, m_type)));
				return tmp.GetMax (_mm_shuffle_ps(tmp.m_type, tmp.m_type, PURMUT_MASK(0, 0, 0, 1)));
			}


			HACD_FORCE_INLINE simd_128 MoveHighToLow (const simd_128& data) const
			{
				return _mm_movehl_ps (m_type, data.m_type);
			}

			HACD_FORCE_INLINE simd_128 MoveLowToHigh (const simd_128& data) const
			{
				return _mm_movelh_ps (m_type, data.m_type);
			}

			HACD_FORCE_INLINE simd_128 PackLow (const simd_128& data) const
			{
				return _mm_unpacklo_ps (m_type, data.m_type);
			}

			HACD_FORCE_INLINE simd_128 PackHigh (const simd_128& data) const
			{
				return _mm_unpackhi_ps (m_type, data.m_type);
			}

			HACD_FORCE_INLINE simd_128 ShiftTripleRight () const
			{
				return _mm_shuffle_ps(m_type, m_type, PURMUT_MASK(3, 1, 0, 2));
			}

			HACD_FORCE_INLINE simd_128 ShiftTripleLeft () const
			{
				return _mm_shuffle_ps(m_type, m_type, PURMUT_MASK(3, 0, 2, 1));
			}


			__m128 m_type;
		}DG_GCC_VECTOR_ALIGMENT;
	#endif
#endif


HACD_FORCE_INLINE void Transpose4x4Simd_128 (simd_128& dst0, simd_128& dst1, simd_128& dst2, simd_128& dst3, 
							         const simd_128& src0, const simd_128& src1, const simd_128& src2, const simd_128& src3)
{
	simd_128 tmp0 (src0.PackLow(src1));
	simd_128 tmp1 (src2.PackLow(src3));
	dst0 = tmp0.MoveLowToHigh (tmp1);
	dst1 = tmp1.MoveHighToLow (tmp0);

	tmp0 = src0.PackHigh(src1);
	tmp1 = src2.PackHigh(src3);
	dst2 = tmp0.MoveLowToHigh (tmp1);
	dst3 = tmp1.MoveHighToLow (tmp0);
}


#endif

