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

#if !defined(__SM_32_INTRINSICS_H__)
#define __SM_32_INTRINSICS_H__

#if defined(__cplusplus) && defined(__CUDACC__)

#if !defined(__CUDA_ARCH__) || __CUDA_ARCH__ >= 320

/*******************************************************************************
*                                                                              *
*                                                                              *
*                                                                              *
*******************************************************************************/

#include "builtin_types.h"
#include "device_types.h"
#include "host_defines.h"

// In here are intrinsics which are built in to the compiler. These may be
// referenced by intrinsic implementations from this file.
extern "C"
{
    // There are no intrinsics built in to the compiler for SM-3.5,
    // all intrinsics are now implemented as inline PTX below.
}

/*******************************************************************************
*                                                                              *
*  Below are implementations of SM-3.5 intrinsics which are included as        *
*  source (instead of being built in to the compiler)                          *
*                                                                              *
*******************************************************************************/

// LDG is a "load from global via texture path" command which can exhibit higher
// bandwidth on GK110 than a regular LD.
// Define a different pointer storage size for 64 and 32 bit
#if (defined(_MSC_VER) && defined(_WIN64)) || defined(__LP64__)
#define __LDG_PTR   "l"
#else
#define __LDG_PTR   "r"
#endif

// Size of long is architecture and OS specific.
#if defined(__LP64__) // 64 bits
static __device__ __inline__ long __ldg(const long *ptr) { unsigned long ret; asm volatile ("ld.global.nc.s64 %0, [%1];"  : "=l"(ret) : __LDG_PTR (ptr)); return (long)ret; }
static __device__ __inline__ unsigned long __ldg(const unsigned long *ptr) { unsigned long ret; asm volatile ("ld.global.nc.u64 %0, [%1];"  : "=l"(ret) : __LDG_PTR (ptr)); return ret; }
#else // 32 bits
static __device__ __inline__ long __ldg(const long *ptr) { unsigned long ret; asm volatile ("ld.global.nc.s32 %0, [%1];"  : "=r"(ret) : __LDG_PTR (ptr)); return (long)ret; }
static __device__ __inline__ unsigned long __ldg(const unsigned long *ptr) { unsigned long ret; asm volatile ("ld.global.nc.u32 %0, [%1];"  : "=r"(ret) : __LDG_PTR (ptr)); return ret; }
#endif


static __device__ __inline__ char __ldg(const char *ptr) { unsigned int ret; asm volatile ("ld.global.nc.s8 %0, [%1];"  : "=r"(ret) : __LDG_PTR (ptr)); return (char)ret; }
static __device__ __inline__ short __ldg(const short *ptr) { unsigned short ret; asm volatile ("ld.global.nc.s16 %0, [%1];"  : "=h"(ret) : __LDG_PTR (ptr)); return (short)ret; }
static __device__ __inline__ int __ldg(const int *ptr) { unsigned int ret; asm volatile ("ld.global.nc.s32 %0, [%1];"  : "=r"(ret) : __LDG_PTR (ptr)); return (int)ret; }
static __device__ __inline__ long long __ldg(const long long *ptr) { unsigned long long ret; asm volatile ("ld.global.nc.s64 %0, [%1];"  : "=l"(ret) : __LDG_PTR (ptr)); return (long long)ret; }
static __device__ __inline__ char2 __ldg(const char2 *ptr) { char2 ret; int2 tmp; asm volatile ("ld.global.nc.v2.s8 {%0,%1}, [%2];"  : "=r"(tmp.x), "=r"(tmp.y) : __LDG_PTR (ptr)); ret.x = (char)tmp.x; ret.y = (char)tmp.y; return ret; }
static __device__ __inline__ char4 __ldg(const char4 *ptr) { char4 ret; int4 tmp; asm volatile ("ld.global.nc.v4.s8 {%0,%1,%2,%3}, [%4];"  : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : __LDG_PTR (ptr)); ret.x = (char)tmp.x; ret.y = (char)tmp.y; ret.z = (char)tmp.z; ret.w = (char)tmp.w; return ret; }
static __device__ __inline__ short2 __ldg(const short2 *ptr) { short2 ret; asm volatile ("ld.global.nc.v2.s16 {%0,%1}, [%2];"  : "=h"(ret.x), "=h"(ret.y) : __LDG_PTR (ptr)); return ret; }
static __device__ __inline__ short4 __ldg(const short4 *ptr) { short4 ret; asm volatile ("ld.global.nc.v4.s16 {%0,%1,%2,%3}, [%4];"  : "=h"(ret.x), "=h"(ret.y), "=h"(ret.z), "=h"(ret.w) : __LDG_PTR (ptr)); return ret; }
static __device__ __inline__ int2 __ldg(const int2 *ptr) { int2 ret; asm volatile ("ld.global.nc.v2.s32 {%0,%1}, [%2];"  : "=r"(ret.x), "=r"(ret.y) : __LDG_PTR (ptr)); return ret; }
static __device__ __inline__ int4 __ldg(const int4 *ptr) { int4 ret; asm volatile ("ld.global.nc.v4.s32 {%0,%1,%2,%3}, [%4];"  : "=r"(ret.x), "=r"(ret.y), "=r"(ret.z), "=r"(ret.w) : __LDG_PTR (ptr)); return ret; }
static __device__ __inline__ longlong2 __ldg(const longlong2 *ptr) { longlong2 ret; asm volatile ("ld.global.nc.v2.s64 %0, [%1];"  : "=l"(ret.x), "=l"(ret.y) : __LDG_PTR (ptr)); return ret; }

static __device__ __inline__ unsigned char __ldg(const unsigned char *ptr) { unsigned int ret; asm volatile ("ld.global.nc.u8 %0, [%1];"  : "=r"(ret) : __LDG_PTR (ptr));  return (unsigned char)ret; }
static __device__ __inline__ unsigned short __ldg(const unsigned short *ptr) { unsigned short ret; asm volatile ("ld.global.nc.u16 %0, [%1];"  : "=h"(ret) : __LDG_PTR (ptr)); return ret; }
static __device__ __inline__ unsigned int __ldg(const unsigned int *ptr) { unsigned int ret; asm volatile ("ld.global.nc.u32 %0, [%1];"  : "=r"(ret) : __LDG_PTR (ptr)); return ret; }
static __device__ __inline__ unsigned long long __ldg(const unsigned long long *ptr) { unsigned long long ret; asm volatile ("ld.global.nc.u64 %0, [%1];"  : "=l"(ret) : __LDG_PTR (ptr)); return ret; }
static __device__ __inline__ uchar2 __ldg(const uchar2 *ptr) { uchar2 ret; uint2 tmp; asm volatile ("ld.global.nc.v2.u8 {%0,%1}, [%2];"  : "=r"(tmp.x), "=r"(tmp.y) : __LDG_PTR (ptr)); ret.x = (unsigned char)tmp.x; ret.y = (unsigned char)tmp.y; return ret; }
static __device__ __inline__ uchar4 __ldg(const uchar4 *ptr) { uchar4 ret; uint4 tmp; asm volatile ("ld.global.nc.v4.u8 {%0,%1,%2,%3}, [%4];"  : "=r"(tmp.x), "=r"(tmp.y), "=r"(tmp.z), "=r"(tmp.w) : __LDG_PTR (ptr)); ret.x = (unsigned char)tmp.x; ret.y = (unsigned char)tmp.y; ret.z = (unsigned char)tmp.z; ret.w = (unsigned char)tmp.w; return ret; }
static __device__ __inline__ ushort2 __ldg(const ushort2 *ptr) { ushort2 ret; asm volatile ("ld.global.nc.v2.u16 {%0,%1}, [%2];"  : "=h"(ret.x), "=h"(ret.y) : __LDG_PTR (ptr)); return ret; }
static __device__ __inline__ ushort4 __ldg(const ushort4 *ptr) { ushort4 ret; asm volatile ("ld.global.nc.v4.u16 {%0,%1,%2,%3}, [%4];"  : "=h"(ret.x), "=h"(ret.y), "=h"(ret.z), "=h"(ret.w) : __LDG_PTR (ptr)); return ret; }
static __device__ __inline__ uint2 __ldg(const uint2 *ptr) { uint2 ret; asm volatile ("ld.global.nc.v2.u32 {%0,%1}, [%2];"  : "=r"(ret.x), "=r"(ret.y) : __LDG_PTR (ptr)); return ret; }
static __device__ __inline__ uint4 __ldg(const uint4 *ptr) { uint4 ret; asm volatile ("ld.global.nc.v4.u32 {%0,%1,%2,%3}, [%4];"  : "=r"(ret.x), "=r"(ret.y), "=r"(ret.z), "=r"(ret.w) : __LDG_PTR (ptr)); return ret; }
static __device__ __inline__ ulonglong2 __ldg(const ulonglong2 *ptr) { ulonglong2 ret; asm volatile ("ld.global.nc.v2.u64 %0, [%1];"  : "=l"(ret.x), "=l"(ret.y) : __LDG_PTR (ptr)); return ret; }

static __device__ __inline__ float __ldg(const float *ptr) { float ret; asm volatile ("ld.global.nc.f32 %0, [%1];"  : "=f"(ret) : __LDG_PTR (ptr)); return ret; }
static __device__ __inline__ double __ldg(const double *ptr) { double ret; asm volatile ("ld.global.nc.f64 %0, [%1];"  : "=d"(ret) : __LDG_PTR (ptr)); return ret; }
static __device__ __inline__ float2 __ldg(const float2 *ptr) { float2 ret; asm volatile ("ld.global.nc.v2.f32 {%0,%1}, [%2];"  : "=f"(ret.x), "=f"(ret.y) : __LDG_PTR (ptr)); return ret; }
static __device__ __inline__ float4 __ldg(const float4 *ptr) { float4 ret; asm volatile ("ld.global.nc.v4.f32 {%0,%1,%2,%3}, [%4];"  : "=f"(ret.x), "=f"(ret.y), "=f"(ret.z), "=f"(ret.w) : __LDG_PTR (ptr)); return ret; }
static __device__ __inline__ double2 __ldg(const double2 *ptr) { double2 ret; asm volatile ("ld.global.nc.v2.f64 {%0,%1}, [%2];"  : "=d"(ret.x), "=d"(ret.y) : __LDG_PTR (ptr)); return ret; }

#undef __LDG_PTR


// SHF is the "funnel shift" operation - an accelerated left/right shift with carry
// operating on 64-bit quantities, which are concatenations of two 32-bit registers.

// This shifts [b:a] left by "shift" bits, returning the most significant bits of the result.
static __device__ inline unsigned int __funnelshift_l(unsigned int lo, unsigned int hi, unsigned int shift)
{
    unsigned int ret;
    asm volatile ("shf.l.wrap.b32 %0, %1, %2, %3;" : "=r"(ret) : "r"(lo), "r"(hi), "r"(shift));
    return ret;
}
static __device__ inline unsigned int __funnelshift_lc(unsigned int lo, unsigned int hi, unsigned int shift)
{
    unsigned int ret;
    asm volatile ("shf.l.clamp.b32 %0, %1, %2, %3;" : "=r"(ret) : "r"(lo), "r"(hi), "r"(shift));
    return ret;
}

// This shifts [b:a] right by "shift" bits, returning the least significant bits of the result.
static __device__ inline unsigned int __funnelshift_r(unsigned int lo, unsigned int hi, unsigned int shift)
{
    unsigned int ret;
    asm volatile ("shf.r.wrap.b32 %0, %1, %2, %3;" : "=r"(ret) : "r"(lo), "r"(hi), "r"(shift));
    return ret;
}
static __device__ inline unsigned int __funnelshift_rc(unsigned int lo, unsigned int hi, unsigned int shift)
{
    unsigned int ret;
    asm volatile ("shf.r.clamp.b32 %0, %1, %2, %3;" : "=r"(ret) : "r"(lo), "r"(hi), "r"(shift));
    return ret;
}


#endif /* !__CUDA_ARCH__ || __CUDA_ARCH__ >= 320 */

#endif /* __cplusplus && __CUDACC__ */

#endif /* !__SM_32_INTRINSICS_H__ */
