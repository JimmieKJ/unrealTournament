/*
 * Copyright 1993-2012 NVIDIA Corporation.  All rights reserved.
 *
 * NOTICE TO LICENSEE:
 *
 * This source code and/or documentation ("Licensed Deliverables") are
 * subject to NVIDIA intellectual property rights under U.S. and
 * international Copyright laws.
 *
 * These Licensed Deliverables contained herein is PROPRIETARY and
 * CONFIDENTIAL to NVIDIA and is being provided under the terms and
 * conditions of a form of NVIDIA software license agreement by and
 * between NVIDIA and Licensee ("License Agreement") or electronically
 * accepted by Licensee.  Notwithstanding any terms or conditions to
 * the contrary in the License Agreement, reproduction or disclosure
 * of the Licensed Deliverables to any third party without the express
 * written consent of NVIDIA is prohibited.
 *
 * NOTWITHSTANDING ANY TERMS OR CONDITIONS TO THE CONTRARY IN THE
 * LICENSE AGREEMENT, NVIDIA MAKES NO REPRESENTATION ABOUT THE
 * SUITABILITY OF THESE LICENSED DELIVERABLES FOR ANY PURPOSE.  IT IS
 * PROVIDED "AS IS" WITHOUT EXPRESS OR IMPLIED WARRANTY OF ANY KIND.
 * NVIDIA DISCLAIMS ALL WARRANTIES WITH REGARD TO THESE LICENSED
 * DELIVERABLES, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY,
 * NONINFRINGEMENT, AND FITNESS FOR A PARTICULAR PURPOSE.
 * NOTWITHSTANDING ANY TERMS OR CONDITIONS TO THE CONTRARY IN THE
 * LICENSE AGREEMENT, IN NO EVENT SHALL NVIDIA BE LIABLE FOR ANY
 * SPECIAL, INDIRECT, INCIDENTAL, OR CONSEQUENTIAL DAMAGES, OR ANY
 * DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
 * WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THESE LICENSED DELIVERABLES.
 *
 * U.S. Government End Users.  These Licensed Deliverables are a
 * "commercial item" as that term is defined at 48 C.F.R. 2.101 (OCT
 * 1995), consisting of "commercial computer software" and "commercial
 * computer software documentation" as such terms are used in 48
 * C.F.R. 12.212 (SEPT 1995) and is provided to the U.S. Government
 * only as a commercial end item.  Consistent with 48 C.F.R.12.212 and
 * 48 C.F.R. 227.7202-1 through 227.7202-4 (JUNE 1995), all
 * U.S. Government End Users acquire the Licensed Deliverables with
 * only those rights set forth herein.
 *
 * Any use of the Licensed Deliverables in individual and commercial
 * software must include, in the user documentation and internal
 * comments to the code, the above Disclaimer and U.S. Government End
 * Users Notice.
 */


#ifndef __TEXTURE_INDIRECT_FUNCTIONS_H__
#define __TEXTURE_INDIRECT_FUNCTIONS_H__

#if defined(__cplusplus) && defined(__CUDACC__)

#if !defined(__CUDA_ARCH__) || __CUDA_ARCH__ >= 200


#include "builtin_types.h"
#include "host_defines.h"
#include "vector_functions.h"


/*******************************************************************************
*                                                                              *
* 1D Linear Texture indirect fetch functions
*                                                                              *
*******************************************************************************/

static __forceinline__ __device__ void tex1Dfetch(char *retVal, cudaTextureObject_t texObject, int x)
{
#if defined(_CHAR_UNSIGNED) || defined(__CHAR_UNSIGNED__)
    uint4 tmp;
    asm volatile ("tex.1d.v4.u32.s32 {%0, %1, %2, %3}, [%4, {%5}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(x));
#else /* _CHAR_UNSIGNED || __CHAR_UNSIGNED__ */
    int4 tmp;
    asm volatile ("tex.1d.v4.s32.s32 {%0, %1, %2, %3}, [%4, {%5}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(x));
#endif /* _CHAR_UNSIGNED || __CHAR_UNSIGNED__ */
    *retVal = (char)tmp.x;
}
static __forceinline__ __device__ void tex1Dfetch(signed char *retVal, cudaTextureObject_t texObject, int x)
{
   int4 tmp;
   asm volatile ("tex.1d.v4.s32.s32 {%0, %1, %2, %3}, [%4, {%5}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(x));
   *retVal = (signed char)(tmp.x);
}

static __forceinline__ __device__ void tex1Dfetch(char1 *retVal, cudaTextureObject_t texObject, int x)
{
   int4 tmp;
   asm volatile ("tex.1d.v4.s32.s32 {%0, %1, %2, %3}, [%4, {%5}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(x));
   *retVal = make_char1(tmp.x);
}

static __forceinline__ __device__ void tex1Dfetch(char2 *retVal, cudaTextureObject_t texObject, int x)
{
   int4 tmp;
   asm volatile ("tex.1d.v4.s32.s32 {%0, %1, %2, %3}, [%4, {%5}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(x));
   *retVal = make_char2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex1Dfetch(char4 *retVal, cudaTextureObject_t texObject, int x)
{
   int4 tmp;
   asm volatile ("tex.1d.v4.s32.s32 {%0, %1, %2, %3}, [%4, {%5}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(x));
   *retVal = make_char4(tmp.x, tmp.y, tmp.z, tmp.w);
}

static __forceinline__ __device__ void tex1Dfetch(unsigned char *retVal, cudaTextureObject_t texObject, int x)
{
   uint4 tmp;
   asm volatile ("tex.1d.v4.u32.s32 {%0, %1, %2, %3}, [%4, {%5}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(x));
   *retVal = (unsigned char)(tmp.x);
}

static __forceinline__ __device__ void tex1Dfetch(uchar1 *retVal, cudaTextureObject_t texObject, int x)
{
   uint4 tmp;
   asm volatile ("tex.1d.v4.u32.s32 {%0, %1, %2, %3}, [%4, {%5}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(x));
   *retVal = make_uchar1(tmp.x);
}

static __forceinline__ __device__ void tex1Dfetch(uchar2 *retVal, cudaTextureObject_t texObject, int x)
{
   uint4 tmp;
   asm volatile ("tex.1d.v4.u32.s32 {%0, %1, %2, %3}, [%4, {%5}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(x));
   *retVal = make_uchar2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex1Dfetch(uchar4 *retVal, cudaTextureObject_t texObject, int x)
{
   uint4 tmp;
   asm volatile ("tex.1d.v4.u32.s32 {%0, %1, %2, %3}, [%4, {%5}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(x));
   *retVal = make_uchar4(tmp.x, tmp.y, tmp.z, tmp.w);
}

/*******************************************************************************
*                                                                              *
*                                                                              *
*                                                                              *
*******************************************************************************/

static __forceinline__ __device__ void tex1Dfetch(short *retVal, cudaTextureObject_t texObject, int x)
{
   int4 tmp;
   asm volatile ("tex.1d.v4.s32.s32 {%0, %1, %2, %3}, [%4, {%5}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(x));
   *retVal = (short)(tmp.x);
}

static __forceinline__ __device__ void tex1Dfetch(short1 *retVal, cudaTextureObject_t texObject, int x)
{
   int4 tmp;
   asm volatile ("tex.1d.v4.s32.s32 {%0, %1, %2, %3}, [%4, {%5}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(x));
   *retVal = make_short1(tmp.x);
}

static __forceinline__ __device__ void tex1Dfetch(short2 *retVal, cudaTextureObject_t texObject, int x)
{
   int4 tmp;
   asm volatile ("tex.1d.v4.s32.s32 {%0, %1, %2, %3}, [%4, {%5}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(x));
   *retVal = make_short2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex1Dfetch(short4 *retVal, cudaTextureObject_t texObject, int x)
{
   int4 tmp;
   asm volatile ("tex.1d.v4.s32.s32 {%0, %1, %2, %3}, [%4, {%5}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(x));
   *retVal = make_short4(tmp.x, tmp.y, tmp.z, tmp.w);
}

static __forceinline__ __device__ void tex1Dfetch(unsigned short *retVal, cudaTextureObject_t texObject, int x)
{
   uint4 tmp;
   asm volatile ("tex.1d.v4.u32.s32 {%0, %1, %2, %3}, [%4, {%5}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(x));
   *retVal = (unsigned short)(tmp.x);
}

static __forceinline__ __device__ void tex1Dfetch(ushort1 *retVal, cudaTextureObject_t texObject, int x)
{
   uint4 tmp;
   asm volatile ("tex.1d.v4.u32.s32 {%0, %1, %2, %3}, [%4, {%5}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(x));
   *retVal = make_ushort1(tmp.x);
}

static __forceinline__ __device__ void tex1Dfetch(ushort2 *retVal, cudaTextureObject_t texObject, int x)
{
   uint4 tmp;
   asm volatile ("tex.1d.v4.u32.s32 {%0, %1, %2, %3}, [%4, {%5}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(x));
   *retVal = make_ushort2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex1Dfetch(ushort4 *retVal, cudaTextureObject_t texObject, int x)
{
   uint4 tmp;
   asm volatile ("tex.1d.v4.u32.s32 {%0, %1, %2, %3}, [%4, {%5}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(x));
   *retVal = make_ushort4(tmp.x, tmp.y, tmp.z, tmp.w);
}

/*******************************************************************************
*                                                                              *
*                                                                              *
*                                                                              *
*******************************************************************************/

static __forceinline__ __device__ void tex1Dfetch(int *retVal, cudaTextureObject_t texObject, int x)
{
   int4 tmp;
   asm volatile ("tex.1d.v4.s32.s32 {%0, %1, %2, %3}, [%4, {%5}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(x));
   *retVal = (int)(tmp.x);
}

static __forceinline__ __device__ void tex1Dfetch(int1 *retVal, cudaTextureObject_t texObject, int x)
{
   int4 tmp;
   asm volatile ("tex.1d.v4.s32.s32 {%0, %1, %2, %3}, [%4, {%5}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(x));
   *retVal = make_int1(tmp.x);
}

static __forceinline__ __device__ void tex1Dfetch(int2 *retVal, cudaTextureObject_t texObject, int x)
{
   int4 tmp;
   asm volatile ("tex.1d.v4.s32.s32 {%0, %1, %2, %3}, [%4, {%5}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(x));
   *retVal = make_int2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex1Dfetch(int4 *retVal, cudaTextureObject_t texObject, int x)
{
   int4 tmp;
   asm volatile ("tex.1d.v4.s32.s32 {%0, %1, %2, %3}, [%4, {%5}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(x));
   *retVal = make_int4(tmp.x, tmp.y, tmp.z, tmp.w);
}

static __forceinline__ __device__ void tex1Dfetch(unsigned int *retVal, cudaTextureObject_t texObject, int x)
{
   uint4 tmp;
   asm volatile ("tex.1d.v4.u32.s32 {%0, %1, %2, %3}, [%4, {%5}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(x));
   *retVal = (unsigned int)(tmp.x);
}

static __forceinline__ __device__ void tex1Dfetch(uint1 *retVal, cudaTextureObject_t texObject, int x)
{
   uint4 tmp;
   asm volatile ("tex.1d.v4.u32.s32 {%0, %1, %2, %3}, [%4, {%5}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(x));
   *retVal = make_uint1(tmp.x);
}

static __forceinline__ __device__ void tex1Dfetch(uint2 *retVal, cudaTextureObject_t texObject, int x)
{
   uint4 tmp;
   asm volatile ("tex.1d.v4.u32.s32 {%0, %1, %2, %3}, [%4, {%5}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(x));
   *retVal = make_uint2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex1Dfetch(uint4 *retVal, cudaTextureObject_t texObject, int x)
{
   uint4 tmp;
   asm volatile ("tex.1d.v4.u32.s32 {%0, %1, %2, %3}, [%4, {%5}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(x));
   *retVal = make_uint4(tmp.x, tmp.y, tmp.z, tmp.w);
}

/*******************************************************************************
*                                                                              *
*                                                                              *
*                                                                              *
*******************************************************************************/

#if !defined(__LP64__)

static __forceinline__ __device__ void tex1Dfetch(long *retVal, cudaTextureObject_t texObject, int x)
{
   int4 tmp;
   asm volatile ("tex.1d.v4.s32.s32 {%0, %1, %2, %3}, [%4, {%5}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(x));
   *retVal = (long)(tmp.x);
}

static __forceinline__ __device__ void tex1Dfetch(long1 *retVal, cudaTextureObject_t texObject, int x)
{
   int4 tmp;
   asm volatile ("tex.1d.v4.s32.s32 {%0, %1, %2, %3}, [%4, {%5}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(x));
   *retVal = make_long1(tmp.x);
}

static __forceinline__ __device__ void tex1Dfetch(long2 *retVal, cudaTextureObject_t texObject, int x)
{
   int4 tmp;
   asm volatile ("tex.1d.v4.s32.s32 {%0, %1, %2, %3}, [%4, {%5}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(x));
   *retVal = make_long2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex1Dfetch(long4 *retVal, cudaTextureObject_t texObject, int x)
{
   int4 tmp;
   asm volatile ("tex.1d.v4.s32.s32 {%0, %1, %2, %3}, [%4, {%5}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(x));
   *retVal = make_long4(tmp.x, tmp.y, tmp.z, tmp.w);
}

static __forceinline__ __device__ void tex1Dfetch(unsigned long *retVal, cudaTextureObject_t texObject, int x)
{
   uint4 tmp;
   asm volatile ("tex.1d.v4.u32.s32 {%0, %1, %2, %3}, [%4, {%5}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(x));
   *retVal = (unsigned long)(tmp.x);
}

static __forceinline__ __device__ void tex1Dfetch(ulong1 *retVal, cudaTextureObject_t texObject, int x)
{
   uint4 tmp;
   asm volatile ("tex.1d.v4.u32.s32 {%0, %1, %2, %3}, [%4, {%5}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(x));
   *retVal = make_ulong1(tmp.x);
}

static __forceinline__ __device__ void tex1Dfetch(ulong2 *retVal, cudaTextureObject_t texObject, int x)
{
   uint4 tmp;
   asm volatile ("tex.1d.v4.u32.s32 {%0, %1, %2, %3}, [%4, {%5}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(x));
   *retVal = make_ulong2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex1Dfetch(ulong4 *retVal, cudaTextureObject_t texObject, int x)
{
   uint4 tmp;
   asm volatile ("tex.1d.v4.u32.s32 {%0, %1, %2, %3}, [%4, {%5}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(x));
   *retVal = make_ulong4(tmp.x, tmp.y, tmp.z, tmp.w);
}

#endif /* !__LP64__ */


/*******************************************************************************
*                                                                              *
*                                                                              *
*                                                                              *
*******************************************************************************/

static __forceinline__ __device__ void tex1Dfetch(float *retVal, cudaTextureObject_t texObject, int x)
{
   float4 tmp;
   asm volatile ("tex.1d.v4.f32.s32 {%0, %1, %2, %3}, [%4, {%5}];" : "=f"(tmp.x), "=f"(tmp.y), "=f"(tmp.z), "=f"(tmp.w) : "l"(texObject), "r"(x));
   *retVal = (float)(tmp.x);
}

static __forceinline__ __device__ void tex1Dfetch(float1 *retVal, cudaTextureObject_t texObject, int x)
{
   float4 tmp;
   asm volatile ("tex.1d.v4.f32.s32 {%0, %1, %2, %3}, [%4, {%5}];" : "=f"(tmp.x), "=f"(tmp.y), "=f"(tmp.z), "=f"(tmp.w) : "l"(texObject), "r"(x));
   *retVal = make_float1(tmp.x);
}

static __forceinline__ __device__ void tex1Dfetch(float2 *retVal, cudaTextureObject_t texObject, int x)
{
   float4 tmp;
   asm volatile ("tex.1d.v4.f32.s32 {%0, %1, %2, %3}, [%4, {%5}];" : "=f"(tmp.x), "=f"(tmp.y), "=f"(tmp.z), "=f"(tmp.w) : "l"(texObject), "r"(x));
   *retVal = make_float2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex1Dfetch(float4 *retVal, cudaTextureObject_t texObject, int x)
{
   float4 tmp;
   asm volatile ("tex.1d.v4.f32.s32 {%0, %1, %2, %3}, [%4, {%5}];" : "=f"(tmp.x), "=f"(tmp.y), "=f"(tmp.z), "=f"(tmp.w) : "l"(texObject), "r"(x));
   *retVal = make_float4(tmp.x, tmp.y, tmp.z, tmp.w);
}

/*******************************************************************************
*                                                                              *
*                                                                              *
*                                                                              *
*******************************************************************************/

template <class T>
static __forceinline__ __device__ T tex1Dfetch(cudaTextureObject_t texObject, int x)
{
   T ret;
   tex1Dfetch(&ret, texObject, x);
   return ret;
}

/*******************************************************************************
*                                                                              *
* 1D Texture indirect fetch functions
*                                                                              *
*******************************************************************************/

static __forceinline__ __device__ void tex1D(char *retVal, cudaTextureObject_t texObject, float x)
{
#if defined(_CHAR_UNSIGNED) || defined(__CHAR_UNSIGNED__)
    uint4 tmp;
    asm volatile ("tex.1d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x));
#else /* _CHAR_UNSIGNED || __CHAR_UNSIGNED__ */
    int4 tmp;
    asm volatile ("tex.1d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x));
#endif /* _CHAR_UNSIGNED || __CHAR_UNSIGNED__ */
    *retVal = (char)tmp.x;
}
static __forceinline__ __device__ void tex1D(signed char *retVal, cudaTextureObject_t texObject, float x)
{
   int4 tmp;
   asm volatile ("tex.1d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x));
   *retVal = (signed char)(tmp.x);
}

static __forceinline__ __device__ void tex1D(char1 *retVal, cudaTextureObject_t texObject, float x)
{
   int4 tmp;
   asm volatile ("tex.1d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x));
   *retVal = make_char1(tmp.x);
}

static __forceinline__ __device__ void tex1D(char2 *retVal, cudaTextureObject_t texObject, float x)
{
   int4 tmp;
   asm volatile ("tex.1d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x));
   *retVal = make_char2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex1D(char4 *retVal, cudaTextureObject_t texObject, float x)
{
   int4 tmp;
   asm volatile ("tex.1d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x));
   *retVal = make_char4(tmp.x, tmp.y, tmp.z, tmp.w);
}

static __forceinline__ __device__ void tex1D(unsigned char *retVal, cudaTextureObject_t texObject, float x)
{
   uint4 tmp;
   asm volatile ("tex.1d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x));
   *retVal = (unsigned char)(tmp.x);
}

static __forceinline__ __device__ void tex1D(uchar1 *retVal, cudaTextureObject_t texObject, float x)
{
   uint4 tmp;
   asm volatile ("tex.1d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x));
   *retVal = make_uchar1(tmp.x);
}

static __forceinline__ __device__ void tex1D(uchar2 *retVal, cudaTextureObject_t texObject, float x)
{
   uint4 tmp;
   asm volatile ("tex.1d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x));
   *retVal = make_uchar2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex1D(uchar4 *retVal, cudaTextureObject_t texObject, float x)
{
   uint4 tmp;
   asm volatile ("tex.1d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x));
   *retVal = make_uchar4(tmp.x, tmp.y, tmp.z, tmp.w);
}

/*******************************************************************************
*                                                                              *
*                                                                              *
*                                                                              *
*******************************************************************************/

static __forceinline__ __device__ void tex1D(short *retVal, cudaTextureObject_t texObject, float x)
{
   int4 tmp;
   asm volatile ("tex.1d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x));
   *retVal = (short)(tmp.x);
}

static __forceinline__ __device__ void tex1D(short1 *retVal, cudaTextureObject_t texObject, float x)
{
   int4 tmp;
   asm volatile ("tex.1d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x));
   *retVal = make_short1(tmp.x);
}

static __forceinline__ __device__ void tex1D(short2 *retVal, cudaTextureObject_t texObject, float x)
{
   int4 tmp;
   asm volatile ("tex.1d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x));
   *retVal = make_short2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex1D(short4 *retVal, cudaTextureObject_t texObject, float x)
{
   int4 tmp;
   asm volatile ("tex.1d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x));
   *retVal = make_short4(tmp.x, tmp.y, tmp.z, tmp.w);
}

static __forceinline__ __device__ void tex1D(unsigned short *retVal, cudaTextureObject_t texObject, float x)
{
   uint4 tmp;
   asm volatile ("tex.1d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x));
   *retVal = (unsigned short)(tmp.x);
}

static __forceinline__ __device__ void tex1D(ushort1 *retVal, cudaTextureObject_t texObject, float x)
{
   uint4 tmp;
   asm volatile ("tex.1d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x));
   *retVal = make_ushort1(tmp.x);
}

static __forceinline__ __device__ void tex1D(ushort2 *retVal, cudaTextureObject_t texObject, float x)
{
   uint4 tmp;
   asm volatile ("tex.1d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x));
   *retVal = make_ushort2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex1D(ushort4 *retVal, cudaTextureObject_t texObject, float x)
{
   uint4 tmp;
   asm volatile ("tex.1d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x));
   *retVal = make_ushort4(tmp.x, tmp.y, tmp.z, tmp.w);
}

/*******************************************************************************
*                                                                              *
*                                                                              *
*                                                                              *
*******************************************************************************/

static __forceinline__ __device__ void tex1D(int *retVal, cudaTextureObject_t texObject, float x)
{
   int4 tmp;
   asm volatile ("tex.1d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x));
   *retVal = (int)(tmp.x);
}

static __forceinline__ __device__ void tex1D(int1 *retVal, cudaTextureObject_t texObject, float x)
{
   int4 tmp;
   asm volatile ("tex.1d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x));
   *retVal = make_int1(tmp.x);
}

static __forceinline__ __device__ void tex1D(int2 *retVal, cudaTextureObject_t texObject, float x)
{
   int4 tmp;
   asm volatile ("tex.1d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x));
   *retVal = make_int2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex1D(int4 *retVal, cudaTextureObject_t texObject, float x)
{
   int4 tmp;
   asm volatile ("tex.1d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x));
   *retVal = make_int4(tmp.x, tmp.y, tmp.z, tmp.w);
}

static __forceinline__ __device__ void tex1D(unsigned int *retVal, cudaTextureObject_t texObject, float x)
{
   uint4 tmp;
   asm volatile ("tex.1d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x));
   *retVal = (unsigned int)(tmp.x);
}

static __forceinline__ __device__ void tex1D(uint1 *retVal, cudaTextureObject_t texObject, float x)
{
   uint4 tmp;
   asm volatile ("tex.1d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x));
   *retVal = make_uint1(tmp.x);
}

static __forceinline__ __device__ void tex1D(uint2 *retVal, cudaTextureObject_t texObject, float x)
{
   uint4 tmp;
   asm volatile ("tex.1d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x));
   *retVal = make_uint2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex1D(uint4 *retVal, cudaTextureObject_t texObject, float x)
{
   uint4 tmp;
   asm volatile ("tex.1d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x));
   *retVal = make_uint4(tmp.x, tmp.y, tmp.z, tmp.w);
}

/*******************************************************************************
*                                                                              *
*                                                                              *
*                                                                              *
*******************************************************************************/

#if !defined(__LP64__)

static __forceinline__ __device__ void tex1D(long *retVal, cudaTextureObject_t texObject, float x)
{
   int4 tmp;
   asm volatile ("tex.1d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x));
   *retVal = (long)(tmp.x);
}

static __forceinline__ __device__ void tex1D(long1 *retVal, cudaTextureObject_t texObject, float x)
{
   int4 tmp;
   asm volatile ("tex.1d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x));
   *retVal = make_long1(tmp.x);
}

static __forceinline__ __device__ void tex1D(long2 *retVal, cudaTextureObject_t texObject, float x)
{
   int4 tmp;
   asm volatile ("tex.1d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x));
   *retVal = make_long2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex1D(long4 *retVal, cudaTextureObject_t texObject, float x)
{
   int4 tmp;
   asm volatile ("tex.1d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x));
   *retVal = make_long4(tmp.x, tmp.y, tmp.z, tmp.w);
}

static __forceinline__ __device__ void tex1D(unsigned long *retVal, cudaTextureObject_t texObject, float x)
{
   uint4 tmp;
   asm volatile ("tex.1d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x));
   *retVal = (unsigned long)(tmp.x);
}

static __forceinline__ __device__ void tex1D(ulong1 *retVal, cudaTextureObject_t texObject, float x)
{
   uint4 tmp;
   asm volatile ("tex.1d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x));
   *retVal = make_ulong1(tmp.x);
}

static __forceinline__ __device__ void tex1D(ulong2 *retVal, cudaTextureObject_t texObject, float x)
{
   uint4 tmp;
   asm volatile ("tex.1d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x));
   *retVal = make_ulong2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex1D(ulong4 *retVal, cudaTextureObject_t texObject, float x)
{
   uint4 tmp;
   asm volatile ("tex.1d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x));
   *retVal = make_ulong4(tmp.x, tmp.y, tmp.z, tmp.w);
}

#endif /* !__LP64__ */


/*******************************************************************************
*                                                                              *
*                                                                              *
*                                                                              *
*******************************************************************************/

static __forceinline__ __device__ void tex1D(float *retVal, cudaTextureObject_t texObject, float x)
{
   float4 tmp;
   asm volatile ("tex.1d.v4.f32.f32 {%0, %1, %2, %3}, [%4, {%5}];" : "=f"(tmp.x), "=f"(tmp.y), "=f"(tmp.z), "=f"(tmp.w) : "l"(texObject), "f"(x));
   *retVal = (float)(tmp.x);
}

static __forceinline__ __device__ void tex1D(float1 *retVal, cudaTextureObject_t texObject, float x)
{
   float4 tmp;
   asm volatile ("tex.1d.v4.f32.f32 {%0, %1, %2, %3}, [%4, {%5}];" : "=f"(tmp.x), "=f"(tmp.y), "=f"(tmp.z), "=f"(tmp.w) : "l"(texObject), "f"(x));
   *retVal = make_float1(tmp.x);
}

static __forceinline__ __device__ void tex1D(float2 *retVal, cudaTextureObject_t texObject, float x)
{
   float4 tmp;
   asm volatile ("tex.1d.v4.f32.f32 {%0, %1, %2, %3}, [%4, {%5}];" : "=f"(tmp.x), "=f"(tmp.y), "=f"(tmp.z), "=f"(tmp.w) : "l"(texObject), "f"(x));
   *retVal = make_float2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex1D(float4 *retVal, cudaTextureObject_t texObject, float x)
{
   float4 tmp;
   asm volatile ("tex.1d.v4.f32.f32 {%0, %1, %2, %3}, [%4, {%5}];" : "=f"(tmp.x), "=f"(tmp.y), "=f"(tmp.z), "=f"(tmp.w) : "l"(texObject), "f"(x));
   *retVal = make_float4(tmp.x, tmp.y, tmp.z, tmp.w);
}

/*******************************************************************************
*                                                                              *
*                                                                              *
*                                                                              *
*******************************************************************************/

template <class T>
static __forceinline__ __device__ T tex1D(cudaTextureObject_t texObject, float x)
{
   T ret;
   tex1D(&ret, texObject, x);
   return ret;
}

/*******************************************************************************
*                                                                              *
* 2D Texture indirect fetch functions
*                                                                              *
*******************************************************************************/

static __forceinline__ __device__ void tex2D(char *retVal, cudaTextureObject_t texObject, float x, float y)
{
#if defined(_CHAR_UNSIGNED) || defined(__CHAR_UNSIGNED__)
    uint4 tmp;
    asm volatile ("tex.2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
#else /* _CHAR_UNSIGNED || __CHAR_UNSIGNED__ */
    int4 tmp;
    asm volatile ("tex.2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
#endif /* _CHAR_UNSIGNED || __CHAR_UNSIGNED__ */
    *retVal = (char)tmp.x;
}
static __forceinline__ __device__ void tex2D(signed char *retVal, cudaTextureObject_t texObject, float x, float y)
{
   int4 tmp;
   asm volatile ("tex.2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   *retVal = (signed char)(tmp.x);
}

static __forceinline__ __device__ void tex2D(char1 *retVal, cudaTextureObject_t texObject, float x, float y)
{
   int4 tmp;
   asm volatile ("tex.2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   *retVal = make_char1(tmp.x);
}

static __forceinline__ __device__ void tex2D(char2 *retVal, cudaTextureObject_t texObject, float x, float y)
{
   int4 tmp;
   asm volatile ("tex.2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   *retVal = make_char2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex2D(char4 *retVal, cudaTextureObject_t texObject, float x, float y)
{
   int4 tmp;
   asm volatile ("tex.2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   *retVal = make_char4(tmp.x, tmp.y, tmp.z, tmp.w);
}

static __forceinline__ __device__ void tex2D(unsigned char *retVal, cudaTextureObject_t texObject, float x, float y)
{
   uint4 tmp;
   asm volatile ("tex.2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   *retVal = (unsigned char)(tmp.x);
}

static __forceinline__ __device__ void tex2D(uchar1 *retVal, cudaTextureObject_t texObject, float x, float y)
{
   uint4 tmp;
   asm volatile ("tex.2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   *retVal = make_uchar1(tmp.x);
}

static __forceinline__ __device__ void tex2D(uchar2 *retVal, cudaTextureObject_t texObject, float x, float y)
{
   uint4 tmp;
   asm volatile ("tex.2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   *retVal = make_uchar2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex2D(uchar4 *retVal, cudaTextureObject_t texObject, float x, float y)
{
   uint4 tmp;
   asm volatile ("tex.2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   *retVal = make_uchar4(tmp.x, tmp.y, tmp.z, tmp.w);
}

/*******************************************************************************
*                                                                              *
*                                                                              *
*                                                                              *
*******************************************************************************/

static __forceinline__ __device__ void tex2D(short *retVal, cudaTextureObject_t texObject, float x, float y)
{
   int4 tmp;
   asm volatile ("tex.2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   *retVal = (short)(tmp.x);
}

static __forceinline__ __device__ void tex2D(short1 *retVal, cudaTextureObject_t texObject, float x, float y)
{
   int4 tmp;
   asm volatile ("tex.2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   *retVal = make_short1(tmp.x);
}

static __forceinline__ __device__ void tex2D(short2 *retVal, cudaTextureObject_t texObject, float x, float y)
{
   int4 tmp;
   asm volatile ("tex.2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   *retVal = make_short2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex2D(short4 *retVal, cudaTextureObject_t texObject, float x, float y)
{
   int4 tmp;
   asm volatile ("tex.2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   *retVal = make_short4(tmp.x, tmp.y, tmp.z, tmp.w);
}

static __forceinline__ __device__ void tex2D(unsigned short *retVal, cudaTextureObject_t texObject, float x, float y)
{
   uint4 tmp;
   asm volatile ("tex.2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   *retVal = (unsigned short)(tmp.x);
}

static __forceinline__ __device__ void tex2D(ushort1 *retVal, cudaTextureObject_t texObject, float x, float y)
{
   uint4 tmp;
   asm volatile ("tex.2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   *retVal = make_ushort1(tmp.x);
}

static __forceinline__ __device__ void tex2D(ushort2 *retVal, cudaTextureObject_t texObject, float x, float y)
{
   uint4 tmp;
   asm volatile ("tex.2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   *retVal = make_ushort2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex2D(ushort4 *retVal, cudaTextureObject_t texObject, float x, float y)
{
   uint4 tmp;
   asm volatile ("tex.2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   *retVal = make_ushort4(tmp.x, tmp.y, tmp.z, tmp.w);
}

/*******************************************************************************
*                                                                              *
*                                                                              *
*                                                                              *
*******************************************************************************/

static __forceinline__ __device__ void tex2D(int *retVal, cudaTextureObject_t texObject, float x, float y)
{
   int4 tmp;
   asm volatile ("tex.2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   *retVal = (int)(tmp.x);
}

static __forceinline__ __device__ void tex2D(int1 *retVal, cudaTextureObject_t texObject, float x, float y)
{
   int4 tmp;
   asm volatile ("tex.2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   *retVal = make_int1(tmp.x);
}

static __forceinline__ __device__ void tex2D(int2 *retVal, cudaTextureObject_t texObject, float x, float y)
{
   int4 tmp;
   asm volatile ("tex.2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   *retVal = make_int2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex2D(int4 *retVal, cudaTextureObject_t texObject, float x, float y)
{
   int4 tmp;
   asm volatile ("tex.2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   *retVal = make_int4(tmp.x, tmp.y, tmp.z, tmp.w);
}

static __forceinline__ __device__ void tex2D(unsigned int *retVal, cudaTextureObject_t texObject, float x, float y)
{
   uint4 tmp;
   asm volatile ("tex.2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   *retVal = (unsigned int)(tmp.x);
}

static __forceinline__ __device__ void tex2D(uint1 *retVal, cudaTextureObject_t texObject, float x, float y)
{
   uint4 tmp;
   asm volatile ("tex.2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   *retVal = make_uint1(tmp.x);
}

static __forceinline__ __device__ void tex2D(uint2 *retVal, cudaTextureObject_t texObject, float x, float y)
{
   uint4 tmp;
   asm volatile ("tex.2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   *retVal = make_uint2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex2D(uint4 *retVal, cudaTextureObject_t texObject, float x, float y)
{
   uint4 tmp;
   asm volatile ("tex.2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   *retVal = make_uint4(tmp.x, tmp.y, tmp.z, tmp.w);
}

/*******************************************************************************
*                                                                              *
*                                                                              *
*                                                                              *
*******************************************************************************/

#if !defined(__LP64__)

static __forceinline__ __device__ void tex2D(long *retVal, cudaTextureObject_t texObject, float x, float y)
{
   int4 tmp;
   asm volatile ("tex.2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   *retVal = (long)(tmp.x);
}

static __forceinline__ __device__ void tex2D(long1 *retVal, cudaTextureObject_t texObject, float x, float y)
{
   int4 tmp;
   asm volatile ("tex.2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   *retVal = make_long1(tmp.x);
}

static __forceinline__ __device__ void tex2D(long2 *retVal, cudaTextureObject_t texObject, float x, float y)
{
   int4 tmp;
   asm volatile ("tex.2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   *retVal = make_long2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex2D(long4 *retVal, cudaTextureObject_t texObject, float x, float y)
{
   int4 tmp;
   asm volatile ("tex.2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   *retVal = make_long4(tmp.x, tmp.y, tmp.z, tmp.w);
}

static __forceinline__ __device__ void tex2D(unsigned long *retVal, cudaTextureObject_t texObject, float x, float y)
{
   uint4 tmp;
   asm volatile ("tex.2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   *retVal = (unsigned long)(tmp.x);
}

static __forceinline__ __device__ void tex2D(ulong1 *retVal, cudaTextureObject_t texObject, float x, float y)
{
   uint4 tmp;
   asm volatile ("tex.2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   *retVal = make_ulong1(tmp.x);
}

static __forceinline__ __device__ void tex2D(ulong2 *retVal, cudaTextureObject_t texObject, float x, float y)
{
   uint4 tmp;
   asm volatile ("tex.2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   *retVal = make_ulong2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex2D(ulong4 *retVal, cudaTextureObject_t texObject, float x, float y)
{
   uint4 tmp;
   asm volatile ("tex.2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   *retVal = make_ulong4(tmp.x, tmp.y, tmp.z, tmp.w);
}

#endif /* !__LP64__ */


/*******************************************************************************
*                                                                              *
*                                                                              *
*                                                                              *
*******************************************************************************/

static __forceinline__ __device__ void tex2D(float *retVal, cudaTextureObject_t texObject, float x, float y)
{
   float4 tmp;
   asm volatile ("tex.2d.v4.f32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=f"(tmp.x), "=f"(tmp.y), "=f"(tmp.z), "=f"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   *retVal = (float)(tmp.x);
}

static __forceinline__ __device__ void tex2D(float1 *retVal, cudaTextureObject_t texObject, float x, float y)
{
   float4 tmp;
   asm volatile ("tex.2d.v4.f32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=f"(tmp.x), "=f"(tmp.y), "=f"(tmp.z), "=f"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   *retVal = make_float1(tmp.x);
}

static __forceinline__ __device__ void tex2D(float2 *retVal, cudaTextureObject_t texObject, float x, float y)
{
   float4 tmp;
   asm volatile ("tex.2d.v4.f32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=f"(tmp.x), "=f"(tmp.y), "=f"(tmp.z), "=f"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   *retVal = make_float2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex2D(float4 *retVal, cudaTextureObject_t texObject, float x, float y)
{
   float4 tmp;
   asm volatile ("tex.2d.v4.f32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=f"(tmp.x), "=f"(tmp.y), "=f"(tmp.z), "=f"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   *retVal = make_float4(tmp.x, tmp.y, tmp.z, tmp.w);
}

/*******************************************************************************
*                                                                              *
*                                                                              *
*                                                                              *
*******************************************************************************/

template <class T>
static __forceinline__ __device__ T tex2D(cudaTextureObject_t texObject, float x, float y)
{
   T ret;
   tex2D(&ret, texObject, x, y);
   return ret;
}

/*******************************************************************************
*                                                                              *
* 3D Texture indirect fetch functions
*                                                                              *
*******************************************************************************/

static __forceinline__ __device__ void tex3D(char *retVal, cudaTextureObject_t texObject, float x, float y, float z)
{
#if defined(_CHAR_UNSIGNED) || defined(__CHAR_UNSIGNED__)
    uint4 tmp;
    asm volatile ("tex.3d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z));
#else /* _CHAR_UNSIGNED || __CHAR_UNSIGNED__ */
    int4 tmp;
    asm volatile ("tex.3d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z));
#endif /* _CHAR_UNSIGNED || __CHAR_UNSIGNED__ */
    *retVal = (char)tmp.x;
}
static __forceinline__ __device__ void tex3D(signed char *retVal, cudaTextureObject_t texObject, float x, float y, float z)
{
   int4 tmp;
   asm volatile ("tex.3d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z));
   *retVal = (signed char)(tmp.x);
}

static __forceinline__ __device__ void tex3D(char1 *retVal, cudaTextureObject_t texObject, float x, float y, float z)
{
   int4 tmp;
   asm volatile ("tex.3d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z));
   *retVal = make_char1(tmp.x);
}

static __forceinline__ __device__ void tex3D(char2 *retVal, cudaTextureObject_t texObject, float x, float y, float z)
{
   int4 tmp;
   asm volatile ("tex.3d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z));
   *retVal = make_char2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex3D(char4 *retVal, cudaTextureObject_t texObject, float x, float y, float z)
{
   int4 tmp;
   asm volatile ("tex.3d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z));
   *retVal = make_char4(tmp.x, tmp.y, tmp.z, tmp.w);
}

static __forceinline__ __device__ void tex3D(unsigned char *retVal, cudaTextureObject_t texObject, float x, float y, float z)
{
   uint4 tmp;
   asm volatile ("tex.3d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z));
   *retVal = (unsigned char)(tmp.x);
}

static __forceinline__ __device__ void tex3D(uchar1 *retVal, cudaTextureObject_t texObject, float x, float y, float z)
{
   uint4 tmp;
   asm volatile ("tex.3d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z));
   *retVal = make_uchar1(tmp.x);
}

static __forceinline__ __device__ void tex3D(uchar2 *retVal, cudaTextureObject_t texObject, float x, float y, float z)
{
   uint4 tmp;
   asm volatile ("tex.3d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z));
   *retVal = make_uchar2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex3D(uchar4 *retVal, cudaTextureObject_t texObject, float x, float y, float z)
{
   uint4 tmp;
   asm volatile ("tex.3d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z));
   *retVal = make_uchar4(tmp.x, tmp.y, tmp.z, tmp.w);
}

/*******************************************************************************
*                                                                              *
*                                                                              *
*                                                                              *
*******************************************************************************/

static __forceinline__ __device__ void tex3D(short *retVal, cudaTextureObject_t texObject, float x, float y, float z)
{
   int4 tmp;
   asm volatile ("tex.3d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z));
   *retVal = (short)(tmp.x);
}

static __forceinline__ __device__ void tex3D(short1 *retVal, cudaTextureObject_t texObject, float x, float y, float z)
{
   int4 tmp;
   asm volatile ("tex.3d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z));
   *retVal = make_short1(tmp.x);
}

static __forceinline__ __device__ void tex3D(short2 *retVal, cudaTextureObject_t texObject, float x, float y, float z)
{
   int4 tmp;
   asm volatile ("tex.3d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z));
   *retVal = make_short2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex3D(short4 *retVal, cudaTextureObject_t texObject, float x, float y, float z)
{
   int4 tmp;
   asm volatile ("tex.3d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z));
   *retVal = make_short4(tmp.x, tmp.y, tmp.z, tmp.w);
}

static __forceinline__ __device__ void tex3D(unsigned short *retVal, cudaTextureObject_t texObject, float x, float y, float z)
{
   uint4 tmp;
   asm volatile ("tex.3d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z));
   *retVal = (unsigned short)(tmp.x);
}

static __forceinline__ __device__ void tex3D(ushort1 *retVal, cudaTextureObject_t texObject, float x, float y, float z)
{
   uint4 tmp;
   asm volatile ("tex.3d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z));
   *retVal = make_ushort1(tmp.x);
}

static __forceinline__ __device__ void tex3D(ushort2 *retVal, cudaTextureObject_t texObject, float x, float y, float z)
{
   uint4 tmp;
   asm volatile ("tex.3d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z));
   *retVal = make_ushort2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex3D(ushort4 *retVal, cudaTextureObject_t texObject, float x, float y, float z)
{
   uint4 tmp;
   asm volatile ("tex.3d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z));
   *retVal = make_ushort4(tmp.x, tmp.y, tmp.z, tmp.w);
}

/*******************************************************************************
*                                                                              *
*                                                                              *
*                                                                              *
*******************************************************************************/

static __forceinline__ __device__ void tex3D(int *retVal, cudaTextureObject_t texObject, float x, float y, float z)
{
   int4 tmp;
   asm volatile ("tex.3d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z));
   *retVal = (int)(tmp.x);
}

static __forceinline__ __device__ void tex3D(int1 *retVal, cudaTextureObject_t texObject, float x, float y, float z)
{
   int4 tmp;
   asm volatile ("tex.3d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z));
   *retVal = make_int1(tmp.x);
}

static __forceinline__ __device__ void tex3D(int2 *retVal, cudaTextureObject_t texObject, float x, float y, float z)
{
   int4 tmp;
   asm volatile ("tex.3d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z));
   *retVal = make_int2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex3D(int4 *retVal, cudaTextureObject_t texObject, float x, float y, float z)
{
   int4 tmp;
   asm volatile ("tex.3d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z));
   *retVal = make_int4(tmp.x, tmp.y, tmp.z, tmp.w);
}

static __forceinline__ __device__ void tex3D(unsigned int *retVal, cudaTextureObject_t texObject, float x, float y, float z)
{
   uint4 tmp;
   asm volatile ("tex.3d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z));
   *retVal = (unsigned int)(tmp.x);
}

static __forceinline__ __device__ void tex3D(uint1 *retVal, cudaTextureObject_t texObject, float x, float y, float z)
{
   uint4 tmp;
   asm volatile ("tex.3d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z));
   *retVal = make_uint1(tmp.x);
}

static __forceinline__ __device__ void tex3D(uint2 *retVal, cudaTextureObject_t texObject, float x, float y, float z)
{
   uint4 tmp;
   asm volatile ("tex.3d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z));
   *retVal = make_uint2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex3D(uint4 *retVal, cudaTextureObject_t texObject, float x, float y, float z)
{
   uint4 tmp;
   asm volatile ("tex.3d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z));
   *retVal = make_uint4(tmp.x, tmp.y, tmp.z, tmp.w);
}

/*******************************************************************************
*                                                                              *
*                                                                              *
*                                                                              *
*******************************************************************************/

#if !defined(__LP64__)

static __forceinline__ __device__ void tex3D(long *retVal, cudaTextureObject_t texObject, float x, float y, float z)
{
   int4 tmp;
   asm volatile ("tex.3d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z));
   *retVal = (long)(tmp.x);
}

static __forceinline__ __device__ void tex3D(long1 *retVal, cudaTextureObject_t texObject, float x, float y, float z)
{
   int4 tmp;
   asm volatile ("tex.3d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z));
   *retVal = make_long1(tmp.x);
}

static __forceinline__ __device__ void tex3D(long2 *retVal, cudaTextureObject_t texObject, float x, float y, float z)
{
   int4 tmp;
   asm volatile ("tex.3d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z));
   *retVal = make_long2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex3D(long4 *retVal, cudaTextureObject_t texObject, float x, float y, float z)
{
   int4 tmp;
   asm volatile ("tex.3d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z));
   *retVal = make_long4(tmp.x, tmp.y, tmp.z, tmp.w);
}

static __forceinline__ __device__ void tex3D(unsigned long *retVal, cudaTextureObject_t texObject, float x, float y, float z)
{
   uint4 tmp;
   asm volatile ("tex.3d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z));
   *retVal = (unsigned long)(tmp.x);
}

static __forceinline__ __device__ void tex3D(ulong1 *retVal, cudaTextureObject_t texObject, float x, float y, float z)
{
   uint4 tmp;
   asm volatile ("tex.3d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z));
   *retVal = make_ulong1(tmp.x);
}

static __forceinline__ __device__ void tex3D(ulong2 *retVal, cudaTextureObject_t texObject, float x, float y, float z)
{
   uint4 tmp;
   asm volatile ("tex.3d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z));
   *retVal = make_ulong2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex3D(ulong4 *retVal, cudaTextureObject_t texObject, float x, float y, float z)
{
   uint4 tmp;
   asm volatile ("tex.3d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z));
   *retVal = make_ulong4(tmp.x, tmp.y, tmp.z, tmp.w);
}

#endif /* !__LP64__ */


/*******************************************************************************
*                                                                              *
*                                                                              *
*                                                                              *
*******************************************************************************/

static __forceinline__ __device__ void tex3D(float *retVal, cudaTextureObject_t texObject, float x, float y, float z)
{
   float4 tmp;
   asm volatile ("tex.3d.v4.f32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}];" : "=f"(tmp.x), "=f"(tmp.y), "=f"(tmp.z), "=f"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z));
   *retVal = (float)(tmp.x);
}

static __forceinline__ __device__ void tex3D(float1 *retVal, cudaTextureObject_t texObject, float x, float y, float z)
{
   float4 tmp;
   asm volatile ("tex.3d.v4.f32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}];" : "=f"(tmp.x), "=f"(tmp.y), "=f"(tmp.z), "=f"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z));
   *retVal = make_float1(tmp.x);
}

static __forceinline__ __device__ void tex3D(float2 *retVal, cudaTextureObject_t texObject, float x, float y, float z)
{
   float4 tmp;
   asm volatile ("tex.3d.v4.f32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}];" : "=f"(tmp.x), "=f"(tmp.y), "=f"(tmp.z), "=f"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z));
   *retVal = make_float2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex3D(float4 *retVal, cudaTextureObject_t texObject, float x, float y, float z)
{
   float4 tmp;
   asm volatile ("tex.3d.v4.f32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}];" : "=f"(tmp.x), "=f"(tmp.y), "=f"(tmp.z), "=f"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z));
   *retVal = make_float4(tmp.x, tmp.y, tmp.z, tmp.w);
}

/*******************************************************************************
*                                                                              *
*                                                                              *
*                                                                              *
*******************************************************************************/

template <class T>
static __forceinline__ __device__ T tex3D(cudaTextureObject_t texObject, float x, float y, float z)
{
   T ret;
   tex3D(&ret, texObject, x, y, z);
   return ret;
}

/*******************************************************************************
*                                                                              *
* 1D Layered Texture indirect fetch functions
*                                                                              *
*******************************************************************************/

static __forceinline__ __device__ void tex1DLayered(char *retVal, cudaTextureObject_t texObject, float x, int layer)
{
#if defined(_CHAR_UNSIGNED) || defined(__CHAR_UNSIGNED__)
    uint4 tmp;
    asm volatile ("tex.a1d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x));
#else /* _CHAR_UNSIGNED || __CHAR_UNSIGNED__ */
    int4 tmp;
    asm volatile ("tex.a1d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x));
#endif /* _CHAR_UNSIGNED || __CHAR_UNSIGNED__ */
    *retVal = (char)tmp.x;
}
static __forceinline__ __device__ void tex1DLayered(signed char *retVal, cudaTextureObject_t texObject, float x, int layer)
{
   int4 tmp;
   asm volatile ("tex.a1d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x));
   *retVal = (signed char)(tmp.x);
}

static __forceinline__ __device__ void tex1DLayered(char1 *retVal, cudaTextureObject_t texObject, float x, int layer)
{
   int4 tmp;
   asm volatile ("tex.a1d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x));
   *retVal = make_char1(tmp.x);
}

static __forceinline__ __device__ void tex1DLayered(char2 *retVal, cudaTextureObject_t texObject, float x, int layer)
{
   int4 tmp;
   asm volatile ("tex.a1d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x));
   *retVal = make_char2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex1DLayered(char4 *retVal, cudaTextureObject_t texObject, float x, int layer)
{
   int4 tmp;
   asm volatile ("tex.a1d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x));
   *retVal = make_char4(tmp.x, tmp.y, tmp.z, tmp.w);
}

static __forceinline__ __device__ void tex1DLayered(unsigned char *retVal, cudaTextureObject_t texObject, float x, int layer)
{
   uint4 tmp;
   asm volatile ("tex.a1d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x));
   *retVal = (unsigned char)(tmp.x);
}

static __forceinline__ __device__ void tex1DLayered(uchar1 *retVal, cudaTextureObject_t texObject, float x, int layer)
{
   uint4 tmp;
   asm volatile ("tex.a1d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x));
   *retVal = make_uchar1(tmp.x);
}

static __forceinline__ __device__ void tex1DLayered(uchar2 *retVal, cudaTextureObject_t texObject, float x, int layer)
{
   uint4 tmp;
   asm volatile ("tex.a1d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x));
   *retVal = make_uchar2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex1DLayered(uchar4 *retVal, cudaTextureObject_t texObject, float x, int layer)
{
   uint4 tmp;
   asm volatile ("tex.a1d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x));
   *retVal = make_uchar4(tmp.x, tmp.y, tmp.z, tmp.w);
}

/*******************************************************************************
*                                                                              *
*                                                                              *
*                                                                              *
*******************************************************************************/

static __forceinline__ __device__ void tex1DLayered(short *retVal, cudaTextureObject_t texObject, float x, int layer)
{
   int4 tmp;
   asm volatile ("tex.a1d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x));
   *retVal = (short)(tmp.x);
}

static __forceinline__ __device__ void tex1DLayered(short1 *retVal, cudaTextureObject_t texObject, float x, int layer)
{
   int4 tmp;
   asm volatile ("tex.a1d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x));
   *retVal = make_short1(tmp.x);
}

static __forceinline__ __device__ void tex1DLayered(short2 *retVal, cudaTextureObject_t texObject, float x, int layer)
{
   int4 tmp;
   asm volatile ("tex.a1d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x));
   *retVal = make_short2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex1DLayered(short4 *retVal, cudaTextureObject_t texObject, float x, int layer)
{
   int4 tmp;
   asm volatile ("tex.a1d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x));
   *retVal = make_short4(tmp.x, tmp.y, tmp.z, tmp.w);
}

static __forceinline__ __device__ void tex1DLayered(unsigned short *retVal, cudaTextureObject_t texObject, float x, int layer)
{
   uint4 tmp;
   asm volatile ("tex.a1d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x));
   *retVal = (unsigned short)(tmp.x);
}

static __forceinline__ __device__ void tex1DLayered(ushort1 *retVal, cudaTextureObject_t texObject, float x, int layer)
{
   uint4 tmp;
   asm volatile ("tex.a1d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x));
   *retVal = make_ushort1(tmp.x);
}

static __forceinline__ __device__ void tex1DLayered(ushort2 *retVal, cudaTextureObject_t texObject, float x, int layer)
{
   uint4 tmp;
   asm volatile ("tex.a1d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x));
   *retVal = make_ushort2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex1DLayered(ushort4 *retVal, cudaTextureObject_t texObject, float x, int layer)
{
   uint4 tmp;
   asm volatile ("tex.a1d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x));
   *retVal = make_ushort4(tmp.x, tmp.y, tmp.z, tmp.w);
}

/*******************************************************************************
*                                                                              *
*                                                                              *
*                                                                              *
*******************************************************************************/

static __forceinline__ __device__ void tex1DLayered(int *retVal, cudaTextureObject_t texObject, float x, int layer)
{
   int4 tmp;
   asm volatile ("tex.a1d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x));
   *retVal = (int)(tmp.x);
}

static __forceinline__ __device__ void tex1DLayered(int1 *retVal, cudaTextureObject_t texObject, float x, int layer)
{
   int4 tmp;
   asm volatile ("tex.a1d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x));
   *retVal = make_int1(tmp.x);
}

static __forceinline__ __device__ void tex1DLayered(int2 *retVal, cudaTextureObject_t texObject, float x, int layer)
{
   int4 tmp;
   asm volatile ("tex.a1d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x));
   *retVal = make_int2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex1DLayered(int4 *retVal, cudaTextureObject_t texObject, float x, int layer)
{
   int4 tmp;
   asm volatile ("tex.a1d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x));
   *retVal = make_int4(tmp.x, tmp.y, tmp.z, tmp.w);
}

static __forceinline__ __device__ void tex1DLayered(unsigned int *retVal, cudaTextureObject_t texObject, float x, int layer)
{
   uint4 tmp;
   asm volatile ("tex.a1d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x));
   *retVal = (unsigned int)(tmp.x);
}

static __forceinline__ __device__ void tex1DLayered(uint1 *retVal, cudaTextureObject_t texObject, float x, int layer)
{
   uint4 tmp;
   asm volatile ("tex.a1d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x));
   *retVal = make_uint1(tmp.x);
}

static __forceinline__ __device__ void tex1DLayered(uint2 *retVal, cudaTextureObject_t texObject, float x, int layer)
{
   uint4 tmp;
   asm volatile ("tex.a1d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x));
   *retVal = make_uint2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex1DLayered(uint4 *retVal, cudaTextureObject_t texObject, float x, int layer)
{
   uint4 tmp;
   asm volatile ("tex.a1d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x));
   *retVal = make_uint4(tmp.x, tmp.y, tmp.z, tmp.w);
}

/*******************************************************************************
*                                                                              *
*                                                                              *
*                                                                              *
*******************************************************************************/

#if !defined(__LP64__)

static __forceinline__ __device__ void tex1DLayered(long *retVal, cudaTextureObject_t texObject, float x, int layer)
{
   int4 tmp;
   asm volatile ("tex.a1d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x));
   *retVal = (long)(tmp.x);
}

static __forceinline__ __device__ void tex1DLayered(long1 *retVal, cudaTextureObject_t texObject, float x, int layer)
{
   int4 tmp;
   asm volatile ("tex.a1d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x));
   *retVal = make_long1(tmp.x);
}

static __forceinline__ __device__ void tex1DLayered(long2 *retVal, cudaTextureObject_t texObject, float x, int layer)
{
   int4 tmp;
   asm volatile ("tex.a1d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x));
   *retVal = make_long2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex1DLayered(long4 *retVal, cudaTextureObject_t texObject, float x, int layer)
{
   int4 tmp;
   asm volatile ("tex.a1d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x));
   *retVal = make_long4(tmp.x, tmp.y, tmp.z, tmp.w);
}

static __forceinline__ __device__ void tex1DLayered(unsigned long *retVal, cudaTextureObject_t texObject, float x, int layer)
{
   uint4 tmp;
   asm volatile ("tex.a1d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x));
   *retVal = (unsigned long)(tmp.x);
}

static __forceinline__ __device__ void tex1DLayered(ulong1 *retVal, cudaTextureObject_t texObject, float x, int layer)
{
   uint4 tmp;
   asm volatile ("tex.a1d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x));
   *retVal = make_ulong1(tmp.x);
}

static __forceinline__ __device__ void tex1DLayered(ulong2 *retVal, cudaTextureObject_t texObject, float x, int layer)
{
   uint4 tmp;
   asm volatile ("tex.a1d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x));
   *retVal = make_ulong2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex1DLayered(ulong4 *retVal, cudaTextureObject_t texObject, float x, int layer)
{
   uint4 tmp;
   asm volatile ("tex.a1d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x));
   *retVal = make_ulong4(tmp.x, tmp.y, tmp.z, tmp.w);
}

#endif /* !__LP64__ */


/*******************************************************************************
*                                                                              *
*                                                                              *
*                                                                              *
*******************************************************************************/

static __forceinline__ __device__ void tex1DLayered(float *retVal, cudaTextureObject_t texObject, float x, int layer)
{
   float4 tmp;
   asm volatile ("tex.a1d.v4.f32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=f"(tmp.x), "=f"(tmp.y), "=f"(tmp.z), "=f"(tmp.w) : "l"(texObject), "r"(layer), "f"(x));
   *retVal = (float)(tmp.x);
}

static __forceinline__ __device__ void tex1DLayered(float1 *retVal, cudaTextureObject_t texObject, float x, int layer)
{
   float4 tmp;
   asm volatile ("tex.a1d.v4.f32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=f"(tmp.x), "=f"(tmp.y), "=f"(tmp.z), "=f"(tmp.w) : "l"(texObject), "r"(layer), "f"(x));
   *retVal = make_float1(tmp.x);
}

static __forceinline__ __device__ void tex1DLayered(float2 *retVal, cudaTextureObject_t texObject, float x, int layer)
{
   float4 tmp;
   asm volatile ("tex.a1d.v4.f32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=f"(tmp.x), "=f"(tmp.y), "=f"(tmp.z), "=f"(tmp.w) : "l"(texObject), "r"(layer), "f"(x));
   *retVal = make_float2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex1DLayered(float4 *retVal, cudaTextureObject_t texObject, float x, int layer)
{
   float4 tmp;
   asm volatile ("tex.a1d.v4.f32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=f"(tmp.x), "=f"(tmp.y), "=f"(tmp.z), "=f"(tmp.w) : "l"(texObject), "r"(layer), "f"(x));
   *retVal = make_float4(tmp.x, tmp.y, tmp.z, tmp.w);
}

/*******************************************************************************
*                                                                              *
*                                                                              *
*                                                                              *
*******************************************************************************/

template <class T>
static __forceinline__ __device__ T tex1DLayered(cudaTextureObject_t texObject, float x, int layer)
{
   T ret;
   tex1DLayered(&ret, texObject, x, layer);
   return ret;
}

/*******************************************************************************
*                                                                              *
* 2D Layered Texture indirect fetch functions
*                                                                              *
*******************************************************************************/

static __forceinline__ __device__ void tex2DLayered(char *retVal, cudaTextureObject_t texObject, float x, float y, int layer)
{
#if defined(_CHAR_UNSIGNED) || defined(__CHAR_UNSIGNED__)
    uint4 tmp;
    asm volatile ("tex.a2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y));
#else /* _CHAR_UNSIGNED || __CHAR_UNSIGNED__ */
    int4 tmp;
    asm volatile ("tex.a2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y));
#endif /* _CHAR_UNSIGNED || __CHAR_UNSIGNED__ */
    *retVal = (char)tmp.x;
}
static __forceinline__ __device__ void tex2DLayered(signed char *retVal, cudaTextureObject_t texObject, float x, float y, int layer)
{
   int4 tmp;
   asm volatile ("tex.a2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y));
   *retVal = (signed char)(tmp.x);
}

static __forceinline__ __device__ void tex2DLayered(char1 *retVal, cudaTextureObject_t texObject, float x, float y, int layer)
{
   int4 tmp;
   asm volatile ("tex.a2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y));
   *retVal = make_char1(tmp.x);
}

static __forceinline__ __device__ void tex2DLayered(char2 *retVal, cudaTextureObject_t texObject, float x, float y, int layer)
{
   int4 tmp;
   asm volatile ("tex.a2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y));
   *retVal = make_char2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex2DLayered(char4 *retVal, cudaTextureObject_t texObject, float x, float y, int layer)
{
   int4 tmp;
   asm volatile ("tex.a2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y));
   *retVal = make_char4(tmp.x, tmp.y, tmp.z, tmp.w);
}

static __forceinline__ __device__ void tex2DLayered(unsigned char *retVal, cudaTextureObject_t texObject, float x, float y, int layer)
{
   uint4 tmp;
   asm volatile ("tex.a2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y));
   *retVal = (unsigned char)(tmp.x);
}

static __forceinline__ __device__ void tex2DLayered(uchar1 *retVal, cudaTextureObject_t texObject, float x, float y, int layer)
{
   uint4 tmp;
   asm volatile ("tex.a2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y));
   *retVal = make_uchar1(tmp.x);
}

static __forceinline__ __device__ void tex2DLayered(uchar2 *retVal, cudaTextureObject_t texObject, float x, float y, int layer)
{
   uint4 tmp;
   asm volatile ("tex.a2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y));
   *retVal = make_uchar2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex2DLayered(uchar4 *retVal, cudaTextureObject_t texObject, float x, float y, int layer)
{
   uint4 tmp;
   asm volatile ("tex.a2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y));
   *retVal = make_uchar4(tmp.x, tmp.y, tmp.z, tmp.w);
}

/*******************************************************************************
*                                                                              *
*                                                                              *
*                                                                              *
*******************************************************************************/

static __forceinline__ __device__ void tex2DLayered(short *retVal, cudaTextureObject_t texObject, float x, float y, int layer)
{
   int4 tmp;
   asm volatile ("tex.a2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y));
   *retVal = (short)(tmp.x);
}

static __forceinline__ __device__ void tex2DLayered(short1 *retVal, cudaTextureObject_t texObject, float x, float y, int layer)
{
   int4 tmp;
   asm volatile ("tex.a2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y));
   *retVal = make_short1(tmp.x);
}

static __forceinline__ __device__ void tex2DLayered(short2 *retVal, cudaTextureObject_t texObject, float x, float y, int layer)
{
   int4 tmp;
   asm volatile ("tex.a2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y));
   *retVal = make_short2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex2DLayered(short4 *retVal, cudaTextureObject_t texObject, float x, float y, int layer)
{
   int4 tmp;
   asm volatile ("tex.a2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y));
   *retVal = make_short4(tmp.x, tmp.y, tmp.z, tmp.w);
}

static __forceinline__ __device__ void tex2DLayered(unsigned short *retVal, cudaTextureObject_t texObject, float x, float y, int layer)
{
   uint4 tmp;
   asm volatile ("tex.a2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y));
   *retVal = (unsigned short)(tmp.x);
}

static __forceinline__ __device__ void tex2DLayered(ushort1 *retVal, cudaTextureObject_t texObject, float x, float y, int layer)
{
   uint4 tmp;
   asm volatile ("tex.a2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y));
   *retVal = make_ushort1(tmp.x);
}

static __forceinline__ __device__ void tex2DLayered(ushort2 *retVal, cudaTextureObject_t texObject, float x, float y, int layer)
{
   uint4 tmp;
   asm volatile ("tex.a2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y));
   *retVal = make_ushort2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex2DLayered(ushort4 *retVal, cudaTextureObject_t texObject, float x, float y, int layer)
{
   uint4 tmp;
   asm volatile ("tex.a2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y));
   *retVal = make_ushort4(tmp.x, tmp.y, tmp.z, tmp.w);
}

/*******************************************************************************
*                                                                              *
*                                                                              *
*                                                                              *
*******************************************************************************/

static __forceinline__ __device__ void tex2DLayered(int *retVal, cudaTextureObject_t texObject, float x, float y, int layer)
{
   int4 tmp;
   asm volatile ("tex.a2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y));
   *retVal = (int)(tmp.x);
}

static __forceinline__ __device__ void tex2DLayered(int1 *retVal, cudaTextureObject_t texObject, float x, float y, int layer)
{
   int4 tmp;
   asm volatile ("tex.a2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y));
   *retVal = make_int1(tmp.x);
}

static __forceinline__ __device__ void tex2DLayered(int2 *retVal, cudaTextureObject_t texObject, float x, float y, int layer)
{
   int4 tmp;
   asm volatile ("tex.a2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y));
   *retVal = make_int2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex2DLayered(int4 *retVal, cudaTextureObject_t texObject, float x, float y, int layer)
{
   int4 tmp;
   asm volatile ("tex.a2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y));
   *retVal = make_int4(tmp.x, tmp.y, tmp.z, tmp.w);
}

static __forceinline__ __device__ void tex2DLayered(unsigned int *retVal, cudaTextureObject_t texObject, float x, float y, int layer)
{
   uint4 tmp;
   asm volatile ("tex.a2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y));
   *retVal = (unsigned int)(tmp.x);
}

static __forceinline__ __device__ void tex2DLayered(uint1 *retVal, cudaTextureObject_t texObject, float x, float y, int layer)
{
   uint4 tmp;
   asm volatile ("tex.a2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y));
   *retVal = make_uint1(tmp.x);
}

static __forceinline__ __device__ void tex2DLayered(uint2 *retVal, cudaTextureObject_t texObject, float x, float y, int layer)
{
   uint4 tmp;
   asm volatile ("tex.a2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y));
   *retVal = make_uint2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex2DLayered(uint4 *retVal, cudaTextureObject_t texObject, float x, float y, int layer)
{
   uint4 tmp;
   asm volatile ("tex.a2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y));
   *retVal = make_uint4(tmp.x, tmp.y, tmp.z, tmp.w);
}

/*******************************************************************************
*                                                                              *
*                                                                              *
*                                                                              *
*******************************************************************************/

#if !defined(__LP64__)

static __forceinline__ __device__ void tex2DLayered(long *retVal, cudaTextureObject_t texObject, float x, float y, int layer)
{
   int4 tmp;
   asm volatile ("tex.a2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y));
   *retVal = (long)(tmp.x);
}

static __forceinline__ __device__ void tex2DLayered(long1 *retVal, cudaTextureObject_t texObject, float x, float y, int layer)
{
   int4 tmp;
   asm volatile ("tex.a2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y));
   *retVal = make_long1(tmp.x);
}

static __forceinline__ __device__ void tex2DLayered(long2 *retVal, cudaTextureObject_t texObject, float x, float y, int layer)
{
   int4 tmp;
   asm volatile ("tex.a2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y));
   *retVal = make_long2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex2DLayered(long4 *retVal, cudaTextureObject_t texObject, float x, float y, int layer)
{
   int4 tmp;
   asm volatile ("tex.a2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y));
   *retVal = make_long4(tmp.x, tmp.y, tmp.z, tmp.w);
}

static __forceinline__ __device__ void tex2DLayered(unsigned long *retVal, cudaTextureObject_t texObject, float x, float y, int layer)
{
   uint4 tmp;
   asm volatile ("tex.a2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y));
   *retVal = (unsigned long)(tmp.x);
}

static __forceinline__ __device__ void tex2DLayered(ulong1 *retVal, cudaTextureObject_t texObject, float x, float y, int layer)
{
   uint4 tmp;
   asm volatile ("tex.a2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y));
   *retVal = make_ulong1(tmp.x);
}

static __forceinline__ __device__ void tex2DLayered(ulong2 *retVal, cudaTextureObject_t texObject, float x, float y, int layer)
{
   uint4 tmp;
   asm volatile ("tex.a2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y));
   *retVal = make_ulong2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex2DLayered(ulong4 *retVal, cudaTextureObject_t texObject, float x, float y, int layer)
{
   uint4 tmp;
   asm volatile ("tex.a2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y));
   *retVal = make_ulong4(tmp.x, tmp.y, tmp.z, tmp.w);
}

#endif /* !__LP64__ */


/*******************************************************************************
*                                                                              *
*                                                                              *
*                                                                              *
*******************************************************************************/

static __forceinline__ __device__ void tex2DLayered(float *retVal, cudaTextureObject_t texObject, float x, float y, int layer)
{
   float4 tmp;
   asm volatile ("tex.a2d.v4.f32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}];" : "=f"(tmp.x), "=f"(tmp.y), "=f"(tmp.z), "=f"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y));
   *retVal = (float)(tmp.x);
}

static __forceinline__ __device__ void tex2DLayered(float1 *retVal, cudaTextureObject_t texObject, float x, float y, int layer)
{
   float4 tmp;
   asm volatile ("tex.a2d.v4.f32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}];" : "=f"(tmp.x), "=f"(tmp.y), "=f"(tmp.z), "=f"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y));
   *retVal = make_float1(tmp.x);
}

static __forceinline__ __device__ void tex2DLayered(float2 *retVal, cudaTextureObject_t texObject, float x, float y, int layer)
{
   float4 tmp;
   asm volatile ("tex.a2d.v4.f32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}];" : "=f"(tmp.x), "=f"(tmp.y), "=f"(tmp.z), "=f"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y));
   *retVal = make_float2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex2DLayered(float4 *retVal, cudaTextureObject_t texObject, float x, float y, int layer)
{
   float4 tmp;
   asm volatile ("tex.a2d.v4.f32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}];" : "=f"(tmp.x), "=f"(tmp.y), "=f"(tmp.z), "=f"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y));
   *retVal = make_float4(tmp.x, tmp.y, tmp.z, tmp.w);
}

/*******************************************************************************
*                                                                              *
*                                                                              *
*                                                                              *
*******************************************************************************/

template <class T>
static __forceinline__ __device__ T tex2DLayered(cudaTextureObject_t texObject, float x, float y, int layer)
{
   T ret;
   tex2DLayered(&ret, texObject, x, y, layer);
   return ret;
}

/*******************************************************************************
*                                                                              *
* Cubemap Texture indirect fetch functions
*                                                                              *
*******************************************************************************/

static __forceinline__ __device__ void texCubemap(char *retVal, cudaTextureObject_t texObject, float x, float y, float z)
{
#if defined(_CHAR_UNSIGNED) || defined(__CHAR_UNSIGNED__)
    uint4 tmp;
    asm volatile ("tex.cube.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z));
#else /* _CHAR_UNSIGNED || __CHAR_UNSIGNED__ */
    int4 tmp;
    asm volatile ("tex.cube.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z));
#endif /* _CHAR_UNSIGNED || __CHAR_UNSIGNED__ */
    *retVal = (char)tmp.x;
}
static __forceinline__ __device__ void texCubemap(signed char *retVal, cudaTextureObject_t texObject, float x, float y, float z)
{
   int4 tmp;
   asm volatile ("tex.cube.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z));
   *retVal = (signed char)(tmp.x);
}

static __forceinline__ __device__ void texCubemap(char1 *retVal, cudaTextureObject_t texObject, float x, float y, float z)
{
   int4 tmp;
   asm volatile ("tex.cube.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z));
   *retVal = make_char1(tmp.x);
}

static __forceinline__ __device__ void texCubemap(char2 *retVal, cudaTextureObject_t texObject, float x, float y, float z)
{
   int4 tmp;
   asm volatile ("tex.cube.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z));
   *retVal = make_char2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void texCubemap(char4 *retVal, cudaTextureObject_t texObject, float x, float y, float z)
{
   int4 tmp;
   asm volatile ("tex.cube.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z));
   *retVal = make_char4(tmp.x, tmp.y, tmp.z, tmp.w);
}

static __forceinline__ __device__ void texCubemap(unsigned char *retVal, cudaTextureObject_t texObject, float x, float y, float z)
{
   uint4 tmp;
   asm volatile ("tex.cube.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z));
   *retVal = (unsigned char)(tmp.x);
}

static __forceinline__ __device__ void texCubemap(uchar1 *retVal, cudaTextureObject_t texObject, float x, float y, float z)
{
   uint4 tmp;
   asm volatile ("tex.cube.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z));
   *retVal = make_uchar1(tmp.x);
}

static __forceinline__ __device__ void texCubemap(uchar2 *retVal, cudaTextureObject_t texObject, float x, float y, float z)
{
   uint4 tmp;
   asm volatile ("tex.cube.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z));
   *retVal = make_uchar2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void texCubemap(uchar4 *retVal, cudaTextureObject_t texObject, float x, float y, float z)
{
   uint4 tmp;
   asm volatile ("tex.cube.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z));
   *retVal = make_uchar4(tmp.x, tmp.y, tmp.z, tmp.w);
}

/*******************************************************************************
*                                                                              *
*                                                                              *
*                                                                              *
*******************************************************************************/

static __forceinline__ __device__ void texCubemap(short *retVal, cudaTextureObject_t texObject, float x, float y, float z)
{
   int4 tmp;
   asm volatile ("tex.cube.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z));
   *retVal = (short)(tmp.x);
}

static __forceinline__ __device__ void texCubemap(short1 *retVal, cudaTextureObject_t texObject, float x, float y, float z)
{
   int4 tmp;
   asm volatile ("tex.cube.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z));
   *retVal = make_short1(tmp.x);
}

static __forceinline__ __device__ void texCubemap(short2 *retVal, cudaTextureObject_t texObject, float x, float y, float z)
{
   int4 tmp;
   asm volatile ("tex.cube.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z));
   *retVal = make_short2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void texCubemap(short4 *retVal, cudaTextureObject_t texObject, float x, float y, float z)
{
   int4 tmp;
   asm volatile ("tex.cube.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z));
   *retVal = make_short4(tmp.x, tmp.y, tmp.z, tmp.w);
}

static __forceinline__ __device__ void texCubemap(unsigned short *retVal, cudaTextureObject_t texObject, float x, float y, float z)
{
   uint4 tmp;
   asm volatile ("tex.cube.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z));
   *retVal = (unsigned short)(tmp.x);
}

static __forceinline__ __device__ void texCubemap(ushort1 *retVal, cudaTextureObject_t texObject, float x, float y, float z)
{
   uint4 tmp;
   asm volatile ("tex.cube.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z));
   *retVal = make_ushort1(tmp.x);
}

static __forceinline__ __device__ void texCubemap(ushort2 *retVal, cudaTextureObject_t texObject, float x, float y, float z)
{
   uint4 tmp;
   asm volatile ("tex.cube.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z));
   *retVal = make_ushort2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void texCubemap(ushort4 *retVal, cudaTextureObject_t texObject, float x, float y, float z)
{
   uint4 tmp;
   asm volatile ("tex.cube.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z));
   *retVal = make_ushort4(tmp.x, tmp.y, tmp.z, tmp.w);
}

/*******************************************************************************
*                                                                              *
*                                                                              *
*                                                                              *
*******************************************************************************/

static __forceinline__ __device__ void texCubemap(int *retVal, cudaTextureObject_t texObject, float x, float y, float z)
{
   int4 tmp;
   asm volatile ("tex.cube.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z));
   *retVal = (int)(tmp.x);
}

static __forceinline__ __device__ void texCubemap(int1 *retVal, cudaTextureObject_t texObject, float x, float y, float z)
{
   int4 tmp;
   asm volatile ("tex.cube.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z));
   *retVal = make_int1(tmp.x);
}

static __forceinline__ __device__ void texCubemap(int2 *retVal, cudaTextureObject_t texObject, float x, float y, float z)
{
   int4 tmp;
   asm volatile ("tex.cube.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z));
   *retVal = make_int2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void texCubemap(int4 *retVal, cudaTextureObject_t texObject, float x, float y, float z)
{
   int4 tmp;
   asm volatile ("tex.cube.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z));
   *retVal = make_int4(tmp.x, tmp.y, tmp.z, tmp.w);
}

static __forceinline__ __device__ void texCubemap(unsigned int *retVal, cudaTextureObject_t texObject, float x, float y, float z)
{
   uint4 tmp;
   asm volatile ("tex.cube.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z));
   *retVal = (unsigned int)(tmp.x);
}

static __forceinline__ __device__ void texCubemap(uint1 *retVal, cudaTextureObject_t texObject, float x, float y, float z)
{
   uint4 tmp;
   asm volatile ("tex.cube.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z));
   *retVal = make_uint1(tmp.x);
}

static __forceinline__ __device__ void texCubemap(uint2 *retVal, cudaTextureObject_t texObject, float x, float y, float z)
{
   uint4 tmp;
   asm volatile ("tex.cube.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z));
   *retVal = make_uint2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void texCubemap(uint4 *retVal, cudaTextureObject_t texObject, float x, float y, float z)
{
   uint4 tmp;
   asm volatile ("tex.cube.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z));
   *retVal = make_uint4(tmp.x, tmp.y, tmp.z, tmp.w);
}

/*******************************************************************************
*                                                                              *
*                                                                              *
*                                                                              *
*******************************************************************************/

#if !defined(__LP64__)

static __forceinline__ __device__ void texCubemap(long *retVal, cudaTextureObject_t texObject, float x, float y, float z)
{
   int4 tmp;
   asm volatile ("tex.cube.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z));
   *retVal = (long)(tmp.x);
}

static __forceinline__ __device__ void texCubemap(long1 *retVal, cudaTextureObject_t texObject, float x, float y, float z)
{
   int4 tmp;
   asm volatile ("tex.cube.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z));
   *retVal = make_long1(tmp.x);
}

static __forceinline__ __device__ void texCubemap(long2 *retVal, cudaTextureObject_t texObject, float x, float y, float z)
{
   int4 tmp;
   asm volatile ("tex.cube.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z));
   *retVal = make_long2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void texCubemap(long4 *retVal, cudaTextureObject_t texObject, float x, float y, float z)
{
   int4 tmp;
   asm volatile ("tex.cube.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z));
   *retVal = make_long4(tmp.x, tmp.y, tmp.z, tmp.w);
}

static __forceinline__ __device__ void texCubemap(unsigned long *retVal, cudaTextureObject_t texObject, float x, float y, float z)
{
   uint4 tmp;
   asm volatile ("tex.cube.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z));
   *retVal = (unsigned long)(tmp.x);
}

static __forceinline__ __device__ void texCubemap(ulong1 *retVal, cudaTextureObject_t texObject, float x, float y, float z)
{
   uint4 tmp;
   asm volatile ("tex.cube.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z));
   *retVal = make_ulong1(tmp.x);
}

static __forceinline__ __device__ void texCubemap(ulong2 *retVal, cudaTextureObject_t texObject, float x, float y, float z)
{
   uint4 tmp;
   asm volatile ("tex.cube.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z));
   *retVal = make_ulong2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void texCubemap(ulong4 *retVal, cudaTextureObject_t texObject, float x, float y, float z)
{
   uint4 tmp;
   asm volatile ("tex.cube.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z));
   *retVal = make_ulong4(tmp.x, tmp.y, tmp.z, tmp.w);
}

#endif /* !__LP64__ */


/*******************************************************************************
*                                                                              *
*                                                                              *
*                                                                              *
*******************************************************************************/

static __forceinline__ __device__ void texCubemap(float *retVal, cudaTextureObject_t texObject, float x, float y, float z)
{
   float4 tmp;
   asm volatile ("tex.cube.v4.f32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}];" : "=f"(tmp.x), "=f"(tmp.y), "=f"(tmp.z), "=f"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z));
   *retVal = (float)(tmp.x);
}

static __forceinline__ __device__ void texCubemap(float1 *retVal, cudaTextureObject_t texObject, float x, float y, float z)
{
   float4 tmp;
   asm volatile ("tex.cube.v4.f32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}];" : "=f"(tmp.x), "=f"(tmp.y), "=f"(tmp.z), "=f"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z));
   *retVal = make_float1(tmp.x);
}

static __forceinline__ __device__ void texCubemap(float2 *retVal, cudaTextureObject_t texObject, float x, float y, float z)
{
   float4 tmp;
   asm volatile ("tex.cube.v4.f32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}];" : "=f"(tmp.x), "=f"(tmp.y), "=f"(tmp.z), "=f"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z));
   *retVal = make_float2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void texCubemap(float4 *retVal, cudaTextureObject_t texObject, float x, float y, float z)
{
   float4 tmp;
   asm volatile ("tex.cube.v4.f32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}];" : "=f"(tmp.x), "=f"(tmp.y), "=f"(tmp.z), "=f"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z));
   *retVal = make_float4(tmp.x, tmp.y, tmp.z, tmp.w);
}

/*******************************************************************************
*                                                                              *
*                                                                              *
*                                                                              *
*******************************************************************************/

template <class T>
static __forceinline__ __device__ T texCubemap(cudaTextureObject_t texObject, float x, float y, float z)
{
   T ret;
   texCubemap(&ret, texObject, x, y, z);
   return ret;
}

/*******************************************************************************
*                                                                              *
* Cubemap Layered Texture indirect fetch functions
*                                                                              *
*******************************************************************************/

static __forceinline__ __device__ void texCubemapLayered(char *retVal, cudaTextureObject_t texObject, float x, float y, float z, int layer)
{
#if defined(_CHAR_UNSIGNED) || defined(__CHAR_UNSIGNED__)
    uint4 tmp;
    asm volatile ("tex.acube.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %8}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(z));
#else /* _CHAR_UNSIGNED || __CHAR_UNSIGNED__ */
    int4 tmp;
    asm volatile ("tex.acube.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %8}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(z));
#endif /* _CHAR_UNSIGNED || __CHAR_UNSIGNED__ */
    *retVal = (char)tmp.x;
}
static __forceinline__ __device__ void texCubemapLayered(signed char *retVal, cudaTextureObject_t texObject, float x, float y, float z, int layer)
{
   int4 tmp;
   asm volatile ("tex.acube.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %8}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(z));
   *retVal = (signed char)(tmp.x);
}

static __forceinline__ __device__ void texCubemapLayered(char1 *retVal, cudaTextureObject_t texObject, float x, float y, float z, int layer)
{
   int4 tmp;
   asm volatile ("tex.acube.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %8}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(z));
   *retVal = make_char1(tmp.x);
}

static __forceinline__ __device__ void texCubemapLayered(char2 *retVal, cudaTextureObject_t texObject, float x, float y, float z, int layer)
{
   int4 tmp;
   asm volatile ("tex.acube.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %8}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(z));
   *retVal = make_char2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void texCubemapLayered(char4 *retVal, cudaTextureObject_t texObject, float x, float y, float z, int layer)
{
   int4 tmp;
   asm volatile ("tex.acube.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %8}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(z));
   *retVal = make_char4(tmp.x, tmp.y, tmp.z, tmp.w);
}

static __forceinline__ __device__ void texCubemapLayered(unsigned char *retVal, cudaTextureObject_t texObject, float x, float y, float z, int layer)
{
   uint4 tmp;
   asm volatile ("tex.acube.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %8}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(z));
   *retVal = (unsigned char)(tmp.x);
}

static __forceinline__ __device__ void texCubemapLayered(uchar1 *retVal, cudaTextureObject_t texObject, float x, float y, float z, int layer)
{
   uint4 tmp;
   asm volatile ("tex.acube.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %8}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(z));
   *retVal = make_uchar1(tmp.x);
}

static __forceinline__ __device__ void texCubemapLayered(uchar2 *retVal, cudaTextureObject_t texObject, float x, float y, float z, int layer)
{
   uint4 tmp;
   asm volatile ("tex.acube.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %8}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(z));
   *retVal = make_uchar2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void texCubemapLayered(uchar4 *retVal, cudaTextureObject_t texObject, float x, float y, float z, int layer)
{
   uint4 tmp;
   asm volatile ("tex.acube.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %8}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(z));
   *retVal = make_uchar4(tmp.x, tmp.y, tmp.z, tmp.w);
}

/*******************************************************************************
*                                                                              *
*                                                                              *
*                                                                              *
*******************************************************************************/

static __forceinline__ __device__ void texCubemapLayered(short *retVal, cudaTextureObject_t texObject, float x, float y, float z, int layer)
{
   int4 tmp;
   asm volatile ("tex.acube.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %8}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(z));
   *retVal = (short)(tmp.x);
}

static __forceinline__ __device__ void texCubemapLayered(short1 *retVal, cudaTextureObject_t texObject, float x, float y, float z, int layer)
{
   int4 tmp;
   asm volatile ("tex.acube.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %8}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(z));
   *retVal = make_short1(tmp.x);
}

static __forceinline__ __device__ void texCubemapLayered(short2 *retVal, cudaTextureObject_t texObject, float x, float y, float z, int layer)
{
   int4 tmp;
   asm volatile ("tex.acube.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %8}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(z));
   *retVal = make_short2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void texCubemapLayered(short4 *retVal, cudaTextureObject_t texObject, float x, float y, float z, int layer)
{
   int4 tmp;
   asm volatile ("tex.acube.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %8}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(z));
   *retVal = make_short4(tmp.x, tmp.y, tmp.z, tmp.w);
}

static __forceinline__ __device__ void texCubemapLayered(unsigned short *retVal, cudaTextureObject_t texObject, float x, float y, float z, int layer)
{
   uint4 tmp;
   asm volatile ("tex.acube.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %8}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(z));
   *retVal = (unsigned short)(tmp.x);
}

static __forceinline__ __device__ void texCubemapLayered(ushort1 *retVal, cudaTextureObject_t texObject, float x, float y, float z, int layer)
{
   uint4 tmp;
   asm volatile ("tex.acube.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %8}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(z));
   *retVal = make_ushort1(tmp.x);
}

static __forceinline__ __device__ void texCubemapLayered(ushort2 *retVal, cudaTextureObject_t texObject, float x, float y, float z, int layer)
{
   uint4 tmp;
   asm volatile ("tex.acube.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %8}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(z));
   *retVal = make_ushort2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void texCubemapLayered(ushort4 *retVal, cudaTextureObject_t texObject, float x, float y, float z, int layer)
{
   uint4 tmp;
   asm volatile ("tex.acube.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %8}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(z));
   *retVal = make_ushort4(tmp.x, tmp.y, tmp.z, tmp.w);
}

/*******************************************************************************
*                                                                              *
*                                                                              *
*                                                                              *
*******************************************************************************/

static __forceinline__ __device__ void texCubemapLayered(int *retVal, cudaTextureObject_t texObject, float x, float y, float z, int layer)
{
   int4 tmp;
   asm volatile ("tex.acube.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %8}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(z));
   *retVal = (int)(tmp.x);
}

static __forceinline__ __device__ void texCubemapLayered(int1 *retVal, cudaTextureObject_t texObject, float x, float y, float z, int layer)
{
   int4 tmp;
   asm volatile ("tex.acube.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %8}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(z));
   *retVal = make_int1(tmp.x);
}

static __forceinline__ __device__ void texCubemapLayered(int2 *retVal, cudaTextureObject_t texObject, float x, float y, float z, int layer)
{
   int4 tmp;
   asm volatile ("tex.acube.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %8}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(z));
   *retVal = make_int2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void texCubemapLayered(int4 *retVal, cudaTextureObject_t texObject, float x, float y, float z, int layer)
{
   int4 tmp;
   asm volatile ("tex.acube.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %8}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(z));
   *retVal = make_int4(tmp.x, tmp.y, tmp.z, tmp.w);
}

static __forceinline__ __device__ void texCubemapLayered(unsigned int *retVal, cudaTextureObject_t texObject, float x, float y, float z, int layer)
{
   uint4 tmp;
   asm volatile ("tex.acube.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %8}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(z));
   *retVal = (unsigned int)(tmp.x);
}

static __forceinline__ __device__ void texCubemapLayered(uint1 *retVal, cudaTextureObject_t texObject, float x, float y, float z, int layer)
{
   uint4 tmp;
   asm volatile ("tex.acube.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %8}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(z));
   *retVal = make_uint1(tmp.x);
}

static __forceinline__ __device__ void texCubemapLayered(uint2 *retVal, cudaTextureObject_t texObject, float x, float y, float z, int layer)
{
   uint4 tmp;
   asm volatile ("tex.acube.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %8}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(z));
   *retVal = make_uint2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void texCubemapLayered(uint4 *retVal, cudaTextureObject_t texObject, float x, float y, float z, int layer)
{
   uint4 tmp;
   asm volatile ("tex.acube.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %8}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(z));
   *retVal = make_uint4(tmp.x, tmp.y, tmp.z, tmp.w);
}

/*******************************************************************************
*                                                                              *
*                                                                              *
*                                                                              *
*******************************************************************************/

#if !defined(__LP64__)

static __forceinline__ __device__ void texCubemapLayered(long *retVal, cudaTextureObject_t texObject, float x, float y, float z, int layer)
{
   int4 tmp;
   asm volatile ("tex.acube.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %8}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(z));
   *retVal = (long)(tmp.x);
}

static __forceinline__ __device__ void texCubemapLayered(long1 *retVal, cudaTextureObject_t texObject, float x, float y, float z, int layer)
{
   int4 tmp;
   asm volatile ("tex.acube.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %8}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(z));
   *retVal = make_long1(tmp.x);
}

static __forceinline__ __device__ void texCubemapLayered(long2 *retVal, cudaTextureObject_t texObject, float x, float y, float z, int layer)
{
   int4 tmp;
   asm volatile ("tex.acube.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %8}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(z));
   *retVal = make_long2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void texCubemapLayered(long4 *retVal, cudaTextureObject_t texObject, float x, float y, float z, int layer)
{
   int4 tmp;
   asm volatile ("tex.acube.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %8}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(z));
   *retVal = make_long4(tmp.x, tmp.y, tmp.z, tmp.w);
}

static __forceinline__ __device__ void texCubemapLayered(unsigned long *retVal, cudaTextureObject_t texObject, float x, float y, float z, int layer)
{
   uint4 tmp;
   asm volatile ("tex.acube.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %8}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(z));
   *retVal = (unsigned long)(tmp.x);
}

static __forceinline__ __device__ void texCubemapLayered(ulong1 *retVal, cudaTextureObject_t texObject, float x, float y, float z, int layer)
{
   uint4 tmp;
   asm volatile ("tex.acube.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %8}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(z));
   *retVal = make_ulong1(tmp.x);
}

static __forceinline__ __device__ void texCubemapLayered(ulong2 *retVal, cudaTextureObject_t texObject, float x, float y, float z, int layer)
{
   uint4 tmp;
   asm volatile ("tex.acube.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %8}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(z));
   *retVal = make_ulong2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void texCubemapLayered(ulong4 *retVal, cudaTextureObject_t texObject, float x, float y, float z, int layer)
{
   uint4 tmp;
   asm volatile ("tex.acube.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %8}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(z));
   *retVal = make_ulong4(tmp.x, tmp.y, tmp.z, tmp.w);
}

#endif /* !__LP64__ */


/*******************************************************************************
*                                                                              *
*                                                                              *
*                                                                              *
*******************************************************************************/

static __forceinline__ __device__ void texCubemapLayered(float *retVal, cudaTextureObject_t texObject, float x, float y, float z, int layer)
{
   float4 tmp;
   asm volatile ("tex.acube.v4.f32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %8}];" : "=f"(tmp.x), "=f"(tmp.y), "=f"(tmp.z), "=f"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(z));
   *retVal = (float)(tmp.x);
}

static __forceinline__ __device__ void texCubemapLayered(float1 *retVal, cudaTextureObject_t texObject, float x, float y, float z, int layer)
{
   float4 tmp;
   asm volatile ("tex.acube.v4.f32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %8}];" : "=f"(tmp.x), "=f"(tmp.y), "=f"(tmp.z), "=f"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(z));
   *retVal = make_float1(tmp.x);
}

static __forceinline__ __device__ void texCubemapLayered(float2 *retVal, cudaTextureObject_t texObject, float x, float y, float z, int layer)
{
   float4 tmp;
   asm volatile ("tex.acube.v4.f32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %8}];" : "=f"(tmp.x), "=f"(tmp.y), "=f"(tmp.z), "=f"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(z));
   *retVal = make_float2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void texCubemapLayered(float4 *retVal, cudaTextureObject_t texObject, float x, float y, float z, int layer)
{
   float4 tmp;
   asm volatile ("tex.acube.v4.f32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %8}];" : "=f"(tmp.x), "=f"(tmp.y), "=f"(tmp.z), "=f"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(z));
   *retVal = make_float4(tmp.x, tmp.y, tmp.z, tmp.w);
}

/*******************************************************************************
*                                                                              *
*                                                                              *
*                                                                              *
*******************************************************************************/

template <class T>
static __forceinline__ __device__ T texCubemapLayered(cudaTextureObject_t texObject, float x, float y, float z, int layer)
{
   T ret;
   texCubemapLayered(&ret, texObject, x, y, z, layer);
   return ret;
}

/*******************************************************************************
*                                                                              *
* 2D Texture indirect gather functions
*                                                                              *
*******************************************************************************/

static __forceinline__ __device__ void tex2Dgather(char *retVal, cudaTextureObject_t texObject, float x, float y, int comp = 0)
{
   int4 tmp;
   if (comp == 0) {
       asm volatile ("tld4.r.2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   else if (comp == 1) {
       asm volatile ("tld4.g.2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   else if (comp == 2) {
       asm volatile ("tld4.b.2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   else if (comp == 3) {
       asm volatile ("tld4.a.2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   *retVal = (char)(tmp.x);
}

static __forceinline__ __device__ void tex2Dgather(signed char *retVal, cudaTextureObject_t texObject, float x, float y, int comp = 0)
{
   int4 tmp;
   if (comp == 0) {
       asm volatile ("tld4.r.2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   else if (comp == 1) {
       asm volatile ("tld4.g.2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   else if (comp == 2) {
       asm volatile ("tld4.b.2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   else if (comp == 3) {
       asm volatile ("tld4.a.2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   *retVal = (signed char)(tmp.x);
}

static __forceinline__ __device__ void tex2Dgather(char1 *retVal, cudaTextureObject_t texObject, float x, float y, int comp = 0)
{
   int4 tmp;
   if (comp == 0) {
       asm volatile ("tld4.r.2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   else if (comp == 1) {
       asm volatile ("tld4.g.2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   else if (comp == 2) {
       asm volatile ("tld4.b.2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   else if (comp == 3) {
       asm volatile ("tld4.a.2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   *retVal = make_char1(tmp.x);
}

static __forceinline__ __device__ void tex2Dgather(char2 *retVal, cudaTextureObject_t texObject, float x, float y, int comp = 0)
{
   int4 tmp;
   if (comp == 0) {
       asm volatile ("tld4.r.2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   else if (comp == 1) {
       asm volatile ("tld4.g.2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   else if (comp == 2) {
       asm volatile ("tld4.b.2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   else if (comp == 3) {
       asm volatile ("tld4.a.2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   *retVal = make_char2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex2Dgather(char4 *retVal, cudaTextureObject_t texObject, float x, float y, int comp = 0)
{
   int4 tmp;
   if (comp == 0) {
       asm volatile ("tld4.r.2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   else if (comp == 1) {
       asm volatile ("tld4.g.2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   else if (comp == 2) {
       asm volatile ("tld4.b.2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   else if (comp == 3) {
       asm volatile ("tld4.a.2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   *retVal = make_char4(tmp.x, tmp.y, tmp.z, tmp.w);
}

static __forceinline__ __device__ void tex2Dgather(unsigned char *retVal, cudaTextureObject_t texObject, float x, float y, int comp = 0)
{
   uint4 tmp;
   if (comp == 0) {
       asm volatile ("tld4.r.2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   else if (comp == 1) {
       asm volatile ("tld4.g.2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   else if (comp == 2) {
       asm volatile ("tld4.b.2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   else if (comp == 3) {
       asm volatile ("tld4.a.2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   *retVal = (unsigned char)(tmp.x);
}

static __forceinline__ __device__ void tex2Dgather(uchar1 *retVal, cudaTextureObject_t texObject, float x, float y, int comp = 0)
{
   uint4 tmp;
   if (comp == 0) {
       asm volatile ("tld4.r.2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   else if (comp == 1) {
       asm volatile ("tld4.g.2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   else if (comp == 2) {
       asm volatile ("tld4.b.2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   else if (comp == 3) {
       asm volatile ("tld4.a.2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   *retVal = make_uchar1(tmp.x);
}

static __forceinline__ __device__ void tex2Dgather(uchar2 *retVal, cudaTextureObject_t texObject, float x, float y, int comp = 0)
{
   uint4 tmp;
   if (comp == 0) {
       asm volatile ("tld4.r.2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   else if (comp == 1) {
       asm volatile ("tld4.g.2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   else if (comp == 2) {
       asm volatile ("tld4.b.2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   else if (comp == 3) {
       asm volatile ("tld4.a.2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   *retVal = make_uchar2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex2Dgather(uchar4 *retVal, cudaTextureObject_t texObject, float x, float y, int comp = 0)
{
   uint4 tmp;
   if (comp == 0) {
       asm volatile ("tld4.r.2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   else if (comp == 1) {
       asm volatile ("tld4.g.2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   else if (comp == 2) {
       asm volatile ("tld4.b.2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   else if (comp == 3) {
       asm volatile ("tld4.a.2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   *retVal = make_uchar4(tmp.x, tmp.y, tmp.z, tmp.w);
}

/*******************************************************************************
*                                                                              *
*                                                                              *
*                                                                              *
*******************************************************************************/

static __forceinline__ __device__ void tex2Dgather(short *retVal, cudaTextureObject_t texObject, float x, float y, int comp = 0)
{
   int4 tmp;
   if (comp == 0) {
       asm volatile ("tld4.r.2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   else if (comp == 1) {
       asm volatile ("tld4.g.2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   else if (comp == 2) {
       asm volatile ("tld4.b.2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   else if (comp == 3) {
       asm volatile ("tld4.a.2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   *retVal = (short)(tmp.x);
}

static __forceinline__ __device__ void tex2Dgather(short1 *retVal, cudaTextureObject_t texObject, float x, float y, int comp = 0)
{
   int4 tmp;
   if (comp == 0) {
       asm volatile ("tld4.r.2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   else if (comp == 1) {
       asm volatile ("tld4.g.2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   else if (comp == 2) {
       asm volatile ("tld4.b.2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   else if (comp == 3) {
       asm volatile ("tld4.a.2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   *retVal = make_short1(tmp.x);
}

static __forceinline__ __device__ void tex2Dgather(short2 *retVal, cudaTextureObject_t texObject, float x, float y, int comp = 0)
{
   int4 tmp;
   if (comp == 0) {
       asm volatile ("tld4.r.2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   else if (comp == 1) {
       asm volatile ("tld4.g.2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   else if (comp == 2) {
       asm volatile ("tld4.b.2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   else if (comp == 3) {
       asm volatile ("tld4.a.2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   *retVal = make_short2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex2Dgather(short4 *retVal, cudaTextureObject_t texObject, float x, float y, int comp = 0)
{
   int4 tmp;
   if (comp == 0) {
       asm volatile ("tld4.r.2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   else if (comp == 1) {
       asm volatile ("tld4.g.2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   else if (comp == 2) {
       asm volatile ("tld4.b.2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   else if (comp == 3) {
       asm volatile ("tld4.a.2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   *retVal = make_short4(tmp.x, tmp.y, tmp.z, tmp.w);
}

static __forceinline__ __device__ void tex2Dgather(unsigned short *retVal, cudaTextureObject_t texObject, float x, float y, int comp = 0)
{
   uint4 tmp;
   if (comp == 0) {
       asm volatile ("tld4.r.2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   else if (comp == 1) {
       asm volatile ("tld4.g.2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   else if (comp == 2) {
       asm volatile ("tld4.b.2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   else if (comp == 3) {
       asm volatile ("tld4.a.2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   *retVal = (unsigned short)(tmp.x);
}

static __forceinline__ __device__ void tex2Dgather(ushort1 *retVal, cudaTextureObject_t texObject, float x, float y, int comp = 0)
{
   uint4 tmp;
   if (comp == 0) {
       asm volatile ("tld4.r.2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   else if (comp == 1) {
       asm volatile ("tld4.g.2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   else if (comp == 2) {
       asm volatile ("tld4.b.2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   else if (comp == 3) {
       asm volatile ("tld4.a.2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   *retVal = make_ushort1(tmp.x);
}

static __forceinline__ __device__ void tex2Dgather(ushort2 *retVal, cudaTextureObject_t texObject, float x, float y, int comp = 0)
{
   uint4 tmp;
   if (comp == 0) {
       asm volatile ("tld4.r.2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   else if (comp == 1) {
       asm volatile ("tld4.g.2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   else if (comp == 2) {
       asm volatile ("tld4.b.2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   else if (comp == 3) {
       asm volatile ("tld4.a.2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   *retVal = make_ushort2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex2Dgather(ushort4 *retVal, cudaTextureObject_t texObject, float x, float y, int comp = 0)
{
   uint4 tmp;
   if (comp == 0) {
       asm volatile ("tld4.r.2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   else if (comp == 1) {
       asm volatile ("tld4.g.2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   else if (comp == 2) {
       asm volatile ("tld4.b.2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   else if (comp == 3) {
       asm volatile ("tld4.a.2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   *retVal = make_ushort4(tmp.x, tmp.y, tmp.z, tmp.w);
}

/*******************************************************************************
*                                                                              *
*                                                                              *
*                                                                              *
*******************************************************************************/

static __forceinline__ __device__ void tex2Dgather(int *retVal, cudaTextureObject_t texObject, float x, float y, int comp = 0)
{
   int4 tmp;
   if (comp == 0) {
       asm volatile ("tld4.r.2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   else if (comp == 1) {
       asm volatile ("tld4.g.2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   else if (comp == 2) {
       asm volatile ("tld4.b.2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   else if (comp == 3) {
       asm volatile ("tld4.a.2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   *retVal = (int)(tmp.x);
}

static __forceinline__ __device__ void tex2Dgather(int1 *retVal, cudaTextureObject_t texObject, float x, float y, int comp = 0)
{
   int4 tmp;
   if (comp == 0) {
       asm volatile ("tld4.r.2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   else if (comp == 1) {
       asm volatile ("tld4.g.2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   else if (comp == 2) {
       asm volatile ("tld4.b.2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   else if (comp == 3) {
       asm volatile ("tld4.a.2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   *retVal = make_int1(tmp.x);
}

static __forceinline__ __device__ void tex2Dgather(int2 *retVal, cudaTextureObject_t texObject, float x, float y, int comp = 0)
{
   int4 tmp;
   if (comp == 0) {
       asm volatile ("tld4.r.2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   else if (comp == 1) {
       asm volatile ("tld4.g.2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   else if (comp == 2) {
       asm volatile ("tld4.b.2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   else if (comp == 3) {
       asm volatile ("tld4.a.2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   *retVal = make_int2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex2Dgather(int4 *retVal, cudaTextureObject_t texObject, float x, float y, int comp = 0)
{
   int4 tmp;
   if (comp == 0) {
       asm volatile ("tld4.r.2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   else if (comp == 1) {
       asm volatile ("tld4.g.2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   else if (comp == 2) {
       asm volatile ("tld4.b.2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   else if (comp == 3) {
       asm volatile ("tld4.a.2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   *retVal = make_int4(tmp.x, tmp.y, tmp.z, tmp.w);
}

static __forceinline__ __device__ void tex2Dgather(unsigned int *retVal, cudaTextureObject_t texObject, float x, float y, int comp = 0)
{
   uint4 tmp;
   if (comp == 0) {
       asm volatile ("tld4.r.2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   else if (comp == 1) {
       asm volatile ("tld4.g.2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   else if (comp == 2) {
       asm volatile ("tld4.b.2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   else if (comp == 3) {
       asm volatile ("tld4.a.2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   *retVal = (unsigned int)(tmp.x);
}

static __forceinline__ __device__ void tex2Dgather(uint1 *retVal, cudaTextureObject_t texObject, float x, float y, int comp = 0)
{
   uint4 tmp;
   if (comp == 0) {
       asm volatile ("tld4.r.2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   else if (comp == 1) {
       asm volatile ("tld4.g.2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   else if (comp == 2) {
       asm volatile ("tld4.b.2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   else if (comp == 3) {
       asm volatile ("tld4.a.2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   *retVal = make_uint1(tmp.x);
}

static __forceinline__ __device__ void tex2Dgather(uint2 *retVal, cudaTextureObject_t texObject, float x, float y, int comp = 0)
{
   uint4 tmp;
   if (comp == 0) {
       asm volatile ("tld4.r.2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   else if (comp == 1) {
       asm volatile ("tld4.g.2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   else if (comp == 2) {
       asm volatile ("tld4.b.2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   else if (comp == 3) {
       asm volatile ("tld4.a.2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   *retVal = make_uint2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex2Dgather(uint4 *retVal, cudaTextureObject_t texObject, float x, float y, int comp = 0)
{
   uint4 tmp;
   if (comp == 0) {
       asm volatile ("tld4.r.2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   else if (comp == 1) {
       asm volatile ("tld4.g.2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   else if (comp == 2) {
       asm volatile ("tld4.b.2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   else if (comp == 3) {
       asm volatile ("tld4.a.2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   *retVal = make_uint4(tmp.x, tmp.y, tmp.z, tmp.w);
}

/*******************************************************************************
*                                                                              *
*                                                                              *
*                                                                              *
*******************************************************************************/

static __forceinline__ __device__ void tex2Dgather(long *retVal, cudaTextureObject_t texObject, float x, float y, int comp = 0)
{
   int4 tmp;
   if (comp == 0) {
       asm volatile ("tld4.r.2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   else if (comp == 1) {
       asm volatile ("tld4.g.2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   else if (comp == 2) {
       asm volatile ("tld4.b.2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   else if (comp == 3) {
       asm volatile ("tld4.a.2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   *retVal = (long)(tmp.x);
}

static __forceinline__ __device__ void tex2Dgather(long1 *retVal, cudaTextureObject_t texObject, float x, float y, int comp = 0)
{
   int4 tmp;
   if (comp == 0) {
       asm volatile ("tld4.r.2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   else if (comp == 1) {
       asm volatile ("tld4.g.2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   else if (comp == 2) {
       asm volatile ("tld4.b.2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   else if (comp == 3) {
       asm volatile ("tld4.a.2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   *retVal = make_long1(tmp.x);
}

static __forceinline__ __device__ void tex2Dgather(long2 *retVal, cudaTextureObject_t texObject, float x, float y, int comp = 0)
{
   int4 tmp;
   if (comp == 0) {
       asm volatile ("tld4.r.2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   else if (comp == 1) {
       asm volatile ("tld4.g.2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   else if (comp == 2) {
       asm volatile ("tld4.b.2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   else if (comp == 3) {
       asm volatile ("tld4.a.2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   *retVal = make_long2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex2Dgather(long4 *retVal, cudaTextureObject_t texObject, float x, float y, int comp = 0)
{
   int4 tmp;
   if (comp == 0) {
       asm volatile ("tld4.r.2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   else if (comp == 1) {
       asm volatile ("tld4.g.2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   else if (comp == 2) {
       asm volatile ("tld4.b.2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   else if (comp == 3) {
       asm volatile ("tld4.a.2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   *retVal = make_long4(tmp.x, tmp.y, tmp.z, tmp.w);
}

static __forceinline__ __device__ void tex2Dgather(unsigned long *retVal, cudaTextureObject_t texObject, float x, float y, int comp = 0)
{
   uint4 tmp;
   if (comp == 0) {
       asm volatile ("tld4.r.2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   else if (comp == 1) {
       asm volatile ("tld4.g.2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   else if (comp == 2) {
       asm volatile ("tld4.b.2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   else if (comp == 3) {
       asm volatile ("tld4.a.2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   *retVal = (unsigned long)(tmp.x);
}

static __forceinline__ __device__ void tex2Dgather(ulong1 *retVal, cudaTextureObject_t texObject, float x, float y, int comp = 0)
{
   uint4 tmp;
   if (comp == 0) {
       asm volatile ("tld4.r.2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   else if (comp == 1) {
       asm volatile ("tld4.g.2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   else if (comp == 2) {
       asm volatile ("tld4.b.2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   else if (comp == 3) {
       asm volatile ("tld4.a.2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   *retVal = make_ulong1(tmp.x);
}

static __forceinline__ __device__ void tex2Dgather(ulong2 *retVal, cudaTextureObject_t texObject, float x, float y, int comp = 0)
{
   uint4 tmp;
   if (comp == 0) {
       asm volatile ("tld4.r.2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   else if (comp == 1) {
       asm volatile ("tld4.g.2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   else if (comp == 2) {
       asm volatile ("tld4.b.2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   else if (comp == 3) {
       asm volatile ("tld4.a.2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   *retVal = make_ulong2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex2Dgather(ulong4 *retVal, cudaTextureObject_t texObject, float x, float y, int comp = 0)
{
   uint4 tmp;
   if (comp == 0) {
       asm volatile ("tld4.r.2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   else if (comp == 1) {
       asm volatile ("tld4.g.2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   else if (comp == 2) {
       asm volatile ("tld4.b.2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   else if (comp == 3) {
       asm volatile ("tld4.a.2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   *retVal = make_ulong4(tmp.x, tmp.y, tmp.z, tmp.w);
}

/*******************************************************************************
*                                                                              *
*                                                                              *
*                                                                              *
*******************************************************************************/

static __forceinline__ __device__ void tex2Dgather(float *retVal, cudaTextureObject_t texObject, float x, float y, int comp = 0)
{
   float4 tmp;
   if (comp == 0) {
       asm volatile ("tld4.r.2d.v4.f32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=f"(tmp.x), "=f"(tmp.y), "=f"(tmp.z), "=f"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   else if (comp == 1) {
       asm volatile ("tld4.g.2d.v4.f32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=f"(tmp.x), "=f"(tmp.y), "=f"(tmp.z), "=f"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   else if (comp == 2) {
       asm volatile ("tld4.b.2d.v4.f32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=f"(tmp.x), "=f"(tmp.y), "=f"(tmp.z), "=f"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   else if (comp == 3) {
       asm volatile ("tld4.a.2d.v4.f32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=f"(tmp.x), "=f"(tmp.y), "=f"(tmp.z), "=f"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   *retVal = (float)(tmp.x);
}

static __forceinline__ __device__ void tex2Dgather(float1 *retVal, cudaTextureObject_t texObject, float x, float y, int comp = 0)
{
   float4 tmp;
   if (comp == 0) {
       asm volatile ("tld4.r.2d.v4.f32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=f"(tmp.x), "=f"(tmp.y), "=f"(tmp.z), "=f"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   else if (comp == 1) {
       asm volatile ("tld4.g.2d.v4.f32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=f"(tmp.x), "=f"(tmp.y), "=f"(tmp.z), "=f"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   else if (comp == 2) {
       asm volatile ("tld4.b.2d.v4.f32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=f"(tmp.x), "=f"(tmp.y), "=f"(tmp.z), "=f"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   else if (comp == 3) {
       asm volatile ("tld4.a.2d.v4.f32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=f"(tmp.x), "=f"(tmp.y), "=f"(tmp.z), "=f"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   *retVal = make_float1(tmp.x);
}

static __forceinline__ __device__ void tex2Dgather(float2 *retVal, cudaTextureObject_t texObject, float x, float y, int comp = 0)
{
   float4 tmp;
   if (comp == 0) {
       asm volatile ("tld4.r.2d.v4.f32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=f"(tmp.x), "=f"(tmp.y), "=f"(tmp.z), "=f"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   else if (comp == 1) {
       asm volatile ("tld4.g.2d.v4.f32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=f"(tmp.x), "=f"(tmp.y), "=f"(tmp.z), "=f"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   else if (comp == 2) {
       asm volatile ("tld4.b.2d.v4.f32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=f"(tmp.x), "=f"(tmp.y), "=f"(tmp.z), "=f"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   else if (comp == 3) {
       asm volatile ("tld4.a.2d.v4.f32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=f"(tmp.x), "=f"(tmp.y), "=f"(tmp.z), "=f"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   *retVal = make_float2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex2Dgather(float4 *retVal, cudaTextureObject_t texObject, float x, float y, int comp = 0)
{
   float4 tmp;
   if (comp == 0) {
       asm volatile ("tld4.r.2d.v4.f32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=f"(tmp.x), "=f"(tmp.y), "=f"(tmp.z), "=f"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   else if (comp == 1) {
       asm volatile ("tld4.g.2d.v4.f32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=f"(tmp.x), "=f"(tmp.y), "=f"(tmp.z), "=f"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   else if (comp == 2) {
       asm volatile ("tld4.b.2d.v4.f32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=f"(tmp.x), "=f"(tmp.y), "=f"(tmp.z), "=f"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   else if (comp == 3) {
       asm volatile ("tld4.a.2d.v4.f32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}];" : "=f"(tmp.x), "=f"(tmp.y), "=f"(tmp.z), "=f"(tmp.w) : "l"(texObject), "f"(x), "f"(y));
   }
   *retVal = make_float4(tmp.x, tmp.y, tmp.z, tmp.w);
}

/*******************************************************************************
*                                                                              *
*                                                                              *
*                                                                              *
*******************************************************************************/

template <class T>
static __forceinline__ __device__ T tex2Dgather(cudaTextureObject_t to, float x, float y, int comp = 0)
{
   T ret;
   tex2Dgather(&ret, to, x, y, comp);
   return ret;
}

/*******************************************************************************
*                                                                              *
* 1D mipmapped texture indirect fetch functions
*                                                                              *
*******************************************************************************/

static __forceinline__ __device__ void tex1DLod(char *retVal, cudaTextureObject_t texObject, float x, float level)
{
#if defined(_CHAR_UNSIGNED) || defined(__CHAR_UNSIGNED__)
    uint4 tmp;
    asm volatile ("tex.level.1d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5}], %6;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(level));
#else /* _CHAR_UNSIGNED || __CHAR_UNSIGNED__ */
    int4 tmp;
    asm volatile ("tex.level.1d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5}], %6;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(level));
#endif /* _CHAR_UNSIGNED || __CHAR_UNSIGNED__ */
    *retVal = (char)tmp.x;
}
static __forceinline__ __device__ void tex1DLod(signed char *retVal, cudaTextureObject_t texObject, float x, float level)
{
   int4 tmp;
   asm volatile ("tex.level.1d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5}], %6;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(level));
   *retVal = (signed char)(tmp.x);
}

static __forceinline__ __device__ void tex1DLod(char1 *retVal, cudaTextureObject_t texObject, float x, float level)
{
   int4 tmp;
   asm volatile ("tex.level.1d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5}], %6;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(level));
   *retVal = make_char1(tmp.x);
}

static __forceinline__ __device__ void tex1DLod(char2 *retVal, cudaTextureObject_t texObject, float x, float level)
{
   int4 tmp;
   asm volatile ("tex.level.1d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5}], %6;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(level));
   *retVal = make_char2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex1DLod(char4 *retVal, cudaTextureObject_t texObject, float x, float level)
{
   int4 tmp;
   asm volatile ("tex.level.1d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5}], %6;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(level));
   *retVal = make_char4(tmp.x, tmp.y, tmp.z, tmp.w);
}

static __forceinline__ __device__ void tex1DLod(unsigned char *retVal, cudaTextureObject_t texObject, float x, float level)
{
   uint4 tmp;
   asm volatile ("tex.level.1d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5}], %6;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(level));
   *retVal = (unsigned char)(tmp.x);
}

static __forceinline__ __device__ void tex1DLod(uchar1 *retVal, cudaTextureObject_t texObject, float x, float level)
{
   uint4 tmp;
   asm volatile ("tex.level.1d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5}], %6;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(level));
   *retVal = make_uchar1(tmp.x);
}

static __forceinline__ __device__ void tex1DLod(uchar2 *retVal, cudaTextureObject_t texObject, float x, float level)
{
   uint4 tmp;
   asm volatile ("tex.level.1d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5}], %6;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(level));
   *retVal = make_uchar2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex1DLod(uchar4 *retVal, cudaTextureObject_t texObject, float x, float level)
{
   uint4 tmp;
   asm volatile ("tex.level.1d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5}], %6;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(level));
   *retVal = make_uchar4(tmp.x, tmp.y, tmp.z, tmp.w);
}

/*******************************************************************************
*                                                                              *
*                                                                              *
*                                                                              *
*******************************************************************************/

static __forceinline__ __device__ void tex1DLod(short *retVal, cudaTextureObject_t texObject, float x, float level)
{
   int4 tmp;
   asm volatile ("tex.level.1d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5}], %6;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(level));
   *retVal = (short)(tmp.x);
}

static __forceinline__ __device__ void tex1DLod(short1 *retVal, cudaTextureObject_t texObject, float x, float level)
{
   int4 tmp;
   asm volatile ("tex.level.1d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5}], %6;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(level));
   *retVal = make_short1(tmp.x);
}

static __forceinline__ __device__ void tex1DLod(short2 *retVal, cudaTextureObject_t texObject, float x, float level)
{
   int4 tmp;
   asm volatile ("tex.level.1d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5}], %6;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(level));
   *retVal = make_short2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex1DLod(short4 *retVal, cudaTextureObject_t texObject, float x, float level)
{
   int4 tmp;
   asm volatile ("tex.level.1d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5}], %6;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(level));
   *retVal = make_short4(tmp.x, tmp.y, tmp.z, tmp.w);
}

static __forceinline__ __device__ void tex1DLod(unsigned short *retVal, cudaTextureObject_t texObject, float x, float level)
{
   uint4 tmp;
   asm volatile ("tex.level.1d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5}], %6;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(level));
   *retVal = (unsigned short)(tmp.x);
}

static __forceinline__ __device__ void tex1DLod(ushort1 *retVal, cudaTextureObject_t texObject, float x, float level)
{
   uint4 tmp;
   asm volatile ("tex.level.1d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5}], %6;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(level));
   *retVal = make_ushort1(tmp.x);
}

static __forceinline__ __device__ void tex1DLod(ushort2 *retVal, cudaTextureObject_t texObject, float x, float level)
{
   uint4 tmp;
   asm volatile ("tex.level.1d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5}], %6;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(level));
   *retVal = make_ushort2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex1DLod(ushort4 *retVal, cudaTextureObject_t texObject, float x, float level)
{
   uint4 tmp;
   asm volatile ("tex.level.1d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5}], %6;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(level));
   *retVal = make_ushort4(tmp.x, tmp.y, tmp.z, tmp.w);
}

/*******************************************************************************
*                                                                              *
*                                                                              *
*                                                                              *
*******************************************************************************/

static __forceinline__ __device__ void tex1DLod(int *retVal, cudaTextureObject_t texObject, float x, float level)
{
   int4 tmp;
   asm volatile ("tex.level.1d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5}], %6;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(level));
   *retVal = (int)(tmp.x);
}

static __forceinline__ __device__ void tex1DLod(int1 *retVal, cudaTextureObject_t texObject, float x, float level)
{
   int4 tmp;
   asm volatile ("tex.level.1d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5}], %6;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(level));
   *retVal = make_int1(tmp.x);
}

static __forceinline__ __device__ void tex1DLod(int2 *retVal, cudaTextureObject_t texObject, float x, float level)
{
   int4 tmp;
   asm volatile ("tex.level.1d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5}], %6;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(level));
   *retVal = make_int2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex1DLod(int4 *retVal, cudaTextureObject_t texObject, float x, float level)
{
   int4 tmp;
   asm volatile ("tex.level.1d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5}], %6;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(level));
   *retVal = make_int4(tmp.x, tmp.y, tmp.z, tmp.w);
}

static __forceinline__ __device__ void tex1DLod(unsigned int *retVal, cudaTextureObject_t texObject, float x, float level)
{
   uint4 tmp;
   asm volatile ("tex.level.1d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5}], %6;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(level));
   *retVal = (unsigned int)(tmp.x);
}

static __forceinline__ __device__ void tex1DLod(uint1 *retVal, cudaTextureObject_t texObject, float x, float level)
{
   uint4 tmp;
   asm volatile ("tex.level.1d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5}], %6;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(level));
   *retVal = make_uint1(tmp.x);
}

static __forceinline__ __device__ void tex1DLod(uint2 *retVal, cudaTextureObject_t texObject, float x, float level)
{
   uint4 tmp;
   asm volatile ("tex.level.1d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5}], %6;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(level));
   *retVal = make_uint2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex1DLod(uint4 *retVal, cudaTextureObject_t texObject, float x, float level)
{
   uint4 tmp;
   asm volatile ("tex.level.1d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5}], %6;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(level));
   *retVal = make_uint4(tmp.x, tmp.y, tmp.z, tmp.w);
}

/*******************************************************************************
*                                                                              *
*                                                                              *
*                                                                              *
*******************************************************************************/

#if !defined(__LP64__)

static __forceinline__ __device__ void tex1DLod(long *retVal, cudaTextureObject_t texObject, float x, float level)
{
   int4 tmp;
   asm volatile ("tex.level.1d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5}], %6;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(level));
   *retVal = (long)(tmp.x);
}

static __forceinline__ __device__ void tex1DLod(long1 *retVal, cudaTextureObject_t texObject, float x, float level)
{
   int4 tmp;
   asm volatile ("tex.level.1d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5}], %6;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(level));
   *retVal = make_long1(tmp.x);
}

static __forceinline__ __device__ void tex1DLod(long2 *retVal, cudaTextureObject_t texObject, float x, float level)
{
   int4 tmp;
   asm volatile ("tex.level.1d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5}], %6;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(level));
   *retVal = make_long2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex1DLod(long4 *retVal, cudaTextureObject_t texObject, float x, float level)
{
   int4 tmp;
   asm volatile ("tex.level.1d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5}], %6;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(level));
   *retVal = make_long4(tmp.x, tmp.y, tmp.z, tmp.w);
}

static __forceinline__ __device__ void tex1DLod(unsigned long *retVal, cudaTextureObject_t texObject, float x, float level)
{
   uint4 tmp;
   asm volatile ("tex.level.1d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5}], %6;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(level));
   *retVal = (unsigned long)(tmp.x);
}

static __forceinline__ __device__ void tex1DLod(ulong1 *retVal, cudaTextureObject_t texObject, float x, float level)
{
   uint4 tmp;
   asm volatile ("tex.level.1d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5}], %6;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(level));
   *retVal = make_ulong1(tmp.x);
}

static __forceinline__ __device__ void tex1DLod(ulong2 *retVal, cudaTextureObject_t texObject, float x, float level)
{
   uint4 tmp;
   asm volatile ("tex.level.1d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5}], %6;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(level));
   *retVal = make_ulong2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex1DLod(ulong4 *retVal, cudaTextureObject_t texObject, float x, float level)
{
   uint4 tmp;
   asm volatile ("tex.level.1d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5}], %6;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(level));
   *retVal = make_ulong4(tmp.x, tmp.y, tmp.z, tmp.w);
}

#endif /* !__LP64__ */


/*******************************************************************************
*                                                                              *
*                                                                              *
*                                                                              *
*******************************************************************************/

static __forceinline__ __device__ void tex1DLod(float *retVal, cudaTextureObject_t texObject, float x, float level)
{
   float4 tmp;
   asm volatile ("tex.level.1d.v4.f32.f32 {%0, %1, %2, %3}, [%4, {%5}], %6;" : "=f"(tmp.x), "=f"(tmp.y), "=f"(tmp.z), "=f"(tmp.w) : "l"(texObject), "f"(x), "f"(level));
   *retVal = (float)(tmp.x);
}

static __forceinline__ __device__ void tex1DLod(float1 *retVal, cudaTextureObject_t texObject, float x, float level)
{
   float4 tmp;
   asm volatile ("tex.level.1d.v4.f32.f32 {%0, %1, %2, %3}, [%4, {%5}], %6;" : "=f"(tmp.x), "=f"(tmp.y), "=f"(tmp.z), "=f"(tmp.w) : "l"(texObject), "f"(x), "f"(level));
   *retVal = make_float1(tmp.x);
}

static __forceinline__ __device__ void tex1DLod(float2 *retVal, cudaTextureObject_t texObject, float x, float level)
{
   float4 tmp;
   asm volatile ("tex.level.1d.v4.f32.f32 {%0, %1, %2, %3}, [%4, {%5}], %6;" : "=f"(tmp.x), "=f"(tmp.y), "=f"(tmp.z), "=f"(tmp.w) : "l"(texObject), "f"(x), "f"(level));
   *retVal = make_float2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex1DLod(float4 *retVal, cudaTextureObject_t texObject, float x, float level)
{
   float4 tmp;
   asm volatile ("tex.level.1d.v4.f32.f32 {%0, %1, %2, %3}, [%4, {%5}], %6;" : "=f"(tmp.x), "=f"(tmp.y), "=f"(tmp.z), "=f"(tmp.w) : "l"(texObject), "f"(x), "f"(level));
   *retVal = make_float4(tmp.x, tmp.y, tmp.z, tmp.w);
}

/*******************************************************************************
*                                                                              *
*                                                                              *
*                                                                              *
*******************************************************************************/

template <class T>
static __forceinline__ __device__ T tex1DLod(cudaTextureObject_t texObject, float x, float level)
{
   T ret;
   tex1DLod(&ret, texObject, x, level);
   return ret;
}

/*******************************************************************************
*                                                                              *
* 2D mipmapped texture indirect fetch functions
*                                                                              *
*******************************************************************************/

static __forceinline__ __device__ void tex2DLod(char *retVal, cudaTextureObject_t texObject, float x, float y, float level)
{
#if defined(_CHAR_UNSIGNED) || defined(__CHAR_UNSIGNED__)
    uint4 tmp;
    asm volatile ("tex.level.2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], %7;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(level));
#else /* _CHAR_UNSIGNED || __CHAR_UNSIGNED__ */
    int4 tmp;
    asm volatile ("tex.level.2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], %7;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(level));
#endif /* _CHAR_UNSIGNED || __CHAR_UNSIGNED__ */
    *retVal = (char)tmp.x;
}
static __forceinline__ __device__ void tex2DLod(signed char *retVal, cudaTextureObject_t texObject, float x, float y, float level)
{
   int4 tmp;
   asm volatile ("tex.level.2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], %7;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(level));
   *retVal = (signed char)(tmp.x);
}

static __forceinline__ __device__ void tex2DLod(char1 *retVal, cudaTextureObject_t texObject, float x, float y, float level)
{
   int4 tmp;
   asm volatile ("tex.level.2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], %7;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(level));
   *retVal = make_char1(tmp.x);
}

static __forceinline__ __device__ void tex2DLod(char2 *retVal, cudaTextureObject_t texObject, float x, float y, float level)
{
   int4 tmp;
   asm volatile ("tex.level.2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], %7;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(level));
   *retVal = make_char2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex2DLod(char4 *retVal, cudaTextureObject_t texObject, float x, float y, float level)
{
   int4 tmp;
   asm volatile ("tex.level.2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], %7;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(level));
   *retVal = make_char4(tmp.x, tmp.y, tmp.z, tmp.w);
}

static __forceinline__ __device__ void tex2DLod(unsigned char *retVal, cudaTextureObject_t texObject, float x, float y, float level)
{
   uint4 tmp;
   asm volatile ("tex.level.2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], %7;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(level));
   *retVal = (unsigned char)(tmp.x);
}

static __forceinline__ __device__ void tex2DLod(uchar1 *retVal, cudaTextureObject_t texObject, float x, float y, float level)
{
   uint4 tmp;
   asm volatile ("tex.level.2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], %7;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(level));
   *retVal = make_uchar1(tmp.x);
}

static __forceinline__ __device__ void tex2DLod(uchar2 *retVal, cudaTextureObject_t texObject, float x, float y, float level)
{
   uint4 tmp;
   asm volatile ("tex.level.2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], %7;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(level));
   *retVal = make_uchar2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex2DLod(uchar4 *retVal, cudaTextureObject_t texObject, float x, float y, float level)
{
   uint4 tmp;
   asm volatile ("tex.level.2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], %7;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(level));
   *retVal = make_uchar4(tmp.x, tmp.y, tmp.z, tmp.w);
}

/*******************************************************************************
*                                                                              *
*                                                                              *
*                                                                              *
*******************************************************************************/

static __forceinline__ __device__ void tex2DLod(short *retVal, cudaTextureObject_t texObject, float x, float y, float level)
{
   int4 tmp;
   asm volatile ("tex.level.2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], %7;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(level));
   *retVal = (short)(tmp.x);
}

static __forceinline__ __device__ void tex2DLod(short1 *retVal, cudaTextureObject_t texObject, float x, float y, float level)
{
   int4 tmp;
   asm volatile ("tex.level.2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], %7;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(level));
   *retVal = make_short1(tmp.x);
}

static __forceinline__ __device__ void tex2DLod(short2 *retVal, cudaTextureObject_t texObject, float x, float y, float level)
{
   int4 tmp;
   asm volatile ("tex.level.2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], %7;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(level));
   *retVal = make_short2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex2DLod(short4 *retVal, cudaTextureObject_t texObject, float x, float y, float level)
{
   int4 tmp;
   asm volatile ("tex.level.2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], %7;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(level));
   *retVal = make_short4(tmp.x, tmp.y, tmp.z, tmp.w);
}

static __forceinline__ __device__ void tex2DLod(unsigned short *retVal, cudaTextureObject_t texObject, float x, float y, float level)
{
   uint4 tmp;
   asm volatile ("tex.level.2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], %7;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(level));
   *retVal = (unsigned short)(tmp.x);
}

static __forceinline__ __device__ void tex2DLod(ushort1 *retVal, cudaTextureObject_t texObject, float x, float y, float level)
{
   uint4 tmp;
   asm volatile ("tex.level.2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], %7;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(level));
   *retVal = make_ushort1(tmp.x);
}

static __forceinline__ __device__ void tex2DLod(ushort2 *retVal, cudaTextureObject_t texObject, float x, float y, float level)
{
   uint4 tmp;
   asm volatile ("tex.level.2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], %7;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(level));
   *retVal = make_ushort2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex2DLod(ushort4 *retVal, cudaTextureObject_t texObject, float x, float y, float level)
{
   uint4 tmp;
   asm volatile ("tex.level.2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], %7;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(level));
   *retVal = make_ushort4(tmp.x, tmp.y, tmp.z, tmp.w);
}

/*******************************************************************************
*                                                                              *
*                                                                              *
*                                                                              *
*******************************************************************************/

static __forceinline__ __device__ void tex2DLod(int *retVal, cudaTextureObject_t texObject, float x, float y, float level)
{
   int4 tmp;
   asm volatile ("tex.level.2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], %7;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(level));
   *retVal = (int)(tmp.x);
}

static __forceinline__ __device__ void tex2DLod(int1 *retVal, cudaTextureObject_t texObject, float x, float y, float level)
{
   int4 tmp;
   asm volatile ("tex.level.2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], %7;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(level));
   *retVal = make_int1(tmp.x);
}

static __forceinline__ __device__ void tex2DLod(int2 *retVal, cudaTextureObject_t texObject, float x, float y, float level)
{
   int4 tmp;
   asm volatile ("tex.level.2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], %7;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(level));
   *retVal = make_int2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex2DLod(int4 *retVal, cudaTextureObject_t texObject, float x, float y, float level)
{
   int4 tmp;
   asm volatile ("tex.level.2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], %7;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(level));
   *retVal = make_int4(tmp.x, tmp.y, tmp.z, tmp.w);
}

static __forceinline__ __device__ void tex2DLod(unsigned int *retVal, cudaTextureObject_t texObject, float x, float y, float level)
{
   uint4 tmp;
   asm volatile ("tex.level.2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], %7;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(level));
   *retVal = (unsigned int)(tmp.x);
}

static __forceinline__ __device__ void tex2DLod(uint1 *retVal, cudaTextureObject_t texObject, float x, float y, float level)
{
   uint4 tmp;
   asm volatile ("tex.level.2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], %7;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(level));
   *retVal = make_uint1(tmp.x);
}

static __forceinline__ __device__ void tex2DLod(uint2 *retVal, cudaTextureObject_t texObject, float x, float y, float level)
{
   uint4 tmp;
   asm volatile ("tex.level.2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], %7;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(level));
   *retVal = make_uint2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex2DLod(uint4 *retVal, cudaTextureObject_t texObject, float x, float y, float level)
{
   uint4 tmp;
   asm volatile ("tex.level.2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], %7;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(level));
   *retVal = make_uint4(tmp.x, tmp.y, tmp.z, tmp.w);
}

/*******************************************************************************
*                                                                              *
*                                                                              *
*                                                                              *
*******************************************************************************/

#if !defined(__LP64__)

static __forceinline__ __device__ void tex2DLod(long *retVal, cudaTextureObject_t texObject, float x, float y, float level)
{
   int4 tmp;
   asm volatile ("tex.level.2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], %7;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(level));
   *retVal = (long)(tmp.x);
}

static __forceinline__ __device__ void tex2DLod(long1 *retVal, cudaTextureObject_t texObject, float x, float y, float level)
{
   int4 tmp;
   asm volatile ("tex.level.2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], %7;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(level));
   *retVal = make_long1(tmp.x);
}

static __forceinline__ __device__ void tex2DLod(long2 *retVal, cudaTextureObject_t texObject, float x, float y, float level)
{
   int4 tmp;
   asm volatile ("tex.level.2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], %7;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(level));
   *retVal = make_long2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex2DLod(long4 *retVal, cudaTextureObject_t texObject, float x, float y, float level)
{
   int4 tmp;
   asm volatile ("tex.level.2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], %7;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(level));
   *retVal = make_long4(tmp.x, tmp.y, tmp.z, tmp.w);
}

static __forceinline__ __device__ void tex2DLod(unsigned long *retVal, cudaTextureObject_t texObject, float x, float y, float level)
{
   uint4 tmp;
   asm volatile ("tex.level.2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], %7;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(level));
   *retVal = (unsigned long)(tmp.x);
}

static __forceinline__ __device__ void tex2DLod(ulong1 *retVal, cudaTextureObject_t texObject, float x, float y, float level)
{
   uint4 tmp;
   asm volatile ("tex.level.2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], %7;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(level));
   *retVal = make_ulong1(tmp.x);
}

static __forceinline__ __device__ void tex2DLod(ulong2 *retVal, cudaTextureObject_t texObject, float x, float y, float level)
{
   uint4 tmp;
   asm volatile ("tex.level.2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], %7;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(level));
   *retVal = make_ulong2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex2DLod(ulong4 *retVal, cudaTextureObject_t texObject, float x, float y, float level)
{
   uint4 tmp;
   asm volatile ("tex.level.2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], %7;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(level));
   *retVal = make_ulong4(tmp.x, tmp.y, tmp.z, tmp.w);
}

#endif /* !__LP64__ */


/*******************************************************************************
*                                                                              *
*                                                                              *
*                                                                              *
*******************************************************************************/

static __forceinline__ __device__ void tex2DLod(float *retVal, cudaTextureObject_t texObject, float x, float y, float level)
{
   float4 tmp;
   asm volatile ("tex.level.2d.v4.f32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], %7;" : "=f"(tmp.x), "=f"(tmp.y), "=f"(tmp.z), "=f"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(level));
   *retVal = (float)(tmp.x);
}

static __forceinline__ __device__ void tex2DLod(float1 *retVal, cudaTextureObject_t texObject, float x, float y, float level)
{
   float4 tmp;
   asm volatile ("tex.level.2d.v4.f32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], %7;" : "=f"(tmp.x), "=f"(tmp.y), "=f"(tmp.z), "=f"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(level));
   *retVal = make_float1(tmp.x);
}

static __forceinline__ __device__ void tex2DLod(float2 *retVal, cudaTextureObject_t texObject, float x, float y, float level)
{
   float4 tmp;
   asm volatile ("tex.level.2d.v4.f32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], %7;" : "=f"(tmp.x), "=f"(tmp.y), "=f"(tmp.z), "=f"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(level));
   *retVal = make_float2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex2DLod(float4 *retVal, cudaTextureObject_t texObject, float x, float y, float level)
{
   float4 tmp;
   asm volatile ("tex.level.2d.v4.f32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], %7;" : "=f"(tmp.x), "=f"(tmp.y), "=f"(tmp.z), "=f"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(level));
   *retVal = make_float4(tmp.x, tmp.y, tmp.z, tmp.w);
}

/*******************************************************************************
*                                                                              *
*                                                                              *
*                                                                              *
*******************************************************************************/

template <class T>
static __forceinline__ __device__ T tex2DLod(cudaTextureObject_t texObject, float x, float y, float level)
{
   T ret;
   tex2DLod(&ret, texObject, x, y, level);
   return ret;
}

/*******************************************************************************
*                                                                              *
* 3D mipmapped texture indirect fetch functions
*                                                                              *
*******************************************************************************/

static __forceinline__ __device__ void tex3DLod(char *retVal, cudaTextureObject_t texObject, float x, float y, float z, float level)
{
#if defined(_CHAR_UNSIGNED) || defined(__CHAR_UNSIGNED__)
    uint4 tmp;
    asm volatile ("tex.level.3d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], %8;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z), "f"(level));
#else /* _CHAR_UNSIGNED || __CHAR_UNSIGNED__ */
    int4 tmp;
    asm volatile ("tex.level.3d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], %8;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z), "f"(level));
#endif /* _CHAR_UNSIGNED || __CHAR_UNSIGNED__ */
    *retVal = (char)tmp.x;
}
static __forceinline__ __device__ void tex3DLod(signed char *retVal, cudaTextureObject_t texObject, float x, float y, float z, float level)
{
   int4 tmp;
   asm volatile ("tex.level.3d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], %8;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z), "f"(level));
   *retVal = (signed char)(tmp.x);
}

static __forceinline__ __device__ void tex3DLod(char1 *retVal, cudaTextureObject_t texObject, float x, float y, float z, float level)
{
   int4 tmp;
   asm volatile ("tex.level.3d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], %8;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z), "f"(level));
   *retVal = make_char1(tmp.x);
}

static __forceinline__ __device__ void tex3DLod(char2 *retVal, cudaTextureObject_t texObject, float x, float y, float z, float level)
{
   int4 tmp;
   asm volatile ("tex.level.3d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], %8;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z), "f"(level));
   *retVal = make_char2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex3DLod(char4 *retVal, cudaTextureObject_t texObject, float x, float y, float z, float level)
{
   int4 tmp;
   asm volatile ("tex.level.3d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], %8;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z), "f"(level));
   *retVal = make_char4(tmp.x, tmp.y, tmp.z, tmp.w);
}

static __forceinline__ __device__ void tex3DLod(unsigned char *retVal, cudaTextureObject_t texObject, float x, float y, float z, float level)
{
   uint4 tmp;
   asm volatile ("tex.level.3d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], %8;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z), "f"(level));
   *retVal = (unsigned char)(tmp.x);
}

static __forceinline__ __device__ void tex3DLod(uchar1 *retVal, cudaTextureObject_t texObject, float x, float y, float z, float level)
{
   uint4 tmp;
   asm volatile ("tex.level.3d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], %8;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z), "f"(level));
   *retVal = make_uchar1(tmp.x);
}

static __forceinline__ __device__ void tex3DLod(uchar2 *retVal, cudaTextureObject_t texObject, float x, float y, float z, float level)
{
   uint4 tmp;
   asm volatile ("tex.level.3d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], %8;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z), "f"(level));
   *retVal = make_uchar2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex3DLod(uchar4 *retVal, cudaTextureObject_t texObject, float x, float y, float z, float level)
{
   uint4 tmp;
   asm volatile ("tex.level.3d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], %8;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z), "f"(level));
   *retVal = make_uchar4(tmp.x, tmp.y, tmp.z, tmp.w);
}

/*******************************************************************************
*                                                                              *
*                                                                              *
*                                                                              *
*******************************************************************************/

static __forceinline__ __device__ void tex3DLod(short *retVal, cudaTextureObject_t texObject, float x, float y, float z, float level)
{
   int4 tmp;
   asm volatile ("tex.level.3d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], %8;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z), "f"(level));
   *retVal = (short)(tmp.x);
}

static __forceinline__ __device__ void tex3DLod(short1 *retVal, cudaTextureObject_t texObject, float x, float y, float z, float level)
{
   int4 tmp;
   asm volatile ("tex.level.3d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], %8;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z), "f"(level));
   *retVal = make_short1(tmp.x);
}

static __forceinline__ __device__ void tex3DLod(short2 *retVal, cudaTextureObject_t texObject, float x, float y, float z, float level)
{
   int4 tmp;
   asm volatile ("tex.level.3d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], %8;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z), "f"(level));
   *retVal = make_short2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex3DLod(short4 *retVal, cudaTextureObject_t texObject, float x, float y, float z, float level)
{
   int4 tmp;
   asm volatile ("tex.level.3d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], %8;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z), "f"(level));
   *retVal = make_short4(tmp.x, tmp.y, tmp.z, tmp.w);
}

static __forceinline__ __device__ void tex3DLod(unsigned short *retVal, cudaTextureObject_t texObject, float x, float y, float z, float level)
{
   uint4 tmp;
   asm volatile ("tex.level.3d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], %8;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z), "f"(level));
   *retVal = (unsigned short)(tmp.x);
}

static __forceinline__ __device__ void tex3DLod(ushort1 *retVal, cudaTextureObject_t texObject, float x, float y, float z, float level)
{
   uint4 tmp;
   asm volatile ("tex.level.3d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], %8;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z), "f"(level));
   *retVal = make_ushort1(tmp.x);
}

static __forceinline__ __device__ void tex3DLod(ushort2 *retVal, cudaTextureObject_t texObject, float x, float y, float z, float level)
{
   uint4 tmp;
   asm volatile ("tex.level.3d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], %8;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z), "f"(level));
   *retVal = make_ushort2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex3DLod(ushort4 *retVal, cudaTextureObject_t texObject, float x, float y, float z, float level)
{
   uint4 tmp;
   asm volatile ("tex.level.3d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], %8;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z), "f"(level));
   *retVal = make_ushort4(tmp.x, tmp.y, tmp.z, tmp.w);
}

/*******************************************************************************
*                                                                              *
*                                                                              *
*                                                                              *
*******************************************************************************/

static __forceinline__ __device__ void tex3DLod(int *retVal, cudaTextureObject_t texObject, float x, float y, float z, float level)
{
   int4 tmp;
   asm volatile ("tex.level.3d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], %8;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z), "f"(level));
   *retVal = (int)(tmp.x);
}

static __forceinline__ __device__ void tex3DLod(int1 *retVal, cudaTextureObject_t texObject, float x, float y, float z, float level)
{
   int4 tmp;
   asm volatile ("tex.level.3d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], %8;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z), "f"(level));
   *retVal = make_int1(tmp.x);
}

static __forceinline__ __device__ void tex3DLod(int2 *retVal, cudaTextureObject_t texObject, float x, float y, float z, float level)
{
   int4 tmp;
   asm volatile ("tex.level.3d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], %8;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z), "f"(level));
   *retVal = make_int2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex3DLod(int4 *retVal, cudaTextureObject_t texObject, float x, float y, float z, float level)
{
   int4 tmp;
   asm volatile ("tex.level.3d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], %8;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z), "f"(level));
   *retVal = make_int4(tmp.x, tmp.y, tmp.z, tmp.w);
}

static __forceinline__ __device__ void tex3DLod(unsigned int *retVal, cudaTextureObject_t texObject, float x, float y, float z, float level)
{
   uint4 tmp;
   asm volatile ("tex.level.3d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], %8;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z), "f"(level));
   *retVal = (unsigned int)(tmp.x);
}

static __forceinline__ __device__ void tex3DLod(uint1 *retVal, cudaTextureObject_t texObject, float x, float y, float z, float level)
{
   uint4 tmp;
   asm volatile ("tex.level.3d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], %8;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z), "f"(level));
   *retVal = make_uint1(tmp.x);
}

static __forceinline__ __device__ void tex3DLod(uint2 *retVal, cudaTextureObject_t texObject, float x, float y, float z, float level)
{
   uint4 tmp;
   asm volatile ("tex.level.3d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], %8;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z), "f"(level));
   *retVal = make_uint2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex3DLod(uint4 *retVal, cudaTextureObject_t texObject, float x, float y, float z, float level)
{
   uint4 tmp;
   asm volatile ("tex.level.3d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], %8;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z), "f"(level));
   *retVal = make_uint4(tmp.x, tmp.y, tmp.z, tmp.w);
}

/*******************************************************************************
*                                                                              *
*                                                                              *
*                                                                              *
*******************************************************************************/

#if !defined(__LP64__)

static __forceinline__ __device__ void tex3DLod(long *retVal, cudaTextureObject_t texObject, float x, float y, float z, float level)
{
   int4 tmp;
   asm volatile ("tex.level.3d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], %8;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z), "f"(level));
   *retVal = (long)(tmp.x);
}

static __forceinline__ __device__ void tex3DLod(long1 *retVal, cudaTextureObject_t texObject, float x, float y, float z, float level)
{
   int4 tmp;
   asm volatile ("tex.level.3d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], %8;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z), "f"(level));
   *retVal = make_long1(tmp.x);
}

static __forceinline__ __device__ void tex3DLod(long2 *retVal, cudaTextureObject_t texObject, float x, float y, float z, float level)
{
   int4 tmp;
   asm volatile ("tex.level.3d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], %8;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z), "f"(level));
   *retVal = make_long2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex3DLod(long4 *retVal, cudaTextureObject_t texObject, float x, float y, float z, float level)
{
   int4 tmp;
   asm volatile ("tex.level.3d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], %8;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z), "f"(level));
   *retVal = make_long4(tmp.x, tmp.y, tmp.z, tmp.w);
}

static __forceinline__ __device__ void tex3DLod(unsigned long *retVal, cudaTextureObject_t texObject, float x, float y, float z, float level)
{
   uint4 tmp;
   asm volatile ("tex.level.3d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], %8;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z), "f"(level));
   *retVal = (unsigned long)(tmp.x);
}

static __forceinline__ __device__ void tex3DLod(ulong1 *retVal, cudaTextureObject_t texObject, float x, float y, float z, float level)
{
   uint4 tmp;
   asm volatile ("tex.level.3d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], %8;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z), "f"(level));
   *retVal = make_ulong1(tmp.x);
}

static __forceinline__ __device__ void tex3DLod(ulong2 *retVal, cudaTextureObject_t texObject, float x, float y, float z, float level)
{
   uint4 tmp;
   asm volatile ("tex.level.3d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], %8;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z), "f"(level));
   *retVal = make_ulong2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex3DLod(ulong4 *retVal, cudaTextureObject_t texObject, float x, float y, float z, float level)
{
   uint4 tmp;
   asm volatile ("tex.level.3d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], %8;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z), "f"(level));
   *retVal = make_ulong4(tmp.x, tmp.y, tmp.z, tmp.w);
}

#endif /* !__LP64__ */


/*******************************************************************************
*                                                                              *
*                                                                              *
*                                                                              *
*******************************************************************************/

static __forceinline__ __device__ void tex3DLod(float *retVal, cudaTextureObject_t texObject, float x, float y, float z, float level)
{
   float4 tmp;
   asm volatile ("tex.level.3d.v4.f32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], %8;" : "=f"(tmp.x), "=f"(tmp.y), "=f"(tmp.z), "=f"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z), "f"(level));
   *retVal = (float)(tmp.x);
}

static __forceinline__ __device__ void tex3DLod(float1 *retVal, cudaTextureObject_t texObject, float x, float y, float z, float level)
{
   float4 tmp;
   asm volatile ("tex.level.3d.v4.f32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], %8;" : "=f"(tmp.x), "=f"(tmp.y), "=f"(tmp.z), "=f"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z), "f"(level));
   *retVal = make_float1(tmp.x);
}

static __forceinline__ __device__ void tex3DLod(float2 *retVal, cudaTextureObject_t texObject, float x, float y, float z, float level)
{
   float4 tmp;
   asm volatile ("tex.level.3d.v4.f32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], %8;" : "=f"(tmp.x), "=f"(tmp.y), "=f"(tmp.z), "=f"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z), "f"(level));
   *retVal = make_float2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex3DLod(float4 *retVal, cudaTextureObject_t texObject, float x, float y, float z, float level)
{
   float4 tmp;
   asm volatile ("tex.level.3d.v4.f32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], %8;" : "=f"(tmp.x), "=f"(tmp.y), "=f"(tmp.z), "=f"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z), "f"(level));
   *retVal = make_float4(tmp.x, tmp.y, tmp.z, tmp.w);
}

/*******************************************************************************
*                                                                              *
*                                                                              *
*                                                                              *
*******************************************************************************/

template <class T>
static __forceinline__ __device__ T tex3DLod(cudaTextureObject_t texObject, float x, float y, float z, float level)
{
   T ret;
   tex3DLod(&ret, texObject, x, y, z, level);
   return ret;
}

/*******************************************************************************
*                                                                              *
* 1D Layered mipmapped texture indirect fetch functions
*                                                                              *
*******************************************************************************/

static __forceinline__ __device__ void tex1DLayeredLod(char *retVal, cudaTextureObject_t texObject, float x, int layer, float level)
{
#if defined(_CHAR_UNSIGNED) || defined(__CHAR_UNSIGNED__)
    uint4 tmp;
    asm volatile ("tex.level.a1d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], %7;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(level));
#else /* _CHAR_UNSIGNED || __CHAR_UNSIGNED__ */
    int4 tmp;
    asm volatile ("tex.level.a1d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], %7;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(level));
#endif /* _CHAR_UNSIGNED || __CHAR_UNSIGNED__ */
    *retVal = (char)tmp.x;
}
static __forceinline__ __device__ void tex1DLayeredLod(signed char *retVal, cudaTextureObject_t texObject, float x, int layer, float level)
{
   int4 tmp;
   asm volatile ("tex.level.a1d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], %7;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(level));
   *retVal = (signed char)(tmp.x);
}

static __forceinline__ __device__ void tex1DLayeredLod(char1 *retVal, cudaTextureObject_t texObject, float x, int layer, float level)
{
   int4 tmp;
   asm volatile ("tex.level.a1d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], %7;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(level));
   *retVal = make_char1(tmp.x);
}

static __forceinline__ __device__ void tex1DLayeredLod(char2 *retVal, cudaTextureObject_t texObject, float x, int layer, float level)
{
   int4 tmp;
   asm volatile ("tex.level.a1d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], %7;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(level));
   *retVal = make_char2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex1DLayeredLod(char4 *retVal, cudaTextureObject_t texObject, float x, int layer, float level)
{
   int4 tmp;
   asm volatile ("tex.level.a1d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], %7;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(level));
   *retVal = make_char4(tmp.x, tmp.y, tmp.z, tmp.w);
}

static __forceinline__ __device__ void tex1DLayeredLod(unsigned char *retVal, cudaTextureObject_t texObject, float x, int layer, float level)
{
   uint4 tmp;
   asm volatile ("tex.level.a1d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], %7;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(level));
   *retVal = (unsigned char)(tmp.x);
}

static __forceinline__ __device__ void tex1DLayeredLod(uchar1 *retVal, cudaTextureObject_t texObject, float x, int layer, float level)
{
   uint4 tmp;
   asm volatile ("tex.level.a1d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], %7;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(level));
   *retVal = make_uchar1(tmp.x);
}

static __forceinline__ __device__ void tex1DLayeredLod(uchar2 *retVal, cudaTextureObject_t texObject, float x, int layer, float level)
{
   uint4 tmp;
   asm volatile ("tex.level.a1d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], %7;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(level));
   *retVal = make_uchar2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex1DLayeredLod(uchar4 *retVal, cudaTextureObject_t texObject, float x, int layer, float level)
{
   uint4 tmp;
   asm volatile ("tex.level.a1d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], %7;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(level));
   *retVal = make_uchar4(tmp.x, tmp.y, tmp.z, tmp.w);
}

/*******************************************************************************
*                                                                              *
*                                                                              *
*                                                                              *
*******************************************************************************/

static __forceinline__ __device__ void tex1DLayeredLod(short *retVal, cudaTextureObject_t texObject, float x, int layer, float level)
{
   int4 tmp;
   asm volatile ("tex.level.a1d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], %7;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(level));
   *retVal = (short)(tmp.x);
}

static __forceinline__ __device__ void tex1DLayeredLod(short1 *retVal, cudaTextureObject_t texObject, float x, int layer, float level)
{
   int4 tmp;
   asm volatile ("tex.level.a1d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], %7;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(level));
   *retVal = make_short1(tmp.x);
}

static __forceinline__ __device__ void tex1DLayeredLod(short2 *retVal, cudaTextureObject_t texObject, float x, int layer, float level)
{
   int4 tmp;
   asm volatile ("tex.level.a1d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], %7;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(level));
   *retVal = make_short2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex1DLayeredLod(short4 *retVal, cudaTextureObject_t texObject, float x, int layer, float level)
{
   int4 tmp;
   asm volatile ("tex.level.a1d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], %7;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(level));
   *retVal = make_short4(tmp.x, tmp.y, tmp.z, tmp.w);
}

static __forceinline__ __device__ void tex1DLayeredLod(unsigned short *retVal, cudaTextureObject_t texObject, float x, int layer, float level)
{
   uint4 tmp;
   asm volatile ("tex.level.a1d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], %7;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(level));
   *retVal = (unsigned short)(tmp.x);
}

static __forceinline__ __device__ void tex1DLayeredLod(ushort1 *retVal, cudaTextureObject_t texObject, float x, int layer, float level)
{
   uint4 tmp;
   asm volatile ("tex.level.a1d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], %7;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(level));
   *retVal = make_ushort1(tmp.x);
}

static __forceinline__ __device__ void tex1DLayeredLod(ushort2 *retVal, cudaTextureObject_t texObject, float x, int layer, float level)
{
   uint4 tmp;
   asm volatile ("tex.level.a1d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], %7;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(level));
   *retVal = make_ushort2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex1DLayeredLod(ushort4 *retVal, cudaTextureObject_t texObject, float x, int layer, float level)
{
   uint4 tmp;
   asm volatile ("tex.level.a1d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], %7;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(level));
   *retVal = make_ushort4(tmp.x, tmp.y, tmp.z, tmp.w);
}

/*******************************************************************************
*                                                                              *
*                                                                              *
*                                                                              *
*******************************************************************************/

static __forceinline__ __device__ void tex1DLayeredLod(int *retVal, cudaTextureObject_t texObject, float x, int layer, float level)
{
   int4 tmp;
   asm volatile ("tex.level.a1d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], %7;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(level));
   *retVal = (int)(tmp.x);
}

static __forceinline__ __device__ void tex1DLayeredLod(int1 *retVal, cudaTextureObject_t texObject, float x, int layer, float level)
{
   int4 tmp;
   asm volatile ("tex.level.a1d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], %7;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(level));
   *retVal = make_int1(tmp.x);
}

static __forceinline__ __device__ void tex1DLayeredLod(int2 *retVal, cudaTextureObject_t texObject, float x, int layer, float level)
{
   int4 tmp;
   asm volatile ("tex.level.a1d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], %7;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(level));
   *retVal = make_int2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex1DLayeredLod(int4 *retVal, cudaTextureObject_t texObject, float x, int layer, float level)
{
   int4 tmp;
   asm volatile ("tex.level.a1d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], %7;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(level));
   *retVal = make_int4(tmp.x, tmp.y, tmp.z, tmp.w);
}

static __forceinline__ __device__ void tex1DLayeredLod(unsigned int *retVal, cudaTextureObject_t texObject, float x, int layer, float level)
{
   uint4 tmp;
   asm volatile ("tex.level.a1d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], %7;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(level));
   *retVal = (unsigned int)(tmp.x);
}

static __forceinline__ __device__ void tex1DLayeredLod(uint1 *retVal, cudaTextureObject_t texObject, float x, int layer, float level)
{
   uint4 tmp;
   asm volatile ("tex.level.a1d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], %7;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(level));
   *retVal = make_uint1(tmp.x);
}

static __forceinline__ __device__ void tex1DLayeredLod(uint2 *retVal, cudaTextureObject_t texObject, float x, int layer, float level)
{
   uint4 tmp;
   asm volatile ("tex.level.a1d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], %7;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(level));
   *retVal = make_uint2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex1DLayeredLod(uint4 *retVal, cudaTextureObject_t texObject, float x, int layer, float level)
{
   uint4 tmp;
   asm volatile ("tex.level.a1d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], %7;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(level));
   *retVal = make_uint4(tmp.x, tmp.y, tmp.z, tmp.w);
}

/*******************************************************************************
*                                                                              *
*                                                                              *
*                                                                              *
*******************************************************************************/

#if !defined(__LP64__)

static __forceinline__ __device__ void tex1DLayeredLod(long *retVal, cudaTextureObject_t texObject, float x, int layer, float level)
{
   int4 tmp;
   asm volatile ("tex.level.a1d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], %7;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(level));
   *retVal = (long)(tmp.x);
}

static __forceinline__ __device__ void tex1DLayeredLod(long1 *retVal, cudaTextureObject_t texObject, float x, int layer, float level)
{
   int4 tmp;
   asm volatile ("tex.level.a1d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], %7;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(level));
   *retVal = make_long1(tmp.x);
}

static __forceinline__ __device__ void tex1DLayeredLod(long2 *retVal, cudaTextureObject_t texObject, float x, int layer, float level)
{
   int4 tmp;
   asm volatile ("tex.level.a1d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], %7;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(level));
   *retVal = make_long2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex1DLayeredLod(long4 *retVal, cudaTextureObject_t texObject, float x, int layer, float level)
{
   int4 tmp;
   asm volatile ("tex.level.a1d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], %7;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(level));
   *retVal = make_long4(tmp.x, tmp.y, tmp.z, tmp.w);
}

static __forceinline__ __device__ void tex1DLayeredLod(unsigned long *retVal, cudaTextureObject_t texObject, float x, int layer, float level)
{
   uint4 tmp;
   asm volatile ("tex.level.a1d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], %7;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(level));
   *retVal = (unsigned long)(tmp.x);
}

static __forceinline__ __device__ void tex1DLayeredLod(ulong1 *retVal, cudaTextureObject_t texObject, float x, int layer, float level)
{
   uint4 tmp;
   asm volatile ("tex.level.a1d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], %7;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(level));
   *retVal = make_ulong1(tmp.x);
}

static __forceinline__ __device__ void tex1DLayeredLod(ulong2 *retVal, cudaTextureObject_t texObject, float x, int layer, float level)
{
   uint4 tmp;
   asm volatile ("tex.level.a1d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], %7;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(level));
   *retVal = make_ulong2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex1DLayeredLod(ulong4 *retVal, cudaTextureObject_t texObject, float x, int layer, float level)
{
   uint4 tmp;
   asm volatile ("tex.level.a1d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], %7;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(level));
   *retVal = make_ulong4(tmp.x, tmp.y, tmp.z, tmp.w);
}

#endif /* !__LP64__ */


/*******************************************************************************
*                                                                              *
*                                                                              *
*                                                                              *
*******************************************************************************/

static __forceinline__ __device__ void tex1DLayeredLod(float *retVal, cudaTextureObject_t texObject, float x, int layer, float level)
{
   float4 tmp;
   asm volatile ("tex.level.a1d.v4.f32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], %7;" : "=f"(tmp.x), "=f"(tmp.y), "=f"(tmp.z), "=f"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(level));
   *retVal = (float)(tmp.x);
}

static __forceinline__ __device__ void tex1DLayeredLod(float1 *retVal, cudaTextureObject_t texObject, float x, int layer, float level)
{
   float4 tmp;
   asm volatile ("tex.level.a1d.v4.f32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], %7;" : "=f"(tmp.x), "=f"(tmp.y), "=f"(tmp.z), "=f"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(level));
   *retVal = make_float1(tmp.x);
}

static __forceinline__ __device__ void tex1DLayeredLod(float2 *retVal, cudaTextureObject_t texObject, float x, int layer, float level)
{
   float4 tmp;
   asm volatile ("tex.level.a1d.v4.f32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], %7;" : "=f"(tmp.x), "=f"(tmp.y), "=f"(tmp.z), "=f"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(level));
   *retVal = make_float2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex1DLayeredLod(float4 *retVal, cudaTextureObject_t texObject, float x, int layer, float level)
{
   float4 tmp;
   asm volatile ("tex.level.a1d.v4.f32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], %7;" : "=f"(tmp.x), "=f"(tmp.y), "=f"(tmp.z), "=f"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(level));
   *retVal = make_float4(tmp.x, tmp.y, tmp.z, tmp.w);
}

/*******************************************************************************
*                                                                              *
*                                                                              *
*                                                                              *
*******************************************************************************/

template <class T>
static __forceinline__ __device__ T tex1DLayeredLod(cudaTextureObject_t texObject, float x, int layer, float level)
{
   T ret;
   tex1DLayeredLod(&ret, texObject, x, layer, level);
   return ret;
}

/*******************************************************************************
*                                                                              *
* 2D Layered mipmapped texture indirect fetch functions
*                                                                              *
*******************************************************************************/

static __forceinline__ __device__ void tex2DLayeredLod(char *retVal, cudaTextureObject_t texObject, float x, float y, int layer, float level)
{
#if defined(_CHAR_UNSIGNED) || defined(__CHAR_UNSIGNED__)
    uint4 tmp;
    asm volatile ("tex.level.a2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], %8;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(level));
#else /* _CHAR_UNSIGNED || __CHAR_UNSIGNED__ */
    int4 tmp;
    asm volatile ("tex.level.a2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], %8;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(level));
#endif /* _CHAR_UNSIGNED || __CHAR_UNSIGNED__ */
    *retVal = (char)tmp.x;
}
static __forceinline__ __device__ void tex2DLayeredLod(signed char *retVal, cudaTextureObject_t texObject, float x, float y, int layer, float level)
{
   int4 tmp;
   asm volatile ("tex.level.a2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], %8;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(level));
   *retVal = (signed char)(tmp.x);
}

static __forceinline__ __device__ void tex2DLayeredLod(char1 *retVal, cudaTextureObject_t texObject, float x, float y, int layer, float level)
{
   int4 tmp;
   asm volatile ("tex.level.a2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], %8;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(level));
   *retVal = make_char1(tmp.x);
}

static __forceinline__ __device__ void tex2DLayeredLod(char2 *retVal, cudaTextureObject_t texObject, float x, float y, int layer, float level)
{
   int4 tmp;
   asm volatile ("tex.level.a2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], %8;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(level));
   *retVal = make_char2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex2DLayeredLod(char4 *retVal, cudaTextureObject_t texObject, float x, float y, int layer, float level)
{
   int4 tmp;
   asm volatile ("tex.level.a2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], %8;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(level));
   *retVal = make_char4(tmp.x, tmp.y, tmp.z, tmp.w);
}

static __forceinline__ __device__ void tex2DLayeredLod(unsigned char *retVal, cudaTextureObject_t texObject, float x, float y, int layer, float level)
{
   uint4 tmp;
   asm volatile ("tex.level.a2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], %8;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(level));
   *retVal = (unsigned char)(tmp.x);
}

static __forceinline__ __device__ void tex2DLayeredLod(uchar1 *retVal, cudaTextureObject_t texObject, float x, float y, int layer, float level)
{
   uint4 tmp;
   asm volatile ("tex.level.a2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], %8;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(level));
   *retVal = make_uchar1(tmp.x);
}

static __forceinline__ __device__ void tex2DLayeredLod(uchar2 *retVal, cudaTextureObject_t texObject, float x, float y, int layer, float level)
{
   uint4 tmp;
   asm volatile ("tex.level.a2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], %8;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(level));
   *retVal = make_uchar2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex2DLayeredLod(uchar4 *retVal, cudaTextureObject_t texObject, float x, float y, int layer, float level)
{
   uint4 tmp;
   asm volatile ("tex.level.a2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], %8;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(level));
   *retVal = make_uchar4(tmp.x, tmp.y, tmp.z, tmp.w);
}

/*******************************************************************************
*                                                                              *
*                                                                              *
*                                                                              *
*******************************************************************************/

static __forceinline__ __device__ void tex2DLayeredLod(short *retVal, cudaTextureObject_t texObject, float x, float y, int layer, float level)
{
   int4 tmp;
   asm volatile ("tex.level.a2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], %8;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(level));
   *retVal = (short)(tmp.x);
}

static __forceinline__ __device__ void tex2DLayeredLod(short1 *retVal, cudaTextureObject_t texObject, float x, float y, int layer, float level)
{
   int4 tmp;
   asm volatile ("tex.level.a2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], %8;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(level));
   *retVal = make_short1(tmp.x);
}

static __forceinline__ __device__ void tex2DLayeredLod(short2 *retVal, cudaTextureObject_t texObject, float x, float y, int layer, float level)
{
   int4 tmp;
   asm volatile ("tex.level.a2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], %8;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(level));
   *retVal = make_short2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex2DLayeredLod(short4 *retVal, cudaTextureObject_t texObject, float x, float y, int layer, float level)
{
   int4 tmp;
   asm volatile ("tex.level.a2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], %8;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(level));
   *retVal = make_short4(tmp.x, tmp.y, tmp.z, tmp.w);
}

static __forceinline__ __device__ void tex2DLayeredLod(unsigned short *retVal, cudaTextureObject_t texObject, float x, float y, int layer, float level)
{
   uint4 tmp;
   asm volatile ("tex.level.a2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], %8;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(level));
   *retVal = (unsigned short)(tmp.x);
}

static __forceinline__ __device__ void tex2DLayeredLod(ushort1 *retVal, cudaTextureObject_t texObject, float x, float y, int layer, float level)
{
   uint4 tmp;
   asm volatile ("tex.level.a2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], %8;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(level));
   *retVal = make_ushort1(tmp.x);
}

static __forceinline__ __device__ void tex2DLayeredLod(ushort2 *retVal, cudaTextureObject_t texObject, float x, float y, int layer, float level)
{
   uint4 tmp;
   asm volatile ("tex.level.a2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], %8;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(level));
   *retVal = make_ushort2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex2DLayeredLod(ushort4 *retVal, cudaTextureObject_t texObject, float x, float y, int layer, float level)
{
   uint4 tmp;
   asm volatile ("tex.level.a2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], %8;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(level));
   *retVal = make_ushort4(tmp.x, tmp.y, tmp.z, tmp.w);
}

/*******************************************************************************
*                                                                              *
*                                                                              *
*                                                                              *
*******************************************************************************/

static __forceinline__ __device__ void tex2DLayeredLod(int *retVal, cudaTextureObject_t texObject, float x, float y, int layer, float level)
{
   int4 tmp;
   asm volatile ("tex.level.a2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], %8;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(level));
   *retVal = (int)(tmp.x);
}

static __forceinline__ __device__ void tex2DLayeredLod(int1 *retVal, cudaTextureObject_t texObject, float x, float y, int layer, float level)
{
   int4 tmp;
   asm volatile ("tex.level.a2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], %8;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(level));
   *retVal = make_int1(tmp.x);
}

static __forceinline__ __device__ void tex2DLayeredLod(int2 *retVal, cudaTextureObject_t texObject, float x, float y, int layer, float level)
{
   int4 tmp;
   asm volatile ("tex.level.a2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], %8;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(level));
   *retVal = make_int2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex2DLayeredLod(int4 *retVal, cudaTextureObject_t texObject, float x, float y, int layer, float level)
{
   int4 tmp;
   asm volatile ("tex.level.a2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], %8;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(level));
   *retVal = make_int4(tmp.x, tmp.y, tmp.z, tmp.w);
}

static __forceinline__ __device__ void tex2DLayeredLod(unsigned int *retVal, cudaTextureObject_t texObject, float x, float y, int layer, float level)
{
   uint4 tmp;
   asm volatile ("tex.level.a2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], %8;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(level));
   *retVal = (unsigned int)(tmp.x);
}

static __forceinline__ __device__ void tex2DLayeredLod(uint1 *retVal, cudaTextureObject_t texObject, float x, float y, int layer, float level)
{
   uint4 tmp;
   asm volatile ("tex.level.a2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], %8;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(level));
   *retVal = make_uint1(tmp.x);
}

static __forceinline__ __device__ void tex2DLayeredLod(uint2 *retVal, cudaTextureObject_t texObject, float x, float y, int layer, float level)
{
   uint4 tmp;
   asm volatile ("tex.level.a2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], %8;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(level));
   *retVal = make_uint2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex2DLayeredLod(uint4 *retVal, cudaTextureObject_t texObject, float x, float y, int layer, float level)
{
   uint4 tmp;
   asm volatile ("tex.level.a2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], %8;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(level));
   *retVal = make_uint4(tmp.x, tmp.y, tmp.z, tmp.w);
}

/*******************************************************************************
*                                                                              *
*                                                                              *
*                                                                              *
*******************************************************************************/

#if !defined(__LP64__)

static __forceinline__ __device__ void tex2DLayeredLod(long *retVal, cudaTextureObject_t texObject, float x, float y, int layer, float level)
{
   int4 tmp;
   asm volatile ("tex.level.a2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], %8;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(level));
   *retVal = (long)(tmp.x);
}

static __forceinline__ __device__ void tex2DLayeredLod(long1 *retVal, cudaTextureObject_t texObject, float x, float y, int layer, float level)
{
   int4 tmp;
   asm volatile ("tex.level.a2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], %8;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(level));
   *retVal = make_long1(tmp.x);
}

static __forceinline__ __device__ void tex2DLayeredLod(long2 *retVal, cudaTextureObject_t texObject, float x, float y, int layer, float level)
{
   int4 tmp;
   asm volatile ("tex.level.a2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], %8;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(level));
   *retVal = make_long2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex2DLayeredLod(long4 *retVal, cudaTextureObject_t texObject, float x, float y, int layer, float level)
{
   int4 tmp;
   asm volatile ("tex.level.a2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], %8;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(level));
   *retVal = make_long4(tmp.x, tmp.y, tmp.z, tmp.w);
}

static __forceinline__ __device__ void tex2DLayeredLod(unsigned long *retVal, cudaTextureObject_t texObject, float x, float y, int layer, float level)
{
   uint4 tmp;
   asm volatile ("tex.level.a2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], %8;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(level));
   *retVal = (unsigned long)(tmp.x);
}

static __forceinline__ __device__ void tex2DLayeredLod(ulong1 *retVal, cudaTextureObject_t texObject, float x, float y, int layer, float level)
{
   uint4 tmp;
   asm volatile ("tex.level.a2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], %8;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(level));
   *retVal = make_ulong1(tmp.x);
}

static __forceinline__ __device__ void tex2DLayeredLod(ulong2 *retVal, cudaTextureObject_t texObject, float x, float y, int layer, float level)
{
   uint4 tmp;
   asm volatile ("tex.level.a2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], %8;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(level));
   *retVal = make_ulong2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex2DLayeredLod(ulong4 *retVal, cudaTextureObject_t texObject, float x, float y, int layer, float level)
{
   uint4 tmp;
   asm volatile ("tex.level.a2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], %8;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(level));
   *retVal = make_ulong4(tmp.x, tmp.y, tmp.z, tmp.w);
}

#endif /* !__LP64__ */


/*******************************************************************************
*                                                                              *
*                                                                              *
*                                                                              *
*******************************************************************************/

static __forceinline__ __device__ void tex2DLayeredLod(float *retVal, cudaTextureObject_t texObject, float x, float y, int layer, float level)
{
   float4 tmp;
   asm volatile ("tex.level.a2d.v4.f32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], %8;" : "=f"(tmp.x), "=f"(tmp.y), "=f"(tmp.z), "=f"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(level));
   *retVal = (float)(tmp.x);
}

static __forceinline__ __device__ void tex2DLayeredLod(float1 *retVal, cudaTextureObject_t texObject, float x, float y, int layer, float level)
{
   float4 tmp;
   asm volatile ("tex.level.a2d.v4.f32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], %8;" : "=f"(tmp.x), "=f"(tmp.y), "=f"(tmp.z), "=f"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(level));
   *retVal = make_float1(tmp.x);
}

static __forceinline__ __device__ void tex2DLayeredLod(float2 *retVal, cudaTextureObject_t texObject, float x, float y, int layer, float level)
{
   float4 tmp;
   asm volatile ("tex.level.a2d.v4.f32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], %8;" : "=f"(tmp.x), "=f"(tmp.y), "=f"(tmp.z), "=f"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(level));
   *retVal = make_float2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex2DLayeredLod(float4 *retVal, cudaTextureObject_t texObject, float x, float y, int layer, float level)
{
   float4 tmp;
   asm volatile ("tex.level.a2d.v4.f32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], %8;" : "=f"(tmp.x), "=f"(tmp.y), "=f"(tmp.z), "=f"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(level));
   *retVal = make_float4(tmp.x, tmp.y, tmp.z, tmp.w);
}

/*******************************************************************************
*                                                                              *
*                                                                              *
*                                                                              *
*******************************************************************************/

template <class T>
static __forceinline__ __device__ T tex2DLayeredLod(cudaTextureObject_t texObject, float x, float y, int layer, float level)
{
   T ret;
   tex2DLayeredLod(&ret, texObject, x, y, layer, level);
   return ret;
}

/*******************************************************************************
*                                                                              *
* Cubemap mipmapped texture indirect fetch functions
*                                                                              *
*******************************************************************************/

static __forceinline__ __device__ void texCubemapLod(char *retVal, cudaTextureObject_t texObject, float x, float y, float z, float level)
{
#if defined(_CHAR_UNSIGNED) || defined(__CHAR_UNSIGNED__)
    uint4 tmp;
    asm volatile ("tex.level.cube.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], %8;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z), "f"(level));
#else /* _CHAR_UNSIGNED || __CHAR_UNSIGNED__ */
    int4 tmp;
    asm volatile ("tex.level.cube.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], %8;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z), "f"(level));
#endif /* _CHAR_UNSIGNED || __CHAR_UNSIGNED__ */
    *retVal = (char)tmp.x;
}
static __forceinline__ __device__ void texCubemapLod(signed char *retVal, cudaTextureObject_t texObject, float x, float y, float z, float level)
{
   int4 tmp;
   asm volatile ("tex.level.cube.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], %8;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z), "f"(level));
   *retVal = (signed char)(tmp.x);
}

static __forceinline__ __device__ void texCubemapLod(char1 *retVal, cudaTextureObject_t texObject, float x, float y, float z, float level)
{
   int4 tmp;
   asm volatile ("tex.level.cube.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], %8;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z), "f"(level));
   *retVal = make_char1(tmp.x);
}

static __forceinline__ __device__ void texCubemapLod(char2 *retVal, cudaTextureObject_t texObject, float x, float y, float z, float level)
{
   int4 tmp;
   asm volatile ("tex.level.cube.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], %8;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z), "f"(level));
   *retVal = make_char2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void texCubemapLod(char4 *retVal, cudaTextureObject_t texObject, float x, float y, float z, float level)
{
   int4 tmp;
   asm volatile ("tex.level.cube.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], %8;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z), "f"(level));
   *retVal = make_char4(tmp.x, tmp.y, tmp.z, tmp.w);
}

static __forceinline__ __device__ void texCubemapLod(unsigned char *retVal, cudaTextureObject_t texObject, float x, float y, float z, float level)
{
   uint4 tmp;
   asm volatile ("tex.level.cube.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], %8;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z), "f"(level));
   *retVal = (unsigned char)(tmp.x);
}

static __forceinline__ __device__ void texCubemapLod(uchar1 *retVal, cudaTextureObject_t texObject, float x, float y, float z, float level)
{
   uint4 tmp;
   asm volatile ("tex.level.cube.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], %8;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z), "f"(level));
   *retVal = make_uchar1(tmp.x);
}

static __forceinline__ __device__ void texCubemapLod(uchar2 *retVal, cudaTextureObject_t texObject, float x, float y, float z, float level)
{
   uint4 tmp;
   asm volatile ("tex.level.cube.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], %8;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z), "f"(level));
   *retVal = make_uchar2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void texCubemapLod(uchar4 *retVal, cudaTextureObject_t texObject, float x, float y, float z, float level)
{
   uint4 tmp;
   asm volatile ("tex.level.cube.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], %8;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z), "f"(level));
   *retVal = make_uchar4(tmp.x, tmp.y, tmp.z, tmp.w);
}

/*******************************************************************************
*                                                                              *
*                                                                              *
*                                                                              *
*******************************************************************************/

static __forceinline__ __device__ void texCubemapLod(short *retVal, cudaTextureObject_t texObject, float x, float y, float z, float level)
{
   int4 tmp;
   asm volatile ("tex.level.cube.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], %8;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z), "f"(level));
   *retVal = (short)(tmp.x);
}

static __forceinline__ __device__ void texCubemapLod(short1 *retVal, cudaTextureObject_t texObject, float x, float y, float z, float level)
{
   int4 tmp;
   asm volatile ("tex.level.cube.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], %8;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z), "f"(level));
   *retVal = make_short1(tmp.x);
}

static __forceinline__ __device__ void texCubemapLod(short2 *retVal, cudaTextureObject_t texObject, float x, float y, float z, float level)
{
   int4 tmp;
   asm volatile ("tex.level.cube.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], %8;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z), "f"(level));
   *retVal = make_short2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void texCubemapLod(short4 *retVal, cudaTextureObject_t texObject, float x, float y, float z, float level)
{
   int4 tmp;
   asm volatile ("tex.level.cube.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], %8;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z), "f"(level));
   *retVal = make_short4(tmp.x, tmp.y, tmp.z, tmp.w);
}

static __forceinline__ __device__ void texCubemapLod(unsigned short *retVal, cudaTextureObject_t texObject, float x, float y, float z, float level)
{
   uint4 tmp;
   asm volatile ("tex.level.cube.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], %8;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z), "f"(level));
   *retVal = (unsigned short)(tmp.x);
}

static __forceinline__ __device__ void texCubemapLod(ushort1 *retVal, cudaTextureObject_t texObject, float x, float y, float z, float level)
{
   uint4 tmp;
   asm volatile ("tex.level.cube.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], %8;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z), "f"(level));
   *retVal = make_ushort1(tmp.x);
}

static __forceinline__ __device__ void texCubemapLod(ushort2 *retVal, cudaTextureObject_t texObject, float x, float y, float z, float level)
{
   uint4 tmp;
   asm volatile ("tex.level.cube.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], %8;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z), "f"(level));
   *retVal = make_ushort2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void texCubemapLod(ushort4 *retVal, cudaTextureObject_t texObject, float x, float y, float z, float level)
{
   uint4 tmp;
   asm volatile ("tex.level.cube.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], %8;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z), "f"(level));
   *retVal = make_ushort4(tmp.x, tmp.y, tmp.z, tmp.w);
}

/*******************************************************************************
*                                                                              *
*                                                                              *
*                                                                              *
*******************************************************************************/

static __forceinline__ __device__ void texCubemapLod(int *retVal, cudaTextureObject_t texObject, float x, float y, float z, float level)
{
   int4 tmp;
   asm volatile ("tex.level.cube.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], %8;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z), "f"(level));
   *retVal = (int)(tmp.x);
}

static __forceinline__ __device__ void texCubemapLod(int1 *retVal, cudaTextureObject_t texObject, float x, float y, float z, float level)
{
   int4 tmp;
   asm volatile ("tex.level.cube.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], %8;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z), "f"(level));
   *retVal = make_int1(tmp.x);
}

static __forceinline__ __device__ void texCubemapLod(int2 *retVal, cudaTextureObject_t texObject, float x, float y, float z, float level)
{
   int4 tmp;
   asm volatile ("tex.level.cube.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], %8;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z), "f"(level));
   *retVal = make_int2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void texCubemapLod(int4 *retVal, cudaTextureObject_t texObject, float x, float y, float z, float level)
{
   int4 tmp;
   asm volatile ("tex.level.cube.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], %8;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z), "f"(level));
   *retVal = make_int4(tmp.x, tmp.y, tmp.z, tmp.w);
}

static __forceinline__ __device__ void texCubemapLod(unsigned int *retVal, cudaTextureObject_t texObject, float x, float y, float z, float level)
{
   uint4 tmp;
   asm volatile ("tex.level.cube.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], %8;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z), "f"(level));
   *retVal = (unsigned int)(tmp.x);
}

static __forceinline__ __device__ void texCubemapLod(uint1 *retVal, cudaTextureObject_t texObject, float x, float y, float z, float level)
{
   uint4 tmp;
   asm volatile ("tex.level.cube.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], %8;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z), "f"(level));
   *retVal = make_uint1(tmp.x);
}

static __forceinline__ __device__ void texCubemapLod(uint2 *retVal, cudaTextureObject_t texObject, float x, float y, float z, float level)
{
   uint4 tmp;
   asm volatile ("tex.level.cube.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], %8;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z), "f"(level));
   *retVal = make_uint2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void texCubemapLod(uint4 *retVal, cudaTextureObject_t texObject, float x, float y, float z, float level)
{
   uint4 tmp;
   asm volatile ("tex.level.cube.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], %8;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z), "f"(level));
   *retVal = make_uint4(tmp.x, tmp.y, tmp.z, tmp.w);
}

/*******************************************************************************
*                                                                              *
*                                                                              *
*                                                                              *
*******************************************************************************/

#if !defined(__LP64__)

static __forceinline__ __device__ void texCubemapLod(long *retVal, cudaTextureObject_t texObject, float x, float y, float z, float level)
{
   int4 tmp;
   asm volatile ("tex.level.cube.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], %8;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z), "f"(level));
   *retVal = (long)(tmp.x);
}

static __forceinline__ __device__ void texCubemapLod(long1 *retVal, cudaTextureObject_t texObject, float x, float y, float z, float level)
{
   int4 tmp;
   asm volatile ("tex.level.cube.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], %8;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z), "f"(level));
   *retVal = make_long1(tmp.x);
}

static __forceinline__ __device__ void texCubemapLod(long2 *retVal, cudaTextureObject_t texObject, float x, float y, float z, float level)
{
   int4 tmp;
   asm volatile ("tex.level.cube.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], %8;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z), "f"(level));
   *retVal = make_long2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void texCubemapLod(long4 *retVal, cudaTextureObject_t texObject, float x, float y, float z, float level)
{
   int4 tmp;
   asm volatile ("tex.level.cube.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], %8;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z), "f"(level));
   *retVal = make_long4(tmp.x, tmp.y, tmp.z, tmp.w);
}

static __forceinline__ __device__ void texCubemapLod(unsigned long *retVal, cudaTextureObject_t texObject, float x, float y, float z, float level)
{
   uint4 tmp;
   asm volatile ("tex.level.cube.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], %8;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z), "f"(level));
   *retVal = (unsigned long)(tmp.x);
}

static __forceinline__ __device__ void texCubemapLod(ulong1 *retVal, cudaTextureObject_t texObject, float x, float y, float z, float level)
{
   uint4 tmp;
   asm volatile ("tex.level.cube.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], %8;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z), "f"(level));
   *retVal = make_ulong1(tmp.x);
}

static __forceinline__ __device__ void texCubemapLod(ulong2 *retVal, cudaTextureObject_t texObject, float x, float y, float z, float level)
{
   uint4 tmp;
   asm volatile ("tex.level.cube.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], %8;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z), "f"(level));
   *retVal = make_ulong2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void texCubemapLod(ulong4 *retVal, cudaTextureObject_t texObject, float x, float y, float z, float level)
{
   uint4 tmp;
   asm volatile ("tex.level.cube.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], %8;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z), "f"(level));
   *retVal = make_ulong4(tmp.x, tmp.y, tmp.z, tmp.w);
}

#endif /* !__LP64__ */


/*******************************************************************************
*                                                                              *
*                                                                              *
*                                                                              *
*******************************************************************************/

static __forceinline__ __device__ void texCubemapLod(float *retVal, cudaTextureObject_t texObject, float x, float y, float z, float level)
{
   float4 tmp;
   asm volatile ("tex.level.cube.v4.f32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], %8;" : "=f"(tmp.x), "=f"(tmp.y), "=f"(tmp.z), "=f"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z), "f"(level));
   *retVal = (float)(tmp.x);
}

static __forceinline__ __device__ void texCubemapLod(float1 *retVal, cudaTextureObject_t texObject, float x, float y, float z, float level)
{
   float4 tmp;
   asm volatile ("tex.level.cube.v4.f32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], %8;" : "=f"(tmp.x), "=f"(tmp.y), "=f"(tmp.z), "=f"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z), "f"(level));
   *retVal = make_float1(tmp.x);
}

static __forceinline__ __device__ void texCubemapLod(float2 *retVal, cudaTextureObject_t texObject, float x, float y, float z, float level)
{
   float4 tmp;
   asm volatile ("tex.level.cube.v4.f32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], %8;" : "=f"(tmp.x), "=f"(tmp.y), "=f"(tmp.z), "=f"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z), "f"(level));
   *retVal = make_float2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void texCubemapLod(float4 *retVal, cudaTextureObject_t texObject, float x, float y, float z, float level)
{
   float4 tmp;
   asm volatile ("tex.level.cube.v4.f32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], %8;" : "=f"(tmp.x), "=f"(tmp.y), "=f"(tmp.z), "=f"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z), "f"(level));
   *retVal = make_float4(tmp.x, tmp.y, tmp.z, tmp.w);
}

/*******************************************************************************
*                                                                              *
*                                                                              *
*                                                                              *
*******************************************************************************/

template <class T>
static __forceinline__ __device__ T texCubemapLod(cudaTextureObject_t texObject, float x, float y, float z, float level)
{
   T ret;
   texCubemapLod(&ret, texObject, x, y, z, level);
   return ret;
}

/*******************************************************************************
*                                                                              *
* Cubemap Layered mipmapped texture indirect fetch functions
*                                                                              *
*******************************************************************************/

static __forceinline__ __device__ void texCubemapLayeredLod(char *retVal, cudaTextureObject_t texObject, float x, float y, float z, int layer, float level)
{
#if defined(_CHAR_UNSIGNED) || defined(__CHAR_UNSIGNED__)
    uint4 tmp;
    asm volatile ("tex.level.acube.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %8}], %9;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(z), "f"(level));
#else /* _CHAR_UNSIGNED || __CHAR_UNSIGNED__ */
    int4 tmp;
    asm volatile ("tex.level.acube.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %8}], %9;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(z), "f"(level));
#endif /* _CHAR_UNSIGNED || __CHAR_UNSIGNED__ */
    *retVal = (char)tmp.x;
}
static __forceinline__ __device__ void texCubemapLayeredLod(signed char *retVal, cudaTextureObject_t texObject, float x, float y, float z, int layer, float level)
{
   int4 tmp;
   asm volatile ("tex.level.acube.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %8}], %9;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(z), "f"(level));
   *retVal = (signed char)(tmp.x);
}

static __forceinline__ __device__ void texCubemapLayeredLod(char1 *retVal, cudaTextureObject_t texObject, float x, float y, float z, int layer, float level)
{
   int4 tmp;
   asm volatile ("tex.level.acube.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %8}], %9;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(z), "f"(level));
   *retVal = make_char1(tmp.x);
}

static __forceinline__ __device__ void texCubemapLayeredLod(char2 *retVal, cudaTextureObject_t texObject, float x, float y, float z, int layer, float level)
{
   int4 tmp;
   asm volatile ("tex.level.acube.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %8}], %9;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(z), "f"(level));
   *retVal = make_char2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void texCubemapLayeredLod(char4 *retVal, cudaTextureObject_t texObject, float x, float y, float z, int layer, float level)
{
   int4 tmp;
   asm volatile ("tex.level.acube.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %8}], %9;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(z), "f"(level));
   *retVal = make_char4(tmp.x, tmp.y, tmp.z, tmp.w);
}

static __forceinline__ __device__ void texCubemapLayeredLod(unsigned char *retVal, cudaTextureObject_t texObject, float x, float y, float z, int layer, float level)
{
   uint4 tmp;
   asm volatile ("tex.level.acube.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %8}], %9;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(z), "f"(level));
   *retVal = (unsigned char)(tmp.x);
}

static __forceinline__ __device__ void texCubemapLayeredLod(uchar1 *retVal, cudaTextureObject_t texObject, float x, float y, float z, int layer, float level)
{
   uint4 tmp;
   asm volatile ("tex.level.acube.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %8}], %9;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(z), "f"(level));
   *retVal = make_uchar1(tmp.x);
}

static __forceinline__ __device__ void texCubemapLayeredLod(uchar2 *retVal, cudaTextureObject_t texObject, float x, float y, float z, int layer, float level)
{
   uint4 tmp;
   asm volatile ("tex.level.acube.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %8}], %9;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(z), "f"(level));
   *retVal = make_uchar2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void texCubemapLayeredLod(uchar4 *retVal, cudaTextureObject_t texObject, float x, float y, float z, int layer, float level)
{
   uint4 tmp;
   asm volatile ("tex.level.acube.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %8}], %9;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(z), "f"(level));
   *retVal = make_uchar4(tmp.x, tmp.y, tmp.z, tmp.w);
}

/*******************************************************************************
*                                                                              *
*                                                                              *
*                                                                              *
*******************************************************************************/

static __forceinline__ __device__ void texCubemapLayeredLod(short *retVal, cudaTextureObject_t texObject, float x, float y, float z, int layer, float level)
{
   int4 tmp;
   asm volatile ("tex.level.acube.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %8}], %9;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(z), "f"(level));
   *retVal = (short)(tmp.x);
}

static __forceinline__ __device__ void texCubemapLayeredLod(short1 *retVal, cudaTextureObject_t texObject, float x, float y, float z, int layer, float level)
{
   int4 tmp;
   asm volatile ("tex.level.acube.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %8}], %9;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(z), "f"(level));
   *retVal = make_short1(tmp.x);
}

static __forceinline__ __device__ void texCubemapLayeredLod(short2 *retVal, cudaTextureObject_t texObject, float x, float y, float z, int layer, float level)
{
   int4 tmp;
   asm volatile ("tex.level.acube.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %8}], %9;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(z), "f"(level));
   *retVal = make_short2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void texCubemapLayeredLod(short4 *retVal, cudaTextureObject_t texObject, float x, float y, float z, int layer, float level)
{
   int4 tmp;
   asm volatile ("tex.level.acube.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %8}], %9;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(z), "f"(level));
   *retVal = make_short4(tmp.x, tmp.y, tmp.z, tmp.w);
}

static __forceinline__ __device__ void texCubemapLayeredLod(unsigned short *retVal, cudaTextureObject_t texObject, float x, float y, float z, int layer, float level)
{
   uint4 tmp;
   asm volatile ("tex.level.acube.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %8}], %9;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(z), "f"(level));
   *retVal = (unsigned short)(tmp.x);
}

static __forceinline__ __device__ void texCubemapLayeredLod(ushort1 *retVal, cudaTextureObject_t texObject, float x, float y, float z, int layer, float level)
{
   uint4 tmp;
   asm volatile ("tex.level.acube.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %8}], %9;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(z), "f"(level));
   *retVal = make_ushort1(tmp.x);
}

static __forceinline__ __device__ void texCubemapLayeredLod(ushort2 *retVal, cudaTextureObject_t texObject, float x, float y, float z, int layer, float level)
{
   uint4 tmp;
   asm volatile ("tex.level.acube.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %8}], %9;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(z), "f"(level));
   *retVal = make_ushort2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void texCubemapLayeredLod(ushort4 *retVal, cudaTextureObject_t texObject, float x, float y, float z, int layer, float level)
{
   uint4 tmp;
   asm volatile ("tex.level.acube.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %8}], %9;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(z), "f"(level));
   *retVal = make_ushort4(tmp.x, tmp.y, tmp.z, tmp.w);
}

/*******************************************************************************
*                                                                              *
*                                                                              *
*                                                                              *
*******************************************************************************/

static __forceinline__ __device__ void texCubemapLayeredLod(int *retVal, cudaTextureObject_t texObject, float x, float y, float z, int layer, float level)
{
   int4 tmp;
   asm volatile ("tex.level.acube.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %8}], %9;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(z), "f"(level));
   *retVal = (int)(tmp.x);
}

static __forceinline__ __device__ void texCubemapLayeredLod(int1 *retVal, cudaTextureObject_t texObject, float x, float y, float z, int layer, float level)
{
   int4 tmp;
   asm volatile ("tex.level.acube.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %8}], %9;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(z), "f"(level));
   *retVal = make_int1(tmp.x);
}

static __forceinline__ __device__ void texCubemapLayeredLod(int2 *retVal, cudaTextureObject_t texObject, float x, float y, float z, int layer, float level)
{
   int4 tmp;
   asm volatile ("tex.level.acube.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %8}], %9;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(z), "f"(level));
   *retVal = make_int2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void texCubemapLayeredLod(int4 *retVal, cudaTextureObject_t texObject, float x, float y, float z, int layer, float level)
{
   int4 tmp;
   asm volatile ("tex.level.acube.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %8}], %9;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(z), "f"(level));
   *retVal = make_int4(tmp.x, tmp.y, tmp.z, tmp.w);
}

static __forceinline__ __device__ void texCubemapLayeredLod(unsigned int *retVal, cudaTextureObject_t texObject, float x, float y, float z, int layer, float level)
{
   uint4 tmp;
   asm volatile ("tex.level.acube.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %8}], %9;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(z), "f"(level));
   *retVal = (unsigned int)(tmp.x);
}

static __forceinline__ __device__ void texCubemapLayeredLod(uint1 *retVal, cudaTextureObject_t texObject, float x, float y, float z, int layer, float level)
{
   uint4 tmp;
   asm volatile ("tex.level.acube.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %8}], %9;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(z), "f"(level));
   *retVal = make_uint1(tmp.x);
}

static __forceinline__ __device__ void texCubemapLayeredLod(uint2 *retVal, cudaTextureObject_t texObject, float x, float y, float z, int layer, float level)
{
   uint4 tmp;
   asm volatile ("tex.level.acube.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %8}], %9;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(z), "f"(level));
   *retVal = make_uint2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void texCubemapLayeredLod(uint4 *retVal, cudaTextureObject_t texObject, float x, float y, float z, int layer, float level)
{
   uint4 tmp;
   asm volatile ("tex.level.acube.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %8}], %9;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(z), "f"(level));
   *retVal = make_uint4(tmp.x, tmp.y, tmp.z, tmp.w);
}

/*******************************************************************************
*                                                                              *
*                                                                              *
*                                                                              *
*******************************************************************************/

#if !defined(__LP64__)

static __forceinline__ __device__ void texCubemapLayeredLod(long *retVal, cudaTextureObject_t texObject, float x, float y, float z, int layer, float level)
{
   int4 tmp;
   asm volatile ("tex.level.acube.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %8}], %9;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(z), "f"(level));
   *retVal = (long)(tmp.x);
}

static __forceinline__ __device__ void texCubemapLayeredLod(long1 *retVal, cudaTextureObject_t texObject, float x, float y, float z, int layer, float level)
{
   int4 tmp;
   asm volatile ("tex.level.acube.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %8}], %9;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(z), "f"(level));
   *retVal = make_long1(tmp.x);
}

static __forceinline__ __device__ void texCubemapLayeredLod(long2 *retVal, cudaTextureObject_t texObject, float x, float y, float z, int layer, float level)
{
   int4 tmp;
   asm volatile ("tex.level.acube.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %8}], %9;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(z), "f"(level));
   *retVal = make_long2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void texCubemapLayeredLod(long4 *retVal, cudaTextureObject_t texObject, float x, float y, float z, int layer, float level)
{
   int4 tmp;
   asm volatile ("tex.level.acube.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %8}], %9;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(z), "f"(level));
   *retVal = make_long4(tmp.x, tmp.y, tmp.z, tmp.w);
}

static __forceinline__ __device__ void texCubemapLayeredLod(unsigned long *retVal, cudaTextureObject_t texObject, float x, float y, float z, int layer, float level)
{
   uint4 tmp;
   asm volatile ("tex.level.acube.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %8}], %9;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(z), "f"(level));
   *retVal = (unsigned long)(tmp.x);
}

static __forceinline__ __device__ void texCubemapLayeredLod(ulong1 *retVal, cudaTextureObject_t texObject, float x, float y, float z, int layer, float level)
{
   uint4 tmp;
   asm volatile ("tex.level.acube.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %8}], %9;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(z), "f"(level));
   *retVal = make_ulong1(tmp.x);
}

static __forceinline__ __device__ void texCubemapLayeredLod(ulong2 *retVal, cudaTextureObject_t texObject, float x, float y, float z, int layer, float level)
{
   uint4 tmp;
   asm volatile ("tex.level.acube.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %8}], %9;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(z), "f"(level));
   *retVal = make_ulong2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void texCubemapLayeredLod(ulong4 *retVal, cudaTextureObject_t texObject, float x, float y, float z, int layer, float level)
{
   uint4 tmp;
   asm volatile ("tex.level.acube.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %8}], %9;" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(z), "f"(level));
   *retVal = make_ulong4(tmp.x, tmp.y, tmp.z, tmp.w);
}

#endif /* !__LP64__ */


/*******************************************************************************
*                                                                              *
*                                                                              *
*                                                                              *
*******************************************************************************/

static __forceinline__ __device__ void texCubemapLayeredLod(float *retVal, cudaTextureObject_t texObject, float x, float y, float z, int layer, float level)
{
   float4 tmp;
   asm volatile ("tex.level.acube.v4.f32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %8}], %9;" : "=f"(tmp.x), "=f"(tmp.y), "=f"(tmp.z), "=f"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(z), "f"(level));
   *retVal = (float)(tmp.x);
}

static __forceinline__ __device__ void texCubemapLayeredLod(float1 *retVal, cudaTextureObject_t texObject, float x, float y, float z, int layer, float level)
{
   float4 tmp;
   asm volatile ("tex.level.acube.v4.f32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %8}], %9;" : "=f"(tmp.x), "=f"(tmp.y), "=f"(tmp.z), "=f"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(z), "f"(level));
   *retVal = make_float1(tmp.x);
}

static __forceinline__ __device__ void texCubemapLayeredLod(float2 *retVal, cudaTextureObject_t texObject, float x, float y, float z, int layer, float level)
{
   float4 tmp;
   asm volatile ("tex.level.acube.v4.f32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %8}], %9;" : "=f"(tmp.x), "=f"(tmp.y), "=f"(tmp.z), "=f"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(z), "f"(level));
   *retVal = make_float2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void texCubemapLayeredLod(float4 *retVal, cudaTextureObject_t texObject, float x, float y, float z, int layer, float level)
{
   float4 tmp;
   asm volatile ("tex.level.acube.v4.f32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %8}], %9;" : "=f"(tmp.x), "=f"(tmp.y), "=f"(tmp.z), "=f"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(z), "f"(level));
   *retVal = make_float4(tmp.x, tmp.y, tmp.z, tmp.w);
}

/*******************************************************************************
*                                                                              *
*                                                                              *
*                                                                              *
*******************************************************************************/

template <class T>
static __forceinline__ __device__ T texCubemapLayeredLod(cudaTextureObject_t texObject, float x, float y, float z, int layer, float level)
{
   T ret;
   texCubemapLayeredLod(&ret, texObject, x, y, z, layer, level);
   return ret;
}

/*******************************************************************************
*                                                                              *
* 1D texture gradient indirect fetch functions
*                                                                              *
*******************************************************************************/

static __forceinline__ __device__ void tex1DGrad(char *retVal, cudaTextureObject_t texObject, float x, float dPdx, float dPdy)
{
#if defined(_CHAR_UNSIGNED) || defined(__CHAR_UNSIGNED__)
    uint4 tmp;
    asm volatile ("tex.grad.1d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5}], {%6}, {%7};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(dPdx), "f"(dPdy));
#else /* _CHAR_UNSIGNED || __CHAR_UNSIGNED__ */
    int4 tmp;
    asm volatile ("tex.grad.1d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5}], {%6}, {%7};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(dPdx), "f"(dPdy));
#endif /* _CHAR_UNSIGNED || __CHAR_UNSIGNED__ */
    *retVal = (char)tmp.x;
}
static __forceinline__ __device__ void tex1DGrad(signed char *retVal, cudaTextureObject_t texObject, float x, float dPdx, float dPdy)
{
   int4 tmp;
   asm volatile ("tex.grad.1d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5}], {%6}, {%7};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(dPdx), "f"(dPdy));
   *retVal = (signed char)(tmp.x);
}

static __forceinline__ __device__ void tex1DGrad(char1 *retVal, cudaTextureObject_t texObject, float x, float dPdx, float dPdy)
{
   int4 tmp;
   asm volatile ("tex.grad.1d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5}], {%6}, {%7};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(dPdx), "f"(dPdy));
   *retVal = make_char1(tmp.x);
}

static __forceinline__ __device__ void tex1DGrad(char2 *retVal, cudaTextureObject_t texObject, float x, float dPdx, float dPdy)
{
   int4 tmp;
   asm volatile ("tex.grad.1d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5}], {%6}, {%7};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(dPdx), "f"(dPdy));
   *retVal = make_char2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex1DGrad(char4 *retVal, cudaTextureObject_t texObject, float x, float dPdx, float dPdy)
{
   int4 tmp;
   asm volatile ("tex.grad.1d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5}], {%6}, {%7};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(dPdx), "f"(dPdy));
   *retVal = make_char4(tmp.x, tmp.y, tmp.z, tmp.w);
}

static __forceinline__ __device__ void tex1DGrad(unsigned char *retVal, cudaTextureObject_t texObject, float x, float dPdx, float dPdy)
{
   uint4 tmp;
   asm volatile ("tex.grad.1d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5}], {%6}, {%7};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(dPdx), "f"(dPdy));
   *retVal = (unsigned char)(tmp.x);
}

static __forceinline__ __device__ void tex1DGrad(uchar1 *retVal, cudaTextureObject_t texObject, float x, float dPdx, float dPdy)
{
   uint4 tmp;
   asm volatile ("tex.grad.1d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5}], {%6}, {%7};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(dPdx), "f"(dPdy));
   *retVal = make_uchar1(tmp.x);
}

static __forceinline__ __device__ void tex1DGrad(uchar2 *retVal, cudaTextureObject_t texObject, float x, float dPdx, float dPdy)
{
   uint4 tmp;
   asm volatile ("tex.grad.1d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5}], {%6}, {%7};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(dPdx), "f"(dPdy));
   *retVal = make_uchar2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex1DGrad(uchar4 *retVal, cudaTextureObject_t texObject, float x, float dPdx, float dPdy)
{
   uint4 tmp;
   asm volatile ("tex.grad.1d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5}], {%6}, {%7};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(dPdx), "f"(dPdy));
   *retVal = make_uchar4(tmp.x, tmp.y, tmp.z, tmp.w);
}

/*******************************************************************************
*                                                                              *
*                                                                              *
*                                                                              *
*******************************************************************************/

static __forceinline__ __device__ void tex1DGrad(short *retVal, cudaTextureObject_t texObject, float x, float dPdx, float dPdy)
{
   int4 tmp;
   asm volatile ("tex.grad.1d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5}], {%6}, {%7};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(dPdx), "f"(dPdy));
   *retVal = (short)(tmp.x);
}

static __forceinline__ __device__ void tex1DGrad(short1 *retVal, cudaTextureObject_t texObject, float x, float dPdx, float dPdy)
{
   int4 tmp;
   asm volatile ("tex.grad.1d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5}], {%6}, {%7};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(dPdx), "f"(dPdy));
   *retVal = make_short1(tmp.x);
}

static __forceinline__ __device__ void tex1DGrad(short2 *retVal, cudaTextureObject_t texObject, float x, float dPdx, float dPdy)
{
   int4 tmp;
   asm volatile ("tex.grad.1d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5}], {%6}, {%7};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(dPdx), "f"(dPdy));
   *retVal = make_short2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex1DGrad(short4 *retVal, cudaTextureObject_t texObject, float x, float dPdx, float dPdy)
{
   int4 tmp;
   asm volatile ("tex.grad.1d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5}], {%6}, {%7};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(dPdx), "f"(dPdy));
   *retVal = make_short4(tmp.x, tmp.y, tmp.z, tmp.w);
}

static __forceinline__ __device__ void tex1DGrad(unsigned short *retVal, cudaTextureObject_t texObject, float x, float dPdx, float dPdy)
{
   uint4 tmp;
   asm volatile ("tex.grad.1d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5}], {%6}, {%7};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(dPdx), "f"(dPdy));
   *retVal = (unsigned short)(tmp.x);
}

static __forceinline__ __device__ void tex1DGrad(ushort1 *retVal, cudaTextureObject_t texObject, float x, float dPdx, float dPdy)
{
   uint4 tmp;
   asm volatile ("tex.grad.1d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5}], {%6}, {%7};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(dPdx), "f"(dPdy));
   *retVal = make_ushort1(tmp.x);
}

static __forceinline__ __device__ void tex1DGrad(ushort2 *retVal, cudaTextureObject_t texObject, float x, float dPdx, float dPdy)
{
   uint4 tmp;
   asm volatile ("tex.grad.1d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5}], {%6}, {%7};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(dPdx), "f"(dPdy));
   *retVal = make_ushort2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex1DGrad(ushort4 *retVal, cudaTextureObject_t texObject, float x, float dPdx, float dPdy)
{
   uint4 tmp;
   asm volatile ("tex.grad.1d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5}], {%6}, {%7};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(dPdx), "f"(dPdy));
   *retVal = make_ushort4(tmp.x, tmp.y, tmp.z, tmp.w);
}

/*******************************************************************************
*                                                                              *
*                                                                              *
*                                                                              *
*******************************************************************************/

static __forceinline__ __device__ void tex1DGrad(int *retVal, cudaTextureObject_t texObject, float x, float dPdx, float dPdy)
{
   int4 tmp;
   asm volatile ("tex.grad.1d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5}], {%6}, {%7};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(dPdx), "f"(dPdy));
   *retVal = (int)(tmp.x);
}

static __forceinline__ __device__ void tex1DGrad(int1 *retVal, cudaTextureObject_t texObject, float x, float dPdx, float dPdy)
{
   int4 tmp;
   asm volatile ("tex.grad.1d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5}], {%6}, {%7};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(dPdx), "f"(dPdy));
   *retVal = make_int1(tmp.x);
}

static __forceinline__ __device__ void tex1DGrad(int2 *retVal, cudaTextureObject_t texObject, float x, float dPdx, float dPdy)
{
   int4 tmp;
   asm volatile ("tex.grad.1d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5}], {%6}, {%7};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(dPdx), "f"(dPdy));
   *retVal = make_int2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex1DGrad(int4 *retVal, cudaTextureObject_t texObject, float x, float dPdx, float dPdy)
{
   int4 tmp;
   asm volatile ("tex.grad.1d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5}], {%6}, {%7};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(dPdx), "f"(dPdy));
   *retVal = make_int4(tmp.x, tmp.y, tmp.z, tmp.w);
}

static __forceinline__ __device__ void tex1DGrad(unsigned int *retVal, cudaTextureObject_t texObject, float x, float dPdx, float dPdy)
{
   uint4 tmp;
   asm volatile ("tex.grad.1d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5}], {%6}, {%7};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(dPdx), "f"(dPdy));
   *retVal = (unsigned int)(tmp.x);
}

static __forceinline__ __device__ void tex1DGrad(uint1 *retVal, cudaTextureObject_t texObject, float x, float dPdx, float dPdy)
{
   uint4 tmp;
   asm volatile ("tex.grad.1d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5}], {%6}, {%7};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(dPdx), "f"(dPdy));
   *retVal = make_uint1(tmp.x);
}

static __forceinline__ __device__ void tex1DGrad(uint2 *retVal, cudaTextureObject_t texObject, float x, float dPdx, float dPdy)
{
   uint4 tmp;
   asm volatile ("tex.grad.1d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5}], {%6}, {%7};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(dPdx), "f"(dPdy));
   *retVal = make_uint2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex1DGrad(uint4 *retVal, cudaTextureObject_t texObject, float x, float dPdx, float dPdy)
{
   uint4 tmp;
   asm volatile ("tex.grad.1d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5}], {%6}, {%7};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(dPdx), "f"(dPdy));
   *retVal = make_uint4(tmp.x, tmp.y, tmp.z, tmp.w);
}

/*******************************************************************************
*                                                                              *
*                                                                              *
*                                                                              *
*******************************************************************************/

#if !defined(__LP64__)

static __forceinline__ __device__ void tex1DGrad(long *retVal, cudaTextureObject_t texObject, float x, float dPdx, float dPdy)
{
   int4 tmp;
   asm volatile ("tex.grad.1d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5}], {%6}, {%7};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(dPdx), "f"(dPdy));
   *retVal = (long)(tmp.x);
}

static __forceinline__ __device__ void tex1DGrad(long1 *retVal, cudaTextureObject_t texObject, float x, float dPdx, float dPdy)
{
   int4 tmp;
   asm volatile ("tex.grad.1d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5}], {%6}, {%7};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(dPdx), "f"(dPdy));
   *retVal = make_long1(tmp.x);
}

static __forceinline__ __device__ void tex1DGrad(long2 *retVal, cudaTextureObject_t texObject, float x, float dPdx, float dPdy)
{
   int4 tmp;
   asm volatile ("tex.grad.1d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5}], {%6}, {%7};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(dPdx), "f"(dPdy));
   *retVal = make_long2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex1DGrad(long4 *retVal, cudaTextureObject_t texObject, float x, float dPdx, float dPdy)
{
   int4 tmp;
   asm volatile ("tex.grad.1d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5}], {%6}, {%7};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(dPdx), "f"(dPdy));
   *retVal = make_long4(tmp.x, tmp.y, tmp.z, tmp.w);
}

static __forceinline__ __device__ void tex1DGrad(unsigned long *retVal, cudaTextureObject_t texObject, float x, float dPdx, float dPdy)
{
   uint4 tmp;
   asm volatile ("tex.grad.1d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5}], {%6}, {%7};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(dPdx), "f"(dPdy));
   *retVal = (unsigned long)(tmp.x);
}

static __forceinline__ __device__ void tex1DGrad(ulong1 *retVal, cudaTextureObject_t texObject, float x, float dPdx, float dPdy)
{
   uint4 tmp;
   asm volatile ("tex.grad.1d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5}], {%6}, {%7};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(dPdx), "f"(dPdy));
   *retVal = make_ulong1(tmp.x);
}

static __forceinline__ __device__ void tex1DGrad(ulong2 *retVal, cudaTextureObject_t texObject, float x, float dPdx, float dPdy)
{
   uint4 tmp;
   asm volatile ("tex.grad.1d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5}], {%6}, {%7};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(dPdx), "f"(dPdy));
   *retVal = make_ulong2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex1DGrad(ulong4 *retVal, cudaTextureObject_t texObject, float x, float dPdx, float dPdy)
{
   uint4 tmp;
   asm volatile ("tex.grad.1d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5}], {%6}, {%7};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(dPdx), "f"(dPdy));
   *retVal = make_ulong4(tmp.x, tmp.y, tmp.z, tmp.w);
}

#endif /* !__LP64__ */


/*******************************************************************************
*                                                                              *
*                                                                              *
*                                                                              *
*******************************************************************************/

static __forceinline__ __device__ void tex1DGrad(float *retVal, cudaTextureObject_t texObject, float x, float dPdx, float dPdy)
{
   float4 tmp;
   asm volatile ("tex.grad.1d.v4.f32.f32 {%0, %1, %2, %3}, [%4, {%5}], {%6}, {%7};" : "=f"(tmp.x), "=f"(tmp.y), "=f"(tmp.z), "=f"(tmp.w) : "l"(texObject), "f"(x), "f"(dPdx), "f"(dPdy));
   *retVal = (float)(tmp.x);
}

static __forceinline__ __device__ void tex1DGrad(float1 *retVal, cudaTextureObject_t texObject, float x, float dPdx, float dPdy)
{
   float4 tmp;
   asm volatile ("tex.grad.1d.v4.f32.f32 {%0, %1, %2, %3}, [%4, {%5}], {%6}, {%7};" : "=f"(tmp.x), "=f"(tmp.y), "=f"(tmp.z), "=f"(tmp.w) : "l"(texObject), "f"(x), "f"(dPdx), "f"(dPdy));
   *retVal = make_float1(tmp.x);
}

static __forceinline__ __device__ void tex1DGrad(float2 *retVal, cudaTextureObject_t texObject, float x, float dPdx, float dPdy)
{
   float4 tmp;
   asm volatile ("tex.grad.1d.v4.f32.f32 {%0, %1, %2, %3}, [%4, {%5}], {%6}, {%7};" : "=f"(tmp.x), "=f"(tmp.y), "=f"(tmp.z), "=f"(tmp.w) : "l"(texObject), "f"(x), "f"(dPdx), "f"(dPdy));
   *retVal = make_float2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex1DGrad(float4 *retVal, cudaTextureObject_t texObject, float x, float dPdx, float dPdy)
{
   float4 tmp;
   asm volatile ("tex.grad.1d.v4.f32.f32 {%0, %1, %2, %3}, [%4, {%5}], {%6}, {%7};" : "=f"(tmp.x), "=f"(tmp.y), "=f"(tmp.z), "=f"(tmp.w) : "l"(texObject), "f"(x), "f"(dPdx), "f"(dPdy));
   *retVal = make_float4(tmp.x, tmp.y, tmp.z, tmp.w);
}

/*******************************************************************************
*                                                                              *
*                                                                              *
*                                                                              *
*******************************************************************************/

template <class T>
static __forceinline__ __device__ T tex1DGrad(cudaTextureObject_t texObject, float x, float dPdx, float dPdy)
{
   T ret;
   tex1DGrad(&ret, texObject, x, dPdx, dPdy);
   return ret;
}

/*******************************************************************************
*                                                                              *
* 2D texture gradient indirect fetch functions
*                                                                              *
*******************************************************************************/

static __forceinline__ __device__ void tex2DGrad(char *retVal, cudaTextureObject_t texObject, float x, float y, float2 dPdx, float2 dPdy)
{
#if defined(_CHAR_UNSIGNED) || defined(__CHAR_UNSIGNED__)
    uint4 tmp;
    asm volatile ("tex.grad.2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], {%7, %8}, {%9, %10};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(dPdx.x), "f"(dPdx.y), "f"(dPdy.x), "f"(dPdy.y));
#else /* _CHAR_UNSIGNED || __CHAR_UNSIGNED__ */
    int4 tmp;
    asm volatile ("tex.grad.2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], {%7, %8}, {%9, %10};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(dPdx.x), "f"(dPdx.y), "f"(dPdy.x), "f"(dPdy.y));
#endif /* _CHAR_UNSIGNED || __CHAR_UNSIGNED__ */
    *retVal = (char)tmp.x;
}
static __forceinline__ __device__ void tex2DGrad(signed char *retVal, cudaTextureObject_t texObject, float x, float y, float2 dPdx, float2 dPdy)
{
   int4 tmp;
   asm volatile ("tex.grad.2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], {%7, %8}, {%9, %10};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(dPdx.x), "f"(dPdx.y), "f"(dPdy.x), "f"(dPdy.y));
   *retVal = (signed char)(tmp.x);
}

static __forceinline__ __device__ void tex2DGrad(char1 *retVal, cudaTextureObject_t texObject, float x, float y, float2 dPdx, float2 dPdy)
{
   int4 tmp;
   asm volatile ("tex.grad.2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], {%7, %8}, {%9, %10};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(dPdx.x), "f"(dPdx.y), "f"(dPdy.x), "f"(dPdy.y));
   *retVal = make_char1(tmp.x);
}

static __forceinline__ __device__ void tex2DGrad(char2 *retVal, cudaTextureObject_t texObject, float x, float y, float2 dPdx, float2 dPdy)
{
   int4 tmp;
   asm volatile ("tex.grad.2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], {%7, %8}, {%9, %10};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(dPdx.x), "f"(dPdx.y), "f"(dPdy.x), "f"(dPdy.y));
   *retVal = make_char2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex2DGrad(char4 *retVal, cudaTextureObject_t texObject, float x, float y, float2 dPdx, float2 dPdy)
{
   int4 tmp;
   asm volatile ("tex.grad.2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], {%7, %8}, {%9, %10};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(dPdx.x), "f"(dPdx.y), "f"(dPdy.x), "f"(dPdy.y));
   *retVal = make_char4(tmp.x, tmp.y, tmp.z, tmp.w);
}

static __forceinline__ __device__ void tex2DGrad(unsigned char *retVal, cudaTextureObject_t texObject, float x, float y, float2 dPdx, float2 dPdy)
{
   uint4 tmp;
   asm volatile ("tex.grad.2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], {%7, %8}, {%9, %10};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(dPdx.x), "f"(dPdx.y), "f"(dPdy.x), "f"(dPdy.y));
   *retVal = (unsigned char)(tmp.x);
}

static __forceinline__ __device__ void tex2DGrad(uchar1 *retVal, cudaTextureObject_t texObject, float x, float y, float2 dPdx, float2 dPdy)
{
   uint4 tmp;
   asm volatile ("tex.grad.2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], {%7, %8}, {%9, %10};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(dPdx.x), "f"(dPdx.y), "f"(dPdy.x), "f"(dPdy.y));
   *retVal = make_uchar1(tmp.x);
}

static __forceinline__ __device__ void tex2DGrad(uchar2 *retVal, cudaTextureObject_t texObject, float x, float y, float2 dPdx, float2 dPdy)
{
   uint4 tmp;
   asm volatile ("tex.grad.2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], {%7, %8}, {%9, %10};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(dPdx.x), "f"(dPdx.y), "f"(dPdy.x), "f"(dPdy.y));
   *retVal = make_uchar2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex2DGrad(uchar4 *retVal, cudaTextureObject_t texObject, float x, float y, float2 dPdx, float2 dPdy)
{
   uint4 tmp;
   asm volatile ("tex.grad.2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], {%7, %8}, {%9, %10};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(dPdx.x), "f"(dPdx.y), "f"(dPdy.x), "f"(dPdy.y));
   *retVal = make_uchar4(tmp.x, tmp.y, tmp.z, tmp.w);
}

/*******************************************************************************
*                                                                              *
*                                                                              *
*                                                                              *
*******************************************************************************/

static __forceinline__ __device__ void tex2DGrad(short *retVal, cudaTextureObject_t texObject, float x, float y, float2 dPdx, float2 dPdy)
{
   int4 tmp;
   asm volatile ("tex.grad.2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], {%7, %8}, {%9, %10};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(dPdx.x), "f"(dPdx.y), "f"(dPdy.x), "f"(dPdy.y));
   *retVal = (short)(tmp.x);
}

static __forceinline__ __device__ void tex2DGrad(short1 *retVal, cudaTextureObject_t texObject, float x, float y, float2 dPdx, float2 dPdy)
{
   int4 tmp;
   asm volatile ("tex.grad.2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], {%7, %8}, {%9, %10};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(dPdx.x), "f"(dPdx.y), "f"(dPdy.x), "f"(dPdy.y));
   *retVal = make_short1(tmp.x);
}

static __forceinline__ __device__ void tex2DGrad(short2 *retVal, cudaTextureObject_t texObject, float x, float y, float2 dPdx, float2 dPdy)
{
   int4 tmp;
   asm volatile ("tex.grad.2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], {%7, %8}, {%9, %10};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(dPdx.x), "f"(dPdx.y), "f"(dPdy.x), "f"(dPdy.y));
   *retVal = make_short2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex2DGrad(short4 *retVal, cudaTextureObject_t texObject, float x, float y, float2 dPdx, float2 dPdy)
{
   int4 tmp;
   asm volatile ("tex.grad.2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], {%7, %8}, {%9, %10};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(dPdx.x), "f"(dPdx.y), "f"(dPdy.x), "f"(dPdy.y));
   *retVal = make_short4(tmp.x, tmp.y, tmp.z, tmp.w);
}

static __forceinline__ __device__ void tex2DGrad(unsigned short *retVal, cudaTextureObject_t texObject, float x, float y, float2 dPdx, float2 dPdy)
{
   uint4 tmp;
   asm volatile ("tex.grad.2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], {%7, %8}, {%9, %10};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(dPdx.x), "f"(dPdx.y), "f"(dPdy.x), "f"(dPdy.y));
   *retVal = (unsigned short)(tmp.x);
}

static __forceinline__ __device__ void tex2DGrad(ushort1 *retVal, cudaTextureObject_t texObject, float x, float y, float2 dPdx, float2 dPdy)
{
   uint4 tmp;
   asm volatile ("tex.grad.2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], {%7, %8}, {%9, %10};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(dPdx.x), "f"(dPdx.y), "f"(dPdy.x), "f"(dPdy.y));
   *retVal = make_ushort1(tmp.x);
}

static __forceinline__ __device__ void tex2DGrad(ushort2 *retVal, cudaTextureObject_t texObject, float x, float y, float2 dPdx, float2 dPdy)
{
   uint4 tmp;
   asm volatile ("tex.grad.2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], {%7, %8}, {%9, %10};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(dPdx.x), "f"(dPdx.y), "f"(dPdy.x), "f"(dPdy.y));
   *retVal = make_ushort2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex2DGrad(ushort4 *retVal, cudaTextureObject_t texObject, float x, float y, float2 dPdx, float2 dPdy)
{
   uint4 tmp;
   asm volatile ("tex.grad.2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], {%7, %8}, {%9, %10};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(dPdx.x), "f"(dPdx.y), "f"(dPdy.x), "f"(dPdy.y));
   *retVal = make_ushort4(tmp.x, tmp.y, tmp.z, tmp.w);
}

/*******************************************************************************
*                                                                              *
*                                                                              *
*                                                                              *
*******************************************************************************/

static __forceinline__ __device__ void tex2DGrad(int *retVal, cudaTextureObject_t texObject, float x, float y, float2 dPdx, float2 dPdy)
{
   int4 tmp;
   asm volatile ("tex.grad.2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], {%7, %8}, {%9, %10};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(dPdx.x), "f"(dPdx.y), "f"(dPdy.x), "f"(dPdy.y));
   *retVal = (int)(tmp.x);
}

static __forceinline__ __device__ void tex2DGrad(int1 *retVal, cudaTextureObject_t texObject, float x, float y, float2 dPdx, float2 dPdy)
{
   int4 tmp;
   asm volatile ("tex.grad.2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], {%7, %8}, {%9, %10};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(dPdx.x), "f"(dPdx.y), "f"(dPdy.x), "f"(dPdy.y));
   *retVal = make_int1(tmp.x);
}

static __forceinline__ __device__ void tex2DGrad(int2 *retVal, cudaTextureObject_t texObject, float x, float y, float2 dPdx, float2 dPdy)
{
   int4 tmp;
   asm volatile ("tex.grad.2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], {%7, %8}, {%9, %10};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(dPdx.x), "f"(dPdx.y), "f"(dPdy.x), "f"(dPdy.y));
   *retVal = make_int2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex2DGrad(int4 *retVal, cudaTextureObject_t texObject, float x, float y, float2 dPdx, float2 dPdy)
{
   int4 tmp;
   asm volatile ("tex.grad.2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], {%7, %8}, {%9, %10};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(dPdx.x), "f"(dPdx.y), "f"(dPdy.x), "f"(dPdy.y));
   *retVal = make_int4(tmp.x, tmp.y, tmp.z, tmp.w);
}

static __forceinline__ __device__ void tex2DGrad(unsigned int *retVal, cudaTextureObject_t texObject, float x, float y, float2 dPdx, float2 dPdy)
{
   uint4 tmp;
   asm volatile ("tex.grad.2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], {%7, %8}, {%9, %10};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(dPdx.x), "f"(dPdx.y), "f"(dPdy.x), "f"(dPdy.y));
   *retVal = (unsigned int)(tmp.x);
}

static __forceinline__ __device__ void tex2DGrad(uint1 *retVal, cudaTextureObject_t texObject, float x, float y, float2 dPdx, float2 dPdy)
{
   uint4 tmp;
   asm volatile ("tex.grad.2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], {%7, %8}, {%9, %10};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(dPdx.x), "f"(dPdx.y), "f"(dPdy.x), "f"(dPdy.y));
   *retVal = make_uint1(tmp.x);
}

static __forceinline__ __device__ void tex2DGrad(uint2 *retVal, cudaTextureObject_t texObject, float x, float y, float2 dPdx, float2 dPdy)
{
   uint4 tmp;
   asm volatile ("tex.grad.2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], {%7, %8}, {%9, %10};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(dPdx.x), "f"(dPdx.y), "f"(dPdy.x), "f"(dPdy.y));
   *retVal = make_uint2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex2DGrad(uint4 *retVal, cudaTextureObject_t texObject, float x, float y, float2 dPdx, float2 dPdy)
{
   uint4 tmp;
   asm volatile ("tex.grad.2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], {%7, %8}, {%9, %10};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(dPdx.x), "f"(dPdx.y), "f"(dPdy.x), "f"(dPdy.y));
   *retVal = make_uint4(tmp.x, tmp.y, tmp.z, tmp.w);
}

/*******************************************************************************
*                                                                              *
*                                                                              *
*                                                                              *
*******************************************************************************/

#if !defined(__LP64__)

static __forceinline__ __device__ void tex2DGrad(long *retVal, cudaTextureObject_t texObject, float x, float y, float2 dPdx, float2 dPdy)
{
   int4 tmp;
   asm volatile ("tex.grad.2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], {%7, %8}, {%9, %10};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(dPdx.x), "f"(dPdx.y), "f"(dPdy.x), "f"(dPdy.y));
   *retVal = (long)(tmp.x);
}

static __forceinline__ __device__ void tex2DGrad(long1 *retVal, cudaTextureObject_t texObject, float x, float y, float2 dPdx, float2 dPdy)
{
   int4 tmp;
   asm volatile ("tex.grad.2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], {%7, %8}, {%9, %10};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(dPdx.x), "f"(dPdx.y), "f"(dPdy.x), "f"(dPdy.y));
   *retVal = make_long1(tmp.x);
}

static __forceinline__ __device__ void tex2DGrad(long2 *retVal, cudaTextureObject_t texObject, float x, float y, float2 dPdx, float2 dPdy)
{
   int4 tmp;
   asm volatile ("tex.grad.2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], {%7, %8}, {%9, %10};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(dPdx.x), "f"(dPdx.y), "f"(dPdy.x), "f"(dPdy.y));
   *retVal = make_long2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex2DGrad(long4 *retVal, cudaTextureObject_t texObject, float x, float y, float2 dPdx, float2 dPdy)
{
   int4 tmp;
   asm volatile ("tex.grad.2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], {%7, %8}, {%9, %10};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(dPdx.x), "f"(dPdx.y), "f"(dPdy.x), "f"(dPdy.y));
   *retVal = make_long4(tmp.x, tmp.y, tmp.z, tmp.w);
}

static __forceinline__ __device__ void tex2DGrad(unsigned long *retVal, cudaTextureObject_t texObject, float x, float y, float2 dPdx, float2 dPdy)
{
   uint4 tmp;
   asm volatile ("tex.grad.2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], {%7, %8}, {%9, %10};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(dPdx.x), "f"(dPdx.y), "f"(dPdy.x), "f"(dPdy.y));
   *retVal = (unsigned long)(tmp.x);
}

static __forceinline__ __device__ void tex2DGrad(ulong1 *retVal, cudaTextureObject_t texObject, float x, float y, float2 dPdx, float2 dPdy)
{
   uint4 tmp;
   asm volatile ("tex.grad.2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], {%7, %8}, {%9, %10};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(dPdx.x), "f"(dPdx.y), "f"(dPdy.x), "f"(dPdy.y));
   *retVal = make_ulong1(tmp.x);
}

static __forceinline__ __device__ void tex2DGrad(ulong2 *retVal, cudaTextureObject_t texObject, float x, float y, float2 dPdx, float2 dPdy)
{
   uint4 tmp;
   asm volatile ("tex.grad.2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], {%7, %8}, {%9, %10};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(dPdx.x), "f"(dPdx.y), "f"(dPdy.x), "f"(dPdy.y));
   *retVal = make_ulong2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex2DGrad(ulong4 *retVal, cudaTextureObject_t texObject, float x, float y, float2 dPdx, float2 dPdy)
{
   uint4 tmp;
   asm volatile ("tex.grad.2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], {%7, %8}, {%9, %10};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(dPdx.x), "f"(dPdx.y), "f"(dPdy.x), "f"(dPdy.y));
   *retVal = make_ulong4(tmp.x, tmp.y, tmp.z, tmp.w);
}

#endif /* !__LP64__ */


/*******************************************************************************
*                                                                              *
*                                                                              *
*                                                                              *
*******************************************************************************/

static __forceinline__ __device__ void tex2DGrad(float *retVal, cudaTextureObject_t texObject, float x, float y, float2 dPdx, float2 dPdy)
{
   float4 tmp;
   asm volatile ("tex.grad.2d.v4.f32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], {%7, %8}, {%9, %10};" : "=f"(tmp.x), "=f"(tmp.y), "=f"(tmp.z), "=f"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(dPdx.x), "f"(dPdx.y), "f"(dPdy.x), "f"(dPdy.y));
   *retVal = (float)(tmp.x);
}

static __forceinline__ __device__ void tex2DGrad(float1 *retVal, cudaTextureObject_t texObject, float x, float y, float2 dPdx, float2 dPdy)
{
   float4 tmp;
   asm volatile ("tex.grad.2d.v4.f32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], {%7, %8}, {%9, %10};" : "=f"(tmp.x), "=f"(tmp.y), "=f"(tmp.z), "=f"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(dPdx.x), "f"(dPdx.y), "f"(dPdy.x), "f"(dPdy.y));
   *retVal = make_float1(tmp.x);
}

static __forceinline__ __device__ void tex2DGrad(float2 *retVal, cudaTextureObject_t texObject, float x, float y, float2 dPdx, float2 dPdy)
{
   float4 tmp;
   asm volatile ("tex.grad.2d.v4.f32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], {%7, %8}, {%9, %10};" : "=f"(tmp.x), "=f"(tmp.y), "=f"(tmp.z), "=f"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(dPdx.x), "f"(dPdx.y), "f"(dPdy.x), "f"(dPdy.y));
   *retVal = make_float2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex2DGrad(float4 *retVal, cudaTextureObject_t texObject, float x, float y, float2 dPdx, float2 dPdy)
{
   float4 tmp;
   asm volatile ("tex.grad.2d.v4.f32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], {%7, %8}, {%9, %10};" : "=f"(tmp.x), "=f"(tmp.y), "=f"(tmp.z), "=f"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(dPdx.x), "f"(dPdx.y), "f"(dPdy.x), "f"(dPdy.y));
   *retVal = make_float4(tmp.x, tmp.y, tmp.z, tmp.w);
}

/*******************************************************************************
*                                                                              *
*                                                                              *
*                                                                              *
*******************************************************************************/

template <class T>
static __forceinline__ __device__ T tex2DGrad(cudaTextureObject_t texObject, float x, float y, float2 dPdx, float2 dPdy)
{
   T ret;
   tex2DGrad(&ret, texObject, x, y, dPdx, dPdy);
   return ret;
}

/*******************************************************************************
*                                                                              *
* 3D texture gradient indirect fetch functions
*                                                                              *
*******************************************************************************/

static __forceinline__ __device__ void tex3DGrad(char *retVal, cudaTextureObject_t texObject, float x, float y, float z, float4 dPdx, float4 dPdy)
{
#if defined(_CHAR_UNSIGNED) || defined(__CHAR_UNSIGNED__)
    uint4 tmp;
    asm volatile ("tex.grad.3d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], {%8, %9, %10, %10}, {%11, %12, %13, %13};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z), "f"(dPdx.x), "f"(dPdx.y), "f"(dPdx.z), "f"(dPdy.x), "f"(dPdy.y), "f"(dPdy.z));
#else /* _CHAR_UNSIGNED || __CHAR_UNSIGNED__ */
    int4 tmp;
    asm volatile ("tex.grad.3d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], {%8, %9, %10, %10}, {%11, %12, %13, %13};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z), "f"(dPdx.x), "f"(dPdx.y), "f"(dPdx.z), "f"(dPdy.x), "f"(dPdy.y), "f"(dPdy.z));
#endif /* _CHAR_UNSIGNED || __CHAR_UNSIGNED__ */
    *retVal = (char)tmp.x;
}
static __forceinline__ __device__ void tex3DGrad(signed char *retVal, cudaTextureObject_t texObject, float x, float y, float z, float4 dPdx, float4 dPdy)
{
   int4 tmp;
   asm volatile ("tex.grad.3d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], {%8, %9, %10, %10}, {%11, %12, %13, %13};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z), "f"(dPdx.x), "f"(dPdx.y), "f"(dPdx.z), "f"(dPdy.x), "f"(dPdy.y), "f"(dPdy.z));
   *retVal = (signed char)(tmp.x);
}

static __forceinline__ __device__ void tex3DGrad(char1 *retVal, cudaTextureObject_t texObject, float x, float y, float z, float4 dPdx, float4 dPdy)
{
   int4 tmp;
   asm volatile ("tex.grad.3d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], {%8, %9, %10, %10}, {%11, %12, %13, %13};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z), "f"(dPdx.x), "f"(dPdx.y), "f"(dPdx.z), "f"(dPdy.x), "f"(dPdy.y), "f"(dPdy.z));
   *retVal = make_char1(tmp.x);
}

static __forceinline__ __device__ void tex3DGrad(char2 *retVal, cudaTextureObject_t texObject, float x, float y, float z, float4 dPdx, float4 dPdy)
{
   int4 tmp;
   asm volatile ("tex.grad.3d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], {%8, %9, %10, %10}, {%11, %12, %13, %13};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z), "f"(dPdx.x), "f"(dPdx.y), "f"(dPdx.z), "f"(dPdy.x), "f"(dPdy.y), "f"(dPdy.z));
   *retVal = make_char2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex3DGrad(char4 *retVal, cudaTextureObject_t texObject, float x, float y, float z, float4 dPdx, float4 dPdy)
{
   int4 tmp;
   asm volatile ("tex.grad.3d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], {%8, %9, %10, %10}, {%11, %12, %13, %13};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z), "f"(dPdx.x), "f"(dPdx.y), "f"(dPdx.z), "f"(dPdy.x), "f"(dPdy.y), "f"(dPdy.z));
   *retVal = make_char4(tmp.x, tmp.y, tmp.z, tmp.w);
}

static __forceinline__ __device__ void tex3DGrad(unsigned char *retVal, cudaTextureObject_t texObject, float x, float y, float z, float4 dPdx, float4 dPdy)
{
   uint4 tmp;
   asm volatile ("tex.grad.3d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], {%8, %9, %10, %10}, {%11, %12, %13, %13};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z), "f"(dPdx.x), "f"(dPdx.y), "f"(dPdx.z), "f"(dPdy.x), "f"(dPdy.y), "f"(dPdy.z));
   *retVal = (unsigned char)(tmp.x);
}

static __forceinline__ __device__ void tex3DGrad(uchar1 *retVal, cudaTextureObject_t texObject, float x, float y, float z, float4 dPdx, float4 dPdy)
{
   uint4 tmp;
   asm volatile ("tex.grad.3d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], {%8, %9, %10, %10}, {%11, %12, %13, %13};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z), "f"(dPdx.x), "f"(dPdx.y), "f"(dPdx.z), "f"(dPdy.x), "f"(dPdy.y), "f"(dPdy.z));
   *retVal = make_uchar1(tmp.x);
}

static __forceinline__ __device__ void tex3DGrad(uchar2 *retVal, cudaTextureObject_t texObject, float x, float y, float z, float4 dPdx, float4 dPdy)
{
   uint4 tmp;
   asm volatile ("tex.grad.3d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], {%8, %9, %10, %10}, {%11, %12, %13, %13};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z), "f"(dPdx.x), "f"(dPdx.y), "f"(dPdx.z), "f"(dPdy.x), "f"(dPdy.y), "f"(dPdy.z));
   *retVal = make_uchar2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex3DGrad(uchar4 *retVal, cudaTextureObject_t texObject, float x, float y, float z, float4 dPdx, float4 dPdy)
{
   uint4 tmp;
   asm volatile ("tex.grad.3d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], {%8, %9, %10, %10}, {%11, %12, %13, %13};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z), "f"(dPdx.x), "f"(dPdx.y), "f"(dPdx.z), "f"(dPdy.x), "f"(dPdy.y), "f"(dPdy.z));
   *retVal = make_uchar4(tmp.x, tmp.y, tmp.z, tmp.w);
}

/*******************************************************************************
*                                                                              *
*                                                                              *
*                                                                              *
*******************************************************************************/

static __forceinline__ __device__ void tex3DGrad(short *retVal, cudaTextureObject_t texObject, float x, float y, float z, float4 dPdx, float4 dPdy)
{
   int4 tmp;
   asm volatile ("tex.grad.3d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], {%8, %9, %10, %10}, {%11, %12, %13, %13};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z), "f"(dPdx.x), "f"(dPdx.y), "f"(dPdx.z), "f"(dPdy.x), "f"(dPdy.y), "f"(dPdy.z));
   *retVal = (short)(tmp.x);
}

static __forceinline__ __device__ void tex3DGrad(short1 *retVal, cudaTextureObject_t texObject, float x, float y, float z, float4 dPdx, float4 dPdy)
{
   int4 tmp;
   asm volatile ("tex.grad.3d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], {%8, %9, %10, %10}, {%11, %12, %13, %13};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z), "f"(dPdx.x), "f"(dPdx.y), "f"(dPdx.z), "f"(dPdy.x), "f"(dPdy.y), "f"(dPdy.z));
   *retVal = make_short1(tmp.x);
}

static __forceinline__ __device__ void tex3DGrad(short2 *retVal, cudaTextureObject_t texObject, float x, float y, float z, float4 dPdx, float4 dPdy)
{
   int4 tmp;
   asm volatile ("tex.grad.3d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], {%8, %9, %10, %10}, {%11, %12, %13, %13};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z), "f"(dPdx.x), "f"(dPdx.y), "f"(dPdx.z), "f"(dPdy.x), "f"(dPdy.y), "f"(dPdy.z));
   *retVal = make_short2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex3DGrad(short4 *retVal, cudaTextureObject_t texObject, float x, float y, float z, float4 dPdx, float4 dPdy)
{
   int4 tmp;
   asm volatile ("tex.grad.3d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], {%8, %9, %10, %10}, {%11, %12, %13, %13};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z), "f"(dPdx.x), "f"(dPdx.y), "f"(dPdx.z), "f"(dPdy.x), "f"(dPdy.y), "f"(dPdy.z));
   *retVal = make_short4(tmp.x, tmp.y, tmp.z, tmp.w);
}

static __forceinline__ __device__ void tex3DGrad(unsigned short *retVal, cudaTextureObject_t texObject, float x, float y, float z, float4 dPdx, float4 dPdy)
{
   uint4 tmp;
   asm volatile ("tex.grad.3d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], {%8, %9, %10, %10}, {%11, %12, %13, %13};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z), "f"(dPdx.x), "f"(dPdx.y), "f"(dPdx.z), "f"(dPdy.x), "f"(dPdy.y), "f"(dPdy.z));
   *retVal = (unsigned short)(tmp.x);
}

static __forceinline__ __device__ void tex3DGrad(ushort1 *retVal, cudaTextureObject_t texObject, float x, float y, float z, float4 dPdx, float4 dPdy)
{
   uint4 tmp;
   asm volatile ("tex.grad.3d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], {%8, %9, %10, %10}, {%11, %12, %13, %13};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z), "f"(dPdx.x), "f"(dPdx.y), "f"(dPdx.z), "f"(dPdy.x), "f"(dPdy.y), "f"(dPdy.z));
   *retVal = make_ushort1(tmp.x);
}

static __forceinline__ __device__ void tex3DGrad(ushort2 *retVal, cudaTextureObject_t texObject, float x, float y, float z, float4 dPdx, float4 dPdy)
{
   uint4 tmp;
   asm volatile ("tex.grad.3d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], {%8, %9, %10, %10}, {%11, %12, %13, %13};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z), "f"(dPdx.x), "f"(dPdx.y), "f"(dPdx.z), "f"(dPdy.x), "f"(dPdy.y), "f"(dPdy.z));
   *retVal = make_ushort2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex3DGrad(ushort4 *retVal, cudaTextureObject_t texObject, float x, float y, float z, float4 dPdx, float4 dPdy)
{
   uint4 tmp;
   asm volatile ("tex.grad.3d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], {%8, %9, %10, %10}, {%11, %12, %13, %13};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z), "f"(dPdx.x), "f"(dPdx.y), "f"(dPdx.z), "f"(dPdy.x), "f"(dPdy.y), "f"(dPdy.z));
   *retVal = make_ushort4(tmp.x, tmp.y, tmp.z, tmp.w);
}

/*******************************************************************************
*                                                                              *
*                                                                              *
*                                                                              *
*******************************************************************************/

static __forceinline__ __device__ void tex3DGrad(int *retVal, cudaTextureObject_t texObject, float x, float y, float z, float4 dPdx, float4 dPdy)
{
   int4 tmp;
   asm volatile ("tex.grad.3d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], {%8, %9, %10, %10}, {%11, %12, %13, %13};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z), "f"(dPdx.x), "f"(dPdx.y), "f"(dPdx.z), "f"(dPdy.x), "f"(dPdy.y), "f"(dPdy.z));
   *retVal = (int)(tmp.x);
}

static __forceinline__ __device__ void tex3DGrad(int1 *retVal, cudaTextureObject_t texObject, float x, float y, float z, float4 dPdx, float4 dPdy)
{
   int4 tmp;
   asm volatile ("tex.grad.3d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], {%8, %9, %10, %10}, {%11, %12, %13, %13};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z), "f"(dPdx.x), "f"(dPdx.y), "f"(dPdx.z), "f"(dPdy.x), "f"(dPdy.y), "f"(dPdy.z));
   *retVal = make_int1(tmp.x);
}

static __forceinline__ __device__ void tex3DGrad(int2 *retVal, cudaTextureObject_t texObject, float x, float y, float z, float4 dPdx, float4 dPdy)
{
   int4 tmp;
   asm volatile ("tex.grad.3d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], {%8, %9, %10, %10}, {%11, %12, %13, %13};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z), "f"(dPdx.x), "f"(dPdx.y), "f"(dPdx.z), "f"(dPdy.x), "f"(dPdy.y), "f"(dPdy.z));
   *retVal = make_int2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex3DGrad(int4 *retVal, cudaTextureObject_t texObject, float x, float y, float z, float4 dPdx, float4 dPdy)
{
   int4 tmp;
   asm volatile ("tex.grad.3d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], {%8, %9, %10, %10}, {%11, %12, %13, %13};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z), "f"(dPdx.x), "f"(dPdx.y), "f"(dPdx.z), "f"(dPdy.x), "f"(dPdy.y), "f"(dPdy.z));
   *retVal = make_int4(tmp.x, tmp.y, tmp.z, tmp.w);
}

static __forceinline__ __device__ void tex3DGrad(unsigned int *retVal, cudaTextureObject_t texObject, float x, float y, float z, float4 dPdx, float4 dPdy)
{
   uint4 tmp;
   asm volatile ("tex.grad.3d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], {%8, %9, %10, %10}, {%11, %12, %13, %13};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z), "f"(dPdx.x), "f"(dPdx.y), "f"(dPdx.z), "f"(dPdy.x), "f"(dPdy.y), "f"(dPdy.z));
   *retVal = (unsigned int)(tmp.x);
}

static __forceinline__ __device__ void tex3DGrad(uint1 *retVal, cudaTextureObject_t texObject, float x, float y, float z, float4 dPdx, float4 dPdy)
{
   uint4 tmp;
   asm volatile ("tex.grad.3d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], {%8, %9, %10, %10}, {%11, %12, %13, %13};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z), "f"(dPdx.x), "f"(dPdx.y), "f"(dPdx.z), "f"(dPdy.x), "f"(dPdy.y), "f"(dPdy.z));
   *retVal = make_uint1(tmp.x);
}

static __forceinline__ __device__ void tex3DGrad(uint2 *retVal, cudaTextureObject_t texObject, float x, float y, float z, float4 dPdx, float4 dPdy)
{
   uint4 tmp;
   asm volatile ("tex.grad.3d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], {%8, %9, %10, %10}, {%11, %12, %13, %13};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z), "f"(dPdx.x), "f"(dPdx.y), "f"(dPdx.z), "f"(dPdy.x), "f"(dPdy.y), "f"(dPdy.z));
   *retVal = make_uint2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex3DGrad(uint4 *retVal, cudaTextureObject_t texObject, float x, float y, float z, float4 dPdx, float4 dPdy)
{
   uint4 tmp;
   asm volatile ("tex.grad.3d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], {%8, %9, %10, %10}, {%11, %12, %13, %13};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z), "f"(dPdx.x), "f"(dPdx.y), "f"(dPdx.z), "f"(dPdy.x), "f"(dPdy.y), "f"(dPdy.z));
   *retVal = make_uint4(tmp.x, tmp.y, tmp.z, tmp.w);
}

/*******************************************************************************
*                                                                              *
*                                                                              *
*                                                                              *
*******************************************************************************/

#if !defined(__LP64__)

static __forceinline__ __device__ void tex3DGrad(long *retVal, cudaTextureObject_t texObject, float x, float y, float z, float4 dPdx, float4 dPdy)
{
   int4 tmp;
   asm volatile ("tex.grad.3d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], {%8, %9, %10, %10}, {%11, %12, %13, %13};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z), "f"(dPdx.x), "f"(dPdx.y), "f"(dPdx.z), "f"(dPdy.x), "f"(dPdy.y), "f"(dPdy.z));
   *retVal = (long)(tmp.x);
}

static __forceinline__ __device__ void tex3DGrad(long1 *retVal, cudaTextureObject_t texObject, float x, float y, float z, float4 dPdx, float4 dPdy)
{
   int4 tmp;
   asm volatile ("tex.grad.3d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], {%8, %9, %10, %10}, {%11, %12, %13, %13};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z), "f"(dPdx.x), "f"(dPdx.y), "f"(dPdx.z), "f"(dPdy.x), "f"(dPdy.y), "f"(dPdy.z));
   *retVal = make_long1(tmp.x);
}

static __forceinline__ __device__ void tex3DGrad(long2 *retVal, cudaTextureObject_t texObject, float x, float y, float z, float4 dPdx, float4 dPdy)
{
   int4 tmp;
   asm volatile ("tex.grad.3d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], {%8, %9, %10, %10}, {%11, %12, %13, %13};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z), "f"(dPdx.x), "f"(dPdx.y), "f"(dPdx.z), "f"(dPdy.x), "f"(dPdy.y), "f"(dPdy.z));
   *retVal = make_long2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex3DGrad(long4 *retVal, cudaTextureObject_t texObject, float x, float y, float z, float4 dPdx, float4 dPdy)
{
   int4 tmp;
   asm volatile ("tex.grad.3d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], {%8, %9, %10, %10}, {%11, %12, %13, %13};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z), "f"(dPdx.x), "f"(dPdx.y), "f"(dPdx.z), "f"(dPdy.x), "f"(dPdy.y), "f"(dPdy.z));
   *retVal = make_long4(tmp.x, tmp.y, tmp.z, tmp.w);
}

static __forceinline__ __device__ void tex3DGrad(unsigned long *retVal, cudaTextureObject_t texObject, float x, float y, float z, float4 dPdx, float4 dPdy)
{
   uint4 tmp;
   asm volatile ("tex.grad.3d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], {%8, %9, %10, %10}, {%11, %12, %13, %13};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z), "f"(dPdx.x), "f"(dPdx.y), "f"(dPdx.z), "f"(dPdy.x), "f"(dPdy.y), "f"(dPdy.z));
   *retVal = (unsigned long)(tmp.x);
}

static __forceinline__ __device__ void tex3DGrad(ulong1 *retVal, cudaTextureObject_t texObject, float x, float y, float z, float4 dPdx, float4 dPdy)
{
   uint4 tmp;
   asm volatile ("tex.grad.3d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], {%8, %9, %10, %10}, {%11, %12, %13, %13};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z), "f"(dPdx.x), "f"(dPdx.y), "f"(dPdx.z), "f"(dPdy.x), "f"(dPdy.y), "f"(dPdy.z));
   *retVal = make_ulong1(tmp.x);
}

static __forceinline__ __device__ void tex3DGrad(ulong2 *retVal, cudaTextureObject_t texObject, float x, float y, float z, float4 dPdx, float4 dPdy)
{
   uint4 tmp;
   asm volatile ("tex.grad.3d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], {%8, %9, %10, %10}, {%11, %12, %13, %13};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z), "f"(dPdx.x), "f"(dPdx.y), "f"(dPdx.z), "f"(dPdy.x), "f"(dPdy.y), "f"(dPdy.z));
   *retVal = make_ulong2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex3DGrad(ulong4 *retVal, cudaTextureObject_t texObject, float x, float y, float z, float4 dPdx, float4 dPdy)
{
   uint4 tmp;
   asm volatile ("tex.grad.3d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], {%8, %9, %10, %10}, {%11, %12, %13, %13};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z), "f"(dPdx.x), "f"(dPdx.y), "f"(dPdx.z), "f"(dPdy.x), "f"(dPdy.y), "f"(dPdy.z));
   *retVal = make_ulong4(tmp.x, tmp.y, tmp.z, tmp.w);
}

#endif /* !__LP64__ */


/*******************************************************************************
*                                                                              *
*                                                                              *
*                                                                              *
*******************************************************************************/

static __forceinline__ __device__ void tex3DGrad(float *retVal, cudaTextureObject_t texObject, float x, float y, float z, float4 dPdx, float4 dPdy)
{
   float4 tmp;
   asm volatile ("tex.grad.3d.v4.f32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], {%8, %9, %10, %10}, {%11, %12, %13, %13};" : "=f"(tmp.x), "=f"(tmp.y), "=f"(tmp.z), "=f"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z), "f"(dPdx.x), "f"(dPdx.y), "f"(dPdx.z), "f"(dPdy.x), "f"(dPdy.y), "f"(dPdy.z));
   *retVal = (float)(tmp.x);
}

static __forceinline__ __device__ void tex3DGrad(float1 *retVal, cudaTextureObject_t texObject, float x, float y, float z, float4 dPdx, float4 dPdy)
{
   float4 tmp;
   asm volatile ("tex.grad.3d.v4.f32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], {%8, %9, %10, %10}, {%11, %12, %13, %13};" : "=f"(tmp.x), "=f"(tmp.y), "=f"(tmp.z), "=f"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z), "f"(dPdx.x), "f"(dPdx.y), "f"(dPdx.z), "f"(dPdy.x), "f"(dPdy.y), "f"(dPdy.z));
   *retVal = make_float1(tmp.x);
}

static __forceinline__ __device__ void tex3DGrad(float2 *retVal, cudaTextureObject_t texObject, float x, float y, float z, float4 dPdx, float4 dPdy)
{
   float4 tmp;
   asm volatile ("tex.grad.3d.v4.f32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], {%8, %9, %10, %10}, {%11, %12, %13, %13};" : "=f"(tmp.x), "=f"(tmp.y), "=f"(tmp.z), "=f"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z), "f"(dPdx.x), "f"(dPdx.y), "f"(dPdx.z), "f"(dPdy.x), "f"(dPdy.y), "f"(dPdy.z));
   *retVal = make_float2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex3DGrad(float4 *retVal, cudaTextureObject_t texObject, float x, float y, float z, float4 dPdx, float4 dPdy)
{
   float4 tmp;
   asm volatile ("tex.grad.3d.v4.f32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], {%8, %9, %10, %10}, {%11, %12, %13, %13};" : "=f"(tmp.x), "=f"(tmp.y), "=f"(tmp.z), "=f"(tmp.w) : "l"(texObject), "f"(x), "f"(y), "f"(z), "f"(dPdx.x), "f"(dPdx.y), "f"(dPdx.z), "f"(dPdy.x), "f"(dPdy.y), "f"(dPdy.z));
   *retVal = make_float4(tmp.x, tmp.y, tmp.z, tmp.w);
}

/*******************************************************************************
*                                                                              *
*                                                                              *
*                                                                              *
*******************************************************************************/

template <class T>
static __forceinline__ __device__ T tex3DGrad(cudaTextureObject_t texObject, float x, float y, float z, float4 dPdx, float4 dPdy)
{
   T ret;
   tex3DGrad(&ret, texObject, x, y, z, dPdx, dPdy);
   return ret;
}

/*******************************************************************************
*                                                                              *
* 1D Layered texture gradient indirect fetch functions
*                                                                              *
*******************************************************************************/

static __forceinline__ __device__ void tex1DLayeredGrad(char *retVal, cudaTextureObject_t texObject, float x, int layer, float dPdx, float dPdy)
{
#if defined(_CHAR_UNSIGNED) || defined(__CHAR_UNSIGNED__)
    uint4 tmp;
    asm volatile ("tex.grad.a1d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], {%7}, {%8};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(dPdx), "f"(dPdy));
#else /* _CHAR_UNSIGNED || __CHAR_UNSIGNED__ */
    int4 tmp;
    asm volatile ("tex.grad.a1d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], {%7}, {%8};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(dPdx), "f"(dPdy));
#endif /* _CHAR_UNSIGNED || __CHAR_UNSIGNED__ */
    *retVal = (char)tmp.x;
}
static __forceinline__ __device__ void tex1DLayeredGrad(signed char *retVal, cudaTextureObject_t texObject, float x, int layer, float dPdx, float dPdy)
{
   int4 tmp;
   asm volatile ("tex.grad.a1d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], {%7}, {%8};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(dPdx), "f"(dPdy));
   *retVal = (signed char)(tmp.x);
}

static __forceinline__ __device__ void tex1DLayeredGrad(char1 *retVal, cudaTextureObject_t texObject, float x, int layer, float dPdx, float dPdy)
{
   int4 tmp;
   asm volatile ("tex.grad.a1d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], {%7}, {%8};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(dPdx), "f"(dPdy));
   *retVal = make_char1(tmp.x);
}

static __forceinline__ __device__ void tex1DLayeredGrad(char2 *retVal, cudaTextureObject_t texObject, float x, int layer, float dPdx, float dPdy)
{
   int4 tmp;
   asm volatile ("tex.grad.a1d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], {%7}, {%8};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(dPdx), "f"(dPdy));
   *retVal = make_char2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex1DLayeredGrad(char4 *retVal, cudaTextureObject_t texObject, float x, int layer, float dPdx, float dPdy)
{
   int4 tmp;
   asm volatile ("tex.grad.a1d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], {%7}, {%8};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(dPdx), "f"(dPdy));
   *retVal = make_char4(tmp.x, tmp.y, tmp.z, tmp.w);
}

static __forceinline__ __device__ void tex1DLayeredGrad(unsigned char *retVal, cudaTextureObject_t texObject, float x, int layer, float dPdx, float dPdy)
{
   uint4 tmp;
   asm volatile ("tex.grad.a1d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], {%7}, {%8};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(dPdx), "f"(dPdy));
   *retVal = (unsigned char)(tmp.x);
}

static __forceinline__ __device__ void tex1DLayeredGrad(uchar1 *retVal, cudaTextureObject_t texObject, float x, int layer, float dPdx, float dPdy)
{
   uint4 tmp;
   asm volatile ("tex.grad.a1d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], {%7}, {%8};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(dPdx), "f"(dPdy));
   *retVal = make_uchar1(tmp.x);
}

static __forceinline__ __device__ void tex1DLayeredGrad(uchar2 *retVal, cudaTextureObject_t texObject, float x, int layer, float dPdx, float dPdy)
{
   uint4 tmp;
   asm volatile ("tex.grad.a1d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], {%7}, {%8};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(dPdx), "f"(dPdy));
   *retVal = make_uchar2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex1DLayeredGrad(uchar4 *retVal, cudaTextureObject_t texObject, float x, int layer, float dPdx, float dPdy)
{
   uint4 tmp;
   asm volatile ("tex.grad.a1d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], {%7}, {%8};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(dPdx), "f"(dPdy));
   *retVal = make_uchar4(tmp.x, tmp.y, tmp.z, tmp.w);
}

/*******************************************************************************
*                                                                              *
*                                                                              *
*                                                                              *
*******************************************************************************/

static __forceinline__ __device__ void tex1DLayeredGrad(short *retVal, cudaTextureObject_t texObject, float x, int layer, float dPdx, float dPdy)
{
   int4 tmp;
   asm volatile ("tex.grad.a1d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], {%7}, {%8};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(dPdx), "f"(dPdy));
   *retVal = (short)(tmp.x);
}

static __forceinline__ __device__ void tex1DLayeredGrad(short1 *retVal, cudaTextureObject_t texObject, float x, int layer, float dPdx, float dPdy)
{
   int4 tmp;
   asm volatile ("tex.grad.a1d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], {%7}, {%8};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(dPdx), "f"(dPdy));
   *retVal = make_short1(tmp.x);
}

static __forceinline__ __device__ void tex1DLayeredGrad(short2 *retVal, cudaTextureObject_t texObject, float x, int layer, float dPdx, float dPdy)
{
   int4 tmp;
   asm volatile ("tex.grad.a1d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], {%7}, {%8};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(dPdx), "f"(dPdy));
   *retVal = make_short2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex1DLayeredGrad(short4 *retVal, cudaTextureObject_t texObject, float x, int layer, float dPdx, float dPdy)
{
   int4 tmp;
   asm volatile ("tex.grad.a1d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], {%7}, {%8};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(dPdx), "f"(dPdy));
   *retVal = make_short4(tmp.x, tmp.y, tmp.z, tmp.w);
}

static __forceinline__ __device__ void tex1DLayeredGrad(unsigned short *retVal, cudaTextureObject_t texObject, float x, int layer, float dPdx, float dPdy)
{
   uint4 tmp;
   asm volatile ("tex.grad.a1d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], {%7}, {%8};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(dPdx), "f"(dPdy));
   *retVal = (unsigned short)(tmp.x);
}

static __forceinline__ __device__ void tex1DLayeredGrad(ushort1 *retVal, cudaTextureObject_t texObject, float x, int layer, float dPdx, float dPdy)
{
   uint4 tmp;
   asm volatile ("tex.grad.a1d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], {%7}, {%8};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(dPdx), "f"(dPdy));
   *retVal = make_ushort1(tmp.x);
}

static __forceinline__ __device__ void tex1DLayeredGrad(ushort2 *retVal, cudaTextureObject_t texObject, float x, int layer, float dPdx, float dPdy)
{
   uint4 tmp;
   asm volatile ("tex.grad.a1d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], {%7}, {%8};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(dPdx), "f"(dPdy));
   *retVal = make_ushort2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex1DLayeredGrad(ushort4 *retVal, cudaTextureObject_t texObject, float x, int layer, float dPdx, float dPdy)
{
   uint4 tmp;
   asm volatile ("tex.grad.a1d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], {%7}, {%8};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(dPdx), "f"(dPdy));
   *retVal = make_ushort4(tmp.x, tmp.y, tmp.z, tmp.w);
}

/*******************************************************************************
*                                                                              *
*                                                                              *
*                                                                              *
*******************************************************************************/

static __forceinline__ __device__ void tex1DLayeredGrad(int *retVal, cudaTextureObject_t texObject, float x, int layer, float dPdx, float dPdy)
{
   int4 tmp;
   asm volatile ("tex.grad.a1d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], {%7}, {%8};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(dPdx), "f"(dPdy));
   *retVal = (int)(tmp.x);
}

static __forceinline__ __device__ void tex1DLayeredGrad(int1 *retVal, cudaTextureObject_t texObject, float x, int layer, float dPdx, float dPdy)
{
   int4 tmp;
   asm volatile ("tex.grad.a1d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], {%7}, {%8};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(dPdx), "f"(dPdy));
   *retVal = make_int1(tmp.x);
}

static __forceinline__ __device__ void tex1DLayeredGrad(int2 *retVal, cudaTextureObject_t texObject, float x, int layer, float dPdx, float dPdy)
{
   int4 tmp;
   asm volatile ("tex.grad.a1d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], {%7}, {%8};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(dPdx), "f"(dPdy));
   *retVal = make_int2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex1DLayeredGrad(int4 *retVal, cudaTextureObject_t texObject, float x, int layer, float dPdx, float dPdy)
{
   int4 tmp;
   asm volatile ("tex.grad.a1d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], {%7}, {%8};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(dPdx), "f"(dPdy));
   *retVal = make_int4(tmp.x, tmp.y, tmp.z, tmp.w);
}

static __forceinline__ __device__ void tex1DLayeredGrad(unsigned int *retVal, cudaTextureObject_t texObject, float x, int layer, float dPdx, float dPdy)
{
   uint4 tmp;
   asm volatile ("tex.grad.a1d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], {%7}, {%8};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(dPdx), "f"(dPdy));
   *retVal = (unsigned int)(tmp.x);
}

static __forceinline__ __device__ void tex1DLayeredGrad(uint1 *retVal, cudaTextureObject_t texObject, float x, int layer, float dPdx, float dPdy)
{
   uint4 tmp;
   asm volatile ("tex.grad.a1d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], {%7}, {%8};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(dPdx), "f"(dPdy));
   *retVal = make_uint1(tmp.x);
}

static __forceinline__ __device__ void tex1DLayeredGrad(uint2 *retVal, cudaTextureObject_t texObject, float x, int layer, float dPdx, float dPdy)
{
   uint4 tmp;
   asm volatile ("tex.grad.a1d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], {%7}, {%8};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(dPdx), "f"(dPdy));
   *retVal = make_uint2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex1DLayeredGrad(uint4 *retVal, cudaTextureObject_t texObject, float x, int layer, float dPdx, float dPdy)
{
   uint4 tmp;
   asm volatile ("tex.grad.a1d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], {%7}, {%8};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(dPdx), "f"(dPdy));
   *retVal = make_uint4(tmp.x, tmp.y, tmp.z, tmp.w);
}

/*******************************************************************************
*                                                                              *
*                                                                              *
*                                                                              *
*******************************************************************************/

#if !defined(__LP64__)

static __forceinline__ __device__ void tex1DLayeredGrad(long *retVal, cudaTextureObject_t texObject, float x, int layer, float dPdx, float dPdy)
{
   int4 tmp;
   asm volatile ("tex.grad.a1d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], {%7}, {%8};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(dPdx), "f"(dPdy));
   *retVal = (long)(tmp.x);
}

static __forceinline__ __device__ void tex1DLayeredGrad(long1 *retVal, cudaTextureObject_t texObject, float x, int layer, float dPdx, float dPdy)
{
   int4 tmp;
   asm volatile ("tex.grad.a1d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], {%7}, {%8};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(dPdx), "f"(dPdy));
   *retVal = make_long1(tmp.x);
}

static __forceinline__ __device__ void tex1DLayeredGrad(long2 *retVal, cudaTextureObject_t texObject, float x, int layer, float dPdx, float dPdy)
{
   int4 tmp;
   asm volatile ("tex.grad.a1d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], {%7}, {%8};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(dPdx), "f"(dPdy));
   *retVal = make_long2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex1DLayeredGrad(long4 *retVal, cudaTextureObject_t texObject, float x, int layer, float dPdx, float dPdy)
{
   int4 tmp;
   asm volatile ("tex.grad.a1d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], {%7}, {%8};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(dPdx), "f"(dPdy));
   *retVal = make_long4(tmp.x, tmp.y, tmp.z, tmp.w);
}

static __forceinline__ __device__ void tex1DLayeredGrad(unsigned long *retVal, cudaTextureObject_t texObject, float x, int layer, float dPdx, float dPdy)
{
   uint4 tmp;
   asm volatile ("tex.grad.a1d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], {%7}, {%8};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(dPdx), "f"(dPdy));
   *retVal = (unsigned long)(tmp.x);
}

static __forceinline__ __device__ void tex1DLayeredGrad(ulong1 *retVal, cudaTextureObject_t texObject, float x, int layer, float dPdx, float dPdy)
{
   uint4 tmp;
   asm volatile ("tex.grad.a1d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], {%7}, {%8};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(dPdx), "f"(dPdy));
   *retVal = make_ulong1(tmp.x);
}

static __forceinline__ __device__ void tex1DLayeredGrad(ulong2 *retVal, cudaTextureObject_t texObject, float x, int layer, float dPdx, float dPdy)
{
   uint4 tmp;
   asm volatile ("tex.grad.a1d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], {%7}, {%8};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(dPdx), "f"(dPdy));
   *retVal = make_ulong2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex1DLayeredGrad(ulong4 *retVal, cudaTextureObject_t texObject, float x, int layer, float dPdx, float dPdy)
{
   uint4 tmp;
   asm volatile ("tex.grad.a1d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], {%7}, {%8};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(dPdx), "f"(dPdy));
   *retVal = make_ulong4(tmp.x, tmp.y, tmp.z, tmp.w);
}

#endif /* !__LP64__ */


/*******************************************************************************
*                                                                              *
*                                                                              *
*                                                                              *
*******************************************************************************/

static __forceinline__ __device__ void tex1DLayeredGrad(float *retVal, cudaTextureObject_t texObject, float x, int layer, float dPdx, float dPdy)
{
   float4 tmp;
   asm volatile ("tex.grad.a1d.v4.f32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], {%7}, {%8};" : "=f"(tmp.x), "=f"(tmp.y), "=f"(tmp.z), "=f"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(dPdx), "f"(dPdy));
   *retVal = (float)(tmp.x);
}

static __forceinline__ __device__ void tex1DLayeredGrad(float1 *retVal, cudaTextureObject_t texObject, float x, int layer, float dPdx, float dPdy)
{
   float4 tmp;
   asm volatile ("tex.grad.a1d.v4.f32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], {%7}, {%8};" : "=f"(tmp.x), "=f"(tmp.y), "=f"(tmp.z), "=f"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(dPdx), "f"(dPdy));
   *retVal = make_float1(tmp.x);
}

static __forceinline__ __device__ void tex1DLayeredGrad(float2 *retVal, cudaTextureObject_t texObject, float x, int layer, float dPdx, float dPdy)
{
   float4 tmp;
   asm volatile ("tex.grad.a1d.v4.f32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], {%7}, {%8};" : "=f"(tmp.x), "=f"(tmp.y), "=f"(tmp.z), "=f"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(dPdx), "f"(dPdy));
   *retVal = make_float2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex1DLayeredGrad(float4 *retVal, cudaTextureObject_t texObject, float x, int layer, float dPdx, float dPdy)
{
   float4 tmp;
   asm volatile ("tex.grad.a1d.v4.f32.f32 {%0, %1, %2, %3}, [%4, {%5, %6}], {%7}, {%8};" : "=f"(tmp.x), "=f"(tmp.y), "=f"(tmp.z), "=f"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(dPdx), "f"(dPdy));
   *retVal = make_float4(tmp.x, tmp.y, tmp.z, tmp.w);
}

/*******************************************************************************
*                                                                              *
*                                                                              *
*                                                                              *
*******************************************************************************/

template <class T>
static __forceinline__ __device__ T tex1DLayeredGrad(cudaTextureObject_t texObject, float x, int layer, float dPdx, float dPdy)
{
   T ret;
   tex1DLayeredGrad(&ret, texObject, x, layer, dPdx, dPdy);
   return ret;
}

/*******************************************************************************
*                                                                              *
* 2D Layered texture gradient indirect fetch functions
*                                                                              *
*******************************************************************************/

static __forceinline__ __device__ void tex2DLayeredGrad(char *retVal, cudaTextureObject_t texObject, float x, float y, int layer, float2 dPdx, float2 dPdy)
{
#if defined(_CHAR_UNSIGNED) || defined(__CHAR_UNSIGNED__)
    uint4 tmp;
    asm volatile ("tex.grad.a2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], {%8, %9}, {%10, %11};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(dPdx.x), "f"(dPdx.y), "f"(dPdy.x), "f"(dPdy.y));
#else /* _CHAR_UNSIGNED || __CHAR_UNSIGNED__ */
    int4 tmp;
    asm volatile ("tex.grad.a2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], {%8, %9}, {%10, %11};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(dPdx.x), "f"(dPdx.y), "f"(dPdy.x), "f"(dPdy.y));
#endif /* _CHAR_UNSIGNED || __CHAR_UNSIGNED__ */
    *retVal = (char)tmp.x;
}
static __forceinline__ __device__ void tex2DLayeredGrad(signed char *retVal, cudaTextureObject_t texObject, float x, float y, int layer, float2 dPdx, float2 dPdy)
{
   int4 tmp;
   asm volatile ("tex.grad.a2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], {%8, %9}, {%10, %11};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(dPdx.x), "f"(dPdx.y), "f"(dPdy.x), "f"(dPdy.y));
   *retVal = (signed char)(tmp.x);
}

static __forceinline__ __device__ void tex2DLayeredGrad(char1 *retVal, cudaTextureObject_t texObject, float x, float y, int layer, float2 dPdx, float2 dPdy)
{
   int4 tmp;
   asm volatile ("tex.grad.a2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], {%8, %9}, {%10, %11};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(dPdx.x), "f"(dPdx.y), "f"(dPdy.x), "f"(dPdy.y));
   *retVal = make_char1(tmp.x);
}

static __forceinline__ __device__ void tex2DLayeredGrad(char2 *retVal, cudaTextureObject_t texObject, float x, float y, int layer, float2 dPdx, float2 dPdy)
{
   int4 tmp;
   asm volatile ("tex.grad.a2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], {%8, %9}, {%10, %11};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(dPdx.x), "f"(dPdx.y), "f"(dPdy.x), "f"(dPdy.y));
   *retVal = make_char2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex2DLayeredGrad(char4 *retVal, cudaTextureObject_t texObject, float x, float y, int layer, float2 dPdx, float2 dPdy)
{
   int4 tmp;
   asm volatile ("tex.grad.a2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], {%8, %9}, {%10, %11};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(dPdx.x), "f"(dPdx.y), "f"(dPdy.x), "f"(dPdy.y));
   *retVal = make_char4(tmp.x, tmp.y, tmp.z, tmp.w);
}

static __forceinline__ __device__ void tex2DLayeredGrad(unsigned char *retVal, cudaTextureObject_t texObject, float x, float y, int layer, float2 dPdx, float2 dPdy)
{
   uint4 tmp;
   asm volatile ("tex.grad.a2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], {%8, %9}, {%10, %11};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(dPdx.x), "f"(dPdx.y), "f"(dPdy.x), "f"(dPdy.y));
   *retVal = (unsigned char)(tmp.x);
}

static __forceinline__ __device__ void tex2DLayeredGrad(uchar1 *retVal, cudaTextureObject_t texObject, float x, float y, int layer, float2 dPdx, float2 dPdy)
{
   uint4 tmp;
   asm volatile ("tex.grad.a2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], {%8, %9}, {%10, %11};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(dPdx.x), "f"(dPdx.y), "f"(dPdy.x), "f"(dPdy.y));
   *retVal = make_uchar1(tmp.x);
}

static __forceinline__ __device__ void tex2DLayeredGrad(uchar2 *retVal, cudaTextureObject_t texObject, float x, float y, int layer, float2 dPdx, float2 dPdy)
{
   uint4 tmp;
   asm volatile ("tex.grad.a2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], {%8, %9}, {%10, %11};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(dPdx.x), "f"(dPdx.y), "f"(dPdy.x), "f"(dPdy.y));
   *retVal = make_uchar2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex2DLayeredGrad(uchar4 *retVal, cudaTextureObject_t texObject, float x, float y, int layer, float2 dPdx, float2 dPdy)
{
   uint4 tmp;
   asm volatile ("tex.grad.a2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], {%8, %9}, {%10, %11};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(dPdx.x), "f"(dPdx.y), "f"(dPdy.x), "f"(dPdy.y));
   *retVal = make_uchar4(tmp.x, tmp.y, tmp.z, tmp.w);
}

/*******************************************************************************
*                                                                              *
*                                                                              *
*                                                                              *
*******************************************************************************/

static __forceinline__ __device__ void tex2DLayeredGrad(short *retVal, cudaTextureObject_t texObject, float x, float y, int layer, float2 dPdx, float2 dPdy)
{
   int4 tmp;
   asm volatile ("tex.grad.a2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], {%8, %9}, {%10, %11};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(dPdx.x), "f"(dPdx.y), "f"(dPdy.x), "f"(dPdy.y));
   *retVal = (short)(tmp.x);
}

static __forceinline__ __device__ void tex2DLayeredGrad(short1 *retVal, cudaTextureObject_t texObject, float x, float y, int layer, float2 dPdx, float2 dPdy)
{
   int4 tmp;
   asm volatile ("tex.grad.a2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], {%8, %9}, {%10, %11};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(dPdx.x), "f"(dPdx.y), "f"(dPdy.x), "f"(dPdy.y));
   *retVal = make_short1(tmp.x);
}

static __forceinline__ __device__ void tex2DLayeredGrad(short2 *retVal, cudaTextureObject_t texObject, float x, float y, int layer, float2 dPdx, float2 dPdy)
{
   int4 tmp;
   asm volatile ("tex.grad.a2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], {%8, %9}, {%10, %11};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(dPdx.x), "f"(dPdx.y), "f"(dPdy.x), "f"(dPdy.y));
   *retVal = make_short2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex2DLayeredGrad(short4 *retVal, cudaTextureObject_t texObject, float x, float y, int layer, float2 dPdx, float2 dPdy)
{
   int4 tmp;
   asm volatile ("tex.grad.a2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], {%8, %9}, {%10, %11};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(dPdx.x), "f"(dPdx.y), "f"(dPdy.x), "f"(dPdy.y));
   *retVal = make_short4(tmp.x, tmp.y, tmp.z, tmp.w);
}

static __forceinline__ __device__ void tex2DLayeredGrad(unsigned short *retVal, cudaTextureObject_t texObject, float x, float y, int layer, float2 dPdx, float2 dPdy)
{
   uint4 tmp;
   asm volatile ("tex.grad.a2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], {%8, %9}, {%10, %11};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(dPdx.x), "f"(dPdx.y), "f"(dPdy.x), "f"(dPdy.y));
   *retVal = (unsigned short)(tmp.x);
}

static __forceinline__ __device__ void tex2DLayeredGrad(ushort1 *retVal, cudaTextureObject_t texObject, float x, float y, int layer, float2 dPdx, float2 dPdy)
{
   uint4 tmp;
   asm volatile ("tex.grad.a2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], {%8, %9}, {%10, %11};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(dPdx.x), "f"(dPdx.y), "f"(dPdy.x), "f"(dPdy.y));
   *retVal = make_ushort1(tmp.x);
}

static __forceinline__ __device__ void tex2DLayeredGrad(ushort2 *retVal, cudaTextureObject_t texObject, float x, float y, int layer, float2 dPdx, float2 dPdy)
{
   uint4 tmp;
   asm volatile ("tex.grad.a2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], {%8, %9}, {%10, %11};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(dPdx.x), "f"(dPdx.y), "f"(dPdy.x), "f"(dPdy.y));
   *retVal = make_ushort2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex2DLayeredGrad(ushort4 *retVal, cudaTextureObject_t texObject, float x, float y, int layer, float2 dPdx, float2 dPdy)
{
   uint4 tmp;
   asm volatile ("tex.grad.a2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], {%8, %9}, {%10, %11};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(dPdx.x), "f"(dPdx.y), "f"(dPdy.x), "f"(dPdy.y));
   *retVal = make_ushort4(tmp.x, tmp.y, tmp.z, tmp.w);
}

/*******************************************************************************
*                                                                              *
*                                                                              *
*                                                                              *
*******************************************************************************/

static __forceinline__ __device__ void tex2DLayeredGrad(int *retVal, cudaTextureObject_t texObject, float x, float y, int layer, float2 dPdx, float2 dPdy)
{
   int4 tmp;
   asm volatile ("tex.grad.a2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], {%8, %9}, {%10, %11};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(dPdx.x), "f"(dPdx.y), "f"(dPdy.x), "f"(dPdy.y));
   *retVal = (int)(tmp.x);
}

static __forceinline__ __device__ void tex2DLayeredGrad(int1 *retVal, cudaTextureObject_t texObject, float x, float y, int layer, float2 dPdx, float2 dPdy)
{
   int4 tmp;
   asm volatile ("tex.grad.a2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], {%8, %9}, {%10, %11};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(dPdx.x), "f"(dPdx.y), "f"(dPdy.x), "f"(dPdy.y));
   *retVal = make_int1(tmp.x);
}

static __forceinline__ __device__ void tex2DLayeredGrad(int2 *retVal, cudaTextureObject_t texObject, float x, float y, int layer, float2 dPdx, float2 dPdy)
{
   int4 tmp;
   asm volatile ("tex.grad.a2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], {%8, %9}, {%10, %11};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(dPdx.x), "f"(dPdx.y), "f"(dPdy.x), "f"(dPdy.y));
   *retVal = make_int2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex2DLayeredGrad(int4 *retVal, cudaTextureObject_t texObject, float x, float y, int layer, float2 dPdx, float2 dPdy)
{
   int4 tmp;
   asm volatile ("tex.grad.a2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], {%8, %9}, {%10, %11};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(dPdx.x), "f"(dPdx.y), "f"(dPdy.x), "f"(dPdy.y));
   *retVal = make_int4(tmp.x, tmp.y, tmp.z, tmp.w);
}

static __forceinline__ __device__ void tex2DLayeredGrad(unsigned int *retVal, cudaTextureObject_t texObject, float x, float y, int layer, float2 dPdx, float2 dPdy)
{
   uint4 tmp;
   asm volatile ("tex.grad.a2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], {%8, %9}, {%10, %11};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(dPdx.x), "f"(dPdx.y), "f"(dPdy.x), "f"(dPdy.y));
   *retVal = (unsigned int)(tmp.x);
}

static __forceinline__ __device__ void tex2DLayeredGrad(uint1 *retVal, cudaTextureObject_t texObject, float x, float y, int layer, float2 dPdx, float2 dPdy)
{
   uint4 tmp;
   asm volatile ("tex.grad.a2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], {%8, %9}, {%10, %11};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(dPdx.x), "f"(dPdx.y), "f"(dPdy.x), "f"(dPdy.y));
   *retVal = make_uint1(tmp.x);
}

static __forceinline__ __device__ void tex2DLayeredGrad(uint2 *retVal, cudaTextureObject_t texObject, float x, float y, int layer, float2 dPdx, float2 dPdy)
{
   uint4 tmp;
   asm volatile ("tex.grad.a2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], {%8, %9}, {%10, %11};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(dPdx.x), "f"(dPdx.y), "f"(dPdy.x), "f"(dPdy.y));
   *retVal = make_uint2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex2DLayeredGrad(uint4 *retVal, cudaTextureObject_t texObject, float x, float y, int layer, float2 dPdx, float2 dPdy)
{
   uint4 tmp;
   asm volatile ("tex.grad.a2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], {%8, %9}, {%10, %11};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(dPdx.x), "f"(dPdx.y), "f"(dPdy.x), "f"(dPdy.y));
   *retVal = make_uint4(tmp.x, tmp.y, tmp.z, tmp.w);
}

/*******************************************************************************
*                                                                              *
*                                                                              *
*                                                                              *
*******************************************************************************/

#if !defined(__LP64__)

static __forceinline__ __device__ void tex2DLayeredGrad(long *retVal, cudaTextureObject_t texObject, float x, float y, int layer, float2 dPdx, float2 dPdy)
{
   int4 tmp;
   asm volatile ("tex.grad.a2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], {%8, %9}, {%10, %11};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(dPdx.x), "f"(dPdx.y), "f"(dPdy.x), "f"(dPdy.y));
   *retVal = (long)(tmp.x);
}

static __forceinline__ __device__ void tex2DLayeredGrad(long1 *retVal, cudaTextureObject_t texObject, float x, float y, int layer, float2 dPdx, float2 dPdy)
{
   int4 tmp;
   asm volatile ("tex.grad.a2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], {%8, %9}, {%10, %11};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(dPdx.x), "f"(dPdx.y), "f"(dPdy.x), "f"(dPdy.y));
   *retVal = make_long1(tmp.x);
}

static __forceinline__ __device__ void tex2DLayeredGrad(long2 *retVal, cudaTextureObject_t texObject, float x, float y, int layer, float2 dPdx, float2 dPdy)
{
   int4 tmp;
   asm volatile ("tex.grad.a2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], {%8, %9}, {%10, %11};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(dPdx.x), "f"(dPdx.y), "f"(dPdy.x), "f"(dPdy.y));
   *retVal = make_long2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex2DLayeredGrad(long4 *retVal, cudaTextureObject_t texObject, float x, float y, int layer, float2 dPdx, float2 dPdy)
{
   int4 tmp;
   asm volatile ("tex.grad.a2d.v4.s32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], {%8, %9}, {%10, %11};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(dPdx.x), "f"(dPdx.y), "f"(dPdy.x), "f"(dPdy.y));
   *retVal = make_long4(tmp.x, tmp.y, tmp.z, tmp.w);
}

static __forceinline__ __device__ void tex2DLayeredGrad(unsigned long *retVal, cudaTextureObject_t texObject, float x, float y, int layer, float2 dPdx, float2 dPdy)
{
   uint4 tmp;
   asm volatile ("tex.grad.a2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], {%8, %9}, {%10, %11};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(dPdx.x), "f"(dPdx.y), "f"(dPdy.x), "f"(dPdy.y));
   *retVal = (unsigned long)(tmp.x);
}

static __forceinline__ __device__ void tex2DLayeredGrad(ulong1 *retVal, cudaTextureObject_t texObject, float x, float y, int layer, float2 dPdx, float2 dPdy)
{
   uint4 tmp;
   asm volatile ("tex.grad.a2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], {%8, %9}, {%10, %11};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(dPdx.x), "f"(dPdx.y), "f"(dPdy.x), "f"(dPdy.y));
   *retVal = make_ulong1(tmp.x);
}

static __forceinline__ __device__ void tex2DLayeredGrad(ulong2 *retVal, cudaTextureObject_t texObject, float x, float y, int layer, float2 dPdx, float2 dPdy)
{
   uint4 tmp;
   asm volatile ("tex.grad.a2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], {%8, %9}, {%10, %11};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(dPdx.x), "f"(dPdx.y), "f"(dPdy.x), "f"(dPdy.y));
   *retVal = make_ulong2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex2DLayeredGrad(ulong4 *retVal, cudaTextureObject_t texObject, float x, float y, int layer, float2 dPdx, float2 dPdy)
{
   uint4 tmp;
   asm volatile ("tex.grad.a2d.v4.u32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], {%8, %9}, {%10, %11};" : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(dPdx.x), "f"(dPdx.y), "f"(dPdy.x), "f"(dPdy.y));
   *retVal = make_ulong4(tmp.x, tmp.y, tmp.z, tmp.w);
}

#endif /* !__LP64__ */


/*******************************************************************************
*                                                                              *
*                                                                              *
*                                                                              *
*******************************************************************************/

static __forceinline__ __device__ void tex2DLayeredGrad(float *retVal, cudaTextureObject_t texObject, float x, float y, int layer, float2 dPdx, float2 dPdy)
{
   float4 tmp;
   asm volatile ("tex.grad.a2d.v4.f32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], {%8, %9}, {%10, %11};" : "=f"(tmp.x), "=f"(tmp.y), "=f"(tmp.z), "=f"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(dPdx.x), "f"(dPdx.y), "f"(dPdy.x), "f"(dPdy.y));
   *retVal = (float)(tmp.x);
}

static __forceinline__ __device__ void tex2DLayeredGrad(float1 *retVal, cudaTextureObject_t texObject, float x, float y, int layer, float2 dPdx, float2 dPdy)
{
   float4 tmp;
   asm volatile ("tex.grad.a2d.v4.f32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], {%8, %9}, {%10, %11};" : "=f"(tmp.x), "=f"(tmp.y), "=f"(tmp.z), "=f"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(dPdx.x), "f"(dPdx.y), "f"(dPdy.x), "f"(dPdy.y));
   *retVal = make_float1(tmp.x);
}

static __forceinline__ __device__ void tex2DLayeredGrad(float2 *retVal, cudaTextureObject_t texObject, float x, float y, int layer, float2 dPdx, float2 dPdy)
{
   float4 tmp;
   asm volatile ("tex.grad.a2d.v4.f32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], {%8, %9}, {%10, %11};" : "=f"(tmp.x), "=f"(tmp.y), "=f"(tmp.z), "=f"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(dPdx.x), "f"(dPdx.y), "f"(dPdy.x), "f"(dPdy.y));
   *retVal = make_float2(tmp.x, tmp.y);
}

static __forceinline__ __device__ void tex2DLayeredGrad(float4 *retVal, cudaTextureObject_t texObject, float x, float y, int layer, float2 dPdx, float2 dPdy)
{
   float4 tmp;
   asm volatile ("tex.grad.a2d.v4.f32.f32 {%0, %1, %2, %3}, [%4, {%5, %6, %7, %7}], {%8, %9}, {%10, %11};" : "=f"(tmp.x), "=f"(tmp.y), "=f"(tmp.z), "=f"(tmp.w) : "l"(texObject), "r"(layer), "f"(x), "f"(y), "f"(dPdx.x), "f"(dPdx.y), "f"(dPdy.x), "f"(dPdy.y));
   *retVal = make_float4(tmp.x, tmp.y, tmp.z, tmp.w);
}

/*******************************************************************************
*                                                                              *
*                                                                              *
*                                                                              *
*******************************************************************************/

template <class T>
static __forceinline__ __device__ T tex2DLayeredGrad(cudaTextureObject_t texObject, float x, float y, int layer, float2 dPdx, float2 dPdy)
{
   T ret;
   tex2DLayeredGrad(&ret, texObject, x, y, layer, dPdx, dPdy);
   return ret;
}

#endif // __CUDA_ARCH__ || __CUDA_ARCH__ >= 200

#endif // __cplusplus && __CUDACC__

#endif // __TEXTURE_INDIRECT_FUNCTIONS_H__


