/*
 * Copyright 1993-2010 NVIDIA Corporation.  All rights reserved.
 *
 * NOTICE TO USER:   
 *
 * This source code is subject to NVIDIA ownership rights under U.S. and 
 * international Copyright laws.  Users and possessors of this source code 
 * are hereby granted a nonexclusive, royalty-free license to use this code 
 * in individual and commercial software.
 *
 * NVIDIA MAKES NO REPRESENTATION ABOUT THE SUITABILITY OF THIS SOURCE 
 * CODE FOR ANY PURPOSE.  IT IS PROVIDED "AS IS" WITHOUT EXPRESS OR 
 * IMPLIED WARRANTY OF ANY KIND.  NVIDIA DISCLAIMS ALL WARRANTIES WITH 
 * REGARD TO THIS SOURCE CODE, INCLUDING ALL IMPLIED WARRANTIES OF 
 * MERCHANTABILITY, NONINFRINGEMENT, AND FITNESS FOR A PARTICULAR PURPOSE.
 * IN NO EVENT SHALL NVIDIA BE LIABLE FOR ANY SPECIAL, INDIRECT, INCIDENTAL, 
 * OR CONSEQUENTIAL DAMAGES, OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS 
 * OF USE, DATA OR PROFITS,  WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE 
 * OR OTHER TORTIOUS ACTION,  ARISING OUT OF OR IN CONNECTION WITH THE USE 
 * OR PERFORMANCE OF THIS SOURCE CODE.  
 *
 * U.S. Government End Users.   This source code is a "commercial item" as 
 * that term is defined at  48 C.F.R. 2.101 (OCT 1995), consisting  of 
 * "commercial computer  software"  and "commercial computer software 
 * documentation" as such terms are  used in 48 C.F.R. 12.212 (SEPT 1995) 
 * and is provided to the U.S. Government only as a commercial end item.  
 * Consistent with 48 C.F.R.12.212 and 48 C.F.R. 227.7202-1 through 
 * 227.7202-4 (JUNE 1995), all U.S. Government End Users acquire the 
 * source code with only those rights set forth herein. 
 *
 * Any use of this source code in individual and commercial software must 
 * include, in the user documentation and internal comments to the code,
 * the above Disclaimer and U.S. Government End Users Notice.
 */

#define __CUDA_INTERNAL_COMPILATION__

#include "host_defines.h"

#define __no_sc__


#if defined(__CUDANVVM__)
typedef __device_builtin_texture_type__ unsigned long long __texture_type__;
typedef __device_builtin_surface_type__ unsigned long long __surface_type__;
#else /* __CUDANVVM__ */
typedef __device_builtin_texture_type__ const void *__texture_type__;
typedef __device_builtin_surface_type__ const void *__surface_type__;
#endif /* __CUDANVVM__ */

#if defined(__CUDABE__) /* cudabe compiler */

#if __CUDA_ARCH__ >= 200

#define ___device__(sc) \
        sc

#else /* __CUDA_ARCH__ >= 200 */

#define ___device__(sc) \
        static

#endif /* __CUDA_ARCH__ >= 200 */

#define __text__ \
        __attribute__((__texture__))
#define __surf__ \
        __attribute__((__surface__))
#define __val_param(name) \
        __val_param##name
#define __copy_param(local_decl, param) \
        local_decl = param
#define __var_used__ \
        __attribute__((__used__))
#define __storage_extern_unsized__shared__ \
        extern
#define __cxa_vec_ctor(n, num, size, c, d) \
        ({ int i; for (i = 0; i < num; i++) c((void*)((unsigned char *)n + i*size)); (void)0; })
#define __cxa_vec_cctor(dest, src, num, size, c, d) \
        ({ int i; for (i = 0; i < num; i++) \
          c((void*)((unsigned char *)dest + i*size), (void*)((unsigned char *)src + i*size)); (void)0; })
#define __cxa_vec_dtor(n, num, size, d) \
        { int i; for (i = num-1; i >= 0; i--) d((void*)((unsigned char *)n + i*size)); }
#define __cxa_vec_new2(num, size, pad, c, d, m, f) \
        ({unsigned char *t = m(num*size + pad); *(size_t*)t = num; \
          (void)__cxa_vec_ctor((void *)(t+pad), num, size, c, d); (void *)(t+pad);})
#define __cxa_vec_new3(num, size, pad, c, d, m, f) \
        __cxa_vec_new2(num, size, pad, c, d, m, f)
#define __cxa_vec_delete2(n, size, pad, d, f) \
        { unsigned char *ptr = (unsigned char *)(n); \
          if (ptr) { \
            unsigned char *t = ptr - pad; size_t num = *(size_t*)t; \
            __cxa_vec_dtor(ptr, num, size, d); f((void *)t); \
          } \
        }
#define __cxa_vec_delete(n, size, pad, d) \
        __cxa_vec_delete2(n, size, pad, d, free)
#define __cxa_vec_delete3(n, size, pad, d, f) \
        { unsigned char *ptr = (unsigned char *)(n); \
          if (ptr) { \
            unsigned char *t = ptr - pad; size_t num = *(size_t*)t; \
            size_t tsize = num*size+pad; \
            __cxa_vec_dtor(ptr, num, size, d); f((void *)t, tsize); \
          } \
        }

#define __gen_nvvm_memcpy_aligned1(dest, src, size) \
({ \
  __nvvm_memcpy(dest, src, size, 1); \
  (void *)(dest); \
})
#define __gen_nvvm_memcpy_aligned2(dest, src, size) \
({ \
  if (((unsigned long long)(dest) % 2) == 0 && ((unsigned long long)(src) % 2) == 0) \
    __nvvm_memcpy(dest, src, size, 2); \
  else \
    __nvvm_memcpy(dest, src, size, 1); \
  (void *)(dest); \
})
#define __gen_nvvm_memcpy_aligned4(dest, src, size) \
({ \
  if (((unsigned long long)(dest) % 4) == 0 && ((unsigned long long)(src) % 4) == 0) \
    __nvvm_memcpy(dest, src, size, 4); \
  else \
    __nvvm_memcpy(dest, src, size, 1); \
  (void *)(dest); \
})
#define __gen_nvvm_memcpy_aligned8(dest, src, size) \
({ \
  if (((unsigned long long)(dest) % 8) == 0 && ((unsigned long long)(src) % 8) == 0) \
    __nvvm_memcpy(dest, src, size, 8); \
  else \
    __nvvm_memcpy(dest, src, size, 1); \
  (void *)(dest); \
})
#define __gen_nvvm_memcpy_aligned16(dest, src, size) \
({ \
  if (((unsigned long long)(dest) % 16) == 0 && ((unsigned long long)(src) % 16) == 0) \
    __nvvm_memcpy(dest, src, size, 16); \
  else \
    __nvvm_memcpy(dest, src, size, 1); \
  (void *)(dest); \
})

#undef __cdecl
#define __cdecl
#undef __w64
#define __w64

#elif defined(__CUDACC__) /* cudafe compiler */

#define __loc_sc__(loc, size, sc) \
        sc loc
#define __text__
#define __surf__
#define ___device__(sc) \
        sc __device__
#define __val_param(name) \
        name
#define __copy_param(local_decl, param)
#define _Znwm \
        malloc
#define _Znwj \
        malloc
#define _Znwy \
        malloc
#define _Znam \
        malloc
#define _Znaj \
        malloc
#define _Znay \
        malloc
#define _ZdlPv \
        free
#define _ZdaPv \
        free
#define __cxa_pure_virtual \
        0

extern __device__ void* malloc(size_t);
extern __device__ void free(void*);

extern __device__ void __assertfail(
  const void  *message,
  const void  *file,
  unsigned int line,
  const void  *function,
  size_t       charsize);

#if defined(__APPLE__)
static __device__ void __assert_rtn(
  const char  *__function,
  const char  *__file,
  int          __line,
  const char  *__assertion)
{
  __assertfail(
    (const void *)__assertion,
    (const void *)__file,
    (unsigned int)__line,
    (const void *)__function,
    sizeof(char));
}
#elif defined(__ANDROID__)
static __device__ void __assert2(
  const char *__file,
  int         __line,
  const char *__function,
  const char *__assertion)
{
  __assertfail(
    (const void *)__assertion,
    (const void *)__file,
    (unsigned int)__line,
    (const void *)__function,
    sizeof(char));
}
#elif defined(__GNUC__)
static __device__ void __assert_fail(
  const char  *__assertion,
  const char  *__file,
  unsigned int __line,
  const char  *__function)
{
  __assertfail(
    (const void *)__assertion,
    (const void *)__file,
                  __line,
    (const void *)__function,
    sizeof(char));
}
#elif defined(_WIN32)
static __device__ void _wassert(
  const unsigned short *_Message,
  const unsigned short *_File,
  unsigned              _Line)
{
  __assertfail(
    (const void *)_Message,
    (const void *)_File,
                  _Line,
    (const void *)0,
    sizeof(unsigned short));
}
#endif

#endif /* __CUDABE__ */

#include "builtin_types.h"
#include "device_launch_parameters.h"
#include "storage_class.h"
