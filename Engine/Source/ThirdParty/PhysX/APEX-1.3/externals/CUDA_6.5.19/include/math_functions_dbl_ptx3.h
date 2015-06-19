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

#if !defined(__MATH_FUNCTIONS_DBL_PTX3_H__)
#define __MATH_FUNCTIONS_DBL_PTX3_H__

/* True double precision implementations, since native double support */

#if defined(__CUDABE__)

#if defined(__CUDANVVM__)

/*******************************************************************************
*                                                                              *
* DEVICE IMPLEMENTATIONS FOR FUNCTIONS WITH BUILTIN NVOPENCC OPERATIONS        *
*                                                                              *
*******************************************************************************/

static __forceinline__ double rint(double a)
{
  return __nv_rint(a);
}

static __forceinline__ long int lrint(double a)
{
#if defined(__LP64__)
  return (long int)__double2ll_rn(a);
#else /* __LP64__ */
  return (long int)__double2int_rn(a);
#endif /* __LP64__ */
}

static __forceinline__ long long int llrint(double a)
{
  return __nv_llrint(a);
}

static __forceinline__ double nearbyint(double a)
{
  return __nv_nearbyint(a);
}

/*******************************************************************************
*                                                                              *
* DEVICE IMPLEMENTATIONS FOR FUNCTIONS WITHOUT BUILTIN NVOPENCC OPERATIONS     *
*                                                                              *
*******************************************************************************/

static __forceinline__ int __signbitd(double a)
{
  return __nv_signbitd(a);
}

static __forceinline__ int __isfinited(double a)
{
  return __nv_isfinited(a);
}

static __forceinline__ int __isinfd(double a)
{
  return __nv_isinfd(a);
}

static __forceinline__ int __isnand(double a)
{
  return __nv_isnand(a);
}

#if defined(__APPLE__)

static __forceinline__ int __signbitl(/* we do not support long double yet, hence double */double a)
{
  return __signbitd((double)a);
}

static __forceinline__ int __isfinite(/* we do not support long double yet, hence double */double a)
{
  return __isfinited((double)a);
}

static __forceinline__ int __isinf(/* we do not support long double yet, hence double */double a)
{
  return __isinfd((double)a);
}

static __forceinline__ int __isnan(/* we do not support long double yet, hence double */double a)
{
  return __isnand((double)a);
}

#else /* __APPLE__ */

static __forceinline__ int __signbit(double a)
{
  return __signbitd(a);
}

static __forceinline__ int __signbitl(/* we do not support long double yet, hence double */double a)
{
  return __signbit((double)a);
}

static __forceinline__ int __finite(double a)
{
  return __isfinited(a);
}

static __forceinline__ int __finitel(/* we do not support long double yet, hence double */double a)
{
  return __finite((double)a);
}

static __forceinline__ int __isinf(double a)
{
  return __isinfd(a);
}

static __forceinline__ int __isinfl(/* we do not support long double yet, hence double */double a)
{
  return __isinf((double)a);
}

static __forceinline__ int __isnan(double a)
{
  return __isnand(a);
}

static __forceinline__ int __isnanl(/* we do not support long double yet, hence double */double a)
{
  return __isnan((double)a);
}

#endif /* __APPLE__ */

static __forceinline__ double copysign(double a, double b)
{
  return __nv_copysign(a, b);
}

static __forceinline__ void sincos(double a, double *sptr, double *cptr)
{
  __nv_sincos(a, sptr, cptr);
}

static __forceinline__ void sincospi(double a, double *sptr, double *cptr)
{
  __nv_sincospi(a, sptr, cptr);
}

static __forceinline__ double sin(double a)
{
  return __nv_sin(a);
}

static __forceinline__ double cos(double a)
{
  return __nv_cos(a);
}

static __forceinline__ double sinpi(double a)
{
  return __nv_sinpi(a);
}

static __forceinline__ double cospi(double a)
{
  return __nv_cospi(a);
}

static __forceinline__ double tan(double a)
{
  return __nv_tan(a);
}

static __forceinline__ double log(double a)
{
  return __nv_log(a);
}

static __forceinline__ double log2(double a)
{
  return __nv_log2(a);
}

static __forceinline__ double log10(double a)
{
  return __nv_log10(a);
}

static __forceinline__ double log1p(double a)
{
  return __nv_log1p(a);
}

static __forceinline__ double exp(double a)
{
  return __nv_exp(a);
}

static __forceinline__ double exp2(double a)
{
  return __nv_exp2(a);
}

static __forceinline__ double exp10(double a)
{
  return __nv_exp10(a);
}

static __forceinline__ double expm1(double a)
{
  return __nv_expm1(a);
}

static __forceinline__ double cosh(double a)
{
  return __nv_cosh(a);
}

static __forceinline__ double sinh(double a)
{
  return __nv_sinh(a);
}

static __forceinline__ double tanh(double a)
{
  return __nv_tanh(a);
}

static __forceinline__ double atan2(double a, double b)
{
  return __nv_atan2(a, b);
}

static __forceinline__ double atan(double a)
{
  return __nv_atan(a);
}

static __forceinline__ double asin(double a)
{
  return __nv_asin(a);
}

static __forceinline__ double acos(double a)
{
  return __nv_acos(a);
}

static __forceinline__ double acosh(double a)
{
  return __nv_acosh(a);
}

static __forceinline__ double asinh(double a)
{
  return __nv_asinh(a);  
}

static __forceinline__ double atanh(double a)
{
  return __nv_atanh(a);
}

static __forceinline__ double hypot(double a, double b)
{
  return __nv_hypot(a, b);
}

static __forceinline__ double rhypot(double a, double b)
{
  return __nv_rhypot(a, b);
}

static __forceinline__ double cbrt(double a)
{
  return __nv_cbrt(a);
}

static __forceinline__ double rcbrt(double a)
{
  return __nv_rcbrt(a);
}

static __forceinline__ double pow(double a, double b)
{
  return __nv_pow(a, b);
}

static __forceinline__ double j0(double a)
{
  return __nv_j0(a);
}

static __forceinline__ double j1(double a)
{
  return __nv_j1(a);
}

static __forceinline__ double y0(double a)
{
  return __nv_y0(a);
}

static __forceinline__ double y1(double a)
{
  return __nv_y1(a);
}

static __forceinline__ double yn(int n, double a)
{
  return __nv_yn(n, a);
}

static __forceinline__ double jn(int n, double a)
{
  return __nv_jn(n, a);
}

static __forceinline__ double cyl_bessel_i0(double a)
{
  return __nv_cyl_bessel_i0(a);
}

static __forceinline__ double cyl_bessel_i1(double a)
{
  return __nv_cyl_bessel_i1(a);
}

static __forceinline__ double erf(double a)
{
  return __nv_erf(a);
}

static __forceinline__ double erfinv(double a)
{
  return __nv_erfinv(a);
}

static __forceinline__ double erfcinv(double a)
{
  return __nv_erfcinv(a);
}

static __forceinline__ double normcdfinv(double a)
{
  return __nv_normcdfinv(a);
}

static __forceinline__ double erfc(double a)  
{  
  return __nv_erfc(a);
}

static __forceinline__ double erfcx(double a)  
{
  return __nv_erfcx(a);
}

static __forceinline__ double normcdf(double a)
{
  return __nv_normcdf(a);
}

static __forceinline__ double tgamma(double a)
{
  return __nv_tgamma(a);
}

static __forceinline__ double lgamma(double a)
{
  return __nv_lgamma(a);
}

static __forceinline__ double ldexp(double a, int b)
{
  return __nv_ldexp(a, b);
}

static __forceinline__ double scalbn(double a, int b)
{
  return __nv_scalbn(a, b);
}

static __forceinline__ double scalbln(double a, long int b)
{
#if defined(__LP64__)
  /* clamp to integer range prior to conversion */
  if (b < -2147483648L) b = -2147483648L;
  if (b >  2147483647L) b =  2147483647L;
#endif /* __LP64__ */
  return scalbn(a, (int)b);
}

static __forceinline__ double frexp(double a, int *b)
{
  return __nv_frexp(a, b);  
}

static __forceinline__ double modf(double a, double *b)
{
  return __nv_modf(a, b);
}

static __forceinline__ double fmod(double a, double b)
{
  return __nv_fmod(a, b);
}

static __forceinline__ double remainder(double a, double b)
{
  return __nv_remainder(a, b);
}

static __forceinline__ double remquo(double a, double b, int *c)
{
  return __nv_remquo(a, b, c);
}

static __forceinline__ double nextafter(double a, double b)
{
  return __nv_nextafter(a, b);
}

static __forceinline__ double nan(const char *tagp)
{
  return __nv_nan((const signed char *) tagp);
}

static __forceinline__ double round(double a)
{
  return __nv_round(a);
}

static __forceinline__ long long int llround(double a)
{
  return __nv_llround(a);
}

static __forceinline__ long int lround(double a)
{
#if defined(__LP64__)
  return (long int)llround(a);
#else /* __LP64__ */
  return (long int)round(a);
#endif /* __LP64__ */
}

static __forceinline__ double fdim(double a, double b)
{
  return __nv_fdim(a, b);
}

static __forceinline__ int ilogb(double a)
{
  return __nv_ilogb(a);
}

static __forceinline__ double logb(double a)
{
  return __nv_logb(a);
}

static __forceinline__ double fma(double a, double b, double c)
{
  return __nv_fma(a, b, c);
}

#else /* __CUDANVVM__ */

/*******************************************************************************
*                                                                              *
* DEVICE IMPLEMENTATIONS FOR FUNCTIONS WITH BUILTIN NVOPENCC OPERATIONS        *
*                                                                              *
*******************************************************************************/

static __forceinline__ double rint(double a)
{
  return __builtin_round(a);
}

static __forceinline__ long int lrint(double a)
{
#if defined(__LP64__)
  return (long int)__double2ll_rn(a);
#else /* __LP64__ */
  return (long int)__double2int_rn(a);
#endif /* __LP64__ */
}

static __forceinline__ long long int llrint(double a)
{
  return __double2ll_rn(a);
}

static __forceinline__ double nearbyint(double a)
{
  return __builtin_round(a);
}

/*******************************************************************************
*                                                                              *
* DEVICE IMPLEMENTATIONS FOR FUNCTIONS WITHOUT BUILTIN NVOPENCC OPERATIONS     *
*                                                                              *
*******************************************************************************/

static __forceinline__ int __signbitd(double a)
{
  return (int)((unsigned int)__double2hiint(a) >> 31);
}

static __forceinline__ int __isfinited(double a)
{
  return fabs(a) < CUDART_INF;
}

static __forceinline__ int __isinfd(double a)
{
#if __CUDA_ARCH__ == 200 || __CUDA_ARCH__ == 350
  return fabs(a) == CUDART_INF;  
#else  /* __CUDA_ARCH__ == 200 || __CUDA_ARCH__ == 350 */
  int alo = __double2loint(a);
  int ahi = __double2hiint(a) & 0x7fffffff;
  return (ahi == 0x7ff00000) && (alo == 0);
#endif /* __CUDA_ARCH__ == 200 || __CUDA_ARCH__ == 350 */
}

static __forceinline__ int __isnand(double a)
{
  return !(fabs(a) <= CUDART_INF);
}

#if defined(__APPLE__)

static __forceinline__ int __signbitl(/* we do not support long double yet, hence double */double a)
{
  return __signbitd((double)a);
}

static __forceinline__ int __isfinite(/* we do not support long double yet, hence double */double a)
{
  return __isfinited((double)a);
}

static __forceinline__ int __isinf(/* we do not support long double yet, hence double */double a)
{
  return __isinfd((double)a);
}

static __forceinline__ int __isnan(/* we do not support long double yet, hence double */double a)
{
  return __isnand((double)a);
}

#else /* __APPLE__ */

static __forceinline__ int __signbit(double a)
{
  return __signbitd(a);
}

static __forceinline__ int __signbitl(/* we do not support long double yet, hence double */double a)
{
  return __signbit((double)a);
}

static __forceinline__ int __finite(double a)
{
  return __isfinited(a);
}

static __forceinline__ int __finitel(/* we do not support long double yet, hence double */double a)
{
  return __finite((double)a);
}

static __forceinline__ int __isinf(double a)
{
  return __isinfd(a);
}

static __forceinline__ int __isinfl(/* we do not support long double yet, hence double */double a)
{
  return __isinf((double)a);
}

static __forceinline__ int __isnan(double a)
{
  return __isnand(a);
}

static __forceinline__ int __isnanl(/* we do not support long double yet, hence double */double a)
{
  return __isnan((double)a);
}

#endif /* __APPLE__ */

static __forceinline__ double copysign(double a, double b)
{
  int alo, ahi, bhi;

  bhi = __double2hiint(b);
  alo = __double2loint(a);
  ahi = __double2hiint(a);
  ahi = (bhi & 0x80000000) | (ahi & ~0x80000000);
  return __hiloint2double(ahi, alo);
}

/* like copysign, but requires that argument a is positive */
static __forceinline__ double __internal_copysign_pos(double a, double b)
{
  int alo, ahi, bhi;

  bhi = __double2hiint(b);
  alo = __double2loint(a);
  ahi = __double2hiint(a);
  ahi = (bhi & 0x80000000) | ahi;
  return __hiloint2double(ahi, alo);
}

static __forceinline__ double __internal_neg(double a)
{
  int hi = __double2hiint(a);
  int lo = __double2loint(a);
  return __hiloint2double(hi ^ 0x80000000, lo);
}

static __forceinline__ double __internal_fast_scale(double a, int i)
{
  unsigned int ihi, ilo;
  ilo = __double2loint (a);
  ihi = __double2hiint (a);
  return __hiloint2double ((i << 20) + ihi, ilo);
}

static __forceinline__ double __internal_half(double a)
{
  return __internal_fast_scale (a, -1);
}

static __forceinline__ double __internal_twice(double a)
{
  return __internal_fast_scale (a, 1);
}

static __forceinline__ double __internal_fast_rcp(double a) 
{ 
  double e, y; 
#if __CUDA_ARCH__ >= 200
  asm ("rcp.approx.ftz.f64 %0,%1;" : "=d"(y) : "d"(a));
#else  /* __CUDA_ARCH__ >= 200 */
  float x; 
  asm ("cvt.rn.f32.f64     %0,%1;" : "=f"(x) : "d"(a)); 
  asm ("rcp.approx.ftz.f32 %0,%1;" : "=f"(x) : "f"(x)); 
  asm ("cvt.f64.f32        %0,%1;" : "=d"(y) : "f"(x)); 
#endif /* __CUDA_ARCH__ >= 200 */
  e = __fma_rn (-a, y, 1.0); 
  e = __fma_rn ( e, e,   e); 
  y = __fma_rn ( e, y,   y); 
  return y; 
} 

static __forceinline__ double __internal_fast_rsqrt (double a)
{
  double x, e, t;
#if __CUDA_ARCH__ >= 200
  asm ("rsqrt.approx.ftz.f64 %0, %1;" : "=d"(x) : "d"(a));
#else  /* __CUDA_ARCH__ >= 200 */
  float f;
  asm ("cvt.rn.f32.f64 %0, %1;" : "=f"(f) : "d"(a));
  asm ("rsqrt.approx.ftz.f32 %0, %1;" : "=f"(f) : "f"(f));
  asm ("cvt.f64.f32 %0, %1;" : "=d"(x) : "f"(f));
#endif /* __CUDA_ARCH__ >= 200 */
  t = __dmul_rn (x, x);
  e = __fma_rn (a, -t, 1.0);
  t = __fma_rn (0.375, e, 0.5);
  e = __dmul_rn (e, x);
  x = __fma_rn (t, e, x);
  return x;
} 

static __forceinline__ double __internal_fast_sqrt (double a)
{
  double x;
#if __CUDA_ARCH__ >= 200
  asm ("rsqrt.approx.ftz.f64 %0, %1;" : "=d"(x) : "d"(a));
#else  /* __CUDA_ARCH__ >= 200 */
  float f;
  asm ("cvt.rn.f32.f64 %0, %1;" : "=f"(f) : "d"(a));
  asm ("rsqrt.approx.ftz.f32 %0, %1;" : "=f"(f) : "f"(f));
  asm ("cvt.f64.f32 %0, %1;" : "=d"(x) : "f"(f));
#endif /* __CUDA_ARCH__ >= 200 */
  /* A. Schoenhage's coupled iteration for square root, as documented in:
     Timm Ahrendt, Schnelle Berechnungen der Exponentialfunktion auf hohe
     Genauigkeit. Berlin, Logos 1999
  */
  double v = __internal_half (x);
  double w = a * x;
  w = __fma_rn (__fma_rn (w, -w, a), v, w);
  v = __fma_rn (__fma_rn (x, -w, 1.0), v, v);
  w = __fma_rn (__fma_rn (w, -w, a), v, w);
  return w;
}

static __forceinline__ double __internal_fast_int2dbl (int a)
{
  double t = __hiloint2double (0x43300000, 0x80000000 ^ a);
  return t - __hiloint2double (0x43300000, 0x80000000);
}

static __forceinline__ int __internal_fast_dbl2int_rn (double a)
{
  return __double2loint (__dadd_rn (a, CUDART_DBL2INT_CVT));
}

static __forceinline__ double __internal_fast_rint (double a)
{
  return __dadd_rn (__dadd_rn (a, CUDART_DBL2INT_CVT), -CUDART_DBL2INT_CVT);
}

static __forceinline__ int __internal_fast_icmp_abs_lt (int a, int b)
{
  return fabsf (__int_as_float (a)) < __int_as_float (b);
}

static __forceinline__ int __internal_fast_icmp_ge (int a, int b)
{
  return __int_as_float (a) >= __int_as_float (b);
}

/* 1152 bits of 2/PI for Payne-Hanek style argument reduction. */
static __constant__ unsigned long long int __cudart_i2opi_d [] = {
  0x6bfb5fb11f8d5d08ULL,
  0x3d0739f78a5292eaULL,
  0x7527bac7ebe5f17bULL,
  0x4f463f669e5fea2dULL,
  0x6d367ecf27cb09b7ULL,
  0xef2f118b5a0a6d1fULL,
  0x1ff897ffde05980fULL,
  0x9c845f8bbdf9283bULL,
  0x3991d639835339f4ULL,
  0xe99c7026b45f7e41ULL,
  0xe88235f52ebb4484ULL,
  0xfe1deb1cb129a73eULL,
  0x06492eea09d1921cULL,
  0xb7246e3a424dd2e0ULL,
  0xfe5163abdebbc561ULL,
  0xdb6295993c439041ULL,
  0xfc2757d1f534ddc0ULL,
  0xa2f9836e4e441529ULL,
};

static __forceinline__ ulonglong2 __internal_add128 (unsigned long long int a,
                                                     unsigned long long int b,
                                                     unsigned long long int c,
                                                     unsigned long long int d)
{
  ulonglong2 res;
  asm ("{\n\t"
       ".reg .u32 r0, r1, r2, r3, a0, a1, a2, a3, b0, b1, b2, b3;\n\t"
       "mov.b64         {a0,a1}, %2;\n\t"
       "mov.b64         {a2,a3}, %3;\n\t"
       "mov.b64         {b0,b1}, %4;\n\t"
       "mov.b64         {b2,b3}, %5;\n\t"
       "add.cc.u32      r0, a0, b0; \n\t"
       "addc.cc.u32     r1, a1, b1; \n\t"
       "addc.cc.u32     r2, a2, b2; \n\t"
       "addc.u32        r3, a3, b3; \n\t"
       "mov.b64         %0, {r0,r1};\n\t"  
       "mov.b64         %1, {r2,r3};\n\t"
       "}"
       : "=l"(res.x), "=l"(res.y)
       : "l"(a),"l"(b),"l"(c),"l"(d));
  return res;
}

static __forceinline__ ulonglong2 __internal_sub128 (unsigned long long int a,
                                                     unsigned long long int b,
                                                     unsigned long long int c,
                                                     unsigned long long int d)
{
  ulonglong2 res;
  asm ("{\n\t"
       ".reg .u32 r0, r1, r2, r3, a0, a1, a2, a3, b0, b1, b2, b3;\n\t"
       "mov.b64         {a0,a1}, %2;\n\t"
       "mov.b64         {a2,a3}, %3;\n\t"
       "mov.b64         {b0,b1}, %4;\n\t"
       "mov.b64         {b2,b3}, %5;\n\t"
       "sub.cc.u32      r0, a0, b0; \n\t"
       "subc.cc.u32     r1, a1, b1; \n\t"
       "subc.cc.u32     r2, a2, b2; \n\t"
       "subc.u32        r3, a3, b3; \n\t"
       "mov.b64         %0, {r0,r1};\n\t"  
       "mov.b64         %1, {r2,r3};\n\t"
       "}"
       : "=l"(res.x), "=l"(res.y)
       : "l"(a),"l"(b),"l"(c),"l"(d));
  return res;
}

static __forceinline__ ulonglong2 __internal_umul64wide (unsigned long long a,
                                                         unsigned long long b)
{
  ulonglong2 res;
  asm ("{\n\t"
#if __CUDA_ARCH__ >= 200 && defined(__CUDANVVM__)
       ".reg .u32 r0, r1, r2, r3, alo, ahi, blo, bhi;\n\t"
       "mov.b64         {alo,ahi}, %2;   \n\t"
       "mov.b64         {blo,bhi}, %3;   \n\t"
       "mul.lo.u32      r0, alo, blo;    \n\t"
       "mul.hi.u32      r1, alo, blo;    \n\t"
       "mad.lo.cc.u32   r1, alo, bhi, r1;\n\t"
       "madc.hi.u32     r2, alo, bhi,  0;\n\t"
       "mad.lo.cc.u32   r1, ahi, blo, r1;\n\t"
       "madc.hi.cc.u32  r2, ahi, blo, r2;\n\t"
       "madc.hi.u32     r3, ahi, bhi,  0;\n\t"
       "mad.lo.cc.u32   r2, ahi, bhi, r2;\n\t"
       "addc.u32        r3, r3,  0;      \n\t"
       "mov.b64         %0, {r0,r1};     \n\t"  
       "mov.b64         %1, {r2,r3};     \n\t"
#else  /* __CUDA_ARCH__ >= 200 && defined(__CUDANVVM__) */
       ".reg .u32 r0, r1, r2, r3, r4, alo, ahi, blo, bhi;\n\t"
       "mov.b64         {alo,ahi}, %2;\n\t"
       "mov.b64         {blo,bhi}, %3;\n\t"
       "mul.lo.u32      r0, alo, blo; \n\t"
       "mul.hi.u32      r1, alo, blo; \n\t"
       "mul.lo.u32      r2, ahi, bhi; \n\t"
       "mul.hi.u32      r3, ahi, bhi; \n\t"
       "mul.lo.u32      r4, alo, bhi; \n\t"
       "add.cc.u32      r1,  r1,  r4; \n\t"
       "mul.hi.u32      r4, alo, bhi; \n\t"
       "addc.cc.u32     r2,  r2,  r4; \n\t"
       "addc.u32        r3,  r3,   0; \n\t"
       "mul.lo.u32      r4, ahi, blo; \n\t"
       "add.cc.u32      r1,  r1,  r4; \n\t"
       "mul.hi.u32      r4, ahi, blo; \n\t"
       "addc.cc.u32     r2,  r2,  r4; \n\t"
       "addc.u32        r3,  r3,   0; \n\t"
       "mov.b64         %0, {r0,r1};  \n\t"  
       "mov.b64         %1, {r2,r3};  \n\t"
#endif /* __CUDA_ARCH__ >= 200 && defined(__CUDANVVM__) */
       "}"
       : "=l"(res.x), "=l"(res.y)
       : "l"(a), "l"(b));
  return res;
}

static __forceinline__ ulonglong2 __internal_umad64wide (unsigned long long a,
                                                         unsigned long long b,
                                                         unsigned long long c)
{
  ulonglong2 res;
#if __CUDA_ARCH__ >= 200 && defined(__CUDANVVM__)
  asm ("{\n\t"
       ".reg .u32 r0, r1, r2, r3, alo, ahi, blo, bhi, clo, chi;\n\t"
       "mov.b64         {alo,ahi}, %2;    \n\t"
       "mov.b64         {blo,bhi}, %3;    \n\t"
       "mov.b64         {clo,chi}, %4;    \n\t"
       "mad.lo.cc.u32   r0, alo, blo, clo;\n\t"
       "madc.hi.cc.u32  r1, alo, blo, chi;\n\t"
       "madc.hi.u32     r2, alo, bhi,   0;\n\t"
       "mad.lo.cc.u32   r1, alo, bhi,  r1;\n\t"
       "madc.hi.cc.u32  r2, ahi, blo,  r2;\n\t"
       "madc.hi.u32     r3, ahi, bhi,   0;\n\t"
       "mad.lo.cc.u32   r1, ahi, blo,  r1;\n\t"
       "madc.lo.cc.u32  r2, ahi, bhi,  r2;\n\t"
       "addc.u32        r3,  r3,   0;     \n\t"
       "mov.b64         %0, {r0,r1};      \n\t"  
       "mov.b64         %1, {r2,r3};      \n\t"
       "}"
       : "=l"(res.x), "=l"(res.y)
       : "l"(a), "l"(b), "l"(c));
#else  /* __CUDA_ARCH__ >= 200 && defined(__CUDANVVM__) */
  res = __internal_umul64wide (a, b);
  res = __internal_add128 (res.x, res.y, c, 0ULL);
#endif /* __CUDA_ARCH__ >= 200 && defined(__CUDANVVM__) */
  return res;
}

/* Payne-Hanek style argument reduction. */
static
#if __CUDA_ARCH__ >= 200
__noinline__
#else
__forceinline__
#endif
double __internal_trig_reduction_slowpathd(double a, int *quadrant)
{
  ulonglong2 p;
  unsigned long long int result[5];
  unsigned long long int ia, hi, lo;
  unsigned int e, s;
  int idx, q;

  e = __double2hiint (a);
  s = e & 0x80000000;
  e = (e >> 20) & 0x7ff;
  if (e == 0x7ff) return a; /* cannot handle NaNs and infinities */
  e = e - 1024;
  ia = __double_as_longlong(a);
  ia = (ia << 11) | 0x8000000000000000ULL;
  /* compute x * 2/pi */
  idx = 16 - (e >> 6);
  p.y = 0;
#pragma unroll 1
  for (q = (idx-1); q < min(18,idx+3); q++) {
    p = __internal_umad64wide (__cudart_i2opi_d[q], ia, p.y);
    result[q-(idx-1)] = p.x;
  }
  result[q-(idx-1)] = p.y;
  e = e & 63;
  /* shift result such that hi:lo<127:126> are the least significant
     integer bits, and hi:lo<125:0> are the fractional bits of the result
  */
  lo = result[2];
  hi = result[3];
  if (e) {
    q = 64 - e;
    hi = (hi << e) | (lo >> q);
    lo = (lo << e) | (result[1] >> q);
  }
  q = (int)(hi >> 62);
  /* fraction */
  hi = (hi << 2) | (lo >> 62);
  lo = (lo << 2);
  e = (unsigned int)(hi >> 63); /* fraction >= 0.5 */
  q += e;
  if (s) q = -q;
  *quadrant = q;
  if (e) {
    p = __internal_sub128 (0ULL, 0ULL, lo, hi);
    lo = p.x;
    hi = p.y;
    s = s ^ 0x80000000;
  }
  /* normalize fraction */
  e = __clzll(hi);
  if (e) {
    hi = (hi << e) | (lo >> (64 - e)); 
  }
  p = __internal_umul64wide (hi, 0xC90FDAA22168C235ULL);
  lo = p.x;
  hi = p.y;
  if ((long long int)hi > 0) {
    p = __internal_add128 (lo, hi, lo, hi);
    lo = p.x;
    hi = p.y;
    e++;
  }
  ia = (((unsigned long long int)s << 32) | 
        ((((unsigned long long int)(1022 - e)) << 52) + 
         ((((hi + 1) >> 10) + 1) >> 1)));
  return __longlong_as_double(ia);
}

static __forceinline__ double __internal_trig_reduction_kerneld(double a, int *quadrant)
{
  double j, t;
  int q;

  /* NOTE: for an input of -0, this returns -0 */
  q = __double2int_rn (a * CUDART_2_OVER_PI);
  j = (double)q;
  /* Constants from S. Boldo, M. Daumas, R.-C. Li: "Formally Verified Argument
   * Reduction with a Fused-Multiply-Add", retrieved from the internet at
   * http://arxiv.org/PS_cache/arxiv/pdf/0708/0708.3722v1.pdf
   */
  t = __fma_rn (-j, 1.5707963267948966e+000, a);
  t = __fma_rn (-j, 6.1232339957367574e-017, t);
  t = __fma_rn (-j, 8.4784276603688985e-032, t);
  if ((__double2hiint (a) & 0x7fffffff) >= 0x41e00000) { /* |a| >= 2**31 */
    t = __internal_trig_reduction_slowpathd (a, &q);
  }
  *quadrant = q;
  return t;
}

static __constant__ double __cudart_sin_cos_coeffs[16] =
{
   1.590307857061102704e-10,  /* sin0 */
  -2.505091138364548653e-08,  /* sin1 */
   2.755731498463002875e-06,  /* sin2 */
  -1.984126983447703004e-04,  /* sin3 */
   8.333333333329348558e-03,  /* sin4 */
  -1.666666666666666297e-01,  /* sin5 */
   0.00000000000000000,       /* sin6 */
   0.00000000000000000,       /* unused */
  -1.136781730462628422e-11,  /* cos0 */
   2.087588337859780049e-09,  /* cos1 */
  -2.755731554299955694e-07,  /* cos2 */
   2.480158729361868326e-05,  /* cos3 */
  -1.388888888888066683e-03,  /* cos4 */
   4.166666666666663660e-02,  /* cos5 */
  -5.000000000000000000e-01,  /* cos6 */
   1.000000000000000000e+00,  /* cos7 */
};

static __forceinline__ double __internal_sin_cos_kerneld(double x, int i)
{    
  const double *coeff = __cudart_sin_cos_coeffs + 8 * (i & 1);
  double x2, z;
  x2 = __dmul_rn(x, x);
  z  = (i & 1) ? -1.136781730462628422e-11 :  1.590307857061102704e-10;
  z  = __fma_rn (z, x2, coeff[1]);
  z  = __fma_rn (z, x2, coeff[2]);
  z  = __fma_rn (z, x2, coeff[3]);
  z  = __fma_rn (z, x2, coeff[4]);
  z  = __fma_rn (z, x2, coeff[5]);
  z  = __fma_rn (z, x2, coeff[6]);
  x  = __fma_rn (z, x, x);
  if (i & 1) x = __fma_rn (z, x2, 1.0);
  if (i & 2) x = __fma_rn (x, -1.0, CUDART_ZERO);
  return x;
}

/* approximate sine on -pi/4...+pi/4 */
static __forceinline__ double __internal_sin_kerneld(double x)
{
  double x2, z;
  x2 = __dmul_rn(x, x);
  z  =                   1.590307857061102704e-10;
  z  = __fma_rn (z, x2, -2.505091138364548653e-08);
  z  = __fma_rn (z, x2,  2.755731498463002875e-06);
  z  = __fma_rn (z, x2, -1.984126983447703004e-04);
  z  = __fma_rn (z, x2,  8.333333333329348558e-03);
  z  = __fma_rn (z, x2, -1.666666666666666297e-01);
  z  = __fma_rn (z, x2,  0.0);
  z  = __fma_rn (z, x, x);
  return z;
}

/* approximate cosine on -pi/4...+pi/4 */
static __forceinline__ double __internal_cos_kerneld(double x)
{
  double x2, z;
  x2 = __dmul_rn(x, x);
  z  =                  -1.136781730462628422e-11;   
  z  = __fma_rn (z, x2,  2.087588337859780049e-09);
  z  = __fma_rn (z, x2, -2.755731554299955694e-07);
  z  = __fma_rn (z, x2,  2.480158729361868326e-05);
  z  = __fma_rn (z, x2, -1.388888888888066683e-03);
  z  = __fma_rn (z, x2,  4.166666666666663660e-02);
  z  = __fma_rn (z, x2, -5.000000000000000000e-01);
  z  = __fma_rn (z, x2,  1.000000000000000000e+00);
  return z;
}

/* approximate tangent on -pi/4...+pi/4 */
static __forceinline__ double __internal_tan_kerneld(double x, int i)
{
  double x2, z, q;
  x2 = x * x;
  z =                   9.8006287203286300E-006;
  z = __fma_rn (z, x2, -2.4279526494179897E-005);
  z = __fma_rn (z, x2,  4.8644173130937162E-005);
  z = __fma_rn (z, x2, -2.5640012693782273E-005);
  z = __fma_rn (z, x2,  6.7223984330880073E-005);
  z = __fma_rn (z, x2,  8.3559287318211639E-005);
  z = __fma_rn (z, x2,  2.4375039850848564E-004);
  z = __fma_rn (z, x2,  5.8886487754856672E-004);
  z = __fma_rn (z, x2,  1.4560454844672040E-003);
  z = __fma_rn (z, x2,  3.5921008885857180E-003);
  z = __fma_rn (z, x2,  8.8632379218613715E-003);
  z = __fma_rn (z, x2,  2.1869488399337889E-002);
  z = __fma_rn (z, x2,  5.3968253972902704E-002);
  z = __fma_rn (z, x2,  1.3333333333325342E-001);
  z = __fma_rn (z, x2,  3.3333333333333381E-001);
  z = z * x2;
  q = __fma_rn (z, x, x);
  if (i) {
    double s = q - x; 
    double w = __fma_rn (z, x, -s); // tail of q
    z = __internal_fast_rcp (q);
    z = -z;
    s = __fma_rn (q, z, 1.0);
    q = __fma_rn (__fma_rn (z, w, s), z, z);
  }           
  return q;
}

/* approximates exp(a)-1 on [-log(1.5),log(1.5)] accurate to 1 ulp */
static __forceinline__ double __internal_expm1_kernel (double a)
{
  double t;
  t =                 2.0900320002536536E-009;
  t = __fma_rn (t, a, 2.5118162590908232E-008);
  t = __fma_rn (t, a, 2.7557338697780046E-007);
  t = __fma_rn (t, a, 2.7557224226875048E-006);
  t = __fma_rn (t, a, 2.4801587233770713E-005);
  t = __fma_rn (t, a, 1.9841269897009385E-004);
  t = __fma_rn (t, a, 1.3888888888929842E-003);
  t = __fma_rn (t, a, 8.3333333333218910E-003);
  t = __fma_rn (t, a, 4.1666666666666609E-002);
  t = __fma_rn (t, a, 1.6666666666666671E-001);
  t = __fma_rn (t, a, 5.0000000000000000E-001);
  t = t * a;
  t = __fma_rn (t, a, a);
  return t;
}

/* approximate 2*atanh(0.5*a) on [-0.25,0.25] */
static __forceinline__ double __internal_atanh_kernel (double a_1, double a_2)
{
  double a, a2, t;

  a = a_1 + a_2;
  a2 = a * a;
  t =                  7.597322383488143E-002/65536.0;
  t = __fma_rn (t, a2, 6.457518383364042E-002/16384.0);          
  t = __fma_rn (t, a2, 7.705685707267146E-002/4096.0);
  t = __fma_rn (t, a2, 9.090417561104036E-002/1024.0);
  t = __fma_rn (t, a2, 1.111112158368149E-001/256.0);
  t = __fma_rn (t, a2, 1.428571416261528E-001/64.0);
  t = __fma_rn (t, a2, 2.000000000069858E-001/16.0);
  t = __fma_rn (t, a2, 3.333333333333198E-001/4.0);
  t = t * a2;
  t = __fma_rn (t, a, a_2);
  t = t + a_1;
  return t;
}

static __forceinline__ double __internal_fast_log(double a)
{
  int hi, lo;
  double m, u2, t, f, g, u, e;

  /* x = m * 2^e. log(a) = log(m) + e*log(2) */
  hi = __double2hiint (a);
  lo = __double2loint (a);
  e = __internal_fast_int2dbl ((int)(((unsigned int)hi >> 20) & 0x7fe) - 1022);
  m = __hiloint2double ((hi & 0x801fffff) + 0x3fe00000, lo);
  /* log((1+m)/(1-m)) = 2*atanh(m). log(m) = 2*atanh ((m-1)/(m+1)) */
  f = m - 1.0;
  g = m + 1.0;
  g = __internal_fast_rcp (g);
  u = f * g;
  t = __fma_rn (-2.0, u, f);
  t = __fma_rn (-u, f, t);
  u = __fma_rn (t, g, u);
  /* compute atanh(u) = atanh ((m-1)/(m+1)) */
  u2 = u * u;
  t =                  8.5048800515742276E-002;
  t = __fma_rn (t, u2, 4.1724849126860759E-002);
  t = __fma_rn (t, u2, 6.0524726220470643E-002);
  t = __fma_rn (t, u2, 6.6505606704187079E-002);
  t = __fma_rn (t, u2, 7.6932741976622004E-002);
  t = __fma_rn (t, u2, 9.0908722394788727E-002);
  t = __fma_rn (t, u2, 1.1111111976824838E-001);
  t = __fma_rn (t, u2, 1.4285714274058975E-001);
  t = __fma_rn (t, u2, 2.0000000000077559E-001);
  t = __fma_rn (t, u2, 3.3333333333333154E-001);
  t = t * u2;
  t = __fma_rn (t, u, u);
  t = t + t;
  /* log(a) = log(m) + ln2 * e */
  t = __fma_rn (e, 6.93147180559945290e-001, t);
  return t;
}

static __forceinline__ double __internal_exp2i_kernel(int b)
{
  return __hiloint2double ((b << 20) + (1023 << 20), 0);
}

static __forceinline__ void sincos(double a, double *sptr, double *cptr)
{
  double s, c;
  double t;
  int i;
  
  if (__isinfd(a)) {
    a = __dmul_rn (a, CUDART_ZERO); /* return NaN */
  }
  a = __internal_trig_reduction_kerneld(a, &i);
  c = __internal_cos_kerneld(a);
  s = __internal_sin_kerneld(a);
  t = __internal_neg(s);
  if (i & 1) {
    s = c;
    c = t;
  }
  if (i & 2) {
    s = __internal_neg(s);
    c = __internal_neg(c);
  }
  *sptr = s;
  *cptr = c;
}

static __forceinline__ void sincospi(double a, double *sptr, double *cptr)
{
  double s, c;
  double t;
  long long int l;
  int i;
 
  i = __double2hiint(a);
  if (((unsigned)i + i) > 0x86800000U) {
    a = __dmul_rn (a, CUDART_ZERO);
  }
  t = rint (__internal_twice(a));
  l = (long long)t;
  i = (int)l;
  t = __fma_rn (-t, 0.5, a);
  t = __fma_rn (t, CUDART_PI_HI, t * CUDART_PI_LO);
  c = __internal_cos_kerneld(t);
  s = __internal_sin_kerneld(t);
  t = __internal_neg(s);
  if (i & 1) {
    s = c;
    c = t;
  }
  if (i & 2) {
    s = __internal_neg(s);
    c = __internal_neg(c);
  }
  c = __fma_rn (c, CUDART_ONE, CUDART_ZERO);
  if (a == trunc(a)) {
    s = __dmul_rn(a, CUDART_ZERO);
  }
  *sptr = s;
  *cptr = c;
}

static __forceinline__ double sin(double a)
{
#if __CUDA_ARCH__ == 350
  double s, c;
  sincos (a, &s, &c);
  return s;
#else /* __CUDA_ARCH__ == 350 */
  double z;
  int i;
  if (__isinfd(a)) {
    a = __dmul_rn (a, CUDART_ZERO);
  }
  z = __internal_trig_reduction_kerneld (a, &i);
  /* here, abs(z) <= pi/4, and i has the quadrant */
  z = __internal_sin_cos_kerneld (z, i);
  return z;
#endif /* __CUDA_ARCH__ == 350 */
}

static __forceinline__ double cos(double a)
{
#if __CUDA_ARCH__ == 350
  double s, c;
  sincos (a, &s, &c);
  return c;
#else /* __CUDA_ARCH__ == 350 */
  double z;
  int i;
  if (__isinfd(a)) {
    a = __dmul_rn (a, CUDART_ZERO);
  }
  z = __internal_trig_reduction_kerneld(a, &i);
  /* here, abs(z) <= pi/4, and i has the quadrant */
  i++;
  z = __internal_sin_cos_kerneld (z, i);
  return z;
#endif /* __CUDA_ARCH__ == 350 */
}

static __forceinline__ double sinpi(double a)
{
#if __CUDA_ARCH__ == 350
  double s, c;
  sincospi (a, &s, &c);
  return s;
#else /* __CUDA_ARCH__ == 350 */
  double z;
  long long int l;
  int i;
 
  z = rint (__internal_twice(a));
  l = (long long)z;
  i = (int)l;
  z = __fma_rn (-z, 0.5, a);
  z = __fma_rn (z, CUDART_PI_HI, z * CUDART_PI_LO);
  z = __internal_sin_cos_kerneld (z, i);
  if (a == trunc(a)) {
    z = __dmul_rn (a, CUDART_ZERO);
  }
  return z;
#endif /* __CUDA_ARCH__ == 350 */
}

static __forceinline__ double cospi(double a)
{
#if __CUDA_ARCH__ == 350
  double s, c;
  sincospi (a, &s, &c);
  return c;
#else /* __CUDA_ARCH__ == 350 */
  double z;
  long long int l;
  int i;
 
  i = __double2hiint(a);
  if (((unsigned)i + i) > 0x86800000U) {
    a = __dmul_rn (a, CUDART_ZERO);
  }
  z = rint (__internal_twice(a));
  l = (long long)z;
  i = (int)l;
  z = __fma_rn (-z, 0.5, a);
  z = __fma_rn (z, CUDART_PI_HI, z * CUDART_PI_LO);
  i++;
  z = __internal_sin_cos_kerneld (z, i);
  return z;
#endif /* __CUDA_ARCH__ == 350 */
}

/* Compute cos(x + offset) with phase offset computed after argument
   reduction for higher accuracy.  Needed for Bessel approximation 
   functions.
*/
static __forceinline__ double __internal_cos_offset(double a, double offset)
{
  double z;
  int i;

  z = __internal_trig_reduction_kerneld(a, &i);
  a = z + offset + (i & 3) * CUDART_PIO2;
  return cos(a);
}

static __forceinline__ double tan(double a)
{
  double z;
  int i;

  if (__isinfd(a)) {
    a = __dmul_rn (a, CUDART_ZERO); /* return NaN */
  }
  z = __internal_trig_reduction_kerneld(a, &i);
  /* here, abs(z) <= pi/4, and i has the quadrant */
  z = __internal_tan_kerneld(z, i & 1);
  return z;
}

static __forceinline__ double log(double a)
{
  double m, f, g, u, v, tmp, q, ulo, log_lo, log_hi;
  int ihi, ilo;

  ihi = __double2hiint(a);
  ilo = __double2loint(a);

  if ((a > CUDART_ZERO) && (ihi < 0x7ff00000)) {
    int e = -1023;
    /* normalize denormals */
    if (ihi < 0x00100000) {
      a = a * CUDART_TWO_TO_54;
      e -= 54;
      ihi = __double2hiint(a);
      ilo = __double2loint(a);
    }
    /* a = m * 2^e. m <= sqrt(2): log2(a) = log2(m) + e.
     * m > sqrt(2): log2(a) = log2(m/2) + (e+1)
     */
    e += (unsigned int)ihi >> 20;
    ihi = (ihi & 0x800fffff) | 0x3ff00000;
    m = __hiloint2double (ihi, ilo);
    if (ihi > 0x3ff6a09e) {
      m = __internal_half(m);
      e = e + 1;
    }
    /* log((1+m)/(1-m)) = 2*atanh(m). log(m) = 2*atanh ((m-1)/(m+1)) */
    f = m - 1.0;
    g = m + 1.0;
    g = __internal_fast_rcp(g);
    u = f * g;
    u = u + u;  
    /* u = 2.0 * (m - 1.0) / (m + 1.0) */
    v = u * u;
    q =                 6.7261411553826339E-2/65536.0;
    q = __fma_rn (q, v, 6.6133829643643394E-2/16384.0);
    q = __fma_rn (q, v, 7.6940931149150890E-2/4096.0);
    q = __fma_rn (q, v, 9.0908745692137444E-2/1024.0);
    q = __fma_rn (q, v, 1.1111111499059706E-1/256.0);
    q = __fma_rn (q, v, 1.4285714283305975E-1/64.0);
    q = __fma_rn (q, v, 2.0000000000007223E-1/16.0);
    q = __fma_rn (q, v, 3.3333333333333326E-1/4.0);
    tmp = 2.0 * (f - u);
    tmp = __fma_rn (-u, f, tmp); // tmp = remainder of division
    ulo = g * tmp;               // less significant quotient bits
    /* u + ulo = 2.0 * (m - 1.0) / (m + 1.0) to more than double precision */
    q = q * v;
    /* log_hi + log_lo = log(m) to more than double precision */ 
    log_hi = u;
    log_lo = __fma_rn (q, u, ulo);
    /* log_hi + log_lo = log(m)+e*log(2)=log(a) to more than double precision*/
    f = __internal_fast_int2dbl (e);
    q = __fma_rn (f, CUDART_LN2_HI, log_hi);
    tmp = __fma_rn (-f, CUDART_LN2_HI, q);
    tmp = tmp - log_hi;
    log_hi = q;
    log_lo = log_lo - tmp;
    log_lo = __fma_rn (f, CUDART_LN2_LO, log_lo);
    q = log_hi + log_lo;
  } else if (__isnand(a)) {
    q = a + a;
  } else if (a == 0) {          /* log(0) = -INF */
    q = -CUDART_INF;
  } else if (a == CUDART_INF) { /* log(INF) = INF */
    q = a;
  } else {  /* log(x) is undefined for x < 0.0, return INDEFINITE */
    q = CUDART_NAN;
  }
  return q;
}

/* Requires |x.y| > |y.y|. 8 DP operations */
static __forceinline__ double2 __internal_ddadd_xgty (double2 x, double2 y)
{
  double2 z;
  double r, s, e;
  r = x.y + y.y;
  e = x.y - r;
  s = ((e + y.y) + y.x) + x.x;
  z.y = e = r + s;
  z.x = (r - e) + s;
  return z;
}

/* Take full advantage of FMA. Only 7 DP operations */
static __forceinline__ double2 __internal_ddmul (double2 x, double2 y)
{
  double e;
  double2 t, z;
  t.y = __dmul_rn (x.y, y.y);       /* prevent FMA-merging */
  t.x = __fma_rn (x.y, y.y, -t.y);
  t.x = __fma_rn (x.y, y.x, t.x);
  t.x = __fma_rn (x.x, y.y, t.x);
  z.y = e = t.y + t.x;
  z.x = (t.y - e) + t.x;
  return z;
}

static __forceinline__ double2 __internal_log_ext_prec(double a)
{
  double2 res;
  double2 qq, cc, uu, tt;
  double f, g, u, v, q, ulo, tmp, m;
  int ilo, ihi, expo;

  ihi = __double2hiint(a);
  ilo = __double2loint(a);
  expo = (unsigned int)ihi >> 20;
  /* convert denormals to normals for computation of log(a) */
  if (expo == 0) {
    a *= CUDART_TWO_TO_54;
    ihi = __double2hiint(a);
    ilo = __double2loint(a);
    expo = (unsigned int)ihi >> 20;
    expo -= 54;
  }  
  expo -= 1023;
  /* log(a) = log(m*2^expo) = 
     log(m) + log(2)*expo, if m < sqrt(2), 
     log(m*0.5) + log(2)*(expo+1), if m >= sqrt(2)
  */
  ihi = (ihi & 0x800fffff) | 0x3ff00000;
  m = __hiloint2double (ihi, ilo);
  if ((unsigned)ihi > (unsigned)0x3ff6a09e) {
    m = __internal_half(m);
    expo = expo + 1;
  }
  /* compute log(m) with extended precision using an algorithm derived from 
   * P.T.P. Tang, "Table Driven Implementation of the Logarithm Function", 
   * TOMS, Vol. 16., No. 4, December 1990, pp. 378-400. A modified polynomial 
   * approximation to atanh(x) on the interval [-0.1716, 0.1716] is utilized.
   */
  f = m - 1.0;
  g = m + 1.0;
  g = __internal_fast_rcp(g);
  u = f * g;
  u = u + u;  
  /* u = 2.0 * (m - 1.0) / (m + 1.0) */
  v = u * u;
  q =                 6.6253631649203309E-2/65536.0;
  q = __fma_rn (q, v, 6.6250935587260612E-2/16384.0);
  q = __fma_rn (q, v, 7.6935437806732829E-2/4096.0);
  q = __fma_rn (q, v, 9.0908878711093280E-2/1024.0);
  q = __fma_rn (q, v, 1.1111111322892790E-1/256.0);
  q = __fma_rn (q, v, 1.4285714284546502E-1/64.0);
  q = __fma_rn (q, v, 2.0000000000003113E-1/16.0);
  q = q * v;
  /* u + ulo = 2.0 * (m - 1.0) / (m + 1.0) to more than double precision */
  tmp = 2.0 * (f - u);
  tmp = __fma_rn (-u, f, tmp); // tmp = remainder of division
  ulo = g * tmp;               // less significand quotient bits
  /* switch to double-double at this point */
  qq.y = q;
  qq.x = 0.0;
  uu.y = u;
  uu.x = ulo;
  cc.y =  3.3333333333333331E-1/4.0;
  cc.x = -9.8201492846582465E-18/4.0;
  qq = __internal_ddadd_xgty (cc, qq);
  /* compute log(m) in double-double format */
  cc.y = __dmul_rn (uu.y, uu.y);
  cc.x = __fma_rn (uu.y, uu.y, -cc.y);
  cc.x = __fma_rn (uu.y, __internal_twice (uu.x), cc.x); /* u**2 */
  tt.y = __dmul_rn (cc.y, uu.y);     
  tt.x = __fma_rn (cc.y, uu.y, -tt.y);
  tt.x = __fma_rn (cc.y, uu.x, tt.x);
  tt.x = __fma_rn (cc.x, uu.y, tt.x); /* u**3 (unnormalized double-double) */
  qq = __internal_ddmul(qq, tt);
  uu = __internal_ddadd_xgty (uu, qq);
  /* compute log(a) = log(m) + log(2)*expo in double-double format */
  f = __internal_fast_int2dbl (expo);
  q = __fma_rn (f, CUDART_LN2_HI, uu.y);
  tmp = __fma_rn (-f, CUDART_LN2_HI, q);
  tmp = tmp - uu.y;
  uu.y = q;
  uu.x = uu.x - tmp;
  uu.x = __fma_rn (f, CUDART_LN2_LO, uu.x);
  /* normalize the result */
  res.y = f = uu.y + uu.x;
  res.x = (uu.y - f) + uu.x;
  return res;
}

static __forceinline__ double log2(double a)
{
  double t;
  t = log(a);
  return __fma_rn (t, CUDART_L2E_HI, t * CUDART_L2E_LO);
}

static __forceinline__ double log10(double a)
{
  double t;
  t = log(a);
  return __fma_rn (t, CUDART_LGE_HI, t * CUDART_LGE_LO);
}

static __forceinline__ double log1p(double a)
{
  double t;
  int i;

  i = __double2hiint(a);
  if (((unsigned)i < (unsigned)0x3fe55555) || ((int)i < (int)0xbfd99999)) {
    /* Compute log2(a+1) = 2*atanh(a/(a+2)) */
    t = a + 2.0;
    t = a / t;
    t = -a * t;
    t = __internal_atanh_kernel(a, t);
  } else {
    t = log (a + CUDART_ONE);
  }
  return t;
}

/* approximate exp() on [log(sqrt(0.5)), log(sqrt(2))] */
static __forceinline__ double __internal_exp_poly(double a)
{ 
  double t; 
  /* The following approximation was generated using Sollya's fpminimax command
   * Sollya: an environment for the development of numerical codes.
   * S. Chevillard, M. Joldes and C. Lauter.
   * In Mathematical Software- ICMS 2010, pages 28-31, Heidelberg, Germany,
   * September 2010. Springer.
   */
  t =                 2.5022322536502990E-008;
  t = __fma_rn (t, a, 2.7630903488173108E-007);
  t = __fma_rn (t, a, 2.7557514545882439E-006);
  t = __fma_rn (t, a, 2.4801491039099165E-005);
  t = __fma_rn (t, a, 1.9841269589115497E-004);
  t = __fma_rn (t, a, 1.3888888945916380E-003);
  t = __fma_rn (t, a, 8.3333333334550432E-003);
  t = __fma_rn (t, a, 4.1666666666519754E-002);
  t = __fma_rn (t, a, 1.6666666666666477E-001);
  t = __fma_rn (t, a, 5.0000000000000122E-001);
  t = __fma_rn (t, a, 1.0000000000000000E+000);
  t = __fma_rn (t, a, 1.0000000000000000E+000);
  return t;
}

/* compute a * 2^i */
static __forceinline__ double __internal_exp_scale(double a, int i)
{
  unsigned int j, k;

  if (abs(i) < 1023) {
    k = (i << 20) + (1023 << 20);
  } else {
    k = i + 2*1023;  
    j = k / 2;
    j = j << 20;
    k = (k << 20) - j;
    a = a * __hiloint2double (j, 0);
  }
  a = a * __hiloint2double (k, 0);
  return a;
}

static __forceinline__ double __internal_fast_exp(double a)
{
  double r;
  int i;
  r = __dmul_rn (a, CUDART_L2E);
  i = __internal_fast_dbl2int_rn (r);
  r = __internal_fast_rint (r);
  r = __fma_rn (r, -CUDART_LN2, a);
  r = __internal_exp_poly (r);
  r = __hiloint2double (__double2hiint(r) + (i << 20), __double2loint(r));
  return r;
}

static __forceinline__ double __internal_exp_kernel(double a, int scale)
{ 
  double t, z;
  int i;

  t = __dmul_rn (a, CUDART_L2E);
  i = __internal_fast_dbl2int_rn (t);
  t = __internal_fast_rint (t);
  z = __fma_rn (t, -CUDART_LN2_HI, a);
  z = __fma_rn (t, -CUDART_LN2_LO, z);
  t = __internal_exp_poly (z);
  z = __internal_exp_scale (t, i + scale); 
  return z;
}   

static __forceinline__ double __internal_old_exp_kernel(double a, int scale)
{ 
  double t, z;
  int i, j, k;

  t = __dmul_rn (a, CUDART_L2E);
  i = __internal_fast_dbl2int_rn (t);
  t = __internal_fast_rint (t);
  z = __fma_rn (t, -CUDART_LN2_HI, a);
  z = __fma_rn (t, -CUDART_LN2_LO, z);
  t = __internal_expm1_kernel (z);
  k = ((i + scale) << 20) + (1023 << 20);
  if (abs(i) < 1021) {
    z = __hiloint2double (k, 0);
    z = __fma_rn (t, z, z);
  } else {
    j = 0x40000000;
    if (i < 0) {
      k += (55 << 20);
      j -= (55 << 20);
    }
    k = k - (1 << 20);
    z = __hiloint2double (j, 0); /* 2^-54 if a is denormal, 2.0 otherwise */
    t = __fma_rn (t, z, z);
    z = __hiloint2double (k, 0);
    z = t * z;
  }
  return z;
}

static __forceinline__ double exp(double a)
{
  double t;
  int i;
  i = __double2hiint(a);
  if (__internal_fast_icmp_abs_lt (i, 0x40874911)) {
    t = __internal_exp_kernel (a, 0);
  } else {
    t = (i < 0) ? CUDART_ZERO : CUDART_INF;
    if (__isnand (a)) {
      t = a + a;
    }
  }
  return t;
}

static __forceinline__ double exp2(double a)
{
  double t, z;
  int i;

  i = __double2hiint(a);
  if (__internal_fast_icmp_abs_lt (i, 0x4090cc00)) {
    t = __internal_fast_rint (a);
    i = __internal_fast_dbl2int_rn (a);
    z = a - t;
    /* 2^z = exp(log(2)*z) */
    z = __fma_rn (z, CUDART_LN2_HI, z * CUDART_LN2_LO);
    t = __internal_exp_poly (z);
    t = __internal_exp_scale (t, i); 
  } else {
    t = (i < 0) ? CUDART_ZERO : CUDART_INF;
    if (__isnand (a)) {
      t = a + a;
    }
  }
  return t;
}

static __forceinline__ double exp10(double a)
{
  double z;
  double t;
  int i;

  i = __double2hiint(a);
  if (__internal_fast_icmp_abs_lt (i, 0x407439b8)) {
    t = __dmul_rn (a, CUDART_L2T);
    i = __internal_fast_dbl2int_rn (t);
    t = __internal_fast_rint (t);
    z = __fma_rn (t, -CUDART_LG2_HI, a);
    z = __fma_rn (t, -CUDART_LG2_LO, z);
    /* 2^z = exp(log(10)*z) */
    z = __fma_rn (z, CUDART_LNT_HI, z * CUDART_LNT_LO);
    t = __internal_exp_poly (z);
    t = __internal_exp_scale (t, i); 
  } else {
    t = (i < 0) ? CUDART_ZERO : CUDART_INF;
    if (__isnand (a)) {
      t = a + a;
    }
  }
  return t;
}

static __forceinline__ double __internal_expm1_scaled(double a, int scale)
{ 
  double t, z, u;
  int i, j;
  unsigned int k;
  k = __double2hiint(a);
  t = __dmul_rn (a, CUDART_L2E);
  i = __internal_fast_dbl2int_rn (t) + scale;
  t = __internal_fast_rint (t);
  z = __fma_rn (t, -CUDART_LN2_HI, a);
  z = __fma_rn (t, -CUDART_LN2_LO, z);
  k = k + k;
  if ((unsigned)k < (unsigned)0x7fb3e647) {
    z = a;
    i = 0;
  }
  t = __internal_expm1_kernel(z);
  j = i;
  if (i == 1024) j--;
  u = __internal_exp2i_kernel(j);
  a = __hiloint2double(0x3ff00000 + (scale << 20), 0);
  a = u - a;
  t = __fma_rn (t, u, a);
  if (i == 1024) t = t + t;
  if (k == 0) t = z;              /* preserve -0 */
  return t;
}   

static __forceinline__ double expm1(double a)
{
  double t;
  int k;

  k = __double2hiint(a);
  if (((unsigned)k < (unsigned)0x40862e43) || ((int)k < (int)0xc04a8000)) {
    t = __internal_expm1_scaled(a, 0);
  } else {
    t = (k < 0) ? -CUDART_ONE : CUDART_INF;
    if (__isnand(a)) {
      t = a + a;
    }
  }
  return t;
}

static __forceinline__ double cosh(double a)
{
  double t, z;
  int i;

  z = fabs(a);
  i = __double2hiint(z);
  if ((unsigned)i < (unsigned)0x408633cf) {
    z = __internal_exp_kernel(z, -2);
    t = __internal_fast_rcp (z);
    z = __fma_rn(2.0, z, 0.125 * t);
  } else {
    if (z > 0.0) a = CUDART_INF_F;
    z = a + a;
  }
  return z;
}

static __forceinline__ double sinh(double a)
{
  double t, z;
  int thi, tlo;
  thi = __double2hiint(a) & 0x7fffffff;
  tlo = __double2loint(a);
  t = __hiloint2double(thi, tlo);
  if (thi < 0x3ff00000) { /* risk of catastrophic cancellation */
    /* approximate sinh(x) on [0,1] with a polynomial */
    double t2 = t * t;
    z =                  7.7587488021505296E-013;
    z = __fma_rn (z, t2, 1.6057259768605444E-010);
    z = __fma_rn (z, t2, 2.5052123136725876E-008);
    z = __fma_rn (z, t2, 2.7557319157071848E-006);
    z = __fma_rn (z, t2, 1.9841269841431873E-004);
    z = __fma_rn (z, t2, 8.3333333333331476E-003);
    z = __fma_rn (z, t2, 1.6666666666666669E-001);
    z = z * t2;
    z = __fma_rn (z, t, t);
  } else {
    z = __internal_expm1_scaled (t, -1);
    z = z + z / __fma_rn (2.0, z, 1.0);
    if (t >= CUDART_LN2_X_1025) {
      z = CUDART_INF;
    }
  } 
  z = __internal_copysign_pos(z, a);
  return z;
}

static __forceinline__ double tanh(double a)
{
  double r, t;
  int thi, tlo;

  thi = __double2hiint (a) & 0x7fffffff;
  tlo = __double2loint (a);
  t = __hiloint2double (thi, tlo);
  if (t >= 0.55) {
    r = __internal_fast_rcp (__internal_old_exp_kernel (2.0 * t, 0) + 1.0);
    r = __fma_rn (2.0, -r, 1.0);
    if (thi >= 0x40400000) {
      r = 1.0;       /* overflow -> 1.0 */
    }
  } else {
    double a2;
    a2 = a * a;
    r =                   5.102147717274194E-005;
    r = __fma_rn (r, a2, -2.103023983278533E-004);
    r = __fma_rn (r, a2,  5.791370145050539E-004);
    r = __fma_rn (r, a2, -1.453216755611004E-003);
    r = __fma_rn (r, a2,  3.591719696944118E-003);
    r = __fma_rn (r, a2, -8.863194503940334E-003);
    r = __fma_rn (r, a2,  2.186948597477980E-002);
    r = __fma_rn (r, a2, -5.396825387607743E-002);
    r = __fma_rn (r, a2,  1.333333333316870E-001);
    r = __fma_rn (r, a2, -3.333333333333232E-001);
    r = r * a2;
    r = __fma_rn (r, a, a);
  }
  r = __internal_copysign_pos (r, a);
  return r;
}

static __forceinline__ double __internal_atan_kernel(double a)
{
  double t, a2;
  a2 = a * a;
  t =                  -2.0258553044438358E-005 ;
  t = __fma_rn (t, a2,  2.2302240345758510E-004);
  t = __fma_rn (t, a2, -1.1640717779930576E-003);
  t = __fma_rn (t, a2,  3.8559749383629918E-003);
  t = __fma_rn (t, a2, -9.1845592187165485E-003);
  t = __fma_rn (t, a2,  1.6978035834597331E-002);
  t = __fma_rn (t, a2, -2.5826796814495994E-002);
  t = __fma_rn (t, a2,  3.4067811082715123E-002);
  t = __fma_rn (t, a2, -4.0926382420509971E-002);
  t = __fma_rn (t, a2,  4.6739496199157994E-002);
  t = __fma_rn (t, a2, -5.2392330054601317E-002);
  t = __fma_rn (t, a2,  5.8773077721790849E-002);
  t = __fma_rn (t, a2, -6.6658603633512573E-002);
  t = __fma_rn (t, a2,  7.6922129305867837E-002);
  t = __fma_rn (t, a2, -9.0909012354005225E-002);
  t = __fma_rn (t, a2,  1.1111110678749424E-001);
  t = __fma_rn (t, a2, -1.4285714271334815E-001);
  t = __fma_rn (t, a2,  1.9999999999755019E-001);
  t = __fma_rn (t, a2, -3.3333333333331860E-001);
  t = t * a2;
  t = __fma_rn (t, a, a);
  return t;
}

static __forceinline__ double atan2(double a, double b)
{
  double t0, t1, t2, t3, t4;

  t1 = fabs(b);
  t2 = fabs(a);
  if (t1 == 0.0 && t2 == 0.0) {
    t0 = (__double2hiint(b) < 0) ? CUDART_PI : 0;
    t0 = __internal_copysign_pos(t0, a);
  } else if (t1 == CUDART_INF && t2 == CUDART_INF) {
    t0 = (__double2hiint(b) < 0) ? CUDART_3PIO4 : CUDART_PIO4;
    t0 = __internal_copysign_pos(t0, a);
  } else {
    t3 = fmax (t2, t1);
    t4 = fmin (t2, t1);
    t0 = t4 / t3;
    t0 = __internal_atan_kernel(t0);
    /* Map result according to octant. */
    if (t2 > t1) t0 = CUDART_PIO2 - t0;
    if (__double2hiint(b) < 0) t0 = CUDART_PI - t0;
    t0 = __internal_copysign_pos(t0, a);
    t1 = t1 + t2;
    if (!(t1 <= CUDART_INF)) t0 = t1;  /* handle NaN inputs */
  }
  return t0;
}

static __forceinline__ double atan(double a)
{
  double t0, t1;
  /* reduce argument to first octant */
  t0 = fabs(a);
  t1 = t0;
  if (t0 > 1.0) {
    t1 = __internal_fast_rcp (t1);
    if (t0 == CUDART_INF) t1 = 0.0;
  }
  /* approximate atan(r) in first octant */
  t1 = __internal_atan_kernel(t1);
  /* map result according to octant. */
  if (t0 > 1.0) {
    t1 = CUDART_PIO2 - t1;
  }
  return __internal_copysign_pos(t1, a);
}

static __forceinline__ double __internal_asin_kernel(double a)
{
  double r;
  r =                  6.259798167646803E-002;
  r = __fma_rn (r, a, -7.620591484676952E-002);
  r = __fma_rn (r, a,  6.686894879337643E-002);
  r = __fma_rn (r, a, -1.787828218369301E-002); 
  r = __fma_rn (r, a,  1.745227928732326E-002);
  r = __fma_rn (r, a,  1.000422754245580E-002);
  r = __fma_rn (r, a,  1.418108777515123E-002);
  r = __fma_rn (r, a,  1.733194598980628E-002);
  r = __fma_rn (r, a,  2.237350511593569E-002);
  r = __fma_rn (r, a,  3.038188875134962E-002);
  r = __fma_rn (r, a,  4.464285849810986E-002);
  r = __fma_rn (r, a,  7.499999998342270E-002);
  r = __fma_rn (r, a,  1.666666666667375E-001);
  r = r * a;
  return r;
}

static __forceinline__ double asin(double a)
{
  double fa, t0, t1;
  int ihi;
  fa = fabs(a);
  ihi = __double2hiint(fa);
  if (ihi < 0x3fe26666) {
    t1 = fa * fa;
    t1 = __internal_asin_kernel (t1);
    t1 = __fma_rn (t1, fa, fa);
    t1 = __internal_copysign_pos(t1, a);
  } else {
    t1 = __fma_rn (-0.5, fa, 0.5);
    t0 = __internal_fast_sqrt (t1);
    if (ihi >= 0x3ff00000) t0 = CUDART_NAN; /* handle negative sqrt inputs */
    if (__double2hiint(t1) == 0) t0 = t1;   /* handle zero sqrt inputs */
    t1 = __internal_asin_kernel (t1);
    t0 = -2.0 * t0;
    t1 = __fma_rn (t0, t1, CUDART_PIO2_LO);
    t0 = t0 + CUDART_PIO4_HI;
    t1 = t0 + t1;
    t1 = t1 + CUDART_PIO4_HI;
    if (ihi <= 0x3ff00000) {
      t1 = __internal_copysign_pos(t1, a);
    }
  }
  return t1;
}

static __forceinline__ double acos(double a)
{
  double t0, t1;
  int ihi, ahi;

  ahi = __double2hiint(a);
  t0 = fabs (a);
  ihi = __double2hiint(t0);
  if (ihi < 0x3fe26666) {  
    t1 = t0 * t0;
    t1 = __internal_asin_kernel (t1);
    t0 = __fma_rn (t1, t0, t0);
    if (ahi < 0) {
      t0 = __dadd_rn (t0, +CUDART_PIO2_LO);
      t0 = __dadd_rn (CUDART_PIO2_HI, +t0);
    } else {
      t0 = __dadd_rn (t0, -CUDART_PIO2_LO);
      t0 = __dadd_rn (CUDART_PIO2_HI, -t0);
    }
  } else {
    /* acos(x) = [1 + y * p(y)] * 2 * sqrt(y/2), y = 1 - x */
    double p, s, y;
    y = 1.0 - t0;
    s = __internal_twice (__internal_fast_sqrt (__internal_half(y)));
    p =                  2.7519189493111718E-006;
    p = __fma_rn (p, y, -1.5951212865388395E-006);
    p = __fma_rn (p, y,  6.1185294127269731E-006);
    p = __fma_rn (p, y,  6.9283438595562408E-006);
    p = __fma_rn (p, y,  1.9480663162164715E-005);
    p = __fma_rn (p, y,  4.5031965455307141E-005);
    p = __fma_rn (p, y,  1.0911426300865435E-004);
    p = __fma_rn (p, y,  2.7113554445344455E-004);
    p = __fma_rn (p, y,  6.9913006155254860E-004);
    p = __fma_rn (p, y,  1.8988715243469585E-003);
    p = __fma_rn (p, y,  5.5803571429249681E-003);
    p = __fma_rn (p, y,  1.8749999999999475E-002);
    p = __fma_rn (p, y,  8.3333333333333329E-002);
    ihi = __double2hiint (y);
    if (ihi <= 0) {
      t0 = __dmul_rn (t0, CUDART_ZERO);
    } else {
      t0 = __fma_rn (p * y, s, s);
    }
    if (ihi < 0) {
      t0 = __dmul_rn (t0, CUDART_INF);
    }
    if (ahi < 0) {    
      t0 = __dadd_rn (t0, -CUDART_PI_LO);
      t0 = __dadd_rn (CUDART_PI_HI, -t0);
    }
  } 
  return t0;
}

static __forceinline__ double acosh(double a)
{
  double t, r;
  int thi;
  t = a - 1.0;
  thi = __double2hiint(t);
  if ((unsigned)thi >= 0x43300000) {
    /* for large a, acosh(a) = log(2*a) */
    r = CUDART_LN2 + log(t);
  } else {
    /* acosh(a) = log (a + sqrt (a*a-1)) = log1p ((a-1) + sqrt (a*a-1)) */
    r = log1p (t + __internal_fast_sqrt (__fma_rn (a, t, t)));
    if (thi == 0) r = t;       /* fast sqrt cannot handle input of zero */
  }
  return r;
}
 
static __forceinline__ double asinh(double a)
{
  double fa, t;
  int ahi, alo, ahia;
  ahi = __double2hiint(a);
  alo = __double2loint(a);
  ahia = ahi & 0x7fffffff;
  fa = __hiloint2double (ahia, alo);
  if (ahia >= 0x43e00000) {
    /* for large a, asinh(a) = log(2*a) */
    t = CUDART_LN2 + log(fa);
  } else {
    /* asinh(a) = log (a + sqrt (a*a+1)) = log1p (a + (sqrt (a*a+1) - 1)) */
    t = __dmul_rn (a, a) / (1.0 + __internal_fast_sqrt (__fma_rn (a, a, 1.0)));
    t = log1p (fa + t);
  }
  return __internal_copysign_pos(t, a); 
}

static __forceinline__ double atanh(double a)
{
  double fa, t;
  fa = fabs(a);
  t = (2.0 * fa) / (1.0 - fa);
  t = 0.5 * log1p(t);
  if (__double2hiint(a) < 0) {
    t = -t;
  }
  return t;
}

static __forceinline__ double hypot(double a, double b)
{
  double x, y, s, t, fa, fb;
  int expa;

  fa = fabs (a);
  fb = fabs (b);

  a = fmax (fa, fb);
  b = fmin (fa, fb);

  /* extract exponent */
  expa = ((unsigned int)__double2hiint (a) >> 20) - 1023;
  
  /* scale down */
  y = __internal_exp_scale (b, -expa);
  if (expa < 0) {
    x = a * __internal_exp2i_kernel (-expa);
  } else {
    x = __internal_fast_scale (a, -expa);
  }

  t = y * y;
  t = __fma_rn (x, x, t);
  t = t * __internal_fast_rsqrt (t);
  
  /* scale up */
  t = __internal_exp_scale (t, expa);

  /* alternative result if NaN input, b is zero, or b much smaller than a */
  s = fa + fb;

  /* fixup for NaN input, b equal to zero, or b much smaller than a */
  if (!(s > a)) t = s;
  /* hypot (+/-INF, NaN) must return +INF per C99 spec section F.9.4.3 */
  if (a == CUDART_INF) t = a;

  return t;
}

static __forceinline__ double rhypot(double a, double b)
{
  double x, y, s, t, fa, fb;
  int expa;

  fa = fabs (a);
  fb = fabs (b);

  a = fmax (fa, fb);
  b = fmin (fa, fb);
  
  /* extract exponent */
  expa = ((unsigned int)__double2hiint (a) >> 20) - 1023;

  /* scale down */
  y = __internal_exp_scale (b, -expa);
  if (expa < 0) {
    x = a * __internal_exp2i_kernel (-expa);
  } else {
    x = __internal_fast_scale (a, -expa);
  }

  t = y * y;
  t = __fma_rn (x, x, t);
  t = __internal_fast_rsqrt (t);
  
  t = __internal_exp_scale(t, -expa);
  s = fa + fb;

  if (!(s > CUDART_ZERO)) t = s; /* fixup for NaNs */
  if (s == CUDART_ZERO) t = CUDART_INF; /* fixup for both zero */
  if (a == CUDART_INF) t = 0.0; /* fixup for inf */
  return t;
}

static __forceinline__ double cbrt(double a)
{
  float s;
  double u, t, r;
  int ilo, ihi, expo, nexpo, denorm;

  ilo = __double2loint (a);
  ihi = __double2hiint (a) & 0x7fffffff;
  if ((a != 0.0) && (ihi < 0x7ff00000)) {
    expo = (int)((unsigned int)ihi >> 20);
    denorm = 0;
    if (expo == 0) { /* denormal */
      t = __hiloint2double (ihi, ilo);
      t = t * CUDART_TWO_TO_54;
      denorm = 18;
      ilo = __double2loint (t);
      ihi = __double2hiint (t);
      expo = (int)((unsigned int)ihi >> 20);
    }
    /* scale into float range */
    nexpo = __float2int_rn (CUDART_THIRD_F * (float)(expo - 1022));
    ihi -= (3 * nexpo) << 20;
    r = __hiloint2double (ihi, ilo);
    /* initial cbrt approximation */
    s = (float)r;
    s = __internal_fast_exp2f (CUDART_THIRD_F * __internal_fast_log2f (s));
    t = (double)s;
    /* refine approximation using one Halley iteration with cubic convergence*/
    u = t * t; /* multiplication is exact */
    u = __internal_fast_rcp (__fma_rn (__internal_twice (u), t, r)) * 
        __fma_rn (u, -t, r);
    t = __fma_rn (t, u, t);
    /* scale result back into double range */
    ilo = __double2loint (t);
    ihi = __double2hiint (t);
    ihi += (nexpo - denorm) << 20;
    t = __hiloint2double (ihi, ilo);
    t = __internal_copysign_pos (t, a);
  } else {
    t = a + a;
  }
  return t;
}

static __forceinline__ double rcbrt(double a)
{
  float s;
  double t, r, e, y, z;
  int ilo, ihi, expo, nexpo, denorm;

  ilo = __double2loint (a);
  ihi = __double2hiint (a) & 0x7fffffff;
  if ((a != 0.0) && (ihi < 0x7ff00000)) {
    expo = (int)((unsigned int)ihi >> 20);
    denorm = 0;
    if (expo == 0) { /* denormal */
      t = __hiloint2double (ihi, ilo);
      t = t * CUDART_TWO_TO_54;
      denorm = 18;
      ilo = __double2loint (t);
      ihi = __double2hiint (t);
      expo = (int)((unsigned int)ihi >> 20);
    }
    /* scale into float range */
    nexpo = __float2int_rn (CUDART_THIRD_F * (float)(expo - 1022));
    ihi -= (3 * nexpo) << 20;
    r = __hiloint2double (ihi, ilo);
    /* initial invcbrt approximation */
    s = (float)r;
    t = __internal_fast_exp2f (-CUDART_THIRD_F * __internal_fast_log2f (s));
    /* refine invcbrt using one iteration with cubic convergence */
    y = __dmul_rn (t, t);
    e = __dmul_rn (r, t);
    z = __fma_rn (y, -e, 1.0);
    r = __fma_rn (r, t, -e);
    z = __fma_rn (y, -r, z);
    y = __fma_rn (t, t, -y);
    e = __fma_rn (e, -y, z);
    y = __fma_rn (2.2222222222222222e-1, e, CUDART_THIRD);
    e = __dmul_rn (e, t);
    t = __fma_rn (y, e, t);
    /* scale result back into double range */
    ilo = __double2loint (t);
    ihi = __double2hiint (t);
    ihi += (-(nexpo - denorm)) << 20;
    t = __hiloint2double (ihi, ilo);
  } else {
    t = __hiloint2double (ihi ^ 0x7ff00000, ilo);
    if (__isnand (a)) {
      t = a + a;
    }
  }
  t = __internal_copysign_pos (t, a);
  return t;
}

static
#if __CUDA_ARCH__ >= 200
__noinline__
#else
__forceinline__
#endif
double __internal_accurate_pow(double a, double b)
{
  double2 loga;
  double2 prod;
  double t_hi, t_lo;
  double tmp;
  double e;
  int bhi, blo;

  /* compute log(a) in double-double format*/
  loga = __internal_log_ext_prec(a);

  /* prevent overflow during extended precision multiply by clamping b */
  bhi = __double2hiint(b);
  blo = __double2loint(b);
  if (((unsigned int)bhi + bhi) >= 0xfe000000U) bhi &= 0xff0fffff;
  b = __hiloint2double (bhi, blo);

  /* compute b * log(a) in double-double format */
  t_hi = __dmul_rn (loga.y, b);   /* prevent FMA-merging */
  t_lo = __fma_rn (loga.y, b, -t_hi);
  t_lo = __fma_rn (loga.x, b, t_lo);
  prod.y = e = t_hi + t_lo;
  prod.x = (t_hi - e) + t_lo;

  /* compute pow(a,b) = exp(b*log(a)) */
  tmp = exp(prod.y);
  /* prevent -INF + INF = NaN */
  if (!__isinfd(tmp)) {
    /* if prod.x is much smaller than prod.y, then exp(prod.y + prod.x) ~= 
     * exp(prod.y) + prod.x * exp(prod.y) 
     */
    tmp = __fma_rn (tmp, prod.x, tmp);
  }
  return tmp;
}

static __forceinline__ double pow(double a, double b)
{
  double t;
  int ahi, bhi, thi, bIsOddInteger;

  ahi = __double2hiint (a);
  bhi = __double2hiint (b);
  bIsOddInteger = fabs (b - (2.0 * trunc (0.5 * b))) == 1.0;
  if ((a == 1.0) || (b == 0.0)) {
    t = 1.0;
  } else if (__isnand (a) || __isnand (b)) {
    t = a + b;
  } else if (__isinfd (b)) {
    thi = 0;
    if (fabs(a) > 1.0) thi = 0x7ff00000;
    if (bhi < 0) thi = thi ^ 0x7ff00000;
    if (a == -1.0) thi = 0x3ff00000;
    t = __hiloint2double (thi, 0);
  } else if (__isinfd (a)) {
    thi = 0;
    if (bhi >= 0) thi = 0x7ff00000;
    if ((ahi < 0) && bIsOddInteger) thi = thi ^ 0x80000000;
    t = __hiloint2double (thi, 0);
  } else if (a == 0.0) {
    thi = 0;
    if (bIsOddInteger) thi = ahi;
    if (bhi < 0) thi = thi | 0x7ff00000;
    t = __hiloint2double (thi, 0);
  } else if ((ahi < 0) && (b != trunc (b))) {
    t = CUDART_NAN;
  } else {
    t = __internal_accurate_pow (fabs (a), b);
    if ((ahi < 0) && bIsOddInteger) {
      t = __internal_neg (t); 
    }
  }
  return t;
}

static __forceinline__ double j0(double a)
{
  double t, r, x;
  r = 0.0;
  t = fabs(a);
  if (t <= 3.962451833991041709e0) {
    x = ((t - 2.404825557695772886e0) - -1.176691651530894036e-16);
    r = -4.668055296522885552e-16;
    r = __fma_rn(r, x, -6.433200527258554127e-15);
    r = __fma_rn(r, x, 1.125154785441239563e-13);
    r = __fma_rn(r, x, 1.639521934089839047e-12);
    r = __fma_rn(r, x, -2.534199601670673987e-11);
    r = __fma_rn(r, x, -3.166660834754117150e-10);
    r = __fma_rn(r, x, 4.326570922239416813e-9);
    r = __fma_rn(r, x, 4.470057037570427580e-8);
    r = __fma_rn(r, x, -5.304914441394479122e-7);
    r = __fma_rn(r, x, -4.338826303234108986e-6);
    r = __fma_rn(r, x, 4.372919273219640746e-5);
    r = __fma_rn(r, x, 2.643770367619977359e-4);
    r = __fma_rn(r, x, -2.194200359017061189e-3);
    r = __fma_rn(r, x, -8.657669593307546971e-3);
    r = __fma_rn(r, x, 5.660177443794636720e-2);
    r = __fma_rn(r, x, 1.079387017549203048e-1);
    r = __fma_rn(r, x, -5.191474972894667417e-1);
    r *= x;
  } else if (t <= 7.086903011598661433e0) {
    x = ((t - 5.520078110286310569e0) - 8.088597146146722332e-17);
    r = 3.981548125960367572e-16;
    r = __fma_rn(r, x, 5.384425646000319613e-15);
    r = __fma_rn(r, x, -1.208169028319422770e-13);
    r = __fma_rn(r, x, -1.379791615846302261e-12);
    r = __fma_rn(r, x, 2.745222536512400531e-11);
    r = __fma_rn(r, x, 2.592191169087820231e-10);
    r = __fma_rn(r, x, -4.683395694900245463e-9);
    r = __fma_rn(r, x, -3.511535752914609294e-8);
    r = __fma_rn(r, x, 5.716490702257101151e-7);
    r = __fma_rn(r, x, 3.199786905053059080e-6);
    r = __fma_rn(r, x, -4.652109073941537520e-5);
    r = __fma_rn(r, x, -1.751857289934499263e-4);
    r = __fma_rn(r, x, 2.257440229032805189e-3);
    r = __fma_rn(r, x, 4.631042145907517116e-3);
    r = __fma_rn(r, x, -5.298855286760461442e-2);
    r = __fma_rn(r, x, -3.082065142559364118e-2);
    r = __fma_rn(r, x, 3.402648065583681602e-1);
    r *= x;
  } else if (t <= 1.022263117596264692e1) {
    x = ((t - 8.653727912911012510e0) - -2.928126073207789799e-16);
    r = -4.124304662099804879e-16;
    r = __fma_rn(r, x, -4.596716020545263225e-15);
    r = __fma_rn(r, x, 1.243104269818899322e-13);
    r = __fma_rn(r, x, 1.149516171925282771e-12);
    r = __fma_rn(r, x, -2.806255120718408997e-11);
    r = __fma_rn(r, x, -2.086671689271728758e-10);
    r = __fma_rn(r, x, 4.736806709085623724e-9);
    r = __fma_rn(r, x, 2.694156819104033891e-8);
    r = __fma_rn(r, x, -5.679379510457043302e-7);
    r = __fma_rn(r, x, -2.288391007218622664e-6);
    r = __fma_rn(r, x, 4.482303544494819955e-5);
    r = __fma_rn(r, x, 1.124348678929902644e-4);
    r = __fma_rn(r, x, -2.060335155125843105e-3);
    r = __fma_rn(r, x, -2.509302227210569083e-3);
    r = __fma_rn(r, x, 4.403377496341183417e-2);
    r = __fma_rn(r, x, 1.568412496095387618e-2);
    r = __fma_rn(r, x, -2.714522999283819349e-1);
    r *= x;
  } else if (!__isinfd(t)) {
    double y = __internal_fast_rcp(t);
    double y2 = y * y;
    double f, arg;
    f = -1.749518042413318545e4;
    f = __fma_rn(f, y2, 1.609818826277744392e3);
    f = __fma_rn(f, y2, -9.327297929346906358e1);
    f = __fma_rn(f, y2, 5.754657357710742716e0);
    f = __fma_rn(f, y2, -5.424139391385890407e-1);
    f = __fma_rn(f, y2, 1.035143619926359032e-1);
    f = __fma_rn(f, y2, -6.249999788858900951e-2);
    f = __fma_rn(f, y2, 9.999999999984622301e-1);
    arg = -2.885116220349355482e6;
    arg = __fma_rn(arg, y2, 2.523286424277686747e5);
    arg = __fma_rn(arg, y2, -1.210196952664123455e4);
    arg = __fma_rn(arg, y2, 4.916296687065029687e2);
    arg = __fma_rn(arg, y2, -2.323271029624128303e1);
    arg = __fma_rn(arg, y2, 1.637144946408570334e0);
    arg = __fma_rn(arg, y2, -2.095680312729443495e-1);
    arg = __fma_rn(arg, y2, 6.510416335987831427e-2);
    arg = __fma_rn(arg, y2, -1.249999999978858578e-1);
    arg = __fma_rn(arg, y, t);
    r = rsqrt(t) * CUDART_SQRT_2OPI * f * 
        __internal_cos_offset(arg, -7.8539816339744831e-1);
  } else {
    /* Input is infinite. */
    r = 0.0;
  }
  return r;
}

static __forceinline__ double j1(double a)
{
  double t, r, x;
  r = 0.0;
  t = fabs(a);
  if (t <= 2.415852985103756012e0) {
    x = ((t - 0.000000000000000000e-1) - 0.000000000000000000e-1);
    r = 8.018399195792647872e-15;
    r = __fma_rn(r, x, -2.118695440834766210e-13);
    r = __fma_rn(r, x, 2.986477477755093929e-13);
    r = __fma_rn(r, x, 3.264658690505054749e-11);
    r = __fma_rn(r, x, 2.365918244990000764e-12);
    r = __fma_rn(r, x, -5.655535980321211576e-9);
    r = __fma_rn(r, x, 5.337726421910612559e-12);
    r = __fma_rn(r, x, 6.781633105423295953e-7);
    r = __fma_rn(r, x, 3.551463066921223471e-12);
    r = __fma_rn(r, x, -5.425347399642436942e-5);
    r = __fma_rn(r, x, 6.141520947159623346e-13);
    r = __fma_rn(r, x, 2.604166666526797937e-3);
    r = __fma_rn(r, x, 1.929721653824376829e-14);
    r = __fma_rn(r, x, -6.250000000000140166e-2);
    r = __fma_rn(r, x, 4.018089105880317857e-17);
    r = __fma_rn(r, x, 5.000000000000000000e-1);
    r *= x;
  } else if (t <= 5.423646320011565535e0) {
    x = ((t - 3.831705970207512468e0) - -1.526918409008806686e-16);
    r = -5.512780891825248469e-15;
    r = __fma_rn(r, x, 1.208228522598007249e-13);
    r = __fma_rn(r, x, 1.250828223475420523e-12);
    r = __fma_rn(r, x, -2.797792344085172005e-11);
    r = __fma_rn(r, x, -2.362345221426392649e-10);
    r = __fma_rn(r, x, 4.735362223346154893e-9);
    r = __fma_rn(r, x, 3.248288715654640665e-8);
    r = __fma_rn(r, x, -5.727805561466869718e-7);
    r = __fma_rn(r, x, -3.036863401211637746e-6);
    r = __fma_rn(r, x, 4.620870128840665444e-5);
    r = __fma_rn(r, x, 1.746642907294104828e-4);
    r = __fma_rn(r, x, -2.233125339145115504e-3);
    r = __fma_rn(r, x, -5.179719245640395341e-3);
    r = __fma_rn(r, x, 5.341044413272456881e-2);
    r = __fma_rn(r, x, 5.255614585697734181e-2);
    r = __fma_rn(r, x, -4.027593957025529803e-1);
    r *= x;
  } else if (t <= 8.594527402439170415e0) {
    x = ((t - 7.015586669815618848e0) - -9.414165653410388908e-17);
    r = 4.423133061281035160e-15;
    r = __fma_rn(r, x, -1.201320120922480112e-13);
    r = __fma_rn(r, x, -1.120851060072903875e-12);
    r = __fma_rn(r, x, 2.798783538427610697e-11);
    r = __fma_rn(r, x, 2.065329706440647244e-10);
    r = __fma_rn(r, x, -4.720444222309518119e-9);
    r = __fma_rn(r, x, -2.727342515669842039e-8);
    r = __fma_rn(r, x, 5.665269543584226731e-7);
    r = __fma_rn(r, x, 2.401580794492155375e-6);
    r = __fma_rn(r, x, -4.499147527210508836e-5);
    r = __fma_rn(r, x, -1.255079095508101735e-4);
    r = __fma_rn(r, x, 2.105587143238240189e-3);
    r = __fma_rn(r, x, 3.130291726048001991e-3);
    r = __fma_rn(r, x, -4.697047894974023391e-2);
    r = __fma_rn(r, x, -2.138921280934158106e-2);
    r = __fma_rn(r, x, 3.001157525261325398e-1);
    r *= x;
  } else if (!__isinfd(t)) {
    double y = __internal_fast_rcp(t);
    double y2 = y * y;
    double f, arg;
    f = 1.485383005325836814e4;
    f = __fma_rn(f, y2, -1.648096811830575007e3);
    f = __fma_rn(f, y2, 1.101438783774615899e2);
    f = __fma_rn(f, y2, -7.551889723469123794e0);
    f = __fma_rn(f, y2, 8.042591538676234775e-1);
    f = __fma_rn(f, y2, -1.933557706160460576e-1);
    f = __fma_rn(f, y2, 1.874999929278536315e-1);
    f = __fma_rn(f, y2, 1.000000000005957013e0);
    arg = -6.214794014836998139e7;
    arg = __fma_rn(arg, y2, 6.865585630355566740e6);
    arg = __fma_rn(arg, y2, -3.832405418387809768e5);
    arg = __fma_rn(arg, y2, 1.571235974698157042e4);
    arg = __fma_rn(arg, y2, -6.181902458868638632e2);
    arg = __fma_rn(arg, y2, 3.039697998508859911e1);
    arg = __fma_rn(arg, y2, -2.368515193214345782e0);
    arg = __fma_rn(arg, y2, 3.708961732933458433e-1);
    arg = __fma_rn(arg, y2, -1.640624965735098806e-1);
    arg = __fma_rn(arg, y2, 3.749999999976813547e-1);
    arg = __fma_rn(arg, y, t);
    r = rsqrt(t) * CUDART_SQRT_2OPI * f * 
        __internal_cos_offset(arg, -2.3561944901923449e0); 
  } else {
    /* Input is infinite. */
    r = 0.0;
  }
  if (a < 0.0) {
    r = -r;
  }
  if (t < 1e-30) {
    r = a * 0.5;
  }
  return r;
}

static __forceinline__ double y0(double a)
{
  double t, r, x;
  r = 0.0;
  t = fabs(a);
  if (t <= 7.967884831395837253e-1) {
    x = t * t;
    r = 5.374806887266719984e-17;
    r = __fma_rn(r, x, -1.690851667879507126e-14);
    r = __fma_rn(r, x, 4.136256698488524230e-12);
    r = __fma_rn(r, x, -7.675202391864751950e-10);
    r = __fma_rn(r, x, 1.032530269160133847e-7);
    r = __fma_rn(r, x, -9.450377743948014966e-6);
    r = __fma_rn(r, x, 5.345180760328465709e-4);
    r = __fma_rn(r, x, -1.584294153256949819e-2);
    r = __fma_rn(r, x, 1.707584669151278045e-1);
    r *= (x - 4.322145581245422363e-1) - -1.259433890510308629e-9;
    r += CUDART_2_OVER_PI * log(t) * j0(t);
  } else if (t <= 2.025627692797012713e0) {
    x = ((t - 8.935769662791674950e-1) - 2.659623153972038487e-17);
    r = -3.316256912072560202e-5;
    r = __fma_rn(r, x, 4.428203736344834521e-4);
    r = __fma_rn(r, x, -2.789856306341642004e-3);
    r = __fma_rn(r, x, 1.105846367024121250e-2);
    r = __fma_rn(r, x, -3.107223394960596102e-2);
    r = __fma_rn(r, x, 6.626287772780777019e-2);
    r = __fma_rn(r, x, -1.125221809100773462e-1);
    r = __fma_rn(r, x, 1.584073414576677719e-1);
    r = __fma_rn(r, x, -1.922273494240156200e-1);
    r = __fma_rn(r, x, 2.093393446684197468e-1);
    r = __fma_rn(r, x, -2.129333765401472400e-1);
    r = __fma_rn(r, x, 2.093702358334368907e-1);
    r = __fma_rn(r, x, -2.037455528835861451e-1);
    r = __fma_rn(r, x, 1.986558106005199553e-1);
    r = __fma_rn(r, x, -1.950678188917356060e-1);
    r = __fma_rn(r, x, 1.933768292594399973e-1);
    r = __fma_rn(r, x, -1.939501240454329922e-1);
    r = __fma_rn(r, x, 1.973356651370720138e-1);
    r = __fma_rn(r, x, -2.048771973714162697e-1);
    r = __fma_rn(r, x, 2.189484270119261000e-1);
    r = __fma_rn(r, x, -2.261217135462367245e-1);
    r = __fma_rn(r, x, 2.205528284817022400e-1);
    r = __fma_rn(r, x, -4.920789342629753871e-1);
    r = __fma_rn(r, x, 8.794208024971947868e-1);
    r *= x;
  } else if (t <= 5.521864739808315283e0) {
    x = ((t - 3.957678419314857976e0) - -1.076434069756270603e-16);
    r = -1.494114173821677059e-15;
    r = __fma_rn(r, x, -1.013791156119442377e-15);
    r = __fma_rn(r, x, 1.577311216240784649e-14);
    r = __fma_rn(r, x, 3.461700831703228390e-14);
    r = __fma_rn(r, x, -1.390049111128330285e-13);
    r = __fma_rn(r, x, -2.651585913591809710e-14);
    r = __fma_rn(r, x, -2.563422432591884445e-13);
    r = __fma_rn(r, x, 3.152125074327968061e-12);
    r = __fma_rn(r, x, -1.135177389965644664e-11);
    r = __fma_rn(r, x, 4.326378313976470202e-11);
    r = __fma_rn(r, x, -1.850879474448778845e-10);
    r = __fma_rn(r, x, 7.689088938316559034e-10);
    r = __fma_rn(r, x, -3.657694558233732877e-9);
    r = __fma_rn(r, x, 1.892629263079880039e-8);
    r = __fma_rn(r, x, -2.185282420222553349e-8);
    r = __fma_rn(r, x, -2.934871156586473999e-7);
    r = __fma_rn(r, x, -4.893369556967850888e-6);
    r = __fma_rn(r, x, 5.092291346093084947e-5);
    r = __fma_rn(r, x, 1.952694025023884918e-4);
    r = __fma_rn(r, x, -2.183518873989655565e-3);
    r = __fma_rn(r, x, -6.852566677116652717e-3);
    r = __fma_rn(r, x, 5.852382210516620525e-2);
    r = __fma_rn(r, x, 5.085590959215843115e-2);
    r = __fma_rn(r, x, -4.025426717750241745e-1);
    r *= x;
  } else if (t <= 8.654198051899094858e0) {
    x = ((t - 7.086051060301772786e0) - -8.835285723085408128e-17);
    r = 3.951031695740590034e-15;
    r = __fma_rn(r, x, -1.110810503294961990e-13);
    r = __fma_rn(r, x, -1.310829469053465703e-12);
    r = __fma_rn(r, x, 2.824621267525193929e-11);
    r = __fma_rn(r, x, 2.302923649674420956e-10);
    r = __fma_rn(r, x, -4.717174021172401832e-9);
    r = __fma_rn(r, x, -3.098470041689314533e-8);
    r = __fma_rn(r, x, 5.749349008560620678e-7);
    r = __fma_rn(r, x, 2.701363791846417715e-6);
    r = __fma_rn(r, x, -4.595140667075523833e-5);
    r = __fma_rn(r, x, -1.406025977407872123e-4);
    r = __fma_rn(r, x, 2.175984016431612746e-3);
    r = __fma_rn(r, x, 3.318348268895694383e-3);
    r = __fma_rn(r, x, -4.802407007625847379e-2);
    r = __fma_rn(r, x, -2.117523655676954025e-2);
    r = __fma_rn(r, x, 3.000976149104751523e-1);
    r *= x;
  } else if (!__isinfd(t)) {
    double y = __internal_fast_rcp(t);
    double y2 = y * y;
    double f, arg;
    f = -1.121823763318965797e4;
    f = __fma_rn(f, y2, 1.277353533221286625e3);
    f = __fma_rn(f, y2, -8.579416608392857313e1);
    f = __fma_rn(f, y2, 5.662125060937317933e0);
    f = __fma_rn(f, y2, -5.417345171533867187e-1);
    f = __fma_rn(f, y2, 1.035114040728313117e-1);
    f = __fma_rn(f, y2, -6.249999082419847168e-2);
    f = __fma_rn(f, y2, 9.999999999913266047e-1);
    arg = 5.562900148486682495e7;
    arg = __fma_rn(arg, y2, -6.039326416769045405e6);
    arg = __fma_rn(arg, y2, 3.303804467797655961e5);
    arg = __fma_rn(arg, y2, -1.320780106166394580e4);
    arg = __fma_rn(arg, y2, 5.015151566589033791e2);
    arg = __fma_rn(arg, y2, -2.329056718317451669e1);
    arg = __fma_rn(arg, y2, 1.637366947135598716e0);
    arg = __fma_rn(arg, y2, -2.095685710525915790e-1);
    arg = __fma_rn(arg, y2, 6.510416411708590256e-2);
    arg = __fma_rn(arg, y2, -1.249999999983588544e-1);
    arg = __fma_rn(arg, y, t);
    r = rsqrt(t) * CUDART_SQRT_2OPI * f * 
        __internal_cos_offset(arg, -2.356194490192344929e0);
  } else {
    /* Input is infinite. */
    r = 0.0;
  }
  if (a < 0.0) {
    r = CUDART_NAN;
  }
  return r;
}

static __forceinline__ double y1(double a)
{
  double t, r, x;
  r = 0.0;
  t = fabs(a);
  if (t < 1e-308) {
    /* Denormalized inputs need care to avoid overflow on 1/t */
    r = -CUDART_2_OVER_PI / t;
  } else if (t <= 1.298570663015508497e0) {
    x = t * t;
    r = 2.599016977114429789e-13;
    r = __fma_rn(r, x, -5.646936040707309767e-11);
    r = __fma_rn(r, x, 8.931867331036295581e-9);
    r = __fma_rn(r, x, -9.926740542145188722e-7);
    r = __fma_rn(r, x, 7.164268749708438400e-5);
    r = __fma_rn(r, x, -2.955305336079382290e-3);
    r = __fma_rn(r, x, 5.434868816051021539e-2);
    r = __fma_rn(r, x, -1.960570906462389407e-1);
    r *= t;
    r += CUDART_2_OVER_PI * (log(t) * j1(t) - 1.0 / t);
  } else if (t <= 3.213411183412576033e0) {
    x = ((t - 2.197141326031017083e0) - -4.825983587645496567e-17);
    r = -3.204918540045980739e-9;
    r = __fma_rn(r, x, 1.126985362938592444e-8);
    r = __fma_rn(r, x, -9.725182107962382221e-9);
    r = __fma_rn(r, x, 1.083612003186428926e-9);
    r = __fma_rn(r, x, -3.318806432859500986e-8);
    r = __fma_rn(r, x, 1.152009920780307640e-7);
    r = __fma_rn(r, x, -2.165762322547769634e-7);
    r = __fma_rn(r, x, 4.248883280005704350e-7);
    r = __fma_rn(r, x, -9.597291015128258274e-7);
    r = __fma_rn(r, x, 2.143651955073189370e-6);
    r = __fma_rn(r, x, -4.688317848511307222e-6);
    r = __fma_rn(r, x, 1.026066296099274397e-5);
    r = __fma_rn(r, x, -2.248872084380127776e-5);
    r = __fma_rn(r, x, 4.924499594496305443e-5);
    r = __fma_rn(r, x, -1.077609598179235436e-4);
    r = __fma_rn(r, x, 2.358698833633901006e-4);
    r = __fma_rn(r, x, -5.096012361553002188e-4);
    r = __fma_rn(r, x, 1.066853008500809634e-3);
    r = __fma_rn(r, x, -2.595241693183597629e-3);
    r = __fma_rn(r, x, 7.422553332334889779e-3);
    r = __fma_rn(r, x, -4.797811669942416563e-3);
    r = __fma_rn(r, x, -3.285739740527982705e-2);
    r = __fma_rn(r, x, -1.185145457490981991e-1);
    r = __fma_rn(r, x, 5.207864124022675290e-1);
    r *= x;
  } else if (t <= 7.012843454562652030e0) {
    x = ((t - 5.429681040794134717e0) - 4.162514026670377007e-16);
    r = 3.641000824697897087e-16;
    r = __fma_rn(r, x, 6.273399595774693961e-16);
    r = __fma_rn(r, x, -1.656717829265264444e-15);
    r = __fma_rn(r, x, -1.793477656341538960e-14);
    r = __fma_rn(r, x, 4.410546816390020042e-14);
    r = __fma_rn(r, x, -1.387851333205382620e-13);
    r = __fma_rn(r, x, 1.170075916815038820e-12);
    r = __fma_rn(r, x, -4.612886656846937267e-12);
    r = __fma_rn(r, x, 2.222126653072601592e-12);
    r = __fma_rn(r, x, -3.852562731318657049e-10);
    r = __fma_rn(r, x, 5.598172933325135304e-9);
    r = __fma_rn(r, x, 2.550481704211604017e-8);
    r = __fma_rn(r, x, -5.464422265470442015e-7);
    r = __fma_rn(r, x, -2.863862325810848798e-6);
    r = __fma_rn(r, x, 4.645867915733586050e-5);
    r = __fma_rn(r, x, 1.466208928172848137e-4);
    r = __fma_rn(r, x, -2.165998751115648553e-3);
    r = __fma_rn(r, x, -4.160115934377754676e-3);
    r = __fma_rn(r, x, 5.094793974342303605e-2);
    r = __fma_rn(r, x, 3.133867744408601330e-2);
    r = __fma_rn(r, x, -3.403180455234405821e-1);
    r *= x;
  } else if (t <= 9.172580349585524928e0) {
    x = ((t - 8.596005868331168642e0) - 2.841583834006366401e-16);
    r = 2.305446091542135639e-16;
    r = __fma_rn(r, x, -1.372616651279859895e-13);
    r = __fma_rn(r, x, -1.067085198258553687e-12);
    r = __fma_rn(r, x, 2.797080742350623921e-11);
    r = __fma_rn(r, x, 1.883663311130206595e-10);
    r = __fma_rn(r, x, -4.684316504597157100e-9);
    r = __fma_rn(r, x, -2.441923258474869187e-8);
    r = __fma_rn(r, x, 5.586530988420728856e-7);
    r = __fma_rn(r, x, 2.081926450587367740e-6);
    r = __fma_rn(r, x, -4.380739676566903498e-5);
    r = __fma_rn(r, x, -1.042014850604930338e-4);
    r = __fma_rn(r, x, 2.011492014389694005e-3);
    r = __fma_rn(r, x, 2.417956732829416259e-3);
    r = __fma_rn(r, x, -4.340642670740071929e-2);
    r = __fma_rn(r, x, -1.578988436429690570e-2);
    r = __fma_rn(r, x, 2.714598773115335373e-1);
    r *= x;
  } else if (!__isinfd(t)) {
    double y = __internal_fast_rcp(t);
    double y2 = y * y;
    double f, arg;
    f = 1.765479925082250655e4;
    f = __fma_rn(f, y2, -1.801727125254790963e3);
    f = __fma_rn(f, y2, 1.136675500338510290e2);
    f = __fma_rn(f, y2, -7.595622833654403827e0);
    f = __fma_rn(f, y2, 8.045758488114477247e-1);
    f = __fma_rn(f, y2, -1.933571068757167499e-1);
    f = __fma_rn(f, y2, 1.874999959666924232e-1);
    f = __fma_rn(f, y2, 1.000000000003085088e0);
    arg = -8.471357607824940103e7;
    arg = __fma_rn(arg, y2, 8.464204863822212443e6);
    arg = __fma_rn(arg, y2, -4.326694608144371887e5);
    arg = __fma_rn(arg, y2, 1.658700399613585250e4);
    arg = __fma_rn(arg, y2, -6.279420695465894369e2);
    arg = __fma_rn(arg, y2, 3.046796375066591622e1);
    arg = __fma_rn(arg, y2, -2.368852258237428732e0);
    arg = __fma_rn(arg, y2, 3.708971794716567350e-1);
    arg = __fma_rn(arg, y2, -1.640624982860321990e-1);
    arg = __fma_rn(arg, y2, 3.749999999989471755e-1);
    arg = __fma_rn(arg, y, t);
    r = rsqrt(t) * CUDART_SQRT_2OPI * f * 
        __internal_cos_offset(arg, -3.926990816987241548e0);
  } else {
    r = 0.0;
  }
  if (a <= 0.0) {
    if (a == 0.0) {
      r = -CUDART_INF;
    } else {
      r = CUDART_NAN;
    }
  }
  return r;
}

/* Bessel functions of the second kind of integer
 * order are calculated using the forward recurrence:
 * Y(n+1, x) = 2n Y(n, x) / x - Y(n-1, x)
 */
static __forceinline__ double yn(int n, double a)
{
  double yip1; // is Y(i+1, a)
  double yi = y1(a); // is Y(i, a)
  double yim1 = y0(a); // is Y(i-1, a)
  double two_over_a;
  int i;
  if(n == 0) {
    return y0(a);
  }
  if(n == 1) {
    return y1(a);
  }
  if(n < 0) {
    return CUDART_NAN;
  }
  if(!(a >= 0.0)) {
    // also catches NAN input
    return CUDART_NAN;
  }
  if (fabs(a) < 1e-308) {
    /* Denormalized inputs need care to avoid overflow on 1/a */
    return -CUDART_2_OVER_PI / a;
  }
  two_over_a = 2.0 / a;
  for(i = 1; i < n; i++) {
    // Use forward recurrence, 6.8.4 from Hart et al.
    yip1 = __fma_rn(i * two_over_a,  yi, -yim1);
    yim1 = yi;
    yi = yip1;
  }
  if(__isnand(yip1)) {
    // We got overflow in forward recurrence
    return -CUDART_INF;
  }
  return yip1;
}

/* Bessel functions of the first kind of integer
 * order are calculated directly using the forward recurrence:
 * J(n+1, x) = 2n J(n, x) / x - J(n-1, x)
 * for large x.  For x small relative to n, Miller's algorithm is used 
 * as described in: F. W. J. Olver, "Error Analysis of Miller's Recurrence
 * Algorithm", Math. of Computation, Vol. 18, No. 85, 1964.
 */
static __forceinline__ double jn(int n, double a)
{
  double jip1 = 0.0; // is J(i+1, a)
  double ji = 1.0; // is J(i, a)
  double jim1; // is J(i-1, a)
  double lambda = 0.0;
  double sum = 0.0;
  int i;
  if(n == 0) {
    return j0(a);
  }
  if(n == 1) {
    return j1(a);
  }
  if(n < 0) {
    return CUDART_NAN;
  }
  if(fabs(a) > (double)n - 1.0) {
    // Use forward recurrence, numerically stable for large x
    double two_over_a = 2.0 / a;
    double ji = j1(a); // is J(i, a)
    double jim1 = j0(a); // is J(i-1, a)
    for(i = 1; i < n; i++) {
      jip1 = __fma_rn(i * two_over_a, ji, -jim1);
      jim1 = ji;
      ji = jip1;
    }
    return jip1;
  } else {
    /* Limit m based on comments from Press et al. "Numerical Recipes
       in C", 2nd ed., p. 234--235, 1992. 
    */
    double two_over_a = 2.0 / a;
    int m = n + (int)sqrt(n * 60);
    m = (m >> 1) << 1;
    for(i = m; i >= 1; --i) {
      // Use backward recurrence
      jim1 = __fma_rn(i * two_over_a, ji, -jip1);
      jip1 = ji;
      // Rescale to avoid intermediate overflow
      if(fabsf(jim1) > 1e15) {
        jim1 *= 1e-15;
        jip1 *= 1e-15;
        lambda *= 1e-15;
        sum *= 1e-15;
      }
      ji = jim1;
      if(i - 1 == n) {
        lambda = ji;
      }
      if(i & 1) {
        sum += 2.0 * ji;
      }
    }
    sum -= ji;
    return lambda / sum;
  }
}

static __forceinline__ double cyl_bessel_i0 (double a)
{
  double p, q;

  a = fabs (a);
  if ((__double2hiint(a) >= 0x402619fc) && (__double2hiint(a) < 0x47c00000)) {
    q = __internal_fast_rcp (a);
    p =                  5.0922208662175570E+009;
    p = __fma_rn (p, q,  8.5730152952159128E+009);
    p = __fma_rn (p, q, -7.1241278713436127E+009);
    p = __fma_rn (p, q,  2.2876821961596842E+009);
    p = __fma_rn (p, q, -4.2191983179961109E+008);
    p = __fma_rn (p, q,  5.1006516886431605E+007);
    p = __fma_rn (p, q, -4.3048725741130961E+006);
    p = __fma_rn (p, q,  2.6238701332680928E+005);
    p = __fma_rn (p, q, -1.1726458210321365E+004);
    p = __fma_rn (p, q,  3.8988079458597497E+002);
    p = __fma_rn (p, q, -8.7117199960996032E+000);
    p = __fma_rn (p, q,  3.9385235358350168E-001);
    p = __fma_rn (p, q,  8.8538914280921907E-002);
    p = __fma_rn (p, q,  4.4759823399561180E-002);
    p = __fma_rn (p, q,  2.9219307956309174E-002);
    p = __fma_rn (p, q,  2.8050629411665751E-002);
    p = __fma_rn (p, q,  4.9867785049630471E-002);
    p = __fma_rn (p, q,  3.9894228040143304E-001);
    q = __internal_fast_rsqrt (a);
    p = p * q;
    q = exp (0.5 * a); /* prevent premature overflow */
    p = p * q;
    p = p * q;
  } else {
    q = a * a;
    p =                 9.5709075556724118E-040;
    p = __fma_rn (p, q, 2.9112378096155269E-037);
    p = __fma_rn (p, q, 6.1067895679240757E-034);
    p = __fma_rn (p, q, 4.7831395577480274E-031);
    p = __fma_rn (p, q, 3.8577538849440383E-028);
    p = __fma_rn (p, q, 2.5964593691150315E-025);
    p = __fma_rn (p, q, 1.4964235720813853E-022);
    p = __fma_rn (p, q, 7.2422148393887080E-020);
    p = __fma_rn (p, q, 2.8969049594568713E-017);
    p = __fma_rn (p, q, 9.3859665827043433E-015);
    p = __fma_rn (p, q, 2.4028075570232502E-012);
    p = __fma_rn (p, q, 4.7095027961268531E-010);
    p = __fma_rn (p, q, 6.7816840278540226E-008);
    p = __fma_rn (p, q, 6.7816840277741182E-006);
    p = __fma_rn (p, q, 4.3402777777778675E-004);
    p = __fma_rn (p, q, 1.5624999999999991E-002);
    p = __fma_rn (p, q, 2.5000000000000000E-001);
    p = __fma_rn (p, q, 1.0000000000000000E+000);
  }
  return p;
}

static __forceinline__ double cyl_bessel_i1 (double a)
{
  double p, q, t;

  t = fabs (a);
  if ((__double2hiint(t) >= 0x40260000) && (__double2hiint(t) < 0x47c00000)) {
    q = __internal_fast_rcp (t);
    p =                  1.6406585915879254E+009;
    p = __fma_rn (p, q, -1.4279800016127026E+010);
    p = __fma_rn (p, q,  9.3223461047499390E+009);
    p = __fma_rn (p, q, -2.7948986417836614E+009);
    p = __fma_rn (p, q,  5.0003957903130484E+008);
    p = __fma_rn (p, q, -5.9499968247650094E+007);
    p = __fma_rn (p, q,  4.9776454973910600E+006);
    p = __fma_rn (p, q, -3.0190721112817561E+005);
    p = __fma_rn (p, q,  1.3459341882089166E+004);
    p = __fma_rn (p, q, -4.4703773216640997E+002);
    p = __fma_rn (p, q,  9.9862744494351148E+000);
    p = __fma_rn (p, q, -4.5985068950833535E-001);
    p = __fma_rn (p, q, -1.0836417269190061E-001);
    p = __fma_rn (p, q, -5.7545984086566769E-002);
    p = __fma_rn (p, q, -4.0907055084192644E-002);
    p = __fma_rn (p, q, -4.6751048855679919E-002);
    p = __fma_rn (p, q, -1.4960335514990164E-001);
    p = __fma_rn (p, q,  3.9894228040143226E-001);
    q = __internal_fast_rsqrt (t);
    p = __dmul_rn (p, q);
    q = exp (0.5 * t);  /* prevent premature overflow */
    p = __dmul_rn (p, q);
    p = __dmul_rn (p, q);
    p = __internal_copysign_pos (p, a);
  } else {
    q = __dmul_rn (a, a);
    p =                 3.3137253377452213E-038;
    p = __fma_rn (p, q, 8.8095137413036301E-036);
    p = __fma_rn (p, q, 1.8513564651143229E-032);
    p = __fma_rn (p, q, 1.3349060575742374E-029);
    p = __fma_rn (p, q, 1.0036698880562031E-026);
    p = __fma_rn (p, q, 6.2308219390238368E-024);
    p = __fma_rn (p, q, 3.2921824758704519E-021);
    p = __fma_rn (p, q, 1.4484402483481260E-018);
    p = __fma_rn (p, q, 5.2144299818133838E-016);
    p = __fma_rn (p, q, 1.5017546240146120E-013);
    p = __fma_rn (p, q, 3.3639305854901483E-011);
    p = __fma_rn (p, q, 5.6514033546125956E-009);
    p = __fma_rn (p, q, 6.7816840279158899E-007);
    p = __fma_rn (p, q, 5.4253472222162520E-005);
    p = __fma_rn (p, q, 2.6041666666667966E-003);
    p = __fma_rn (p, q, 6.2499999999999896E-002);
    p = __dmul_rn (p, q);
    q = 0.5 * a;
    p = __fma_rn (a, p, q);
  }
  return p;
}


static __forceinline__ double erf(double a)
{
  double t, r, q;
  int thi, tlo;

  thi = __double2hiint (a) & 0x7fffffff;
  tlo = __double2loint (a);
  if (thi < 0x3ff00000) {
    q = a * a;
    r =                 -7.7166816519184167E-010;
    r = __fma_rn (r, q,  1.3677519329265672E-008);
    r = __fma_rn (r, q, -1.6198625734382580E-007);
    r = __fma_rn (r, q,  1.6446135110441203E-006);
    r = __fma_rn (r, q, -1.4924632258758608E-005);
    r = __fma_rn (r, q,  1.2055289455137751E-004);
    r = __fma_rn (r, q, -8.5483257933145144E-004);
    r = __fma_rn (r, q,  5.2239776033277674E-003);
    r = __fma_rn (r, q, -2.6866170642778277E-002);
    r = __fma_rn (r, q,  1.1283791670942150E-001);
    r = __fma_rn (r, q, -3.7612638903183471E-001);
    r = __fma_rn (r, q,  1.1283791670955126E+000);
    a = r * a;
  } else if (thi < 0x7ff00000) {
    t = __hiloint2double (thi, tlo);
    r =                  4.7895783128820029E-017;
    r = __fma_rn (r, t, -3.8235910506463359E-015);
    r = __fma_rn (r, t,  1.4456996699204952E-013);
    r = __fma_rn (r, t, -3.4435871373385631E-012);
    r = __fma_rn (r, t,  5.7973768836833395E-011);
    r = __fma_rn (r, t, -7.3345204772352132E-010);
    r = __fma_rn (r, t,  7.2354184788524057E-009);
    r = __fma_rn (r, t, -5.6961738224275610E-008);
    r = __fma_rn (r, t,  3.6289206755856494E-007);
    r = __fma_rn (r, t, -1.8822735275154204E-006);
    r = __fma_rn (r, t,  7.9261387823959588E-006);
    r = __fma_rn (r, t, -2.6639530891574236E-005);
    r = __fma_rn (r, t,  6.8061390012392822E-005);
    r = __fma_rn (r, t, -1.1262921229640835E-004);
    r = __fma_rn (r, t,  1.7497463232659722E-005);
    r = __fma_rn (r, t,  5.6803056369657159E-004);
    r = __fma_rn (r, t, -1.7016951440205080E-003);
    r = __fma_rn (r, t,  2.3304978103485461E-004);
    r = __fma_rn (r, t,  1.9102842258128881E-002);
    r = __fma_rn (r, t, -1.0275617264260788E-001);
    r = __fma_rn (r, t, -6.3662649697815699E-001);
    r = __fma_rn (r, t, -1.1283775336768613E+000);
    r = __fma_rn (r, t, -1.8055674071995543E-007);
    r = __internal_fast_exp (r);
    if (thi >= 0x4017b000) { /* 5.921875 */
      r = 0.0;
    }    
    r = 1.0 - r;
    a = __internal_copysign_pos (r, a);
  } else if ((thi == 0x7ff00000) && (tlo == 0)) {
    a = __internal_copysign_pos (1.0, a);
  } else {
    a = a + a;
  }
  return a;
}

static __forceinline__ double __internal_erfinv_polyd(double a)
{
  double r, t;
  t = a - 3.125;
  r =                 -3.6444120640178197e-21;
  r = __fma_rn (r, t, -1.6850591381820166e-19);
  r = __fma_rn (r, t,  1.2858480715256400e-18);
  r = __fma_rn (r, t,  1.1157877678025181e-17);
  r = __fma_rn (r, t, -1.3331716628546209e-16);
  r = __fma_rn (r, t,  2.0972767875968562e-17);
  r = __fma_rn (r, t,  6.6376381343583238e-15);
  r = __fma_rn (r, t, -4.0545662729752069e-14);
  r = __fma_rn (r, t, -8.1519341976054722e-14);
  r = __fma_rn (r, t,  2.6335093153082323e-12);
  r = __fma_rn (r, t, -1.2975133253453532e-11);
  r = __fma_rn (r, t, -5.4154120542946279e-11);
  r = __fma_rn (r, t,  1.0512122733215323e-09);
  r = __fma_rn (r, t, -4.1126339803469837e-09);
  r = __fma_rn (r, t, -2.9070369957882005e-08);
  r = __fma_rn (r, t,  4.2347877827932404e-07);
  r = __fma_rn (r, t, -1.3654692000834679e-06);
  r = __fma_rn (r, t, -1.3882523362786469e-05);
  r = __fma_rn (r, t,  1.8673420803405714e-04);
  r = __fma_rn (r, t, -7.4070253416626698e-04);
  r = __fma_rn (r, t, -6.0336708714301491e-03);
  r = __fma_rn (r, t,  2.4015818242558962e-01);
  r = __fma_rn (r, t,  1.6536545626831027e+00);
  return r;
}

/*
 * This erfinv implementation is derived with minor modifications from:
 * Mike Giles, Approximating the erfinv function, GPU Gems 4, volume 2
 * Retrieved from http://www.gpucomputing.net/?q=node/1828 on 8/15/2010
 */
static __forceinline__ double erfinv(double a)
{
  double t, r;

  if (__internal_fast_icmp_abs_lt (__double2hiint (a), 0x3ff00000)) {
    t = __fma_rn (a, -a, 1.0);
    t = __internal_fast_log (t);
    if ((unsigned int)__double2hiint (t) < 0xC0190000U) {
      r = __internal_erfinv_polyd (-t);
    } else {
      t = sqrt (-t);
      if (__double2hiint (t) < 0x40100000) {
        t = t - 3.25;
        r =                  2.2137376921775787e-09;
        r = __fma_rn (r, t,  9.0756561938885391e-08);
        r = __fma_rn (r, t, -2.7517406297064545e-07);
        r = __fma_rn (r, t,  1.8239629214389228e-08);
        r = __fma_rn (r, t,  1.5027403968909828e-06);
        r = __fma_rn (r, t, -4.0138675269815460e-06);
        r = __fma_rn (r, t,  2.9234449089955446e-06);
        r = __fma_rn (r, t,  1.2475304481671779e-05);
        r = __fma_rn (r, t, -4.7318229009055734e-05);
        r = __fma_rn (r, t,  6.8284851459573175e-05);
        r = __fma_rn (r, t,  2.4031110387097894e-05);
        r = __fma_rn (r, t, -3.5503752036284748e-04);
        r = __fma_rn (r, t,  9.5328937973738050e-04);
        r = __fma_rn (r, t, -1.6882755560235047e-03);
        r = __fma_rn (r, t,  2.4914420961078508e-03);
        r = __fma_rn (r, t, -3.7512085075692412e-03);
        r = __fma_rn (r, t,  5.3709145535900636e-03);
        r = __fma_rn (r, t,  1.0052589676941592e+00);
        r = __fma_rn (r, t,  3.0838856104922208e+00);
      } else {
        t = t - 5.0;
        r =                 -2.7109920616438573e-11;
        r = __fma_rn (r, t, -2.5556418169965252e-10);
        r = __fma_rn (r, t,  1.5076572693500548e-09);
        r = __fma_rn (r, t, -3.7894654401267370e-09);
        r = __fma_rn (r, t,  7.6157012080783394e-09);
        r = __fma_rn (r, t, -1.4960026627149240e-08);
        r = __fma_rn (r, t,  2.9147953450901081e-08);
        r = __fma_rn (r, t, -6.7711997758452339e-08);
        r = __fma_rn (r, t,  2.2900482228026655e-07);
        r = __fma_rn (r, t, -9.9298272942317003e-07);
        r = __fma_rn (r, t,  4.5260625972231537e-06);
        r = __fma_rn (r, t, -1.9681778105531671e-05);
        r = __fma_rn (r, t,  7.5995277030017761e-05);
        r = __fma_rn (r, t, -2.1503011930044477e-04);
        r = __fma_rn (r, t, -1.3871931833623122e-04);
        r = __fma_rn (r, t,  1.0103004648645344e+00);
        r = __fma_rn (r, t,  4.8499064014085844e+00);
      }
    }
  } else {
    if (__isnand(a)) {
      r = a;
    } else if (fabs(a) == 1.0) {
      r = CUDART_INF;
    } else {
      r = CUDART_NAN;
    }
  }
  r = r * a;
  return r;
}

static __forceinline__ double erfcinv(double a)
{
  double p, q, r;
  double t = __dadd_rn (2.0, -a);
  if ((a >= 9.6569334993691083e-4) && (a <= 1.9990343066500631e+0)) {
    t = __dmul_rn (t, a);
    t = __internal_fast_log (t);
    t = __internal_erfinv_polyd (-t);
    t = __fma_rn (t, -a, t);
  } else {
    p = a;
    if (__double2hiint (a) >= 0x3ff00000) p = t;
    if (__internal_fast_icmp_ge (__double2hiint (p), 0x2b2bff2f)) {
      /* approximation on [1.0000000584303507e-100, 9.6569334993691083e-4] */
      t = __internal_fast_log (p);
      t = __internal_fast_rsqrt (-t);
      p =                 8.7220866525375607E-001;
      p = __fma_rn (p, t, 2.0785659791501603E+000);
      p = __fma_rn (p, t, 3.0578860632810301E+000);
      p = __fma_rn (p, t, 3.4335477542378512E+000);
      p = __fma_rn (p, t, 1.2068239635789821E+000);
      p = __fma_rn (p, t, 1.4033373337606694E-001);
      p = __fma_rn (p, t, 5.3383655180290453E-003);
      p = __fma_rn (p, t, 5.1217434951970566E-005);
      q =              t+ 2.2387999676904644E+000;
      q = __fma_rn (q, t, 4.2799211712099634E+000);
      q = __fma_rn (q, t, 4.4071222015530971E+000);
      q = __fma_rn (q, t, 3.6633748801179196E+000);
      q = __fma_rn (q, t, 1.2187093791443073E+000);
      q = __fma_rn (q, t, 1.4049118729485741E-001);
      q = __fma_rn (q, t, 5.3384824646671471E-003);
      q = __fma_rn (q, t, 5.1217236388702518E-005);
      t = p / (q * t);
    } else {
      /* approximation on [1e-325, 1.0000000584303507e-100] */
      t = log (p);
      t = rsqrt (-t);
      p =                 7.7759557300080206E-001;
      p = __fma_rn (p, t, 1.6328311797427437E+000);
      p = __fma_rn (p, t, 4.1796327487734297E-001);
      p = __fma_rn (p, t, 2.3330972813256093E-002);
      p = __fma_rn (p, t, 2.6105822240716473E-004);
      q =              t+ 1.3497521990745058E+000;
      q = __fma_rn (q, t, 1.6818595730303654E+000);
      q = __fma_rn (q, t, 4.1875657436037611E-001);
      q = __fma_rn (q, t, 2.3331573970655654E-002);
      q = __fma_rn (q, t, 2.6105728696961081E-004);
      t = p / (q * t);
    }
    if (__double2hiint (a) >= 0x3ff00000) t = -t;
  }
  return t;
}

static __forceinline__ double normcdfinv(double a)
{
  double z;

  z = erfcinv (a + a);
  z = __fma_rn (-1.4142135623730949, z, -1.2537167179050217e-16 * z);
  z = __dadd_rn (z, CUDART_ZERO); /* fixup result for a == 0.5 */
  return z;
}

static __forceinline__ double __internal_erfcx_kernel (double a)
{
  /*  
   * The implementation of erfcx() is based on the algorithm in: M. M. Shepherd
   * and J. G. Laframboise, "Chebyshev Approximation of (1 + 2x)exp(x^2)erfc x
   * in 0 <= x < INF", Mathematics of Computation, Vol. 36, No. 153, January
   * 1981, pp. 249-253. For the core approximation, the input domain [0,INF] is
   * transformed via (x-k) / (x+k) where k is a precision-dependent constant.  
   * Here, we choose k = 4.0, so input domain [0,27.3] is transformed to the   
   * core approximation domain [-1,0.744409].   
   */  
  double t1, t2, t3, t4;  
  /* (1+2*x)*exp(x*x)*erfc(x) */ 
  /* t2 = (x-4.0)/(x+4.0), transforming [0,INF] to [-1,+1] */
  t1 = a - 4.0; 
  t2 = a + 4.0; 
  t2 = __internal_fast_rcp(t2);
  t3 = t1 * t2;
  t4 = __dadd_rn (t3, 1.0);         /* prevent FMA-merging */
  t1 = __fma_rn (-4.0, t4, a); 
  t1 = __fma_rn (-t3, a, t1); 
  t2 = __fma_rn (t2, t1, t3); 
  /* approximate on [-1, 0.744409] */   
  t1 =                   -3.5602694826817400E-010; 
  t1 = __fma_rn (t1, t2, -9.7239122591447274E-009); 
  t1 = __fma_rn (t1, t2, -8.9350224851649119E-009); 
  t1 = __fma_rn (t1, t2,  1.0404430921625484E-007); 
  t1 = __fma_rn (t1, t2,  5.8806698585341259E-008); 
  t1 = __fma_rn (t1, t2, -8.2147414929116908E-007); 
  t1 = __fma_rn (t1, t2,  3.0956409853306241E-007); 
  t1 = __fma_rn (t1, t2,  5.7087871844325649E-006); 
  t1 = __fma_rn (t1, t2, -1.1231787437600085E-005); 
  t1 = __fma_rn (t1, t2, -2.4399558857200190E-005); 
  t1 = __fma_rn (t1, t2,  1.5062557169571788E-004); 
  t1 = __fma_rn (t1, t2, -1.9925637684786154E-004); 
  t1 = __fma_rn (t1, t2, -7.5777429182785833E-004); 
  t1 = __fma_rn (t1, t2,  5.0319698792599572E-003); 
  t1 = __fma_rn (t1, t2, -1.6197733895953217E-002); 
  t1 = __fma_rn (t1, t2,  3.7167515553018733E-002); 
  t1 = __fma_rn (t1, t2, -6.6330365827532434E-002); 
  t1 = __fma_rn (t1, t2,  9.3732834997115544E-002); 
  t1 = __fma_rn (t1, t2, -1.0103906603555676E-001); 
  t1 = __fma_rn (t1, t2,  6.8097054254735140E-002); 
  t1 = __fma_rn (t1, t2,  1.5379652102605428E-002); 
  t1 = __fma_rn (t1, t2, -1.3962111684056291E-001); 
  t1 = __fma_rn (t1, t2,  1.2329951186255526E+000); 
  /* (1+2*x)*exp(x*x)*erfc(x) / (1+2*x) = exp(x*x)*erfc(x) */
  t2 = __fma_rn (2.0, a, 1.0);  
  t2 = __internal_fast_rcp(t2);
  t3 = t1 * t2; 
  t4 = __fma_rn (a, -2.0*t3, t1); 
  t4 = __dadd_rn (t4, -t3);         /* prevent FMA-merging */
  t1 = __fma_rn (t4, t2, t3); 
  return t1;
}

static __forceinline__ double erfc(double a)  
{  
  double x, t1, t2, t3;
  int ahi, xhi, xlo;

  ahi = __double2hiint(a);
  xhi = __double2hiint(a) & 0x7fffffff;
  xlo = __double2loint(a);
  if (xhi < 0x7ff00000) {
    x = __hiloint2double (xhi, xlo);
    t1 = __internal_erfcx_kernel (x);
    /* exp(-x*x) * exp(x*x)*erfc(x) = erfc(x) */
    t2 = -x * x;
    t3 = __internal_exp_kernel (t2, 0);
    t2 = __fma_rn (-x, x, -t2);
    t2 = __fma_rn (t3, t2, t3);
    t1 = t1 * t2;
    if (xhi > 0x403b4000) t1 = 0.0;
    t1 = (ahi < 0) ? (2.0 - t1) : t1;
  } else if ((xhi == 0x7ff00000) && (xlo == 0)) {
    t1 = (ahi < 0) ? 2.0 : 0.0;
  } else {
    t1 = a + a;
  }
  return t1;
}

static __forceinline__ double erfcx(double a)  
{
  double x, t1, t2, t3;
  int ahi, alo;

  ahi = __double2hiint (a);
  alo = __double2loint (a);
  if (__internal_fast_icmp_abs_lt (ahi, 0x40400000)) {
    x = __hiloint2double (ahi & 0x7fffffff, alo);
    t1 = __internal_erfcx_kernel (x);
  } else {
    /* asymptotic expansion for large aguments */
    t2 = 1.0 / a;
    t3 = t2 * t2;
    t1 =                  -29.53125;
    t1 = __fma_rn (t1, t3, +6.5625);
    t1 = __fma_rn (t1, t3, -1.875);
    t1 = __fma_rn (t1, t3, +0.75);
    t1 = __fma_rn (t1, t3, -0.5);
    t1 = __fma_rn (t1, t3, +1.0);
    t2 = t2 * 5.6418958354775628e-001;
    t1 = t1 * t2;
  }
  if (ahi < 0) {
    /* erfcx(x) = 2*exp(x^2) - erfcx(|x|) */
    t2 = a * a;
    t3 = __fma_rn (a, a, -t2);
    t2 = exp (t2);
    t2 = t2 + t2;
    t3 = __fma_rn (t2, t3, t2);
    t1 = t3 - t1;
    if (t2 == CUDART_INF) t1 = t2;
  }
  return t1;
}

/* normcdf(x) = 0.5 * erfc(-sqrt(0.5)*x). When x < -1.0, the numerical error
   incurred when scaling the argument of erfc() is amplified, so we need to
   compensate for this to guarantee accurate results.
*/
static __forceinline__ double normcdf(double a)
{
  double ah, al, t1, t2, z;
  if (fabs (a) > 38.5) a = copysign (38.5, a);
  t1 = __dmul_rn (a, -CUDART_SQRT_HALF_HI);
  t2 = __fma_rn (a, -CUDART_SQRT_HALF_HI, -t1);
  t2 = __fma_rn (a, -CUDART_SQRT_HALF_LO, t2);
  ah = __dadd_rn (t1, t2);
  z = erfc(ah);
  if ((unsigned int)__double2hiint (a) > 0xbff00000U) {
    al = __dadd_rn (t1 - ah, t2);
    t1 = -2.0 * ah * z;       // crude approximation of slope of erfc() at 'ah'
    z = __fma_rn (t1, al, z);
  }
  return 0.5 * z;
}

/* approximate 1.0/(a*gamma(a)) on [-0.5,0.5] */
static __forceinline__ double __internal_tgamma_kernel(double a)
{
  double t;
  t =                 -4.42689340712524750E-010;
  t = __fma_rn (t, a, -2.02665918466589540E-007);
  t = __fma_rn (t, a,  1.13812117211195270E-006);
  t = __fma_rn (t, a, -1.25077348166307480E-006);
  t = __fma_rn (t, a, -2.01365017404087710E-005);
  t = __fma_rn (t, a,  1.28050126073544860E-004);
  t = __fma_rn (t, a, -2.15241408115274180E-004);
  t = __fma_rn (t, a, -1.16516754597046040E-003);
  t = __fma_rn (t, a,  7.21894322484663810E-003);
  t = __fma_rn (t, a, -9.62197153268626320E-003);
  t = __fma_rn (t, a, -4.21977345547223940E-002);
  t = __fma_rn (t, a,  1.66538611382503560E-001);
  t = __fma_rn (t, a, -4.20026350341054440E-002);
  t = __fma_rn (t, a, -6.55878071520257120E-001);
  t = __fma_rn (t, a,  5.77215664901532870E-001);
  t = __fma_rn (t, a,  1.00000000000000000E+000);
  return t;
}

/* Stirling approximation for gamma(a), a >= 15 */
static __forceinline__ double __internal_stirling_poly(double a)
{
  double x = __internal_fast_rcp (a);
  double z = 0.0;
  z = __fma_rn (z, x,  8.3949872067208726e-004);
  z = __fma_rn (z, x, -5.1717909082605919e-005);
  z = __fma_rn (z, x, -5.9216643735369393e-004);
  z = __fma_rn (z, x,  6.9728137583658571e-005);
  z = __fma_rn (z, x,  7.8403922172006662e-004);
  z = __fma_rn (z, x, -2.2947209362139917e-004);
  z = __fma_rn (z, x, -2.6813271604938273e-003);
  z = __fma_rn (z, x,  3.4722222222222220e-003);
  z = __fma_rn (z, x,  8.3333333333333329e-002);
  z = __fma_rn (z, x,  1.0000000000000000e+000);
  return z;
}

static __forceinline__ double __internal_tgamma_stirling(double a)
{
  if (a < 1.7162437695630274e+002) {
    double t_hi, t_lo, e;

    double2 loga, prod;
    double z = __internal_stirling_poly (a);
    double b = a - 0.5;

    /* compute log(a) in double-double format*/
    loga = __internal_log_ext_prec (a);

    /* compute (a - 0.5) * log(a) in double-double format */
    t_hi = __dmul_rn (loga.y, b);  /* prevent FMA-merging */
    t_lo = __fma_rn (loga.y, b, -t_hi);
    t_lo = __fma_rn (loga.x, b, t_lo);
    prod.y = e = t_hi + t_lo;
    prod.x = (t_hi - e) + t_lo;

    /* compute (a - 0.5) * log(a) - a in double-double format */
    loga.y = -a;
    loga.x = 0.0;
    prod = __internal_ddadd_xgty (prod, loga);

    /* compute pow(a,b) = exp(b*log(a)) */
    a = exp (prod.y);
    /* prevent -INF + INF = NaN */
    if (!__isinfd (a)) {
      /* if prod.x is much smaller than prod.y, then exp(prod.y + prod.x) ~= 
       * exp(prod.y) + prod.x * exp(prod.y) 
       */
      a = __fma_rn (a, prod.x, a);
    }
    a = __fma_rn (a, CUDART_SQRT_2PI_HI, a * CUDART_SQRT_2PI_LO);
    a = a * z;
  } else {
    a = CUDART_INF;
  }
  return a;
}

static __forceinline__ double tgamma(double a)
{
  double s, xx, x = a;

  if (x >= 0.0) {
    if (__double2hiint (x) < 0x402e0000) {
      /* Based on: Kraemer, W.: "Berechnung der Gammafunktion G(x) fuer reelle
         Punkt- und Intervallargumente". ZAMM, Vol. 70, No. 6, pp. 581-584
      */
      s = 1.0;
      xx = x;
      while (__double2hiint (xx) >= 0x3ff80000) {
        s = __fma_rn(s, xx, -s);
        xx = xx - 1.0;
      }
      if (__double2hiint (x) >= 0x3fe00000) {
        xx = xx - 1.0;
      }
      xx = __internal_tgamma_kernel (xx);
      if (__double2hiint (x) < 0x3fe00000) {
        xx = xx * x;
      }
      s = s / xx;
    } else {
      s = __internal_tgamma_stirling (x);
    }
  } else if (x < 0.0) {
    if (x == trunc(x)) {
      s = CUDART_NAN;
    } else if (__double2hiint (x) < 0xc02e0000U) {
      /* Based on: Kraemer, W.: "Berechnung der Gammafunktion G(x) fuer reelle
         Punkt- und Intervallargumente". ZAMM, Vol. 70, No. 6, pp. 581-584
      */
      xx = x;
      s = xx;
      while ((unsigned int)__double2hiint(xx) >= 0xbfe00000U) {
        s = __fma_rn (s, xx, s);
        xx = xx + 1.0;
      }
      xx = __internal_tgamma_kernel (xx);
      s = s * xx;
      s = 1.0 / s;
    } else if ((unsigned int)__double2hiint (x) < 0xc0672000U) {
      double t;
      xx = sinpi (x);
      s = __internal_exp_kernel (x, 0);
      x = fabs (x);
      t = x - 0.5;
      if (__double2hiint (x) >= 0x40618000) t = __internal_half(t);
      t = __internal_accurate_pow (x, t);
      if (__double2hiint (x) >= 0x40618000) s = s * t;
      s = s * __internal_stirling_poly (x);
      s = s * x;
      s = s * xx;
      s = 1.0 / s;
      s = __fma_rn (s, CUDART_SQRT_PIO2_HI, CUDART_SQRT_PIO2_LO * s);
      s = s / t;
    } else {
      xx = floor (x);
      int negative = ((xx - (2.0 * floor(0.5 * xx))) == 1.0);
      s = negative ? CUDART_NEG_ZERO : CUDART_ZERO;
    }
  } else {
    s = a + a;
  }
  return s;
}

static
#if __CUDA_ARCH__ >= 200
__noinline__
#else
__forceinline__
#endif
double __internal_lgamma_pos(double a)
{
  double sum;
  double s, t;

  if (__double2hiint (a) >= 0x40080000) {
    if (__double2hiint (a) >= 0x40200000) {
      /* Stirling approximation; coefficients from Hart et al, "Computer 
       * Approximations", Wiley 1968. Approximation 5404. 
       */
      s = __internal_fast_rcp(a);
      t = s * s;
      sum =                   -0.1633436431e-2;
      sum = __fma_rn (sum, t,  0.83645878922e-3);
      sum = __fma_rn (sum, t, -0.5951896861197e-3);
      sum = __fma_rn (sum, t,  0.793650576493454e-3);
      sum = __fma_rn (sum, t, -0.277777777735865004e-2);
      sum = __fma_rn (sum, t,  0.833333333333331018375e-1);
      sum = __fma_rn (sum, s,  0.918938533204672);
      s = __internal_half(log (a));
      t = a - 0.5;
      sum = __fma_rn(s, t, sum);
      t = __fma_rn (s, t, - a);
      t = t + sum;
      if (a == CUDART_INF) {
        t = a;
      }
    } else {
      a = a - 3.0;
      s =                 -4.02412642744125560E+003;
      s = __fma_rn (s, a, -2.97693796998962000E+005);
      s = __fma_rn (s, a, -6.38367087682528790E+006);
      s = __fma_rn (s, a, -5.57807214576539320E+007);
      s = __fma_rn (s, a, -2.24585140671479230E+008);
      s = __fma_rn (s, a, -4.70690608529125090E+008);
      s = __fma_rn (s, a, -7.62587065363263010E+008);
      s = __fma_rn (s, a, -9.71405112477113250E+008);
      t =              a  -1.02277248359873170E+003;
      t = __fma_rn (t, a, -1.34815350617954480E+005);
      t = __fma_rn (t, a, -4.64321188814343610E+006);
      t = __fma_rn (t, a, -6.48011106025542540E+007);
      t = __fma_rn (t, a, -4.19763847787431360E+008);
      t = __fma_rn (t, a, -1.25629926018000720E+009);
      t = __fma_rn (t, a, -1.40144133846491690E+009);
      t = s / t;
      t = t + a;
    }
  } else if (__double2hiint (a) >= 0x3ff80000) {
    a = a - 2.0;
    t =                  9.84839283076310610E-009;
    t = __fma_rn (t, a, -6.69743850483466500E-008);
    t = __fma_rn (t, a,  2.16565148880011450E-007);
    t = __fma_rn (t, a, -4.86170275781575260E-007);
    t = __fma_rn (t, a,  9.77962097401114400E-007);
    t = __fma_rn (t, a, -2.03041287574791810E-006);
    t = __fma_rn (t, a,  4.36119725805364580E-006);
    t = __fma_rn (t, a, -9.43829310866446590E-006);
    t = __fma_rn (t, a,  2.05106878496644220E-005);
    t = __fma_rn (t, a, -4.49271383742108440E-005);
    t = __fma_rn (t, a,  9.94570466342226000E-005);
    t = __fma_rn (t, a, -2.23154589559238440E-004);
    t = __fma_rn (t, a,  5.09669559149637430E-004);
    t = __fma_rn (t, a, -1.19275392649162300E-003);
    t = __fma_rn (t, a,  2.89051032936815490E-003);
    t = __fma_rn (t, a, -7.38555102806811700E-003);
    t = __fma_rn (t, a,  2.05808084278121250E-002);
    t = __fma_rn (t, a, -6.73523010532073720E-002);
    t = __fma_rn (t, a,  3.22467033424113040E-001);
    t = __fma_rn (t, a,  4.22784335098467190E-001);
    t = t * a;
  } else if (__double2hiint (a) >= 0x3fe66666) {
    a = 1.0 - a;
    t =                 1.17786911519331130E-002;  
    t = __fma_rn (t, a, 3.89046747413522300E-002);
    t = __fma_rn (t, a, 5.90045711362049900E-002);
    t = __fma_rn (t, a, 6.02143305254344420E-002);
    t = __fma_rn (t, a, 5.61652708964839180E-002);
    t = __fma_rn (t, a, 5.75052755193461370E-002);
    t = __fma_rn (t, a, 6.21061973447320710E-002);
    t = __fma_rn (t, a, 6.67614724532521880E-002);
    t = __fma_rn (t, a, 7.14856037245421020E-002);
    t = __fma_rn (t, a, 7.69311251313347100E-002);
    t = __fma_rn (t, a, 8.33503129714946310E-002);
    t = __fma_rn (t, a, 9.09538288991182800E-002);
    t = __fma_rn (t, a, 1.00099591546322310E-001);
    t = __fma_rn (t, a, 1.11334278141734510E-001);
    t = __fma_rn (t, a, 1.25509666613462880E-001);
    t = __fma_rn (t, a, 1.44049896457704160E-001);
    t = __fma_rn (t, a, 1.69557177031481600E-001);
    t = __fma_rn (t, a, 2.07385551032182120E-001);
    t = __fma_rn (t, a, 2.70580808427600350E-001);
    t = __fma_rn (t, a, 4.00685634386517050E-001);
    t = __fma_rn (t, a, 8.22467033424113540E-001);
    t = __fma_rn (t, a, 5.77215664901532870E-001);
    t = t * a;
  } else {
    t=                  -9.04051686831357990E-008;
    t = __fma_rn (t, a,  7.06814224969349250E-007);
    t = __fma_rn (t, a, -3.80702154637902830E-007);
    t = __fma_rn (t, a, -2.12880892189316100E-005);
    t = __fma_rn (t, a,  1.29108470307156190E-004);
    t = __fma_rn (t, a, -2.15932815215386580E-004);
    t = __fma_rn (t, a, -1.16484324388538480E-003);
    t = __fma_rn (t, a,  7.21883433044470670E-003);
    t = __fma_rn (t, a, -9.62194579514229560E-003);
    t = __fma_rn (t, a, -4.21977386992884450E-002);
    t = __fma_rn (t, a,  1.66538611813682460E-001);
    t = __fma_rn (t, a, -4.20026350606819980E-002);
    t = __fma_rn (t, a, -6.55878071519427450E-001);
    t = __fma_rn (t, a,  5.77215664901523870E-001);
    t = t * a;
    t = __fma_rn (t, a, a);
    t = -log (t);
  }
  return t;
}

static __forceinline__ double lgamma(double a)
{
  double s, t;

  if (!__isnand(a)) {
    s = fabs (a);
    t = __internal_lgamma_pos (s);
    if (__double2hiint (a) < 0) {
      double i = trunc (s);       
      if (s == i) {
        t = CUDART_INF; /* a is an integer: return infinity */
      } else {
        double u;
        if (__double2hiint (s) < 0x3c000000) {
          u = s;
        } else {
          i = fabs (sinpi (s));
          u = CUDART_PI / (i * s);
        }
        u = log (u);
        if (__double2hiint (s) < 0x3c000000) {
          t = -u;
        } else {
          t = u - t;
        }
      }
    }
  } else {
    t = a + a;
  }
  return t;
}

static __forceinline__ double ldexp(double a, int b)
{
  double fa = fabs (a);
  b = max (-2200, min (b, 2200));
  if ((fa == CUDART_ZERO) || (fa == CUDART_INF) || (b == 0)) {
    if (!(fa > CUDART_ZERO)) a = a + a;
  } else if (abs (b) < 1022) {
    a = a * __internal_exp2i_kernel(b);
  } else if (abs (b) < 2044) {
    int bhalf = b / 2;
    a = a * __internal_exp2i_kernel(bhalf) * __internal_exp2i_kernel(b - bhalf);
  } else {
    int bquarter = b / 4;
    double t = __internal_exp2i_kernel(bquarter);
    a = a * t * t * t *__internal_exp2i_kernel (b - 3 * bquarter);
  }
  return a;
}

static __forceinline__ double scalbn(double a, int b)
{
  /* On binary systems, ldexp(x,exp) is equivalent to scalbn(x,exp) */
  return ldexp(a, b);
}

static __forceinline__ double scalbln(double a, long int b)
{
#if defined(__LP64__)
  /* clamp to integer range prior to conversion */
  if (b < -2147483648L) b = -2147483648L;
  if (b >  2147483647L) b =  2147483647L;
#endif /* __LP64__ */
  return scalbn(a, (int)b);
}

static __forceinline__ double frexp(double a, int *b)
{
  unsigned int expo;

  expo = __double2hiint(a) & 0x7ff00000;
  if (expo < 0x00100000) {
    a *= CUDART_TWO_TO_54;
    expo = (__double2hiint(a) & 0x7ff00000) - (54 << 20);
  }
  if ((a == 0.0) || ((int)expo >= 0x7ff00000)) {
    expo = 0;
    a = a + a;
  } else {
    long long int ia = __double_as_longlong (a);
    ia = (ia & 0x800fffffffffffffULL) | 0x3fe0000000000000ULL;
    a = __longlong_as_double (ia);
    expo = expo - (1022 << 20);
    asm ("shr.s32 %0, %0, 20;" : "+r"(expo));
  }
  *b = expo;
  return a;  
}

static __forceinline__ double modf(double a, double *b)
{
  double t;
  if (__isfinited(a)) {
    t = trunc(a);
    *b = t;
    t = a - t;
    a = __internal_copysign_pos(t, a);
  } else if (__isinfd(a)) {
    t = 0.0;
    *b = a;
    a = __internal_copysign_pos(t, a);
  } else {
    *b = a + a; 
    a = a + a;
  }
  return a;
}

#if __CUDA_ARCH__ == 200 || __CUDA_ARCH__ == 350
static __forceinline__ double fmod(double a, double b)
{
  double orig_a = a;
  double orig_b = b;
  int ahi = __double2hiint(a) & 0x7fffffff;
  int alo = __double2loint(a);
  int bhi = __double2hiint(b) & 0x7fffffff;
  int blo = __double2loint(b);

  a = __hiloint2double (ahi, alo);
  b = __hiloint2double (bhi, blo);
  if ((ahi >= 0x7ff00000) || (bhi >= 0x7ff00000)) {
    if (!((a <= CUDART_INF) && (b <= CUDART_INF))) {
      a = orig_a + orig_b;
    } else if (a == CUDART_INF) {
      a = CUDART_NAN;
    } else {
      a = orig_a;
    }
  } else if (b == 0.0) {
    a = CUDART_NAN;
  } else if (a >= b) {
    double scaled_b = 0.0;
    if (__double2hiint (b) < 0x00100000) {
      double t = b;
      while ((t < a) && (__double2hiint (t) < 0x00100000)) {
        t = t + t;
      }
      bhi = __double2hiint(t);
      blo = __double2loint(t);
      scaled_b = t;
    }
    if (__double2hiint (a) >= 0x00100000) {
      scaled_b = __hiloint2double ((bhi & 0x000fffff)|(ahi & 0x7ff00000), blo);
    }
    if (scaled_b > a) {
      scaled_b *= 0.5;
    }
    while (scaled_b >= b) {
      if (a >= scaled_b) {
        a -= scaled_b;
      }
      scaled_b *= 0.5;
    }
    a = __internal_copysign_pos (a, orig_a);
  } else {
    a = orig_a;
  }
  return a;
}
#else
static __forceinline__ double fmod(double a, double b)
{
  double orig_a = a;
  double orig_b = b;
  int ahi = __double2hiint(a) & 0x7fffffff;
  int alo = __double2loint(a);
  int bhi = __double2hiint(b) & 0x7fffffff;
  int blo = __double2loint(b);

  a = __hiloint2double (ahi, alo);
  b = __hiloint2double (bhi, blo);
  if ((ahi >= 0x7ff00000) || (bhi >= 0x7ff00000)) {
    if (!((a <= CUDART_INF) && (b <= CUDART_INF))) {
      a = orig_a + orig_b;
    } else if (a == CUDART_INF) {
      a = CUDART_NAN;
    } else {
      a = orig_a;
    }
  } else if (b == 0.0) {
    a = CUDART_NAN;
  } else if (a >= b) {
    unsigned long long int ia;
    unsigned long long int ib;
    int expoa = (unsigned int)__double2hiint(a) >> 20;
    int expob = (unsigned int)__double2hiint(b) >> 20;
    if (expoa < 1) {      
      a = a * CUDART_TWO_TO_54;
      expoa = expoa + ((unsigned int)__double2hiint(a) >> 20) - 54;
    }
    if (expob < 1) {
      b = b * CUDART_TWO_TO_54;
      expob = expob + ((unsigned int)__double2hiint(b) >> 20) - 54;
    }
    ia = __double_as_longlong(a);
    ib = __double_as_longlong(b);
    ia = (ia & 0x000fffffffffffffULL) | 0x0010000000000000ULL;
    ib = (ib & 0x000fffffffffffffULL) | 0x0010000000000000ULL;
    expob = expoa - expob;
    expoa = expoa - expob;

    do {
      ia = ia - ib;
      if (__double2hiint(__longlong_as_double(ia)) < 0) {
        ia = ia + ib;
      }
      ia = ia + ia;
      expob--;
    } while (expob >= 0);

    ia = ia >> 1; 
    if (ia != 0ULL) { 
      b = __longlong_as_double(ia) * CUDART_TWO_TO_54;
      int i = 55 - ((unsigned int)__double2hiint(b) >> 20);
      expoa = expoa - i; 
      ia = ia << i; 
      if (expoa < 1) { 
        ia = ia >> (1 - expoa); 
      } else { 
        ia = ia + ((unsigned long long int)(expoa - 1) << 52); 
      } 
    } 
    a = __longlong_as_double(ia);
    a = __internal_copysign_pos(a, orig_a);
  } else {
    a = orig_a;
  }
  return a;
}
#endif

#if __CUDA_ARCH__ == 200 || __CUDA_ARCH__ == 350
static __forceinline__ double remainder(double a, double b)
{
  double orig_a = a;
  double orig_b = b;
  unsigned int quot = 0;
  int ahi = __double2hiint(a) & 0x7fffffff;
  int alo = __double2loint(a);
  int bhi = __double2hiint(b) & 0x7fffffff;
  int blo = __double2loint(b);

  a = __hiloint2double (ahi, alo);
  b = __hiloint2double (bhi, blo);
  if ((ahi >= 0x7ff00000) || (bhi >= 0x7ff00000)) {
    if (!((a <= CUDART_INF) && (b <= CUDART_INF))) {
      a = orig_a + orig_b;
    } else if (a == CUDART_INF) {
      a = CUDART_NAN;
    } else {
      a = orig_a;
    }
  } else if (b == 0.0) {
    a = CUDART_NAN;
  } else {
    if (a >= b) {
      double scaled_b = 0.0;
      if (__double2hiint (b) < 0x00100000) {
        double t = b;
        while ((t < a) && (__double2hiint (t) < 0x00100000)) {
          t = t + t;
        }
        bhi = __double2hiint(t);
        blo = __double2loint(t);
        scaled_b = t;
      }
      if (__double2hiint (a) >= 0x00100000) {
        scaled_b = __hiloint2double ((bhi & 0x000fffff)|(ahi & 0x7ff00000),
                                     blo);
      }
      if (scaled_b > a) {
        scaled_b *= 0.5;
      }
      unsigned int nquot = ~quot;
      while (scaled_b >= b) {
        nquot = nquot * 2 + 1;
        if (a >= scaled_b) {
          a -= scaled_b;
          nquot--;
        }
        scaled_b *= 0.5;
      }
      quot = ~nquot;
    }
    /* round quotient to nearest even */
    double twoa = a + a;
    if ((twoa > b) || ((twoa == b) && (quot & 1))) {
      a -= b;
    }
    bhi = __double2hiint (a);
    blo = __double2loint (a);
    ahi = __double2hiint (orig_a);
    a = __hiloint2double ((ahi & 0x80000000) ^ bhi, blo);
  }
  return a;
}
#else
static __forceinline__ double remainder(double a, double b)
{
  double orig_a = a;
  double orig_b = b;
  int nquot0 = 1;
  int ahi = __double2hiint(a) & 0x7fffffff;
  int alo = __double2loint(a);
  int bhi = __double2hiint(b) & 0x7fffffff;
  int blo = __double2loint(b);

  a = __hiloint2double (ahi, alo);
  b = __hiloint2double (bhi, blo);
  if ((ahi >= 0x7ff00000) || (bhi >= 0x7ff00000)) {
    if (!((a <= CUDART_INF) && (b <= CUDART_INF))) {
      a = orig_a + orig_b;
    } else if (a == CUDART_INF) {
      a = CUDART_NAN;
    } else {
      a = orig_a;
    }
  } else if (b == 0.0) {
    a = CUDART_NAN;
  } else {
    if (a >= b) {
      unsigned long long int ia;
      unsigned long long int ib;
      double t = b;
      int expoa = (unsigned int)__double2hiint(a) >> 20;
      int expob = (unsigned int)__double2hiint(t) >> 20;
      if (expoa < 1) {      
        a = a * CUDART_TWO_TO_54;
        expoa = expoa + ((unsigned int)__double2hiint(a) >> 20) - 54;
      }
      if (expob < 1) {
        t = t * CUDART_TWO_TO_54;
        expob = expob + ((unsigned int)__double2hiint(t) >> 20) - 54;
      }
      ia = __double_as_longlong(a);
      ib = __double_as_longlong(t);
      ia = (ia & 0x000fffffffffffffULL) | 0x0010000000000000ULL;
      ib = (ib & 0x000fffffffffffffULL) | 0x0010000000000000ULL;
      expob = expoa - expob;
      expoa = expoa - expob;

      do {
        ia = ia - ib;
        nquot0 = __double2hiint(__longlong_as_double(ia)) & 0x80000000;
        if (nquot0 < 0) {
          ia = ia + ib;
        }
        ia = ia + ia;
        expob--;
      } while (expob >= 0);

      ia = ia >> 1; 
      if (ia != 0ULL) { 
        t = __longlong_as_double(ia) * CUDART_TWO_TO_54;
        int i = 55 - ((unsigned int)__double2hiint(t) >> 20);
        expoa = expoa - i; 
        ia = ia << i; 
        if (expoa < 1) { 
          ia = ia >> (1 - expoa); 
        } else { 
          ia = ia + ((unsigned long long int)(expoa - 1) << 52); 
        } 
      } 
      a = __longlong_as_double(ia);
    }
    double twoa = a + a;
    if ((twoa > b) || ((twoa == b) && !nquot0)) {
      a -= b;
    }
    a = __hiloint2double((__double2hiint(orig_a) & 0x80000000) ^ 
                          __double2hiint(a), __double2loint(a));
  }
  return a;
}
#endif

#if __CUDA_ARCH__ == 200 || __CUDA_ARCH__ == 350
static __forceinline__ double remquo(double a, double b, int *c)
{
  double orig_a = a;
  double orig_b = b;
  unsigned int quot = 0;
  int sign = 0 - ((__double2hiint(a) ^ __double2hiint(b)) < 0);
  int ahi = __double2hiint(a) & 0x7fffffff;
  int alo = __double2loint(a);
  int bhi = __double2hiint(b) & 0x7fffffff;
  int blo = __double2loint(b);

  a = __hiloint2double (ahi, alo);
  b = __hiloint2double (bhi, blo);
  if ((ahi >= 0x7ff00000) || (bhi >= 0x7ff00000)) {
    if (!((a <= CUDART_INF) && (b <= CUDART_INF))) {
      a = orig_a + orig_b;
    } else if (a == CUDART_INF) {
      a = CUDART_NAN;
    } else {
      a = orig_a;
    }
  } else if (b == 0.0) {
    a = CUDART_NAN;
  } else {
    if (a >= b) {
      double scaled_b = 0.0;
      if (__double2hiint (b) < 0x00100000) {
        double t = b;
        while ((t < a) && (__double2hiint (t) < 0x00100000)) {
          t = t + t;
        }
        bhi = __double2hiint (t);
        blo = __double2loint (t);
        scaled_b = t;
      }
      if (__double2hiint (a) >= 0x00100000) {
        scaled_b = __hiloint2double ((bhi & 0x000fffff)|(ahi & 0x7ff00000), 
                                     blo);
      }
      if (scaled_b > a) {
        scaled_b *= 0.5;
      }
      unsigned int nquot = ~quot;
      while (scaled_b >= b) {
        nquot = nquot * 2 + 1;
        if (a >= scaled_b) {
          a -= scaled_b;
          nquot--;
        }
        scaled_b *= 0.5;
      }
      quot = ~nquot;
    }
    /* round quotient to nearest even */
    double twoa = a + a;
    if ((twoa > b) || ((twoa == b) && (quot & 1))) {
      quot++;
      a -= b;
    }
    bhi = __double2hiint (a);
    blo = __double2loint (a);
    ahi = __double2hiint (orig_a);
    a = __hiloint2double ((ahi & 0x80000000) ^ bhi, blo);
    quot = quot & CUDART_REMQUO_MASK_F;
    quot = quot ^ sign;
    quot = quot - sign;
  }
  *c = quot;
  return a;
}
#else
static __forceinline__ double remquo(double a, double b, int *c)
{
  double orig_a = a;
  double orig_b = b;
  unsigned int quot = 0;
  unsigned int nquot = ~quot;
  unsigned int sign = 0 - ((__double2hiint(a) ^ __double2hiint(b)) < 0);
  int ahi = __double2hiint(a) & 0x7fffffff;
  int alo = __double2loint(a);
  int bhi = __double2hiint(b) & 0x7fffffff;
  int blo = __double2loint(b);

  a = __hiloint2double (ahi, alo);
  b = __hiloint2double (bhi, blo);
  if ((ahi >= 0x7ff00000) || (bhi >= 0x7ff00000)) {
    if (!((a <= CUDART_INF) && (b <= CUDART_INF))) {
      a = orig_a + orig_b;
    } else if (a == CUDART_INF) {
      a = CUDART_NAN;
    } else {
      a = orig_a;
    }
  } else if (b == 0.0) {
    a = CUDART_NAN;
  } else {
    if (a >= b) {
      unsigned long long int ia;
      unsigned long long int ib;
      unsigned int nquot0;
      double t = b;
      int expoa = (unsigned int)__double2hiint(a) >> 20;
      int expob = (unsigned int)__double2hiint(t) >> 20;
      if (expoa < 1) {      
        a = a * CUDART_TWO_TO_54;
        expoa = expoa + ((unsigned int)__double2hiint(a) >> 20) - 54;
      }
      if (expob < 1) {
        t = t * CUDART_TWO_TO_54;
        expob = expob + ((unsigned int)__double2hiint(t) >> 20) - 54;
      }
      ia = __double_as_longlong(a);
      ib = __double_as_longlong(t);
      ia = (ia & 0x000fffffffffffffULL) | 0x0010000000000000ULL;
      ib = (ib & 0x000fffffffffffffULL) | 0x0010000000000000ULL;
      expob = expoa - expob;
      expoa = expoa - expob;

      do {
        ia = ia - ib;
        nquot0 = ((unsigned int)__double2hiint(__longlong_as_double(ia))) >> 31;
        if (nquot0 != 0) {
          ia = ia + ib;
        }
        ia = ia + ia;
        nquot = (nquot << 1) + nquot0;
        expob--;
      } while (expob >= 0);

      ia = ia >> 1; 
      if (ia != 0ULL) { 
        t = __longlong_as_double(ia) * CUDART_TWO_TO_54;
        int i = 55 - ((unsigned int)__double2hiint(t) >> 20);
        expoa = expoa - i; 
        ia = ia << i; 
        if (expoa < 1) { 
          ia = ia >> (1 - expoa); 
        } else { 
          ia = ia + ((unsigned long long int)(expoa - 1) << 52); 
        } 
      } 
      a = __longlong_as_double(ia);
    }
    double twoa = a + a;
    quot = ~nquot;
    if ((twoa > b) || ((twoa == b) && (quot & 1))) {
      quot++;
      a -= b;
    }
    a = __hiloint2double((__double2hiint(orig_a) & 0x80000000) ^ 
                         __double2hiint(a), __double2loint(a));
    quot = quot & CUDART_REMQUO_MASK_F;
    quot = quot ^ sign;
    quot = quot - sign;
  }
  *c = quot;
  return a;
}
#endif

static __forceinline__ double nextafter(double a, double b)
{
  unsigned long long int ia;
  unsigned long long int ib;
  ia = __double_as_longlong(a);
  ib = __double_as_longlong(b);
  if (__isnand(a) || __isnand(b)) return a + b; /* NaN */
  if (((ia | ib) << 1) == 0ULL) return b;
  if ((ia + ia) == 0ULL) {
    return __internal_copysign_pos(CUDART_MIN_DENORM, b);   /* crossover */
  }
  if ((a < b) && (a < 0.0)) ia--;
  if ((a < b) && (a > 0.0)) ia++;
  if ((a > b) && (a < 0.0)) ia++;
  if ((a > b) && (a > 0.0)) ia--;
  a = __longlong_as_double(ia);
  return a;
}

static __forceinline__ double nan(const char *tagp)
{
  unsigned long long int i;

  i = __internal_nan_kernel (tagp);
  return __longlong_as_double(i);
}

static __forceinline__ double round(double a)
{
  double u;
  double fa = fabs(a);
  if (fa >= CUDART_TWO_TO_52) {
    u = a;
  } else {      
    u = trunc(fa + 0.5);
    if (fa < 0.5) u = 0;
    u = __internal_copysign_pos(u, a);
  }
  return u;
}

static __forceinline__ long long int llround(double a)
{
  return (long long int)round(a);
}

static __forceinline__ long int lround(double a)
{
#if defined(__LP64__)
  return (long int)llround(a);
#else /* __LP64__ */
  return (long int)round(a);
#endif /* __LP64__ */
}

static __forceinline__ double fdim(double a, double b)
{
  double t;
  t = a - b;    /* default also takes care of NaNs */
  if (a <= b) {
    t = 0.0;
  }
  return t;
}

static __forceinline__ int ilogb(double a)
{
  unsigned long long int i;
  unsigned int ihi;
  unsigned int ilo;
  if (__isnand(a)) return -__cuda_INT_MAX-1;
  if (__isinfd(a)) return __cuda_INT_MAX;
  if (a == 0.0) return -__cuda_INT_MAX-1;
  a = fabs(a);
  ilo = __double2loint(a);
  ihi = __double2hiint(a);
  i = ((unsigned long long int)ihi) << 32 | (unsigned long long int)ilo;
  if (ihi >= 0x00100000) {
    return (int)((ihi >> 20) - 1023);
  } else {
    return -1011 - __clzll(i);
  }
}

static __forceinline__ double logb(double a)
{
  unsigned long long int i;
  unsigned int ihi;
  unsigned int ilo;
  if (__isnand(a)) return a + a;
  a = fabs(a);
  if (a == CUDART_INF) return a;
  if (a == 0.0) return -CUDART_INF;
  ilo = __double2loint(a);
  ihi = __double2hiint(a);
  i = ((unsigned long long int)ihi) << 32 | (unsigned long long int)ilo;
  if (ihi >= 0x00100000) {
    return (double)((int)((ihi >> 20) - 1023));
  } else {
    int expo = -1011 - __clzll(i);
    return (double)expo;
  }
}

static __forceinline__ double fma(double a, double b, double c)
{
  return __fma_rn(a, b, c);
}

#endif /* __CUDANVVM__ */

#endif /* __CUDABE__ */

#endif /* __MATH_FUNCTIONS_DBL_PTX3_H__ */
