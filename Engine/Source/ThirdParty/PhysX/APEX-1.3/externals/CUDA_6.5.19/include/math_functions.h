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

#if !defined(__MATH_FUNCTIONS_H__)
#define __MATH_FUNCTIONS_H__

/**
 * \defgroup CUDA_MATH Mathematical Functions
 *
 * CUDA mathematical functions are always available in device code.
 * Some functions are also available in host code as indicated.
 *
 * Note that floating-point functions are overloaded for different
 * argument types.  For example, the ::log() function has the following
 * prototypes:
 * \code
 * double log(double x);
 * float log(float x);
 * float logf(float x);
 * \endcode
 */

/*******************************************************************************
*                                                                              *
*                                                                              *
*                                                                              *
*******************************************************************************/

#if defined(__cplusplus) && defined(__CUDACC__)

/*******************************************************************************
*                                                                              *
*                                                                              *
*                                                                              *
*******************************************************************************/

#include "builtin_types.h"
#include "host_defines.h"

/*******************************************************************************
*                                                                              *
*                                                                              *
*                                                                              *
*******************************************************************************/

extern "C"
{

/* Define math function DOXYGEN toplevel groups, functions will
   be added to these groups later.
*/
/**
 * \defgroup CUDA_MATH_SINGLE Single Precision Mathematical Functions
 * This section describes single precision mathematical functions.
 */

/**
 * \defgroup CUDA_MATH_DOUBLE Double Precision Mathematical Functions
 * This section describes double precision mathematical functions.
 */

/**
 * \defgroup CUDA_MATH_INTRINSIC_SINGLE Single Precision Intrinsics
 * This section describes single precision intrinsic functions that are
 * only supported in device code.
 */

/**
 * \defgroup CUDA_MATH_INTRINSIC_DOUBLE Double Precision Intrinsics
 * This section describes double precision intrinsic functions that are
 * only supported in device code.
 */

/**
 * \defgroup CUDA_MATH_INTRINSIC_INT Integer Intrinsics
 * This section describes integer intrinsic functions that are
 * only supported in device code.
 */

/**
 * \defgroup CUDA_MATH_INTRINSIC_CAST Type Casting Intrinsics
 * This section describes type casting intrinsic functions that are
 * only supported in device code.
 */

/**
 *
 * \defgroup CUDA_MATH_INTRINSIC_SIMD SIMD Intrinsics
 * This section describes SIMD intrinsic functions that are
 * only supported in device code.
 */


/**
 * @}
 */

#if defined(__ANDROID__) && !defined(__aarch64__)
static __host__ __device__ __device_builtin__ __cudart_builtin__ int                    abs(int);
static __host__ __device__ __device_builtin__ __cudart_builtin__ long int               labs(long int);
static __host__ __device__ __device_builtin__ __cudart_builtin__ long long int          llabs(long long int);
#else /* __ANDROID__ */
extern __host__ __device__ __device_builtin__ __cudart_builtin__ int            __cdecl abs(int) __THROW;
extern __host__ __device__ __device_builtin__ __cudart_builtin__ long int       __cdecl labs(long int) __THROW;
extern __host__ __device__ __device_builtin__ __cudart_builtin__ long long int          llabs(long long int) __THROW;
#endif /* __ANDROID__ */

/**
 * \ingroup CUDA_MATH_DOUBLE
 * \brief Calculate the absolute value of the input argument.
 *
 * Calculate the absolute value of the input argument \p x.
 *
 * \return
 * Returns the absolute value of the input argument.
 * - fabs(
 * \latexonly $\pm \infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns 
 * \latexonly $+\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>+</m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 * - fabs(
 * \latexonly $\pm 0$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mn>0</m:mn>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns 0.
 * \note_accuracy_double
 */
extern __host__ __device__ __device_builtin__ double         __cdecl fabs(double x) __THROW;
/**
 * \ingroup CUDA_MATH_SINGLE
 * \brief Calculate the absolute value of its argument
 *
 * Calculate the absolute value of the input argument \p x.
 *
 * \return
 * Returns the absolute value of its argument.
 * - fabs(
 * \latexonly $\pm \infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns 
 * \latexonly $+\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>+</m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 * - fabs(
 * \latexonly $\pm 0$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mn>0</m:mn>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns 0.
 * \note_accuracy_single
 */
extern __host__ __device__ __device_builtin__ float                  fabsf(float x) __THROW;
extern __host__ __device__ __device_builtin__ int                    min(int, int);
extern __host__ __device__ __device_builtin__ unsigned int           umin(unsigned int, unsigned int);
extern __host__ __device__ __device_builtin__ long long int          llmin(long long int, long long int);
extern __host__ __device__ __device_builtin__ unsigned long long int ullmin(unsigned long long int, unsigned long long int);
/**
 * \ingroup CUDA_MATH_SINGLE
 * \brief Determine the minimum numeric value of the arguments.
 *
 * Determines the minimum numeric value of the arguments \p x and \p y. Treats NaN 
 * arguments as missing data. If one argument is a NaN and the other is legitimate numeric
 * value, the numeric value is chosen.
 *
 * \return
 * Returns the minimum numeric values of the arguments \p x and \p y.
 * - If both arguments are NaN, returns NaN.
 * - If one argument is NaN, returns the numeric argument.
 *
 * \note_accuracy_single
 */
#if _MSC_VER < 1800
extern __host__ __device__ __device_builtin__ float                  fminf(float x, float y) __THROW;
#else /* _MSC_VER < 1800 */
extern __host__ __device__ __device_builtin__ _CRTIMP float  __cdecl fminf(float x, float y);
#endif /* _MSC_VER < 1800 */
/**
 * \ingroup CUDA_MATH_DOUBLE
 * \brief Determine the minimum numeric value of the arguments.
 *
 * Determines the minimum numeric value of the arguments \p x and \p y. Treats NaN 
 * arguments as missing data. If one argument is a NaN and the other is legitimate numeric
 * value, the numeric value is chosen.
 *
 * \return
 * Returns the minimum numeric values of the arguments \p x and \p y.
 * - If both arguments are NaN, returns NaN.
 * - If one argument is NaN, returns the numeric argument.
 *
 * \note_accuracy_double
 */
#if _MSC_VER < 1800
extern __host__ __device__ __device_builtin__ double                 fmin(double x, double y) __THROW;
#else /* _MSC_VER < 1800 */
extern __host__ __device__ __device_builtin__ _CRTIMP double __cdecl fmin(double x, double y);
#endif /* _MSC_VER < 1800 */
extern __host__ __device__ __device_builtin__ int                    max(int, int);
extern __host__ __device__ __device_builtin__ unsigned int           umax(unsigned int, unsigned int);
extern __host__ __device__ __device_builtin__ long long int          llmax(long long int, long long int);
extern __host__ __device__ __device_builtin__ unsigned long long int ullmax(unsigned long long int, unsigned long long int);
/**
 * \ingroup CUDA_MATH_SINGLE
 * \brief Determine the maximum numeric value of the arguments.
 *
 * Determines the maximum numeric value of the arguments \p x and \p y. Treats NaN 
 * arguments as missing data. If one argument is a NaN and the other is legitimate numeric
 * value, the numeric value is chosen.
 *
 * \return
 * Returns the maximum numeric values of the arguments \p x and \p y.
 * - If both arguments are NaN, returns NaN.
 * - If one argument is NaN, returns the numeric argument.
 *
 * \note_accuracy_single
 */
#if _MSC_VER < 1800
extern __host__ __device__ __device_builtin__ float                  fmaxf(float x, float y) __THROW;
#else /* _MSC_VER < 1800 */
extern __host__ __device__ __device_builtin__ _CRTIMP float  __cdecl fmaxf(float x, float y);
#endif /* _MSC_VER < 1800 */
/**
 * \ingroup CUDA_MATH_DOUBLE
 * \brief Determine the maximum numeric value of the arguments.
 *
 * Determines the maximum numeric value of the arguments \p x and \p y. Treats NaN 
 * arguments as missing data. If one argument is a NaN and the other is legitimate numeric
 * value, the numeric value is chosen.
 *
 * \return
 * Returns the maximum numeric values of the arguments \p x and \p y.
 * - If both arguments are NaN, returns NaN.
 * - If one argument is NaN, returns the numeric argument.
 *
 * \note_accuracy_double
 */
#if _MSC_VER < 1800
extern __host__ __device__ __device_builtin__ double                 fmax(double, double) __THROW;
#else /* _MSC_VER < 1800 */
extern __host__ __device__ __device_builtin__ _CRTIMP double __cdecl fmax(double, double);
#endif /* _MSC_VER < 1800 */
/**
 * \ingroup CUDA_MATH_DOUBLE
 * \brief Calculate the sine of the input argument.
 *
 * Calculate the sine of the input argument \p x (measured in radians).
 *
 * \return 
 * - sin(
 * \latexonly $\pm 0$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mn>0</m:mn>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns 
 * \latexonly $\pm 0$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mn>0</m:mn>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 * - sin(
 * \latexonly $\pm \infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns NaN.
 *
 * \note_accuracy_double
 */
extern __host__ __device__ __device_builtin__ double         __cdecl sin(double x) __THROW;
/**
 * \ingroup CUDA_MATH_DOUBLE
 * \brief Calculate the cosine of the input argument.
 *
 * Calculate the cosine of the input argument \p x (measured in radians).
 *
 * \return 
 * - cos(
 * \latexonly $\pm 0$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mn>0</m:mn>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns 1.
 * - cos(
 * \latexonly $\pm \infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns NaN.
 *
 * \note_accuracy_double
 */
extern __host__ __device__ __device_builtin__ double         __cdecl cos(double x) __THROW;
/**
 * \ingroup CUDA_MATH_DOUBLE
 * \brief Calculate the sine and cosine of the first input argument.
 *
 * Calculate the sine and cosine of the first input argument \p x (measured 
 * in radians). The results for sine and cosine are written into the
 * second argument, \p sptr, and, respectively, third argument, \p cptr.
 *
 * \return 
 * - none
 *
 * \see ::sin() and ::cos().
 * \note_accuracy_double
 */
extern __host__ __device__ __device_builtin__ void                   sincos(double x, double *sptr, double *cptr) __THROW;
/**
 * \ingroup CUDA_MATH_SINGLE
 * \brief Calculate the sine and cosine of the first input argument.
 *
 * Calculate the sine and cosine of the first input argument \p x (measured
 * in radians). The results for sine and cosine are written into the second 
 * argument, \p sptr, and, respectively, third argument, \p cptr.
 *
 * \return 
 * - none
 *
 * \see ::sinf() and ::cosf().
 * \note_accuracy_single
 * \note_fastmath
 */
extern __host__ __device__ __device_builtin__ void                   sincosf(float x, float *sptr, float *cptr) __THROW;
/**
 * \ingroup CUDA_MATH_DOUBLE
 * \brief Calculate the tangent of the input argument.
 *
 * Calculate the tangent of the input argument \p x (measured in radians).
 *
 * \return 
 * - tan(
 * \latexonly $\pm 0$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mn>0</m:mn>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns 
 * \latexonly $\pm 0$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mn>0</m:mn>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 * - tan(
 * \latexonly $\pm \infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns NaN.
 *
 * \note_accuracy_double
 */
extern __host__ __device__ __device_builtin__ double         __cdecl tan(double x) __THROW;
/**
 * \ingroup CUDA_MATH_DOUBLE
 * \brief Calculate the square root of the input argument.
 *
 * Calculate the nonnegative square root of \p x, 
 * \latexonly $\sqrt{x}$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:msqrt>
 *     <m:mi>x</m:mi>
 *   </m:msqrt>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 *
 * \return 
 * Returns 
 * \latexonly $\sqrt{x}$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:msqrt>
 *     <m:mi>x</m:mi>
 *   </m:msqrt>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 * - sqrt(
 * \latexonly $\pm 0$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mn>0</m:mn>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns 
 * \latexonly $\pm 0$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mn>0</m:mn>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 * - sqrt(
 * \latexonly $+\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>+</m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns 
 * \latexonly $+\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>+</m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 * - sqrt(\p x) returns NaN if \p x is less than 0.
 *
 * \note_accuracy_double
 */
extern __host__ __device__ __device_builtin__ double         __cdecl sqrt(double x) __THROW;
/**
 * \ingroup CUDA_MATH_DOUBLE
 * \brief Calculate the reciprocal of the square root of the input argument.
 *
 * Calculate the reciprocal of the nonnegative square root of \p x, 
 * \latexonly $1/\sqrt{x}$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mn>1</m:mn>
 *   <m:mrow class="MJX-TeXAtom-ORD">
 *     <m:mo>/</m:mo>
 *   </m:mrow>
 *   <m:msqrt>
 *     <m:mi>x</m:mi>
 *   </m:msqrt>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 *
 * \return 
 * Returns 
 * \latexonly $1/\sqrt{x}$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mn>1</m:mn>
 *   <m:mrow class="MJX-TeXAtom-ORD">
 *     <m:mo>/</m:mo>
 *   </m:mrow>
 *   <m:msqrt>
 *     <m:mi>x</m:mi>
 *   </m:msqrt>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 * - rsqrt(
 * \latexonly $+\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>+</m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns +0.
 * - rsqrt(
 * \latexonly $\pm 0$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mn>0</m:mn>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns 
 * \latexonly $\pm \infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 * - rsqrt(\p x) returns NaN if \p x is less than 0.
 *
 * \note_accuracy_double
 */
extern __host__ __device__ __device_builtin__ double                 rsqrt(double x);
/**
 * \ingroup CUDA_MATH_SINGLE
 * \brief Calculate the reciprocal of the square root of the input argument.
 *
 * Calculate the reciprocal of the nonnegative square root of \p x, 
 * \latexonly $1/\sqrt{x}$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mn>1</m:mn>
 *   <m:mrow class="MJX-TeXAtom-ORD">
 *     <m:mo>/</m:mo>
 *   </m:mrow>
 *   <m:msqrt>
 *     <m:mi>x</m:mi>
 *   </m:msqrt>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 *
 * \return 
 * Returns 
 * \latexonly $1/\sqrt{x}$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mn>1</m:mn>
 *   <m:mrow class="MJX-TeXAtom-ORD">
 *     <m:mo>/</m:mo>
 *   </m:mrow>
 *   <m:msqrt>
 *     <m:mi>x</m:mi>
 *   </m:msqrt>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 * - rsqrtf(
 * \latexonly $+\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>+</m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns +0.
 * - rsqrtf(
 * \latexonly $\pm 0$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mn>0</m:mn>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns 
 * \latexonly $\pm \infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 * - rsqrtf(\p x) returns NaN if \p x is less than 0.
 *
 * \note_accuracy_single
 */
extern __host__ __device__ __device_builtin__ float                  rsqrtf(float x);
/**
 * \ingroup CUDA_MATH_DOUBLE
 * \brief Calculate the base 2 logarithm of the input argument.
 *
 * Calculate the base 2 logarithm of the input argument \p x.
 *
 * \return 
 * - log2(
 * \latexonly $\pm 0$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mn>0</m:mn>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns 
 * \latexonly $-\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x2212;<!-- − --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 * - log2(1) returns +0.
 * - log2(\p x) returns NaN for \p x < 0.
 * - log2(
 * \latexonly $+\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>+</m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns 
 * \latexonly $+\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>+</m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 *
 * \note_accuracy_double
 */
#if _MSC_VER < 1800
extern __host__ __device__ __device_builtin__ double                 log2(double x) __THROW;
#else /* _MSC_VER < 1800 */
extern __host__ __device__ __device_builtin__ _CRTIMP double __cdecl log2(double x);
#endif /* _MSC_VER < 1800 */
/**
 * \ingroup CUDA_MATH_DOUBLE
 * \brief Calculate the base 2 exponential of the input argument.
 *
 * Calculate the base 2 exponential of the input argument \p x.
 *
 * \return Returns 
 * \latexonly $2^x$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:msup>
 *     <m:mn>2</m:mn>
 *     <m:mi>x</m:mi>
 *   </m:msup>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 *
 * \note_accuracy_double
 */
#if _MSC_VER < 1800
extern __host__ __device__ __device_builtin__ double                 exp2(double x) __THROW;
#else /* _MSC_VER < 1800 */
extern __host__ __device__ __device_builtin__ _CRTIMP double __cdecl exp2(double x);
#endif /* _MSC_VER < 1800 */
/**
 * \ingroup CUDA_MATH_SINGLE
 * \brief Calculate the base 2 exponential of the input argument.
 *
 * Calculate the base 2 exponential of the input argument \p x.
 *
 * \return Returns 
 * \latexonly $2^x$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:msup>
 *     <m:mn>2</m:mn>
 *     <m:mi>x</m:mi>
 *   </m:msup>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 *
 * \note_accuracy_single
 */
#if _MSC_VER < 1800
extern __host__ __device__ __device_builtin__ float                  exp2f(float x) __THROW;
#else /* _MSC_VER < 1800 */
extern __host__ __device__ __device_builtin__ _CRTIMP float  __cdecl exp2f(float x);
#endif /* _MSC_VER < 1800 */
/**
 * \ingroup CUDA_MATH_DOUBLE
 * \brief Calculate the base 10 exponential of the input argument.
 *
 * Calculate the base 10 exponential of the input argument \p x.
 *
 * \return Returns 
 * \latexonly $10^x$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:msup>
 *     <m:mn>10</m:mn>
 *     <m:mi>x</m:mi>
 *   </m:msup>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 *
 * \note_accuracy_double
 */         
extern __host__ __device__ __device_builtin__ double                 exp10(double x) __THROW;
/**
 * \ingroup CUDA_MATH_SINGLE
 * \brief Calculate the base 10 exponential of the input argument.
 *
 * Calculate the base 10 exponential of the input argument \p x.
 *
 * \return Returns 
 * \latexonly $10^x$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:msup>
 *     <m:mn>10</m:mn>
 *     <m:mi>x</m:mi>
 *   </m:msup>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 *
 * \note_accuracy_single
 * \note_fastmath
 */
extern __host__ __device__ __device_builtin__ float                  exp10f(float x) __THROW;
/**
 * \ingroup CUDA_MATH_DOUBLE
 * \brief Calculate the base 
 * \latexonly $e$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mi>e</m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 *  exponential of the input argument, minus 1.
 *
 * Calculate the base 
 * \latexonly $e$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mi>e</m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 *  exponential of the input argument \p x, minus 1.
 *
 * \return Returns 
 * \latexonly $e^x - 1$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:msup>
 *     <m:mi>e</m:mi>
 *     <m:mi>x</m:mi>
 *   </m:msup>
 *   <m:mo>&#x2212;<!-- − --></m:mo>
 *   <m:mn>1</m:mn>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 *
 * \note_accuracy_double
 */
#if _MSC_VER < 1800
extern __host__ __device__ __device_builtin__ double                 expm1(double x) __THROW;
#else /* _MSC_VER < 1800 */
extern __host__ __device__ __device_builtin__ _CRTIMP double __cdecl expm1(double x);
#endif /* _MSC_VER < 1800 */
/**
 * \ingroup CUDA_MATH_SINGLE
 * \brief Calculate the base 
 * \latexonly $e$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mi>e</m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 *  exponential of the input argument, minus 1.
 *
 * Calculate the base 
 * \latexonly $e$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mi>e</m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 *  exponential of the input argument \p x, minus 1.
 *
 * \return  Returns 
 * \latexonly $e^x - 1$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:msup>
 *     <m:mi>e</m:mi>
 *     <m:mi>x</m:mi>
 *   </m:msup>
 *   <m:mo>&#x2212;<!-- − --></m:mo>
 *   <m:mn>1</m:mn>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 *
 * \note_accuracy_single
 */
#if _MSC_VER < 1800
extern __host__ __device__ __device_builtin__ float                  expm1f(float x) __THROW;        
#else /* _MSC_VER < 1800 */
extern __host__ __device__ __device_builtin__ _CRTIMP float  __cdecl expm1f(float x);
#endif /* _MSC_VER < 1800 */
/**
 * \ingroup CUDA_MATH_SINGLE
 * \brief Calculate the base 2 logarithm of the input argument.
 *
 * Calculate the base 2 logarithm of the input argument \p x.
 *
 * \return
 * - log2f(
 * \latexonly $\pm 0$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mn>0</m:mn>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns 
 * \latexonly $-\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x2212;<!-- − --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 * - log2f(1) returns +0.
 * - log2f(\p x) returns NaN for \p x < 0.
 * - log2f(
 * \latexonly $+\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>+</m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns 
 * \latexonly $+\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>+</m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 *
 * \note_accuracy_single
 */
#if _MSC_VER < 1800
extern __host__ __device__ __device_builtin__ float                  log2f(float x) __THROW;         
#else /* _MSC_VER < 1800 */
extern __host__ __device__ __device_builtin__ _CRTIMP float  __cdecl log2f(float x);
#endif /* _MSC_VER < 1800 */
/**
 * \ingroup CUDA_MATH_DOUBLE
 * \brief Calculate the base 10 logarithm of the input argument.
 *
 * Calculate the base 10 logarithm of the input argument \p x.
 *
 * \return 
 * - log10(
 * \latexonly $\pm 0$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mn>0</m:mn>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns 
 * \latexonly $-\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x2212;<!-- − --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 * - log10(1) returns +0.
 * - log10(\p x) returns NaN for \p x < 0.
 * - log10(
 * \latexonly $+\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>+</m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns 
 * \latexonly $+\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>+</m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 *
 * \note_accuracy_double
 */
extern __host__ __device__ __device_builtin__ double         __cdecl log10(double x) __THROW;
/**
 * \ingroup CUDA_MATH_DOUBLE
 * \brief Calculate the base 
 * \latexonly $e$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mi>e</m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 *  logarithm of the input argument.
 *
 * Calculate the base 
 * \latexonly $e$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mi>e</m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 *  logarithm of the input argument \p x.
 *
 * \return 
 * - log(
 * \latexonly $\pm 0$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mn>0</m:mn>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns 
 * \latexonly $-\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x2212;<!-- − --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 * - log(1) returns +0.
 * - log(\p x) returns NaN for \p x < 0.
 * - log(
 * \latexonly $+\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>+</m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns 
 * \latexonly $+\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>+</m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly

 *
 * \note_accuracy_double
 */
extern __host__ __device__ __device_builtin__ double         __cdecl log(double x) __THROW;
/**
 * \ingroup CUDA_MATH_DOUBLE
 * \brief Calculate the value of 
 * \latexonly $log_{e}(1+x)$ \endlatexonly
 * \latexonly $\lfloor x \rfloor$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mi>l</m:mi>
 *   <m:mi>o</m:mi>
 *   <m:msub>
 *     <m:mi>g</m:mi>
 *     <m:mrow class="MJX-TeXAtom-ORD">
 *       <m:mi>e</m:mi>
 *     </m:mrow>
 *   </m:msub>
 *   <m:mo stretchy="false">(</m:mo>
 *   <m:mn>1</m:mn>
 *   <m:mo>+</m:mo>
 *   <m:mi>x</m:mi>
 *   <m:mo stretchy="false">)</m:mo>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 *
 * Calculate the value of 
 * \latexonly $log_{e}(1+x)$ \endlatexonly
 * \latexonly $\lfloor x \rfloor$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mi>l</m:mi>
 *   <m:mi>o</m:mi>
 *   <m:msub>
 *     <m:mi>g</m:mi>
 *     <m:mrow class="MJX-TeXAtom-ORD">
 *       <m:mi>e</m:mi>
 *     </m:mrow>
 *   </m:msub>
 *   <m:mo stretchy="false">(</m:mo>
 *   <m:mn>1</m:mn>
 *   <m:mo>+</m:mo>
 *   <m:mi>x</m:mi>
 *   <m:mo stretchy="false">)</m:mo>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 *   of the input argument \p x.
 *
 * \return 
 * - log1p(
 * \latexonly $\pm 0$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mn>0</m:mn>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns 
 * \latexonly $-\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x2212;<!-- − --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 * - log1p(-1) returns +0.
 * - log1p(\p x) returns NaN for \p x < -1.
 * - log1p(
 * \latexonly $+\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>+</m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns 
 * \latexonly $+\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>+</m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 *
 * \note_accuracy_double
 */
#if _MSC_VER < 1800
extern __host__ __device__ __device_builtin__ double                 log1p(double x) __THROW;
#else /* _MSC_VER < 1800 */
extern __host__ __device__ __device_builtin__ _CRTIMP double __cdecl log1p(double x);
#endif /* _MSC_VER < 1800 */
/**
 * \ingroup CUDA_MATH_SINGLE
 * \brief Calculate the value of 
 * \latexonly $log_{e}(1+x)$ \endlatexonly
 * \latexonly $\lfloor x \rfloor$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mi>l</m:mi>
 *   <m:mi>o</m:mi>
 *   <m:msub>
 *     <m:mi>g</m:mi>
 *     <m:mrow class="MJX-TeXAtom-ORD">
 *       <m:mi>e</m:mi>
 *     </m:mrow>
 *   </m:msub>
 *   <m:mo stretchy="false">(</m:mo>
 *   <m:mn>1</m:mn>
 *   <m:mo>+</m:mo>
 *   <m:mi>x</m:mi>
 *   <m:mo stretchy="false">)</m:mo>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 *
 * Calculate the value of 
 * \latexonly $log_{e}(1+x)$ \endlatexonly
 * \latexonly $\lfloor x \rfloor$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mi>l</m:mi>
 *   <m:mi>o</m:mi>
 *   <m:msub>
 *     <m:mi>g</m:mi>
 *     <m:mrow class="MJX-TeXAtom-ORD">
 *       <m:mi>e</m:mi>
 *     </m:mrow>
 *   </m:msub>
 *   <m:mo stretchy="false">(</m:mo>
 *   <m:mn>1</m:mn>
 *   <m:mo>+</m:mo>
 *   <m:mi>x</m:mi>
 *   <m:mo stretchy="false">)</m:mo>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 *   of the input argument \p x.
 *
 * \return 
 * - log1pf(
 * \latexonly $\pm 0$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mn>0</m:mn>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns 
 * \latexonly $-\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x2212;<!-- − --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 * - log1pf(-1) returns +0.
 * - log1pf(\p x) returns NaN for \p x < -1.
 * - log1pf(
 * \latexonly $+\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>+</m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns 
 * \latexonly $+\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>+</m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 *
 * \note_accuracy_single
 */
#if _MSC_VER < 1800
extern __host__ __device__ __device_builtin__ float                  log1pf(float x) __THROW;
#else /* _MSC_VER < 1800 */
extern __host__ __device__ __device_builtin__ _CRTIMP float  __cdecl log1pf(float x);
#endif /* _MSC_VER < 1800 */
/**
 * \ingroup CUDA_MATH_DOUBLE
 * \brief Calculate the largest integer less than or equal to \p x.
 * 
 * Calculates the largest integer value which is less than or equal to \p x.
 * 
 * \return
 * Returns 
 * \latexonly $log_{e}(1+x)$ \endlatexonly
 * \latexonly $\lfloor x \rfloor$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mi>l</m:mi>
 *   <m:mi>o</m:mi>
 *   <m:msub>
 *     <m:mi>g</m:mi>
 *     <m:mrow class="MJX-TeXAtom-ORD">
 *       <m:mi>e</m:mi>
 *     </m:mrow>
 *   </m:msub>
 *   <m:mo stretchy="false">(</m:mo>
 *   <m:mn>1</m:mn>
 *   <m:mo>+</m:mo>
 *   <m:mi>x</m:mi>
 *   <m:mo stretchy="false">)</m:mo>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 *  expressed as a floating-point number.
 * - floor(
 * \latexonly $\pm \infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns 
 * \latexonly $\pm \infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 * - floor(
 * \latexonly $\pm 0$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mn>0</m:mn>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns 
 * \latexonly $\pm 0$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mn>0</m:mn>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 *
 * \note_accuracy_double
 */
extern __host__ __device__ __device_builtin__ _CRTIMP double __cdecl floor(double x) __THROW;
/**
 * \ingroup CUDA_MATH_DOUBLE
 * \brief Calculate the base 
 * \latexonly $e$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mi>e</m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 *  exponential of the input argument.
 *
 * Calculate the base 
 * \latexonly $e$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mi>e</m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 *  exponential of the input argument \p x.
 *
 * \return Returns 
 * \latexonly $e^x$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:msup>
 *     <m:mi>e</m:mi>
 *     <m:mi>x</m:mi>
 *   </m:msup>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 *
 * \note_accuracy_double
 */
extern __host__ __device__ __device_builtin__ double         __cdecl exp(double x) __THROW;
/**
 * \ingroup CUDA_MATH_DOUBLE
 * \brief Calculate the hyperbolic cosine of the input argument.
 *
 * Calculate the hyperbolic cosine of the input argument \p x.
 *
 * \return 
 * - cosh(0) returns 1.
 * - cosh(
 * \latexonly $\pm \infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns 
 * \latexonly $+\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>+</m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 *
 * \note_accuracy_double
 */
extern __host__ __device__ __device_builtin__ double         __cdecl cosh(double x) __THROW;
/**
 * \ingroup CUDA_MATH_DOUBLE
 * \brief Calculate the hyperbolic sine of the input argument.
 *
 * Calculate the hyperbolic sine of the input argument \p x.
 *
 * \return 
 * - sinh(
 * \latexonly $\pm 0$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mn>0</m:mn>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns 
 * \latexonly $\pm 0$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mn>0</m:mn>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 *
 * \note_accuracy_double
 */
extern __host__ __device__ __device_builtin__ double         __cdecl sinh(double x) __THROW;
/**
 * \ingroup CUDA_MATH_DOUBLE
 * \brief Calculate the hyperbolic tangent of the input argument.
 *
 * Calculate the hyperbolic tangent of the input argument \p x.
 *
 * \return 
 * - tanh(
 * \latexonly $\pm 0$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mn>0</m:mn>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns 
 * \latexonly $\pm 0$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mn>0</m:mn>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 *
 * \note_accuracy_double
 */
extern __host__ __device__ __device_builtin__ double         __cdecl tanh(double x) __THROW;
/**
 * \ingroup CUDA_MATH_DOUBLE
 * \brief Calculate the nonnegative arc hyperbolic cosine of the input argument.
 *
 * Calculate the nonnegative arc hyperbolic cosine of the input argument \p x.
 *
 * \return 
 * Result will be in the interval [0, 
 * \latexonly $+\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>+</m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ].
 * - acosh(1) returns 0.
 * - acosh(\p x) returns NaN for \p x in the interval [
 * \latexonly $-\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x2212;<!-- − --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * , 1).
 *
 * \note_accuracy_double
 */
#if _MSC_VER < 1800
extern __host__ __device__ __device_builtin__ double                 acosh(double x) __THROW;
#else /* _MSC_VER < 1800 */
extern __host__ __device__ __device_builtin__ _CRTIMP double __cdecl acosh(double x);
#endif /* _MSC_VER < 1800 */
/**
 * \ingroup CUDA_MATH_SINGLE
 * \brief Calculate the nonnegative arc hyperbolic cosine of the input argument.
 *
 * Calculate the nonnegative arc hyperbolic cosine of the input argument \p x.
 *
 * \return 
 * Result will be in the interval [0, 
 * \latexonly $+\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>+</m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ].
 * - acoshf(1) returns 0.
 * - acoshf(\p x) returns NaN for \p x in the interval [
 * \latexonly $-\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x2212;<!-- − --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * , 1).
 *
 * \note_accuracy_single
 */
#if _MSC_VER < 1800
extern __host__ __device__ __device_builtin__ float                  acoshf(float x) __THROW;
#else /* _MSC_VER < 1800 */
extern __host__ __device__ __device_builtin__ _CRTIMP float  __cdecl acoshf(float x);
#endif /* _MSC_VER < 1800 */
/**
 * \ingroup CUDA_MATH_DOUBLE
 * \brief Calculate the arc hyperbolic sine of the input argument.
 *
 * Calculate the arc hyperbolic sine of the input argument \p x.
 *
 * \return 
 * - asinh(0) returns 1.
 *
 * \note_accuracy_double
 */
#if _MSC_VER < 1800
extern __host__ __device__ __device_builtin__ double                 asinh(double x) __THROW;
#else /* _MSC_VER < 1800 */
extern __host__ __device__ __device_builtin__ _CRTIMP double __cdecl asinh(double x);
#endif /* _MSC_VER < 1800 */
/**
 * \ingroup CUDA_MATH_SINGLE
 * \brief Calculate the arc hyperbolic sine of the input argument.
 *
 * Calculate the arc hyperbolic sine of the input argument \p x.
 *
 * \return 
 * - asinhf(0) returns 1.
 *
 * \note_accuracy_single
 */
#if _MSC_VER < 1800
extern __host__ __device__ __device_builtin__ float                  asinhf(float x) __THROW;
#else /* _MSC_VER < 1800 */
extern __host__ __device__ __device_builtin__ _CRTIMP float  __cdecl asinhf(float x);
#endif /* _MSC_VER < 1800 */
/**
 * \ingroup CUDA_MATH_DOUBLE
 * \brief Calculate the arc hyperbolic tangent of the input argument.
 *
 * Calculate the arc hyperbolic tangent of the input argument \p x.
 *
 * \return 
 * - atanh(
 * \latexonly $\pm 0$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mn>0</m:mn>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns 
 * \latexonly $\pm 0$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mn>0</m:mn>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 * - atanh(
 * \latexonly $\pm 1$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mn>1</m:mn>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns 
 * \latexonly $\pm \infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 * - atanh(\p x) returns NaN for \p x outside interval [-1, 1].
 *
 * \note_accuracy_double
 */
#if _MSC_VER < 1800
extern __host__ __device__ __device_builtin__ double                 atanh(double x) __THROW;
#else /* _MSC_VER < 1800 */
extern __host__ __device__ __device_builtin__ _CRTIMP double __cdecl atanh(double x);
#endif /* _MSC_VER < 1800 */
/**
 * \ingroup CUDA_MATH_SINGLE
 * \brief Calculate the arc hyperbolic tangent of the input argument.
 *
 * Calculate the arc hyperbolic tangent of the input argument \p x.
 *
 * \return 
 * - atanhf(
 * \latexonly $\pm 0$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mn>0</m:mn>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns 
 * \latexonly $\pm 0$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mn>0</m:mn>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 * - atanhf(
 * \latexonly $\pm 1$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mn>1</m:mn>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns 
 * \latexonly $\pm \infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 * - atanhf(\p x) returns NaN for \p x outside interval [-1, 1].
 *
 * \note_accuracy_single
 */
#if _MSC_VER < 1800
extern __host__ __device__ __device_builtin__ float                  atanhf(float x) __THROW;
#else /* _MSC_VER < 1800 */
extern __host__ __device__ __device_builtin__ _CRTIMP float  __cdecl atanhf(float x);
#endif /* _MSC_VER < 1800 */
/**
 * \ingroup CUDA_MATH_DOUBLE
 * \brief Calculate the value of 
 * \latexonly $x\cdot 2^{exp}$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mi>x</m:mi>
 *   <m:mo>&#x22C5;<!-- ⋅ --></m:mo>
 *   <m:msup>
 *     <m:mn>2</m:mn>
 *     <m:mrow class="MJX-TeXAtom-ORD">
 *       <m:mi>e</m:mi>
 *       <m:mi>x</m:mi>
 *       <m:mi>p</m:mi>
 *     </m:mrow>
 *   </m:msup>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 *
 * Calculate the value of 
 * \latexonly $x\cdot 2^{exp}$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mi>x</m:mi>
 *   <m:mo>&#x22C5;<!-- ⋅ --></m:mo>
 *   <m:msup>
 *     <m:mn>2</m:mn>
 *     <m:mrow class="MJX-TeXAtom-ORD">
 *       <m:mi>e</m:mi>
 *       <m:mi>x</m:mi>
 *       <m:mi>p</m:mi>
 *     </m:mrow>
 *   </m:msup>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 *  of the input arguments \p x and \p exp.
 *
 * \return 
 * - ldexp(\p x) returns 
 * \latexonly $\pm \infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 *  if the correctly calculated value is outside the double floating point range.
 *
 * \note_accuracy_double
 */
extern __host__ __device__ __device_builtin__ _CRTIMP double __cdecl ldexp(double x, int exp) __THROW;
/**
 * \ingroup CUDA_MATH_SINGLE
 * \brief Calculate the value of 
 * \latexonly $x\cdot 2^{exp}$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mi>x</m:mi>
 *   <m:mo>&#x22C5;<!-- ⋅ --></m:mo>
 *   <m:msup>
 *     <m:mn>2</m:mn>
 *     <m:mrow class="MJX-TeXAtom-ORD">
 *       <m:mi>e</m:mi>
 *       <m:mi>x</m:mi>
 *       <m:mi>p</m:mi>
 *     </m:mrow>
 *   </m:msup>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 *
 * Calculate the value of 
 * \latexonly $x\cdot 2^{exp}$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mi>x</m:mi>
 *   <m:mo>&#x22C5;<!-- ⋅ --></m:mo>
 *   <m:msup>
 *     <m:mn>2</m:mn>
 *     <m:mrow class="MJX-TeXAtom-ORD">
 *       <m:mi>e</m:mi>
 *       <m:mi>x</m:mi>
 *       <m:mi>p</m:mi>
 *     </m:mrow>
 *   </m:msup>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 *  of the input arguments \p x and \p exp.
 *
 * \return 
 * - ldexpf(\p x) returns 
 * \latexonly $\pm \infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 *  if the correctly calculated value is outside the single floating point range.
 *
 * \note_accuracy_single
 */
extern __host__ __device__ __device_builtin__ float                  ldexpf(float x, int exp) __THROW;
/**
 * \ingroup CUDA_MATH_DOUBLE
 * \brief Calculate the floating point representation of the exponent of the input argument.
 *
 * Calculate the floating point representation of the exponent of the input argument \p x.
 *
 * \return 
 * - logb
 * \latexonly $\pm 0$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mn>0</m:mn>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 returns 
 * \latexonly $-\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x2212;<!-- − --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * - logb
 * \latexonly $\pm \infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 *  returns 
 * \latexonly $+\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>+</m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 *
 * \note_accuracy_double
 */
#if _MSC_VER < 1800
extern __host__ __device__ __device_builtin__ double                 logb(double x) __THROW;
#else /* _MSC_VER < 1800 */
extern __host__ __device__ __device_builtin__ _CRTIMP double __cdecl logb(double x);
#endif /* _MSC_VER < 1800 */
/**
 * \ingroup CUDA_MATH_SINGLE
 * \brief Calculate the floating point representation of the exponent of the input argument.
 *
 * Calculate the floating point representation of the exponent of the input argument \p x.
 *
 * \return 
 * - logbf
 * \latexonly $\pm 0$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mn>0</m:mn>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 *  returns 
 * \latexonly $-\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x2212;<!-- − --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * - logbf
 * \latexonly $+\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>+</m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 *  returns 
 * \latexonly $+\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>+</m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 *
 * \note_accuracy_single
 */
#if _MSC_VER < 1800
extern __host__ __device__ __device_builtin__ float                  logbf(float x) __THROW;
#else /* _MSC_VER < 1800 */
extern __host__ __device__ __device_builtin__ _CRTIMP float  __cdecl logbf(float x);
#endif /* _MSC_VER < 1800 */
/**
 * \ingroup CUDA_MATH_DOUBLE
 * \brief Compute the unbiased integer exponent of the argument.
 *
 * Calculates the unbiased integer exponent of the input argument \p x.
 *
 * \return
 * - If successful, returns the unbiased exponent of the argument.
 * - ilogb(0) returns <tt>INT_MIN</tt>.
 * - ilogb(NaN) returns NaN.
 * - ilogb(\p x) returns <tt>INT_MAX</tt> if \p x is 
 * \latexonly $\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly 
 *     or the correct value is greater than <tt>INT_MAX</tt>.
 * - ilogb(\p x) return <tt>INT_MIN</tt> if the correct value is less 
 *     than <tt>INT_MIN</tt>.
 *
 * \note_accuracy_double
 */
#if _MSC_VER < 1800
extern __host__ __device__ __device_builtin__ int                    ilogb(double x) __THROW;
#else /* _MSC_VER < 1800 */
extern __host__ __device__ __device_builtin__ _CRTIMP int    __cdecl ilogb(double x);
#endif /* _MSC_VER < 1800 */
/**
 * \ingroup CUDA_MATH_SINGLE
 * \brief Compute the unbiased integer exponent of the argument.
 *
 * Calculates the unbiased integer exponent of the input argument \p x.
 *
 * \return
 * - If successful, returns the unbiased exponent of the argument.
 * - ilogbf(0) returns <tt>INT_MIN</tt>.
 * - ilogbf(NaN) returns NaN.
 * - ilogbf(\p x) returns <tt>INT_MAX</tt> if \p x is 
 * \latexonly $\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly 
 *     or the correct value is greater than <tt>INT_MAX</tt>.
 * - ilogbf(\p x) return <tt>INT_MIN</tt> if the correct value is less 
 *     than <tt>INT_MIN</tt>.
 *
 * \note_accuracy_single
 */
#if _MSC_VER < 1800
extern __host__ __device__ __device_builtin__ int                    ilogbf(float x) __THROW;
#else /* _MSC_VER < 1800 */
extern __host__ __device__ __device_builtin__ _CRTIMP int    __cdecl ilogbf(float x);
#endif /* _MSC_VER < 1800 */
/**
 * \ingroup CUDA_MATH_DOUBLE
 * \brief Scale floating-point input by integer power of two.
 *
 * Scale \p x by 
 * \latexonly $2^n$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:msup>
 *     <m:mn>2</m:mn>
 *     <m:mi>n</m:mi>
 *   </m:msup>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 *  by efficient manipulation of the floating-point
 * exponent.
 *
 * \return 
 * Returns \p x * 
 * \latexonly $2^n$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:msup>
 *     <m:mn>2</m:mn>
 *     <m:mi>n</m:mi>
 *   </m:msup>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 * - scalbn(
 * \latexonly $\pm 0$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mn>0</m:mn>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * , \p n) returns 
 * \latexonly $\pm 0$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mn>0</m:mn>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 * - scalbn(\p x, 0) returns \p x.
 * - scalbn(
 * \latexonly $\pm \infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * , \p n) returns 
 * \latexonly $\pm \infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 */
#if _MSC_VER < 1800
extern __host__ __device__ __device_builtin__ double                 scalbn(double x, int n) __THROW;
#else /* _MSC_VER < 1800 */
extern __host__ __device__ __device_builtin__ _CRTIMP double __cdecl scalbn(double x, int n);
#endif /* _MSC_VER < 1800 */
/**
 * \ingroup CUDA_MATH_SINGLE
 * \brief Scale floating-point input by integer power of two.
 *
 * Scale \p x by 
 * \latexonly $2^n$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:msup>
 *     <m:mn>2</m:mn>
 *     <m:mi>n</m:mi>
 *   </m:msup>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 *  by efficient manipulation of the floating-point
 * exponent.
 *
 * \return 
 * Returns \p x * 
 * \latexonly $2^n$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:msup>
 *     <m:mn>2</m:mn>
 *     <m:mi>n</m:mi>
 *   </m:msup>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 * - scalbnf(
 * \latexonly $\pm 0$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mn>0</m:mn>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * , \p n) returns 
 * \latexonly $\pm 0$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mn>0</m:mn>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 * - scalbnf(\p x, 0) returns \p x.
 * - scalbnf(
 * \latexonly $\pm \infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * , \p n) returns 
 * \latexonly $\pm \infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 */
#if _MSC_VER < 1800
extern __host__ __device__ __device_builtin__ float                  scalbnf(float x, int n) __THROW;
#else /* _MSC_VER < 1800 */
extern __host__ __device__ __device_builtin__ _CRTIMP float  __cdecl scalbnf(float x, int n);
#endif /* _MSC_VER < 1800 */
/**
 * \ingroup CUDA_MATH_DOUBLE
 * \brief Scale floating-point input by integer power of two.
 *
 * Scale \p x by 
 * \latexonly $2^n$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:msup>
 *     <m:mn>2</m:mn>
 *     <m:mi>n</m:mi>
 *   </m:msup>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 *  by efficient manipulation of the floating-point
 * exponent.
 *
 * \return 
 * Returns \p x * 
 * \latexonly $2^n$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:msup>
 *     <m:mn>2</m:mn>
 *     <m:mi>n</m:mi>
 *   </m:msup>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 * - scalbln(
 * \latexonly $\pm 0$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mn>0</m:mn>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * , \p n) returns 
 * \latexonly $\pm 0$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mn>0</m:mn>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 * - scalbln(\p x, 0) returns \p x.
 * - scalbln(
 * \latexonly $\pm \infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * , \p n) returns 
 * \latexonly $\pm \infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 */
#if _MSC_VER < 1800
extern __host__ __device__ __device_builtin__ double                 scalbln(double x, long int n) __THROW;
#else /* _MSC_VER < 1800 */
extern __host__ __device__ __device_builtin__ _CRTIMP double __cdecl scalbln(double x, long int n);
#endif /* _MSC_VER < 1800 */
/**
 * \ingroup CUDA_MATH_SINGLE
 * \brief Scale floating-point input by integer power of two.
 *
 * Scale \p x by 
 * \latexonly $2^n$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:msup>
 *     <m:mn>2</m:mn>
 *     <m:mi>n</m:mi>
 *   </m:msup>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 *  by efficient manipulation of the floating-point
 * exponent.
 *
 * \return 
 * Returns \p x * 
 * \latexonly $2^n$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:msup>
 *     <m:mn>2</m:mn>
 *     <m:mi>n</m:mi>
 *   </m:msup>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 * - scalblnf(
 * \latexonly $\pm 0$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mn>0</m:mn>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * , \p n) returns 
 * \latexonly $\pm 0$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mn>0</m:mn>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 * - scalblnf(\p x, 0) returns \p x.
 * - scalblnf(
 * \latexonly $\pm \infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * , \p n) returns 
 * \latexonly $\pm \infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 */
#if _MSC_VER < 1800
extern __host__ __device__ __device_builtin__ float                  scalblnf(float x, long int n) __THROW;
#else /* _MSC_VER < 1800 */
extern __host__ __device__ __device_builtin__ _CRTIMP float  __cdecl scalblnf(float x, long int n);
#endif /* _MSC_VER < 1800 */
/**
 * \ingroup CUDA_MATH_DOUBLE
 * \brief Extract mantissa and exponent of a floating-point value
 * 
 * Decompose the floating-point value \p x into a component \p m for the 
 * normalized fraction element and another term \p n for the exponent.
 * The absolute value of \p m will be greater than or equal to  0.5 and 
 * less than 1.0 or it will be equal to 0; 
 * \latexonly $x = m\cdot 2^n$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mi>x</m:mi>
 *   <m:mo>=</m:mo>
 *   <m:mi>m</m:mi>
 *   <m:mo>&#x22C5;<!-- ⋅ --></m:mo>
 *   <m:msup>
 *     <m:mn>2</m:mn>
 *     <m:mi>n</m:mi>
 *   </m:msup>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 * The integer exponent \p n will be stored in the location to which \p nptr points.
 *
 * \return
 * Returns the fractional component \p m.
 * - frexp(0, \p nptr) returns 0 for the fractional component and zero for the integer component.
 * - frexp(
 * \latexonly $\pm 0$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mn>0</m:mn>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * , \p nptr) returns 
 * \latexonly $\pm 0$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mn>0</m:mn>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 *  and stores zero in the location pointed to by \p nptr.
 * - frexp(
 * \latexonly $\pm \infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * , \p nptr) returns 
 * \latexonly $\pm \infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 *  and stores an unspecified value in the 
 * location to which \p nptr points.
 * - frexp(NaN, \p y) returns a NaN and stores an unspecified value in the location to which \p nptr points.
 *
 * \note_accuracy_double
 */
extern __host__ __device__ __device_builtin__ _CRTIMP double __cdecl frexp(double x, int *nptr) __THROW;
/**
 * \ingroup CUDA_MATH_SINGLE
 * \brief Extract mantissa and exponent of a floating-point value
 * 
 * Decomposes the floating-point value \p x into a component \p m for the 
 * normalized fraction element and another term \p n for the exponent.
 * The absolute value of \p m will be greater than or equal to  0.5 and 
 * less than 1.0 or it will be equal to 0; 
 * \latexonly $x = m\cdot 2^n$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mi>x</m:mi>
 *   <m:mo>=</m:mo>
 *   <m:mi>m</m:mi>
 *   <m:mo>&#x22C5;<!-- ⋅ --></m:mo>
 *   <m:msup>
 *     <m:mn>2</m:mn>
 *     <m:mi>n</m:mi>
 *   </m:msup>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 * The integer exponent \p n will be stored in the location to which \p nptr points.
 *
 * \return
 * Returns the fractional component \p m.
 * - frexp(0, \p nptr) returns 0 for the fractional component and zero for the integer component.
 * - frexp(
 * \latexonly $\pm 0$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mn>0</m:mn>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * , \p nptr) returns 
 * \latexonly $\pm 0$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mn>0</m:mn>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 *  and stores zero in the location pointed to by \p nptr.
 * - frexp(
 * \latexonly $\pm \infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * , \p nptr) returns 
 * \latexonly $\pm \infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 *  and stores an unspecified value in the 
 * location to which \p nptr points.
 * - frexp(NaN, \p y) returns a NaN and stores an unspecified value in the location to which \p nptr points.
 *
 * \note_accuracy_single
 */
extern __host__ __device__ __device_builtin__ float                  frexpf(float x, int *nptr) __THROW;
/**
 * \ingroup CUDA_MATH_DOUBLE
 * \brief Round to nearest integer value in floating-point.
 *
 * Round \p x to the nearest integer value in floating-point format,
 * with halfway cases rounded away from zero.
 *
 * \return 
 * Returns rounded integer value.
 *
 * \note_slow_round See ::rint().
 */
#if _MSC_VER < 1800
extern __host__ __device__ __device_builtin__ double                 round(double x) __THROW;
#else /* _MSC_VER < 1800 */
extern __host__ __device__ __device_builtin__ _CRTIMP double __cdecl round(double x);
#endif /* _MSC_VER < 1800 */
/**
 * \ingroup CUDA_MATH_SINGLE
 * \brief Round to nearest integer value in floating-point.
 *
 * Round \p x to the nearest integer value in floating-point format,
 * with halfway cases rounded away from zero.
 *
 * \return
 * Returns rounded integer value.
 *
 * \note_slow_round See ::rintf().
 */
#if _MSC_VER < 1800
extern __host__ __device__ __device_builtin__ float                  roundf(float x) __THROW;
#else /* _MSC_VER < 1800 */
extern __host__ __device__ __device_builtin__ _CRTIMP float  __cdecl roundf(float x);
#endif /* _MSC_VER < 1800 */
/**
 * \ingroup CUDA_MATH_DOUBLE
 * \brief Round to nearest integer value.
 *
 * Round \p x to the nearest integer value, with halfway cases rounded 
 * away from zero.  If the result is outside the range of the return type,
 * the result is undefined.
 *
 * \return 
 * Returns rounded integer value.
 *
 * \note_slow_round See ::lrint().
 */
#if _MSC_VER < 1800
extern __host__ __device__ __device_builtin__ long int               lround(double x) __THROW;
#else /* _MSC_VER < 1800 */
extern __host__ __device__ __device_builtin__ _CRTIMP long int __cdecl lround(double x);
#endif /* _MSC_VER < 1800 */
/**
 * \ingroup CUDA_MATH_SINGLE
 * \brief Round to nearest integer value.
 *
 * Round \p x to the nearest integer value, with halfway cases rounded 
 * away from zero.  If the result is outside the range of the return type,
 * the result is undefined.
 *
 * \return 
 * Returns rounded integer value.
 *
 * \note_slow_round See ::lrintf().
 */
#if _MSC_VER < 1800
extern __host__ __device__ __device_builtin__ long int               lroundf(float x) __THROW;
#else /* _MSC_VER < 1800 */
extern __host__ __device__ __device_builtin__ _CRTIMP long int __cdecl lroundf(float x);
#endif /* _MSC_VER < 1800 */
/**
 * \ingroup CUDA_MATH_DOUBLE
 * \brief Round to nearest integer value.
 *
 * Round \p x to the nearest integer value, with halfway cases rounded 
 * away from zero.  If the result is outside the range of the return type,
 * the result is undefined.
 *
 * \return 
 * Returns rounded integer value.
 *
 * \note_slow_round See ::llrint().
 */
#if _MSC_VER < 1800
extern __host__ __device__ __device_builtin__ long long int          llround(double x) __THROW;
#else /* _MSC_VER < 1800 */
extern __host__ __device__ __device_builtin__ _CRTIMP long long int __cdecl llround(double x);
#endif /* _MSC_VER < 1800 */
/**
 * \ingroup CUDA_MATH_SINGLE
 * \brief Round to nearest integer value.
 *
 * Round \p x to the nearest integer value, with halfway cases rounded 
 * away from zero.  If the result is outside the range of the return type,
 * the result is undefined.
 *
 * \return 
 * Returns rounded integer value.
 *
 * \note_slow_round See ::llrintf().
 */
#if _MSC_VER < 1800
extern __host__ __device__ __device_builtin__ long long int          llroundf(float x) __THROW;
#else /* _MSC_VER < 1800 */
extern __host__ __device__ __device_builtin__ _CRTIMP long long int __cdecl llroundf(float x);
#endif /* _MSC_VER < 1800 */
/**
 * \ingroup CUDA_MATH_DOUBLE
 * \brief Round to nearest integer value in floating-point.
 *
 * Round \p x to the nearest integer value in floating-point format,
 * with halfway cases rounded to the nearest even integer value.
 *
 * \return 
 * Returns rounded integer value.
 */
#if _MSC_VER < 1800
extern __host__ __device__ __device_builtin__ double                 rint(double x) __THROW;
#else /* _MSC_VER < 1800 */
extern __host__ __device__ __device_builtin__ _CRTIMP double __cdecl rint(double x);
#endif /* _MSC_VER < 1800 */
/**
 * \ingroup CUDA_MATH_SINGLE
 * \brief Round input to nearest integer value in floating-point.
 *
 * Round \p x to the nearest integer value in floating-point format,
 * with halfway cases rounded towards zero.
 *
 * \return 
 * Returns rounded integer value.
 */
#if _MSC_VER < 1800
extern __host__ __device__ __device_builtin__ float                  rintf(float x) __THROW;
#else /* _MSC_VER < 1800 */
extern __host__ __device__ __device_builtin__ _CRTIMP float  __cdecl rintf(float x);
#endif /* _MSC_VER < 1800 */
/**
 * \ingroup CUDA_MATH_DOUBLE
 * \brief Round input to nearest integer value.
 *
 * Round \p x to the nearest integer value, with halfway cases rounded 
 * towards zero.  If the result is outside the range of the return type,
 * the result is undefined.
 *
 * \return 
 * Returns rounded integer value.
 */
#if _MSC_VER < 1800
extern __host__ __device__ __device_builtin__ long int               lrint(double x) __THROW;
#else /* _MSC_VER < 1800 */
extern __host__ __device__ __device_builtin__ _CRTIMP long int __cdecl lrint(double x);
#endif /* _MSC_VER < 1800 */
/**
 * \ingroup CUDA_MATH_SINGLE
 * \brief Round input to nearest integer value.
 *
 * Round \p x to the nearest integer value, with halfway cases rounded 
 * towards zero.  If the result is outside the range of the return type,
 * the result is undefined.
 *
 * \return 
 * Returns rounded integer value.
 */
#if _MSC_VER < 1800
extern __host__ __device__ __device_builtin__ long int               lrintf(float x) __THROW;
#else /* _MSC_VER < 1800 */
extern __host__ __device__ __device_builtin__ _CRTIMP long int __cdecl lrintf(float x);
#endif /* _MSC_VER < 1800 */
/**
 * \ingroup CUDA_MATH_DOUBLE
 * \brief Round input to nearest integer value.
 *
 * Round \p x to the nearest integer value, with halfway cases rounded 
 * towards zero.  If the result is outside the range of the return type,
 * the result is undefined.
 *
 * \return 
 * Returns rounded integer value.
 */
#if _MSC_VER < 1800
extern __host__ __device__ __device_builtin__ long long int          llrint(double x) __THROW;
#else /* _MSC_VER < 1800 */
extern __host__ __device__ __device_builtin__ _CRTIMP long long int __cdecl llrint(double x);
#endif /* _MSC_VER < 1800 */
/**
 * \ingroup CUDA_MATH_SINGLE
 * \brief Round input to nearest integer value.
 *
 * Round \p x to the nearest integer value, with halfway cases rounded 
 * towards zero.  If the result is outside the range of the return type,
 * the result is undefined.
 *
 * \return 
 * Returns rounded integer value.
 */
#if _MSC_VER < 1800
extern __host__ __device__ __device_builtin__ long long int          llrintf(float x) __THROW;
#else /* _MSC_VER < 1800 */
extern __host__ __device__ __device_builtin__ _CRTIMP long long int __cdecl llrintf(float x);
#endif /* _MSC_VER < 1800 */
/**
 * \ingroup CUDA_MATH_DOUBLE
 * \brief Round the input argument to the nearest integer.
 *
 * Round argument \p x to an integer value in double precision floating-point format.
 *
 * \return 
 * - nearbyint(
 * \latexonly $\pm 0$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mn>0</m:mn>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns 
 * \latexonly $\pm 0$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mn>0</m:mn>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 * - nearbyint(
 * \latexonly $\pm \infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns 
 * \latexonly $\pm \infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 *
 * \note_accuracy_double
 */
#if _MSC_VER < 1800
extern __host__ __device__ __device_builtin__ double                 nearbyint(double x) __THROW;
#else /* _MSC_VER < 1800 */
extern __host__ __device__ __device_builtin__ _CRTIMP double __cdecl nearbyint(double x);
#endif /* _MSC_VER < 1800 */
/**
 * \ingroup CUDA_MATH_SINGLE
 * \brief Round the input argument to the nearest integer.
 *
 * Round argument \p x to an integer value in single precision floating-point format.
 *
 * \return 
 * - nearbyintf(
 * \latexonly $\pm 0$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mn>0</m:mn>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns 
 * \latexonly $\pm 0$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mn>0</m:mn>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 * - nearbyintf(
 * \latexonly $\pm \infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns 
 * \latexonly $\pm \infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 *
 * \note_accuracy_single
 */
#if _MSC_VER < 1800
extern __host__ __device__ __device_builtin__ float                  nearbyintf(float x) __THROW;
#else /* _MSC_VER < 1800 */
extern __host__ __device__ __device_builtin__ _CRTIMP float  __cdecl nearbyintf(float x);
#endif /* _MSC_VER < 1800 */
/**
 * \ingroup CUDA_MATH_DOUBLE
 * \brief Calculate ceiling of the input argument.
 *
 * Compute the smallest integer value not less than \p x.
 *
 * \return
 * Returns 
 * \latexonly $\lceil x \rceil$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo fence="false" stretchy="false">&#x2308;<!-- ⌈ --></m:mo>
 *   <m:mi>x</m:mi>
 *   <m:mo fence="false" stretchy="false">&#x2309;<!-- ⌉ --></m:mo>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 expressed as a floating-point number.
 * - ceil(
 * \latexonly $\pm 0$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mn>0</m:mn>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns 
 * \latexonly $\pm 0$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mn>0</m:mn>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 * - ceil(
 * \latexonly $\pm \infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns 
 * \latexonly $\pm \infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 */
extern __host__ __device__ __device_builtin__ _CRTIMP double __cdecl ceil(double x) __THROW;
/**
 * \ingroup CUDA_MATH_DOUBLE
 * \brief Truncate input argument to the integral part.
 *
 * Round \p x to the nearest integer value that does not exceed \p x in 
 * magnitude.
 *
 * \return 
 * Returns truncated integer value.
 */
#if _MSC_VER < 1800
extern __host__ __device__ __device_builtin__ double                 trunc(double x) __THROW;
#else /* _MSC_VER < 1800 */
extern __host__ __device__ __device_builtin__ _CRTIMP double __cdecl trunc(double x);
#endif /* _MSC_VER < 1800 */
/**
 * \ingroup CUDA_MATH_SINGLE
 * \brief Truncate input argument to the integral part.
 *
 * Round \p x to the nearest integer value that does not exceed \p x in 
 * magnitude.
 *
 * \return 
 * Returns truncated integer value.
 */
#if _MSC_VER < 1800
extern __host__ __device__ __device_builtin__ float                  truncf(float x) __THROW;
#else /* _MSC_VER < 1800 */
extern __host__ __device__ __device_builtin__ _CRTIMP float  __cdecl truncf(float x);
#endif /* _MSC_VER < 1800 */
/**
 * \ingroup CUDA_MATH_DOUBLE
 * \brief Compute the positive difference between \p x and \p y.
 *
 * Compute the positive difference between \p x and \p y.  The positive
 * difference is \p x - \p y when \p x > \p y and +0 otherwise.
 *
 * \return 
 * Returns the positive difference between \p x and \p y.
 * - fdim(\p x, \p y) returns \p x - \p y if \p x > \p y.
 * - fdim(\p x, \p y) returns +0 if \p x 
 * \latexonly $\leq$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x2264;<!-- ≤ --></m:mo>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly \p y.
 * \note_accuracy_single
 */
#if _MSC_VER < 1800
extern __host__ __device__ __device_builtin__ double                 fdim(double x, double y) __THROW;
#else /* _MSC_VER < 1800 */
extern __host__ __device__ __device_builtin__ _CRTIMP double __cdecl fdim(double x, double y);
#endif /* _MSC_VER < 1800 */
/**
 * \ingroup CUDA_MATH_SINGLE
 * \brief Compute the positive difference between \p x and \p y.
 *
 * Compute the positive difference between \p x and \p y.  The positive
 * difference is \p x - \p y when \p x > \p y and +0 otherwise.
 *
 * \return 
 * Returns the positive difference between \p x and \p y.
 * - fdimf(\p x, \p y) returns \p x - \p y if \p x > \p y.
 * - fdimf(\p x, \p y) returns +0 if \p x 
 * \latexonly $\leq$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x2264;<!-- ≤ --></m:mo>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly \p y.
 * \note_accuracy_single
 */
#if _MSC_VER < 1800
extern __host__ __device__ __device_builtin__ float                  fdimf(float x, float y) __THROW;
#else /* _MSC_VER < 1800 */
extern __host__ __device__ __device_builtin__ _CRTIMP float  __cdecl fdimf(float x, float y);
#endif /* _MSC_VER < 1800 */
/**
 * \ingroup CUDA_MATH_DOUBLE
 * \brief Calculate the arc tangent of the ratio of first and second input arguments.
 *
 * Calculate the principal value of the arc tangent of the ratio of first
 * and second input arguments \p y / \p x. The quadrant of the result is
 * determined by the signs of inputs \p y and \p x.
 *
 * \return 
 * Result will be in radians, in the interval [-
 * \latexonly $\pi$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mi>&#x03C0;<!-- π --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * /, +
 * \latexonly $\pi$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mi>&#x03C0;<!-- π --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ].
 * - atan2(0, 1) returns +0.
 *
 * \note_accuracy_double
 */
extern __host__ __device__ __device_builtin__ double         __cdecl atan2(double y, double x) __THROW;
/**
 * \ingroup CUDA_MATH_DOUBLE
 * \brief Calculate the arc tangent of the input argument.
 *
 * Calculate the principal value of the arc tangent of the input argument \p x.
 *
 * \return 
 * Result will be in radians, in the interval [-
 * \latexonly $\pi$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mi>&#x03C0;<!-- π --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * /2, +
 * \latexonly $\pi$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mi>&#x03C0;<!-- π --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * /2].
 * - atan(0) returns +0.
 *
 * \note_accuracy_double
 */
extern __host__ __device__ __device_builtin__ double         __cdecl atan(double x) __THROW;
/**
 * \ingroup CUDA_MATH_DOUBLE
 * \brief Calculate the arc cosine of the input argument.
 *
 * Calculate the principal value of the arc cosine of the input argument \p x.
 *
 * \return 
 * Result will be in radians, in the interval [0, 
 * \latexonly $\pi$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mi>&#x03C0;<!-- π --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ] for \p x inside [-1, +1].
 * - acos(1) returns +0.
 * - acos(\p x) returns NaN for \p x outside [-1, +1].
 *
 * \note_accuracy_double
 */
extern __host__ __device__ __device_builtin__ double         __cdecl acos(double x) __THROW;
/**
 * \ingroup CUDA_MATH_DOUBLE
 * \brief Calculate the arc sine of the input argument.
 *
 * Calculate the principal value of the arc sine of the input argument \p x.
 *
 * \return 
 * Result will be in radians, in the interval [-
 * \latexonly $\pi$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mi>&#x03C0;<!-- π --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * /2, +
 * \latexonly $\pi$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mi>&#x03C0;<!-- π --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * /2] for \p x inside [-1, +1].
 * - asin(0) returns +0.
 * - asin(\p x) returns NaN for \p x outside [-1, +1].
 *
 * \note_accuracy_double
 */
extern __host__ __device__ __device_builtin__ double         __cdecl asin(double x) __THROW;
/**
 * \ingroup CUDA_MATH_DOUBLE
 * \brief Calculate the square root of the sum of squares of two arguments.
 *
 * Calculate the length of the hypotenuse of a right triangle whose two sides have lengths 
 * \p x and \p y without undue overflow or underflow.
 *
 * \return Returns the length of the hypotenuse 
 * \latexonly $\sqrt{x^2+y^2}$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:msqrt>
 *     <m:msup>
 *       <m:mi>x</m:mi>
 *       <m:mn>2</m:mn>
 *     </m:msup>
 *     <m:mo>+</m:mo>
 *     <m:msup>
 *       <m:mi>y</m:mi>
 *       <m:mn>2</m:mn>
 *     </m:msup>
 *   </m:msqrt>
 * </m:math>
 * </d4p_MathML>\endxmlonly. 
 * If the correct value would overflow, returns 
 * \latexonly $+\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>+</m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 * If the correct value would underflow, returns 0.
 *
 * \note_accuracy_double
 */
#if _MSC_VER < 1600
extern __host__ __device__ __device_builtin__ _CRTIMP double __cdecl hypot(double x, double y) __THROW;
#else
static __host__ __device__ __device_builtin__ double       __CRTDECL hypot(double x, double y) __THROW;
#endif /* _MSC_VER < 1600 */

/**
 * \ingroup CUDA_MATH_DOUBLE
 * \brief Calculate one over the square root of the sum of squares of two arguments.
 *
 * Calculate one over the length of the hypotenuse of a right triangle whose two sides have 
 * lengths \p x and \p y without undue overflow or underflow.
 *
 * \return Returns one over the length of the hypotenuse 
 * \latexonly $\frac{1}{\sqrt{x^2+y^2}}$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mfrac>
 *     <m:mrow>
 *       <m:mi>1</m:mi>
 *     </m:mrow>
 *     <m:mrow>
 *       <m:msqrt>
 *         <m:msup>
 *           <m:mi>x</m:mi>
 *           <m:mn>2</m:mn>
 *         </m:msup>
 *         <m:mo>+</m:mo>
 *         <m:msup>
 *           <m:mi>y</m:mi>
 *           <m:mn>2</m:mn>
 *         </m:msup>
 *       </m:msqrt>
 *     </m:mrow>
 *   </m:mfrac>
 * </m:math>
 * </d4p_MathML>\endxmlonly. 
 * If the square root would overflow, returns 0.
 * If the square root would underflow, returns
 * \latexonly $+\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>+</m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 *
 * \note_accuracy_double
 */
extern __host__ __device__ __device_builtin__ double                rhypot(double x, double y) __THROW;

/**
 * \ingroup CUDA_MATH_SINGLE
 * \brief Calculate the square root of the sum of squares of two arguments.
 *
 * Calculates the length of the hypotenuse of a right triangle whose two sides have lengths 
 * \p x and \p y without undue overflow or underflow.
 *
 * \return Returns the length of the hypotenuse 
 * \latexonly $\sqrt{x^2+y^2}$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:msqrt>
 *     <m:msup>
 *       <m:mi>x</m:mi>
 *       <m:mn>2</m:mn>
 *     </m:msup>
 *     <m:mo>+</m:mo>
 *     <m:msup>
 *       <m:mi>y</m:mi>
 *       <m:mn>2</m:mn>
 *     </m:msup>
 *   </m:msqrt>
 * </m:math>
 * </d4p_MathML>\endxmlonly. 
 * If the correct value would overflow, returns 
 * \latexonly $+\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>+</m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 * If the correct value would underflow, returns 0.
 *
 * \note_accuracy_single
 */
#if _MSC_VER < 1600
extern __host__ __device__ __device_builtin__ float                  hypotf(float x, float y) __THROW;
#else
static __host__ __device__ __device_builtin__ float        __CRTDECL hypotf(float x, float y) __THROW;
#endif /* _MSC_VER < 1600 */

/**
 * \ingroup CUDA_MATH_SINGLE
 * \brief Calculate one over the square root of the sum of squares of two arguments.
 *
 * Calculates one over the length of the hypotenuse of a right triangle whose two sides have 
 * lengths \p x and \p y without undue overflow or underflow.
 *
 * \return Returns one over the length of the hypotenuse 
 * \latexonly $\frac{1}{\sqrt{x^2+y^2}}$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mfrac>
 *     <m:mrow>
 *       <m:mi>1</m:mi>
 *     </m:mrow>
 *     <m:mrow>
 *       <m:msqrt>
 *         <m:msup>
 *           <m:mi>x</m:mi>
 *           <m:mn>2</m:mn>
 *         </m:msup>
 *         <m:mo>+</m:mo>
 *         <m:msup>
 *           <m:mi>y</m:mi>
 *           <m:mn>2</m:mn>
 *         </m:msup>
 *       </m:msqrt>
 *     </m:mrow>
 *   </m:mfrac>
 * </m:math>
 * </d4p_MathML>\endxmlonly. 
 * If the square root would overflow, returns 0.
 * If the square root would underflow, returns
 * \latexonly $+\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>+</m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 *
 * \note_accuracy_single
 */
extern __host__ __device__ __device_builtin__ float                 rhypotf(float x, float y) __THROW;

/**
 * \ingroup CUDA_MATH_DOUBLE
 * \brief Calculate the cube root of the input argument.
 *
 * Calculate the cube root of \p x, 
 * \latexonly $x^{1/3}$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:msup>
 *     <m:mi>x</m:mi>
 *     <m:mrow class="MJX-TeXAtom-ORD">
 *       <m:mn>1</m:mn>
 *       <m:mrow class="MJX-TeXAtom-ORD">
 *         <m:mo>/</m:mo>
 *       </m:mrow>
 *       <m:mn>3</m:mn>
 *     </m:mrow>
 *   </m:msup>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 *
 * \return 
 * Returns 
 * \latexonly $x^{1/3}$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:msup>
 *     <m:mi>x</m:mi>
 *     <m:mrow class="MJX-TeXAtom-ORD">
 *       <m:mn>1</m:mn>
 *       <m:mrow class="MJX-TeXAtom-ORD">
 *         <m:mo>/</m:mo>
 *       </m:mrow>
 *       <m:mn>3</m:mn>
 *     </m:mrow>
 *   </m:msup>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 * - cbrt(
 * \latexonly $\pm 0$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mn>0</m:mn>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns 
 * \latexonly $\pm 0$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mn>0</m:mn>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 * - cbrt(
 * \latexonly $\pm \infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns 
 * \latexonly $\pm \infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 *
 * \note_accuracy_double
 */
#if _MSC_VER < 1800
extern __host__ __device__ __device_builtin__ double                 cbrt(double x) __THROW;
#else /* _MSC_VER < 1800 */
extern __host__ __device__ __device_builtin__ _CRTIMP double __cdecl cbrt(double x);
#endif /* _MSC_VER < 1800 */
/**
 * \ingroup CUDA_MATH_SINGLE
 * \brief Calculate the cube root of the input argument.
 *
 * Calculate the cube root of \p x, 
 * \latexonly $x^{1/3}$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:msup>
 *     <m:mi>x</m:mi>
 *     <m:mrow class="MJX-TeXAtom-ORD">
 *       <m:mn>1</m:mn>
 *       <m:mrow class="MJX-TeXAtom-ORD">
 *         <m:mo>/</m:mo>
 *       </m:mrow>
 *       <m:mn>3</m:mn>
 *     </m:mrow>
 *   </m:msup>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 *
 * \return 
 * Returns 
 * \latexonly $x^{1/3}$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:msup>
 *     <m:mi>x</m:mi>
 *     <m:mrow class="MJX-TeXAtom-ORD">
 *       <m:mn>1</m:mn>
 *       <m:mrow class="MJX-TeXAtom-ORD">
 *         <m:mo>/</m:mo>
 *       </m:mrow>
 *       <m:mn>3</m:mn>
 *     </m:mrow>
 *   </m:msup>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 * - cbrtf(
 * \latexonly $\pm 0$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mn>0</m:mn>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns 
 * \latexonly $\pm 0$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mn>0</m:mn>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 * - cbrtf(
 * \latexonly $\pm \infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns 
 * \latexonly $\pm \infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 *
 * \note_accuracy_single
 */
#if _MSC_VER < 1800
extern __host__ __device__ __device_builtin__ float                  cbrtf(float x) __THROW;
#else /* _MSC_VER < 1800 */
extern __host__ __device__ __device_builtin__ _CRTIMP float  __cdecl cbrtf(float x);
#endif /* _MSC_VER < 1800 */
/**
 * \ingroup CUDA_MATH_DOUBLE
 * \brief Calculate reciprocal cube root function.
 *
 * Calculate reciprocal cube root function of \p x
 *
 * \return 
 * - rcbrt(
 * \latexonly $\pm 0$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mn>0</m:mn>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns 
 * \latexonly $\pm \infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 * - rcbrt(
 * \latexonly $\pm \infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns 
 * \latexonly $\pm 0$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mn>0</m:mn>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 *
 * \note_accuracy_double
 */
extern __host__ __device__ __device_builtin__ double                 rcbrt(double x);
/**
 * \ingroup CUDA_MATH_SINGLE
 * \brief Calculate reciprocal cube root function.
 *
 * Calculate reciprocal cube root function of \p x
 *
 * \return 
 * - rcbrt(
 * \latexonly $\pm 0$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mn>0</m:mn>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns 
 * \latexonly $\pm \infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 * - rcbrt(
 * \latexonly $\pm \infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns 
 * \latexonly $\pm 0$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mn>0</m:mn>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 *
 * \note_accuracy_single
 */
extern __host__ __device__ __device_builtin__ float                  rcbrtf(float x);
/**
 * \ingroup CUDA_MATH_DOUBLE
 * \brief Calculate the sine of the input argument 
 * \latexonly $\times \pi$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00D7;<!-- × --></m:mo>
 *   <m:mi>&#x03C0;<!-- π --></m:mi>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 *
 * Calculate the sine of \p x
 * \latexonly $\times \pi$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00D7;<!-- × --></m:mo>
 *   <m:mi>&#x03C0;<!-- π --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 *  (measured in radians), 
 * where \p x is the input argument.
 *
 * \return 
 * - sinpi(
 * \latexonly $\pm 0$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mn>0</m:mn>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns 
 * \latexonly $\pm 0$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mn>0</m:mn>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 * - sinpi(
 * \latexonly $\pm \infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns NaN.
 *
 * \note_accuracy_double
 */
extern __host__ __device__ __device_builtin__ double                 sinpi(double x);
/**
 * \ingroup CUDA_MATH_SINGLE
 * \brief Calculate the sine of the input argument 
 * \latexonly $\times \pi$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00D7;<!-- × --></m:mo>
 *   <m:mi>&#x03C0;<!-- π --></m:mi>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 *
 * Calculate the sine of \p x
 * \latexonly $\times \pi$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00D7;<!-- × --></m:mo>
 *   <m:mi>&#x03C0;<!-- π --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 *  (measured in radians), 
 * where \p x is the input argument.
 *
 * \return 
 * - sinpif(
 * \latexonly $\pm 0$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mn>0</m:mn>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns 
 * \latexonly $\pm 0$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mn>0</m:mn>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 * - sinpif(
 * \latexonly $\pm \infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns NaN.
 *
 * \note_accuracy_single
 */
extern __host__ __device__ __device_builtin__ float                  sinpif(float x);
/**
 * \ingroup CUDA_MATH_DOUBLE
 * \brief Calculate the cosine of the input argument 
 * \latexonly $\times \pi$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00D7;<!-- × --></m:mo>
 *   <m:mi>&#x03C0;<!-- π --></m:mi>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 *
 * Calculate the cosine of \p x
 * \latexonly $\times \pi$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00D7;<!-- × --></m:mo>
 *   <m:mi>&#x03C0;<!-- π --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 *  (measured in radians), 
 * where \p x is the input argument.
 *
 * \return 
 * - cospi(
 * \latexonly $\pm 0$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mn>0</m:mn>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns 1.
 * - cospi(
 * \latexonly $\pm \infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns NaN.
 *
 * \note_accuracy_double
 */
extern __host__ __device__ __device_builtin__ double                 cospi(double x);
/**
 * \ingroup CUDA_MATH_SINGLE
 * \brief Calculate the cosine of the input argument 
 * \latexonly $\times \pi$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00D7;<!-- × --></m:mo>
 *   <m:mi>&#x03C0;<!-- π --></m:mi>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 *
 * Calculate the cosine of \p x
 * \latexonly $\times \pi$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00D7;<!-- × --></m:mo>
 *   <m:mi>&#x03C0;<!-- π --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 *  (measured in radians),
 * where \p x is the input argument.
 *
 * \return 
 * - cospif(
 * \latexonly $\pm 0$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mn>0</m:mn>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns 1.
 * - cospif(
 * \latexonly $\pm \infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns NaN.
 *
 * \note_accuracy_single
 */
extern __host__ __device__ __device_builtin__ float                  cospif(float x);
/**
 * \ingroup CUDA_MATH_DOUBLE
 * \brief  Calculate the sine and cosine of the first input argument 
 * \latexonly $\times \pi$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00D7;<!-- × --></m:mo>
 *   <m:mi>&#x03C0;<!-- π --></m:mi>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 *
 * Calculate the sine and cosine of the first input argument, \p x (measured in radians), 
 * \latexonly $\times \pi$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00D7;<!-- × --></m:mo>
 *   <m:mi>&#x03C0;<!-- π --></m:mi>
 * </m:math>
 * </d4p_MathML>\endxmlonly.  The results for sine and cosine are written into the
 * second argument, \p sptr, and, respectively, third argument, \p cptr.
 *
 * \return 
 * - none
 *
 * \see ::sinpi() and ::cospi().
 * \note_accuracy_double
 */
extern __host__ __device__ __device_builtin__ void                   sincospi(double x, double *sptr, double *cptr);
/**
 * \ingroup CUDA_MATH_SINGLE
 * \brief  Calculate the sine and cosine of the first input argument 
 * \latexonly $\times \pi$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00D7;<!-- × --></m:mo>
 *   <m:mi>&#x03C0;<!-- π --></m:mi>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 *
 * Calculate the sine and cosine of the first input argument, \p x (measured in radians), 
 * \latexonly $\times \pi$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00D7;<!-- × --></m:mo>
 *   <m:mi>&#x03C0;<!-- π --></m:mi>
 * </m:math>
 * </d4p_MathML>\endxmlonly.  The results for sine and cosine are written into the
 * second argument, \p sptr, and, respectively, third argument, \p cptr.
 *
 * \return 
 * - none
 *
 * \see ::sinpif() and ::cospif().
 * \note_accuracy_single
 */
extern __host__ __device__ __device_builtin__ void                   sincospif(float x, float *sptr, float *cptr);
/**
 * \ingroup CUDA_MATH_DOUBLE
 * \brief Calculate the value of first argument to the power of second argument.
 *
 * Calculate the value of \p x to the power of \p y
 *
 * \return 
 * - pow(
 * \latexonly $\pm 0$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mn>0</m:mn>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * , \p y) returns 
 * \latexonly $\pm \infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 *  for \p y an integer less than 0.
 * - pow(
 * \latexonly $\pm 0$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mn>0</m:mn>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * , \p y) returns 
 * \latexonly $\pm 0$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mn>0</m:mn>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 *  for \p y an odd integer greater than 0.
 * - pow(
 * \latexonly $\pm 0$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mn>0</m:mn>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * , \p y) returns +0 for \p y > 0 and not and odd integer.
 * - pow(-1, 
 * \latexonly $\pm \infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns 1.
 * - pow(+1, \p y) returns 1 for any \p y, even a NaN.
 * - pow(\p x, 
 * \latexonly $\pm 0$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mn>0</m:mn>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns 1 for any \p x, even a NaN.
 * - pow(\p x, \p y) returns a NaN for finite \p x < 0 and finite non-integer \p y.
 * - pow(\p x, 
 * \latexonly $-\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x2212;<!-- − --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns 
 * \latexonly $+\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>+</m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 *  for 
 * \latexonly $| x | < 1$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mrow class="MJX-TeXAtom-ORD">
 *     <m:mo stretchy="false">|</m:mo>
 *   </m:mrow>
 *   <m:mi>x</m:mi>
 *   <m:mrow class="MJX-TeXAtom-ORD">
 *     <m:mo stretchy="false">|</m:mo>
 *   </m:mrow>
 *   <m:mo>&lt;</m:mo>
 *   <m:mn>1</m:mn>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 * - pow(\p x, 
 * \latexonly $-\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x2212;<!-- − --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns +0 for 
 * \latexonly $| x | > 1$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mrow class="MJX-TeXAtom-ORD">
 *     <m:mo stretchy="false">|</m:mo>
 *   </m:mrow>
 *   <m:mi>x</m:mi>
 *   <m:mrow class="MJX-TeXAtom-ORD">
 *     <m:mo stretchy="false">|</m:mo>
 *   </m:mrow>
 *   <m:mo>&gt;</m:mo>
 *   <m:mn>1</m:mn>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 * - pow(\p x, 
 * \latexonly $+\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>+</m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns +0 for 
 * \latexonly $| x | < 1$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mrow class="MJX-TeXAtom-ORD">
 *     <m:mo stretchy="false">|</m:mo>
 *   </m:mrow>
 *   <m:mi>x</m:mi>
 *   <m:mrow class="MJX-TeXAtom-ORD">
 *     <m:mo stretchy="false">|</m:mo>
 *   </m:mrow>
 *   <m:mo>&lt;</m:mo>
 *   <m:mn>1</m:mn>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 * - pow(\p x, 
 * \latexonly $+\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>+</m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns 
 * \latexonly $+\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>+</m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 *  for 
 * \latexonly $| x | > 1$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mrow class="MJX-TeXAtom-ORD">
 *     <m:mo stretchy="false">|</m:mo>
 *   </m:mrow>
 *   <m:mi>x</m:mi>
 *   <m:mrow class="MJX-TeXAtom-ORD">
 *     <m:mo stretchy="false">|</m:mo>
 *   </m:mrow>
 *   <m:mo>&gt;</m:mo>
 *   <m:mn>1</m:mn>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 * - pow(
 * \latexonly $-\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x2212;<!-- − --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * , \p y) returns -0 for \p y an odd integer less than 0.
 * - pow(
 * \latexonly $-\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x2212;<!-- − --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * , \p y) returns +0 for \p y < 0 and not an odd integer.
 * - pow(
 * \latexonly $-\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x2212;<!-- − --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * , \p y) returns 
 * \latexonly $-\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x2212;<!-- − --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 *  for \p y an odd integer greater than 0.
 * - pow(
 * \latexonly $-\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x2212;<!-- − --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * , \p y) returns 
 * \latexonly $+\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>+</m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 *  for \p y > 0 and not an odd integer.
 * - pow(
 * \latexonly $+\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>+</m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * , \p y) returns +0 for \p y < 0.
 * - pow(
 * \latexonly $+\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>+</m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * , \p y) returns 
 * \latexonly $+\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>+</m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 *  for \p y > 0.
 *
 * \note_accuracy_double
 */
extern __host__ __device__ __device_builtin__ double         __cdecl pow(double x, double y) __THROW;
/**
 * \ingroup CUDA_MATH_DOUBLE
 * \brief Break down the input argument into fractional and integral parts.
 *
 * Break down the argument \p x into fractional and integral parts. The 
 * integral part is stored in the argument \p iptr.
 * Fractional and integral parts are given the same sign as the argument \p x.
 *
 * \return 
 * - modf(
 * \latexonly $\pm x$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *  <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mi>x</m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * , \p iptr) returns a result with the same sign as \p x.
 * - modf(
 * \latexonly $\pm \infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * , \p iptr) returns 
 * \latexonly $\pm 0$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mn>0</m:mn>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 *  and stores 
 * \latexonly $\pm \infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 *   in the object pointed to by \p iptr.
 * - modf(NaN, \p iptr) stores a NaN in the object pointed to by \p iptr and returns a NaN.
 *
 * \note_accuracy_double
 */
extern __host__ __device__ __device_builtin__ _CRTIMP double __cdecl modf(double x, double *iptr) __THROW;
/**
 * \ingroup CUDA_MATH_DOUBLE
 * \brief Calculate the floating-point remainder of \p x / \p y.
 *
 * Calculate the floating-point remainder of \p x / \p y. 
 * The absolute value of the computed value is always less than \p y's
 * absolute value and will have the same sign as \p x.
 *
 * \return
 * - Returns the floating point remainder of \p x / \p y.
 * - fmod(
 * \latexonly $\pm 0$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mn>0</m:mn>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * , \p y) returns 
 * \latexonly $\pm 0$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mn>0</m:mn>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 *  if \p y is not zero.
 * - fmod(\p x, \p y) returns NaN and raised an invalid floating point exception if \p x is 
 * \latexonly $\pm\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 *  or \p y is zero.
 * - fmod(\p x, \p y) returns zero if \p y is zero or the result would overflow.
 * - fmod(\p x, 
 * \latexonly $\pm \infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns \p x if \p x is finite.
 * - fmod(\p x, 0) returns NaN.
 *
 * \note_accuracy_double
 */
extern __host__ __device__ __device_builtin__ double         __cdecl fmod(double x, double y) __THROW;
/**
 * \ingroup CUDA_MATH_DOUBLE
 * \brief Compute double-precision floating-point remainder.
 *
 * Compute double-precision floating-point remainder \p r of dividing 
 * \p x by \p y for nonzero \p y. Thus 
 * \latexonly $ r = x - n y$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mi>r</m:mi>
 *   <m:mo>=</m:mo>
 *   <m:mi>x</m:mi>
 *   <m:mo>&#x2212;<!-- − --></m:mo>
 *   <m:mi>n</m:mi>
 *   <m:mi>y</m:mi>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 * The value \p n is the integer value nearest 
 * \latexonly $ \frac{x}{y} $ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mfrac>
 *     <m:mi>x</m:mi>
 *     <m:mi>y</m:mi>
 *   </m:mfrac>
 * </m:math>
 * </d4p_MathML>\endxmlonly. 
 * In the case when 
 * \latexonly $ | n -\frac{x}{y} | = \frac{1}{2} $ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mrow class="MJX-TeXAtom-ORD">
 *     <m:mo stretchy="false">|</m:mo>
 *   </m:mrow>
 *   <m:mi>n</m:mi>
 *   <m:mo>&#x2212;<!-- − --></m:mo>
 *   <m:mfrac>
 *     <m:mi>x</m:mi>
 *     <m:mi>y</m:mi>
 *   </m:mfrac>
 *   <m:mrow class="MJX-TeXAtom-ORD">
 *     <m:mo stretchy="false">|</m:mo>
 *   </m:mrow>
 *   <m:mo>=</m:mo>
 *   <m:mfrac>
 *     <m:mn>1</m:mn>
 *     <m:mn>2</m:mn>
 *   </m:mfrac>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * , the
 * even \p n value is chosen.
 *
 * \return 
 * - remainder(\p x, 0) returns NaN.
 * - remainder(
 * \latexonly $\pm \infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * , \p y) returns NaN.
 * - remainder(\p x, 
 * \latexonly $\pm \infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns \p x for finite \p x.
 *
 * \note_accuracy_double
 */
#if _MSC_VER < 1800
extern __host__ __device__ __device_builtin__ double                 remainder(double x, double y) __THROW;
#else /* _MSC_VER < 1800 */
extern __host__ __device__ __device_builtin__ _CRTIMP double __cdecl remainder(double x, double y);
#endif /* _MSC_VER < 1800 */
/**
 * \ingroup CUDA_MATH_SINGLE
 * \brief Compute single-precision floating-point remainder.
 *
 * Compute single-precision floating-point remainder \p r of dividing 
 * \p x by \p y for nonzero \p y. Thus 
 * \latexonly $ r = x - n y$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mi>r</m:mi>
 *   <m:mo>=</m:mo>
 *   <m:mi>x</m:mi>
 *   <m:mo>&#x2212;<!-- − --></m:mo>
 *   <m:mi>n</m:mi>
 *   <m:mi>y</m:mi>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 * The value \p n is the integer value nearest 
 * \latexonly $ \frac{x}{y} $ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mfrac>
 *     <m:mi>x</m:mi>
 *     <m:mi>y</m:mi>
 *   </m:mfrac>
 * </m:math>
 * </d4p_MathML>\endxmlonly. 
 * In the case when 
 * \latexonly $ | n -\frac{x}{y} | = \frac{1}{2} $ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mrow class="MJX-TeXAtom-ORD">
 *     <m:mo stretchy="false">|</m:mo>
 *   </m:mrow>
 *   <m:mi>n</m:mi>
 *   <m:mo>&#x2212;<!-- − --></m:mo>
 *   <m:mfrac>
 *     <m:mi>x</m:mi>
 *     <m:mi>y</m:mi>
 *   </m:mfrac>
 *   <m:mrow class="MJX-TeXAtom-ORD">
 *     <m:mo stretchy="false">|</m:mo>
 *   </m:mrow>
 *   <m:mo>=</m:mo>
 *   <m:mfrac>
 *     <m:mn>1</m:mn>
 *     <m:mn>2</m:mn>
 *   </m:mfrac>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * , the
 * even \p n value is chosen.
 *
 * \return 
 * \return 
 * - remainderf(\p x, 0) returns NaN.
 * - remainderf(
 * \latexonly $\pm \infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * , \p y) returns NaN.
 * - remainderf(\p x, 
 * \latexonly $\pm \infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns \p x for finite \p x.
 *
 * \note_accuracy_single
 */
#if _MSC_VER < 1800
extern __host__ __device__ __device_builtin__ float                  remainderf(float x, float y) __THROW;
#else /* _MSC_VER < 1800 */
extern __host__ __device__ __device_builtin__ _CRTIMP float  __cdecl remainderf(float x, float y);
#endif /* _MSC_VER < 1800 */
/**
 * \ingroup CUDA_MATH_DOUBLE
 * \brief Compute double-precision floating-point remainder and part of quotient.
 *
 * Compute a double-precision floating-point remainder in the same way as the
 * ::remainder() function. Argument \p quo returns part of quotient upon 
 * division of \p x by \p y. Value \p quo has the same sign as 
 * \latexonly $ \frac{x}{y} $ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mfrac>
 *     <m:mi>x</m:mi>
 *     <m:mi>y</m:mi>
 *   </m:mfrac>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * and may not be the exact quotient but agrees with the exact quotient
 * in the low order 3 bits.
 *
 * \return 
 * Returns the remainder.
 * - remquo(\p x, 0, \p quo) returns NaN.
 * - remquo(
 * \latexonly $\pm \infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * , \p y, \p quo) returns NaN.
 * - remquo(\p x, 
 * \latexonly $\pm \infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * , \p quo) returns \p x.
 *
 * \note_accuracy_double
 */
#if _MSC_VER < 1800
extern __host__ __device__ __device_builtin__ double                 remquo(double x, double y, int *quo) __THROW;
#else /* _MSC_VER < 1800 */
extern __host__ __device__ __device_builtin__ _CRTIMP double __cdecl remquo(double x, double y, int *quo);
#endif /* _MSC_VER < 1800 */
/**
 * \ingroup CUDA_MATH_SINGLE
 * \brief Compute single-precision floating-point remainder and part of quotient.
 *
 * Compute a double-precision floating-point remainder in the same way as the 
 * ::remainderf() function. Argument \p quo returns part of quotient upon 
 * division of \p x by \p y. Value \p quo has the same sign as 
 * \latexonly $ \frac{x}{y} $ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mfrac>
 *     <m:mi>x</m:mi>
 *     <m:mi>y</m:mi>
 *   </m:mfrac>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * and may not be the exact quotient but agrees with the exact quotient
 * in the low order 3 bits.
 *
 * \return 
 * Returns the remainder.
 * - remquof(\p x, 0, \p quo) returns NaN.
 * - remquof(
 * \latexonly $\pm \infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * , \p y, \p quo) returns NaN.
 * - remquof(\p x, 
 * \latexonly $\pm \infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * , \p quo) returns \p x.
 *
 * \note_accuracy_single
 */
#if _MSC_VER < 1800
extern __host__ __device__ __device_builtin__ float                  remquof(float x, float y, int *quo) __THROW;
#else /* _MSC_VER < 1800 */
extern __host__ __device__ __device_builtin__ _CRTIMP float  __cdecl remquof(float x, float y, int *quo);
#endif /* _MSC_VER < 1800 */
/**
 * \ingroup CUDA_MATH_DOUBLE
 * \brief Calculate the value of the Bessel function of the first kind of order 0 for the input argument.
 *
 * Calculate the value of the Bessel function of the first kind of order 0 for
 * the input argument \p x, 
 * \latexonly $J_0(x)$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:msub>
 *     <m:mi>J</m:mi>
 *     <m:mn>0</m:mn>
 *   </m:msub>
 *   <m:mo stretchy="false">(</m:mo>
 *   <m:mi>x</m:mi>
 *   <m:mo stretchy="false">)</m:mo>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 *
 * \return
 * Returns the value of the Bessel function of the first kind of order 0.
 * - j0(
 * \latexonly $\pm \infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns +0.
 * - j0(NaN) returns NaN.
 *
 * \note_accuracy_double
 */
extern __host__ __device__ __device_builtin__ _CRTIMP double __cdecl j0(double x) __THROW;
/**
 * \ingroup CUDA_MATH_SINGLE
 * \brief Calculate the value of the Bessel function of the first kind of order 0 for the input argument.
 *
 * Calculate the value of the Bessel function of the first kind of order 0 for
 * the input argument \p x, 
 * \latexonly $J_0(x)$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:msub>
 *     <m:mi>J</m:mi>
 *     <m:mn>0</m:mn>
 *   </m:msub>
 *   <m:mo stretchy="false">(</m:mo>
 *   <m:mi>x</m:mi>
 *   <m:mo stretchy="false">)</m:mo>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 *
 * \return
 * Returns the value of the Bessel function of the first kind of order 0.
 * - j0f(
 * \latexonly $\pm \infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns +0.
 * - j0f(NaN) returns NaN.
 *
 * \note_accuracy_single
 */
extern __host__ __device__ __device_builtin__ float                  j0f(float x) __THROW;
/**
 * \ingroup CUDA_MATH_DOUBLE
 * \brief Calculate the value of the Bessel function of the first kind of order 1 for the input argument.
 *
 * Calculate the value of the Bessel function of the first kind of order 1 for
 * the input argument \p x, 
 * \latexonly $J_1(x)$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:msub>
 *     <m:mi>J</m:mi>
 *     <m:mn>1</m:mn>
 *   </m:msub>
 *   <m:mo stretchy="false">(</m:mo>
 *   <m:mi>x</m:mi>
 *   <m:mo stretchy="false">)</m:mo>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 *
 * \return
 * Returns the value of the Bessel function of the first kind of order 1.
 * - j1(
 * \latexonly $\pm 0$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mn>0</m:mn>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns 
 * \latexonly $\pm 0$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mn>0</m:mn>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 * - j1(
 * \latexonly $\pm \infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns +0.
 * - j1(NaN) returns NaN.
 *
 * \note_accuracy_double
 */
extern __host__ __device__ __device_builtin__ _CRTIMP double __cdecl j1(double x) __THROW;
/**
 * \ingroup CUDA_MATH_SINGLE
 * \brief Calculate the value of the Bessel function of the first kind of order 1 for the input argument.
 *
 * Calculate the value of the Bessel function of the first kind of order 1 for
 * the input argument \p x, 
 * \latexonly $J_1(x)$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:msub>
 *     <m:mi>J</m:mi>
 *     <m:mn>1</m:mn>
 *   </m:msub>
 *   <m:mo stretchy="false">(</m:mo>
 *   <m:mi>x</m:mi>
 *   <m:mo stretchy="false">)</m:mo>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 *
 * \return
 * Returns the value of the Bessel function of the first kind of order 1.
 * - j1f(
 * \latexonly $\pm 0$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mn>0</m:mn>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns 
 * \latexonly $\pm 0$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mn>0</m:mn>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 * - j1f(
 * \latexonly $\pm \infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns +0.
 * - j1f(NaN) returns NaN.
 *
 * \note_accuracy_single
 */
extern __host__ __device__ __device_builtin__ float                  j1f(float x) __THROW;
/**
 * \ingroup CUDA_MATH_DOUBLE
 * \brief Calculate the value of the Bessel function of the first kind of order n for the input argument.
 *
 * Calculate the value of the Bessel function of the first kind of order \p n for
 * the input argument \p x, 
 * \latexonly $J_n(x)$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:msub>
 *     <m:mi>J</m:mi>
 *     <m:mi>n</m:mi>
 *   </m:msub>
 *   <m:mo stretchy="false">(</m:mo>
 *   <m:mi>x</m:mi>
 *   <m:mo stretchy="false">)</m:mo>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 *
 * \return
 * Returns the value of the Bessel function of the first kind of order \p n.
 * - jn(\p n, NaN) returns NaN.
 * - jn(\p n, \p x) returns NaN for \p n < 0.
 * - jn(\p n, 
 * \latexonly $+\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>+</m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns +0.
 *
 * \note_accuracy_double
 */
extern __host__ __device__ __device_builtin__ _CRTIMP double __cdecl jn(int n, double x) __THROW;
/**
 * \ingroup CUDA_MATH_SINGLE
 * \brief Calculate the value of the Bessel function of the first kind of order n for the input argument.
 *
 * Calculate the value of the Bessel function of the first kind of order \p n for
 * the input argument \p x, 
 * \latexonly $J_n(x)$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:msub>
 *     <m:mi>J</m:mi>
 *     <m:mi>n</m:mi>
 *   </m:msub>
 *   <m:mo stretchy="false">(</m:mo>
 *   <m:mi>x</m:mi>
 *   <m:mo stretchy="false">)</m:mo>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 *
 * \return
 * Returns the value of the Bessel function of the first kind of order \p n.
 * - jnf(\p n, NaN) returns NaN.
 * - jnf(\p n, \p x) returns NaN for \p n < 0.
 * - jnf(\p n, 
 * \latexonly $+\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>+</m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns +0.
 *
 * \note_accuracy_single
 */
extern __host__ __device__ __device_builtin__ float                  jnf(int n, float x) __THROW;
/**
 * \ingroup CUDA_MATH_DOUBLE
 * \brief Calculate the value of the Bessel function of the second kind of order 0 for the input argument.
 *
 * Calculate the value of the Bessel function of the second kind of order 0 for
 * the input argument \p x, 
 * \latexonly $Y_0(x)$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:msub>
 *     <m:mi>Y</m:mi>
 *     <m:mn>0</m:mn>
 *   </m:msub>
 *   <m:mo stretchy="false">(</m:mo>
 *   <m:mi>x</m:mi>
 *   <m:mo stretchy="false">)</m:mo>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 *
 * \return
 * Returns the value of the Bessel function of the second kind of order 0.
 * - y0(0) returns 
 * \latexonly $-\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x2212;<!-- − --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 * - y0(\p x) returns NaN for \p x < 0.
 * - y0(
 * \latexonly $+\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>+</m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns +0.
 * - y0(NaN) returns NaN.
 *
 * \note_accuracy_double
 */
extern __host__ __device__ __device_builtin__ _CRTIMP double __cdecl y0(double x) __THROW;
/**
 * \ingroup CUDA_MATH_SINGLE
 * \brief Calculate the value of the Bessel function of the second kind of order 0 for the input argument.
 *
 * Calculate the value of the Bessel function of the second kind of order 0 for
 * the input argument \p x, 
 * \latexonly $Y_0(x)$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:msub>
 *     <m:mi>Y</m:mi>
 *     <m:mn>0</m:mn>
 *   </m:msub>
 *   <m:mo stretchy="false">(</m:mo>
 *   <m:mi>x</m:mi>
 *   <m:mo stretchy="false">)</m:mo>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 *
 * \return
 * Returns the value of the Bessel function of the second kind of order 0.
 * - y0f(0) returns 
 * \latexonly $-\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x2212;<!-- − --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 * - y0f(\p x) returns NaN for \p x < 0.
 * - y0f(
 * \latexonly $+\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>+</m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns +0.
 * - y0f(NaN) returns NaN.
 *
 * \note_accuracy_single
 */
extern __host__ __device__ __device_builtin__ float                  y0f(float x) __THROW;
/**
 * \ingroup CUDA_MATH_DOUBLE
 * \brief Calculate the value of the Bessel function of the second kind of order 1 for the input argument.
 *
 * Calculate the value of the Bessel function of the second kind of order 1 for
 * the input argument \p x, 
 * \latexonly $Y_1(x)$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:msub>
 *     <m:mi>Y</m:mi>
 *     <m:mn>1</m:mn>
 *   </m:msub>
 *   <m:mo stretchy="false">(</m:mo>
 *   <m:mi>x</m:mi>
 *   <m:mo stretchy="false">)</m:mo>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 *
 * \return
 * Returns the value of the Bessel function of the second kind of order 1.
 * - y1(0) returns 
 * \latexonly $-\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x2212;<!-- − --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 * - y1(\p x) returns NaN for \p x < 0.
 * - y1(
 * \latexonly $+\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>+</m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns +0.
 * - y1(NaN) returns NaN.
 *
 * \note_accuracy_double
 */
extern __host__ __device__ __device_builtin__ _CRTIMP double __cdecl y1(double x) __THROW;
/**
 * \ingroup CUDA_MATH_SINGLE
 * \brief Calculate the value of the Bessel function of the second kind of order 1 for the input argument.
 *
 * Calculate the value of the Bessel function of the second kind of order 1 for
 * the input argument \p x, 
 * \latexonly $Y_1(x)$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:msub>
 *     <m:mi>Y</m:mi>
 *     <m:mn>1</m:mn>
 *   </m:msub>
 *   <m:mo stretchy="false">(</m:mo>
 *   <m:mi>x</m:mi>
 *   <m:mo stretchy="false">)</m:mo>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 *
 * \return
 * Returns the value of the Bessel function of the second kind of order 1.
 * - y1f(0) returns 
 * \latexonly $-\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x2212;<!-- − --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 * - y1f(\p x) returns NaN for \p x < 0.
 * - y1f(
 * \latexonly $+\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>+</m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns +0.
 * - y1f(NaN) returns NaN.
 *
 * \note_accuracy_single
 */
extern __host__ __device__ __device_builtin__ float                  y1f(float x) __THROW;
/**
 * \ingroup CUDA_MATH_DOUBLE
 * \brief Calculate the value of the Bessel function of the second kind of order n for the input argument.
 *
 * Calculate the value of the Bessel function of the second kind of order \p n for
 * the input argument \p x, 
 * \latexonly $Y_n(x)$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:msub>
 *     <m:mi>Y</m:mi>
 *     <m:mi>n</m:mi>
 *   </m:msub>
 *   <m:mo stretchy="false">(</m:mo>
 *   <m:mi>x</m:mi>
 *   <m:mo stretchy="false">)</m:mo>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 *
 * \return
 * Returns the value of the Bessel function of the second kind of order \p n.
 * - yn(\p n, \p x) returns NaN for \p n < 0.
 * - yn(\p n, 0) returns 
 * \latexonly $-\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x2212;<!-- − --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 * - yn(\p n, \p x) returns NaN for \p x < 0.
 * - yn(\p n, 
 * \latexonly $+\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>+</m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns +0.
 * - yn(\p n, NaN) returns NaN.
 *
 * \note_accuracy_double
 */
extern __host__ __device__ __device_builtin__ _CRTIMP double __cdecl yn(int n, double x) __THROW;
/**
 * \ingroup CUDA_MATH_SINGLE
 * \brief Calculate the value of the Bessel function of the second kind of order n for the input argument.
 *
 * Calculate the value of the Bessel function of the second kind of order \p n for
 * the input argument \p x, 
 * \latexonly $Y_n(x)$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:msub>
 *     <m:mi>Y</m:mi>
 *     <m:mi>n</m:mi>
 *   </m:msub>
 *   <m:mo stretchy="false">(</m:mo>
 *   <m:mi>x</m:mi>
 *   <m:mo stretchy="false">)</m:mo>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 *
 * \return
 * Returns the value of the Bessel function of the second kind of order \p n.
 * - ynf(\p n, \p x) returns NaN for \p n < 0.
 * - ynf(\p n, 0) returns 
 * \latexonly $-\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x2212;<!-- − --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 * - ynf(\p n, \p x) returns NaN for \p x < 0.
 * - ynf(\p n, 
 * \latexonly $+\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>+</m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns +0.
 * - ynf(\p n, NaN) returns NaN.
 *
 * \note_accuracy_single
 */
extern __host__ __device__ __device_builtin__ float                  ynf(int n, float x) __THROW;

/**
 * \ingroup CUDA_MATH_DOUBLE
 * \brief Calculate the value of the regular modified cylindrical Bessel function of order 0 for the input argument.
 *
 * Calculate the value of the regular modified cylindrical Bessel function of order 0 for
 * the input argument \p x, 
 * \latexonly $I_0(x)$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:msub>
 *     <m:mi>I</m:mi>
 *     <m:mn>0</m:mn>
 *   </m:msub>
 *   <m:mo stretchy="false">(</m:mo>
 *   <m:mi>x</m:mi>
 *   <m:mo stretchy="false">)</m:mo>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 *
 * \return
 * Returns the value of the regular modified cylindrical Bessel function of order 0.
 *
 * \note_accuracy_double
 */
extern __host__ __device__ __device_builtin__ _CRTIMP double __cdecl cyl_bessel_i0(double x) __THROW;
/**
 * \ingroup CUDA_MATH_SINGLE
 * \brief Calculate the value of the regular modified cylindrical Bessel function of order 0 for the input argument.
 *
 * Calculate the value of the regular modified cylindrical Bessel function of order 0 for
 * the input argument \p x, 
 * \latexonly $I_0(x)$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:msub>
 *     <m:mi>I</m:mi>
 *     <m:mn>0</m:mn>
 *   </m:msub>
 *   <m:mo stretchy="false">(</m:mo>
 *   <m:mi>x</m:mi>
 *   <m:mo stretchy="false">)</m:mo>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 *
 * \return
 * Returns the value of the regular modified cylindrical Bessel function of order 0.
 *
 * \note_accuracy_single
 */
extern __host__ __device__ __device_builtin__ float                  cyl_bessel_i0f(float x) __THROW;

/**
 * \ingroup CUDA_MATH_DOUBLE
 * \brief Calculate the value of the regular modified cylindrical Bessel function of order 1 for the input argument.
 *
 * Calculate the value of the regular modified cylindrical Bessel function of order 1 for
 * the input argument \p x, 
 * \latexonly $I_1(x)$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:msub>
 *     <m:mi>I</m:mi>
 *     <m:mn>1</m:mn>
 *   </m:msub>
 *   <m:mo stretchy="false">(</m:mo>
 *   <m:mi>x</m:mi>
 *   <m:mo stretchy="false">)</m:mo>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 *
 * \return
 * Returns the value of the regular modified cylindrical Bessel function of order 1.
 *
 * \note_accuracy_double
 */
extern __host__ __device__ __device_builtin__ _CRTIMP double __cdecl cyl_bessel_i1(double x) __THROW;
/**
 * \ingroup CUDA_MATH_SINGLE
 * \brief Calculate the value of the regular modified cylindrical Bessel function of order 1 for the input argument.
 *
 * Calculate the value of the regular modified cylindrical Bessel function of order 1 for
 * the input argument \p x, 
 * \latexonly $I_1(x)$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:msub>
 *     <m:mi>I</m:mi>
 *     <m:mn>1</m:mn>
 *   </m:msub>
 *   <m:mo stretchy="false">(</m:mo>
 *   <m:mi>x</m:mi>
 *   <m:mo stretchy="false">)</m:mo>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 *
 * \return
 * Returns the value of the regular modified cylindrical Bessel function of order 1.
 *
 * \note_accuracy_single
 */
extern __host__ __device__ __device_builtin__ float                  cyl_bessel_i1f(float x) __THROW;

/**
 * \ingroup CUDA_MATH_DOUBLE
 * \brief Calculate the error function of the input argument.
 *
 * Calculate the value of the error function for the input argument \p x,
 * \latexonly $\frac{2}{\sqrt \pi} \int_0^x e^{-t^2} dt$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mfrac>
 *     <m:mn>2</m:mn>
 *     <m:msqrt>
 *       <m:mi>&#x03C0;<!-- π --></m:mi>
 *     </m:msqrt>
 *   </m:mfrac>
 *   <m:msubsup>
 *     <m:mo>&#x222B;<!-- ∫ --></m:mo>
 *     <m:mn>0</m:mn>
 *     <m:mi>x</m:mi>
 *   </m:msubsup>
 *   <m:msup>
 *     <m:mi>e</m:mi>
 *     <m:mrow class="MJX-TeXAtom-ORD">
 *       <m:mo>&#x2212;<!-- − --></m:mo>
 *       <m:msup>
 *         <m:mi>t</m:mi>
 *         <m:mn>2</m:mn>
 *       </m:msup>
 *     </m:mrow>
 *   </m:msup>
 *   <m:mi>d</m:mi>
 *   <m:mi>t</m:mi>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 *
 * \return 
 * - erf(
 * \latexonly $\pm 0$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mn>0</m:mn>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns 
 * \latexonly $\pm 0$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mn>0</m:mn>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 * - erf(
 * \latexonly $\pm \infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns 
 * \latexonly $\pm 1$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mn>1</m:mn>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 *
 * \note_accuracy_double
 */
#if _MSC_VER < 1800
extern __host__ __device__ __device_builtin__ double                 erf(double x) __THROW;
#else /* _MSC_VER < 1800 */
extern __host__ __device__ __device_builtin__ _CRTIMP double __cdecl erf(double x);
#endif /* _MSC_VER < 1800 */
/**
 * \ingroup CUDA_MATH_SINGLE
 * \brief Calculate the error function of the input argument.
 *
 * Calculate the value of the error function for the input argument \p x,
 * \latexonly $\frac{2}{\sqrt \pi} \int_0^x e^{-t^2} dt$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mfrac>
 *     <m:mn>2</m:mn>
 *     <m:msqrt>
 *       <m:mi>&#x03C0;<!-- π --></m:mi>
 *     </m:msqrt>
 *   </m:mfrac>
 *   <m:msubsup>
 *     <m:mo>&#x222B;<!-- ∫ --></m:mo>
 *     <m:mn>0</m:mn>
 *     <m:mi>x</m:mi>
 *   </m:msubsup>
 *   <m:msup>
 *     <m:mi>e</m:mi>
 *     <m:mrow class="MJX-TeXAtom-ORD">
 *       <m:mo>&#x2212;<!-- − --></m:mo>
 *       <m:msup>
 *         <m:mi>t</m:mi>
 *         <m:mn>2</m:mn>
 *       </m:msup>
 *     </m:mrow>
 *   </m:msup>
 *   <m:mi>d</m:mi>
 *   <m:mi>t</m:mi>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 *
 * \return  
 * - erff(
 * \latexonly $\pm 0$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mn>0</m:mn>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns 
 * \latexonly $\pm 0$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mn>0</m:mn>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 * - erff(
 * \latexonly $\pm \infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns 
 * \latexonly $\pm 1$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mn>1</m:mn>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 *
 * \note_accuracy_single
 */
#if _MSC_VER < 1800
extern __host__ __device__ __device_builtin__ float                  erff(float x) __THROW;
#else /* _MSC_VER < 1800 */
extern __host__ __device__ __device_builtin__ _CRTIMP float  __cdecl erff(float x);
#endif /* _MSC_VER < 1800 */
/**
 * \ingroup CUDA_MATH_DOUBLE
 * \brief Calculate the inverse error function of the input argument.
 *
 * Calculate the inverse error function of the input argument \p y, for \p y in the interval [-1, 1].
 * The inverse error function finds the value \p x that satisfies the equation \p y = erf(\p x),
 * for 
 * \latexonly $-1 \le y \le 1$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x2212;<!-- − --></m:mo>
 *   <m:mn>1</m:mn>
 *   <m:mo>&#x2264;<!-- ≤ --></m:mo>
 *   <m:mi>y</m:mi>
 *   <m:mo>&#x2264;<!-- ≤ --></m:mo>
 *   <m:mn>1</m:mn>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * , and 
 * \latexonly $-\infty \le x \le \infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x2212;<!-- − --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 *   <m:mo>&#x2264;<!-- ≤ --></m:mo>
 *   <m:mi>x</m:mi>
 *   <m:mo>&#x2264;<!-- ≤ --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 *
 * \return 
 * - erfinv(1) returns 
 * \latexonly $+\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>+</m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 * - erfinv(-1) returns 
 * \latexonly $-\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x2212;<!-- − --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 *
 * \note_accuracy_double
 */
extern __host__ __device__ __device_builtin__ double                 erfinv(double y);
/**
 * \ingroup CUDA_MATH_SINGLE
 * \brief Calculate the inverse error function of the input argument.
 *
 * Calculate the inverse error function of the input argument \p y, for \p y in the interval [-1, 1].
 * The inverse error function finds the value \p x that satisfies the equation \p y = erf(\p x),
 * for 
 * \latexonly $-1 \le y \le 1$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x2212;<!-- − --></m:mo>
 *   <m:mn>1</m:mn>
 *   <m:mo>&#x2264;<!-- ≤ --></m:mo>
 *   <m:mi>y</m:mi>
 *   <m:mo>&#x2264;<!-- ≤ --></m:mo>
 *   <m:mn>1</m:mn>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * , and 
 * \latexonly $-\infty \le x \le \infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x2212;<!-- − --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 *   <m:mo>&#x2264;<!-- ≤ --></m:mo>
 *   <m:mi>x</m:mi>
 *   <m:mo>&#x2264;<!-- ≤ --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 *
 * \return 
 * - erfinvf(1) returns 
 * \latexonly $+\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>+</m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 * - erfinvf(-1) returns 
 * \latexonly $-\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x2212;<!-- − --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 *
 * \note_accuracy_single
 */
extern __host__ __device__ __device_builtin__ float                  erfinvf(float y);
/**
 * \ingroup CUDA_MATH_DOUBLE
 * \brief Calculate the complementary error function of the input argument.
 *
 * Calculate the complementary error function of the input argument \p x,
 * 1 - erf(\p x).
 *
 * \return 
 * - erfc(
 * \latexonly $-\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x2212;<!-- − --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns 2.
 * - erfc(
 * \latexonly $+\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>+</m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns +0.
 *
 * \note_accuracy_double
 */
#if _MSC_VER < 1800
extern __host__ __device__ __device_builtin__ double                 erfc(double x) __THROW;
#else /* _MSC_VER < 1800 */
extern __host__ __device__ __device_builtin__ _CRTIMP double __cdecl erfc(double x);
#endif /* _MSC_VER < 1800 */
/**
 * \ingroup CUDA_MATH_SINGLE
 * \brief Calculate the complementary error function of the input argument.
 *
 * Calculate the complementary error function of the input argument \p x,
 * 1 - erf(\p x).
 *
 * \return 
 * - erfcf(
 * \latexonly $-\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x2212;<!-- − --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns 2.
 * - erfcf(
 * \latexonly $+\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>+</m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns +0.
 *
 * \note_accuracy_single
 */
#if _MSC_VER < 1800
extern __host__ __device__ __device_builtin__ float                  erfcf(float x) __THROW;
#else /* _MSC_VER < 1800 */
extern __host__ __device__ __device_builtin__ _CRTIMP float  __cdecl erfcf(float x);
#endif /* _MSC_VER < 1800 */
/**
 * \ingroup CUDA_MATH_DOUBLE
 * \brief Calculate the natural logarithm of the absolute value of the gamma function of the input argument.
 *
 * Calculate the natural logarithm of the absolute value of the gamma function of the input argument \p x, namely the value of
 * \latexonly $\log_{e}\left|\int_{0}^{\infty} e^{-t}t^{x-1}dt\right|$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:msub>
 *     <m:mi>log</m:mi>
 *     <m:mrow class="MJX-TeXAtom-ORD">
 *       <m:mi>e</m:mi>
 *     </m:mrow>
 *   </m:msub>
 *   <m:mfenced open="|" close="|">
 *     <m:mrow>
 *       <m:msubsup>
 *         <m:mo>&#x222B;<!-- ∫ --></m:mo>
 *         <m:mrow class="MJX-TeXAtom-ORD">
 *           <m:mn>0</m:mn>
 *         </m:mrow>
 *         <m:mrow class="MJX-TeXAtom-ORD">
 *           <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 *         </m:mrow>
 *       </m:msubsup>
 *       <m:msup>
 *         <m:mi>e</m:mi>
 *         <m:mrow class="MJX-TeXAtom-ORD">
 *           <m:mo>&#x2212;<!-- − --></m:mo>
 *           <m:mi>t</m:mi>
 *         </m:mrow>
 *       </m:msup>
 *       <m:msup>
 *         <m:mi>t</m:mi>
 *         <m:mrow class="MJX-TeXAtom-ORD">
 *           <m:mi>x</m:mi>
 *           <m:mo>&#x2212;<!-- − --></m:mo>
 *           <m:mn>1</m:mn>
 *         </m:mrow>
 *       </m:msup>
 *       <m:mi>d</m:mi>
 *       <m:mi>t</m:mi>
 *     </m:mrow>
 *   </m:mfenced>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 *
 * \return 
 * - lgamma(1) returns +0.
 * - lgamma(2) returns +0.
 * - lgamma(\p x) returns 
 * \latexonly $\pm \infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 *  if the correctly calculated value is outside the double floating point range.
 * - lgamma(\p x) returns 
 * \latexonly $+\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>+</m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 *  if \p x 
 * \latexonly $\leq$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x2264;<!-- ≤ --></m:mo>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly 0.
 * - lgamma(
 * \latexonly $-\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x2212;<!-- − --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns 
 * \latexonly $-\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x2212;<!-- − --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 * - lgamma(
 * \latexonly $+\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>+</m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns 
 * \latexonly $+\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>+</m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 *
 * \note_accuracy_double
 */
#if _MSC_VER < 1800
extern __host__ __device__ __device_builtin__ double                 lgamma(double x) __THROW;
#else /* _MSC_VER < 1800 */
extern __host__ __device__ __device_builtin__ _CRTIMP double __cdecl lgamma(double x);
#endif /* _MSC_VER < 1800 */
/**
 * \ingroup CUDA_MATH_DOUBLE
 * \brief Calculate the inverse complementary error function of the input argument.
 *
 * Calculate the inverse complementary error function of the input argument \p y, for \p y in the interval [0, 2].
 * The inverse complementary error function find the value \p x that satisfies the equation \p y = erfc(\p x),
 * for 
 * \latexonly $0 \le y \le 2$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mn>0</m:mn>
 *   <m:mo>&#x2264;<!-- ≤ --></m:mo>
 *   <m:mi>y</m:mi>
 *   <m:mo>&#x2264;<!-- ≤ --></m:mo>
 *   <m:mn>2</m:mn>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * , and 
 * \latexonly $-\infty \le x \le \infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x2212;<!-- − --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 *   <m:mo>&#x2264;<!-- ≤ --></m:mo>
 *   <m:mi>x</m:mi>
 *   <m:mo>&#x2264;<!-- ≤ --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 *
 * \return 
 * - erfcinv(0) returns 
 * \latexonly $+\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>+</m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 * - erfcinv(2) returns 
 * \latexonly $-\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x2212;<!-- − --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 *
 * \note_accuracy_double
 */
extern __host__ __device__ __device_builtin__ double                 erfcinv(double y);
/**
 * \ingroup CUDA_MATH_SINGLE
 * \brief Calculate the inverse complementary error function of the input argument.
 *
 * Calculate the inverse complementary error function of the input argument \p y, for \p y in the interval [0, 2].
 * The inverse complementary error function find the value \p x that satisfies the equation \p y = erfc(\p x),
 * for 
 * \latexonly $0 \le y \le 2$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mn>0</m:mn>
 *   <m:mo>&#x2264;<!-- ≤ --></m:mo>
 *   <m:mi>y</m:mi>
 *   <m:mo>&#x2264;<!-- ≤ --></m:mo>
 *   <m:mn>2</m:mn>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * , and 
 * \latexonly $-\infty \le x \le \infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x2212;<!-- − --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 *   <m:mo>&#x2264;<!-- ≤ --></m:mo>
 *   <m:mi>x</m:mi>
 *   <m:mo>&#x2264;<!-- ≤ --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 *
 * \return 
 * - erfcinvf(0) returns 
 * \latexonly $+\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>+</m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 * - erfcinvf(2) returns 
 * \latexonly $-\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x2212;<!-- − --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 *
 * \note_accuracy_single
 */
extern __host__ __device__ __device_builtin__ float                  erfcinvf(float y);
/**
 * \ingroup CUDA_MATH_DOUBLE
 * \brief Calculate the inverse of the standard normal cumulative distribution function.
 *
 * Calculate the inverse of the standard normal cumulative distribution function for input argument \p y,
 * \latexonly $\Phi^{-1}(y)$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:msup>
 *     <m:mi mathvariant="normal">&#x03A6;<!-- Φ --></m:mi>
 *     <m:mrow class="MJX-TeXAtom-ORD">
 *       <m:mo>&#x2212;<!-- − --></m:mo>
 *       <m:mn>1</m:mn>
 *     </m:mrow>
 *   </m:msup>
 *   <m:mo stretchy="false">(</m:mo>
 *   <m:mi>y</m:mi>
 *   <m:mo stretchy="false">)</m:mo>
 * </m:math>
 * </d4p_MathML>\endxmlonly. The function is defined for input values in the interval 
 * \latexonly $(0, 1)$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo stretchy="false">(</m:mo>
 *   <m:mn>0</m:mn>
 *   <m:mo>,</m:mo>
 *   <m:mn>1</m:mn>
 *   <m:mo stretchy="false">)</m:mo>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 *
 * \return 
 * - normcdfinv(0) returns
 * \latexonly $-\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x2212;<!-- − --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 * - normcdfinv(1) returns 
 * \latexonly $+\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>+</m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 * - normcdfinv(\p x) returns NaN
 *  if \p x is not in the interval [0,1].
 *
 * \note_accuracy_double
 */
extern __host__ __device__ __device_builtin__ double                 normcdfinv(double y);
/**
 * \ingroup CUDA_MATH_SINGLE
 * \brief Calculate the inverse of the standard normal cumulative distribution function.
 *
 * Calculate the inverse of the standard normal cumulative distribution function for input argument \p y,
 * \latexonly $\Phi^{-1}(y)$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:msup>
 *     <m:mi mathvariant="normal">&#x03A6;<!-- Φ --></m:mi>
 *     <m:mrow class="MJX-TeXAtom-ORD">
 *       <m:mo>&#x2212;<!-- − --></m:mo>
 *       <m:mn>1</m:mn>
 *     </m:mrow>
 *   </m:msup>
 *   <m:mo stretchy="false">(</m:mo>
 *   <m:mi>y</m:mi>
 *   <m:mo stretchy="false">)</m:mo>
 * </m:math>
 * </d4p_MathML>\endxmlonly. The function is defined for input values in the interval 
 * \latexonly $(0, 1)$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo stretchy="false">(</m:mo>
 *   <m:mn>0</m:mn>
 *   <m:mo>,</m:mo>
 *   <m:mn>1</m:mn>
 *   <m:mo stretchy="false">)</m:mo>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 *
 * \return 
 * - normcdfinvf(0) returns
 * \latexonly $-\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x2212;<!-- − --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 * - normcdfinvf(1) returns 
 * \latexonly $+\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>+</m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 * - normcdfinvf(\p x) returns NaN
 *  if \p x is not in the interval [0,1].
 *
 * \note_accuracy_single
 */
extern __host__ __device__ __device_builtin__ float                  normcdfinvf(float y);
/**
 * \ingroup CUDA_MATH_DOUBLE
 * \brief Calculate the standard normal cumulative distribution function.
 *
 * Calculate the cumulative distribution function of the standard normal distribution for input argument \p y,
 * \latexonly $\Phi(y)$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mi mathvariant="normal">&#x03A6;<!-- Φ --></m:mi>
 *   <m:mo stretchy="false">(</m:mo>
 *   <m:mi>y</m:mi>
 *   <m:mo stretchy="false">)</m:mo>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 *
 * \return 
 * - normcdf(
 * \latexonly $+\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>+</m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns 1
 * - normcdf(
 * \latexonly $-\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML"> 
 *   <m:mo>&#x2212;<!-- − --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns +0
 *
 * \note_accuracy_double
 */
extern __host__ __device__ __device_builtin__ double                 normcdf(double y);
/**
 * \ingroup CUDA_MATH_SINGLE
 * \brief Calculate the standard normal cumulative distribution function.
 *
 * Calculate the cumulative distribution function of the standard normal distribution for input argument \p y,
 * \latexonly $\Phi(y)$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mi mathvariant="normal">&#x03A6;<!-- Φ --></m:mi>
 *   <m:mo stretchy="false">(</m:mo>
 *   <m:mi>y</m:mi>
 *   <m:mo stretchy="false">)</m:mo>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 *
 * \return 
 * - normcdff(
 * \latexonly $+\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>+</m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns 1
 * - normcdff(
 * \latexonly $-\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML"> 
 *   <m:mo>&#x2212;<!-- − --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns +0

 * \note_accuracy_single
 */
extern __host__ __device__ __device_builtin__ float                  normcdff(float y);
/**
 * \ingroup CUDA_MATH_DOUBLE
 * \brief Calculate the scaled complementary error function of the input argument.
 *
 * Calculate the scaled complementary error function of the input argument \p x,
 * \latexonly $e^{x^2}\cdot \textrm{erfc}(x)$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:msup>
 *     <m:mi>e</m:mi>
 *     <m:mrow class="MJX-TeXAtom-ORD">
 *       <m:msup>
 *         <m:mi>x</m:mi>
 *         <m:mn>2</m:mn>
 *       </m:msup>
 *     </m:mrow>
 *   </m:msup>
 *   <m:mo>&#x22C5;<!-- ⋅ --></m:mo>
 *   <m:mrow class="MJX-TeXAtom-ORD">
 *     <m:mtext>erfc</m:mtext>
 *   </m:mrow>
 *   <m:mo stretchy="false">(</m:mo>
 *   <m:mi>x</m:mi>
 *   <m:mo stretchy="false">)</m:mo>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 *
 * \return 
 * - erfcx(
 * \latexonly $-\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>-</m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns 
 * \latexonly $+\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>+</m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * - erfcx(
 * \latexonly $+\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML"> 
 *   <m:mo>+</m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns +0
 * - erfcx(\p x) returns 
 * \latexonly $+\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>+</m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 *  if the correctly calculated value is outside the double floating point range.
 *
 * \note_accuracy_double
 */
extern __host__ __device__ __device_builtin__ double                 erfcx(double x);
/**
 * \ingroup CUDA_MATH_SINGLE
 * \brief Calculate the scaled complementary error function of the input argument.
 *
 * Calculate the scaled complementary error function of the input argument \p x,
 * \latexonly $e^{x^2}\cdot \textrm{erfc}(x)$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:msup>
 *     <m:mi>e</m:mi>
 *     <m:mrow class="MJX-TeXAtom-ORD">
 *       <m:msup>
 *         <m:mi>x</m:mi>
 *         <m:mn>2</m:mn>
 *       </m:msup>
 *     </m:mrow>
 *   </m:msup>
 *   <m:mo>&#x22C5;<!-- ⋅ --></m:mo>
 *   <m:mrow class="MJX-TeXAtom-ORD">
 *     <m:mtext>erfc</m:mtext>
 *   </m:mrow>
 *   <m:mo stretchy="false">(</m:mo>
 *   <m:mi>x</m:mi>
 *   <m:mo stretchy="false">)</m:mo>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 *
 * \return 
 * - erfcxf(
 * \latexonly $-\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>-</m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns 
 * \latexonly $+\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>+</m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * - erfcxf(
 * \latexonly $+\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML"> 
 *   <m:mo>+</m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns +0
 * - erfcxf(\p x) returns 
 * \latexonly $+\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>+</m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 *  if the correctly calculated value is outside the single floating point range.

 * \note_accuracy_single
 */
extern __host__ __device__ __device_builtin__ float                  erfcxf(float x);
/**
 * \ingroup CUDA_MATH_SINGLE
 * \brief Calculate the natural logarithm of the absolute value of the gamma function of the input argument.
 *
 * Calculate the natural logarithm of the absolute value of the gamma function of the input argument \p x, namely the value of
 * \latexonly $log_{e}|\ \int_{0}^{\infty} e^{-t}t^{x-1}dt|$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mi>l</m:mi>
 *   <m:mi>o</m:mi>
 *   <m:msub>
 *     <m:mi>g</m:mi>
 *     <m:mrow class="MJX-TeXAtom-ORD">
 *       <m:mi>e</m:mi>
 *     </m:mrow>
 *   </m:msub>
 *   <m:mrow class="MJX-TeXAtom-ORD">
 *     <m:mo stretchy="false">|</m:mo>
 *   </m:mrow>
 *   <m:mtext>&#xA0;</m:mtext>
 *   <m:msubsup>
 *     <m:mo>&#x222B;<!-- ∫ --></m:mo>
 *     <m:mrow class="MJX-TeXAtom-ORD">
 *       <m:mn>0</m:mn>
 *     </m:mrow>
 *     <m:mrow class="MJX-TeXAtom-ORD">
 *       <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 *     </m:mrow>
 *   </m:msubsup>
 *   <m:msup>
 *     <m:mi>e</m:mi>
 *     <m:mrow class="MJX-TeXAtom-ORD">
 *       <m:mo>&#x2212;<!-- − --></m:mo>
 *       <m:mi>t</m:mi>
 *     </m:mrow>
 *   </m:msup>
 *   <m:msup>
 *     <m:mi>t</m:mi>
 *     <m:mrow class="MJX-TeXAtom-ORD">
 *       <m:mi>x</m:mi>
 *       <m:mo>&#x2212;<!-- − --></m:mo>
 *       <m:mn>1</m:mn>
 *     </m:mrow>
 *   </m:msup>
 *   <m:mi>d</m:mi>
 *   <m:mi>t</m:mi>
 *   <m:mrow class="MJX-TeXAtom-ORD">
 *     <m:mo stretchy="false">|</m:mo>
 *   </m:mrow>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 *
 * \return 
 * - lgammaf(1) returns +0.
 * - lgammaf(2) returns +0.
 * - lgammaf(\p x) returns 
 * \latexonly $\pm \infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 *  if the correctly calculated value is outside the single floating point range.
 * - lgammaf(\p x) returns 
 * \latexonly $+\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>+</m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 *  if \p x
 * \latexonly $\leq$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x2264;<!-- ≤ --></m:mo>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 *  0.
 * - lgammaf(
 * \latexonly $-\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x2212;<!-- − --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns 
 * \latexonly $-\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x2212;<!-- − --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 * - lgammaf(
 * \latexonly $+\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>+</m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns 
 * \latexonly $+\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>+</m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 *
 * \note_accuracy_single
 */
#if _MSC_VER < 1800
extern __host__ __device__ __device_builtin__ float                  lgammaf(float x) __THROW;
#else /* _MSC_VER < 1800 */
extern __host__ __device__ __device_builtin__ _CRTIMP float  __cdecl lgammaf(float x);
#endif /* _MSC_VER < 1800 */
/**
 * \ingroup CUDA_MATH_DOUBLE
 * \brief Calculate the gamma function of the input argument.
 *
 * Calculate the gamma function of the input argument \p x, namely the value of
 * \latexonly $\int_{0}^{\infty} e^{-t}t^{x-1}dt$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:msubsup>
 *     <m:mo>&#x222B;<!-- ∫ --></m:mo>
 *     <m:mrow class="MJX-TeXAtom-ORD">
 *       <m:mn>0</m:mn>
 *     </m:mrow>
 *     <m:mrow class="MJX-TeXAtom-ORD">
 *       <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 *     </m:mrow>
 *   </m:msubsup>
 *   <m:msup>
 *     <m:mi>e</m:mi>
 *     <m:mrow class="MJX-TeXAtom-ORD">
 *       <m:mo>&#x2212;<!-- − --></m:mo>
 *       <m:mi>t</m:mi>
 *     </m:mrow>
 *   </m:msup>
 *   <m:msup>
 *     <m:mi>t</m:mi>
 *     <m:mrow class="MJX-TeXAtom-ORD">
 *       <m:mi>x</m:mi>
 *       <m:mo>&#x2212;<!-- − --></m:mo>
 *       <m:mn>1</m:mn>
 *     </m:mrow>
 *   </m:msup>
 *   <m:mi>d</m:mi>
 *   <m:mi>t</m:mi>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 *
 * \return 
 * - tgamma(
 * \latexonly $\pm 0$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mn>0</m:mn>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns 
 * \latexonly $\pm \infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 * - tgamma(2) returns +0.
 * - tgamma(\p x) returns 
 * \latexonly $\pm \infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 *  if the correctly calculated value is outside the double floating point range.
 * - tgamma(\p x) returns NaN if \p x < 0.
 * - tgamma(
 * \latexonly $-\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x2212;<!-- − --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns NaN.
 * - tgamma(
 * \latexonly $+\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>+</m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns 
 * \latexonly $+\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>+</m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 *
 * \note_accuracy_double
 */
#if _MSC_VER < 1800
extern __host__ __device__ __device_builtin__ double                 tgamma(double x) __THROW;
#else /* _MSC_VER < 1800 */
extern __host__ __device__ __device_builtin__ _CRTIMP double __cdecl tgamma(double x);
#endif /* _MSC_VER < 1800 */
/**
 * \ingroup CUDA_MATH_SINGLE
 * \brief Calculate the gamma function of the input argument.
 *
 * Calculate the gamma function of the input argument \p x, namely the value of
 * \latexonly $\int_{0}^{\infty} e^{-t}t^{x-1}dt$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:msubsup>
 *     <m:mo>&#x222B;<!-- ∫ --></m:mo>
 *     <m:mrow class="MJX-TeXAtom-ORD">
 *       <m:mn>0</m:mn>
 *     </m:mrow>
 *     <m:mrow class="MJX-TeXAtom-ORD">
 *       <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 *     </m:mrow>
 *   </m:msubsup>
 *   <m:msup>
 *     <m:mi>e</m:mi>
 *     <m:mrow class="MJX-TeXAtom-ORD">
 *       <m:mo>&#x2212;<!-- − --></m:mo>
 *       <m:mi>t</m:mi>
 *     </m:mrow>
 *   </m:msup>
 *   <m:msup>
 *     <m:mi>t</m:mi>
 *     <m:mrow class="MJX-TeXAtom-ORD">
 *       <m:mi>x</m:mi>
 *       <m:mo>&#x2212;<!-- − --></m:mo>
 *       <m:mn>1</m:mn>
 *     </m:mrow>
 *   </m:msup>
 *   <m:mi>d</m:mi>
 *   <m:mi>t</m:mi>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 *
 * \return 
 * - tgammaf(
 * \latexonly $\pm 0$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mn>0</m:mn>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns 
 * \latexonly $\pm \infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 * - tgammaf(2) returns +0.
 * - tgammaf(\p x) returns 
 * \latexonly $\pm \infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 *  if the correctly calculated value is outside the single floating point range.
 * - tgammaf(\p x) returns NaN if \p x < 0.
 * - tgammaf(
 * \latexonly $-\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x2212;<!-- − --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns NaN.
 * - tgammaf(
 * \latexonly $+\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>+</m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns 
 * \latexonly $+\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>+</m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 *
 * \note_accuracy_single
 */
#if _MSC_VER < 1800
extern __host__ __device__ __device_builtin__ float                  tgammaf(float x) __THROW;
#else /* _MSC_VER < 1800 */
extern __host__ __device__ __device_builtin__ _CRTIMP float  __cdecl tgammaf(float x);
#endif /* _MSC_VER < 1800 */
/** \ingroup CUDA_MATH_DOUBLE
 * \brief Create value with given magnitude, copying sign of second value.
 *
 * Create a floating-point value with the magnitude \p x and the sign of \p y.
 *
 * \return
 * Returns a value with the magnitude of \p x and the sign of \p y.
 */
#if _MSC_VER < 1800
extern __host__ __device__ __device_builtin__ double                 copysign(double x, double y) __THROW;
#else /* _MSC_VER < 1800 */
extern __host__ __device__ __device_builtin__ _CRTIMP double __cdecl copysign(double x, double y);
#endif /* _MSC_VER < 1800 */
/** \ingroup CUDA_MATH_SINGLE
 * \brief Create value with given magnitude, copying sign of second value.
 *
 * Create a floating-point value with the magnitude \p x and the sign of \p y.
 *
 * \return
 * Returns a value with the magnitude of \p x and the sign of \p y.
 */
#if _MSC_VER < 1800
extern __host__ __device__ __device_builtin__ float                  copysignf(float x, float y) __THROW;
#else /* _MSC_VER < 1800 */
extern __host__ __device__ __device_builtin__ _CRTIMP float  __cdecl copysignf(float x, float y);
#endif /* _MSC_VER < 1800 */
// FIXME exceptional cases for nextafter
/**
 * \ingroup CUDA_MATH_DOUBLE
 * \brief Return next representable double-precision floating-point value after argument.
 *
 * Calculate the next representable double-precision floating-point value
 * following \p x in the direction of \p y. For example, if \p y is greater than \p x, ::nextafter()
 * returns the smallest representable number greater than \p x
 *
 * \return 
 * - nextafter(
 * \latexonly $\pm \infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * , \p y) returns 
 * \latexonly $\pm \infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 *
 * \note_accuracy_double
 */
#if _MSC_VER < 1800
extern __host__ __device__ __device_builtin__ double                 nextafter(double x, double y) __THROW;
#else /* _MSC_VER < 1800 */
extern __host__ __device__ __device_builtin__ _CRTIMP double __cdecl nextafter(double x, double y);
#endif /* _MSC_VER < 1800 */
// FIXME exceptional cases for nextafterf
/**
 * \ingroup CUDA_MATH_SINGLE
 * \brief Return next representable single-precision floating-point value afer argument.
 *
 * Calculate the next representable single-precision floating-point value
 * following \p x in the direction of \p y. For example, if \p y is greater than \p x, ::nextafterf()
 * returns the smallest representable number greater than \p x
 *
 * \return 
 * - nextafterf(
 * \latexonly $\pm \infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * , \p y) returns 
 * \latexonly $\pm \infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 *
 * \note_accuracy_single
 */
#if _MSC_VER < 1800
extern __host__ __device__ __device_builtin__ float                  nextafterf(float x, float y) __THROW;
#else /* _MSC_VER < 1800 */
extern __host__ __device__ __device_builtin__ _CRTIMP float  __cdecl nextafterf(float x, float y);
#endif /* _MSC_VER < 1800 */
/**
 * \ingroup CUDA_MATH_DOUBLE
 * \brief Returns "Not a Number" value.
 *
 * Return a representation of a quiet NaN. Argument \p tagp selects one of the possible representations.
 *
 * \return 
 * - nan(\p tagp) returns NaN.
 *
 * \note_accuracy_double
 */
#if _MSC_VER < 1800
extern __host__ __device__ __device_builtin__ double                 nan(const char *tagp) __THROW;
#else /* _MSC_VER < 1800 */
extern __host__ __device__ __device_builtin__ _CRTIMP double __cdecl nan(const char *tagp);
#endif /* _MSC_VER < 1800 */
/**
 * \ingroup CUDA_MATH_SINGLE
 * \brief Returns "Not a Number" value
 *
 * Return a representation of a quiet NaN. Argument \p tagp selects one of the possible representations.
 *
 * \return 
 * - nanf(\p tagp) returns NaN.
 *
 * \note_accuracy_single
 */
#if _MSC_VER < 1800
extern __host__ __device__ __device_builtin__ float                  nanf(const char *tagp) __THROW;
#else /* _MSC_VER < 1800 */
extern __host__ __device__ __device_builtin__ _CRTIMP float  __cdecl nanf(const char *tagp);
#endif /* _MSC_VER < 1800 */
extern __host__ __device__ __device_builtin__ int                    __isinff(float) __THROW;
extern __host__ __device__ __device_builtin__ int                    __isnanf(float) __THROW;

#if defined(__APPLE__)
extern __host__ __device__ __device_builtin__ int                    __isfinited(double) __THROW;
extern __host__ __device__ __device_builtin__ int                    __isfinitef(float) __THROW;
extern __host__ __device__ __device_builtin__ int                    __signbitd(double) __THROW;
extern __host__ __device__ __device_builtin__ int                    __isnand(double) __THROW;
extern __host__ __device__ __device_builtin__ int                    __isinfd(double) __THROW;
#else /* __APPLE__ */
extern __host__ __device__ __device_builtin__ int                    __finite(double) __THROW;
extern __host__ __device__ __device_builtin__ int                    __finitef(float) __THROW;
extern __host__ __device__ __device_builtin__ int                    __signbit(double) __THROW;
extern __host__ __device__ __device_builtin__ int                    __isnan(double) __THROW;
extern __host__ __device__ __device_builtin__ int                    __isinf(double) __THROW;
#endif /* __APPLE__ */

extern __host__ __device__ __device_builtin__ int                    __signbitf(float) __THROW;
/**
 * \ingroup CUDA_MATH_DOUBLE
 * \brief Compute 
 * \latexonly $x \times y + z$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mi>x</m:mi>
 *   <m:mo>&#x00D7;<!-- × --></m:mo>
 *   <m:mi>y</m:mi>
 *   <m:mo>+</m:mo>
 *   <m:mi>z</m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 *  as a single operation.
 *
 * Compute the value of 
 * \latexonly $x \times y + z$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mi>x</m:mi>
 *   <m:mo>&#x00D7;<!-- × --></m:mo>
 *   <m:mi>y</m:mi>
 *   <m:mo>+</m:mo>
 *   <m:mi>z</m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 *  as a single ternary operation. After computing the value
 * to infinite precision, the value is rounded once.
 *
 * \return
 * Returns the rounded value of 
 * \latexonly $x \times y + z$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mi>x</m:mi>
 *   <m:mo>&#x00D7;<!-- × --></m:mo>
 *   <m:mi>y</m:mi>
 *   <m:mo>+</m:mo>
 *   <m:mi>z</m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 *  as a single operation.
 * - fma(
 * \latexonly $\pm \infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * , 
 * \latexonly $\pm 0$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mn>0</m:mn>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * , \p z) returns NaN.
 * - fma(
 * \latexonly $\pm 0$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mn>0</m:mn>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * , 
 * \latexonly $\pm \infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * , \p z) returns NaN.
 * - fma(\p x, \p y, 
 * \latexonly $-\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x2212;<!-- − --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns NaN if 
 * \latexonly $x \times y$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mi>x</m:mi>
 *   <m:mo>&#x00D7;<!-- × --></m:mo>
 *   <m:mi>y</m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 *  is an exact 
 * \latexonly $+\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>+</m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 * - fma(\p x, \p y, 
 * \latexonly $+\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>+</m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns NaN if 
 * \latexonly $x \times y$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mi>x</m:mi>
 *   <m:mo>&#x00D7;<!-- × --></m:mo>
 *   <m:mi>y</m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 *  is an exact 
 * \latexonly $-\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x2212;<!-- − --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 *
 * \note_accuracy_double
 */
#if _MSC_VER < 1800
extern __host__ __device__ __device_builtin__ double                 fma(double x, double y, double z) __THROW;
#else /* _MSC_VER < 1800 */
extern __host__ __device__ __device_builtin__ _CRTIMP double __cdecl fma(double x, double y, double z);
#endif /* _MSC_VER < 1800 */
/**
 * \ingroup CUDA_MATH_SINGLE
 * \brief Compute 
 * \latexonly $x \times y + z$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mi>x</m:mi>
 *   <m:mo>&#x00D7;<!-- × --></m:mo>
 *   <m:mi>y</m:mi>
 *   <m:mo>+</m:mo>
 *   <m:mi>z</m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 *  as a single operation.
 *
 * Compute the value of 
 * \latexonly $x \times y + z$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mi>x</m:mi>
 *   <m:mo>&#x00D7;<!-- × --></m:mo>
 *   <m:mi>y</m:mi>
 *   <m:mo>+</m:mo>
 *   <m:mi>z</m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 *  as a single ternary operation. After computing the value
 * to infinite precision, the value is rounded once.
 *
 * \return
 * Returns the rounded value of 
 * \latexonly $x \times y + z$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mi>x</m:mi>
 *   <m:mo>&#x00D7;<!-- × --></m:mo>
 *   <m:mi>y</m:mi>
 *   <m:mo>+</m:mo>
 *   <m:mi>z</m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 *  as a single operation.
 * - fmaf(
 * \latexonly $\pm \infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * , 
 * \latexonly $\pm 0$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mn>0</m:mn>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * , \p z) returns NaN.
 * - fmaf(
 * \latexonly $\pm 0$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mn>0</m:mn>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * , 
 * \latexonly $\pm \infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * , \p z) returns NaN.
 * - fmaf(\p x, \p y, 
 * \latexonly $-\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x2212;<!-- − --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns NaN if 
 * \latexonly $x \times y$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mi>x</m:mi>
 *   <m:mo>&#x00D7;<!-- × --></m:mo>
 *   <m:mi>y</m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 *  is an exact 
 * \latexonly $+\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>+</m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 * - fmaf(\p x, \p y, 
 * \latexonly $+\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>+</m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns NaN if 
 * \latexonly $x \times y$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mi>x</m:mi>
 *   <m:mo>&#x00D7;<!-- × --></m:mo>
 *   <m:mi>y</m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 *  is an exact 
 * \latexonly $-\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x2212;<!-- − --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 *
 * \note_accuracy_single
 */
#if _MSC_VER < 1800
extern __host__ __device__ __device_builtin__ float                  fmaf(float x, float y, float z) __THROW;
#else /* _MSC_VER < 1800 */
extern __host__ __device__ __device_builtin__ _CRTIMP float  __cdecl fmaf(float x, float y, float z);
#endif /* _MSC_VER < 1800 */


/* these are here to avoid warnings on the call graph.
   long double is not supported on the device */
extern __host__ __device__ __device_builtin__ int                    __signbitl(long double) __THROW;
#if defined(__APPLE__)
extern __host__ __device__ __device_builtin__ int                    __isfinite(long double) __THROW;
extern __host__ __device__ __device_builtin__ int                    __isinf(long double) __THROW;
extern __host__ __device__ __device_builtin__ int                    __isnan(long double) __THROW;
#else /* __APPLE__ */
extern __host__ __device__ __device_builtin__ int                    __finitel(long double) __THROW;
extern __host__ __device__ __device_builtin__ int                    __isinfl(long double) __THROW;
extern __host__ __device__ __device_builtin__ int                    __isnanl(long double) __THROW;
#endif /* __APPLE__ */

#if defined(_WIN32) && defined(_M_AMD64)
extern _CRTIMP __host__ __device__ __device_builtin__ float __cdecl acosf(float) __THROW;
extern _CRTIMP __host__ __device__ __device_builtin__ float __cdecl asinf(float) __THROW;
extern _CRTIMP __host__ __device__ __device_builtin__ float __cdecl atanf(float) __THROW;
extern _CRTIMP __host__ __device__ __device_builtin__ float __cdecl atan2f(float, float) __THROW;
extern _CRTIMP __host__ __device__ __device_builtin__ float __cdecl cosf(float) __THROW;
extern _CRTIMP __host__ __device__ __device_builtin__ float __cdecl sinf(float) __THROW;
extern _CRTIMP __host__ __device__ __device_builtin__ float __cdecl tanf(float) __THROW;
extern _CRTIMP __host__ __device__ __device_builtin__ float __cdecl coshf(float) __THROW;
extern _CRTIMP __host__ __device__ __device_builtin__ float __cdecl sinhf(float) __THROW;
extern _CRTIMP __host__ __device__ __device_builtin__ float __cdecl tanhf(float) __THROW;
extern _CRTIMP __host__ __device__ __device_builtin__ float __cdecl expf(float) __THROW;
extern _CRTIMP __host__ __device__ __device_builtin__ float __cdecl logf(float) __THROW;
extern _CRTIMP __host__ __device__ __device_builtin__ float __cdecl log10f(float) __THROW;
extern _CRTIMP __host__ __device__ __device_builtin__ float __cdecl modff(float, float*) __THROW;
extern _CRTIMP __host__ __device__ __device_builtin__ float __cdecl powf(float, float) __THROW;
extern _CRTIMP __host__ __device__ __device_builtin__ float __cdecl sqrtf(float) __THROW;         
extern _CRTIMP __host__ __device__ __device_builtin__ float __cdecl ceilf(float) __THROW;
extern _CRTIMP __host__ __device__ __device_builtin__ float __cdecl floorf(float) __THROW;
extern _CRTIMP __host__ __device__ __device_builtin__ float __cdecl fmodf(float, float) __THROW;
#else /* _WIN32 && _M_AMD64 */

/**
 * \ingroup CUDA_MATH_SINGLE
 * \brief Calculate the arc cosine of the input argument.
 *
 * Calculate the principal value of the arc cosine of the input argument \p x.
 *
 * \return 
 * Result will be in radians, in the interval [0, 
 * \latexonly $\pi$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mi>&#x03C0;<!-- π --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ] for \p x inside [-1, +1].
 * - acosf(1) returns +0.
 * - acosf(\p x) returns NaN for \p x outside [-1, +1].
 *
 * \note_accuracy_single
 */
extern __host__ __device__ __device_builtin__ float                  acosf(float x) __THROW;
/**
 * \ingroup CUDA_MATH_SINGLE
 * \brief Calculate the arc sine of the input argument.
 *
 * Calculate the principal value of the arc sine of the input argument \p x.
 *
 * \return 
 * Result will be in radians, in the interval [-
 * \latexonly $\pi/2$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mi>&#x03C0;<!-- π --></m:mi>
 *   <m:mrow class="MJX-TeXAtom-ORD">
 *     <m:mo>/</m:mo>
 *   </m:mrow>
 *   <m:mn>2</m:mn>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * , +
 * \latexonly $\pi/2$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mi>&#x03C0;<!-- π --></m:mi>
 *   <m:mrow class="MJX-TeXAtom-ORD">
 *     <m:mo>/</m:mo>
 *   </m:mrow>
 *   <m:mn>2</m:mn>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ] for \p x inside [-1, +1].
 * - asinf(0) returns +0.
 * - asinf(\p x) returns NaN for \p x outside [-1, +1].
 *
 * \note_accuracy_single
 */
extern __host__ __device__ __device_builtin__ float                  asinf(float x) __THROW;

/**
 * \ingroup CUDA_MATH_SINGLE
 * \brief Calculate the arc tangent of the input argument.
 *
 * Calculate the principal value of the arc tangent of the input argument \p x.
 *
 * \return 
 * Result will be in radians, in the interval [-
 * \latexonly $\pi/2$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mi>&#x03C0;<!-- π --></m:mi>
 *   <m:mrow class="MJX-TeXAtom-ORD">
 *     <m:mo>/</m:mo>
 *   </m:mrow>
 *   <m:mn>2</m:mn>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * , +
 * \latexonly $\pi/2$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mi>&#x03C0;<!-- π --></m:mi>
 *   <m:mrow class="MJX-TeXAtom-ORD">
 *     <m:mo>/</m:mo>
 *   </m:mrow>
 *   <m:mn>2</m:mn>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ].
 * - atanf(0) returns +0.
 *
 * \note_accuracy_single
 */
extern __host__ __device__ __device_builtin__ float                  atanf(float x) __THROW;
/**
 * \ingroup CUDA_MATH_SINGLE
 * \brief Calculate the arc tangent of the ratio of first and second input arguments.
 *
 * Calculate the principal value of the arc tangent of the ratio of first
 * and second input arguments \p y / \p x. The quadrant of the result is 
 * determined by the signs of inputs \p y and \p x.
 *
 * \return 
 * Result will be in radians, in the interval [-
 * \latexonly $\pi$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mi>&#x03C0;<!-- π --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * , +
 * \latexonly $\pi$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mi>&#x03C0;<!-- π --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ].
 * - atan2f(0, 1) returns +0.
 *
 * \note_accuracy_single
 */
extern __host__ __device__ __device_builtin__ float                  atan2f(float y, float x) __THROW;
/**
 * \ingroup CUDA_MATH_SINGLE
 * \brief Calculate the cosine of the input argument.
 *
 * Calculate the cosine of the input argument \p x (measured in radians).
 *
 * \return 
 * - cosf(0) returns 1.
 * - cosf(
 * \latexonly $\pm \infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns NaN.
 *
 * \note_accuracy_single
 * \note_fastmath
 */
extern __host__ __device__ __device_builtin__ float                  cosf(float x) __THROW;
/**
 * \ingroup CUDA_MATH_SINGLE
 * \brief Calculate the sine of the input argument.
 *
 * Calculate the sine of the input argument \p x (measured in radians).
 *
 * \return 
 * - sinf(
 * \latexonly $\pm 0$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mn>0</m:mn>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns 
 * \latexonly $\pm 0$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mn>0</m:mn>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 * - sinf(
 * \latexonly $\pm \infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns NaN.
 *
 * \note_accuracy_single
 * \note_fastmath
 */
extern __host__ __device__ __device_builtin__ float                  sinf(float x) __THROW;
/**
 * \ingroup CUDA_MATH_SINGLE
 * \brief Calculate the tangent of the input argument.
 *
 * Calculate the tangent of the input argument \p x (measured in radians).
 *
 * \return 
 * - tanf(
 * \latexonly $\pm 0$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mn>0</m:mn>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns 
 * \latexonly $\pm 0$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mn>0</m:mn>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 * - tanf(
 * \latexonly $\pm \infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns NaN.
 *
 * \note_accuracy_single
 * \note_fastmath
 */
extern __host__ __device__ __device_builtin__ float                  tanf(float x) __THROW;
// FIXME return values for large arg values
/**
 * \ingroup CUDA_MATH_SINGLE
 * \brief Calculate the hyperbolic cosine of the input argument.
 *
 * Calculate the hyperbolic cosine of the input argument \p x.
 *
 * \return 
 * - coshf(0) returns 1.
 * - coshf(
 * \latexonly $\pm \infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns NaN.
 *
 * \note_accuracy_single
 */
extern __host__ __device__ __device_builtin__ float                  coshf(float x) __THROW;
/**
 * \ingroup CUDA_MATH_SINGLE
 * \brief Calculate the hyperbolic sine of the input argument.
 *
 * Calculate the hyperbolic sine of the input argument \p x.
 *
 * \return 
 * - sinhf(
 * \latexonly $\pm 0$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mn>0</m:mn>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns 
 * \latexonly $\pm 0$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mn>0</m:mn>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 * - sinhf(
 * \latexonly $\pm \infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns NaN.
 *
 * \note_accuracy_single
 */
extern __host__ __device__ __device_builtin__ float                  sinhf(float x) __THROW;
/**
 * \ingroup CUDA_MATH_SINGLE
 * \brief Calculate the hyperbolic tangent of the input argument.
 *
 * Calculate the hyperbolic tangent of the input argument \p x.
 *
 * \return 
 * - tanhf(
 * \latexonly $\pm 0$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mn>0</m:mn>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns 
 * \latexonly $\pm 0$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mn>0</m:mn>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 *
 * \note_accuracy_single
 */
extern __host__ __device__ __device_builtin__ float                  tanhf(float x) __THROW;
/**
 * \ingroup CUDA_MATH_SINGLE
 * \brief Calculate the natural logarithm of the input argument.
 *
 * Calculate the natural logarithm of the input argument \p x.
 *
 * \return 
 * - logf(
 * \latexonly $\pm 0$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mn>0</m:mn>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns 
 * \latexonly $-\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x2212;<!-- − --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 * - logf(1) returns +0.
 * - logf(\p x) returns NaN for \p x < 0.
 * - logf(
 * \latexonly $+\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>+</m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns 
 * \latexonly $+\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>+</m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 *
 * \note_accuracy_single
 */
extern __host__ __device__ __device_builtin__ float                  logf(float x) __THROW;
/**
 * \ingroup CUDA_MATH_SINGLE
 * \brief Calculate the base 
 * \latexonly $e$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mi>e</m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 *  exponential of the input argument.
 *
 * Calculate the base 
 * \latexonly $e$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mi>e</m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 *  exponential of the input argument \p x, 
 * \latexonly $e^x$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:msup>
 *     <m:mi>e</m:mi>
 *     <m:mi>x</m:mi>
 *   </m:msup>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 *
 * \return Returns 
 * \latexonly $e^x$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:msup>
 *     <m:mi>e</m:mi>
 *     <m:mi>x</m:mi>
 *   </m:msup>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 *
 * \note_accuracy_single
 * \note_fastmath
 */
extern __host__ __device__ __device_builtin__ float                  expf(float x) __THROW;
/**
 * \ingroup CUDA_MATH_SINGLE
 * \brief Calculate the base 10 logarithm of the input argument.
 *
 * Calculate the base 10 logarithm of the input argument \p x.
 *
 * \return 
 * - log10f(
 * \latexonly $\pm 0$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mn>0</m:mn>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns 
 * \latexonly $-\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x2212;<!-- − --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 * - log10f(1) returns +0.
 * - log10f(\p x) returns NaN for \p x < 0.
 * - log10f(
 * \latexonly $+\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>+</m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns 
 * \latexonly $+\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>+</m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 *
 * \note_accuracy_single
 */
extern __host__ __device__ __device_builtin__ float                  log10f(float x) __THROW;
/**
 * \ingroup CUDA_MATH_SINGLE
 * \brief Break down the input argument into fractional and integral parts.
 *
 * Break down the argument \p x into fractional and integral parts. The integral part is stored in the argument \p iptr.
 * Fractional and integral parts are given the same sign as the argument \p x.
 *
 * \return 
 * - modff(
 * \latexonly $\pm x$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *  <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mi>x</m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * , \p iptr) returns a result with the same sign as \p x.
 * - modff(
 * \latexonly $\pm \infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * , \p iptr) returns 
 * \latexonly $\pm 0$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mn>0</m:mn>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 *  and stores 
 * \latexonly $\pm \infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 *   in the object pointed to by \p iptr.
 * - modff(NaN, \p iptr) stores a NaN in the object pointed to by \p iptr and returns a NaN.
 *
 * \note_accuracy_single
 */
extern __host__ __device__ __device_builtin__ float                  modff(float x, float *iptr) __THROW;
/**
 * \ingroup CUDA_MATH_SINGLE
 * \brief Calculate the value of first argument to the power of second argument.
 *
 * Calculate the value of \p x to the power of \p y.
 *
 * \return 
 * - powf(
 * \latexonly $\pm 0$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mn>0</m:mn>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * , \p y) returns 
 * \latexonly $\pm \infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 *  for \p y an integer less than 0.
 * - powf(
 * \latexonly $\pm 0$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mn>0</m:mn>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * , \p y) returns 
 * \latexonly $\pm 0$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mn>0</m:mn>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 *  for \p y an odd integer greater than 0.
 * - powf(
 * \latexonly $\pm 0$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mn>0</m:mn>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * , \p y) returns +0 for \p y > 0 and not and odd integer.
 * - powf(-1, 
 * \latexonly $\pm \infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns 1.
 * - powf(+1, \p y) returns 1 for any \p y, even a NaN.
 * - powf(\p x, 
 * \latexonly $\pm 0$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mn>0</m:mn>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns 1 for any \p x, even a NaN.
 * - powf(\p x, \p y) returns a NaN for finite \p x < 0 and finite non-integer \p y.
 * - powf(\p x, 
 * \latexonly $-\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x2212;<!-- − --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns 
 * \latexonly $+\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>+</m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 *  for 
 * \latexonly $| x | < 1$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mrow class="MJX-TeXAtom-ORD">
 *     <m:mo stretchy="false">|</m:mo>
 *   </m:mrow>
 *   <m:mi>x</m:mi>
 *   <m:mrow class="MJX-TeXAtom-ORD">
 *     <m:mo stretchy="false">|</m:mo>
 *   </m:mrow>
 *   <m:mo>&lt;</m:mo>
 *   <m:mn>1</m:mn>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 * - powf(\p x, 
 * \latexonly $-\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x2212;<!-- − --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns +0 for 
 * \latexonly $| x | > 1$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mrow class="MJX-TeXAtom-ORD">
 *     <m:mo stretchy="false">|</m:mo>
 *   </m:mrow>
 *   <m:mi>x</m:mi>
 *   <m:mrow class="MJX-TeXAtom-ORD">
 *     <m:mo stretchy="false">|</m:mo>
 *   </m:mrow>
 *   <m:mo>&gt;</m:mo>
 *   <m:mn>1</m:mn>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 * - powf(\p x, 
 * \latexonly $+\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>+</m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns +0 for 
 * \latexonly $| x | < 1$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mrow class="MJX-TeXAtom-ORD">
 *     <m:mo stretchy="false">|</m:mo>
 *   </m:mrow>
 *   <m:mi>x</m:mi>
 *   <m:mrow class="MJX-TeXAtom-ORD">
 *     <m:mo stretchy="false">|</m:mo>
 *   </m:mrow>
 *   <m:mo>&lt;</m:mo>
 *   <m:mn>1</m:mn>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 * - powf(\p x, 
 * \latexonly $+\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>+</m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns 
 * \latexonly $+\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>+</m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 *  for 
 * \latexonly $| x | > 1$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mrow class="MJX-TeXAtom-ORD">
 *     <m:mo stretchy="false">|</m:mo>
 *   </m:mrow>
 *   <m:mi>x</m:mi>
 *   <m:mrow class="MJX-TeXAtom-ORD">
 *     <m:mo stretchy="false">|</m:mo>
 *   </m:mrow>
 *   <m:mo>&gt;</m:mo>
 *   <m:mn>1</m:mn>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 * - powf(
 * \latexonly $-\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x2212;<!-- − --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * , \p y) returns -0 for \p y an odd integer less than 0.
 * - powf(
 * \latexonly $-\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x2212;<!-- − --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * , \p y) returns +0 for \p y < 0 and not an odd integer.
 * - powf(
 * \latexonly $-\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x2212;<!-- − --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * , \p y) returns 
 * \latexonly $-\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x2212;<!-- − --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 *  for \p y an odd integer greater than 0.
 * - powf(
 * \latexonly $-\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x2212;<!-- − --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * , \p y) returns 
 * \latexonly $+\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>+</m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 *  for \p y > 0 and not an odd integer.
 * - powf(
 * \latexonly $+\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>+</m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * , \p y) returns +0 for \p y < 0.
 * - powf(
 * \latexonly $+\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>+</m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * , \p y) returns 
 * \latexonly $+\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>+</m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 *  for \p y > 0.
 *
 * \note_accuracy_single
 */
extern __host__ __device__ __device_builtin__ float                  powf(float x, float y) __THROW;
/**
 * \ingroup CUDA_MATH_SINGLE
 * \brief Calculate the square root of the input argument.
 *
 * Calculate the nonnegative square root of \p x, 
 * \latexonly $\sqrt{x}$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:msqrt>
 *     <m:mi>x</m:mi>
 *   </m:msqrt>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 *
 * \return 
 * Returns 
 * \latexonly $\sqrt{x}$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:msqrt>
 *     <m:mi>x</m:mi>
 *   </m:msqrt>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 * - sqrtf(
 * \latexonly $\pm 0$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mn>0</m:mn>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns 
 * \latexonly $\pm 0$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mn>0</m:mn>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 * - sqrtf(
 * \latexonly $+\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>+</m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns 
 * \latexonly $+\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>+</m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 * - sqrtf(\p x) returns NaN if \p x is less than 0.
 *
 * \note_accuracy_single
 */
extern __host__ __device__ __device_builtin__ float                  sqrtf(float x) __THROW;         
/**
 * \ingroup CUDA_MATH_SINGLE
 * \brief Calculate ceiling of the input argument.
 *
 * Compute the smallest integer value not less than \p x.
 *
 * \return
 * Returns 
 * \latexonly $\lceil x \rceil$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo fence="false" stretchy="false">&#x2308;<!-- ⌈ --></m:mo>
 *   <m:mi>x</m:mi>
 *   <m:mo fence="false" stretchy="false">&#x2309;<!-- ⌉ --></m:mo>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 *  expressed as a floating-point number.
 * - ceilf(
 * \latexonly $\pm 0$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mn>0</m:mn>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns 
 * \latexonly $\pm 0$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mn>0</m:mn>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 * - ceilf(
 * \latexonly $\pm \infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns 
 * \latexonly $\pm \infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 */
extern __host__ __device__ __device_builtin__ float                  ceilf(float x) __THROW;
/**
 * \ingroup CUDA_MATH_SINGLE
 * \brief Calculate the largest integer less than or equal to \p x.
 * 
 * Calculate the largest integer value which is less than or equal to \p x.
 * 
 * \return
 * Returns 
 * \latexonly $log_{e}(1+x)$ \endlatexonly
 * \latexonly $\lfloor x \rfloor$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mi>l</m:mi>
 *   <m:mi>o</m:mi>
 *   <m:msub>
 *     <m:mi>g</m:mi>
 *     <m:mrow class="MJX-TeXAtom-ORD">
 *       <m:mi>e</m:mi>
 *     </m:mrow>
 *   </m:msub>
 *   <m:mo stretchy="false">(</m:mo>
 *   <m:mn>1</m:mn>
 *   <m:mo>+</m:mo>
 *   <m:mi>x</m:mi>
 *   <m:mo stretchy="false">)</m:mo>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 *  expressed as a floating-point number.
 * - floorf(
 * \latexonly $\pm \infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns 
 * \latexonly $\pm \infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 * - floorf(
 * \latexonly $\pm 0$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mn>0</m:mn>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns 
 * \latexonly $\pm 0$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mn>0</m:mn>
 * </m:math>
 * </d4p_MathML>\endxmlonly.
 *
 * \note_accuracy_single
 */
extern __host__ __device__ __device_builtin__ float                  floorf(float x) __THROW;
/**
 * \ingroup CUDA_MATH_SINGLE
 * \brief Calculate the floating-point remainder of \p x / \p y.
 *
 * Calculate the floating-point remainder of \p x / \p y. 
 * The absolute value of the computed value is always less than \p y's
 * absolute value and will have the same sign as \p x.
 *
 * \return
 * - Returns the floating point remainder of \p x / \p y.
 * - fmodf(
 * \latexonly $\pm 0$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mn>0</m:mn>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * , \p y) returns 
 * \latexonly $\pm 0$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mn>0</m:mn>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 *  if \p y is not zero.
 * - fmodf(\p x, \p y) returns NaN and raised an invalid floating point exception if \p x is 
 * \latexonly $\pm\infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 *  or \p y is zero.
 * - fmodf(\p x, \p y) returns zero if \p y is zero or the result would overflow.
 * - fmodf(\p x, 
 * \latexonly $\pm \infty$ \endlatexonly
 * \xmlonly
 * <d4p_MathML outputclass="xmlonly">
 * <m:math xmlns:m="http://www.w3.org/1998/Math/MathML">
 *   <m:mo>&#x00B1;<!-- ± --></m:mo>
 *   <m:mi mathvariant="normal">&#x221E;<!-- ∞ --></m:mi>
 * </m:math>
 * </d4p_MathML>
 * \endxmlonly
 * ) returns \p x if \p x is finite.
 * - fmodf(\p x, 0) returns NaN.
 *
 * \note_accuracy_single
 */
extern __host__ __device__ __device_builtin__ float                  fmodf(float x, float y) __THROW;
#endif /* _WIN32 && _M_AMD64 */

}

#include <math.h>
#include <stdlib.h>

#ifndef __CUDA_INTERNAL_SKIP_CPP_HEADERS__
#include <cmath>
#include <cstdlib>
#endif

/*******************************************************************************
*                                                                              *
*                                                                              *
*                                                                              *
*******************************************************************************/

#if defined(__GNUC__)

#undef signbit
#undef isfinite
#undef isnan
#undef isinf

#if defined(__APPLE__)
__forceinline__ __host__ __device__ __cudart_builtin__ int signbit(float x) { return __signbitf(x); }
__forceinline__ __host__ __device__ __cudart_builtin__ int signbit(double x) { return __signbitd(x); }
__forceinline__ __host__ __device__ __cudart_builtin__ int signbit(long double x) { return __signbitl(x);}

__forceinline__ __host__ __device__ __cudart_builtin__ int isfinite(float x) { return __isfinitef(x); } 
__forceinline__ __host__ __device__ __cudart_builtin__ int isfinite(double x) { return __isfinited(x); }
__forceinline__ __host__ __device__ __cudart_builtin__ int isfinite(long double x) { return __isfinite(x); }

__forceinline__ __host__ __device__ __cudart_builtin__ int isnan(float x) { return __isnanf(x); }
__forceinline__ __host__ __device__ __cudart_builtin__ int isnan(double x) throw()  { return __isnand(x); }
__forceinline__ __host__ __device__ __cudart_builtin__ int isnan(long double x) { return __isnan(x); }

__forceinline__ __host__ __device__ __cudart_builtin__ int isinf(float x) { return __isinff(x); }
__forceinline__ __host__ __device__ __cudart_builtin__ int isinf(double x) throw()  { return __isinfd(x); }
__forceinline__ __host__ __device__ __cudart_builtin__ int isinf(long double x) { return __isinf(x); }

#else /* __APPLE__ */

__forceinline__ __host__ __device__ __cudart_builtin__ int signbit(float x) { return __signbitf(x); }
#if defined(__ICC)
__forceinline__ __host__ __device__ __cudart_builtin__ int signbit(double x) throw() { return __signbit(x); }
#else /* !__ICC */
__forceinline__ __host__ __device__ __cudart_builtin__ int signbit(double x) { return __signbit(x); }
#endif /* __ICC */
__forceinline__ __host__ __device__ __cudart_builtin__ int signbit(long double x) { return __signbitl(x);}


__forceinline__ __host__ __device__ __cudart_builtin__ int isfinite(float x) { return __finitef(x); } 
#if defined(__ICC)
__forceinline__ __host__ __device__ __cudart_builtin__ int isfinite(double x) throw() { return __finite(x); }
#else /* !__ICC */
__forceinline__ __host__ __device__ __cudart_builtin__ int isfinite(double x) { return __finite(x); }
#endif /* __ICC */
__forceinline__ __host__ __device__ __cudart_builtin__ int isfinite(long double x) { return __finitel(x); }


__forceinline__ __host__ __device__ __cudart_builtin__ int isnan(float x) { return __isnanf(x); }
__forceinline__ __host__ __device__ __cudart_builtin__ int isnan(double x) throw()  { return __isnan(x); }
__forceinline__ __host__ __device__ __cudart_builtin__ int isnan(long double x) { return __isnanl(x); }

__forceinline__ __host__ __device__ __cudart_builtin__ int isinf(float x) { return __isinff(x); }
__forceinline__ __host__ __device__ __cudart_builtin__ int isinf(double x) throw()  { return __isinf(x); }
__forceinline__ __host__ __device__ __cudart_builtin__ int isinf(long double x) { return __isinfl(x); }

#endif /* __APPLE__ */

#if defined(__arm__) && !defined(_STLPORT_VERSION) && !_GLIBCXX_USE_C99
#if !defined(__ANDROID__) || __GNUC__ < 4 || (__GNUC__ == 4 && __GNUC_MINOR__ < 8)

static __inline__ __host__ __device__ __cudart_builtin__ long long int abs(long long int a)
{
  return llabs(a);
}

#endif /* !__ANDROID__ || __GNUC__ < 4 || (__GNUC__ == 4 && __GNUC_MINOR__ < 8) */
#endif /* __arm__ && !_STLPORT_VERSION && !_GLIBCXX_USE_C99 */

#if __GNUC__ < 4 || (__GNUC__ == 4 && __GNUC_MINOR__ < 8)

#if !defined(_STLPORT_VERSION)
namespace __gnu_cxx
{
#endif /* !_STLPORT_VERSION */

extern __host__ __device__ __cudart_builtin__ long long int abs(long long int a);

#if !defined(_STLPORT_VERSION)
}
#endif /* !_STLPORT_VERSION */

#endif /* __GNUC__ < 4 || (__GNUC__ == 4 && __GNUC_MINOR__ < 8) */

namespace std
{
  template<typename T> extern __host__ __device__ __cudart_builtin__ T __pow_helper(T, int);
  template<typename T> extern __host__ __device__ __cudart_builtin__ T __cmath_power(T, unsigned int);
}

using std::abs;
using std::fabs;
using std::ceil;
using std::floor;
using std::sqrt;
using std::pow;
using std::log;
using std::log10;
using std::fmod;
using std::modf;
using std::exp;
using std::frexp;
using std::ldexp;
using std::asin;
using std::sin;
using std::sinh;
using std::acos;
using std::cos;
using std::cosh;
using std::atan;
using std::atan2;
using std::tan;
using std::tanh;

#elif defined(_WIN32)

#if _MSC_VER < 1600

static __inline__ __host__ __device__ long long int abs(long long int a)
{
  return llabs(a);
}

#else /* _MSC_VER < 1600 */

extern __host__ __device__ __cudart_builtin__ _CRTIMP double __cdecl _hypot(double x, double y);
extern __host__ __device__ __cudart_builtin__ _CRTIMP float  __cdecl _hypotf(float x, float y);

#endif /* _MSC_VER < 1600 */

#if _MSC_VER < 1800
static __inline__ __host__ __device__ int signbit(long double a)
{
  return __signbitl(a);
}
#else /* _MSC_VER < 1800 */
extern __host__ __device__ __cudart_builtin__ bool signbit(long double);
extern __host__ __device__ __cudart_builtin__ __device_builtin__ _CRTIMP int _ldsign(long double);
#endif /* _MSC_VER < 1800 */

#if _MSC_VER < 1800
#undef __RETURN_TYPE 
#define __RETURN_TYPE int
/**
 * \ingroup CUDA_MATH_DOUBLE
 * 
 * \brief Return the sign bit of the input.
 *
 * Determine whether the floating-point value \p a is negative.
 *
 * \return
 * Reports the sign bit of all values including infinities, zeros, and NaNs.
 * - With Visual Studio 2013 host compiler: __RETURN_TYPE is 'bool'. Returns 
 * true if and only if \p a is negative.
 * - With other host compilers: __RETURN_TYPE is 'int'. Returns a 
 * nonzero value if and only if \p a is negative. 
 */
static __inline__ __host__ __device__ __RETURN_TYPE signbit(double a)
{
  return __signbit(a);
}
#else /* _MSC_VER < 1800 */
#undef __RETURN_TYPE 
#define __RETURN_TYPE bool
/**
 * \ingroup CUDA_MATH_DOUBLE
 * 
 * \brief Return the sign bit of the input.
 *
 * Determine whether the floating-point value \p a is negative.
 *
 * \return
 * Reports the sign bit of all values including infinities, zeros, and NaNs.
 * - With Visual Studio 2013 host compiler: __RETURN_TYPE is 'bool'. Returns 
 * true if and only if \p a is negative.
 * - With other host compilers: __RETURN_TYPE is 'int'. Returns a 
 * nonzero value if and only if \p a is negative. 
 */
extern __host__ __device__ __cudart_builtin__ __RETURN_TYPE signbit(double);
extern __host__ __device__ __cudart_builtin__ __device_builtin__ _CRTIMP int _dsign(double);
#endif /* _MSC_VER < 1800 */

#if _MSC_VER < 1800
#undef __RETURN_TYPE
#define __RETURN_TYPE int
/**
 * \ingroup CUDA_MATH_SINGLE
 * 
 * \brief Return the sign bit of the input.
 *
 * Determine whether the floating-point value \p a is negative.
 *
 * \return
 * Reports the sign bit of all values including infinities, zeros, and NaNs.
 * - With Visual Studio 2013 host compiler: __RETURN_TYPE is 'bool'. Returns 
 * true if and only if \p a is negative.
 * - With other host compilers: __RETURN_TYPE is 'int'.  Returns a nonzero value 
 * if and only if \p a is negative.  
 */
static __inline__ __host__ __device__ __RETURN_TYPE signbit(float a)
{
  return __signbitf(a);
}
#else /* _MSC_VER < 1800 */
#undef __RETURN_TYPE
#define __RETURN_TYPE bool
/**
 * \ingroup CUDA_MATH_SINGLE
 * 
 * \brief Return the sign bit of the input.
 *
 * Determine whether the floating-point value \p a is negative.
 *
 * \return
 * Reports the sign bit of all values including infinities, zeros, and NaNs.
 * - With Visual Studio 2013 host compiler: __RETURN_TYPE is 'bool'. Returns 
 * true if and only if \p a is negative.
 * - With other host compilers: __RETURN_TYPE is 'int'.  Returns a nonzero value 
 * if and only if \p a is negative.  
 */
extern __host__ __device__ __cudart_builtin__ __RETURN_TYPE signbit(float);
extern __host__ __device__ __cudart_builtin__ __device_builtin__ _CRTIMP int _fdsign(float);
#endif /* _MSC_VER < 1800 */

#if _MSC_VER < 1800
static __inline__ __host__ __device__ int isinf(long double a)
{
  return __isinfl(a);
}
#else /* _MSC_VER < 1800 */
static __inline__ __host__ __device__ bool isinf(long double a)
{
#if defined(__CUDA_ARCH__)
  return (__isinfl(a) != 0);
#else /* defined(__CUDA_ARCH__) */
  return isinf<long double>(a);
#endif /* defined(__CUDA_ARCH__) */
}
#endif /* _MSC_VER < 1800 */

#if _MSC_VER < 1800
#undef __RETURN_TYPE
#define __RETURN_TYPE int
/**
 * \ingroup CUDA_MATH_DOUBLE
 * 
 * \brief Determine whether argument is infinite.
 *
 * Determine whether the floating-point value \p a is an infinite value
 * (positive or negative).
 * \return
 * - With Visual Studio 2013 host compiler: Returns true if and only 
 * if \p a is a infinite value.
 * - With other host compilers: Returns a nonzero value if and only 
 * if \p a is a infinite value.
 */
static __inline__ __host__ __device__ __RETURN_TYPE isinf(double a)
{
  return __isinf(a);
}
#else /* _MSC_VER < 1800 */
#undef __RETURN_TYPE
#define __RETURN_TYPE bool
/**
 * \ingroup CUDA_MATH_DOUBLE
 * 
 * \brief Determine whether argument is infinite.
 *
 * Determine whether the floating-point value \p a is an infinite value
 * (positive or negative).
 * \return
 * - With Visual Studio 2013 host compiler: Returns true if and only 
 * if \p a is a infinite value.
 * - With other host compilers: Returns a nonzero value if and only 
 * if \p a is a infinite value.
 */
static __inline__ __host__ __device__ __RETURN_TYPE isinf(double a)
{
#if defined(__CUDA_ARCH__)
  return (__isinf(a) != 0);
#else /* defined(__CUDA_ARCH__) */
  return isinf<double>(a);
#endif /* defined(__CUDA_ARCH__) */
}
#endif /* _MSC_VER < 1800 */

#if _MSC_VER < 1800
#undef __RETURN_TYPE
#define __RETURN_TYPE int
/**
 * \ingroup CUDA_MATH_SINGLE
 * 
 * \brief Determine whether argument is infinite.
 *
 * Determine whether the floating-point value \p a is an infinite value
 * (positive or negative).
 *
 * \return
 * - With Visual Studio 2013 host compiler: __RETURN_TYPE is 'bool'. Returns 
 * true if and only if \p a is a infinite value.
 * - With other host compilers: __RETURN_TYPE is 'int'. Returns a nonzero 
 * value if and only if \p a is a infinite value.
 */
static __inline__ __host__ __device__ __RETURN_TYPE isinf(float a)
{
  return __isinff(a);
}
#else /* _MSC_VER < 1800 */
#undef __RETURN_TYPE
#define __RETURN_TYPE bool
/**
 * \ingroup CUDA_MATH_SINGLE
 * 
 * \brief Determine whether argument is infinite.
 *
 * Determine whether the floating-point value \p a is an infinite value
 * (positive or negative).
 *
 * \return
 * - With Visual Studio 2013 host compiler: __RETURN_TYPE is 'bool'. Returns 
 * true if and only if \p a is a infinite value.
 * - With other host compilers: __RETURN_TYPE is 'int'. Returns a nonzero 
 * value if and only if \p a is a infinite value.
 */
static __inline__ __host__ __device__ __RETURN_TYPE isinf(float a)
{
#if defined(__CUDA_ARCH__)
  return (__isinff(a) != 0);
#else /* defined(__CUDA_ARCH__) */
  return isinf<float>(a);
#endif /* defined(__CUDA_ARCH__) */
}
#endif /* _MSC_VER < 1800 */

#if _MSC_VER < 1800
static __inline__ __host__ __device__ int isnan(long double a)
{
  return __isnanl(a);
}
#else /* _MSC_VER < 1800 */
static __inline__ __host__ __device__ bool isnan(long double a)
{
#if defined(__CUDA_ARCH__)
  return (__isnanl(a) != 0);
#else /* defined(__CUDA_ARCH__) */
  return isnan<long double>(a);
#endif /* defined(__CUDA_ARCH__) */
}
#endif /* _MSC_VER < 1800 */

#if _MSC_VER < 1800
#undef __RETURN_TYPE
#define __RETURN_TYPE int
/**
 * \ingroup CUDA_MATH_DOUBLE
 * 
 * \brief Determine whether argument is a NaN.
 *
 * Determine whether the floating-point value \p a is a NaN.
 * \return
 * - With Visual Studio 2013 host compiler: __RETURN_TYPE is 'bool'. 
 * Returns true if and only if \p a is a NaN value.
 * - With other host compilers: __RETURN_TYPE is 'int'. Returns a 
 * nonzero value if and only if \p a is a NaN value.
 */
static __inline__ __host__ __device__ __RETURN_TYPE isnan(double a)
{
  return __isnan(a);
}
#else /* _MSC_VER < 1800 */
#undef __RETURN_TYPE
#define __RETURN_TYPE bool
/**
 * \ingroup CUDA_MATH_DOUBLE
 * 
 * \brief Determine whether argument is a NaN.
 *
 * Determine whether the floating-point value \p a is a NaN.
 * \return
 * - With Visual Studio 2013 host compiler: __RETURN_TYPE is 'bool'. 
 * Returns true if and only if \p a is a NaN value.
 * - With other host compilers: __RETURN_TYPE is 'int'. Returns a 
 * nonzero value if and only if \p a is a NaN value.
 */
static __inline__ __host__ __device__ __RETURN_TYPE isnan(double a)
{
#if defined(__CUDA_ARCH__)
  return (__isnan(a) != 0);
#else /* defined(__CUDA_ARCH__) */
  return isnan<double>(a);
#endif /* defined(__CUDA_ARCH__) */
}
#endif /* _MSC_VER < 1800 */

#if _MSC_VER < 1800
#undef __RETURN_TYPE
#define __RETURN_TYPE int
/**
 * \ingroup CUDA_MATH_SINGLE
 * 
 * 
 * \brief Determine whether argument is a NaN.
 *
 * Determine whether the floating-point value \p a is a NaN.
 * \return
 * - With Visual Studio 2013 host compiler: __RETURN_TYPE is 'bool'. 
 * Returns true if and only if \p a is a NaN value.
 * - With other host compilers: __RETURN_TYPE is 'int'. Returns a 
 * nonzero value if and only if \p a is a NaN value.
 */
static __inline__ __host__ __device__ __RETURN_TYPE isnan(float a)
{
  return __isnanf(a);
}
#else /* _MSC_VER < 1800 */
/**
 * \ingroup CUDA_MATH_SINGLE
 * 
 * 
 * \brief Determine whether argument is a NaN.
 *
 * Determine whether the floating-point value \p a is a NaN.
 * \return
 * - With Visual Studio 2013 host compiler: __RETURN_TYPE is 'bool'. 
 * Returns true if and only if \p a is a NaN value.
 * - With other host compilers: __RETURN_TYPE is 'int'. Returns a 
 * nonzero value if and only if \p a is a NaN value.
 */
#undef __RETURN_TYPE
#define __RETURN_TYPE bool

static __inline__ __host__ __device__ __RETURN_TYPE isnan(float a)
{
#if defined(__CUDA_ARCH__)
  return (__isnanf(a) != 0);
#else /* defined(__CUDA_ARCH__) */
  return isnan<float>(a);
#endif /* defined(__CUDA_ARCH__) */
}
#endif /* _MSC_VER < 1800 */

#if _MSC_VER < 1800
static __inline__ __host__ __device__ int isfinite(long double a)
{
  return __finitel(a);
}
#else /* _MSC_VER < 1800 */
static __inline__ __host__ __device__ bool isfinite(long double a)
{
#if defined(__CUDA_ARCH__)
  return (__finitel(a) != 0);
#else /* defined(__CUDA_ARCH__) */
  return isfinite<long double>(a);
#endif /* defined(__CUDA_ARCH__) */
}
#endif /* _MSC_VER < 1800 */

#if _MSC_VER < 1800
#undef __RETURN_TYPE
#define __RETURN_TYPE int
/**
 * \ingroup CUDA_MATH_DOUBLE
 * 
 * \brief Determine whether argument is finite.
 *
 * Determine whether the floating-point value \p a is a finite value
 * (zero, subnormal, or normal and not infinity or NaN).
 *
 * \return
 * - With Visual Studio 2013 host compiler: __RETURN_TYPE is 'bool'. Returns
 * true if and only if \p a is a finite value.
 * - With other host compilers: __RETURN_TYPE is 'int'. Returns 
 * a nonzero value if and only if \p a is a finite value.
 */
static __inline__ __host__ __device__ __RETURN_TYPE isfinite(double a)
{
  return __finite(a);
}
#else /* _MSC_VER < 1800 */
#undef __RETURN_TYPE
#define __RETURN_TYPE bool
/**
 * \ingroup CUDA_MATH_DOUBLE
 * 
 * \brief Determine whether argument is finite.
 *
 * Determine whether the floating-point value \p a is a finite value
 * (zero, subnormal, or normal and not infinity or NaN).
 *
 * \return
 * - With Visual Studio 2013 host compiler: __RETURN_TYPE is 'bool'. Returns
 * true if and only if \p a is a finite value.
 * - With other host compilers: __RETURN_TYPE is 'int'. Returns 
 * a nonzero value if and only if \p a is a finite value.
 */
static __inline__ __host__ __device__ __RETURN_TYPE isfinite(double a)
{
#if defined(__CUDA_ARCH__)
  return (__finite(a) != 0);
#else /* defined(__CUDA_ARCH__) */
  return isfinite<double>(a);
#endif /* defined(__CUDA_ARCH__) */
}
#endif /* _MSC_VER < 1800 */

#if _MSC_VER < 1800
#undef __RETURN_TYPE
#define __RETURN_TYPE int
/**
 * \ingroup CUDA_MATH_SINGLE
 * \brief Determine whether argument is finite.
 *
 * Determine whether the floating-point value \p a is a finite value
 * (zero, subnormal, or normal and not infinity or NaN).
 *
 * \return
 * - With Visual Studio 2013 host compiler: __RETURN_TYPE is 'bool'. Returns
 * true if and only if \p a is a finite value.
 * - With other host compilers: __RETURN_TYPE is 'int'. Returns 
 * a nonzero value if and only if \p a is a finite value.
 */
static __inline__ __host__ __device__ __RETURN_TYPE isfinite(float a)
{
  return __finitef(a);
}
#else /* _MSC_VER < 1800 */
#undef __RETURN_TYPE
#define __RETURN_TYPE bool
/**
 * \ingroup CUDA_MATH_SINGLE
 * \brief Determine whether argument is finite.
 *
 * Determine whether the floating-point value \p a is a finite value
 * (zero, subnormal, or normal and not infinity or NaN).
 *
 * \return
 * - With Visual Studio 2013 host compiler: __RETURN_TYPE is 'bool'. Returns
 * true if and only if \p a is a finite value.
 * - With other host compilers: __RETURN_TYPE is 'int'. Returns 
 * a nonzero value if and only if \p a is a finite value.
 */
static __inline__ __host__ __device__ __RETURN_TYPE isfinite(float a)
{
#if defined(__CUDA_ARCH__)
  return (__finitef(a) != 0);
#else /* defined(__CUDA_ARCH__) */
  return isfinite<float>(a);
#endif /* defined(__CUDA_ARCH__) */
}
#endif /* _MSC_VER < 1800 */

#if _MSC_VER < 1800
template<class T> extern __host__ __device__ __cudart_builtin__ T _Pow_int(T, int);
extern __host__ __device__ __cudart_builtin__ long long int abs(long long int);
#else /* _MSC_VER < 1800 */
template<class T> extern __host__ __device__ __cudart_builtin__ T _Pow_int(T, int) throw();
extern __host__ __device__ __cudart_builtin__ long long int abs(long long int) throw();
#endif /* _MSC_VER < 1800 */

#endif /* __GNUC__ */

#if defined(__GNUC__) && !defined(_STLPORT_VERSION)
namespace std {
#endif /* __GNUC__ && !_STLPORT_VERSION */

#if defined(__GNUC__)

#if __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 8)
extern __host__ __device__ __cudart_builtin__ long long int abs(long long int);
#endif /* __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 8) */

#endif /* __GNUC__ */

#if _MSC_VER < 1800
extern __host__ __device__ __cudart_builtin__ long int __cdecl abs(long int);
extern __host__ __device__ __cudart_builtin__ float    __cdecl abs(float);
extern __host__ __device__ __cudart_builtin__ double   __cdecl abs(double);
extern __host__ __device__ __cudart_builtin__ float    __cdecl fabs(float);
extern __host__ __device__ __cudart_builtin__ float    __cdecl ceil(float);
extern __host__ __device__ __cudart_builtin__ float    __cdecl floor(float);
extern __host__ __device__ __cudart_builtin__ float    __cdecl sqrt(float);
extern __host__ __device__ __cudart_builtin__ float    __cdecl pow(float, float);
extern __host__ __device__ __cudart_builtin__ float    __cdecl pow(float, int);
extern __host__ __device__ __cudart_builtin__ double   __cdecl pow(double, int);
extern __host__ __device__ __cudart_builtin__ float    __cdecl log(float);
extern __host__ __device__ __cudart_builtin__ float    __cdecl log10(float);
extern __host__ __device__ __cudart_builtin__ float    __cdecl fmod(float, float);
extern __host__ __device__ __cudart_builtin__ float    __cdecl modf(float, float*);
extern __host__ __device__ __cudart_builtin__ float    __cdecl exp(float);
extern __host__ __device__ __cudart_builtin__ float    __cdecl frexp(float, int*);
extern __host__ __device__ __cudart_builtin__ float    __cdecl ldexp(float, int);
extern __host__ __device__ __cudart_builtin__ float    __cdecl asin(float);
extern __host__ __device__ __cudart_builtin__ float    __cdecl sin(float);
extern __host__ __device__ __cudart_builtin__ float    __cdecl sinh(float);
extern __host__ __device__ __cudart_builtin__ float    __cdecl acos(float);
extern __host__ __device__ __cudart_builtin__ float    __cdecl cos(float);
extern __host__ __device__ __cudart_builtin__ float    __cdecl cosh(float);
extern __host__ __device__ __cudart_builtin__ float    __cdecl atan(float);
extern __host__ __device__ __cudart_builtin__ float    __cdecl atan2(float, float);
extern __host__ __device__ __cudart_builtin__ float    __cdecl tan(float);
extern __host__ __device__ __cudart_builtin__ float    __cdecl tanh(float);
#else /* _MSC_VER < 1800 */
extern __host__ __device__ __cudart_builtin__ long int __cdecl abs(long int) throw();
extern __host__ __device__ __cudart_builtin__ float    __cdecl abs(float) throw();
extern __host__ __device__ __cudart_builtin__ double   __cdecl abs(double) throw();
extern __host__ __device__ __cudart_builtin__ float    __cdecl fabs(float) throw();
extern __host__ __device__ __cudart_builtin__ float    __cdecl ceil(float) throw();
extern __host__ __device__ __cudart_builtin__ float    __cdecl floor(float) throw();
extern __host__ __device__ __cudart_builtin__ float    __cdecl sqrt(float) throw();
extern __host__ __device__ __cudart_builtin__ float    __cdecl pow(float, float) throw();
extern __host__ __device__ __cudart_builtin__ float    __cdecl pow(float, int) throw();
extern __host__ __device__ __cudart_builtin__ double   __cdecl pow(double, int) throw();
extern __host__ __device__ __cudart_builtin__ float    __cdecl log(float) throw();
extern __host__ __device__ __cudart_builtin__ float    __cdecl log10(float) throw();
extern __host__ __device__ __cudart_builtin__ float    __cdecl fmod(float, float) throw();
extern __host__ __device__ __cudart_builtin__ float    __cdecl modf(float, float*) throw();
extern __host__ __device__ __cudart_builtin__ float    __cdecl exp(float) throw();
extern __host__ __device__ __cudart_builtin__ float    __cdecl frexp(float, int*) throw();
extern __host__ __device__ __cudart_builtin__ float    __cdecl ldexp(float, int) throw();
extern __host__ __device__ __cudart_builtin__ float    __cdecl asin(float) throw();
extern __host__ __device__ __cudart_builtin__ float    __cdecl sin(float) throw();
extern __host__ __device__ __cudart_builtin__ float    __cdecl sinh(float) throw();
extern __host__ __device__ __cudart_builtin__ float    __cdecl acos(float) throw();
extern __host__ __device__ __cudart_builtin__ float    __cdecl cos(float) throw();
extern __host__ __device__ __cudart_builtin__ float    __cdecl cosh(float) throw();
extern __host__ __device__ __cudart_builtin__ float    __cdecl atan(float) throw();
extern __host__ __device__ __cudart_builtin__ float    __cdecl atan2(float, float) throw();
extern __host__ __device__ __cudart_builtin__ float    __cdecl tan(float) throw();
extern __host__ __device__ __cudart_builtin__ float    __cdecl tanh(float) throw();
#endif /* _MSC_VER < 1800 */

#if defined(__GNUC__) && !defined(_STLPORT_VERSION)
}
#endif /* __GNUC__ && !_STLPORT_VERSION */

#if _MSC_VER < 1800
static __inline__ __host__ __device__ float logb(float a)
{
  return logbf(a);
}

static __inline__ __host__ __device__ int ilogb(float a)
{
  return ilogbf(a);
}

static __inline__ __host__ __device__ float scalbn(float a, int b)
{
  return scalbnf(a, b);
}

static __inline__ __host__ __device__ float scalbln(float a, long int b)
{
  return scalblnf(a, b);
}

static __inline__ __host__ __device__ float exp2(float a)
{
  return exp2f(a);
}

static __inline__ __host__ __device__ float expm1(float a)
{
  return expm1f(a);
}

static __inline__ __host__ __device__ float log2(float a)
{
  return log2f(a);
}

static __inline__ __host__ __device__ float log1p(float a)
{
  return log1pf(a);
}

static __inline__ __host__ __device__ float acosh(float a)
{
  return acoshf(a);
}

static __inline__ __host__ __device__ float asinh(float a)
{
  return asinhf(a);
}

static __inline__ __host__ __device__ float atanh(float a)
{
  return atanhf(a);
}

static __inline__ __host__ __device__ float hypot(float a, float b)
{
  return hypotf(a, b);
}

static __inline__ __host__ __device__ float cbrt(float a)
{
  return cbrtf(a);
}

static __inline__ __host__ __device__ float erf(float a)
{
  return erff(a);
}

static __inline__ __host__ __device__ float erfc(float a)
{
  return erfcf(a);
}

static __inline__ __host__ __device__ float lgamma(float a)
{
  return lgammaf(a);
}

static __inline__ __host__ __device__ float tgamma(float a)
{
  return tgammaf(a);
}

static __inline__ __host__ __device__ float copysign(float a, float b)
{
  return copysignf(a, b);
}

static __inline__ __host__ __device__ float nextafter(float a, float b)
{
  return nextafterf(a, b);
}

static __inline__ __host__ __device__ float remainder(float a, float b)
{
  return remainderf(a, b);
}

static __inline__ __host__ __device__ float remquo(float a, float b, int *quo)
{
  return remquof(a, b, quo);
}

static __inline__ __host__ __device__ float round(float a)
{
  return roundf(a);
}

static __inline__ __host__ __device__ long int lround(float a)
{
  return lroundf(a);
}

static __inline__ __host__ __device__ long long int llround(float a)
{
  return llroundf(a);
}

static __inline__ __host__ __device__ float trunc(float a)
{
  return truncf(a);
}

static __inline__ __host__ __device__ float rint(float a)
{
  return rintf(a);
}

static __inline__ __host__ __device__ long int lrint(float a)
{
  return lrintf(a);
}

static __inline__ __host__ __device__ long long int llrint(float a)
{
  return llrintf(a);
}

static __inline__ __host__ __device__ float nearbyint(float a)
{
  return nearbyintf(a);
}

static __inline__ __host__ __device__ float fdim(float a, float b)
{
  return fdimf(a, b);
}

static __inline__ __host__ __device__ float fma(float a, float b, float c)
{
  return fmaf(a, b, c);
}

static __inline__ __host__ __device__ float fmax(float a, float b)
{
  return fmaxf(a, b);
}

static __inline__ __host__ __device__ float fmin(float a, float b)
{
  return fminf(a, b);
}
#else /* _MSC_VER < 1800 */
extern __host__ __device__ __cudart_builtin__ float __cdecl logb(float) throw();
extern __host__ __device__ __cudart_builtin__ int   __cdecl ilogb(float) throw();
extern __host__ __device__ __cudart_builtin__ float __cdecl scalbn(float, float) throw();
extern __host__ __device__ __cudart_builtin__ float __cdecl scalbln(float, long int) throw();
extern __host__ __device__ __cudart_builtin__ float __cdecl exp2(float) throw();
extern __host__ __device__ __cudart_builtin__ float __cdecl expm1(float) throw();
extern __host__ __device__ __cudart_builtin__ float __cdecl log2(float) throw();
extern __host__ __device__ __cudart_builtin__ float __cdecl log1p(float) throw();
extern __host__ __device__ __cudart_builtin__ float __cdecl acosh(float) throw();
extern __host__ __device__ __cudart_builtin__ float __cdecl asinh(float) throw();
extern __host__ __device__ __cudart_builtin__ float __cdecl atanh(float) throw();
extern __host__ __device__ __cudart_builtin__ float __cdecl hypot(float, float) throw();
extern __host__ __device__ __cudart_builtin__ float __cdecl cbrt(float) throw();
extern __host__ __device__ __cudart_builtin__ float __cdecl erf(float) throw();
extern __host__ __device__ __cudart_builtin__ float __cdecl erfc(float) throw();
extern __host__ __device__ __cudart_builtin__ float __cdecl lgamma(float) throw();
extern __host__ __device__ __cudart_builtin__ float __cdecl tgamma(float) throw();
extern __host__ __device__ __cudart_builtin__ float __cdecl copysign(float, float) throw();
extern __host__ __device__ __cudart_builtin__ float __cdecl nextafter(float, float) throw();
extern __host__ __device__ __cudart_builtin__ float __cdecl remainder(float, float) throw();
extern __host__ __device__ __cudart_builtin__ float __cdecl remquo(float, float, int *) throw();
extern __host__ __device__ __cudart_builtin__ float __cdecl round(float) throw();
extern __host__ __device__ __cudart_builtin__ long int      __cdecl lround(float) throw();
extern __host__ __device__ __cudart_builtin__ long long int __cdecl llround(float) throw();
extern __host__ __device__ __cudart_builtin__ float         __cdecl trunc(float) throw();
extern __host__ __device__ __cudart_builtin__ float         __cdecl rint(float) throw();
extern __host__ __device__ __cudart_builtin__ long int      __cdecl lrint(float) throw();
extern __host__ __device__ __cudart_builtin__ long long int __cdecl llrint(float) throw();
extern __host__ __device__ __cudart_builtin__ float         __cdecl nearbyint(float) throw();
extern __host__ __device__ __cudart_builtin__ float         __cdecl fdim(float, float) throw();
extern __host__ __device__ __cudart_builtin__ float         __cdecl fma(float, float, float) throw();
extern __host__ __device__ __cudart_builtin__ float         __cdecl fmax(float, float) throw();
extern __host__ __device__ __cudart_builtin__ float         __cdecl fmin(float, float) throw();
#endif /* _MSC_VER < 1800 */

static __inline__ __host__ __device__ float exp10(float a)
{
  return exp10f(a);
}

static __inline__ __host__ __device__ float rsqrt(float a)
{
  return rsqrtf(a);
}

static __inline__ __host__ __device__ float rcbrt(float a)
{
  return rcbrtf(a);
}

static __inline__ __host__ __device__ float sinpi(float a)
{
  return sinpif(a);
}

static __inline__ __host__ __device__ float cospi(float a)
{
  return cospif(a);
}

static __inline__ __host__ __device__ void sincospi(float a, float *sptr, float *cptr)
{
  sincospif(a, sptr, cptr);
}

static __inline__ __host__ __device__ void sincos(float a, float *sptr, float *cptr)
{
  sincosf(a, sptr, cptr);
}

static __inline__ __host__ __device__ float j0(float a)
{
  return j0f(a);
}

static __inline__ __host__ __device__ float j1(float a)
{
  return j1f(a);
}

static __inline__ __host__ __device__ float jn(int n, float a)
{
  return jnf(n, a);
}

static __inline__ __host__ __device__ float y0(float a)
{
  return y0f(a);
}

static __inline__ __host__ __device__ float y1(float a)
{
  return y1f(a);
}

static __inline__ __host__ __device__ float yn(int n, float a)
{ 
  return ynf(n, a);
}

static __inline__ __host__ __device__ float cyl_bessel_i0(float a)
{
  return cyl_bessel_i0f(a);
}

static __inline__ __host__ __device__ float cyl_bessel_i1(float a)
{
  return cyl_bessel_i1f(a);
}

static __inline__ __host__ __device__ float erfinv(float a)
{
  return erfinvf(a);
}

static __inline__ __host__ __device__ float erfcinv(float a)
{
  return erfcinvf(a);
}

static __inline__ __host__ __device__ float normcdfinv(float a)
{
  return normcdfinvf(a);
}

static __inline__ __host__ __device__ float normcdf(float a)
{
  return normcdff(a);
}

static __inline__ __host__ __device__ float erfcx(float a)
{
  return erfcxf(a);
}

static __inline__ __host__ __device__ double copysign(double a, float b)
{
  return copysign(a, (double)b);
}

static __inline__ __host__ __device__ float copysign(float a, double b)
{
  return copysignf(a, (float)b);
}

static __inline__ __host__ __device__ unsigned int min(unsigned int a, unsigned int b)
{
  return umin(a, b);
}

static __inline__ __host__ __device__ unsigned int min(int a, unsigned int b)
{
  return umin((unsigned int)a, b);
}

static __inline__ __host__ __device__ unsigned int min(unsigned int a, int b)
{
  return umin(a, (unsigned int)b);
}

static __inline__ __host__ __device__ long long int min(long long int a, long long int b)
{
  return llmin(a, b);
}

static __inline__ __host__ __device__ unsigned long long int min(unsigned long long int a, unsigned long long int b)
{
  return ullmin(a, b);
}

static __inline__ __host__ __device__ unsigned long long int min(long long int a, unsigned long long int b)
{
  return ullmin((unsigned long long int)a, b);
}

static __inline__ __host__ __device__ unsigned long long int min(unsigned long long int a, long long int b)
{
  return ullmin(a, (unsigned long long int)b);
}

static __inline__ __host__ __device__ float min(float a, float b)
{
  return fminf(a, b);
}

static __inline__ __host__ __device__ double min(double a, double b)
{
  return fmin(a, b);
}

static __inline__ __host__ __device__ double min(float a, double b)
{
  return fmin((double)a, b);
}

static __inline__ __host__ __device__ double min(double a, float b)
{
  return fmin(a, (double)b);
}

static __inline__ __host__ __device__ unsigned int max(unsigned int a, unsigned int b)
{
  return umax(a, b);
}

static __inline__ __host__ __device__ unsigned int max(int a, unsigned int b)
{
  return umax((unsigned int)a, b);
}

static __inline__ __host__ __device__ unsigned int max(unsigned int a, int b)
{
  return umax(a, (unsigned int)b);
}

static __inline__ __host__ __device__ long long int max(long long int a, long long int b)
{
  return llmax(a, b);
}

static __inline__ __host__ __device__ unsigned long long int max(unsigned long long int a, unsigned long long int b)
{
  return ullmax(a, b);
}

static __inline__ __host__ __device__ unsigned long long int max(long long int a, unsigned long long int b)
{
  return ullmax((unsigned long long int)a, b);
}

static __inline__ __host__ __device__ unsigned long long int max(unsigned long long int a, long long int b)
{
  return ullmax(a, (unsigned long long int)b);
}

static __inline__ __host__ __device__ float max(float a, float b)
{
  return fmaxf(a, b);
}

static __inline__ __host__ __device__ double max(double a, double b)
{
  return fmax(a, b);
}

static __inline__ __host__ __device__ double max(float a, double b)
{
  return fmax((double)a, b);
}

static __inline__ __host__ __device__ double max(double a, float b)
{
  return fmax(a, (double)b);
}

/*******************************************************************************
*                                                                              *
*                                                                              *
*                                                                              *
*******************************************************************************/

#elif !defined(__CUDACC__)

#include "host_defines.h"
#include "math_constants.h"

#define __cuda_INT_MAX \
        ((int)((unsigned int)-1 >> 1))

#if defined(__CUDABE__)

#if defined(__CUDANVVM__)
#include "device_functions_decls.h"
#endif /* __CUDANVVM__ */

#include "device_functions.h"

#if defined(__CUDANVVM__)

/*******************************************************************************
*                                                                              *
* DEVICE                                                                       *
*                                                                              *
*******************************************************************************/

/*******************************************************************************
*                                                                              *
* DEVICE IMPLEMENTATIONS FOR FUNCTIONS WITH BUILTIN NVOPENCC OPERATIONS        *
*                                                                              *
*******************************************************************************/

static __forceinline__ float rintf(float a)
{
  return __nv_rintf(a);
}

static __forceinline__ long int lrintf(float a)
{
#if defined(__LP64__)
  return (long int)__float2ll_rn(a);
#else /* __LP64__ */
  return (long int)__float2int_rn(a);
#endif /* __LP64__ */
}

static __forceinline__ long long int llrintf(float a)
{
  return __nv_llrintf(a);
}

static __forceinline__ float nearbyintf(float a)
{
  return __nv_nearbyintf(a);
}

/*******************************************************************************
*                                                                              *
* DEVICE IMPLEMENTATIONS FOR FUNCTIONS WITHOUT BUILTIN NVOPENCC OPERATIONS     *
*                                                                              *
*******************************************************************************/

static __forceinline__ int __signbitf(float a)
{
  return __nv_signbitf(a);
}

#if _MSC_VER >= 1800
static __forceinline__ int __signbitl(/* we do not support long double yet, hence double */double a);
static __forceinline__ int _ldsign(/* we do not support long double yet, hence double */double a)
{
  return __signbitl(a);
}

static __forceinline__ int __signbit(double a);
static __forceinline__ int _dsign(double a)
{
  return __signbit(a);
}

static __forceinline__ int _fdsign(float a)
{
  return __signbitf(a);
}
#endif

/* The copysign() function returns a with its sign changed to 
 * match the sign of b.
 */
static __forceinline__ float copysignf(float a, float b)
{
  return __nv_copysignf(a, b);
}

static __forceinline__ int __finitef(float a)
{
  return __nv_finitef(a);
}

#if defined(__APPLE__)

static __forceinline__ int __isfinitef(float a)
{
  return __finitef(a);
}

#endif /* __APPLE__ */

static __forceinline__ int __isinff(float a)
{
  return __nv_isinff(a);
}

static __forceinline__ int __isnanf(float a)
{
  return __nv_isnanf(a);
}

static __forceinline__ float nextafterf(float a, float b)
{
  return __nv_nextafterf(a, b);
}

static __forceinline__ float nanf(const char *tagp)
{
  return __nv_nanf((const signed char *) tagp);
}

/*******************************************************************************
*                                                                              *
*                                                                              *
*                                                                              *
*******************************************************************************/

static __forceinline__ float sinf(float a)
{
#if defined(__USE_FAST_MATH__)
  return __nv_fast_sinf(a);
#else /* __USE_FAST_MATH__ */
  return __nv_sinf(a);
#endif /* __USE_FAST_MATH__ */
}

static __forceinline__ float cosf(float a)
{
#if defined(__USE_FAST_MATH__)
  return __nv_fast_cosf(a);
#else /* __USE_FAST_MATH__ */
  return __nv_cosf(a);
#endif /* __USE_FAST_MATH__ */
}

static __forceinline__ void sincosf(float a, float *sptr, float *cptr)
{
#if defined(__USE_FAST_MATH__)
  __nv_fast_sincosf(a, sptr, cptr);
#else /* __USE_FAST_MATH__ */
  __nv_sincosf(a, sptr, cptr);
#endif /* __USE_FAST_MATH__ */
}

static __forceinline__ float sinpif(float a)
{
  return __nv_sinpif(a);
}

static __forceinline__ float cospif(float a)
{
  return __nv_cospif(a);
}

static __forceinline__ void sincospif(float a, float *sptr, float *cptr)
{
  __nv_sincospif(a, sptr, cptr);
}

static __forceinline__ float tanf(float a)
{
#if defined(__USE_FAST_MATH__)
  return __nv_fast_tanf(a);
#else /* __USE_FAST_MATH__ */
  return __nv_tanf(a);
#endif /* __USE_FAST_MATH__ */
}

static __forceinline__ float log2f(float a)
{
#if defined(__USE_FAST_MATH__)
  return __nv_fast_log2f(a);
#else /* __USE_FAST_MATH__ */
  return __nv_log2f(a);
#endif /* __USE_FAST_MATH__ */
}

static __forceinline__ float expf(float a)
{
#if defined(__USE_FAST_MATH__)
  return __nv_fast_expf(a);
#else /* __USE_FAST_MATH__ */
  return __nv_expf(a);
#endif /* __USE_FAST_MATH__ */
}

static __forceinline__ float exp10f(float a)
{
#if defined(__USE_FAST_MATH__)
  return __nv_fast_exp10f(a);
#else /* __USE_FAST_MATH__ */
  return __nv_exp10f(a);
#endif /* __USE_FAST_MATH__ */
}

static __forceinline__ float coshf(float a)
{
  return __nv_coshf(a);
}

static __forceinline__ float sinhf(float a)
{
  return __nv_sinhf(a);
}

static __forceinline__ float tanhf(float a)
{
  return __nv_tanhf(a);
}

static __forceinline__ float atan2f(float a, float b)
{
  return __nv_atan2f(a, b);
}

static __forceinline__ float atanf(float a)
{
  return __nv_atanf(a);
}

static __forceinline__ float asinf(float a)
{
  return __nv_asinf(a);
}

static __forceinline__ float acosf(float a)
{
  return __nv_acosf(a);
}

static __forceinline__ float logf(float a)
{
#if defined(__USE_FAST_MATH__)
  return __nv_fast_logf(a);
#else /* __USE_FAST_MATH__ */
  return __nv_logf(a);
#endif /* __USE_FAST_MATH__ */
}

static __forceinline__ float log10f(float a)
{
#if defined(__USE_FAST_MATH__)
  return __nv_fast_log10f(a);
#else /* __USE_FAST_MATH__ */
  return __nv_log10f(a);
#endif /* __USE_FAST_MATH__ */
}

static __forceinline__ float log1pf(float a)
{
  return __nv_log1pf(a);
}

static __forceinline__ float acoshf(float a)
{
  return __nv_acoshf(a);
}

static __forceinline__ float asinhf(float a)
{
  return __nv_asinhf(a);
}

static __forceinline__ float atanhf(float a)
{
  return __nv_atanhf(a);
}

static __forceinline__ float expm1f(float a)
{
  return __nv_expm1f(a);
}

static __forceinline__ float hypotf(float a, float b)
{
  return __nv_hypotf(a, b);
}

static __forceinline__ float rhypotf(float a, float b)
{
  return __nv_rhypotf(a, b);
}
static __forceinline__ float cbrtf(float a)
{
  return __nv_cbrtf(a);
}

static __forceinline__ float rcbrtf(float a)
{
  return __nv_rcbrtf(a);
}

static __forceinline__ float j0f(float a)
{
  return __nv_j0f(a);
}

static __forceinline__ float j1f(float a)
{
  return __nv_j1f(a);
}

static __forceinline__ float y0f(float a)
{
  return __nv_y0f(a);
}

static __forceinline__ float y1f(float a)
{
  return __nv_y1f(a);
}

static __forceinline__ float ynf(int n, float a)
{
  return __nv_ynf(n, a);
}

static __forceinline__ float jnf(int n, float a)
{
  return __nv_jnf(n, a);
}

static __forceinline__ float cyl_bessel_i0f(float a)
{
  return __nv_cyl_bessel_i0f(a);
}

static __forceinline__ float cyl_bessel_i1f(float a)
{
  return __nv_cyl_bessel_i1f(a);
}

static __forceinline__ float erff(float a)
{
  return __nv_erff(a);
}

/* 
 * This implementation of erfinvf() adopts algorithmic techniques from a 
 * variety of sources. The transformation of the argument into log space 
 * follows Strecok [1], the transformation of the argument into reciprocal 
 * square root space for arguments near unity follows Blair et. al. [2], 
 * and the aggressive widening of the primary approximation interval around 
 * zero to minimize divergence as well as the use of a CUDA device function 
 * for the computation of the logarithm follows work by professor Mike Giles 
 * (Oxford University, UK). 
 * 
 * [1] Anthony J. Strecok, On the Calculation of the Inverse of the Error 
 * Function. Mathematics of Computation, Vol. 22, No. 101 (Jan. 1968), 
 * pp. 144-158 
 * [2] J. M. Blair, C. A. Edwards and J. H. Johnson, Rational Chebyshev 
 * Approximations for the Inverse of the Error Function. Mathematics 
 * of Computation, Vol. 30, No. 136 (Oct. 1976), pp. 827-830 
 */ 
static __forceinline__ float erfinvf(float a)
{
  return __nv_erfinvf(a);
}

static __forceinline__ float erfcf(float a)
{
  return __nv_erfcf(a);
}

static __forceinline__ float erfcxf(float a)
{
  return __nv_erfcxf(a);
}

static __forceinline__ float erfcinvf(float a)
{
  return __nv_erfcinvf(a);
}

static __forceinline__ float normcdfinvf(float a)
{
  return __nv_normcdfinvf(a);
}

/* normcdf(x) = 0.5 * erfc (-sqrt(0.5) * x). When x < -1.0, the numerical error
   incurred when scaling the argument of normcdf() is amplified, so we need to
   compensate this to guarantee accurate results.
*/
static __forceinline__ float normcdff(float a)
{
  return __nv_normcdff(a);
}

static __forceinline__ float lgammaf(float a)
{
  return __nv_lgammaf(a);
}

static __forceinline__ float ldexpf(float a, int b)
{
  return __nv_ldexpf(a, b);
}

static __forceinline__ float scalbnf(float a, int b)
{
  return __nv_scalbnf(a, b);
}

static __forceinline__ float scalblnf(float a, long int b)
{
  int t;
  if (b > 2147483647L) {
    t = 2147483647;
  } else if (b < (-2147483647 - 1)) {
    t = (-2147483647 - 1);
  } else {
    t = (int)b;
  }
  return scalbnf(a, t);
}

static __forceinline__ float frexpf(float a, int *b)
{
  return __nv_frexpf(a, b);
}

static __forceinline__ float modff(float a, float *b)
{
  return __nv_modff(a, b);
}

static __forceinline__ float fmodf(float a, float b)
{
  return __nv_fmodf(a, b);
}

static __forceinline__ float remainderf(float a, float b)
{
  return __nv_remainderf(a, b);
}

static __forceinline__ float remquof(float a, float b, int* quo)
{
  return __nv_remquof(a, b, quo);
}

static __forceinline__ float fmaf(float a, float b, float c)
{
  return __nv_fmaf(a, b, c);
}

static __forceinline__ float powif(float a, int b)
{
  return __nv_powif(a, b);
}

static __forceinline__ double powi(double a, int b)
{
  return __nv_powi(a, b);
}

static __forceinline__ float powf(float a, float b)
{
#if defined(__USE_FAST_MATH__)
  return __nv_fast_powf(a, b);
#else /* __USE_FAST_MATH__ */
  return __nv_powf(a, b);
#endif /* __USE_FAST_MATH__ */
}

/* Based on: Kraemer, W.: "Berechnung der Gammafunktion G(x) fuer reelle Punkt-
   und Intervallargumente". Zeitschrift fuer angewandte Mathematik und 
   Mechanik, Vol. 70 (1990), No. 6, pp. 581-584
*/
static __forceinline__ float tgammaf(float a)
{
  return __nv_tgammaf(a);
}

static __forceinline__ float roundf(float a)
{
  return __nv_roundf(a);
}

static __forceinline__ long long int llroundf(float a)
{
  return __nv_llroundf(a);
}

static __forceinline__ long int lroundf(float a)
{
#if defined(__LP64__)
  return (long int)llroundf(a);
#else /* __LP64__ */
  return (long int)(roundf(a));
#endif /* __LP64__ */
}

static __forceinline__ float fdimf(float a, float b)
{
  return __nv_fdimf(a, b);
}

static __forceinline__ int ilogbf(float a)
{
  return __nv_ilogbf(a);
}

static __forceinline__ float logbf(float a)
{
  return __nv_logbf(a);
}

#else /* __CUDANVVM__ */


/*******************************************************************************
*                                                                              *
* DEVICE                                                                       *
*                                                                              *
*******************************************************************************/

/*******************************************************************************
*                                                                              *
* DEVICE IMPLEMENTATIONS FOR FUNCTIONS WITH BUILTIN NVOPENCC OPERATIONS        *
*                                                                              *
*******************************************************************************/

static __forceinline__ float rintf(float a)
{
  return __builtin_roundf(a);
}

static __forceinline__ long int lrintf(float a)
{
#if defined(__LP64__)
  return (long int)__float2ll_rn(a);
#else /* __LP64__ */
  return (long int)__float2int_rn(a);
#endif /* __LP64__ */
}

static __forceinline__ long long int llrintf(float a)
{
  return __float2ll_rn(a);
}

static __forceinline__ float nearbyintf(float a)
{
  return __builtin_roundf(a);
}

/*******************************************************************************
*                                                                              *
* DEVICE IMPLEMENTATIONS FOR FUNCTIONS WITHOUT BUILTIN NVOPENCC OPERATIONS     *
*                                                                              *
*******************************************************************************/

static __forceinline__ int __signbitf(float a)
{
  return (int)((unsigned int)__float_as_int(a) >> 31);
}

#if _MSC_VER >= 1800
static __forceinline__ int __signbitl(/* we do not support long double yet, hence double */double a);
static __forceinline__ int _ldsign(/* we do not support long double yet, hence double */double a)
{
  return __signbitl(a);
}

static __forceinline__ int __signbit(double a);
static __forceinline__ int _dsign(double a)
{
  return __signbit(a);
}

static __forceinline__ int _fdsign(float a)
{
  return __signbitf(a);
}
#endif

/* The copysign() function returns a with its sign changed to 
 * match the sign of b.
 */
static __forceinline__ float copysignf(float a, float b)
{
  return __int_as_float((__float_as_int(b) &  0x80000000) | 
                        (__float_as_int(a) & ~0x80000000));
}

static __forceinline__ int __finitef(float a)
{
  return fabsf(a) < CUDART_INF_F;
}

#if defined(__APPLE__)

static __forceinline__ int __isfinitef(float a)
{
  return __finitef(a);
}

#endif /* __APPLE__ */

static __forceinline__ int __isinff(float a)
{
  return fabsf(a) == CUDART_INF_F;
}

static __forceinline__ int __isnanf(float a)
{
  return !(fabsf(a) <= CUDART_INF_F);
}

/* like copysign, but requires that argument a is positive */
static __forceinline__ float __internal_copysignf_pos(float a, float b)
{
  int ia = __float_as_int (a);
  int ib = __float_as_int (b);
  return __int_as_float (ia | (ib & 0x80000000));
}

static __forceinline__ float nextafterf(float a, float b)
{
  unsigned int ia;
  unsigned int ib;
  ia = __float_as_int(a);
  ib = __float_as_int(b);
#if defined(__CUDA_FTZ)
  if ((ia << 1) < 0x01000000) ia &= 0x80000000; /* DAZ */
  if ((ib << 1) < 0x01000000) ib &= 0x80000000; /* DAZ */
#endif
  if (__isnanf(a) || __isnanf(b)) return a + b; /* NaN */
  if (__int_as_float (ia | ib) == 0.0f) return __int_as_float(ib);
#if defined(__CUDA_FTZ)
  if (__int_as_float(ia) == 0.0f) {
    return __internal_copysignf_pos (CUDART_TWO_TO_M126_F, b);
  }
#else
  if (__int_as_float(ia) == 0.0f) {
    return __internal_copysignf_pos (CUDART_MIN_DENORM_F, b);
  }
#endif
  if ((a < b) && (a < 0.0f)) ia--;
  if ((a < b) && (a > 0.0f)) ia++;
  if ((a > b) && (a < 0.0f)) ia++;
  if ((a > b) && (a > 0.0f)) ia--;
  a = __int_as_float(ia);
#if defined(__CUDA_FTZ)
  if (a == 0.0f) {
    a = __int_as_float(ia & 0x80000000); /* FTZ */
  }
#endif
  return a;
}

static __forceinline__ unsigned long long int __internal_nan_kernel(const char *s)
{
  unsigned long long i = 0;
  int c;
  int ovfl = 0;
  int invld = 0;
  if (s && (*s == '0')) {
    s++;
    if ((*s == 'x') || (*s == 'X')) {
      s++; 
      while (*s == '0') s++;
      while (*s) {
        if (i > 0x0fffffffffffffffULL) {
          ovfl = 1;
        }
        c = (((*s) >= 'A') && ((*s) <= 'F')) ? (*s + 'a' - 'A') : (*s);
        if ((c >= 'a') && (c <= 'f')) { 
          c = c - 'a' + 10;
          i = i * 16 + c;
        } else if ((c >= '0') && (c <= '9')) { 
          c = c - '0';
          i = i * 16 + c;
        } else {
          invld = 1;
        }
        s++;
      }
    } else {
      while (*s == '0') s++;
      while (*s) {
        if (i > 0x1fffffffffffffffULL) {
          ovfl = 1;
        }
        c = *s;
        if ((c >= '0') && (c <= '7')) { 
          c = c - '0';
          i = i * 8 + c;
        } else {
          invld = 1; 
        }
        s++;
      }
    }
  } else if (s) {
    while (*s) {
      c = *s;
      if ((i > 1844674407370955161ULL) || 
          ((i == 1844674407370955161ULL) && (c > '5'))) {
        ovfl = 1;
      }
      if ((c >= '0') && (c <= '9')) { 
        c = c - '0';
        i = i * 10 + c;
      } else {
        invld = 1;
      }
      s++;
    }
  }
  if (ovfl) {
    i = ~0ULL;
  }
  if (invld) {
    i = 0ULL;
  }
  i = (i & 0x000fffffffffffffULL) | 0x7ff8000000000000ULL;
  return i;
}

static __forceinline__ float nanf(const char *tagp)
{
  return CUDART_NAN_F;
}

static __forceinline__ float __internal_fmad(float a, float b, float c)
{
#if __CUDA_ARCH__ >= 200
  return __fmaf_rn (a, b, c);
#else /* __CUDA_ARCH__ >= 200 */
  float r;
  asm ("mad.f32 %0, %1, %2, %3;" : "=f"(r) : "f"(a), "f"(b), "f"(c));
  return r;
#endif /* __CUDA_ARCH__ >= 200 */
}

static __forceinline__ float __internal_fast_rcpf(float a)
{
  float r; 
  asm ("rcp.approx.ftz.f32 %0,%1;" : "=f"(r) : "f"(a));
  return r;
}

static __forceinline__ float __internal_fast_rsqrtf(float a)
{
  float r; 
  asm ("rsqrt.approx.ftz.f32 %0,%1;" : "=f"(r) : "f"(a));
  return r;
}

static __forceinline__ float __internal_fast_log2f(float a)
{
  float r; 
  asm ("lg2.approx.ftz.f32 %0,%1;" : "=f"(r) : "f"(a));
  return r;
}

static __forceinline__ float __internal_fast_exp2f(float a)
{
  float r; 
  asm ("ex2.approx.ftz.f32 %0,%1;" : "=f"(r) : "f"(a));
  return r;
}

/* approximate 2*atanh(a/2) for |a| < 0.245 */
static __forceinline__ float __internal_atanhf_kernel(float a_1, float a_2)
{
  float a, a2, t;

  a = __fadd_rn (a_1, a_2);
  a2 = a * a;    
  t =                         1.566305595598990E-001f/64.0f;
  t = __internal_fmad (t, a2, 1.995081856004762E-001f/16.0f);
  t = __internal_fmad (t, a2, 3.333382699617026E-001f/4.0f);
  t = t * a2;
  t = __internal_fmad (t, a, a_2);
  t = t + a_1;
  return t;
}  

/* compute atan(a) in first octant, i.e. 0 <= a <= 1
 * eps ~= 2.16e-7
 */
static __forceinline__ float __internal_atanf_kernel(float a)
{
  float t4, t0, t1;

  t4 = __fmul_rn (a, a);
  t0 = -5.674867153f;
  t0 = __internal_fmad (t4, -0.823362947f, t0);
  t0 = __internal_fmad (t0, t4, -6.565555096f);
  t0 = t0 * t4;
  t0 = t0 * a;
  t1 = t4 + 11.33538818f;
  t1 = __internal_fmad (t1, t4, 28.84246826f);
  t1 = __internal_fmad (t1, t4, 19.69667053f);
  t1 = 1.0f / t1;
  a = __internal_fmad (t0, t1, a);
  return a;
}

/* approximate tangent on -pi/4...+pi/4 */
static __forceinline__ float __internal_tan_kernel(float a)
{
  float a2, s, t;

  a2 = a * a;
  t  = __internal_fmad (4.114678393115178E-003f, a2, -8.231194034909670E-001f);
  s  = a2 - 2.469348886157666E+000f;
  s  = 1.0f / s;
  t  = t * s;
  t  = t * a2;
  t  = __internal_fmad (t, a, a);
  return t;
}

static __forceinline__ float __internal_accurate_logf(float a)
{
  float t;
  float z;
  float m;

  if ((a > CUDART_ZERO_F) && (a < CUDART_INF_F)) {
    float e = -127.0f;
    int ia;
#if !defined(__CUDA_FTZ)
    /* normalize denormals */
    if (a < CUDART_TWO_TO_M126_F) {
      a = a * CUDART_TWO_TO_24_F;
      e -= 24.0f;
    }
#endif
    /* log(a) = 2 * atanh((a-1)/(a+1)) */
    ia = __float_as_int(a);
    m = __int_as_float((ia & 0x007fffff) | 0x3f800000);
    e += (float)((unsigned)ia >> 23);
    if (m > CUDART_SQRT_TWO_F) {
      m = m * 0.5f;
      e = e + 1.0f;
    }      
    t = m - 1.0f;
    z = m + 1.0f;
    z = __internal_fast_rcpf (z);
    z = __fmul_rn (z, -t * t);
    z = __internal_atanhf_kernel(t, z);
    z = __internal_fmad (e, CUDART_LN2_F, z);
  } else {
    /* handle special cases */
    z = __log2f(a);
  }
  return z;
}  

static __forceinline__ float2 __internal_log_ep(float a)
{
  float2 res;
  float expo;
  float m;
  float log_hi, log_lo;
  float t_hi, t_lo;
  float f, g, u, ulo, v, q;
  float r, s, e;
  int ia;

  expo = -127.0f;
#if !defined(__CUDA_FTZ)
  /* normalize denormals */
  if (a < CUDART_TWO_TO_M126_F) {
    a = a * CUDART_TWO_TO_24_F;
    expo -= 24.0f;
  }
#endif
  ia = __float_as_int(a);
  m = __int_as_float((ia & 0x007fffff) | 0x3f800000);
  expo += (float)((unsigned)ia >> 23);
  if (m > CUDART_SQRT_TWO_F) {
    m = m * 0.5f;
    expo = expo + 1.0f;
  }      

  /* compute log(m) with extended precision using an algorithm from P.T.P.
   * Tang, "Table Driven Implementation of the Logarithm Function", TOMS, 
   * Vol. 16., No. 4, December 1990, pp. 378-400. A modified polynomial 
   * approximation to atanh(x) on the interval [-0.1716, 0.1716] is utilized.
   */
  f = m - 1.0f;
  g = m + 1.0f;
  g = __internal_fast_rcpf (g);
  u = 2.0f * f * g;
  v = u * u;
  q =                        1.49356810919559350E-001f/64.0f;
  q = __internal_fmad (q, v, 1.99887797540072460E-001f/16.0f);
  q = __internal_fmad (q, v, 3.33333880955515580E-001f/ 4.0f);
  q = __fmul_rn (q, v);
  q = __fmul_rn (q, u);

  /* compute u + ulo = 2.0*(m-1.0)/(m+1.0) to more than single precision */
#if __CUDA_ARCH__ >= 200
  r = 2.0f * (f - u);
  r = __fmaf_rn (-u, f, r);
  ulo = __fmul_rn (g, r);
#else /* __CUDA_ARCH__ >= 200 */
  s = __int_as_float(__float_as_int(u) & 0xfffff000);
  v = __int_as_float(__float_as_int(f) & 0xfffff000);
  r = 2.0f * (f - s);
  f = f - v;
  r = __internal_fmad (-s, v, r);        /* multiplication is exact */
  r = __internal_fmad (-s, f, r);
  ulo = __fmul_rn (g, r);
  /* normalize result so u:ulo is a normalized double-single number */
  u = e = s + ulo;
  ulo = (s - e) + ulo;
#endif /* __CUDA_ARCH__ >= 200 */

  /* compute log(m) as double-single number log_hi:log_lo = u:ulo + q:0 */
  r = u + q;
  s = ((u - r) + q) + ulo;
  log_hi = e = r + s;
  log_lo = (r - e) + s;

  /* compute log(2)*expo as a double-single number t_hi:t_lo */
  t_hi = __fmul_rn (expo, 0.6931457519f);    /* multiplication is exact */
  t_lo = __fmul_rn (expo, 1.4286067653e-6f);

  /* log(a) = log(m) + log(2)*expo;  if expo != 0, |log(2)*expo| > |log(m)| */
  r = t_hi + log_hi;
  s = (((t_hi - r) + log_hi) + log_lo) + t_lo;
  res.y = e = r + s;
  res.x = (r - e) + s;
  return res;
}

static __forceinline__ float __internal_accurate_log2f(float a)
{
  return CUDART_L2E_F * __internal_accurate_logf(a);
}

/* Based on: Guillaume Da Graca, David Defour. Implementation of Float-Float 
 * Operators on Graphics Hardware. RNC'7, pp. 23-32, 2006.
 */
static __forceinline__ float2 __internal_dsmul (float2 x, float2 y)
{
  float2 z;
#if __CUDA_ARCH__ < 200
  float u1, u2, v1, v2, mh, ml, up;
  u1 = __internal_fmad (x.y, 4097.0f, __internal_fmad (-x.y, 4097.0f, x.y));
  v1 = __internal_fmad (y.y, 4097.0f, __internal_fmad (-y.y, 4097.0f, y.y));
  u2 = x.y - u1;
  v2 = y.y - v1;
  mh = __fmul_rn (x.y, y.y);
  ml = __internal_fmad (u1, v2, __internal_fmad (u1, v1, -mh));
  ml = __internal_fmad (u2, v2, __internal_fmad (v1, u2, ml));
  ml = __fmul_rn (x.y, y.x) + __fmul_rn (x.x, y.y) + ml;
  z.y = up = __fadd_rn (mh, ml);
  z.x = __fadd_rn (mh - up, ml);
#else /* __CUDA_ARCH__ < 200 */
  float up, mh, ml;
  ml = __fmul_rn (x.y, y.y);
  mh = __fmaf_rn (x.y, y.y, -ml);
  mh = __fmaf_rn (x.y, y.x, mh);
  mh = __fmaf_rn (x.x, y.y, mh);
  z.y = up = __fadd_rn (ml, mh);
  z.x = __fadd_rn (__fadd_rn (ml, -up), mh);
#endif /* __CUDA_ARCH__ < 200 */
  return z;
}

/* 160 bits of 2/PI for Payne-Hanek style argument reduction. */
static __constant__ unsigned int __cudart_i2opi_f [] = {
  0x3c439041,
  0xdb629599,
  0xf534ddc0,
  0xfc2757d1,
  0x4e441529,
  0xa2f9836e,
};

static __forceinline__ uint2 __internal_umad32wide (unsigned int a, 
                                                    unsigned int b, 
                                                    unsigned int c)
{
  uint2 res;
  asm ("{\n\t"
#if __CUDA_ARCH__ >= 200 && defined(__CUDANVVM__)
       "mad.lo.cc.u32   %0, %2, %3, %4;\n\t"
       "madc.hi.u32     %1, %2, %3,  0;\n\t"
#else  /* __CUDA_ARCH__ >= 200 && defined(__CUDANVVM__) */
       ".reg .u64 tmp;\n\t"
       "mul.wide.u32 tmp, %2, %3;\n\t"
       "mov.b64         {%0,%1}, tmp;\n\t"
       "add.cc.u32      %0, %0, %4;\n\t"
       "addc.u32        %1, %1, 0;\n\t"
#endif /* __CUDA_ARCH__ >= 200 && defined(__CUDANVVM__) */
       "}"
       : "=r"(res.x), "=r"(res.y)
       : "r"(a), "r"(b), "r"(c));
  return res;
}

/* Payne-Hanek style argument reduction. */
static
#if __CUDA_ARCH__ >= 200
__forceinline__
#else
__forceinline__
#endif
float __internal_trig_reduction_slowpath(float a, int *quadrant)
{
  uint2 p;
  unsigned int ia = __float_as_int(a);
  unsigned int s = ia & 0x80000000;
  unsigned int result[7];
  unsigned int hi, lo;
  unsigned int e;
  int idx;
  int q;
  e = ((ia >> 23) & 0xff) - 128;
  ia = (ia << 8) | 0x80000000;
  /* compute x * 2/pi */
  idx = 4 - (e >> 5);
  hi = 0;
#pragma unroll 1
  for (q = 0; q < 6; q++) {
    p = __internal_umad32wide (__cudart_i2opi_f[q], ia, hi);
    lo = p.x;
    hi = p.y;
    result[q] = lo;
  }
  result[q] = hi;
  e = e & 31;
  /* shift result such that hi:lo<63:62> are the least significant
     integer bits, and hi:lo<61:0> are the fractional bits of the result
  */
  hi = result[idx+2];
  lo = result[idx+1];
  if (e) {
    q = 32 - e;
    hi = (hi << e) + (lo >> q);
    lo = (lo << e) + (result[idx] >> q);
  }
  q = hi >> 30;
  /* fraction */
  hi = (hi << 2) + (lo >> 30);
  lo = (lo << 2);
  e = hi >> 31;  /* fraction >= 0.5 */
  q += e;
  if (s) q = -q;
  *quadrant = q;
  if (e) {
    unsigned int t;
    hi = ~hi;
    lo = -(int)lo;
    t = (lo == 0);
    hi += t;
    s = s ^ 0x80000000;
  }
  /* normalize fraction */
  e = __clz(hi);
  if (e) {
      hi = (hi << e) + (lo >> (32 - e)); 
  }
  lo = hi * 0xc90fdaa2;
  hi = __umulhi(hi, 0xc90fdaa2);
  if ((int)hi > 0) {
    hi = (hi << 1) + (lo >> 31);
    e++;
  }
  ia = s | (((126 - e) << 23) + ((((hi + 1) >> 7) + 1) >> 1));
  return __int_as_float(ia);
}

/* reduce argument to trig function to -pi/4...+pi/4 */
static __forceinline__ float __internal_trig_reduction_kernel(float a, int *quadrant)
{
  float j, t;
  int q;
  q = __float2int_rn (a * CUDART_2_OVER_PI_F);
  j = (float)q;
#if __CUDA_ARCH__ >= 200
  /* take advantage of FMA */
  t = __fmaf_rn (-j, 1.5707962512969971e+000f, a);
  t = __fmaf_rn (-j, 7.5497894158615964e-008f, t);
  t = __fmaf_rn (-j, 5.3903029534742384e-015f, t);
#else /* __CUDA_ARCH__ >= 200 */
  t = __internal_fmad (-j, 1.5703125000000000e+000f, a);
  t = __internal_fmad (-j, 4.8351287841796875e-004f, t);
  t = __internal_fmad (-j, 3.1385570764541626e-007f, t);
  t = __internal_fmad (-j, 6.0771005065061922e-011f, t);
#endif /* __CUDA_ARCH__ >= 200 */
  if (fabsf(a) > CUDART_TRIG_PLOSS_F) {
    t = __internal_trig_reduction_slowpath (a, &q);
  }
  *quadrant = q;
  return t;
}

static __forceinline__ float __internal_expf_arg_reduction(float a, float *i)
{
  float j, z;

  j = truncf (a * CUDART_L2E_F);
  z = __internal_fmad (j, -0.6931457519f,    a);
  z = __internal_fmad (j, -1.4286067653e-6f, z);
  z = z * CUDART_L2E_F;
  *i = j;
  return z;
}

static __forceinline__ float __internal_expf_kernel(float a, float scale)
{
  float j, z;

  z = __internal_expf_arg_reduction (a, &j);
  z = __internal_fast_exp2f(z) * exp2f(j + scale);
  return z;
}

static __forceinline__ float __internal_expf_add_kernel(float a, float addend)
{
  float j, z;

  z = __internal_expf_arg_reduction (a, &j);
  z = __internal_fmad (__internal_fast_exp2f(z), exp2f(j), addend);
  return z;
}

static __forceinline__ float __internal_accurate_expf(float a)
{
  float z;

  z = __internal_expf_kernel(a, 0.0f);
  if (a < -105.0f) z = 0.0f;
  if (a >  105.0f) z = CUDART_INF_F;
  return z;
}

static __forceinline__ float __internal_accurate_exp10f(float a)
{
  float j, z;

  j = truncf(a * CUDART_L2T_F);
  z = __internal_fmad (j, -3.0102920532226563e-001f, a);
  z = __internal_fmad (j, -7.9034171557301747e-007f, z);
  z = z * CUDART_L2T_F;
  z = __internal_fast_exp2f(z) * exp2f(j);
  if (a < -46.0f) z = 0.0f;
  if (a >  46.0f) z = CUDART_INF_F;
  return z;
}

static __forceinline__ float __internal_lgammaf_pos(float a)
{
  float sum;
  float s, t;

  if (a >= 3.0f) {
    if (a >= 7.8f) {
      /* Stirling approximation for a >= 8; coefficients from Hart et al, 
       * "Computer Approximations", Wiley 1968. Approximation 5401
       */
      s = __internal_fast_rcpf (a);
      t = s * s;
      sum =                           0.77783067e-3f;
      sum = __internal_fmad (sum, t, -0.2777655457e-2f);
      sum = __internal_fmad (sum, t,  0.83333273853e-1f);
      sum = __internal_fmad (sum, s,  0.918938533204672f);
      s = 0.5f * __internal_accurate_logf(a);
      t = a - 0.5f;
      s = __fmul_rn(s, t);
      t = s - a;
      s = __fadd_rn(s, sum); /* prevent FMAD merging */
      t = t + s;
      if (a == CUDART_INF_F) {
        t = a;
      }
    } else {
      a = a - 3.0f;
      s =                        -7.488903254816711E+002f;
      s = __internal_fmad (s, a, -1.234974215949363E+004f);
      s = __internal_fmad (s, a, -4.106137688064877E+004f);
      s = __internal_fmad (s, a, -4.831066242492429E+004f);
      s = __internal_fmad (s, a, -1.430333998207429E+005f);
      t =                     a - 2.592509840117874E+002f;
      t = __internal_fmad (t, a, -1.077717972228532E+004f);
      t = __internal_fmad (t, a, -9.268505031444956E+004f);
      t = __internal_fmad (t, a, -2.063535768623558E+005f);
      t = __internal_fmad (s, __internal_fast_rcpf (t), a);
    }
  } else if (a >= 1.5f) {
    a = a - 2.0f;
    t =                         4.959849168282574E-005f;
    t = __internal_fmad (t, a, -2.208948403848352E-004f);
    t = __internal_fmad (t, a,  5.413142447864599E-004f);
    t = __internal_fmad (t, a, -1.204516976842832E-003f);
    t = __internal_fmad (t, a,  2.884251838546602E-003f);
    t = __internal_fmad (t, a, -7.382757963931180E-003f);
    t = __internal_fmad (t, a,  2.058131963026755E-002f);
    t = __internal_fmad (t, a, -6.735248600734503E-002f);
    t = __internal_fmad (t, a,  3.224670187176319E-001f);
    t = __internal_fmad (t, a,  4.227843368636472E-001f);
    t = t * a;
  } else if (a >= 0.7f) {
    a = 1.0f - a;
    t =                        4.588266515364258E-002f;
    t = __internal_fmad (t, a, 1.037396712740616E-001f);
    t = __internal_fmad (t, a, 1.228036339653591E-001f);
    t = __internal_fmad (t, a, 1.275242157462838E-001f);
    t = __internal_fmad (t, a, 1.432166835245778E-001f);
    t = __internal_fmad (t, a, 1.693435824224152E-001f);
    t = __internal_fmad (t, a, 2.074079329483975E-001f);
    t = __internal_fmad (t, a, 2.705875136435339E-001f);
    t = __internal_fmad (t, a, 4.006854436743395E-001f);
    t = __internal_fmad (t, a, 8.224669796332661E-001f);
    t = __internal_fmad (t, a, 5.772156651487230E-001f);
    t = t * a;
  } else {
    t =                         3.587515669447039E-003f;
    t = __internal_fmad (t, a, -5.471285428060787E-003f);
    t = __internal_fmad (t, a, -4.462712795343244E-002f);
    t = __internal_fmad (t, a,  1.673177015593242E-001f);
    t = __internal_fmad (t, a, -4.213597883575600E-002f);
    t = __internal_fmad (t, a, -6.558672843439567E-001f);
    t = __internal_fmad (t, a,  5.772153712885004E-001f);
    t = t * a;
    t = __internal_fmad (t, a, a);
    t = -__internal_accurate_logf(t);
  }
  return t;
}

/* approximate sine on -pi/4...+pi/4 */
static __forceinline__ float __internal_sin_kernel(float x)
{
  float x2, z;

  x2 = x * x;
  z  =                         -1.95152959e-4f;
  z  = __internal_fmad (z, x2,  8.33216087e-3f);
  z  = __internal_fmad (z, x2, -1.66666546e-1f);
#if __CUDA_ARCH__ >= 200
  z  = __internal_fmad (z, x2,  0.0f);
#else  /* __CUDA_ARCH__ >= 200 */
  z  = z * x2;
#endif /* __CUDA_ARCH__ >= 200 */ 
  z  = __internal_fmad (z, x, x);
  return z;
}

/* approximate cosine on -pi/4...+pi/4 */
static __forceinline__ float __internal_cos_kernel(float x)
{
  float x2, z;

  x2 = x * x;
  z  =                          2.44331571e-5f;
  z  = __internal_fmad (z, x2, -1.38873163e-3f);
  z  = __internal_fmad (z, x2,  4.16666457e-2f);
  z  = __internal_fmad (z, x2, -5.00000000e-1f);
  z  = __internal_fmad (z, x2,  1.00000000e+0f);
  return z;
}

static __forceinline__ float __internal_sin_cos_kernel(float x, int i)
{
#if __CUDA_ARCH__ >= 200
  float x2, z;
  x2 = __fmul_rn (x, x);
  if (i & 1) {
    z  =                          2.44331571e-5f;
    z  = __internal_fmad (z, x2, -1.38873163e-3f);
  } else {
    z  =                         -1.95152959e-4f;
    z  = __internal_fmad (z, x2,  8.33216087e-3f);
  }
  if (i & 1) {
    z  = __internal_fmad (z, x2,  4.16666457e-2f);
    z  = __internal_fmad (z, x2, -5.00000000e-1f);
  } else {
    z  = __internal_fmad (z, x2, -1.66666546e-1f);
    z  = __internal_fmad (z, x2,  0.0f);
  }
  x = __internal_fmad (z, x, x);
  if (i & 1) x = __internal_fmad (z, x2, 1.0f);
  if (i & 2) x = __internal_fmad (x, -1.0f, CUDART_ZERO_F);
#else /* __CUDA_ARCH__ >= 200 */
  if (i & 1) {
    x = __internal_cos_kernel(x);
  } else {
    x = __internal_sin_kernel(x);
  }
  if (i & 2) {
    x = __internal_fmad (x, -1.0f, CUDART_ZERO_F);
  }
#endif /* __CUDA_ARCH__ >= 200 */
  return x;
}

static __forceinline__ void __internal_accurate_sincosf(float a, float *sptr, float *cptr)
{
#if !defined(__CUDANVVM__)
  volatile
#endif /* !defined(__CUDANVVM__) */
  float s, c;
  float t;
  int quadrant;

  if (__isinff(a)) {
    a = __fmul_rn (a, CUDART_ZERO_F); /* return NaN */
  }
  a = __internal_trig_reduction_kernel(a, &quadrant);
  c = __internal_cos_kernel(a);
  s = __internal_sin_kernel(a);
  t = s;
  if (quadrant & 1) {
    s = c;
    c = t;
  }
  if (quadrant & 2) {
    s = -s;
  }
  quadrant++;
  if (quadrant & 2) {
    c = -c;
  }
#if __CUDA_ARCH__ < 200
  if (a == CUDART_ZERO_F) {
    s = __fmul_rn (a, CUDART_ZERO_F);
  }
#endif /* __CUDA_ARCH__ < 200 */
  *sptr = s;
  *cptr = c;
}

static __forceinline__ float __internal_accurate_sinf(float a)
{
#if __CUDA_ARCH__ < 200
  float s, c;
  __internal_accurate_sincosf (a, &s, &c);
  return s;
#else /* __CUDA_ARCH__ < 200 */
  float z;
  int i;

  if (__isinff(a)) {
    a = __fmul_rn (a, CUDART_ZERO_F);
  }
  a = __internal_trig_reduction_kernel(a, &i);
  z = __internal_sin_cos_kernel(a, i);
  return z;
#endif
}

static __forceinline__ float __internal_accurate_cosf(float a)
{
#if __CUDA_ARCH__ < 200
  float s, c;
  __internal_accurate_sincosf (a, &s, &c);
  return c;
#else /* __CUDA_ARCH__ < 200 */
  float z;
  int i;

  if (__isinff(a)) {
    a = __fmul_rn (a, CUDART_ZERO_F);
  }
  z = __internal_trig_reduction_kernel(a, &i);
  i++;
  z = __internal_sin_cos_kernel(z, i);
  return z;
#endif /* __CUDA_ARCH__ < 200 */
}

/* Compute cos(x + offset) with phase offset computed after argument
   reduction for higher accuracy.  Needed for Bessel approximation 
   functions.
*/
static __forceinline__ float __internal_cos_offset_f(float a, float offset)
{
  float z;
  int i;

  z = __internal_trig_reduction_kernel(a, &i);
  a = z + offset + (i & 3) * CUDART_PIO2_F;
  return __internal_accurate_cosf(a);
}

/*******************************************************************************
*                                                                              *
*                                                                              *
*                                                                              *
*******************************************************************************/

static __forceinline__ float sinf(float a)
{
#if defined(__USE_FAST_MATH__)
  return __sinf(a);
#else /* __USE_FAST_MATH__ */
  return __internal_accurate_sinf(a);
#endif /* __USE_FAST_MATH__ */
}

static __forceinline__ float cosf(float a)
{
#if defined(__USE_FAST_MATH__)
  return __cosf(a);
#else /* __USE_FAST_MATH__ */
  return __internal_accurate_cosf(a);
#endif /* __USE_FAST_MATH__ */
}

static __forceinline__ void sincosf(float a, float *sptr, float *cptr)
{
#if defined(__USE_FAST_MATH__)
  __sincosf(a, sptr, cptr);
#else /* __USE_FAST_MATH__ */
  __internal_accurate_sincosf(a, sptr, cptr);
#endif /* __USE_FAST_MATH__ */
}

static __forceinline__ float sinpif(float a)
{
  float z;
  int i;

  z = rintf (a + a);
  i = (int)z;
  z = __internal_fmad (-z, 0.5f, a);
#if __CUDA_ARCH__ >= 200
  z = __fmaf_rn (z, 3.141592503e+00f, z * 1.509958025e-07f);
#else  /* __CUDA_ARCH__ >= 200 */
  z = z * CUDART_PI_F;
#endif /* __CUDA_ARCH__ >= 200 */
  z = __internal_sin_cos_kernel (z, i);
  if (a == truncf(a)) {
    z = __fmul_rn (a, CUDART_ZERO_F);
  }
  return z;
}

static __forceinline__ float cospif(float a)
{
  float z;
  int i;

  if (fabsf(a) > CUDART_TWO_TO_24_F) {
    a = __fmul_rn (a, CUDART_ZERO_F);
  }
  z = rintf (a + a);
  i = (int)z;
  z = __internal_fmad (-z, 0.5f, a);
#if __CUDA_ARCH__ >= 200
  z = __fmaf_rn (z, 3.141592503e+00f, z * 1.509958025e-07f);
#else  /* __CUDA_ARCH__ >= 200 */
  z = z * CUDART_PI_F;
#endif /* __CUDA_ARCH__ >= 200 */
  i++;
  z = __internal_sin_cos_kernel (z, i);
  return z;
}

static __forceinline__ void sincospif(float a, float *sptr, float *cptr)
{
#if !defined(__CUDANVVM__)
  volatile
#endif
  float s, c;
  float t;
  int i;

  t = rintf (a + a);
  i = (int)t;
  t = __internal_fmad (-t, 0.5f, a);
#if __CUDA_ARCH__ >= 200
  t = __fmaf_rn (t, 3.141592503e+00f, t * 1.509958025e-07f);
#else /* __CUDA_ARCH__ >= 200 */
  t = t * CUDART_PI_F;
#endif /* __CUDA_ARCH__ >= 200 */
  c = __internal_cos_kernel(t);
  s = __internal_sin_kernel(t);
  t = s;
  if (i & 1) {
    s = c;
    c = t;
  }
  if (i & 2) {
    s = -s;
  }
  i++;
  if (i & 2) {
    c = __internal_fmad (c, -1.0f, CUDART_ZERO_F);
  }
  if (a == truncf(a)) {
    s = __fmul_rn (a, CUDART_ZERO_F);
  }
  if (fabsf(a) > CUDART_TWO_TO_24_F) {
    c = __fadd_rn (s, 1.0f);
  }
  *sptr = s;
  *cptr = c;
}

static __forceinline__ float tanf(float a)
{
#if defined(__USE_FAST_MATH__)
  return __tanf(a);
#else /* __USE_FAST_MATH__ */
  float z;
  int   i;

  if (__isinff(a)) {
    a = __fmul_rn (a, CUDART_ZERO_F); /* return NaN */
  }
  z = __internal_trig_reduction_kernel(a, &i);
  /* here, abs(z) <= pi/4, and i has the quadrant */
  z = __internal_tan_kernel(z);
  if (i & 1) {
    z = - (1.0f / z);
  }
  return z;
#endif /* __USE_FAST_MATH__ */
}

static __forceinline__ float log2f(float a)
{
#if defined(__USE_FAST_MATH__)
  return __log2f(a);
#else /* __USE_FAST_MATH__ */
  return __internal_accurate_log2f(a);
#endif /* __USE_FAST_MATH__ */
}

static __forceinline__ float expf(float a)
{
#if defined(__USE_FAST_MATH__)
  return __expf(a);
#else /* __USE_FAST_MATH__ */
  return __internal_accurate_expf(a);
#endif /* __USE_FAST_MATH__ */
}

static __forceinline__ float exp10f(float a)
{
#if defined(__USE_FAST_MATH__)
  return __exp10f(a);
#else /* __USE_FAST_MATH__ */
  return __internal_accurate_exp10f(a);
#endif /* __USE_FAST_MATH__ */
}

static __forceinline__ float coshf(float a)
{
  float z;

  a = fabsf(a);
  z = __internal_expf_kernel(a, -2.0f);
  z = __internal_fmad (2.0f, z,  __fdividef (0.125f, z));
  if (a >= 90.0f) {
    z = CUDART_INF_F;     /* overflow -> infinity */
  }
  return z;
}

static __forceinline__ float sinhf(float a)
{
  float s, z;

  s = a;
  a = fabsf(a);
  if (a >= 1.0f) {         /* no danger of catastrophic cancellation */
    z = __internal_expf_kernel(a, -2.0f);
    z = __internal_fmad (2.0f, z, -__fdividef (0.125f, z));
    if (a >= 90.0f) {
      z = CUDART_INF_F;     /* overflow -> infinity */
    }
    z =  __internal_copysignf_pos (z, s);
  } else {
    float a2 = s * s;
    /* approximate sinh(x) on [0,1] with a polynomial */
    z =                         2.816951222e-6f;
    z = __internal_fmad (z, a2, 1.983615978e-4f);
    z = __internal_fmad (z, a2, 8.333350058e-3f);
    z = __internal_fmad (z, a2, 1.666666650e-1f);
    z = z * a2;
    z = __internal_fmad (z, s, s);
  }
  return z;
}

static __forceinline__ float tanhf(float a)
{
  float s, t;

  t = fabsf(a);
  if (t >= 0.55f) {
    s = __internal_fmad (__internal_fast_rcpf (
                         __internal_expf_add_kernel (t+t, 1.0f)), -2.0f, 1.0f);
    if (t >= 88.0f) {
      s = 1.0f;
    }
    s = __internal_copysignf_pos (s, a);
  } else {
    float z2;
    z2 = a * a;
    t =                          1.643758066599993e-2f;
    t = __internal_fmad (t, z2, -5.267181327760551e-2f);
    t = __internal_fmad (t, z2,  1.332072505223051e-1f);
    t = __internal_fmad (t, z2, -3.333294663641083e-1f);
    t = t * z2;
    s = __internal_fmad (t, a, a);
    if (a == 0.0f) {   /* preserve negative zero */
      s = a + a;
    }
  }
  return s;
}

static __forceinline__ float atan2f(float a, float b)
{
  float t0, t1, t2, t3, t4;

  t1 = fabsf(b);
  t2 = fabsf(a);
  if (t1 == 0.0f && t2 == 0.0f) {
    t0 = (__float_as_int(b) < 0) ? CUDART_PI_F : CUDART_ZERO_F;
    t0 = __internal_copysignf_pos(t0, a);
  } else if ((t1 == CUDART_INF_F) && (t2 == CUDART_INF_F)) {
    t0 = (__float_as_int(b) < 0) ? CUDART_3PIO4_F : CUDART_PIO4_F;
    t0 = __internal_copysignf_pos(t0, a);
  } else {
    t3 = fmaxf(t2, t1);
    t4 = fminf(t2, t1);
    t0 = t4 / t3;
    t0 = __internal_atanf_kernel(t0);
    /* Map result according to octant. */
    if (t2 > t1) t0 = CUDART_PIO2_F - t0;
    if (__float_as_int(b) < 0) t0 = CUDART_PI_F - t0;
    t0 = __internal_copysignf_pos(t0, a);
    t1 = t1 + t2;
    if (!(t1 <= CUDART_INF_F)) t0 = t1;  /* handle NaN inputs */
  }
  return t0;
}

static __forceinline__ float atanf(float a)
{
  float t0, t1;

  /* reduce argument to first octant */
  t0 = fabsf(a);
  t1 = t0;
  if (t0 > 1.0f) {
    t1 = 1.0f / t1;
  }
  /* approximate atan(a) in first octant */
  t1 = __internal_atanf_kernel(t1);
  /* map result according to octant. */
  if (t0 > 1.0f) {
    t1 = CUDART_PIO2_F - t1;
  }
  if (t0 <= CUDART_INF_F) {
    t1 = __internal_copysignf_pos (t1, a);
  }
  return t1;
}

/* approximate asin(a) on [0, 0.57] */
static __forceinline__ float __internal_asinf_kernel(float a)
{
  float a2, t;
  a2 = a * a;
  t =                         5.175137819e-002f;
  t = __internal_fmad (t, a2, 1.816697683e-002f);
  t = __internal_fmad (t, a2, 4.675685871e-002f);
  t = __internal_fmad (t, a2, 7.484657646e-002f);
  t = __internal_fmad (t, a2, 1.666701424e-001f);
  t = t * a2;
  a = __internal_fmad (t, a, a);
  return a;
}

static __forceinline__ float asinf(float a)
{
  float t0, t1, t2;

  t0 = fabsf(a);
  t2 = 1.0f - t0;
  t2 = 0.5f * t2;
  t2 = sqrtf(t2);
  t1 = t0 > 0.57f ? t2 : t0;
  t1 = __internal_asinf_kernel(t1);
  t2 = __internal_fmad (-2.0f, t1, CUDART_PIO2_F);
  if (t0 > 0.57f) {
    t1 = t2;
  }
  if (t1 <= CUDART_INF_F) {
    t1 = __internal_copysignf_pos (t1, a);
  }
  return t1;
}

static __forceinline__ float acosf(float a)
{
  float t0, t1, t2;

  t0 = fabsf(a);
  t2 = 1.0f - t0;
  t2 = 0.5f * t2;
  t2 = sqrtf(t2);
  t1 = t0 > 0.57f ? t2 : t0;
  t1 = __internal_asinf_kernel(t1);
  t1 = t0 > 0.57f ? 2.0f * t1 : CUDART_PIO2_F - t1;
  if (a < 0.0f) {
    t1 = CUDART_PI_F - t1;
  }
  return t1;
}

static __forceinline__ float logf(float a)
{
#if defined(__USE_FAST_MATH__)
  return __logf(a);
#else /* __USE_FAST_MATH__ */
  return __internal_accurate_logf(a);
#endif /* __USE_FAST_MATH__ */
}

static __forceinline__ float log10f(float a)
{
#if defined(__USE_FAST_MATH__)
  return __log10f(a);
#else /* __USE_FAST_MATH__ */
  return CUDART_LGE_F * __internal_accurate_logf(a);
#endif /* __USE_FAST_MATH__ */
}

static __forceinline__ float log1pf(float a)
{
  float t;
  if (a >= -0.394f && a <= 0.65f) {
    /* log(a+1) = 2*atanh(a/(a+2)) */
    t = a + 2.0f;
    t = __fdividef (a, t);
    t = __fmul_rn (-a, t);
    t = __internal_atanhf_kernel (a, t);
  } else {
    t = __internal_accurate_logf (CUDART_ONE_F + a);
  }
  return t;
}

static __forceinline__ float acoshf(float a)
{
  float t;

  t = a - 1.0f;
  if ((unsigned)__float_as_int(t) > __float_as_int(CUDART_TWO_TO_23_F)) {
    /* for large a, acosh(a) = log(2*a) */
    t = CUDART_LN2_F + __internal_accurate_logf(t);
  } else {
    t = t + sqrtf(__fadd_rn(__fmul_rz(a, t), t));
    t = log1pf(t);
  }
  return t;
}

static __forceinline__ float asinhf(float a)
{
  float fa, oofa, t;

  fa = fabsf(a);
  if (fa > CUDART_TWO_TO_126_F) {        /* prevent intermediate underflow */
    t = __fadd_rn (CUDART_LN2_F, __logf(fa));  /* fast logf() is safe here */
  } else {
    oofa = 1.0f / fa;
    t = __internal_fmad (fa, __internal_fast_rcpf (oofa + 
                             sqrtf (__internal_fmad (oofa, oofa, 1.0f))), fa);
    t = log1pf(t);
  }
  if (fa <= CUDART_INF_F) {
    t = __internal_copysignf_pos (t, a);
  }
  return t;
}

static __forceinline__ float atanhf(float a)
{
  float fa, t;

  fa = fabsf(a);
  t = __internal_fast_rcpf(1.0f- fa);
  t = t * 2.0f * fa;
  if (fa > CUDART_TWO_TO_126_F) t = -2.0f;
  t = 0.5f * log1pf(t);
  if (fabsf(t) <= CUDART_INF_F) {
    t = __internal_copysignf_pos (t, a);
  }
  return t;
} 

static __forceinline__ float expm1f(float a)
{
  float t, z, j, u;
  /* expm1(a) = 2^t*(expm1(z)+1)-1 */
  t = rintf (a * CUDART_L2E_F);
  /* prevent loss of accuracy for args a tad outside [-0.5*log(2),0.5*log(2)]*/
  if (fabsf(a) < 0.41f) {
    t = CUDART_ZERO_F;
  }  
  z = __internal_fmad (-t, 0.6931457519f, a);
  z = __internal_fmad (-t, 1.4286067653e-6f, z);
  /* prevent intermediate overflow in computation of 2^t */
  j = t;
  if (t == 128.0f) j = j - 1.0f; 
  /* expm1(z) on [log(2/3), log(3/2)] */
  u =                        1.38795078474044430E-003f;
  u = __internal_fmad (u, z, 8.38241261853264930E-003f);
  u = __internal_fmad (u, z, 4.16678317762833940E-002f);
  u = __internal_fmad (u, z, 1.66663978874356580E-001f);
  u = __internal_fmad (u, z, 4.99999940395997040E-001f);
  u = u * z;
  u = __internal_fmad (u, z, z);
  /* 2^j*[exmp1(z)+1]-1 = 2^j*expm1(z)+2^j-1 */
  z = exp2f (j);
  u = __internal_fmad (u, z, z - 1.0f);
  if (t == 128.0f) u = u + u; /* work around intermediate overflow */
  /* handle massive overflow and underflow */
  if (j > 128.0f) u = CUDART_INF_F;
  if (j < -25.0f) u = -1.0f;
  /* take care of denormal and zero inputs */
  if (a == 0.0f) {
    u = a + a;
  }
  return u;
}

static __forceinline__ float hypotf(float a, float b)
{
  float v, w, s, t, fa, fb;

  fa = fabsf(a);
  fb = fabsf(b);
  v = s = fmaxf(fa, fb);
  w = t = fminf(fa, fb);
  if (v > CUDART_TWO_TO_126_F) {
    s = s * 0.25f;
    t = t * 0.25f;
  }
  t = __fdividef(t, s);
  t = __internal_fmad (t, t, 1.0f);
  t = v * sqrtf(t);
  if (v == 0.0f) {
    t = v + w;         /* fixup for zero divide */
  }
  if ((!(fa <= CUDART_INF_F)) || (!(fb <= CUDART_INF_F))) {
    t = a + b;         /* fixup for NaNs */
  }
  if (v == CUDART_INF_F) {
    t = v + w;         /* fixup for infinities */
  }
  return t;
}

static __forceinline__ float rhypotf(float a, float b) 
{
  float v, w, s, t, fa, fb, rv;
  fa = fabsf(a);
  fb = fabsf(b);
  v = s = fmaxf(fa, fb);
  w = t = fminf(fa, fb);
  if (v > CUDART_TWO_TO_126_F) {
    s = s * 0.25f;
    t = t * 0.25f;
  }
  t = __fdividef(t, s);
  t = __internal_fmad(t, t, 1.0f);
  
#if defined(__CUDA_FTZ)
  rv = 1.0f / v;
  t = rv * __internal_fast_rsqrtf(t);
#else
  v = (s < 1.0f) ? 2.0f * v : v; /* Scale for denormals */
  rv = 1.0f / v;
  t = rv * __internal_fast_rsqrtf(t);
  t = (s < 1.0f) ? 2.0f * t : t; /* Scale back */
#endif
  if (v == 0.0f) t = CUDART_INF_F; /* fixup for both zero */
  if ((!(fa <= CUDART_INF_F)) || (!(fb <= CUDART_INF_F))) {
    t = a + b; /* fixup for NaNs */
  }
  if (v == CUDART_INF_F) t = 0.0f; /* fixup for infinities */
  return t;
}

static __forceinline__ float cbrtf(float a)
{
  float s, t;

  s = fabsf (a);
  t = __internal_fast_exp2f (CUDART_THIRD_F * __log2f(s)); /* initial approx */
  s = __internal_fmad (__internal_fast_rcpf (t * t), -s, t);
  t = __internal_fmad (s, -CUDART_THIRD_F, t);
  if (__float_as_int (a) < 0) {
    t = -t;
  }
  s = a + a;
  if (s == a) t = s; /* handle special cases */
  return t;
}

static __forceinline__ float rcbrtf(float a)
{
  float s, t;

  s = fabsf (a);
  t = __internal_fast_exp2f (-CUDART_THIRD_F * __log2f(s));/* initial approx */
  t = __internal_fmad (__internal_fmad (t*t, -s*t, 1.0f), CUDART_THIRD_F*t, t);
  if (__float_as_int (a) < 0) {
    t = -t;
  }
  s = a + a;
  if (s == a) t = __internal_fast_rcpf (a); /* handle special cases */
  return t;
}

static __forceinline__ float j0f(float a)
{
  float t, r, x;
  r = 0.0f;
  t = fabsf(a);
  if (t <= 8.0f) {
    /* The approximation for |a| <= 8 is based on an expansion around
     * the first zero, and dividing out the first three zeros.
     * The zero positions are represented in float-float format to
     * maintain accuracy near the zeros.
     */
    x = ((t - 2.4048254e0f) - 1.0870590e-7f);
    r = -1.2470738e-15f;
    r = __internal_fmad(r, x, -7.6677725e-14f);
    r = __internal_fmad(r, x, 2.7150556e-12f);
    r = __internal_fmad(r, x, -6.0280119e-12f);
    r = __internal_fmad(r, x, -4.2393267e-10f);
    r = __internal_fmad(r, x, 5.8276378e-10f);
    r = __internal_fmad(r, x, 5.8077841e-8f);
    r = __internal_fmad(r, x, 1.8003311e-9f);
    r = __internal_fmad(r, x, -5.4500733e-6f);
    r = __internal_fmad(r, x, -7.3432207e-6f);
    r = __internal_fmad(r, x, 3.0170320e-4f);
    r = __internal_fmad(r, x, 7.7395444e-4f);
    r = __internal_fmad(r, x, -7.2834617e-3f);
    r = __internal_fmad(r, x, -2.6668204e-2f);
    r *= x;
    r *= ((t - 5.5200782e0f) - -7.1934146e-8f);
    r *= ((t - 8.6537275e0f) - 3.8147792e-7f);
  } else if (!__isinff(t)) {
    /* The approximation for |a| > 8 is inspired by:
     * John Harrison, "Fast and Accurate Bessel Function Computation",
     * ARITH 2009.
     */
    float y = __internal_fast_rcpf(t);
    float y2 = y * y;
    float f, arg;
    f = 3.3592878957727e0f;
    f = __internal_fmad(f, y2, -5.1452267169952e-1f);
    f = __internal_fmad(f, y2, 1.0337056964636e-1f);
    f = __internal_fmad(f, y2, -6.2499724328518e-2f);
    f = __internal_fmad(f, y2, 1.0000000000000e0f);
    arg = 1.1396494694586e0f;
    arg = __internal_fmad(arg, y2, -2.0532675087452e-1f);
    arg = __internal_fmad(arg, y2, 6.5091736614704e-2f);
    arg = __internal_fmad(arg, y2, -1.2499999254942e-1f);
    arg = __internal_fmad(arg, y, t);
    r = rsqrtf(t) * CUDART_SQRT_2_OVER_PI_F * f * 
        __internal_cos_offset_f(arg, -7.8539816e-1f);
  } else {
    /* Input is infinite. */
    r = 0.0f;
  }
  return r;
}

static __forceinline__ float j1f(float a)
{
  float t, r, x;
  r = 0.0f;
  t = fabsf(a);
  if (t <= 7.85f) {
    /* The approximation for |a| <= 8 is based on an expansion around
     * the first zero, and dividing out the first three zeros.
     * The zero positions are represented in float-float format to
     * maintain accuracy near the zeros.
     */
    x = ((t - 3.8317060e0f) - -7.6850590e-8f);
    r = 7.7806488e-14f;
    r = __internal_fmad(r, x, 9.2190860e-13f);
    r = __internal_fmad(r, x, -2.5706883e-11f);
    r = __internal_fmad(r, x, -2.0179057e-10f);
    r = __internal_fmad(r, x, 4.5125277e-9f);
    r = __internal_fmad(r, x, 2.7016290e-8f);
    r = __internal_fmad(r, x, -5.3477970e-7f);
    r = __internal_fmad(r, x, -2.3602763e-6f);
    r = __internal_fmad(r, x, 4.1210809e-5f);
    r = __internal_fmad(r, x, 1.1917029e-4f);
    r = __internal_fmad(r, x, -1.8075588e-3f);
    r = __internal_fmad(r, x, -2.5548928e-3f);
    r = __internal_fmad(r, x, 3.3013891e-2f);
    r *= ((t - 7.0155869e0f) - -1.8321172e-7f);
    r *= x;
    r *= t;
  } else if (!__isinff(t)) {
    float y = __internal_fast_rcpf(t);
    float y2 = y * y;
    float f, arg;
    f = -4.0873065e0f;
    f = __internal_fmad(f, y2, 7.4987656e-1f);
    f = __internal_fmad(f, y2, -1.9291565e-1f);
    f = __internal_fmad(f, y2, 1.8749826e-1f);
    f = __internal_fmad(f, y2, 1.0000000e0f);
    arg = -1.5799448e0f;
    arg = __internal_fmad(arg, y2, 3.6148587e-1f);
    arg = __internal_fmad(arg, y2, -1.6401261e-1f);
    arg = __internal_fmad(arg, y2, 3.7499991e-1f);
    arg = __internal_fmad(arg, y, t);
    r = rsqrtf(t) * CUDART_SQRT_2_OVER_PI_F * f * 
        __internal_cos_offset_f(arg, -2.3561945e0f);
  } else {
    r = 0.0f;
  }
  if (a < 0.0f) {
    r = -r;
  }
  if (t < 1e-30f) {
    r = copysignf(r, a);
  }
  return r;
}

static __forceinline__ float y0f(float a)
{
  float t, r, x;
  r = 0.0f;
  t = fabsf(a);
  if (t <= 4.4678848e-1f) {
    /* For |a| < .45 we use a polynomial in a^2 to approximate
       y0(a) - 2/pi * j0f(a) * log(a)
       This allows good relative accuracy near the origin.
       The method follows Hart et al. "Computer Approximations",
       Wiley, 1968, p. 145, eqn. 6.8.27.
    */
    x = t * t;
    r = 1.0239759e-7f;
    r = __internal_fmad(r, x, -9.4943007e-6f);
    r = __internal_fmad(r, x, 5.3860247e-4f);
    r = __internal_fmad(r, x, -1.6073968e-2f);
    r = __internal_fmad(r, x, 1.7760602e-1f);
    r = __internal_fmad(r, x, -7.3804297e-2f);
    r += CUDART_2_OVER_PI_F * __internal_accurate_logf(t) * j0f(t);
  } else if (t <= 1.9256277e0f) {
    /* For .45 < |a| < 8.6 we use polynomials around the
       zeros to get good relative accuracy near the zeros.
    */
    x = ((t - 8.9357698e-1f) - -1.3357979e-8f);
    r = 2.7073240e-2f;
    r = __internal_fmad(r, x, -1.2728614e-1f);
    r = __internal_fmad(r, x, 2.4919428e-1f);
    r = __internal_fmad(r, x, -2.7333531e-1f);
    r = __internal_fmad(r, x, 2.1465521e-1f);
    r = __internal_fmad(r, x, -1.8043898e-1f);
    r = __internal_fmad(r, x, 1.8990822e-1f);
    r = __internal_fmad(r, x, -2.0543173e-1f);
    r = __internal_fmad(r, x, 2.1968170e-1f);
    r = __internal_fmad(r, x, -2.2614010e-1f);
    r = __internal_fmad(r, x, 2.2052875e-1f);
    r = __internal_fmad(r, x, -4.9207821e-1f);
    r = __internal_fmad(r, x, 8.7942094e-1f);
    r *= x;
  } else if (t <= 5.5218647e0f) {
    x = ((t - 3.9576783e0f) - 1.0129118e-7f);
    r = -1.8800073e-7f;
    r = __internal_fmad(r, x, -2.0689195e-7f);
    r = __internal_fmad(r, x, -3.9767929e-6f);
    r = __internal_fmad(r, x, 5.0848408e-5f);
    r = __internal_fmad(r, x, 1.9341275e-4f);
    r = __internal_fmad(r, x, -2.1835594e-3f);
    r = __internal_fmad(r, x, -6.8510985e-3f);
    r = __internal_fmad(r, x, 5.8523852e-2f);
    r = __internal_fmad(r, x, 5.0855581e-2f);
    r = __internal_fmad(r, x, -4.0254268e-1f);
    r *= x;
  } else if (t <= 8.6541981e0f) {
    x = ((t - 7.0860510e0f) - 7.3058118e-8f);
    r = 5.3945030e-7f;
    r = __internal_fmad(r, x, 2.5310101e-6f);
    r = __internal_fmad(r, x, -4.5855297e-5f);
    r = __internal_fmad(r, x, -1.4028202e-4f);
    r = __internal_fmad(r, x, 2.1758752e-3f);
    r = __internal_fmad(r, x, 3.3181210e-3f);
    r = __internal_fmad(r, x, -4.8024025e-2f);
    r = __internal_fmad(r, x, -2.1175193e-2f);
    r = __internal_fmad(r, x, 3.0009761e-1f);
    r *= x;
  } else if (!__isinff(t)) {
    float y = __internal_fast_rcpf(t);
    float y2 = y * y;
    float f, arg;
    f = -3.9924583e-1f;
    f = __internal_fmad(f, y2, 1.0197055e-1f);
    f = __internal_fmad(f, y2, -6.2492687e-2f);
    f = __internal_fmad(f, y2, 1.0000000e0f);
    arg = 1.1000177e0f;
    arg = __internal_fmad(arg, y2, -2.0393032e-1f);
    arg = __internal_fmad(arg, y2, 6.5077804e-2f);
    arg = __internal_fmad(arg, y2, -1.2499996e-1f);
    arg = __internal_fmad(arg, y, t);
    r = rsqrtf(t) * CUDART_SQRT_2_OVER_PI_F * f * 
        __internal_cos_offset_f(arg, -2.3561945e0f);
  } else {
    /* Input is infinite. */
    r = 0.0f;
  }
  if (a < 0.0f) {
    r = sqrtf(-1.0f);
  }
  return r;
}

static __forceinline__ float y1f(float a)
{
  float t, r, x;
  r = 0.0f;
  t = fabsf(a);
  if (t < CUDART_TWO_TO_M126_F) {
    /* Denormalized inputs need care to avoid overflow on 1/t */
    r = -CUDART_2_OVER_PI_F / t;
  } else if (t <= 1.6985707e0f) {
    /* For |a| < 1.7 we use a polynomial of the form a * P(a^2) 
       to approximate y1(a) - 2/pi * (j1f(a) * log(a) - 1/a)
       This allows good relative accuracy near the origin.
       The method follows Hart et al. "Computer Approximations",
       Wiley, 1968, p. 145, eqn. 6.8.28.
    */
    x = t * t;
    r = 8.6371976e-9f;
    r = __internal_fmad(r, x, -9.9208705e-7f);
    r = __internal_fmad(r, x, 7.1642142e-5f);
    r = __internal_fmad(r, x, -2.9553052e-3f);
    r = __internal_fmad(r, x, 5.4348689e-2f);
    r = __internal_fmad(r, x, -1.9605710e-1f);
    r *= t;
    r += CUDART_2_OVER_PI_F * (__internal_accurate_logf(t) * j1f(t) - 1.0f /t);
  } else if (t <= 3.8134112e0f) {
    /* For 1.7 < |a| < 10.2 we use polynomials around the
       zeros to get good relative accuracy near the zeros.
    */
    x = ((t - 2.1971414e0f) - -8.2889272e-8f);
    r = -1.6437198e-5f;
    r = __internal_fmad(r, x, 1.2807001e-4f);
    r = __internal_fmad(r, x, -4.5903810e-4f);
    r = __internal_fmad(r, x, 1.0938945e-3f);
    r = __internal_fmad(r, x, -2.6312035e-3f);
    r = __internal_fmad(r, x, 7.4269730e-3f);
    r = __internal_fmad(r, x, -4.7923904e-3f);
    r = __internal_fmad(r, x, -3.2858409e-2f);
    r = __internal_fmad(r, x, -1.1851477e-1f);
    r = __internal_fmad(r, x, 5.2078640e-1f);
    r *= x;
  } else if (t <= 7.0128435e0f) {
    x = ((t - 5.4296808e0f) - 2.1651435e-7f);
    r = 2.2158844e-8f;
    r = __internal_fmad(r, x, -5.0902725e-7f);
    r = __internal_fmad(r, x, -2.8541590e-6f);
    r = __internal_fmad(r, x, 4.6370558e-5f);
    r = __internal_fmad(r, x, 1.4660921e-4f);
    r = __internal_fmad(r, x, -2.1659129e-3f);
    r = __internal_fmad(r, x, -4.1601104e-3f);
    r = __internal_fmad(r, x, 5.0947908e-2f);
    r = __internal_fmad(r, x, 3.1338677e-2f);
    r = __internal_fmad(r, x, -3.4031805e-1f);
    r *= x;
  } else if (t <= 1.0172580e1f) {
    x = ((t - 8.5960054e0f) - 4.2857286e-7f);
    r = 5.2508420e-7f;
    r = __internal_fmad(r, x, 1.9455433e-6f);
    r = __internal_fmad(r, x, -4.3719487e-5f);
    r = __internal_fmad(r, x, -1.0394055e-4f);
    r = __internal_fmad(r, x, 2.0113946e-3f);
    r = __internal_fmad(r, x, 2.4177686e-3f);
    r = __internal_fmad(r, x, -4.3406386e-2f);
    r = __internal_fmad(r, x, -1.5789848e-2f);
    r = __internal_fmad(r, x, 2.7145988e-1f);
    r *= x;
  } else if (!__isinff(t)) {
    float y = __internal_fast_rcpf(t);
    float y2 = y * y;
    float f, arg;
    f = 6.5038110e-1f;
    f = __internal_fmad(f, y2, -1.9206071e-1f);
    f = __internal_fmad(f, y2, 1.8749522e-1f);
    f = __internal_fmad(f, y2, 1.0000000e0f);
    arg = -1.7881368e0f;
    arg = __internal_fmad(arg, y2, 3.6611685e-1f);
    arg = __internal_fmad(arg, y2, -1.6404507e-1f);
    arg = __internal_fmad(arg, y2, 3.7499997e-1f);
    arg = __internal_fmad(arg, y, t);
    r = rsqrtf(t) * CUDART_SQRT_2_OVER_PI_F * f * 
        __internal_cos_offset_f(arg, -3.9269908e0f);
  } else {
    /* Input is infinite. */
    r = 0.0f;
  }
  if (a < 0.0f) {
    r = sqrtf(-1.0f);
  }
  return r;
}

static __forceinline__ float ynf(int n, float a)
{
  float yip1; // is Y(i+1, a)
  float yi = y1f(a); // is Y(i, a)
  float yim1 = y0f(a); // is Y(i-1, a)
  float two_over_a;
  int i;
  if(n == 0) {
    return y0f(a);
  }
  if(n == 1) {
    return y1f(a);
  }
  if(n < 0) {
    return CUDART_NAN_F;
  }
  if(!(a >= 0.0f)) {
    // also catches NAN input
    return CUDART_NAN_F;
  }
  if(fabsf(a) < CUDART_TWO_TO_M126_F) {
    // Denormalized inputs
    return -10e8f / fabsf(a);
  }
  two_over_a = 2.0f / a;
  for(i = 1; i < n; i++) {
    // Use forward recurrence
    yip1 = __internal_fmad(i * two_over_a, yi, -yim1);
    yim1 = yi;
    yi = yip1;
  }
  if(__isnanf(yip1)) {
    // We got overflow in forward recurrence
    return -CUDART_INF_F;
  }
  return yip1;
}

static __forceinline__ float jnf(int n, float a)
{
  float jip1 = 0.0f; // is J(i+1, a)
  float ji = 1.0f; // is J(i, a)
  float jim1; // is J(i-1, a)
  float lambda = 0.0f;
  float sum = 0.0f;
  int i;
  if(n == 0) {
    return j0f(a);
  }
  if(n == 1) {
    return j1f(a);
  }
  if(n < 0) {
    return CUDART_NAN_F;
  }
  if(fabsf(a) > (float)(n + (n > 4))) {
    // Use forward recurrence, numerically stable for large x
    float two_over_a = 2.0f / a;
    float ji = j1f(a); // is J(i, a)
    float jim1 = j0f(a); // is J(i-1, a)
    for(i = 1; i < n; i++) {
      jip1 = __internal_fmad(i * ji, two_over_a, -jim1);
      jim1 = ji;
      ji = jip1;
    }
    return jip1;
  } else {
    int m = n + (int)sqrtf(n * 40);
    m = (m >> 1) << 1;
    for(i = m; i >= 1; --i) {
      // Use backward recurrence
      jim1 = i * 2.0f / a * ji - jip1;
      jip1 = ji;
      if(fabsf(jim1) > 1e15f) {
        // Rescale to avoid intermediate overflow
        jim1 *= 1e-15f;
        jip1 *= 1e-15f;
        lambda *= 1e-15f;
        sum *= 1e-15f;
      }
      ji = jim1;
      if(i - 1 == n) {
        lambda = ji;
      }
      if(i & 1) {
        sum += 2.0f * ji;
      }
    }
    sum -= ji;
    return lambda / sum;
  }
}


static __forceinline__ float cyl_bessel_i0f (float a)
{
  float p, q;

  a = fabsf (a);
  if ((a >= 9.0f) && (a < __int_as_float(0x7f800000))) {
    q = __internal_fast_rcpf (a);
    p =                        3.487216849e-01f;
    p = __internal_fmad (p, q,-5.456334531e-03f);
    p = __internal_fmad (p, q, 3.334715532e-02f);
    p = __internal_fmad (p, q, 2.788919520e-02f);
    p = __internal_fmad (p, q, 4.987062961e-02f);
    p = __internal_fmad (p, q, 3.989422631e-01f);
    p = p * __internal_fast_rsqrtf (a);
    q = expm1f (0.5f * a);               /* prevent premature overflow */
    p = __fadd_rn (__fmul_rn (__fmul_rn (p, q), __fadd_rn (q, 2.0f)), p);
  } else {
    q = a * a;
#if __CUDA_ARCH__ >= 200
    p =                        1.551427019e-19f;
    p = __internal_fmad (p, q, 1.449250594e-17f);
    p = __internal_fmad (p, q, 1.068764625e-14f);
    p = __internal_fmad (p, q, 2.334957438e-12f);
    p = __internal_fmad (p, q, 4.730662512e-10f);
    p = __internal_fmad (p, q, 6.777800411e-08f);
    p = __internal_fmad (p, q, 6.782078392e-06f);
    p = __internal_fmad (p, q, 4.340258310e-04f);
    p = __internal_fmad (p, q, 1.562500354e-02f);
    p = __internal_fmad (p, q, 2.499999990e-01f);
    p = __internal_fmad (p, q, 1.000000000e+00f);
#else  /* __CUDA_ARCH__ >= 200 */
    /* coefficients adjusted to compensate for truncating multiply in FMAD */
    p =                        1.551427516e-19f;
    p = __internal_fmad (p, q, 1.449251204e-17f);
    p = __internal_fmad (p, q, 1.068764997e-14f);
    p = __internal_fmad (p, q, 2.334958182e-12f);
    p = __internal_fmad (p, q, 4.730664194e-10f);
    p = __internal_fmad (p, q, 6.777801786e-08f);
    p = __internal_fmad (p, q, 6.782079254e-06f);
    p = __internal_fmad (p, q, 4.340258602e-04f);
    p = __internal_fmad (p, q, 1.562500559e-02f);
    p = __internal_fmad (p, q, 2.500000000e-01f);
    p = __internal_fmad (p, q, 1.000000000e-00f);
#endif /* __CUDA_ARCH__ >= 200 */
  }
  return p;
}


static __forceinline__ float cyl_bessel_i1f (float a)
{
  float p, q, t;

  t = fabsf (a);
  if ((t >= 8.085f) && (t < __int_as_float(0x7f800000))) {
    q = __internal_fast_rcpf (t);
    p =                        -5.02881298e-1f;
    p = __internal_fmad (p, q,  2.84715557e-2f);
    p = __internal_fmad (p, q, -4.87367091e-2f);
    p = __internal_fmad (p, q, -4.64159570e-2f);
    p = __internal_fmad (p, q, -1.49609730e-1f);
    p = __internal_fmad (p, q,  3.98942322e-1f);
    p = p * __internal_fast_rsqrtf (t);
    q = expm1f (0.5f * t);               /* prevent premature overflow */
    p = __fadd_rn (__fmul_rn (__fmul_rn (p, q), __fadd_rn (q, 2.0f)), p);
    p = __internal_copysignf_pos (p, a);
  } else {
    q = a * a;
#if __CUDA_ARCH__ >= 200
    p =                         2.78482528e-18f;
    p = __internal_fmad (p, q,  3.42247083e-16f);
    p = __internal_fmad (p, q,  1.62580021e-13f);
    p = __internal_fmad (p, q,  3.31421728e-11f);
    p = __internal_fmad (p, q,  5.66327350e-09f);
    p = __internal_fmad (p, q,  6.78002705e-07f);
    p = __internal_fmad (p, q,  5.42547398e-05f);
    p = __internal_fmad (p, q,  2.60416201e-03f);
    p = __internal_fmad (p, q,  6.25000062e-02f);
#else  /* __CUDA_ARCH__ >= 200 */
    /* coefficients adjusted to compensate for truncating multiply in FMAD */
    p =                         2.78482637e-18f;
    p = __internal_fmad (p, q,  3.42247209e-16f);
    p = __internal_fmad (p, q,  1.62580073e-13f);
    p = __internal_fmad (p, q,  3.31421869e-11f);
    p = __internal_fmad (p, q,  5.66327474e-09f);
    p = __internal_fmad (p, q,  6.78002834e-07f);
    p = __internal_fmad (p, q,  5.42547459e-05f);
    p = __internal_fmad (p, q,  2.60416232e-03f);
    p = __internal_fmad (p, q,  6.25000062e-02f);
#endif /* __CUDA_ARCH__ >= 200 */
    p = p * q;
    q = 0.5f * a;
    p = __internal_fmad (a, p, q);
  }
  return p;
}


static __forceinline__ float erff(float a)
{
  float t, r;

  t = fabsf(a);
  if (t >= 1.0f) { 
    r =                        -2.1838108096813042E-005f;
    r = __internal_fmad (r, t,  5.0251842396186700E-004f);
    r = __internal_fmad (r, t, -5.2685376878506664E-003f);
    r = __internal_fmad (r, t,  3.3802289909563604E-002f);
    r = __internal_fmad (r, t, -1.5159399705325438E-001f);
    r = __internal_fmad (r, t, -9.1882860913602138E-001f);
    r = __internal_fmad (r, t, -1.6265084770219975E+000f);
    r = __internal_fmad (r, t, -5.0015456083261922E-004f);
    r = 1.0f - __internal_fast_exp2f (r);
    if (t >= 3.919205904f) {
      r = 1.0f;
    }
    a = __internal_copysignf_pos (r, a);
  } else {
    t = a * a;
    r =                        -5.58510127926029810E-004f;
    r = __internal_fmad (r, t,  4.90688891415893070E-003f);
    r = __internal_fmad (r, t, -2.67027980930150640E-002f);
    r = __internal_fmad (r, t,  1.12799056505903940E-001f);
    r = __internal_fmad (r, t, -3.76122956138427440E-001f);
    r = __internal_fmad (r, t,  1.12837911712623450E+000f);
    a = a * r;
  }
  return a;
}

static __forceinline__ float __internal_erfinv_poly(float a)
{
  float r;

  r =                        -2.5172707606685652E-010f; 
  r = __internal_fmad (r, a,  9.4274289432374619E-009f); 
  r = __internal_fmad (r, a, -1.2054753059594516E-007f); 
  r = __internal_fmad (r, a,  2.1697004698667657E-007f); 
  r = __internal_fmad (r, a,  8.0621488510822390E-006f); 
  r = __internal_fmad (r, a, -3.1675491789646913E-005f); 
  r = __internal_fmad (r, a, -7.7436312951712784E-004f); 
  r = __internal_fmad (r, a,  5.5465877941375773E-003f); 
  r = __internal_fmad (r, a,  1.6082022430967846E-001f); 
  r = __internal_fmad (r, a,  8.8622690039402130E-001f); 
  return r;
}

/* 
 * This implementation of erfinvf() adopts algorithmic techniques from a 
 * variety of sources. The transformation of the argument into log space 
 * follows Strecok [1], the transformation of the argument into reciprocal 
 * square root space for arguments near unity follows Blair et. al. [2], 
 * and the aggressive widening of the primary approximation interval around 
 * zero to minimize divergence as well as the use of a CUDA device function 
 * for the computation of the logarithm follows work by professor Mike Giles 
 * (Oxford University, UK). 
 * 
 * [1] Anthony J. Strecok, On the Calculation of the Inverse of the Error 
 * Function. Mathematics of Computation, Vol. 22, No. 101 (Jan. 1968), 
 * pp. 144-158 
 * [2] J. M. Blair, C. A. Edwards and J. H. Johnson, Rational Chebyshev 
 * Approximations for the Inverse of the Error Function. Mathematics 
 * of Computation, Vol. 30, No. 136 (Oct. 1976), pp. 827-830 
 */ 
static __forceinline__ float erfinvf(float a)
{
  float s, t, r;

#if __CUDA_ARCH__ >= 200
  s = __fmaf_rn (a, -a, 1.0f);
#else
  s = 1.0f + a;
  t = 1.0f - a;
  s = s * t;
#endif
  t = - __internal_fast_log2f (s);
  if (t > 8.2f) {
    t = __internal_fast_rsqrtf (t);
    r =                        -5.8991436871681446E-001f;
    r = __internal_fmad (r, t, -6.6300422537735360E-001f);
    r = __internal_fmad (r, t,  1.5970110948817704E+000f);
    r = __internal_fmad (r, t, -6.7521557026467416E-001f);
    r = __internal_fmad (r, t, -9.5224791160399669E-002f);
    r = __internal_fmad (r, t,  8.3535343797791939E-001f);
    t = 1.0f / t;
    t = r * t;
    if (a < 0.0f) t = -t;
  } else {
    t = a * __internal_erfinv_poly (t);
  }
  return t;
}

static __forceinline__ float __internal_erfcxf_kernel (float a)
{
#if __CUDA_ARCH__ >= 200
  /*  
   * The implementation of erfcx() is based on the algorithm in: M. M. Shepherd
   * and J. G. Laframboise, "Chebyshev Approximation of (1 + 2x)exp(x^2)erfc x
   * in 0 <= x < INF", Mathematics of Computation, Vol. 36, No. 153, January
   * 1981, pp. 249-253. For the core approximation, the input domain [0,INF] is
   * transformed via (x-k) / (x+k) where k is a precision-dependent constant.
   * Here, we choose k = 4.0, so input domain [0,10.0542] is transformed to the
   * core approximation domain [-1,0.430775].   
   */  
  float t1, t2, t3, t4;  
  /* (1+2*x)*exp(x*x)*erfc(x) */ 
  /* t2 = (x-4.0)/(x+4.0), transforming [0,INF] to [-1,+1] */ 
  t1 = a - 4.0f; 
  t2 = a + 4.0f; 
  t2 = __internal_fast_rcpf (t2);
  t3 = __fmul_rn (t1, t2); 
  t1 = __fmaf_rn (-4.0f, t3 + 1.0f, a); 
  t1 = __fmaf_rn (-t3, a, t1); 
  t2 = __fmaf_rn (t2, t1, t3); 
  /* approximate on [-1, 0.43077] */   
  t1 =                     8.9121708796596137E-004f;
  t1 = __fmaf_rn (t1, t2,  7.0457882096080539E-003f);
  t1 = __fmaf_rn (t1, t2, -1.5866896179569826E-002f);
  t1 = __fmaf_rn (t1, t2,  3.6429623548273796E-002f);
  t1 = __fmaf_rn (t1, t2, -6.6643431826764105E-002f);
  t1 = __fmaf_rn (t1, t2,  9.3814531408377352E-002f);
  t1 = __fmaf_rn (t1, t2, -1.0099056031741439E-001f);
  t1 = __fmaf_rn (t1, t2,  6.8094000865639340E-002f);
  t1 = __fmaf_rn (t1, t2,  1.5377387724225955E-002f);
  t1 = __fmaf_rn (t1, t2, -1.3962107929750070E-001f);
  t1 = __fmaf_rn (t1, t2,  1.2329951349050698E+000f);
  /* (1+2*x)*exp(x*x)*erfc(x) / (1+2*x) = exp(x*x)*erfc(x) */  
  t2 = __fmaf_rn (2.0f, a, 1.0f);  
  t2 = __internal_fast_rcpf (t2);
  t3 = __fmul_rn (t1, t2); 
  t4 = __fmaf_rn (a, -2.0f*t3, t1); 
  t4 = t4 - t3; 
  t1 = __fmaf_rn (t4, t2, t3); 
  return t1;
#else /* __CUDA_ARCH__ >= 200 */
  float p, q;
  /* On the interval [0.813, 10.055] the following approximation is used:
     erfc(a) = (1.0 + 1/a * r(1/a)) * 1/a * 0.5 * exp(-a*a), where the range 
     of r(1/a) is approximately [-0.12, 0.27]. r(1/a) is computed by rational
     approximation. Problematic is the accurate computation of exp(-a*a).
     Despite the accuracy-enhancing techniques being used, this computation
     adds almost 3 ulps to the total error for compute capability < sm_20.
     IEEE-rounded divisions don't help improve the accuracy of the rational
     approximation, so force approximate operations for best performance.
  */
  p =                        - 9.9910025099425892E-001f;
  p = __internal_fmad (p, a, - 2.6108451215634448E-001f);
  p = __internal_fmad (p, a, + 1.2460347155371844E-001f);
  p = __internal_fmad (p, a, + 1.3243909814778765E-001f);
  p = __internal_fmad (p, a, + 3.3477599162629441E-002f);
  q =                     a  + 2.2542670016875404E+000f;
  q = __internal_fmad (q, a, + 2.1514433559696009E+000f);
  q = __internal_fmad (q, a, + 1.0284106806050302E+000f);
  q = __internal_fmad (q, a, + 2.6094298761636442E-001f);
  q = __internal_fmad (q, a, - 3.9951461024063317E-006f);
  p = __fdividef(p, q);
  return p;
#endif /* __CUDA_ARCH__ >= 200 */
}

static __forceinline__ float erfcf(float a)
{
#if __CUDA_ARCH__ >= 200
  float x, t1, t2, t3;
  x = fabsf(a); 
  t1 = __internal_erfcxf_kernel (x);
  /* exp(-x*x) * exp(x*x)*erfc(x) = erfc(x) */  
  t2 = -x * x;  
  t3 = __internal_expf_kernel (t2, 0.0f);  
  t2 = __fmaf_rn (-x, x, -t2);  
  t2 = __fmaf_rn (t3, t2, t3);  
  t1 = t1 * t2;  
  if (x > 10.055f) t1 = 0.0f;  
  return (a < 0.0f) ? (2.0f - t1) : t1; 
#else /* __CUDA_ARCH__ >= 200 */
  float p, q, h, l, t;
  if (a <= 0.813f) {
    p = 1.0f - erff(a);
  } else {
    t = __internal_fast_rcpf (a);
    p = __internal_erfcxf_kernel (t);
    h = __int_as_float (__float_as_int (a) & 0xfffff000);
    l = a - h;
    q = -h * h;
    q = __internal_expf_kernel (q, 0.0f);
    h = a + h;
    l = l * h;
    h = __expf (-l);
    q = 0.5f * h * q;
    p = p * t;
    p = __internal_fmad (p, t, t);
    p = p * q;
    if (a > 10.055f) {
      p = 0.0f;
    }
  }
  return p;
#endif /* __CUDA_ARCH__ >= 200 */
}

static __forceinline__ float erfcxf(float a)
{
#if __CUDA_ARCH__ >= 200
  float x, t1, t2, t3;
  x = fabsf(a); 
  if (x < 10.055f) {
    t1 =  __internal_erfcxf_kernel(x);
  } else {
    /* asymptotic expansion for large aguments */
    t2 = __fdividef (0.25f, 0.25f * x);
    t3 = t2 * t2;
    t1 =                    +6.5625f;
    t1 = __fmaf_rn (t1, t3, -1.875f);
    t1 = __fmaf_rn (t1, t3, +0.75f);
    t1 = __fmaf_rn (t1, t3, -0.5f);
    t1 = __fmaf_rn (t1, t3, +1.0f);
    t2 = t2 * 5.6418958354775628e-001f;
    t1 = t1 * t2;
  }
  if (a < 0.0f) {
    /* erfcx(x) = 2*exp(x^2) - erfcx(|x|) */
    t2 = __fmul_rz (x, x);  /* prevent premature overflow */
    t3 = __fmaf_rn (x, x, -t2);
    t2 = __internal_accurate_expf(t2);
    t2 = t2 + t2;
    t3 = __fmaf_rn (t2, t3, t2);
    t1 = t3 - t1;
    if (t2 == __int_as_float(0x7f800000)) t1 = t2;
  }
  return t1;
#else
  float x, t1, t2, t3, t4;
  x = fabsf(a); 
  if (x < 10.055f) {
    if (x <= 0.813f) {
      t1 =                          1.0561599769184452E-002f;
      t1 = __internal_fmad (t1, x, -5.4788623043555737E-002f);
      t1 = __internal_fmad (t1, x,  1.4788597280302476E-001f);
      t1 = __internal_fmad (t1, x, -2.9421924559249779E-001f);
      t1 = __internal_fmad (t1, x,  4.9866246647734674E-001f);
      t1 = __internal_fmad (t1, x, -7.5211812529808164E-001f);
      t1 = __internal_fmad (t1, x,  9.9999473193293242E-001f);
      t1 = __internal_fmad (t1, x, -1.1283791325606334E+000f);
      t1 = __internal_fmad (t1, x,  1.0f);
    } else {
      t2 = __internal_fast_rcpf (x);
      t3 = __internal_erfcxf_kernel (t2);
      t3 = __internal_fmad (t3, t2, 1.0f);
      t1 = t3 * t2 * 0.5f;
    }
  } else {
    /* asymptotic expansion for large aguments */
    t2 = __internal_fast_rcpf (x);
    t3 = t2 * t2;
    t1 =                          +6.5625f;
    t1 = __internal_fmad (t1, t3, -1.875f);
    t1 = __internal_fmad (t1, t3, +0.75f);
    t1 = __internal_fmad (t1, t3, -0.5f);
    t1 = __internal_fmad (t1, t3, +1.0f);
    t2 = t2 * 5.6418958354775628e-001f;
    t1 = t1 * t2;
  }
  if (a <= 0.0f) {
    /* erfcx(x) = 2*exp(x^2) - erfcx(|x|) */
    t2 = __int_as_float(__float_as_int(x) & 0xfffff000);
    t3 = x - t2;
    t4 = t2 * t2;
    t4 = __internal_accurate_expf(t4);
    t2 = x + t2;
    t3 = t3 * t2;
    t2 = __expf(t3);
    t2 = __fmul_rn (t4, t2);
    t3 = t2 + t2;
    t1 = t3 - t1;
    if (t4 == __int_as_float(0x7f800000)) t1 = t4;
  }
  return t1;
#endif
}

static __forceinline__ float erfcinvf(float a)
{
  float t = __fadd_rn (2.0f, -a);
  if ((a >= 0.0034f) && (a <= 1.9966f)) {
    t = __fmul_rn (t, a);
    t = __internal_fast_log2f (t);
    t = __internal_erfinv_poly (-t);
#if __CUDA_ARCH__ >= 200
    t = __fmaf_rn (t, -a, t);
#else
    t = __fmul_rn (__fadd_rn (1.0f, -a), t);
#endif
  } else {
    float p = a;
    if (a > 1.0f) p = t;   
    t = __log2f (p);
    t = __internal_fast_rsqrtf (-t);  
    p =                        -6.3113223322530750E+1f;
    p = __internal_fmad (p, t,  1.2748468750377106E+2f);
    p = __internal_fmad (p, t, -1.1410568387522068E+2f);
    p = __internal_fmad (p, t,  6.0325788262642078E+1f);
    p = __internal_fmad (p, t, -2.1789891427552199E+1f);
    p = __internal_fmad (p, t,  6.4674089804347199E+0f);
    p = __internal_fmad (p, t, -1.8329473786019568E+0f);
    p = __internal_fmad (p, t, -3.0327774474001094E-2f);
    p = __internal_fmad (p, t,  8.3287745746760911E-1f);
    t = __fmul_rn (p, __internal_fast_rcpf (t));
    if (a > 1.0f) t = -t;
  }
  return t;
}

static __forceinline__ float normcdfinvf(float a)
{
  float r = -CUDART_SQRT_TWO_F * erfcinvf (a + a);
  r = __fadd_rn (r, CUDART_ZERO_F);  /* fixup result for a == 0.5f */
  return r;
}

/* normcdf(x) = 0.5 * erfc (-sqrt(0.5) * x). When x < -1.0, the numerical error
   incurred when scaling the argument of normcdf() is amplified, so we need to
   compensate this to guarantee accurate results.
*/
static __forceinline__ float normcdff(float a)
{
  float ah, al, t1, t2, z;
  if (fabsf (a) > 14.5f) a = copysignf (14.5f, a);
#if __CUDA_ARCH__ >= 200
  t1 = __fmul_rn (a, -CUDART_SQRT_HALF_HI_F);
  t2 = __fmaf_rn (a, -CUDART_SQRT_HALF_HI_F, -t1);
  t2 = __fmaf_rn (a, -CUDART_SQRT_HALF_LO_F, t2);
#else /* __CUDA_ARCH__ >= 200 */
  float t3, t4, t5, t6;
  t3 = __internal_fmad (a, 4097.0f, __internal_fmad (-a, 4097.0f, a));
  t4 = a - t3;
  t5 = -0.70703125f;
  t6 = -7.551908493e-5f;
  t1 = __fmul_rn (a, -CUDART_SQRT_HALF_HI_F);
  t2 = __internal_fmad (t3, t6, __internal_fmad (t3, t5, -t1));
  t2 = __internal_fmad (t4, t6, __internal_fmad (t5, t4, t2));
  t2 = __fadd_rn (__fmul_rn (a, -CUDART_SQRT_HALF_LO_F), t2);
#endif /* __CUDA_ARCH__ >= 200 */
  ah = __fadd_rn (t1, t2);
  z = erfcf (ah);
  if (a < -1.0f) {
    al = __fadd_rn (t1 - ah, t2);
    t1 = -2.0f * ah * z;      // crude approximation of slope of erfc() at 'ah'
    z = __internal_fmad (t1, al, z);
  }
  return 0.5f * z;
}

static __forceinline__ float lgammaf(float a)
{
  float t;
  float i;
  int quot;
  t = __internal_lgammaf_pos(fabsf(a));
  if (a >= 0.0f) return t;
  a = fabsf(a);
  i = floorf(a);                   
  if (a == i) return CUDART_INF_F; /* a is an integer: return infinity */
  if (a < 1e-19f) return -__internal_accurate_logf(a);
  i = rintf (2.0f * a);
  quot = (int)i;
  i = __internal_fmad (-i, 0.5f, a);
  i = i * CUDART_PI_F;
  i = __internal_sin_cos_kernel(i, quot);
  i = fabsf(i);
  t = CUDART_LNPI_F - __internal_accurate_logf(i * a) - t;
  return t;
}

static __forceinline__ float ldexpf(float a, int b)
{
  float fa = fabsf(a);

  if ((fa == CUDART_ZERO_F) || (fa == CUDART_INF_F) || (b == 0)) {
    if (!(fa > CUDART_ZERO_F)) a = a + a;
  } else if (abs(b) < 126) {
    a = a * __internal_fast_exp2f((float)b);
  } else if (abs(b) < 252) {
    int bhalf = b / 2;
    a = a * __internal_fast_exp2f((float)bhalf) * 
        __internal_fast_exp2f((float)(b - bhalf));
  } else {
    int bquarter = b / 4;
    float t = __internal_fast_exp2f((float)bquarter);
    a = a * t * t * t * __internal_fast_exp2f((float)(b - 3 * bquarter));
  }
  return a;
}

static __forceinline__ float scalbnf(float a, int b)
{
  /* On binary systems, ldexp(x,exp) is equivalent to scalbn(x,exp) */
  return ldexpf(a, b);
}

static __forceinline__ float scalblnf(float a, long int b)
{
  int t;
  if (b > 2147483647L) {
    t = 2147483647;
  } else if (b < (-2147483647 - 1)) {
    t = (-2147483647 - 1);
  } else {
    t = (int)b;
  }
  return scalbnf(a, t);
}

static __forceinline__ float frexpf(float a, int *b)
{
  float fa = fabsf(a);
  unsigned int expo;
  unsigned int denorm;

  if (fa < CUDART_TWO_TO_M126_F) {
    a *= CUDART_TWO_TO_24_F;
    denorm = 24;
  } else {
    denorm = 0;
  }
  expo = ((unsigned int)__float_as_int(a) >> 23) & 0xff;
  if ((fa == 0.0f) || (expo == 0xff)) {
    expo = 0;
    a = a + a;
  } else {  
    expo = expo - denorm - 126;
    a = __int_as_float(((__float_as_int(a) & 0x807fffff) | 0x3f000000));
  }
  *b = expo;
  return a;
}

static __forceinline__ float modff(float a, float *b)
{
  float t;
  if (__finitef(a)) {
    t = truncf(a);
    *b = t;
    t = a - t;
    t = __internal_copysignf_pos (t, a);
  } else if (__isinff(a)) {
    *b = a;
    t = __internal_copysignf_pos (CUDART_ZERO_F, a);
  } else {
    *b = a + a; 
    t = a + a;
  }
  return t;
}

static __forceinline__ float fmodf(float a, float b)
{
  float orig_a = a;
  float orig_b = b;
  float res;
  a = fabsf(a);
  b = fabsf(b);
  if ((a == CUDART_INF_F) || (b == CUDART_ZERO_F)) {
    res = CUDART_NAN_F;
  } else if (a >= b) {
#if defined(__CUDA_FTZ)
    float scaled_b = __int_as_float ((__float_as_int(b) & 0x007fffff) | 
                                     (__float_as_int(a) & 0x7f800000));
    if (scaled_b > a) {
      scaled_b *= 0.5f;
    }
#else /* defined(__CUDA_FTZ) */
    /* Need to be able to handle denormals correctly */
    int expoa = (int)__log2f(a);
    int expob = (int)__log2f(b);
    int scale = expoa - expob;
    float scaled_b = ldexpf(b, scale);
    if (scaled_b <= 0.5f * a) {
      scaled_b *= 2.0f;
    }
#endif /* defined(__CUDA_FTZ) */
    while (scaled_b >= b) {
      if (a >= scaled_b) {
        a -= scaled_b;
      }
      scaled_b *= 0.5f;
    }
    res = __internal_copysignf_pos (a, orig_a);
  } else {
    res = orig_a;
    if (!(b <= CUDART_INF_F)) res += orig_b;
    if (!(a > CUDART_ZERO_F)) res += orig_a;
  }
  return res;
}

static __forceinline__ float remainderf(float a, float b)
{

  float twoa = 0.0f;
  unsigned int quot0 = 0;  /* quotient bit 0 */
  float orig_a = a;
  float orig_b = b;

  a = fabsf(a);
  b = fabsf(b);
  if (!((a <= CUDART_INF_F) && (b <= CUDART_INF_F))) {
    return orig_a + orig_b;
  }
  if ((a == CUDART_INF_F) || (b == CUDART_ZERO_F)) {
    return CUDART_NAN_F;
  } else if (a >= b) {
#if !defined(__CUDA_FTZ)
    int expoa = (int)__log2f(a);
    int expob = (int)__log2f(b);
    int scale = expoa - expob;
    float scaled_b = ldexpf(b, scale);
    if (scaled_b <= 0.5f * a) {
      scaled_b *= 2.0f;
    }
#else
    float scaled_b = __int_as_float ((__float_as_int(b) & 0x007fffff) | 
                                     (__float_as_int(a) & 0x7f800000));
    if (scaled_b > a) {
      scaled_b *= 0.5f;
    }
    /* check whether divisor is a power of two */
    if (a == scaled_b) {
      return __internal_copysignf_pos (CUDART_ZERO_F, orig_a);
    }    
#endif /* !__CUDA_FTZ */
    while (scaled_b >= b) {
      quot0 = 0;
      if (a >= scaled_b) {
        twoa = __internal_fmad(2.0f, a, -2.0f*scaled_b);
        a = a - scaled_b;
        quot0 = 1;
      }
      scaled_b *= 0.5f;
    }
  }
  /* round quotient to nearest even */
#if !defined(__CUDA_FTZ)
  twoa = 2.0f * a;
  if ((twoa > b) || ((twoa == b) && quot0)) {
    a -= b;
  }
#else
  if (a >= CUDART_TWO_TO_M126_F) {
    twoa = 2.0f * a;
    if ((twoa > b) || ((twoa == b) && quot0)) {
      a -= b;
      a = __int_as_float(__float_as_int(a) | 0x80000000);
    }
  } else {
    /* a already got flushed to zero, so use twoa instead */
    if ((twoa > b) || ((twoa == b) && quot0)) {
      a = 0.5f * __internal_fmad(b, -2.0f, twoa);
      a = __int_as_float(__float_as_int(a) | 0x80000000);
    }
  }
#endif /* !__CUDA_FTZ */
  a = __int_as_float((__float_as_int(orig_a) & 0x80000000) ^
                     __float_as_int(a));
  return a;
}

static __forceinline__ float remquof(float a, float b, int* quo)
{
  float twoa = 0.0f;
  unsigned int quot = 0;  /* trailing quotient bits */
  unsigned int sign;
  float orig_a = a;
  float orig_b = b;

  /* quo has a value whose sign is the sign of x/y */
  sign = 0 - ((__float_as_int(a) ^ __float_as_int(b)) < 0);
  a = fabsf(a);
  b = fabsf(b);
  if (!((a <= CUDART_INF_F) && (b <= CUDART_INF_F))) {
    *quo = quot;
    return orig_a + orig_b;
  }
  if ((a == CUDART_INF_F) || (b == CUDART_ZERO_F)) {
    *quo = quot;
    return CUDART_NAN_F;
  } else if (a >= b) {
#if !defined(__CUDA_FTZ)
    /* Need to be able to handle denormals correctly */
    int expoa = (int)__log2f(a);
    int expob = (int)__log2f(b);
    int scale = expoa - expob;
    float scaled_b = ldexpf(b, scale);
    if (scaled_b <= 0.5f * a) {
      scaled_b *= 2.0f;
    }
#else /* !__CUDA_FTZ */
    float scaled_b = __int_as_float ((__float_as_int(b) & 0x007fffff) | 
                                     (__float_as_int(a) & 0x7f800000));
    if (scaled_b > a) {
      scaled_b *= 0.5f;
    }
    /* check whether divisor is a power of two */
    if (a == scaled_b) {
      if (__float_as_int(b) > 0x7e800000) {
          a *= 0.25f;
          b *= 0.25f;
      }
      a = __fdividef(a,b) + 0.5f;
      quot = (a < 8.0f) ? (int)a : 0;
      quot = quot ^ sign;
      quot = quot - sign;
      *quo = quot;
      return __internal_copysignf_pos (CUDART_ZERO_F, orig_a);
    }    
#endif /* !__CUDA_FTZ */
    while (scaled_b >= b) {
      quot += quot;
      if (a >= scaled_b) {
        twoa = __internal_fmad(2.0f, a, -2.0f*scaled_b);
        a -= scaled_b;
        quot += 1;
      }
      scaled_b *= 0.5f;
    }
  }
  /* round quotient to nearest even */
#if !defined(__CUDA_FTZ)
  twoa = 2.0f * a;
  if ((twoa > b) || ((twoa == b) && (quot & 1))) {
    quot++;
    a -= b;
    a = __int_as_float(__float_as_int(a) | 0x80000000);
  }
#else /* !__CUDA_FTZ */
  if (a >= CUDART_TWO_TO_M126_F) {
    twoa = 2.0f * a;
    if ((twoa > b) || ((twoa == b) && (quot & 1))) {
      quot++;
      a -= b;
      a = __int_as_float(__float_as_int(a) | 0x80000000);
    }
  } else {
    /* a already got flushed to zero, so use twoa instead */
    if ((twoa > b) || ((twoa == b) && (quot & 1))) {
      quot++;
      a = 0.5f * (twoa - 2.0f * b);
      a = __int_as_float(__float_as_int(a) | 0x80000000);
    }
  }
#endif /* !__CUDA_FTZ */
  a = __int_as_float((__float_as_int(orig_a) & 0x80000000) ^
                     __float_as_int(a));
  quot = quot & CUDART_REMQUO_MASK_F;
  quot = quot ^ sign;
  quot = quot - sign;
  *quo = quot;
  return a;
}

static __forceinline__ float fmaf(float a, float b, float c)
{
  return __fmaf_rn(a, b, c);
}

static __forceinline__ float __internal_accurate_powf(float a, float b)
{
  float2 loga, prod;
  float t;

  /* compute log(a) in double-single format*/
  loga = __internal_log_ep(a);

  /* prevent overflow during extended precision multiply */
  if (fabsf(b) > 1.0e34f) b *= 1.220703125e-4f;
  prod.y = b;
  prod.x = 0.0f;
  prod = __internal_dsmul (prod, loga);

  /* prevent intermediate overflow in exponentiation */
  if (__float_as_int(prod.y) == 0x42b17218) {
    prod.y = __int_as_float(__float_as_int(prod.y) - 1);
    prod.x = prod.x + __int_as_float(0x37000000);
  }

  /* compute pow(a,b) = exp(b*log(a)) */
  t = __internal_accurate_expf(prod.y);
  /* prevent -INF + INF = NaN */
  if (t != CUDART_INF_F) {
    /* if prod.x is much smaller than prod.y, then exp(prod.y+prod.x) ~= 
     * exp(prod.y) + prod.x * exp(prod.y) 
     */
    t = __internal_fmad (t, prod.x, t);
  }
  return t;
}

static __forceinline__ float powif(float a, int b)
{
  unsigned int e = abs(b);
  float r = 1.0f;

  while (1) {
    if ((e & 1) != 0) {
      r = __fmul_rn (r, a);
    }
    e = e >> 1;
    if (e == 0) {
      r = (b < 0) ? 1.0f / r : r;
      return r;
    }
    a = __fmul_rn (a, a);
  }
}

static __forceinline__ double powi(double a, int b)
{
  unsigned int e = abs(b);
  double       r = 1.0;

  while (1) {
    if ((e & 1) != 0) {
      r = r * a;
    }
    e = e >> 1;
    if (e == 0) {
      return b < 0 ? 1.0 / r : r;
    }
    a = a * a;
  }
}

static __forceinline__ float powf(float a, float b)
{
#if defined(__USE_FAST_MATH__)
  return __powf(a, b);
#else /* __USE_FAST_MATH__ */
  float t;
  int ti, bIsOddInteger;

  bIsOddInteger = fabsf(b - (2.0f * truncf(0.5f * b))) == 1.0f;
  if (a == 1.0f || b == 0.0f) {
    t = 1.0f;
  } else if (__isnanf(a) || __isnanf(b)) {
    t = a + b;
  } else if (__isinff(b)) {
    ti = 0;
    if (fabsf(a) > 1.0f) ti = 0x7f800000;
    if (b < 0.0f) ti = ti ^ 0x7f800000;
    if (a == -1.0f) ti = 0x3f800000;
    t = __int_as_float (ti);
   } else if (__isinff(a)) {
    ti = 0;
    if (b >= 0.0f) ti = 0x7f800000;
    if ((a < 0.0f) && bIsOddInteger) ti = ti ^ 0x80000000;
    t = __int_as_float (ti);
  } else if (a == 0.0f) {
    ti = 0;
    if (bIsOddInteger) ti = __float_as_int(a+a);
    if (b < 0.0f) ti = ti | 0x7f800000;
    t = __int_as_float (ti);
  } else if ((a < 0.0f) && (b != truncf (b))) {
    t = CUDART_NAN_F;
  } else {
    t = __internal_accurate_powf(fabsf(a), b);
    if ((a < 0.0f) && bIsOddInteger) {
      t = __int_as_float(__float_as_int(t) ^ 0x80000000);
    }
  }
  return t;
#endif /* __USE_FAST_MATH__ */
}

/* approximate 1.0/(x*gamma(x)) on [-0.5,0.5] */
static __forceinline__ float __internal_tgammaf_kernel(float a)
{
  float t;
  t =                        -1.05767296987211380E-003f;
  t = __internal_fmad (t, a,  7.09279059435508670E-003f);
  t = __internal_fmad (t, a, -9.65347121958557050E-003f);
  t = __internal_fmad (t, a, -4.21736613253687960E-002f);
  t = __internal_fmad (t, a,  1.66542401247154280E-001f);
  t = __internal_fmad (t, a, -4.20043267827838460E-002f);
  t = __internal_fmad (t, a, -6.55878234051332940E-001f);
  t = __internal_fmad (t, a,  5.77215696929794240E-001f);
  t = __internal_fmad (t, a,  1.00000000000000000E+000f);
  return t;
}

/* Based on: Kraemer, W.: "Berechnung der Gammafunktion G(x) fuer reelle Punkt-
   und Intervallargumente". Zeitschrift fuer angewandte Mathematik und 
   Mechanik, Vol. 70 (1990), No. 6, pp. 581-584
*/
static __forceinline__ float tgammaf(float a)
{
  float s, xx, x=a;
  if (x >= 0.0f) {
    if (x > 36.0f) x = 36.0f; /* clamp */
    s = 1.0f;
    xx = x;
    if (x > 34.03f) { /* prevent premature overflow */
      xx -= 1.0f;
    }
    while (xx > 1.5f) {
      xx = xx - 1.0f;
      s = s * xx;
    }
    if (x >= 0.5f) {
      xx = xx - 1.0f;
    }
    xx = __internal_tgammaf_kernel(xx);
    if (x < 0.5f) {
      xx = xx * x;
    }
    s = __fdividef(s, xx);
    if (x > 34.03f) {
      /* Cannot use s = s * x - s due to intermediate overflow! */
      xx = x - 1.0f;
      s = s * xx;
    }
    return s;
  } else {
    if (x == floorf(x)) {  /* x is negative integer */
      x = CUDART_NAN_F;
    } 
    if (x < -41.1f) x = -41.1f; /* clamp */
    xx = x;
    if (x < -34.03f) {   /* prevent overflow in intermediate result */        
      xx += 6.0f;
    } 
    s = xx;
    while (xx < -0.5f) {
      xx = xx + 1.0f;
      s = s * xx;
    }
    xx = __internal_tgammaf_kernel(xx);
    s = s * xx;
    s = 1.0f / s;
    if (x < -34.03f) {
      xx = x;
      xx *= (x + 1.0f);
      xx *= (x + 2.0f);
      xx *= (x + 3.0f);
      xx *= (x + 4.0f);
      xx *= (x + 5.0f);
      xx = 1.0f / xx;
      s = s * xx;
      if ((a < -42.0f) && !(((int)a)&1)) {
        s = CUDART_NEG_ZERO_F;
      }
    }    
    return s;
  }
}

static __forceinline__ float roundf(float a)
{
  float fa = fabsf(a);
  float u = __internal_copysignf_pos (0.5f, a);
  u = truncf (a + u);
  if (fa > CUDART_TWO_TO_23_F) u = a;
  if (fa < 0.5f) u = truncf (a);
  return u;
}

static __forceinline__ long long int __internal_llroundf_kernel(float a)
{
  unsigned long long int res, t = 0ULL;
  int shift;
  unsigned int ia = __float_as_int(a);
  /* Make NaN response identical to the hardware response for llrintf() */
  if ((ia << 1) > 0xff000000) return 0x8000000000000000LL;
  if ((int)ia >= 0x5f000000) return 0x7fffffffffffffffLL;
  if (ia >= 0xdf000000) return 0x8000000000000000LL;
  shift = 189 - ((ia >> 23) & 0xff);
  res = ((long long int)(((ia << 8) | 0x80000000) >> 1)) << 32;
  if (shift >= 64) {
    t = res;
    res = 0;
  } else if (shift) {
    t = res << (64 - shift);
    res = res >> shift;
  }
  if (t >= 0x8000000000000000ULL) {
    res++;
  }
  if ((int)ia < 0) res = (unsigned long long int)(-(long long int)res);
  return (long long int)res;
}

static __forceinline__ long long int llroundf(float a)
{
  return __internal_llroundf_kernel(a);
}

static __forceinline__ long int lroundf(float a)
{
#if defined(__LP64__)
  return (long int)llroundf(a);
#else /* __LP64__ */
  return (long int)(roundf(a));
#endif /* __LP64__ */
}

static __forceinline__ float fdimf(float a, float b)
{
  float t;
  t = a - b;    /* default also handles NaNs */
  if (a <= b) {
    t = 0.0f;
  }
  return t;
}

static __forceinline__ int ilogbf(float a)
{
  unsigned int i;
  int expo;
  float fa;
  fa = fabsf(a);
  i = __float_as_int(fa);
  if (i < 0x00800000) {
    /* handle zero and denormals */
    expo = -118 - __clz(i);
    if (i == 0x00000000) {
      expo = -__cuda_INT_MAX-1;
    }
  } else {
      expo = (int)(i >> 23) - 127;
    if (i == 0x7f800000) {
      expo = __cuda_INT_MAX;
    }
    if (i > 0x7f800000) {
      expo = -__cuda_INT_MAX-1;
    }
  } 
  return expo;
}

static __forceinline__ float logbf(float a)
{
  unsigned int i;
  int expo;
  float fa, res;
  fa = fabsf(a);
  i = __float_as_int(fa);
  if (i < 0x00800000) {
    /* handle zero and denormals */
    expo = -118 - __clz(i);
    res = (float)expo;
    if (a == 0.0f) {
      res = -CUDART_INF_F;
    }
  } else {
    expo = (int)(i >> 23) - 127;
    res = (float)expo;
    if (i >= 0x7f800000) {  
      /* return +INF or NaN */
      res = a * a;
    }
  } 
  return res;
}

#endif /* __CUDANVVM__ */

#else /* __CUDABE__ */

/*******************************************************************************
*                                                                              *
* ONLY FOR HOST CODE! NOT FOR DEVICE EXECUTION                                 *
*                                                                              *
*******************************************************************************/

#include <crt/func_macro.h>

#if defined(_WIN32)
#pragma warning(disable : 4211)
#endif /* _WIN32 */

#if defined(_WIN32) || defined(__APPLE__) || defined (__ANDROID__)

__func__(int __isnan(double a))
{
  volatile union {
    double                 d;
    unsigned long long int l;
  } cvt;

  cvt.d = a;

  return cvt.l << 1 > 0xffe0000000000000ull;
}

#endif /* _WIN32 || __APPLE__ || __ANDROID__ */

#if defined(_WIN32) || defined(__APPLE__)

/*******************************************************************************
*                                                                              *
* HOST IMPLEMENTATION FOR DOUBLE ROUTINES FOR WINDOWS & APPLE PLATFORMS        *
*                                                                              *
*******************************************************************************/

__func__(double exp10(double a))
{
  return pow(10.0, a);
}

__func__(float exp10f(float a))
{
    return (float)exp10((double)a);
}

__func__(void sincos(double a, double *sptr, double *cptr))
{
  *sptr = sin(a);
  *cptr = cos(a);
}

__func__(void sincosf(float a, float *sptr, float *cptr))
{
  double s, c;

  sincos((double)a, &s, &c);
  *sptr = (float)s;
  *cptr = (float)c;
}

__func__(int __isinf(double a))
{
  volatile union {
    double                 d;
    unsigned long long int l;
  } cvt;

  cvt.d = a;

  return cvt.l << 1 == 0xffe0000000000000ull;
}

#endif /* _WIN32 || __APPLE__ */

#if defined(_WIN32) || defined (__ANDROID__)

#if _MSC_VER < 1800
__func__(double log2(double a))
{
  return log(a) * 1.44269504088896340;
}
#endif /* _MSC_VER < 1800 */

#endif /* _WIN32 || __ANDROID__ */

#if defined(_WIN32)

#if _MSC_VER < 1600
__func__(long long int llabs(long long int a))
{
  return a < 0ll ? -a : a;
}
#endif /* _MSC_VER < 1600 */

/*******************************************************************************
*                                                                              *
* HOST IMPLEMENTATION FOR DOUBLE ROUTINES FOR WINDOWS PLATFORM                 *
*                                                                              *
*******************************************************************************/

__func__(int __signbit(double a))
{
  volatile union {
    double               d;
    signed long long int l;
  } cvt;

  cvt.d = a;
  return cvt.l < 0ll;
}

#if _MSC_VER < 1800
__func__(double copysign(double a, double b))
{
  volatile union {
    double                 d;
    unsigned long long int l;
  } cvta, cvtb;

  cvta.d = a;
  cvtb.d = b;
  cvta.l = (cvta.l & 0x7fffffffffffffffULL) | (cvtb.l & 0x8000000000000000ULL);
  return cvta.d;
}
#endif /* MSC_VER < 1800 */

__func__(int __finite(double a))
{
  volatile union {
    double                 d;
    unsigned long long int l;
  } cvt;

  cvt.d = a;
  return cvt.l << 1 < 0xffe0000000000000ull;
}

#if _MSC_VER < 1800
__func__(double fmax(double a, double b))
{
  if (__isnan(a) && __isnan(b)) return a + b;
  if (__isnan(a)) return b;
  if (__isnan(b)) return a;
  if ((a == 0.0) && (b == 0.0) && __signbit(b)) return a;
  return a > b ? a : b;
}

__func__(double fmin(double a, double b))
{
  if (__isnan(a) && __isnan(b)) return a + b;
  if (__isnan(a)) return b;
  if (__isnan(b)) return a;
  if ((a == 0.0) && (b == 0.0) && __signbit(a)) return a;
  return a < b ? a : b;
}

__func__(double trunc(double a))
{
  return a < 0.0 ? ceil(a) : floor(a);
}

__func__(double round(double a))
{
  double fa = fabs(a);

  if (fa > CUDART_TWO_TO_52) {
    return a;
  } else {
    double u = floor(fa + 0.5);
    if (fa < 0.5) u = 0;
    u = copysign (u, a);
    return u;
  }
}

__func__(long int lround(double a))
{
  return (long int)round(a);
}

__func__(long long int llround(double a))
{
  return (long long int)round(a);
}

__func__(double rint(double a))
{
  double fa = fabs(a);
  double u = CUDART_TWO_TO_52 + fa;
  if (fa >= CUDART_TWO_TO_52) {
    u = a;
  } else {
    u = u - CUDART_TWO_TO_52;
    u = copysign (u, a);
  }
  return u;  
}

__func__(double nearbyint(double a))
{
  return rint(a);
}

__func__(long int lrint(double a))
{
  return (long int)rint(a);
}

__func__(long long int llrint(double a))
{
  return (long long int)rint(a);
}

__func__(double fdim(double a, double b))
{
  if (a > b) {
    return (a - b);
  } else if (a <= b) {
    return 0.0;
  } else if (__isnan(a)) {
    return a;
  } else {
    return b;
  }
}

__func__(double scalbn(double a, int b))
{
  return ldexp(a, b);
}

__func__(double scalbln(double a, long int b))
{
  int t;

  if (b > 2147483647L) {
    t = 2147483647;
  } else if (b < (-2147483647 - 1)) {
    t = (-2147483647 - 1);
  } else {
    t = (int)b;
  }
  return scalbn(a, t);
}

__func__(double exp2(double a))
{
  return pow(2.0, a);
}

/*  
 * The following is based on: David Goldberg, "What every computer scientist 
 * should know about floating-point arithmetic", ACM Computing Surveys, Volume 
 * 23, Issue 1, March 1991.
 */
__func__(double log1p(double a))
{
  volatile double u, m;

  u = 1.0 + a;
  if (u == 1.0) {
    /* a very close to zero */
    u = a;
  } else {
    m = u - 1.0;
    u = log(u);
    if (a < 1.0) {
      /* a somewhat close to zero */
      u = a * u;
      u = u / m;
    }
  }
  return u;
}

/*
 * This code based on: http://www.cs.berkeley.edu/~wkahan/Math128/Sumnfp.pdf
 */
__func__(double expm1(double a))
{
  volatile double u, m;

  u = exp(a);
  m = u - 1.0;
  if (m == 0.0) {
    /* a very close zero */
    m = a;
  } 
  else if (fabs(a) < 1.0) {
    /* a somewhat close zero */
    u = log(u);
    m = m * a;
    m = m / u;
  }
  return m;
}

__func__(double cbrt(double a))
{
  double s, t;

  if (a == 0.0 || __isinf(a)) {
    return a;
  } 
  s = fabs(a);
  t = exp2(CUDART_THIRD * log2(s));           /* initial approximation */
  t = t - (t - (s / (t * t))) * CUDART_THIRD; /* refine approximation */
  t = copysign(t, a);
  return t;
}

__func__(double acosh(double a))
{
  double s, t;

  t = a - 1.0;
  if (t == a) {
    return log(2.0) + log(a);
  } else {
    s = a + 1.0;
    t = t + sqrt(s * t);
    return log1p(t);
  }
}

__func__(double asinh(double a))
{
  double fa, oofa, t;

  fa = fabs(a);
  if (fa > 1e18) {
    t = log(2.0) + log(fa);
  } else {
    oofa = 1.0 / fa;
    t = fa + fa / (oofa + sqrt(1.0 + oofa * oofa));
    t = log1p(t);
  }
  t = copysign(t, a);
  return t;
}

__func__(double atanh(double a))
{
  double fa, t;

  if (__isnan(a)) {
    return a + a;
  }
  fa = fabs(a);
  t = (2.0 * fa) / (1.0 - fa);
  t = 0.5 * log1p(t);
  if (__isnan(t) || !__signbit(a)) {
    return t;
  }
  return -t;
}

__func__(int ilogb(double a))
{
  volatile union {
    double                 d;
    unsigned long long int l;
  } x;
  unsigned long long int i;
  int expo = -1022;

  if (__isnan(a)) return -__cuda_INT_MAX-1;
  if (__isinf(a)) return __cuda_INT_MAX;
  x.d = a;
  i = x.l & 0x7fffffffffffffffull;
  if (i == 0) return -__cuda_INT_MAX-1;
  if (i >= 0x0010000000000000ull) {
    return (int)(((i >> 52) & 0x7ff) - 1023);
  }
  while (i < 0x0010000000000000ull) {
    expo--;
    i <<= 1;
  }
  return expo;
}

__func__(double logb(double a))
{
  volatile union {
    double                 d;
    unsigned long long int l;
  } x;
  unsigned long long int i;
  int expo = -1022;

  if (__isnan(a)) return a + a;
  if (__isinf(a)) return fabs(a);
  x.d = a;
  i = x.l & 0x7fffffffffffffffull;
  if (i == 0) return -1.0/fabs(a);
  if (i >= 0x0010000000000000ull) {
    return (double)((int)((i >> 52) & 0x7ff) - 1023);
  }
  while (i < 0x0010000000000000ull) {
    expo--;
    i <<= 1;
  }
  return (double)expo;
}

__func__(double remquo(double a, double b, int *quo))
{
  volatile union {
    double                 d;
    unsigned long long int l;
  } cvt;
  int rem1 = 1; /* do FPREM1, a.k.a IEEE remainder */
  int expo_a;
  int expo_b;
  unsigned long long mant_a;
  unsigned long long mant_b;
  unsigned long long mant_c;
  unsigned long long temp;
  int sign_a;
  int sign_b;
  int sign_c;
  int expo_c;
  int expodiff;
  int quot = 0;                 /* initialize quotient */
  int l;
  int iter;

  cvt.d = a;
  mant_a = (cvt.l << 11) | 0x8000000000000000ULL;
  expo_a = (int)((cvt.l >> 52) & 0x7ff) - 1023;
  sign_a = (int)(cvt.l >> 63);

  cvt.d = b;
  mant_b = (cvt.l << 11) | 0x8000000000000000ULL;
  expo_b = (int)((cvt.l >> 52) & 0x7ff) - 1023;
  sign_b = (int)(cvt.l >> 63);

  sign_c = sign_a;  /* remainder has sign of dividend */
  expo_c = expo_a;  /* default */
      
  /* handled NaNs and infinities */
  if (__isnan(a) || __isnan(b)) {
    *quo = quot;
    return a + b;
  }
  if (__isinf(a) || (b == 0.0)) {
    *quo = quot;
    cvt.l = 0xfff8000000000000ULL;
    return cvt.d;
  }
  if ((a == 0.0) || (__isinf(b))) {
    *quo = quot;
    return a;
  }
  /* normalize denormals */
  if (expo_a < -1022) {
    mant_a = mant_a + mant_a;
    while (mant_a < 0x8000000000000000ULL) {
      mant_a = mant_a + mant_a;
      expo_a--;
    }
  } 
  if (expo_b < -1022) {
    mant_b = mant_b + mant_b;
    while (mant_b < 0x8000000000000000ULL) {
      mant_b = mant_b + mant_b;
      expo_b--;
    }
  }
  expodiff = expo_a - expo_b;
  /* clamp iterations if exponent difference negative */
  if (expodiff < 0) {
    iter = -1;
  } else {
    iter = expodiff;
  }
  /* Shift dividend and divisor right by one bit to prevent overflow
     during the division algorithm.
   */
  mant_a = mant_a >> 1;
  mant_b = mant_b >> 1;
  expo_c = expo_a - iter; /* default exponent of result   */

  /* Use binary longhand division (restoring) */
  for (l = 0; l < (iter + 1); l++) {
    mant_a = mant_a - mant_b;
    if (mant_a & 0x8000000000000000ULL) {
      mant_a = mant_a + mant_b;
      quot = quot + quot;
    } else {
      quot = quot + quot + 1;
    }
    mant_a = mant_a + mant_a;
  }

  /* Save current remainder */
  mant_c = mant_a;
  /* If remainder's mantissa is all zeroes, final result is zero. */
  if (mant_c == 0) {
    quot = quot & 7;
    *quo = (sign_a ^ sign_b) ? -quot : quot;
    cvt.l = (unsigned long long int)sign_c << 63;
    return cvt.d;
  }
  /* Normalize result */
  while (!(mant_c & 0x8000000000000000ULL)) {
    mant_c = mant_c + mant_c;
    expo_c--;
  }
  /* For IEEE remainder (quotient rounded to nearest-even we might need to 
     do a final subtraction of the divisor from the remainder.
  */
  if (rem1 && ((expodiff+1) >= 0)) {
    temp = mant_a - mant_b;
    /* round quotient to nearest even */
    if (((temp != 0ULL) && (!(temp & 0x8000000000000000ULL))) ||
        ((temp == 0ULL) && (quot & 1))) {
      mant_a = mant_a >> 1;
      quot++;
      /* Since the divisor is greater than the remainder, the result will
         have opposite sign of the dividend. To avoid a negative mantissa
         when subtracting the divisor from remainder, reverse subtraction
      */
      sign_c = 1 ^ sign_c;
      expo_c = expo_a - iter + 1;
      mant_c = mant_b - mant_a;
      /* normalize result */
      while (!(mant_c & 0x8000000000000000ULL)) {
        mant_c = mant_c + mant_c;
        expo_c--;
      }
    }
  }
  /* package up result */
  if (expo_c >= -1022) { /* normal */
    mant_c = ((mant_c >> 11) + 
              ((((unsigned long long)sign_c) << 63) +
               (((unsigned long long)(expo_c + 1022)) << 52)));
  } else { /* denormal */
    mant_c = ((((unsigned long long)sign_c) << 63) + 
              (mant_c >> (11 - expo_c - 1022)));
  }
  quot = quot & 7; /* mask quotient down to least significant three bits */
  *quo = (sign_a ^ sign_b) ? -quot : quot;
  cvt.l = mant_c;
  return cvt.d;
}

__func__(double remainder(double a, double b))
{
  int quo;
  return remquo (a, b, &quo);
}

__func__(double fma (double a, double b, double c))
{
  volatile union {
    struct {
      unsigned int lo;
      unsigned int hi;
    } part;
    double d;
  } xx, yy, zz, ww;
  unsigned int s, t, u, prod0, prod1, prod2, prod3, expo_x, expo_y, expo_z;
  
  xx.d = a;
  yy.d = b;
  zz.d = c;

  expo_z = 0x7FF;
  t =  xx.part.hi >> 20;
  expo_x = expo_z & t;
  expo_x = expo_x - 1;    /* expo(x) - 1 */
  t =  yy.part.hi >> 20;
  expo_y = expo_z & t;
  expo_y = expo_y - 1;    /* expo(y) - 1 */
  t =  zz.part.hi >> 20;
  expo_z = expo_z & t;
  expo_z = expo_z - 1;    /* expo(z) - 1 */

  if (!((expo_x <= 0x7FD) &&
        (expo_y <= 0x7FD) &&
        (expo_z <= 0x7FD))) {
    
    /* fma (nan, y, z) --> nan
       fma (x, nan, z) --> nan
       fma (x, y, nan) --> nan 
    */
    if (((yy.part.hi << 1) | (yy.part.lo != 0)) > 0xffe00000) {
      yy.part.hi |= 0x00080000;
      return yy.d;
    }
    if (((zz.part.hi << 1) | (zz.part.lo != 0)) > 0xffe00000) {
      zz.part.hi |= 0x00080000;
      return zz.d;
    }
    if (((xx.part.hi << 1) | (xx.part.lo != 0)) > 0xffe00000) {
      xx.part.hi |= 0x00080000;
      return xx.d;
    }
    
    /* fma (0, inf, z) --> INDEFINITE
       fma (inf, 0, z) --> INDEFINITE
       fma (-inf,+y,+inf) --> INDEFINITE
       fma (+x,-inf,+inf) --> INDEFINITE
       fma (+inf,-y,+inf) --> INDEFINITE
       fma (-x,+inf,+inf) --> INDEFINITE
       fma (-inf,-y,-inf) --> INDEFINITE
       fma (-x,-inf,-inf) --> INDEFINITE
       fma (+inf,+y,-inf) --> INDEFINITE
       fma (+x,+inf,-inf) --> INDEFINITE
    */
    if (((((xx.part.hi << 1) | xx.part.lo) == 0) && 
         (((yy.part.hi << 1) | (yy.part.lo != 0)) == 0xffe00000)) ||
        ((((yy.part.hi << 1) | yy.part.lo) == 0) && 
         (((xx.part.hi << 1) | (xx.part.lo != 0)) == 0xffe00000))) {
      xx.part.hi = 0xfff80000;
      xx.part.lo = 0x00000000;
      return xx.d;
    }
    if (((zz.part.hi << 1) | (zz.part.lo != 0)) == 0xffe00000) {
      if ((((yy.part.hi << 1) | (yy.part.lo != 0)) == 0xffe00000) ||
          (((xx.part.hi << 1) | (xx.part.lo != 0)) == 0xffe00000)) {
        if ((int)(xx.part.hi ^ yy.part.hi ^ zz.part.hi) < 0) {
          xx.part.hi = 0xfff80000;
          xx.part.lo = 0x00000000;
          return xx.d;
        }
      }
    }
    /* fma (inf, y, z) --> inf
       fma (x, inf, z) --> inf
       fma (x, y, inf) --> inf
    */
    if (((xx.part.hi << 1) | (xx.part.lo != 0)) == 0xffe00000) {
      xx.part.hi = xx.part.hi ^ (yy.part.hi & 0x80000000);
      return xx.d;
    }
    if (((yy.part.hi << 1) | (yy.part.lo != 0)) == 0xffe00000) {
      yy.part.hi = yy.part.hi ^ (xx.part.hi & 0x80000000);
      return yy.d;
    }
    if (((zz.part.hi << 1) | (zz.part.lo != 0)) == 0xffe00000) {
      return zz.d;
    }
    /* fma (+0, -y, -0) --> -0
       fma (-0, +y, -0) --> -0
       fma (+x, -0, -0) --> -0
       fma (-x, +0, -0) --> -0
    */
    if ((zz.part.hi == 0x80000000) && (zz.part.lo == 0)) {
      if ((((xx.part.hi << 1) | xx.part.lo) == 0) ||
          (((yy.part.hi << 1) | yy.part.lo) == 0)) {
        if ((int)(xx.part.hi ^ yy.part.hi) < 0) {
          return zz.d;
        }
      }
    }
    /* fma (0, y, 0) --> +0  (-0 if round down and signs of addend differ)
       fma (x, 0, 0) --> +0  (-0 if round down and signs of addend differ)
    */
    if ((((zz.part.hi << 1) | zz.part.lo) == 0) &&
        ((((xx.part.hi << 1) | xx.part.lo) == 0) ||
         (((yy.part.hi << 1) | yy.part.lo) == 0))) {
      zz.part.hi &= 0x7fffffff;
      return zz.d;
    }
    
    /* fma (0, y, z) --> z
       fma (x, 0, z) --> z
    */
    if ((((xx.part.hi << 1) | xx.part.lo) == 0) ||
        (((yy.part.hi << 1) | yy.part.lo) == 0)) {
      return zz.d;
    }
    
    if (expo_x == 0xffffffff) {
      expo_x++;
      t = xx.part.hi & 0x80000000;
      s = xx.part.lo >> 21;
      xx.part.lo = xx.part.lo << 11;
      xx.part.hi = xx.part.hi << 11;
      xx.part.hi = xx.part.hi | s;
      if (!xx.part.hi) {
        xx.part.hi = xx.part.lo;
        xx.part.lo = 0;
        expo_x -= 32;
      }
      while ((int)xx.part.hi > 0) {
        s = xx.part.lo >> 31;
        xx.part.lo = xx.part.lo + xx.part.lo;
        xx.part.hi = xx.part.hi + xx.part.hi;
        xx.part.hi = xx.part.hi | s;
        expo_x--;
      }
      xx.part.lo = (xx.part.lo >> 11);
      xx.part.lo |= (xx.part.hi << 21);
      xx.part.hi = (xx.part.hi >> 11) | t;
    }
    if (expo_y == 0xffffffff) {
      expo_y++;
      t = yy.part.hi & 0x80000000;
      s = yy.part.lo >> 21;
      yy.part.lo = yy.part.lo << 11;
      yy.part.hi = yy.part.hi << 11;
      yy.part.hi = yy.part.hi | s;
      if (!yy.part.hi) {
        yy.part.hi = yy.part.lo;
        yy.part.lo = 0;
        expo_y -= 32;
      }
      while ((int)yy.part.hi > 0) {
        s = yy.part.lo >> 31;
        yy.part.lo = yy.part.lo + yy.part.lo;
        yy.part.hi = yy.part.hi + yy.part.hi;
        yy.part.hi = yy.part.hi | s;
        expo_y--;
      }
      yy.part.lo = (yy.part.lo >> 11);
      yy.part.lo |= (yy.part.hi << 21);
      yy.part.hi = (yy.part.hi >> 11) | t;
    }
    if (expo_z == 0xffffffff) {
      expo_z++;
      t = zz.part.hi & 0x80000000;
      s = zz.part.lo >> 21;
      zz.part.lo = zz.part.lo << 11;
      zz.part.hi = zz.part.hi << 11;
      zz.part.hi = zz.part.hi | s;
      if (!zz.part.hi) {
        zz.part.hi = zz.part.lo;
        zz.part.lo = 0;
        expo_z -= 32;
      }
      while ((int)zz.part.hi > 0) {
        s = zz.part.lo >> 31;
        zz.part.lo = zz.part.lo + zz.part.lo;
        zz.part.hi = zz.part.hi + zz.part.hi;
        zz.part.hi = zz.part.hi | s;
        expo_z--;
      }
      zz.part.lo = (zz.part.lo >> 11);
      zz.part.lo |= (zz.part.hi << 21);
      zz.part.hi = (zz.part.hi >> 11) | t;
    }
  }
  
  expo_x = expo_x + expo_y;
  expo_y = xx.part.hi ^ yy.part.hi;
  t = xx.part.lo >> 21;
  xx.part.lo = xx.part.lo << 11;
  xx.part.hi = xx.part.hi << 11;
  xx.part.hi = xx.part.hi | t;
  yy.part.hi = yy.part.hi & 0x000fffff;
  xx.part.hi = xx.part.hi | 0x80000000; /* set mantissa hidden bit */
  yy.part.hi = yy.part.hi | 0x00100000; /* set mantissa hidden bit */

  prod0 = xx.part.lo * yy.part.lo;
  prod1 =(unsigned)(((unsigned long long)xx.part.lo*(unsigned long long)yy.part.lo)>>32);
  prod2 = xx.part.hi * yy.part.lo;
  prod3 = xx.part.lo * yy.part.hi;
  prod1 += prod2;
  t = (unsigned)(prod1 < prod2);
  prod1 += prod3;
  t += prod1 < prod3;
  prod2 =(unsigned)(((unsigned long long)xx.part.hi*(unsigned long long)yy.part.lo)>>32);
  prod3 =(unsigned)(((unsigned long long)xx.part.lo*(unsigned long long)yy.part.hi)>>32);
  prod2 += prod3;
  s = (unsigned)(prod2 < prod3);
  prod3 = xx.part.hi * yy.part.hi;
  prod2 += prod3;
  s += prod2 < prod3;
  prod2 += t;
  s += prod2 < t;
  prod3 =(unsigned)(((unsigned long long)xx.part.hi*(unsigned long long)yy.part.hi)>>32);
  prod3 = prod3 + s;
  
  yy.part.lo = prod0;                 /* mantissa */
  yy.part.hi = prod1;                 /* mantissa */
  xx.part.lo = prod2;                 /* mantissa */
  xx.part.hi = prod3;                 /* mantissa */
  expo_x = expo_x - (1023 - 2);  /* expo-1 */
  expo_y = expo_y & 0x80000000;  /* sign */

  if (xx.part.hi < 0x00100000) {
    s = xx.part.lo >> 31;
    s = (xx.part.hi << 1) + s;
    xx.part.hi = s;
    s = yy.part.hi >> 31;
    s = (xx.part.lo << 1) + s;
    xx.part.lo = s;
    s = yy.part.lo >> 31;
    s = (yy.part.hi << 1) + s;
    yy.part.hi = s;
    s = yy.part.lo << 1;
    yy.part.lo = s;
    expo_x--;
  }

  t = 0;
  if (((zz.part.hi << 1) | zz.part.lo) != 0) { /* z is not zero */
    
    s = zz.part.hi & 0x80000000;
    
    zz.part.hi &= 0x000fffff;
    zz.part.hi |= 0x00100000;
    ww.part.hi = 0;
    ww.part.lo = 0;
    
    /* compare and swap. put augend into xx:yy */
    if ((int)expo_z > (int)expo_x) {
      t = expo_z;
      expo_z = expo_x;
      expo_x = t;
      t = zz.part.hi;
      zz.part.hi = xx.part.hi;
      xx.part.hi = t;
      t = zz.part.lo;
      zz.part.lo = xx.part.lo;
      xx.part.lo = t;
      t = ww.part.hi;
      ww.part.hi = yy.part.hi;
      yy.part.hi = t;
      t = ww.part.lo;
      ww.part.lo = yy.part.lo;
      yy.part.lo = t;
      t = expo_y;
      expo_y = s;
      s = t;
    }
    
    /* augend_sign = expo_y, augend_mant = xx:yy, augend_expo = expo_x */
    /* addend_sign = s, addend_mant = zz:ww, addend_expo = expo_z */
    expo_z = expo_x - expo_z;
    u = expo_y ^ s;
    if (expo_z <= 107) {
      /* denormalize addend */
      t = 0;
      while (expo_z >= 32) {
        t     = ww.part.lo | (t != 0);
        ww.part.lo = ww.part.hi;
        ww.part.hi = zz.part.lo;
        zz.part.lo = zz.part.hi;
        zz.part.hi = 0;
        expo_z -= 32;
      }
      if (expo_z) {
        t     = (t     >> expo_z) | (ww.part.lo << (32 - expo_z)) | 
                ((t << (32 - expo_z)) != 0);
        ww.part.lo = (ww.part.lo >> expo_z) | (ww.part.hi << (32 - expo_z));
        ww.part.hi = (ww.part.hi >> expo_z) | (zz.part.lo << (32 - expo_z));
        zz.part.lo = (zz.part.lo >> expo_z) | (zz.part.hi << (32 - expo_z));
        zz.part.hi = (zz.part.hi >> expo_z);
      }
    } else {
      t = 1;
      ww.part.lo = 0;
      ww.part.hi = 0;
      zz.part.lo = 0;
      zz.part.hi = 0;
    }
    if ((int)u < 0) {
      /* signs differ, effective subtraction */
      t = (unsigned)(-(int)t);
      s = (unsigned)(t != 0);
      u = yy.part.lo - s;
      s = (unsigned)(u > yy.part.lo);
      yy.part.lo = u - ww.part.lo;
      s += yy.part.lo > u;
      u = yy.part.hi - s;
      s = (unsigned)(u > yy.part.hi);
      yy.part.hi = u - ww.part.hi;
      s += yy.part.hi > u;
      u = xx.part.lo - s;
      s = (unsigned)(u > xx.part.lo);
      xx.part.lo = u - zz.part.lo;
      s += xx.part.lo > u;
      xx.part.hi = (xx.part.hi - zz.part.hi) - s;
      if (!(xx.part.hi | xx.part.lo | yy.part.hi | yy.part.lo | t)) {
        /* complete cancelation, return 0 */
        return xx.d;
      }
      if ((int)xx.part.hi < 0) {
        /* Oops, augend had smaller mantissa. Negate mantissa and flip
           sign of result
        */
        t = ~t;
        yy.part.lo = ~yy.part.lo;
        yy.part.hi = ~yy.part.hi;
        xx.part.lo = ~xx.part.lo;
        xx.part.hi = ~xx.part.hi;
        if (++t == 0) {
          if (++yy.part.lo == 0) {
            if (++yy.part.hi == 0) {
              if (++xx.part.lo == 0) {
              ++xx.part.hi;
              }
            }
          }
        }
        expo_y ^= 0x80000000;
      }
        
      /* normalize mantissa, if necessary */
      while (!(xx.part.hi & 0x00100000)) {
        xx.part.hi = (xx.part.hi << 1) | (xx.part.lo >> 31);
        xx.part.lo = (xx.part.lo << 1) | (yy.part.hi >> 31);
        yy.part.hi = (yy.part.hi << 1) | (yy.part.lo >> 31);
        yy.part.lo = (yy.part.lo << 1);
        expo_x--;
      }
    } else {
      /* signs are the same, effective addition */
      yy.part.lo = yy.part.lo + ww.part.lo;
      s = (unsigned)(yy.part.lo < ww.part.lo);
      yy.part.hi = yy.part.hi + s;
      u = (unsigned)(yy.part.hi < s);
      yy.part.hi = yy.part.hi + ww.part.hi;
      u += yy.part.hi < ww.part.hi;
      xx.part.lo = xx.part.lo + u;
      s = (unsigned)(xx.part.lo < u);
      xx.part.lo = xx.part.lo + zz.part.lo;
      s += xx.part.lo < zz.part.lo;
      xx.part.hi = xx.part.hi + zz.part.hi + s;
      if (xx.part.hi & 0x00200000) {
        t = t | (yy.part.lo << 31);
        yy.part.lo = (yy.part.lo >> 1) | (yy.part.hi << 31);
        yy.part.hi = (yy.part.hi >> 1) | (xx.part.lo << 31);
        xx.part.lo = (xx.part.lo >> 1) | (xx.part.hi << 31);
        xx.part.hi = ((xx.part.hi & 0x80000000) | (xx.part.hi >> 1)) & ~0x40000000;
        expo_x++;
      }
    }
  }
  t = yy.part.lo | (t != 0);
  t = yy.part.hi | (t != 0);
        
  xx.part.hi |= expo_y; /* or in sign bit */
  if (expo_x <= 0x7FD) {
    /* normal */
    xx.part.hi = xx.part.hi & ~0x00100000; /* lop off integer bit */
    s = xx.part.lo & 1; /* mantissa lsb */
    u = xx.part.lo;
    xx.part.lo += (t == 0x80000000) ? s : (t >> 31);
    xx.part.hi += (u > xx.part.lo);
    xx.part.hi += ((expo_x + 1) << 20);
    return xx.d;
  } else if ((int)expo_x >= 2046) {      
    /* overflow */
    xx.part.hi = (xx.part.hi & 0x80000000) | 0x7ff00000;
    xx.part.lo = 0;
    return xx.d;
  }
  /* subnormal */
  expo_x = (unsigned)(-(int)expo_x);
  if (expo_x > 54) {
    xx.part.hi = xx.part.hi & 0x80000000;
    xx.part.lo = 0;
    return xx.d;
  }  
  yy.part.hi = xx.part.hi &  0x80000000;   /* save sign bit */
  xx.part.hi = xx.part.hi & ~0xffe00000;
  if (expo_x >= 32) {
    t = xx.part.lo | (t != 0);
    xx.part.lo = xx.part.hi;
    xx.part.hi = 0;
    expo_x -= 32;
  }
  if (expo_x) {
    t     = (t     >> expo_x) | (xx.part.lo << (32 - expo_x)) | (t != 0);
    xx.part.lo = (xx.part.lo >> expo_x) | (xx.part.hi << (32 - expo_x));
    xx.part.hi = (xx.part.hi >> expo_x);
  }
  expo_x = xx.part.lo & 1; 
  u = xx.part.lo;
  xx.part.lo += (t == 0x80000000) ? expo_x : (t >> 31);
  xx.part.hi += (u > xx.part.lo);
  xx.part.hi |= yy.part.hi;
  return xx.d;
}

__func__(double nextafter(double a, double b))
{
  volatile union {
    double d;
    unsigned long long int l;
  } cvt;
  unsigned long long int ia;
  unsigned long long int ib;
  cvt.d = a;
  ia = cvt.l;
  cvt.d = b;
  ib = cvt.l;
  if (__isnan(a) || __isnan(b)) return a + b; /* NaN */
  if (((ia | ib) << 1) == 0ULL) return b;
  if (a == 0.0) {
    return copysign (4.9406564584124654e-324, b); /* crossover */
  }
  if ((a < b) && (a < 0.0)) ia--;
  if ((a < b) && (a > 0.0)) ia++;
  if ((a > b) && (a < 0.0)) ia++;
  if ((a > b) && (a > 0.0)) ia--;
  cvt.l = ia;
  return cvt.d;
}

__func__(double erf(double a))
{
  double t, r, q;

  t = fabs(a);
  if (t >= 1.0) {
    r =        -1.28836351230756500E-019;
    r = r * t + 1.30597472161093370E-017;
    r = r * t - 6.33924401259620500E-016;
    r = r * t + 1.96231865908940140E-014;
    r = r * t - 4.35272243559990750E-013;
    r = r * t + 7.37083927929352150E-012;
    r = r * t - 9.91402142550461630E-011;
    r = r * t + 1.08817017167760820E-009;
    r = r * t - 9.93918713097634620E-009;
    r = r * t + 7.66739923255145500E-008;
    r = r * t - 5.05440278302806720E-007;
    r = r * t + 2.87474157099000620E-006;
    r = r * t - 1.42246725399722510E-005;
    r = r * t + 6.16994555079419460E-005;
    r = r * t - 2.36305221938908790E-004;
    r = r * t + 8.05032844055371070E-004;
    r = r * t - 2.45833366629108140E-003;
    r = r * t + 6.78340988296706120E-003;
    r = r * t - 1.70509103597554640E-002;
    r = r * t + 3.93322852515666300E-002;
    r = r * t - 8.37271292613764040E-002;
    r = r * t + 1.64870423707623280E-001;
    r = r * t - 2.99729521787681470E-001;
    r = r * t + 4.99394435612628580E-001;
    r = r * t - 7.52014596480123030E-001;
    r = r * t + 9.99933138314926250E-001;
    r = r * t - 1.12836725321102670E+000;
    r = r * t + 9.99998988715182450E-001;
    q = exp (-t * t);
    r = 1.0 - r * q;
    if (t >= 6.5) {
      r = 1.0;
    }    
    a = copysign (r, a);
  } else {
    q = a * a;
    r =        -7.77946848895991420E-010;
    r = r * q + 1.37109803980285950E-008;
    r = r * q - 1.62063137584932240E-007;
    r = r * q + 1.64471315712790040E-006;
    r = r * q - 1.49247123020098620E-005;
    r = r * q + 1.20552935769006260E-004;
    r = r * q - 8.54832592931448980E-004;
    r = r * q + 5.22397760611847340E-003;
    r = r * q - 2.68661706431114690E-002;
    r = r * q + 1.12837916709441850E-001;
    r = r * q - 3.76126389031835210E-001;
    r = r * q + 1.12837916709551260E+000;
    a = r * a;
  }
  return a;
}

__func__(double erfc(double a))
{
  double p, q, h, l;

  if (a < 0.75) {
    return 1.0 - erf(a);
  } 
  if (a > 27.3) {
    return 0.0;
  }
  if (a < 5.0) {
    double t;
    t = 1.0 / a;
    p =         1.9759923722227928E-008;
    p = p * t - 1.0000002670474897E+000;
    p = p * t - 7.4935303236347828E-001;
    p = p * t - 1.5648136328071860E-001;
    p = p * t + 1.2871196242447239E-001;
    p = p * t + 1.1126459974811195E-001;
    p = p * t + 4.0678642255914332E-002;
    p = p * t + 7.9915414156678296E-003;
    p = p * t + 7.1458332107840234E-004;
    q =     t + 2.7493547525030619E+000;
    q = q * t + 3.3984254815725423E+000;
    q = q * t + 2.4635304979947761E+000;
    q = q * t + 1.1405284734691286E+000;
    q = q * t + 3.4130157606195649E-001;
    q = q * t + 6.2250967676044953E-002;
    q = q * t + 5.5661370941268700E-003;
    q = q * t + 1.0575248365468671E-009;
    p = p / q;
    p = p * t;
    h = ((int)(a * 16.0)) * 0.0625;
    l = (a - h) * (a + h);
    q = exp(-h * h) * exp(-l);
    q = q * 0.5;
    p = p * q + q;
    p = p * t;
  } else {
    double ooa, ooasq;

    ooa = 1.0 / a;
    ooasq = ooa * ooa;
    p =            -4.0025406686930527E+005;
    p = p * ooasq + 1.4420582543942123E+005;
    p = p * ooasq - 2.7664185780951841E+004;
    p = p * ooasq + 4.1144611644767283E+003;
    p = p * ooasq - 5.8706000519209351E+002;
    p = p * ooasq + 9.1490086446323375E+001;
    p = p * ooasq - 1.6659491387740221E+001;
    p = p * ooasq + 3.7024804085481784E+000;
    p = p * ooasq - 1.0578553994424316E+000;
    p = p * ooasq + 4.2314218745087778E-001;
    p = p * ooasq - 2.8209479177354962E-001;
    p = p * ooasq + 5.6418958354775606E-001;
    h = a * a;
    h = ((int)(a * 16.0)) * 0.0625;
    l = (a - h) * (a + h);
    q = exp(-h * h) * exp(-l);
    p = p * ooa;
    p = p * q;
  }
  return p;
}

__func__(double lgamma(double a))
{
  double s;
  double t;
  double i;
  double fa;
  double sum;
  long long int quot;
  if (__isnan(a) || __isinf(a)) {
    return a * a;
  }
  fa = fabs(a);
  if (fa >= 3.0) {
    if (fa >= 8.0) {
      /* Stirling approximation; coefficients from Hart et al, "Computer 
       * Approximations", Wiley 1968. Approximation 5404. 
       */
      s = 1.0 / fa;
      t = s * s;
      sum =          -0.1633436431e-2;
      sum = sum * t + 0.83645878922e-3;
      sum = sum * t - 0.5951896861197e-3;
      sum = sum * t + 0.793650576493454e-3;
      sum = sum * t - 0.277777777735865004e-2;
      sum = sum * t + 0.833333333333331018375e-1;
      sum = sum * s + 0.918938533204672;
      s = 0.5 * log (fa);
      t = fa - 0.5;
      s = s * t;
      t = s - fa;
      s = s + sum;
      t = t + s;
    } else {
      i = fa - 3.0;
      s =        -4.02412642744125560E+003;
      s = s * i - 2.97693796998962000E+005;
      s = s * i - 6.38367087682528790E+006;
      s = s * i - 5.57807214576539320E+007;
      s = s * i - 2.24585140671479230E+008;
      s = s * i - 4.70690608529125090E+008;
      s = s * i - 7.62587065363263010E+008;
      s = s * i - 9.71405112477113250E+008;
      t =     i - 1.02277248359873170E+003;
      t = t * i - 1.34815350617954480E+005;
      t = t * i - 4.64321188814343610E+006;
      t = t * i - 6.48011106025542540E+007;
      t = t * i - 4.19763847787431360E+008;
      t = t * i - 1.25629926018000720E+009;
      t = t * i - 1.40144133846491690E+009;
      t = s / t;
      t = t + i;
    }
  } else if (fa >= 1.5) {
    i = fa - 2.0;
    t =         9.84839283076310610E-009;
    t = t * i - 6.69743850483466500E-008;
    t = t * i + 2.16565148880011450E-007;
    t = t * i - 4.86170275781575260E-007;
    t = t * i + 9.77962097401114400E-007;
    t = t * i - 2.03041287574791810E-006;
    t = t * i + 4.36119725805364580E-006;
    t = t * i - 9.43829310866446590E-006;
    t = t * i + 2.05106878496644220E-005;
    t = t * i - 4.49271383742108440E-005;
    t = t * i + 9.94570466342226000E-005;
    t = t * i - 2.23154589559238440E-004;
    t = t * i + 5.09669559149637430E-004;
    t = t * i - 1.19275392649162300E-003;
    t = t * i + 2.89051032936815490E-003;
    t = t * i - 7.38555102806811700E-003;
    t = t * i + 2.05808084278121250E-002;
    t = t * i - 6.73523010532073720E-002;
    t = t * i + 3.22467033424113040E-001;
    t = t * i + 4.22784335098467190E-001;
    t = t * i;
  } else if (fa >= 0.7) {
    i = 1.0 - fa;
    t =         1.17786911519331130E-002;  
    t = t * i + 3.89046747413522300E-002;
    t = t * i + 5.90045711362049900E-002;
    t = t * i + 6.02143305254344420E-002;
    t = t * i + 5.61652708964839180E-002;
    t = t * i + 5.75052755193461370E-002;
    t = t * i + 6.21061973447320710E-002;
    t = t * i + 6.67614724532521880E-002;
    t = t * i + 7.14856037245421020E-002;
    t = t * i + 7.69311251313347100E-002;
    t = t * i + 8.33503129714946310E-002;
    t = t * i + 9.09538288991182800E-002;
    t = t * i + 1.00099591546322310E-001;
    t = t * i + 1.11334278141734510E-001;
    t = t * i + 1.25509666613462880E-001;
    t = t * i + 1.44049896457704160E-001;
    t = t * i + 1.69557177031481600E-001;
    t = t * i + 2.07385551032182120E-001;
    t = t * i + 2.70580808427600350E-001;
    t = t * i + 4.00685634386517050E-001;
    t = t * i + 8.22467033424113540E-001;
    t = t * i + 5.77215664901532870E-001;
    t = t * i;
  } else {
    t =         -9.04051686831357990E-008;
    t = t * fa + 7.06814224969349250E-007;
    t = t * fa - 3.80702154637902830E-007;
    t = t * fa - 2.12880892189316100E-005;
    t = t * fa + 1.29108470307156190E-004;
    t = t * fa - 2.15932815215386580E-004;
    t = t * fa - 1.16484324388538480E-003;
    t = t * fa + 7.21883433044470670E-003;
    t = t * fa - 9.62194579514229560E-003;
    t = t * fa - 4.21977386992884450E-002;
    t = t * fa + 1.66538611813682460E-001;
    t = t * fa - 4.20026350606819980E-002;
    t = t * fa - 6.55878071519427450E-001;
    t = t * fa + 5.77215664901523870E-001;
    t = t * fa;
    t = t * fa + fa;
    t = -log (t);
  }
  if (a >= 0.0) return t;
  if (fa < 1e-19) return -log(fa);
  i = floor(fa);       
  if (fa == i) return 1.0 / (fa - i); /* a is an integer: return infinity */
  i = rint (2.0 * fa);
  quot = (long long int)i;
  i = fa - 0.5 * i;
  i = i * CUDART_PI;
  if (quot & 1) {
    i = cos(i);
  } else {
    i = sin(i);
  }
  i = fabs(i);
  t = log(CUDART_PI / (i * fa)) - t;
  return t;
}

__func__(unsigned long long int __internal_host_nan_kernel(const char *s))
{
  unsigned long long i = 0;
  int c;
  int ovfl = 0;
  int invld = 0;
  if (s && (*s == '0')) {
    s++;
    if ((*s == 'x') || (*s == 'X')) {
      s++; 
      while (*s == '0') s++;
      while (*s) {
        if (i > 0x0fffffffffffffffULL) {
          ovfl = 1;
        }
        c = (((*s) >= 'A') && ((*s) <= 'F')) ? (*s + 'a' - 'A') : (*s);
        if ((c >= 'a') && (c <= 'f')) { 
          c = c - 'a' + 10;
          i = i * 16 + c;
        } else if ((c >= '0') && (c <= '9')) { 
          c = c - '0';
          i = i * 16 + c;
        } else {
          invld = 1;
        }
        s++;
      }
    } else {
      while (*s == '0') s++;
      while (*s) {
        if (i > 0x1fffffffffffffffULL) {
          ovfl = 1;
        }
        c = *s;
        if ((c >= '0') && (c <= '7')) { 
          c = c - '0';
          i = i * 8 + c;
        } else {
          invld = 1; 
        }
        s++;
      }
    }
  } else if (s) {
    while (*s) {
      c = *s;
      if ((i > 1844674407370955161ULL) || 
          ((i == 1844674407370955161ULL) && (c > '5'))) {
        ovfl = 1;
      }
      if ((c >= '0') && (c <= '9')) { 
        c = c - '0';
        i = i * 10 + c;
      } else {
        invld = 1;
      }
      s++;
    }
  }
  if (ovfl) {
    i = ~0ULL;
  }
  if (invld) {
    i = 0ULL;
  }
  i = (i & 0x000fffffffffffffULL) | 0x7ff8000000000000ULL;
  return i;
}

__func__(double nan(const char *tagp))
{
  volatile union {
    unsigned long long l;
    double d;
  } cvt;

  cvt.l = __internal_host_nan_kernel(tagp);
  return cvt.d;
}

__func__(double __host_tgamma_kernel(double a))
{
  double t;
  t =       - 4.4268934071252475E-010;
  t = t * a - 2.0266591846658954E-007;
  t = t * a + 1.1381211721119527E-006;
  t = t * a - 1.2507734816630748E-006;
  t = t * a - 2.0136501740408771E-005;
  t = t * a + 1.2805012607354486E-004;
  t = t * a - 2.1524140811527418E-004;
  t = t * a - 1.1651675459704604E-003;
  t = t * a + 7.2189432248466381E-003;
  t = t * a - 9.6219715326862632E-003;
  t = t * a - 4.2197734554722394E-002;
  t = t * a + 1.6653861138250356E-001;
  t = t * a - 4.2002635034105444E-002;
  t = t * a - 6.5587807152025712E-001;
  t = t * a + 5.7721566490153287E-001;
  t = t * a + 1.0000000000000000E+000;
  return t;
}

__func__(double __host_stirling_poly(double a))
{
  double x = 1.0 / a;
  double z = 0.0;
  z =       + 8.3949872067208726e-004;
  z = z * x - 5.1717909082605919e-005;
  z = z * x - 5.9216643735369393e-004;
  z = z * x + 6.9728137583658571e-005;
  z = z * x + 7.8403922172006662e-004;
  z = z * x - 2.2947209362139917e-004;
  z = z * x - 2.6813271604938273e-003;
  z = z * x + 3.4722222222222220e-003;
  z = z * x + 8.3333333333333329e-002;
  z = z * x + 1.0000000000000000e+000;
  return z;
}

__func__(double __host_tgamma_stirling(double a))
{
  double z;
  double x;
  z = __host_stirling_poly (a);
  if (a < 142.0) {
    x = pow (a, a - 0.5);
    a = x * exp (-a);
    a = a * CUDART_SQRT_2PI;
    return a * z;
  } else if (a < 172.0) {
    x = pow (a, 0.5 * a - 0.25);
    a = x * exp (-a);
    a = a * CUDART_SQRT_2PI;
    a = a * z;
    return a * x;
  } else {
    return exp(1000.0); /* INF */
  }
}

__func__(double tgamma(double a))
{
  double s, xx, x = a;
  if (__isnan(a)) {
    return a + a;
  }
  if (fabs(x) < 20.0) {
    if (x >= 0.0) {
      s = 1.0;
      xx = x;
      while (xx > 1.5) {
        xx = xx - 1.0;
        s = s * xx;
      }
      if (x >= 0.5) {
        xx = xx - 1.0;
      }
      xx = __host_tgamma_kernel (xx);
      if (x < 0.5) {
        xx = xx * x;
      }
      s = s / xx;
    } else {
      xx = x;
      s = xx;
      if (x == floor(x)) {
        return 0.0 / (x - floor(x));
      }
      while (xx < -0.5) {
        xx = xx + 1.0;
        s = s * xx;
      }
      xx = __host_tgamma_kernel (xx);
      s = s * xx;
      s = 1.0 / s;
    }
    return s;
  } else {
    if (x >= 0.0) {
      return __host_tgamma_stirling (x);
    } else {
      double t;
      int quot;
      if (x == floor(x)) {
        return 0.0 / (x - floor(x));
      }
      if (x < -185.0) {
        int negative;
        x = floor(x);
        negative = ((x - (2.0 * floor(0.5 * x))) == 1.0);
        return negative ? (-1.0 / 1e308 / 1e308) : CUDART_ZERO;
      }
      /* compute sin(pi*x) accurately */
      xx = rint (2.0 * x);
      quot = (int)xx;
      xx = -0.5 * xx + x;
      xx = xx * CUDART_PI;
      if (quot & 1) {
        xx = cos (xx);
      } else {
        xx = sin (xx);
      }
      if (quot & 2) {
        xx = -xx;
      }
      x = fabs (x);
      s = exp (-x);
      t = x - 0.5;
      if (x > 140.0) t = 0.5 * t;
      t = pow (x, t);
      if (x > 140.0) s = s * t;
      s = s * __host_stirling_poly (x);
      s = s * x;
      s = s * xx;
      s = 1.0 / s;
      s = s * CUDART_SQRT_PIO2;
      s = s / t;
      return s;
    }
  }
}
#endif /* _MSC_VER < 1800 */

#if _MSC_VER < 1600

__func__(double hypot(double a, double b))
{
  return _hypot(a, b);
}

#endif /* _MSC_VER < 1600 */

/*******************************************************************************
*                                                                              *
* HOST IMPLEMENTATION FOR FLOAT AND LONG DOUBLE ROUTINES FOR WINDOWS PLATFORM  *
* MAP FLOAT AND LONG DOUBLE ROUTINES TO DOUBLE ROUTINES                        *
*                                                                              *
*******************************************************************************/

__func__(int __signbitl(long double a))
{
  return __signbit((double)a);
}

__func__(int __signbitf(float a))
{
  return __signbit((double)a);
}

__func__(int __finitel(long double a))
{
  return __finite((double)a);
}

__func__(int __finitef(float a))
{
  return __finite((double)a);
}

__func__(int __isinfl(long double a))
{
  return __isinf((double)a);
}

__func__(int __isinff(float a))
{
  return __isinf((double)a);
}

__func__(int __isnanl(long double a))
{
  return __isnan((double)a);
}

__func__(int __isnanf(float a))
{
  return __isnan((double)a);
}

#if _MSC_VER < 1800
__func__(float fmaxf(float a, float b))
{
  return (float)fmax((double)a, (double)b);
}

__func__(float fminf(float a, float b))
{
  return (float)fmin((double)a, (double)b);
}

__func__(float roundf(float a))
{
  return (float)round((double)a);
}

__func__(long int lroundf(float a))
{
  return lround((double)a);
}

__func__(long long int llroundf(float a))
{
  return llround((double)a);
}

__func__(float truncf(float a))
{
  return (float)trunc((double)a);
}

__func__(float rintf(float a))
{
  return (float)rint((double)a);
}

__func__(float nearbyintf(float a))
{
  return (float)nearbyint((double)a);
}

__func__(long int lrintf(float a))
{
  return lrint((double)a);
}

__func__(long long int llrintf(float a))
{
  return llrint((double)a);
}

__func__(float logbf(float a))
{
  return (float)logb((double)a);
}

__func__(float scalblnf(float a, long int b))
{
  return (float)scalbln((double)a, b);
}

__func__(float log2f(float a))
{
  return (float)log2((double)a);
}

__func__(float exp2f(float a))
{
  return (float)exp2((double)a);
}

__func__(float acoshf(float a))
{
  return (float)acosh((double)a);
}

__func__(float asinhf(float a))
{
  return (float)asinh((double)a);
}

__func__(float atanhf(float a))
{
  return (float)atanh((double)a);
}

__func__(float cbrtf(float a))
{
  return (float)cbrt((double)a);
}

__func__(float expm1f(float a))
{
  return (float)expm1((double)a);
}

__func__(float fdimf(float a, float b))
{
  return (float)fdim((double)a, (double)b);
}

__func__(float log1pf(float a))
{
  return (float)log1p((double)a);
}

__func__(float scalbnf(float a, int b))
{
  return (float)scalbn((double)a, b);
}

__func__(float fmaf(float a, float b, float c))
{
  return (float)fma((double)a, (double)b, (double)c);
}

__func__(int ilogbf(float a))
{
  return ilogb((double)a);
}

__func__(float erff(float a))
{
  return (float)erf((double)a);
}

__func__(float erfcf(float a))
{
  return (float)erfc((double)a);
}

__func__(float lgammaf(float a))
{
  return (float)lgamma((double)a);
}

__func__(float tgammaf(float a))
{
  return (float)tgamma((double)a);
}

__func__(float remquof(float a, float b, int *quo))
{
  return (float)remquo((double)a, (double)b, quo);
}

__func__(float remainderf(float a, float b))
{
  return (float)remainder((double)a, (double)b);
}
#endif /* _MSC_VER < 1800 */

/*******************************************************************************
*                                                                              *
* HOST IMPLEMENTATION FOR FLOAT ROUTINES FOR WINDOWS PLATFORM                  *
*                                                                              *
*******************************************************************************/

#if _MSC_VER < 1600

__func__(float hypotf(float a, float b))
{
  return _hypotf(a, b);
}

#endif /* _MSC_VER < 1600 */

#if _MSC_VER < 1800
__func__(float copysignf(float a, float b))
{
  volatile union {
    float f;
    unsigned int i;
  } aa, bb;

  aa.f = a;
  bb.f = b;
  aa.i = (aa.i & ~0x80000000) | (bb.i & 0x80000000);
  return aa.f;
}

__func__(float nextafterf(float a, float b))
{
  volatile union {
    float f;
    unsigned int i;
  } cvt;
  unsigned int ia;
  unsigned int ib;
  cvt.f = a;
  ia = cvt.i;
  cvt.f = b;
  ib = cvt.i;
  if (__isnanf(a) || __isnanf(b)) return a + b; /*NaN*/
  if (((ia | ib) << 1) == 0) return b;
  if (a == 0.0f) {
    return copysignf(1.401298464e-045f, b); /*crossover*/
  }
  if ((a < b) && (a < 0.0f)) ia--;
  if ((a < b) && (a > 0.0f)) ia++;
  if ((a > b) && (a < 0.0f)) ia++;
  if ((a > b) && (a > 0.0f)) ia--;
  cvt.i = ia;
  return cvt.f;
}

__func__(float nanf(const char *tagp))
{
  volatile union {
    float f;
    unsigned int i;
  } cvt;
  
  cvt.i = (unsigned int)__internal_host_nan_kernel(tagp);
  cvt.i = (cvt.i & 0x007fffff) | 0x7fc00000;
  return cvt.f;
}

#endif /* _MSC_VER < 1800 */

#endif /* _WIN32 */

/*******************************************************************************
*                                                                              *
* HOST IMPLEMENTATION FOR DOUBLE AND FLOAT ROUTINES. ALL PLATFORMS             *
*                                                                              *
*******************************************************************************/

__func__(double rsqrt(double a))
{
  return 1.0 / sqrt(a);
}

__func__(double rcbrt(double a))
{
  double s, t;

  if (__isnan(a)) {
    return a + a;
  }
  if (a == 0.0 || __isinf(a)) {
    return 1.0 / a;
  } 
  s = fabs(a);
  t = exp2(-CUDART_THIRD * log2(s));                /* initial approximation */
  t = ((t*t) * (-s*t) + 1.0) * (CUDART_THIRD*t) + t;/* refine approximation */
#if defined(__APPLE__)
  if (__signbitd(a))
#else /* __APPLE__ */
  if (__signbit(a))
#endif /* __APPLE__ */
  {
    t = -t;
  }
  return t;
}

__func__(double sinpi(double a))
{
  int n;

  if (__isnan(a)) {
    return a + a;
  }
  if (a == 0.0 || __isinf(a)) {
    return sin (a);
  } 
  if (a == floor(a)) {
    return ((a / 1.0e308) / 1.0e308) / 1.0e308;
  }
  a = remquo (a, 0.5, &n);
  a = a * CUDART_PI;
  if (n & 1) {
    a = cos (a);
  } else {
    a = sin (a);
  }
  if (n & 2) {
    a = -a;
  }
  return a;
}

__func__(double cospi(double a))
{
  int n;

  if (__isnan(a)) {
    return a + a;
  }
  if (__isinf(a)) {
    return cos (a);
  } 
  if (fabs(a) > 9.0071992547409920e+015) {
    a = 0.0;
  }
  a = remquo (a, 0.5, &n);
  a = a * CUDART_PI;
  n++;
  if (n & 1) {
    a = cos (a);
  } else {
    a = sin (a);
  }
  if (n & 2) {
    a = -a;
  }
  if (a == 0.0) {
    a = fabs(a);
  }
  return a;
}

__func__(void sincospi(double a, double *sptr, double *cptr))
{
  *sptr = sinpi(a);
  *cptr = cospi(a);
}

__func__(double erfinv(double a))
{
  double p, q, t, fa;
  volatile union {
    double d;
    unsigned long long int l;
  } cvt;

  fa = fabs(a);
  if (fa >= 1.0) {
    cvt.l = 0xfff8000000000000ull;
    t = cvt.d;                    /* INDEFINITE */
    if (fa == 1.0) {
      t = a * exp(1000.0);        /* Infinity */
    }
  } else if (fa >= 0.9375) {
    /* Based on: J.M. Blair, C.A. Edwards, J.H. Johnson: Rational Chebyshev
       Approximations for the Inverse of the Error Function. Mathematics of
       Computation, Vol. 30, No. 136 (Oct. 1976), pp. 827-830. Table 59
     */
    t = log1p(-fa);
    t = 1.0 / sqrt(-t);
    p =         2.7834010353747001060e-3;
    p = p * t + 8.6030097526280260580e-1;
    p = p * t + 2.1371214997265515515e+0;
    p = p * t + 3.1598519601132090206e+0;
    p = p * t + 3.5780402569085996758e+0;
    p = p * t + 1.5335297523989890804e+0;
    p = p * t + 3.4839207139657522572e-1;
    p = p * t + 5.3644861147153648366e-2;
    p = p * t + 4.3836709877126095665e-3;
    p = p * t + 1.3858518113496718808e-4;
    p = p * t + 1.1738352509991666680e-6;
    q =     t + 2.2859981272422905412e+0;
    q = q * t + 4.3859045256449554654e+0;
    q = q * t + 4.6632960348736635331e+0;
    q = q * t + 3.9846608184671757296e+0;
    q = q * t + 1.6068377709719017609e+0;
    q = q * t + 3.5609087305900265560e-1;
    q = q * t + 5.3963550303200816744e-2;
    q = q * t + 4.3873424022706935023e-3;
    q = q * t + 1.3858762165532246059e-4;
    q = q * t + 1.1738313872397777529e-6;
    t = p / (q * t);
    if (a < 0.0) t = -t;
  } else if (fa >= 0.75) {
    /* Based on: J.M. Blair, C.A. Edwards, J.H. Johnson: Rational Chebyshev
       Approximations for the Inverse of the Error Function. Mathematics of
       Computation, Vol. 30, No. 136 (Oct. 1976), pp. 827-830. Table 39
    */
    t = a * a - .87890625;
    p =         .21489185007307062000e+0;
    p = p * t - .64200071507209448655e+1;
    p = p * t + .29631331505876308123e+2;
    p = p * t - .47644367129787181803e+2;
    p = p * t + .34810057749357500873e+2;
    p = p * t - .12954198980646771502e+2;
    p = p * t + .25349389220714893917e+1;
    p = p * t - .24758242362823355486e+0;
    p = p * t + .94897362808681080020e-2;
    q =     t - .12831383833953226499e+2;
    q = q * t + .41409991778428888716e+2;
    q = q * t - .53715373448862143349e+2;
    q = q * t + .33880176779595142685e+2;
    q = q * t - .11315360624238054876e+2;
    q = q * t + .20369295047216351160e+1;
    q = q * t - .18611650627372178511e+0;
    q = q * t + .67544512778850945940e-2;
    p = p / q;
    t = a * p;
  } else {
    /* Based on: J.M. Blair, C.A. Edwards, J.H. Johnson: Rational Chebyshev
       Approximations for the Inverse of the Error Function. Mathematics of
       Computation, Vol. 30, No. 136 (Oct. 1976), pp. 827-830. Table 18
    */
    t = a * a - .5625;
    p =       - .23886240104308755900e+2;
    p = p * t + .45560204272689128170e+3;
    p = p * t - .22977467176607144887e+4;
    p = p * t + .46631433533434331287e+4;
    p = p * t - .43799652308386926161e+4;
    p = p * t + .19007153590528134753e+4;
    p = p * t - .30786872642313695280e+3;
    q =     t - .83288327901936570000e+2;
    q = q * t + .92741319160935318800e+3;
    q = q * t - .35088976383877264098e+4;
    q = q * t + .59039348134843665626e+4;
    q = q * t - .48481635430048872102e+4;
    q = q * t + .18997769186453057810e+4;
    q = q * t - .28386514725366621129e+3;
    p = p / q;
    t = a * p;
  }
  return t;
}

__func__(double erfcinv(double a))
{
  double t;
  volatile union {
    double d;
    unsigned long long int l;
  } cvt;

  if (__isnan(a)) {
    return a + a;
  }
  if (a <= 0.0) {
    cvt.l = 0xfff8000000000000ull;
    t = cvt.d;                        /* INDEFINITE */
    if (a == 0.0) {
        t = (1.0 - a) * exp(1000.0);  /* Infinity */
    }
  } 
  else if (a >= 0.0625) {
    t = erfinv (1.0 - a);
  }
  else if (a >= 1e-100) {
    /* Based on: J.M. Blair, C.A. Edwards, J.H. Johnson: Rational Chebyshev
       Approximations for the Inverse of the Error Function. Mathematics of
       Computation, Vol. 30, No. 136 (Oct. 1976), pp. 827-830. Table 59
    */
    double p, q;
    t = log(a);
    t = 1.0 / sqrt(-t);
    p =         2.7834010353747001060e-3;
    p = p * t + 8.6030097526280260580e-1;
    p = p * t + 2.1371214997265515515e+0;
    p = p * t + 3.1598519601132090206e+0;
    p = p * t + 3.5780402569085996758e+0;
    p = p * t + 1.5335297523989890804e+0;
    p = p * t + 3.4839207139657522572e-1;
    p = p * t + 5.3644861147153648366e-2;
    p = p * t + 4.3836709877126095665e-3;
    p = p * t + 1.3858518113496718808e-4;
    p = p * t + 1.1738352509991666680e-6;
    q =     t + 2.2859981272422905412e+0;
    q = q * t + 4.3859045256449554654e+0;
    q = q * t + 4.6632960348736635331e+0;
    q = q * t + 3.9846608184671757296e+0;
    q = q * t + 1.6068377709719017609e+0;
    q = q * t + 3.5609087305900265560e-1;
    q = q * t + 5.3963550303200816744e-2;
    q = q * t + 4.3873424022706935023e-3;
    q = q * t + 1.3858762165532246059e-4;
    q = q * t + 1.1738313872397777529e-6;
    t = p / (q * t);
  }
  else {
    /* Based on: J.M. Blair, C.A. Edwards, J.H. Johnson: Rational Chebyshev
       Approximations for the Inverse of the Error Function. Mathematics of
       Computation, Vol. 30, No. 136 (Oct. 1976), pp. 827-830. Table 82
    */
    double p, q;
    t = log(a);
    t = 1.0 / sqrt(-t);
    p =         6.9952990607058154858e-1;
    p = p * t + 1.9507620287580568829e+0;
    p = p * t + 8.2810030904462690216e-1;
    p = p * t + 1.1279046353630280005e-1;
    p = p * t + 6.0537914739162189689e-3;
    p = p * t + 1.3714329569665128933e-4;
    p = p * t + 1.2964481560643197452e-6;
    p = p * t + 4.6156006321345332510e-9;
    p = p * t + 4.5344689563209398450e-12;
    q =     t + 1.5771922386662040546e+0;
    q = q * t + 2.1238242087454993542e+0;
    q = q * t + 8.4001814918178042919e-1;
    q = q * t + 1.1311889334355782065e-1;
    q = q * t + 6.0574830550097140404e-3;
    q = q * t + 1.3715891988350205065e-4;
    q = q * t + 1.2964671850944981713e-6;
    q = q * t + 4.6156017600933592558e-9;
    q = q * t + 4.5344687377088206783e-12;
    t = p / (q * t);
  }
  return t;
}

__func__(double normcdfinv(double a))
{
  return -1.4142135623730951 * erfcinv(a + a);
}

__func__(double normcdf(double a))
{
  double ah, al, t1, t2, u1, u2, v1, v2, z;
  if (fabs (a) > 38.5) a = copysign (38.5, a);
  ah = a * 134217729.0;
  u1 = (a - ah) + ah;
  u2 = a - u1;
  v1 = -7.0710678398609161e-01;
  v2 =  2.7995440410322203e-09;
  t1 = a * -CUDART_SQRT_HALF_HI;
  t2 = (((u1 * v1 - t1) + u1 * v2) + u2 * v1) + u2 * v2;
  t2 = (a * -CUDART_SQRT_HALF_LO) + t2;
  ah = t1 + t2;
  z = erfc (ah);
  if (a < -1.0) {
    al = (t1 - ah) + t2;
    t1 = -2.0 * ah * z;
    z = t1 * al + z;
  }
  return 0.5 * z;
}

__func__(double erfcx(double a))
{
  double x, t1, t2, t3;

  if (__isnan(a)) {
    return a + a;
  }
  x = fabs(a); 
  if (x < 32.0) {
    /*  
     * This implementation of erfcx() is based on the algorithm in: M. M. 
     * Shepherd and J. G. Laframboise, "Chebyshev Approximation of (1 + 2x)
     * exp(x^2)erfc x in 0 <= x < INF", Mathematics of Computation, Vol. 
     * 36, No. 153, January 1981, pp. 249-253. For the core approximation,
     * the input domain [0,INF] is transformed via (x-k) / (x+k) where k is
     * a precision-dependent constant. Here, we choose k = 4.0, so the input 
     * domain [0, 27.3] is transformed into the core approximation domain 
     * [-1, 0.744409].   
     */  
    /* (1+2*x)*exp(x*x)*erfc(x) */ 
    /* t2 = (x-4.0)/(x+4.0), transforming [0,INF] to [-1,+1] */ 
    t1 = x - 4.0; 
    t2 = x + 4.0; 
    t2 = t1 / t2;
    /* approximate on [-1, 0.744409] */   
    t1 =         - 3.5602694826817400E-010; 
    t1 = t1 * t2 - 9.7239122591447274E-009; 
    t1 = t1 * t2 - 8.9350224851649119E-009; 
    t1 = t1 * t2 + 1.0404430921625484E-007; 
    t1 = t1 * t2 + 5.8806698585341259E-008; 
    t1 = t1 * t2 - 8.2147414929116908E-007; 
    t1 = t1 * t2 + 3.0956409853306241E-007; 
    t1 = t1 * t2 + 5.7087871844325649E-006; 
    t1 = t1 * t2 - 1.1231787437600085E-005; 
    t1 = t1 * t2 - 2.4399558857200190E-005; 
    t1 = t1 * t2 + 1.5062557169571788E-004; 
    t1 = t1 * t2 - 1.9925637684786154E-004; 
    t1 = t1 * t2 - 7.5777429182785833E-004; 
    t1 = t1 * t2 + 5.0319698792599572E-003; 
    t1 = t1 * t2 - 1.6197733895953217E-002; 
    t1 = t1 * t2 + 3.7167515553018733E-002; 
    t1 = t1 * t2 - 6.6330365827532434E-002; 
    t1 = t1 * t2 + 9.3732834997115544E-002; 
    t1 = t1 * t2 - 1.0103906603555676E-001; 
    t1 = t1 * t2 + 6.8097054254735140E-002; 
    t1 = t1 * t2 + 1.5379652102605428E-002; 
    t1 = t1 * t2 - 1.3962111684056291E-001; 
    t1 = t1 * t2 + 1.2329951186255526E+000; 
    /* (1+2*x)*exp(x*x)*erfc(x) / (1+2*x) = exp(x*x)*erfc(x) */  
    t2 = 2.0 * x + 1.0; 
    t1 = t1 / t2;
  } else {
    /* asymptotic expansion for large aguments */
    t2 = 1.0 / x;
    t3 = t2 * t2;
    t1 =         -29.53125;
    t1 = t1 * t3 + 6.5625;
    t1 = t1 * t3 - 1.875;
    t1 = t1 * t3 + 0.75;
    t1 = t1 * t3 - 0.5;
    t1 = t1 * t3 + 1.0;
    t2 = t2 * 5.6418958354775628e-001;
    t1 = t1 * t2;
  }
  if (a < 0.0) {
    /* erfcx(x) = 2*exp(x^2) - erfcx(|x|) */
    t2 = ((int)(x * 16.0)) * 0.0625;
    t3 = (x - t2) * (x + t2);
    t3 = exp(t2 * t2) * exp(t3);
    t3 = t3 + t3;
    t1 = t3 - t1;
  }
  return t1;
}

__func__(float rsqrtf(float a))
{
  return (float)rsqrt((double)a);
}

__func__(float rcbrtf(float a))
{
  return (float)rcbrt((double)a);
}

__func__(float sinpif(float a))
{
  return (float)sinpi((double)a);
}

__func__(float cospif(float a))
{
  return (float)cospi((double)a);
}

__func__(void sincospif(float a, float *sptr, float *cptr))
{
  double s, c;

  sincospi((double)a, &s, &c);
  *sptr = (float)s;
  *cptr = (float)c;
}

__func__(float erfinvf(float a))
{
  return (float)erfinv((double)a);
}

__func__(float erfcinvf(float a))
{
  return (float)erfcinv((double)a);
}

__func__(float normcdfinvf(float a))
{
  return (float)normcdfinv((double)a);
}

__func__(float normcdff(float a))
{
  return (float)normcdf((double)a);
}

__func__(float erfcxf(float a))
{
  return (float)erfcx((double)a);
}

/*******************************************************************************
*                                                                              *
* HOST IMPLEMENTATION FOR UTILITY ROUTINES. ALL PLATFORMS                      *
*                                                                              *
*******************************************************************************/

__func__(int min(int a, int b))
{
  return a < b ? a : b;
}

__func__(unsigned int umin(unsigned int a, unsigned int b))
{
  return a < b ? a : b;
}

__func__(long long int llmin(long long int a, long long int b))
{
  return a < b ? a : b;
}

__func__(unsigned long long int ullmin(unsigned long long int a, unsigned long long int b))
{
  return a < b ? a : b;
}

__func__(int max(int a, int b))
{
  return a > b ? a : b;
}

__func__(unsigned int umax(unsigned int a, unsigned int b))
{
  return a > b ? a : b;
}

__func__(long long int llmax(long long int a, long long int b))
{
  return a > b ? a : b;
}

__func__(unsigned long long int ullmax(unsigned long long int a, unsigned long long int b))
{
  return a > b ? a : b;
}

#if defined(_WIN32)
#pragma warning(default: 4211)
#endif /* _WIN32 */

#endif /* __CUDABE__ */

#endif /* __cplusplus && __CUDACC__ */

#if defined(CUDA_DOUBLE_MATH_FUNCTIONS) && defined(CUDA_FLOAT_MATH_FUNCTIONS)

#error -- conflicting mode for double math routines

#endif /* CUDA_DOUBLE_MATH_FUNCTIONS && CUDA_FLOAT_MATH_FUNCTIONS */

#if defined(CUDA_FLOAT_MATH_FUNCTIONS)

#include "math_functions_dbl_ptx1.h"

#endif /* CUDA_FLOAT_MATH_FUNCTIONS */

#if defined(CUDA_DOUBLE_MATH_FUNCTIONS)

#include "math_functions_dbl_ptx3.h"

#endif /* CUDA_DOUBLE_MATH_FUNCTIONS */

#endif /* !__MATH_FUNCTIONS_H__ */
