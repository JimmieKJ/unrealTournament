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

#if !defined(__CUDA_INTERNAL_COMPILATION__)

#define __CUDA_INTERNAL_COMPILATION__
#define __text__
#define __surf__
#define __name__shadow_var(c, cpp) \
        #c
#define __name__text_var(c, cpp) \
        #cpp
#define __host__shadow_var(c, cpp) \
        cpp
#define __text_var(c, cpp) \
        cpp
#define __device_fun(fun) \
        #fun
#define __device_var(var) \
        #var
#define __device__text_var(c, cpp) \
        #c
#define __device__shadow_var(c, cpp) \
        #c

#if defined(_WIN32) && !defined(_WIN64)

#define __pad__(f) \
        f

#else /* _WIN32 && !_WIN64 */

#define __pad__(f)

#endif /* _WIN32 && !_WIN64 */

#include "builtin_types.h"
#include "storage_class.h"

#else /* !__CUDA_INTERNAL_COMPILATION__ */

template <typename T>
static inline T *__cudaAddressOf(T &val) 
{
    return (T *)((void *)(&(const_cast<char &>(reinterpret_cast<const volatile char &>(val)))));
}

#define __cudaRegisterBinary(X)                                                   \
        __cudaFatCubinHandle = __cudaRegisterFatBinary((void*)&__fatDeviceText); \
        { void (*callback_fp)(void **) =  (void (*)(void **))(X); (*callback_fp)(__cudaFatCubinHandle); }\
        atexit(__cudaUnregisterBinaryUtil)
        
#define __cudaRegisterVariable(handle, var, ext, size, constant, global) \
        __cudaRegisterVar(handle, (char*)&__host##var, (char*)__device##var, __name##var, ext, size, constant, global)
#define __cudaRegisterManagedVariable(handle, var, ext, size, constant, global) \
        __cudaRegisterManagedVar(handle, (void **)&__host##var, (char*)__device##var, __name##var, ext, size, constant, global)

#define __cudaRegisterGlobalTexture(handle, tex, dim, norm, ext) \
        __cudaRegisterTexture(handle, (const struct textureReference*)&tex, (const void**)(void*)__device##tex, __name##tex, dim, norm, ext)
#define __cudaRegisterGlobalSurface(handle, surf, dim, ext) \
        __cudaRegisterSurface(handle, (const struct surfaceReference*)&surf, (const void**)(void*)__device##surf, __name##surf, dim, ext)
#define __cudaRegisterEntry(handle, funptr, fun, thread_limit) \
        __cudaRegisterFunction(handle, (const char*)funptr, (char*)__device_fun(fun), #fun, -1, (uint3*)0, (uint3*)0, (dim3*)0, (dim3*)0, (int*)0)
          
#define __cudaSetupArg(arg, offset) \
        if (cudaSetupArgument((void *)(__cudaAddressOf(arg)), sizeof(arg), (size_t)offset) != cudaSuccess) \
          return
          
#define __cudaSetupArgSimple(arg, offset) \
        if (cudaSetupArgument((void *)(char *)&arg, sizeof(arg), (size_t)offset) != cudaSuccess) \
          return

#if defined(__GNUC__)
#define __cudaLaunch(fun) \
        { volatile static char *__f __attribute__((unused)); __f = fun; (void)cudaLaunch(fun); }
#else /* __GNUC__ */
#define __cudaLaunch(fun) \
        { volatile static char *__f; __f = fun; (void)cudaLaunch(fun); }
#endif /* __GNUC__ */

#if defined(__GNUC__)
#define __nv_dummy_param_ref(param) \
        { volatile static void **__ref __attribute__((unused)); __ref = (volatile void **)param; }
#else /* __GNUC__ */
#define __nv_dummy_param_ref(param) \
        { volatile static void **__ref; __ref = (volatile void **)param; }
#endif /* __GNUC__ */

static void ____nv_dummy_param_ref(void *param) __nv_dummy_param_ref(param)

#define __REGISTERFUNCNAME_CORE(X) __cudaRegisterLinkedBinary##X
#define __REGISTERFUNCNAME(X) __REGISTERFUNCNAME_CORE(X)

extern "C" {
void __REGISTERFUNCNAME( __NV_MODULE_ID ) ( void (*)(void **), void *, void *, void (*)(void *));
}

#define __TO_STRING_CORE(X) #X
#define __TO_STRING(X) __TO_STRING_CORE(X)

extern "C" {
#if defined(_WIN32)
#pragma data_seg("__nv_module_id")
  static const __declspec(allocate("__nv_module_id")) unsigned char __module_id_str[] = __TO_STRING(__NV_MODULE_ID);
#pragma data_seg()
#elif defined(__APPLE__)
  static const unsigned char __module_id_str[] __attribute__((section ("__NV_CUDA,__nv_module_id"))) = __TO_STRING(__NV_MODULE_ID);
#else
  static const unsigned char __module_id_str[] __attribute__((section ("__nv_module_id"))) = __TO_STRING(__NV_MODULE_ID);
#endif

#undef __FATIDNAME_CORE
#undef __FATIDNAME
#define __FATIDNAME_CORE(X) __fatbinwrap##X
#define __FATIDNAME(X) __FATIDNAME_CORE(X)

#define  ____cudaRegisterLinkedBinary(X) \
{ __REGISTERFUNCNAME(__NV_MODULE_ID) (( void (*)(void **))(X), (void *)&__FATIDNAME(__NV_MODULE_ID), (void *)&__module_id_str, (void (*)(void *))&____nv_dummy_param_ref); }

}

extern "C" {
extern void** CUDARTAPI __cudaRegisterFatBinary(
  void *fatCubin
);

extern void CUDARTAPI __cudaUnregisterFatBinary(
  void **fatCubinHandle
);

extern void CUDARTAPI __cudaRegisterVar(
        void **fatCubinHandle,
        char  *hostVar,
        char  *deviceAddress,
  const char  *deviceName,
        int    ext,
        int    size,
        int    constant,
        int    global
);

extern void CUDARTAPI __cudaRegisterManagedVar(
        void **fatCubinHandle,
        void **hostVarPtrAddress,
        char  *deviceAddress,
  const char  *deviceName,
        int    ext,
        int    size,
        int    constant,
        int    global
);

extern char CUDARTAPI __cudaInitModule(
        void **fatCubinHandle
);

extern void CUDARTAPI __cudaRegisterTexture(
        void                    **fatCubinHandle,
  const struct textureReference  *hostVar,
  const void                    **deviceAddress,
  const char                     *deviceName,
        int                       dim,       
        int                       norm,      
        int                        ext        
);

extern void CUDARTAPI __cudaRegisterSurface(
        void                    **fatCubinHandle,
  const struct surfaceReference  *hostVar,
  const void                    **deviceAddress,
  const char                     *deviceName,
        int                       dim,       
        int                       ext        
);

extern void CUDARTAPI __cudaRegisterFunction(
        void   **fatCubinHandle,
  const char    *hostFun,
        char    *deviceFun,
  const char    *deviceName,
        int      thread_limit,
        uint3   *tid,
        uint3   *bid,
        dim3    *bDim,
        dim3    *gDim,
        int     *wSize
);

#if defined(__APPLE__)
extern "C" int atexit(void (*)(void));

#elif  defined(__GNUC__) && !defined(__ANDROID__)
extern int atexit(void(*)(void)) throw();

#else /* __GNUC__ && !__ANDROID__ */
extern int __cdecl atexit(void(__cdecl *)(void));
#endif

}

static void **__cudaFatCubinHandle;

static void __cdecl __cudaUnregisterBinaryUtil(void)
{
  ____nv_dummy_param_ref((void *)&__cudaFatCubinHandle);
  __cudaUnregisterFatBinary(__cudaFatCubinHandle);
}

static char __nv_init_managed_rt_with_module(void **handle)
{
  return __cudaInitModule(handle);
}

#include "common_functions.h"

#if defined(__APPLE__)

#pragma options align=natural

#else /* __APPLE__ */

#pragma pack()

#if defined(_WIN32)

#pragma warning(disable: 4099)

#if !defined(_WIN64)

#pragma warning(disable: 4408)

#endif /* !_WIN64 */

#endif /* _WIN32 */

#endif /* __APPLE__ */

#endif /* !__CUDA_INTERNAL_COMPILATION__ */
