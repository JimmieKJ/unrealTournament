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

#if !defined(__HOST_CONFIG_H__)
#define __HOST_CONFIG_H__

/*******************************************************************************
*                                                                              *
*                                                                              *
*                                                                              *
*******************************************************************************/

#if defined(__CUDACC__)

/* check for host compilers that are compatible with nvcc */
#if !defined(__GNUC__) && !defined(_WIN32)

#error --- !!! UNSUPPORTED COMPILER !!! ---

#endif /* !__GNUC__ && !_WIN32 */

#if defined(__ICC)

#if __ICC != 1400 || !defined(__GNUC__) || !defined(__LP64__)

#error -- unsupported ICC configuration! Only ICC 14.0 on Linux x86_64 is supported!

#endif /* __ICC != 1400 || !__GNUC__ || !__LP64__ */

#endif /* __ICC */

#if defined(__GNUC__)

#if __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ > 8)

#error -- unsupported GNU version! gcc 4.9 and up are not supported!

#endif /* __GNUC__> 4 || (__GNUC__ == 4 && __GNUC_MINOR__ > 8) */

#endif /* __GNUC__ */

#if defined(_WIN32)

#if _MSC_VER < 1600 || _MSC_VER > 1800

#error -- unsupported Microsoft Visual Studio version! Only the versions 2010, 2012, and 2013 are supported!

#endif /* _MSC_VER < 1600 || _MSC_VER > 1800 */

#endif /* _WIN32 */

/* configure host compiler */
#if defined(__APPLE__)

#define _CRTIMP
#define __THROW

#if defined(__BLOCKS__) /* nvcc does not support closures */

#undef __BLOCKS__

#endif /* __BLOCKS__ */

#elif defined(__ANDROID__)

#define _CRTIMP
#define __THROW

#elif defined(__GNUC__)

#define _CRTIMP

#include <features.h> /* for __THROW */

#elif defined(_WIN32)

#if _MSC_VER >= 1500

#undef _USE_DECLSPECS_FOR_SAL
#define _USE_DECLSPECS_FOR_SAL \
        1

#endif /* _MSC_VER >= 1500 */

#if !defined(_CRT_NONSTDC_NO_WARNINGS)

#define _CRT_NONSTDC_NO_WARNINGS /* to suppress warnings */

#endif /* !_CRT_NONSTDC_NO_WARNINGS */

#if !defined(_CRT_SECURE_NO_WARNINGS)

#define _CRT_SECURE_NO_WARNINGS /* to suppress warnings */

#endif /* !_CRT_SECURE_NO_WARNINGS */

#if !defined(NOMINMAX)

#define NOMINMAX /* min and max are part of cuda runtime */

#endif /* !NOMINMAX */

#include <crtdefs.h> /* for _CRTIMP */

#define __THROW

#endif /* __APPLE__ */

#endif /* __CUDACC__ */

#endif /* !__HOST_CONFIG_H__ */
