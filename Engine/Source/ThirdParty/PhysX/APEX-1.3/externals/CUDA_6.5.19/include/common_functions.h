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

#if !defined(__COMMON_FUNCTIONS_H__)
#define __COMMON_FUNCTIONS_H__

/*******************************************************************************
*                                                                              *
*                                                                              *
*                                                                              *
*******************************************************************************/

#if defined(__cplusplus) && defined(__CUDACC__)

#include "builtin_types.h"
#include "host_defines.h"

#include <string.h>
#include <time.h>

extern "C"
{
extern _CRTIMP __host__ __device__ __device_builtin__ __cudart_builtin__ clock_t __cdecl clock(void) __THROW;
extern         __host__ __device__ __device_builtin__ __cudart_builtin__ void*   __cdecl memset(void*, int, size_t) __THROW;
extern         __host__ __device__ __device_builtin__ __cudart_builtin__ void*   __cdecl memcpy(void*, const void*, size_t) __THROW;
}

#if defined(__CUDA_ARCH__)

#ifndef __CUDA_INTERNAL_SKIP_CPP_HEADERS__
#include <new>
#endif

#if defined (__GNUC__)

#define STD \
        std::
        
#else /* __GNUC__ */

#define STD

#endif /* __GNUC__ */

extern         __host__ __device__ __cudart_builtin__ void*   __cdecl operator new(STD size_t, void*) throw();
extern         __host__ __device__ __cudart_builtin__ void*   __cdecl operator new[](STD size_t, void*) throw();
extern         __host__ __device__ __cudart_builtin__ void    __cdecl operator delete(void*, void*) throw();
extern         __host__ __device__ __cudart_builtin__ void    __cdecl operator delete[](void*, void*) throw();

#if __CUDA_ARCH__ >= 200

#include <stdio.h>
#include <stdlib.h>

extern "C"
{
extern _CRTIMP __host__ __device__ __device_builtin__ __cudart_builtin__ int     __cdecl printf(const char*, ...);
extern _CRTIMP __host__ __device__ __device_builtin__ __cudart_builtin__ int     __cdecl fprintf(FILE*, const char*, ...);
extern _CRTIMP __host__ __device__ __cudart_builtin__ void*   __cdecl malloc(size_t) __THROW;
extern _CRTIMP __host__ __device__ __cudart_builtin__ void    __cdecl free(void*) __THROW;

}

#include <assert.h>

extern "C"
{
#if defined(__APPLE__)
#define __builtin_expect(exp,c) (exp)
extern __host__ __device__ __cudart_builtin__ void __assert_rtn(
  const char *, const char *, int, const char *);
#elif defined(__ANDROID__)
extern __host__ __device__ __cudart_builtin__ void __assert2(
  const char *, int, const char *, const char *);
#elif defined(__GNUC__)
extern __host__ __device__ __cudart_builtin__ void __assert_fail(
  const char *, const char *, unsigned int, const char *)
  __THROW; 
#elif defined(_WIN32)
extern __host__ __device__ __cudart_builtin__ _CRTIMP void __cdecl _wassert(
  const wchar_t *, const wchar_t *, unsigned);
#endif
}

#if defined (__GNUC__)

# if __cplusplus >= 201103L
#define THROWBADALLOC 
#else
#define THROWBADALLOC  throw(STD bad_alloc)
#endif

#else /* __GNUC__ */

#define THROWBADALLOC  throw(...)

#endif /* __GNUC__ */

extern         __host__ __device__ __cudart_builtin__ void*   __cdecl operator new(STD size_t) THROWBADALLOC;
extern         __host__ __device__ __cudart_builtin__ void*   __cdecl operator new[](STD size_t) THROWBADALLOC;
extern         __host__ __device__ __cudart_builtin__ void    __cdecl operator delete(void*) throw();
extern         __host__ __device__ __cudart_builtin__ void    __cdecl operator delete[](void*) throw();

#undef THROWBADALLOC

#endif /* __CUDA_ARCH__ >= 200 */

#undef STD

#endif /* __CUDA_ARCH__ */

#endif /* __cplusplus && __CUDACC__ */

/*******************************************************************************
*                                                                              *
*                                                                              *
*                                                                              *
*******************************************************************************/
#if defined(__CUDABE__) && (__CUDA_ARCH__ >= 350)
#include "cuda_device_runtime_api.h"
#endif

#include "math_functions.h"

#endif /* !__COMMON_FUNCTIONS_H__ */

