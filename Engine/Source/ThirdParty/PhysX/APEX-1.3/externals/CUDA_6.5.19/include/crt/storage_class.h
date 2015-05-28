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

#if !defined(__STORAGE_CLASS_H__)
#define __STORAGE_CLASS_H__

#if !defined(__var_used__)

#define __var_used__

#endif /* __var_used__ */

#if !defined(__loc_sc__)

#define __loc_sc__(loc, size, sc) \
        __storage##_##sc##size##loc loc

#endif /* !__loc_sc__ */

#if !defined(__storage___device__)
#define __storage___device__ static __var_used__
#endif /* __storage___device__ */

#if !defined(__storage_extern__device__)
#define __storage_extern__device__ static __var_used__
#endif /* __storage_extern__device__ */

#if !defined(__storage_auto__device__)
#define __storage_auto__device__ @@@ COMPILER @@@ ERROR @@@
#endif /* __storage_auto__device__ */

#if !defined(__storage_static__device__)
#define __storage_static__device__ static __var_used__
#endif /* __storage_static__device__ */

#if !defined(__storage___constant__)
#define __storage___constant__ static __var_used__
#endif /* __storage___constant__ */

#if !defined(__storage_extern__constant__)
#define __storage_extern__constant__ static __var_used__
#endif /* __storage_extern__constant__ */

#if !defined(__storage_auto__constant__)
#define __storage_auto__constant__ @@@ COMPILER @@@ ERROR @@@
#endif /* __storage_auto__constant__ */

#if !defined(__storage_static__constant__)
#define __storage_static__constant__ static __var_used__
#endif /* __storage_static__constant__ */

#if !defined(__storage___shared__)
#define __storage___shared__ static __var_used__
#endif /* __storage___shared__ */

#if !defined(__storage_extern__shared__)
#define __storage_extern__shared__ static __var_used__
#endif /* __storage_extern__shared__ */

#if !defined(__storage_auto__shared__)
#define __storage_auto__shared__ static
#endif /* __storage_auto__shared__ */

#if !defined(__storage_static__shared__)
#define __storage_static__shared__ static __var_used__
#endif /* __storage_static__shared__ */

#if !defined(__storage__unsized__shared__)
#define __storage__unsized__shared__ @@@ COMPILER @@@ ERROR @@@
#endif /* __storage__unsized__shared__ */

#if !defined(__storage_extern_unsized__shared__)
#define __storage_extern_unsized__shared__ static __var_used__
#endif /* __storage_extern_unsized__shared__ */

#if !defined(__storage_auto_unsized__shared__)
#define __storage_auto_unsized__shared__ @@@ COMPILER @@@ ERROR @@@
#endif /* __storage_auto_unsized__shared__ */

#if !defined(__storage_static_unsized__shared__)
#define __storage_static_unsized__shared__ @@@ COMPILER @@@ ERROR @@@
#endif /* __storage_static_unsized__shared__ */

#if !defined(__storage___text__)
#define __storage___text__ static __var_used__
#endif /* __storage___text__ */

#if !defined(__storage_extern__text__)
#define __storage_extern__text__ static __var_used__
#endif /* __storage_extern__text__ */

#if !defined(__storage_auto__text__)
#define __storage_auto__text__ @@@ COMPILER @@@ ERROR @@@
#endif /* __storage_auto__text__ */

#if !defined(__storage_static__text__)
#define __storage_static__text__ static __var_used__
#endif /* __storage_static__text__ */

#if !defined(__storage___surf__)
#define __storage___surf__ static __var_used__
#endif /* __storage___surf__ */

#if !defined(__storage_extern__surf__)
#define __storage_extern__surf__ static __var_used__
#endif /* __storage_extern__surf__ */

#if !defined(__storage_auto__surf__)
#define __storage_auto__surf__ @@@ COMPILER @@@ ERROR @@@
#endif /* __storage_auto__surf__ */

#if !defined(__storage_static__surf__)
#define __storage_static__surf__ static __var_used__
#endif /* __storage_static__surf__ */

#endif /* !__STORAGE_CLASS_H__ */
