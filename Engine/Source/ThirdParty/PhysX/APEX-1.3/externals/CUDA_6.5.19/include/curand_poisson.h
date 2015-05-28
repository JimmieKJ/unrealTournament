
 /* Copyright 2010-2014 NVIDIA Corporation.  All rights reserved.
  *                                                  
  * NOTICE TO LICENSEE:
  *
  * The source code and/or documentation ("Licensed Deliverables") are
  * subject to NVIDIA intellectual property rights under U.S. and
  * international Copyright laws.
  *
  * The Licensed Deliverables contained herein are PROPRIETARY and
  * CONFIDENTIAL to NVIDIA and are being provided under the terms and
  * conditions of a form of NVIDIA software license agreement by and
  * between NVIDIA and Licensee ("License Agreement") or electronically
  * accepted by Licensee.  Notwithstanding any terms or conditions to
  * the contrary in the License Agreement, reproduction or disclosure
  * of the Licensed Deliverables to any third party without the express
  * written consent of NVIDIA is prohibited.
  *
  * NOTWITHSTANDING ANY TERMS OR CONDITIONS TO THE CONTRARY IN THE
  * LICENSE AGREEMENT, NVIDIA MAKES NO REPRESENTATION ABOUT THE
  * SUITABILITY OF THESE LICENSED DELIVERABLES FOR ANY PURPOSE.  THEY ARE
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
  * C.F.R. 12.212 (SEPT 1995) and are provided to the U.S. Government
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


#if !defined(CURAND_POISSON_H_)
#define CURAND_POISSON_H_

/**
 * \defgroup DEVICE Device API
 *
 * @{
 */

#include "curand_mrg32k3a.h"
#include "curand_mtgp32_kernel.h"
#include <math.h>

#include "curand_philox4x32_x.h" 


#define CR_CUDART_PI               3.1415926535897931e+0
#define CR_CUDART_TWO_TO_52        4503599627370496.0

QUALIFIERS int __cr_isnan(double a)
{
  volatile union {
    double                 d;
    unsigned long long int l;
  } cvt;

  cvt.d = a;

  return cvt.l << 1 > 0xffe0000000000000ull;
}


QUALIFIERS int __cr_isinf(double a)
{
  volatile union {
    double                 d;
    unsigned long long int l;
  } cvt;

  cvt.d = a;

  return cvt.l << 1 == 0xffe0000000000000ull;
}


QUALIFIERS double __cr_copysign(double a, double b)
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

QUALIFIERS double __cr_rint(double a)
{
  double fa = fabs(a);
  double u = CR_CUDART_TWO_TO_52 + fa;
  if (fa >= CR_CUDART_TWO_TO_52) {
    u = a;
  } else {
    u = u - CR_CUDART_TWO_TO_52;
    u = __cr_copysign (u, a);
  }
  return u;  
}

QUALIFIERS float __cr_rsqrt(float a) 
{
#ifdef __CUDA_ARCH__
    asm ("rsqrt.approx.f32.ftz %0, %1;" : "=f"(a) : "f"(a));
#else
    a = 1.0f / sqrt (a);
#endif
    return a;
}

QUALIFIERS float __cr_exp (float a)
{
#ifdef __CUDA_ARCH__
    a = a * 1.4426950408889634074;
    asm ("ex2.approx.f32.ftz %0, %1;" : "=f"(a) : "f"(a));
#else
    a = exp (a);
#endif
    return a;
}

QUALIFIERS float __cr_log (float a)
{
#ifdef __CUDA_ARCH__
    asm ("lg2.approx.f32.ftz %0, %1;" : "=f"(a) : "f"(a));
    a = a * 0.69314718055994530942;
#else
    a = log (a);
#endif
    return a;
}

QUALIFIERS float __cr_rcp (float a)
{
#ifdef __CUDA_ARCH__
    asm ("rcp.approx.f32.ftz %0, %1;" : "=f"(a) : "f"(a));
#else
    a = 1.0f / a;
#endif
    return a;
}

/* Computes regularized gamma function:  gammainc(a,x)/gamma(a) */
QUALIFIERS float __cr_pgammainc (float a, float x)
{
    float t, alpha, beta;

    /* First level parametrization constants */
    float ma1 = 1.43248035075540910f,
          ma2 = 0.12400979329415655f,
          ma3 = 0.00025361074907033f,
          mb1 = 0.21096734870196546f,
          mb2 = 1.97381164089999420f,
          mb3 = 0.94201734077887530f;

    /* Second level parametrization constants (depends only on a) */

    alpha = __cr_rsqrt (a - ma2);
    alpha = ma1 * alpha + ma3;
    beta = __cr_rsqrt (a - mb2);
    beta = mb1 * beta + mb3;

    /* Final approximation (depends on a and x) */

    t = a - x;
    t = alpha * t - beta;
    t = 1.0f + __cr_exp (t);
    t = t * t;
    t = __cr_rcp (t);

    /* Negative a,x or a,x=NAN requires special handling */
    //t = !(x > 0 && a >= 0) ? 0.0 : t;

    return t;
}

/* Computes inverse of pgammainc */
QUALIFIERS float __cr_pgammaincinv (float a, float y)
{
    float t, alpha, beta;

    /* First level parametrization constants */

    float ma1 = 1.43248035075540910f,
          ma2 = 0.12400979329415655f,
          ma3 = 0.00025361074907033f,
          mb1 = 0.21096734870196546f,
          mb2 = 1.97381164089999420f,
          mb3 = 0.94201734077887530f;

    /* Second level parametrization constants (depends only on a) */

    alpha = __cr_rsqrt (a - ma2);
    alpha = ma1 * alpha + ma3;
    beta = __cr_rsqrt (a - mb2);
    beta = mb1 * beta + mb3;

    /* Final approximation (depends on a and y) */

    t = __cr_rsqrt (y) - 1.0f;
    t = __cr_log (t);
    t = beta + t;
    t = - t * __cr_rcp (alpha) + a;
    /* Negative a,x or a,x=NAN requires special handling */
    //t = !(y > 0 && a >= 0) ? 0.0 : t;
    return t;
}

static __constant__ double __cr_lgamma_table [] = { 0.000000000000000000e-1,
    0.000000000000000000e-1,
    6.931471805599453094e-1,
    1.791759469228055001e0,
    3.178053830347945620e0,
    4.787491742782045994e0,
    6.579251212010100995e0,
    8.525161361065414300e0,
    1.060460290274525023e1};


QUALIFIERS double __cr_lgamma_integer(int a)
{
    double s;
    double t;
    double fa = fabs((float)a);
    double sum;

    if (a > 8) {
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
        return t;
    } else {
#ifdef __CUDA_ARCH__
        return __cr_lgamma_table [(int) fa-1];
#else
        switch(a) {
            case 1: return 0.000000000000000000e-1;
            case 2: return 0.000000000000000000e-1;
            case 3: return 6.931471805599453094e-1;
            case 4: return 1.791759469228055001e0;
            case 5: return 3.178053830347945620e0;
            case 6: return 4.787491742782045994e0;
            case 7: return 6.579251212010100995e0;
            case 8: return 8.525161361065414300e0;
            default: return 1.060460290274525023e1;
        }
#endif
    }
}

QUALIFIERS double __cr_lgamma(double a)
{
  double s;
  double t;
  double i;
  double fa;
  double sum;
  long long int quot;

  if (__cr_isnan(a) || __cr_isinf(a)) {
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

  i = __cr_rint (2.0 * fa);
  quot = (long long int)i;
  i = fa - 0.5 * i;
  i = i * CR_CUDART_PI;
  if (quot & 1) {
    i = cos(i);
  } else {
    i = sin(i);
  }
  i = fabs(i);
  t = log(CR_CUDART_PI / (i * fa)) - t;
  return t;
}

#define KNUTH_FLOAT_CONST 60.0
template <typename T>
// Donald E. Knuth Seminumerical Algorithms. The Art of Computer Programming, Volume 2
QUALIFIERS unsigned int curand_poisson_knuth(T *state, float lambda)
{
  unsigned int k = 0;
  float p=exp(lambda);
  do{
      k++;
      p *= curand_uniform(state);
  }while (p > 1.0);
  return k-1;
}

template <typename T>
// Donald E. Knuth Seminumerical Algorithms. The Art of Computer Programming, Volume 2
QUALIFIERS uint4 curand_poisson_knuth4(T *state, float lambda)
{
  uint4 k = {0,0,0,0};
  float exp_lambda = exp(lambda);
  float4 p={ exp_lambda,exp_lambda,exp_lambda,exp_lambda };
  do{
      k.x++;
      p.x *= curand_uniform(state);
  }while (p.x > 1.0);
  do{
      k.y++;
      p.y *= curand_uniform(state);
  }while (p.y > 1.0);
  do{
      k.z++;
      p.z *= curand_uniform(state);
  }while (p.z > 1.0);
  do{
      k.w++;
      p.w *= curand_uniform(state);
  }while (p.w > 1.0);

  k.x--;
  k.y--;
  k.z--;
  k.w--;
  return k;
}

template <typename T>
// Marsaglia, Tsang, Wang Journal of Statistical Software, square histogram.
QUALIFIERS unsigned int _curand_M2_double(T x, curandDistributionM2Shift_t distributionM2)
{
    double u = _curand_uniform_double(x);
    int j = (int) floor(distributionM2->length*u);  


#if defined(__CUDA_ARCH__) && (__CUDA_ARCH__ >= 350)
    double histogramVj = __ldg( &(distributionM2->histogram->V[j]));
    unsigned int histogramKj = __ldg( &(distributionM2->histogram->K[j]));
#else
    double histogramVj = distributionM2->histogram->V[j];
    unsigned int histogramKj = distributionM2->histogram->K[j];
#endif
    //if (u < distributionM2->histogram->V[j]) return distributionM2->shift + j;
    //return distributionM2->shift + distributionM2->histogram->K[j];
    if (u < histogramVj) return distributionM2->shift + j;
    return distributionM2->shift + histogramKj;
}

template <typename T>
// Marsaglia, Tsang, Wang Journal of Statistical Software, square histogram.
QUALIFIERS uint4 _curand_M2_double4(T x, curandDistributionM2Shift_t distributionM2)
{
    double4 u;
    uint4 result = {0,0,0,0};
    int4 flag = {1,1,1,1};

    u.x = _curand_uniform_double(x.x);
    u.y = _curand_uniform_double(x.y);
    u.z = _curand_uniform_double(x.z);
    u.w = _curand_uniform_double(x.w);
    
    int4 j;
    j.x = (int) floor(distributionM2->length*u.x);  
    j.y = (int) floor(distributionM2->length*u.y);  
    j.z = (int) floor(distributionM2->length*u.z);  
    j.w = (int) floor(distributionM2->length*u.w);  
//    int result;

#if defined(__CUDA_ARCH__) && (__CUDA_ARCH__ >= 350)
    double histogramVjx =  __ldg( &(distributionM2->histogram->V[j.x]));
    double histogramVjy =  __ldg( &(distributionM2->histogram->V[j.y]));
    double histogramVjz =  __ldg( &(distributionM2->histogram->V[j.z]));
    double histogramVjw =  __ldg( &(distributionM2->histogram->V[j.w]));

    unsigned int histogramKjx = __ldg( &(distributionM2->histogram->K[j.x]));
    unsigned int histogramKjy = __ldg( &(distributionM2->histogram->K[j.y]));
    unsigned int histogramKjz = __ldg( &(distributionM2->histogram->K[j.z]));
    unsigned int histogramKjw = __ldg( &(distributionM2->histogram->K[j.w]));
#else
    double histogramVjx =  distributionM2->histogram->V[j.x];
    double histogramVjy =  distributionM2->histogram->V[j.y];
    double histogramVjz =  distributionM2->histogram->V[j.z];
    double histogramVjw =  distributionM2->histogram->V[j.w];

    unsigned int histogramKjx = distributionM2->histogram->K[j.x];
    unsigned int histogramKjy = distributionM2->histogram->K[j.y];
    unsigned int histogramKjz = distributionM2->histogram->K[j.z];
    unsigned int histogramKjw = distributionM2->histogram->K[j.w];
#endif

    if (u.x < histogramVjx){ result.x = distributionM2->shift + j.x; flag.x = 0; }
    if (u.y < histogramVjy){ result.y = distributionM2->shift + j.y; flag.y = 0; }
    if (u.z < histogramVjz){ result.z = distributionM2->shift + j.z; flag.z = 0; }
    if (u.w < histogramVjw){ result.w = distributionM2->shift + j.w; flag.w = 0; }
    //return distributionM2->shift + distributionM2->histogram->K[j];
    
    if(flag.x) result.x = distributionM2->shift + histogramKjx;
    if(flag.y) result.y = distributionM2->shift + histogramKjy;
    if(flag.z) result.z = distributionM2->shift + histogramKjz;
    if(flag.w) result.w = distributionM2->shift + histogramKjw;

    return result;
}

template <typename STATE>
QUALIFIERS unsigned int curand_M2_double(STATE *state, curandDistributionM2Shift_t distributionM2)
{
    return _curand_M2_double(curand(state), distributionM2);
}

template <typename STATE>
QUALIFIERS uint4 curand_M2_double4(STATE *state, curandDistributionM2Shift_t distributionM2)
{
    return _curand_M2_double4(curand4(state), distributionM2);
}


template <typename T>
QUALIFIERS unsigned int _curand_binary_search_double(T x, curandDistributionShift_t distribution)
{
    double u = _curand_uniform_double(x);
    int min = 0;
    int max = distribution->length-1;
    do{
        int mid = (max + min)/2;
#if defined(__CUDA_ARCH__) && (__CUDA_ARCH__ >= 350)
        double probability_mid = __ldg( &(distribution->probability[mid]));
#else
        double probability_mid = distribution->probability[mid];
#endif
        if (u <= probability_mid){
            max = mid;
        }else{
            min = mid+1;
        }
    }while (min < max);
    return distribution->shift + min;
}

template <typename STATE>
QUALIFIERS unsigned int curand_binary_search_double(STATE *state, curandDistributionShift_t distribution)
{
    return _curand_binary_search_double(curand(state), distribution);
}


#define MAGIC_DOUBLE_CONST 500.0
template <typename T>
//George S. Fishman Discrete-event simulation: modeling, programming, and analysis
QUALIFIERS unsigned int _curand_poisson_ITR_double(T x, double lambda)
{
  double L,p = 1.0;
  double q = 1.0;
  unsigned int k = 0;
  int pow=0;
  double u = _curand_uniform_double(x);
  do{
      if (lambda > (double)(pow+MAGIC_DOUBLE_CONST)){
          L = exp(-MAGIC_DOUBLE_CONST);
      }else{
          L = exp((double)(pow - lambda));
      }
      p *= L;
      q *= L;
      pow += (int) MAGIC_DOUBLE_CONST;
      while (u > q){
        k++;
        p *= ((double)lambda / (double) k);
        q += p;
      }
  }while((double)pow < lambda);
  return k;
}

template <typename T>
/* Rejection Method for Poisson distribution based on gammainc approximation */
QUALIFIERS unsigned int curand_poisson_gammainc(T state, float lambda){
    float y, x, t, z,v;
    float logl = __cr_log (lambda);
    while (true) {
        y = curand_uniform (state);
        x = __cr_pgammaincinv (lambda, y);
        x = floor (x);
        z = curand_uniform (state);
        v = (__cr_pgammainc (lambda, x + 1.0f) - __cr_pgammainc (lambda, x)) * 1.3f;
        z = z*v;
        t = (float)__cr_exp (-lambda + x * logl - (float)__cr_lgamma_integer ((int)(1.0f + x)));
        if ((z < t) && (v>=1e-20))
            break;
    }
    return (unsigned int)x;
}

template <typename T>
/* Rejection Method for Poisson distribution based on gammainc approximation */
QUALIFIERS uint4 curand_poisson_gammainc4(T state, float lambda){
    uint4 result;
    float y, x, t, z,v;
    float logl = __cr_log (lambda);
    while (true) {
        y = curand_uniform(state);
        x = __cr_pgammaincinv (lambda, y);
        x = floor (x);
        z = curand_uniform (state);
        v = (__cr_pgammainc (lambda, x + 1.0f) - __cr_pgammainc (lambda, x)) * 1.3f;
        z = z*v;
        t = (float)__cr_exp (-lambda + x * logl - (float)__cr_lgamma_integer ((int)(1.0f + x)));
        if ((z < t) && (v>=1e-20))
            break;
    }
    result.x = (unsigned int)x;
    
    while (true) {
        y = curand_uniform(state);
        x = __cr_pgammaincinv (lambda, y);
        x = floor (x);
        z = curand_uniform (state);
        v = (__cr_pgammainc (lambda, x + 1.0f) - __cr_pgammainc (lambda, x)) * 1.3f;
        z = z*v;
        t = (float)__cr_exp (-lambda + x * logl - (float)__cr_lgamma_integer ((int)(1.0f + x)));
        if ((z < t) && (v>=1e-20))
            break;
    }
    result.y = (unsigned int)x;

    while (true) {
        y = curand_uniform(state);
        x = __cr_pgammaincinv (lambda, y);
        x = floor (x);
        z = curand_uniform (state);
        v = (__cr_pgammainc (lambda, x + 1.0f) - __cr_pgammainc (lambda, x)) * 1.3f;
        z = z*v;
        t = (float)__cr_exp (-lambda + x * logl - (float)__cr_lgamma_integer ((int)(1.0f + x)));
        if ((z < t) && (v>=1e-20))
            break;
    }
    result.z = (unsigned int)x;
   
    while (true) {
        y = curand_uniform(state);
        x = __cr_pgammaincinv (lambda, y);
        x = floor (x);
        z = curand_uniform (state);
        v = (__cr_pgammainc (lambda, x + 1.0f) - __cr_pgammainc (lambda, x)) * 1.3f;
        z = z*v;
        t = (float)__cr_exp (-lambda + x * logl - (float)__cr_lgamma_integer ((int)(1.0f + x)));
        if ((z < t) && (v>=1e-20))
            break;
    }
    result.w = (unsigned int)x;
     
    return result;
}
// Note below that the round to nearest integer, where needed,is done in line with code that
// assumes the range of values is < 2**32

template <typename T>
QUALIFIERS unsigned int _curand_poisson(T x, double lambda)
{
    if (lambda < 1000)
        return _curand_poisson_ITR_double(x, lambda);
    return (unsigned int)((sqrt(lambda) * _curand_normal_icdf_double(x)) + lambda + 0.5); //Round to nearest
}

template <typename T>
QUALIFIERS unsigned int _curand_poisson_from_normal(T x, double lambda)
{
    return (unsigned int)((sqrt(lambda) * _curand_normal_icdf(x)) + lambda + 0.5); //Round to nearest
}

template <typename STATE>
QUALIFIERS unsigned int curand_poisson_from_normal(STATE state, double lambda)
{
    return (unsigned int)((sqrt(lambda) * curand_normal(state)) + lambda + 0.5); //Round to nearest
}

template <typename STATE>
QUALIFIERS uint4 curand_poisson_from_normal4(STATE state, double lambda)
{
   uint4 result;
   float4 _res;

   _res = curand_normal4(state);
   
   result.x = (unsigned int)((sqrt(lambda) * _res.x) + lambda + 0.5); //Round to nearest
   result.y = (unsigned int)((sqrt(lambda) * _res.y) + lambda + 0.5); //Round to nearest
   result.z = (unsigned int)((sqrt(lambda) * _res.z) + lambda + 0.5); //Round to nearest
   result.w = (unsigned int)((sqrt(lambda) * _res.w) + lambda + 0.5); //Round to nearest
   return result; //Round to nearest
}

/**
 * \brief Return a Poisson-distributed unsigned int from a XORWOW generator.
 *
 * Return a single unsigned int from a Poisson
 * distribution with lambda \p lambda from the XORWOW generator in \p state,
 * increment the  position of the generator by a variable amount, depending 
 * on the algorithm used.
 *
 * \param state - Pointer to state to update
 * \param lambda - Lambda of the Poisson distribution
 *
 * \return Poisson-distributed unsigned int with lambda \p lambda
 */
QUALIFIERS unsigned int curand_poisson(curandStateXORWOW_t *state, double lambda)
{
    if (lambda < 64)
        return curand_poisson_knuth(state, (float)lambda);
    if (lambda > 4000)
        return (unsigned int)((sqrt(lambda) * curand_normal_double(state)) + lambda + 0.5); //Round to nearest
    return curand_poisson_gammainc(state, (float)lambda);
}

/**
 * \brief Return a Poisson-distributed unsigned int from a Philox4_32_10 generator.
 *
 * Return a single unsigned int from a Poisson
 * distribution with lambda \p lambda from the Philox4_32_10 generator in \p state,
 * increment the  position of the generator by a variable amount, depending 
 * on the algorithm used.
 *
 * \param state - Pointer to state to update
 * \param lambda - Lambda of the Poisson distribution
 *
 * \return Poisson-distributed unsigned int with lambda \p lambda
 */
QUALIFIERS unsigned int curand_poisson(curandStatePhilox4_32_10_t *state, double lambda)
{
    if (lambda < 64)
        return curand_poisson_knuth(state, (float)lambda);
    if (lambda > 4000)
        return (unsigned int)((sqrt(lambda) * curand_normal_double(state)) + lambda + 0.5); //Round to nearest
    return curand_poisson_gammainc(state, (float)lambda);
}
/**
 * \brief Return four Poisson-distributed unsigned ints from a Philox4_32_10 generator.
 *
 * Return a four unsigned ints from a Poisson
 * distribution with lambda \p lambda from the Philox4_32_10 generator in \p state,
 * increment the  position of the generator by a variable amount, depending 
 * on the algorithm used.
 *
 * \param state - Pointer to state to update
 * \param lambda - Lambda of the Poisson distribution
 *
 * \return Poisson-distributed unsigned int with lambda \p lambda
 */
QUALIFIERS uint4 curand_poisson4(curandStatePhilox4_32_10_t *state, double lambda)
{
    uint4 result;
    double4 _res;
    if (lambda < 64)
        return curand_poisson_knuth4(state, (float)lambda);
    if (lambda > 4000) {
        _res = curand_normal4_double(state);
        result.x = (unsigned int)((sqrt(lambda) * _res.x) + lambda + 0.5); //Round to nearest
        result.y = (unsigned int)((sqrt(lambda) * _res.y) + lambda + 0.5); //Round to nearest
        result.z = (unsigned int)((sqrt(lambda) * _res.z) + lambda + 0.5); //Round to nearest
        result.w = (unsigned int)((sqrt(lambda) * _res.w) + lambda + 0.5); //Round to nearest
    	return result;
    }
    return curand_poisson_gammainc4(state, (float)lambda);
}



/**
 * \brief Return a Poisson-distributed unsigned int from a MRG32k3A generator.
 *
 * Return a single unsigned int from a Poisson
 * distribution with lambda \p lambda from the MRG32k3a generator in \p state,
 * increment the position of the generator by a variable amount, depending 
 * on the algorithm used.
 *
 * \param state - Pointer to state to update
 * \param lambda - Lambda of the Poisson distribution
 *
 * \return Poisson-distributed unsigned int with lambda \p lambda
 */
QUALIFIERS unsigned int curand_poisson(curandStateMRG32k3a_t *state, double lambda)
{
    if (lambda < 64)
        return curand_poisson_knuth(state, (float)lambda);
    if (lambda > 4000)
        return (unsigned int)((sqrt(lambda) * curand_normal_double(state)) + lambda + 0.5); //Round to nearest
    return curand_poisson_gammainc(state, (float)lambda);
}

/**
 * \brief Return a Poisson-distributed unsigned int from a MTGP32 generator.
 *
 * Return a single int from a Poisson
 * distribution with lambda \p lambda from the MTGP32 generator in \p state,
 * increment the position of the generator by one.
 *
 * \param state - Pointer to state to update
 * \param lambda - Lambda of the Poisson distribution
 *
 * \return Poisson-distributed unsigned int with lambda \p lambda
 */
QUALIFIERS unsigned int curand_poisson(curandStateMtgp32_t *state, double lambda)
{
    return _curand_poisson(curand(state), lambda);
}

/**
 * \brief Return a Poisson-distributed unsigned int from a Sobol32 generator.
 *
 * Return a single unsigned int from a Poisson
 * distribution with lambda \p lambda from the Sobol32 generator in \p state,
 * increment the position of the generator by one.
 *
 * \param state - Pointer to state to update
 * \param lambda - Lambda of the Poisson distribution
 *
 * \return Poisson-distributed unsigned int with lambda \p lambda
 */

QUALIFIERS unsigned int curand_poisson(curandStateSobol32_t *state, double lambda)
{
    return _curand_poisson(curand(state), lambda);
}

/**
 * \brief Return a Poisson-distributed unsigned int from a scrambled Sobol32 generator.
 *
 * Return a single unsigned int from a Poisson
 * distribution with lambda \p lambda from the scrambled Sobol32 generator in \p state,
 * increment the position of the generator by one.
 *
 * \param state - Pointer to state to update
 * \param lambda - Lambda of the Poisson distribution
 *
 * \return Poisson-distributed unsigned int with lambda \p lambda
 */
QUALIFIERS unsigned int curand_poisson(curandStateScrambledSobol32_t *state, double lambda)
{
    return _curand_poisson(curand(state), lambda);
}

/**
 * \brief Return a Poisson-distributed unsigned int from a Sobol64 generator.
 *
 * Return a single unsigned int from a Poisson
 * distribution with lambda \p lambda from the Sobol64 generator in \p state,
 * increment position of generator by one.
 *
 * \param state - Pointer to state to update
 * \param lambda - Lambda of the Poisson distribution
 *
 * \return Poisson-distributed unsigned int with lambda \p lambda
 */
QUALIFIERS unsigned int curand_poisson(curandStateSobol64_t *state, double lambda)
{
    return _curand_poisson(curand(state), lambda);
}

/**
 * \brief Return a Poisson-distributed unsigned int from a scrambled Sobol64 generator.
 *
 * Return a single unsigned int from a Poisson
 * distribution with lambda \p lambda from the scrambled Sobol64 generator in \p state,
 * increment position of generator by one.
 *
 * \param state - Pointer to state to update
 * \param lambda - Lambda of the Poisson distribution
 *
 * \return Poisson-distributed unsigned int with lambda \p lambda
 */
QUALIFIERS unsigned int curand_poisson(curandStateScrambledSobol64_t *state, double lambda)
{
    return _curand_poisson(curand(state), lambda);
}
#endif // !defined(CURAND_POISSON_H_)
