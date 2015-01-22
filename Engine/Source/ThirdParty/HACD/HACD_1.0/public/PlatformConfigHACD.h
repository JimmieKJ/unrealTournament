// Modifications copyright (c) 2014-2015 Epic Games, Inc. All rights reserved.

#ifndef PLATFORM_CONFIG_H

#define PLATFORM_CONFIG_H

// Modify this header file to make the HACD data types be compatible with your
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stddef.h>
#include <math.h>
#include <float.h>
#include <new>

// This header file provides a brief compatibility layer between the PhysX and APEX SDK foundation header files.
// Modify this header file to your own data types and memory allocation routines and do a global find/replace if necessary


/**
Compiler define
*/
#ifdef _MSC_VER 
#	define HACD_VC
#   if _MSC_VER >= 1500
#       define HACD_VC9
#	elif _MSC_VER >= 1400
#		define HACD_VC8
#	elif _MSC_VER >= 1300
#		define HACD_VC7
#	else
#		define HACD_VC6
#	endif
#elif __GNUC__ || __SNC__
#	define HACD_GNUC
#elif defined(__MWERKS__)
#	define	HACD_CW
#else
#	error "Unknown compiler"
#endif


/**
Platform define
*/
#ifdef HACD_VC
#	ifdef _M_IX86
#		define HACD_X86
#		define HACD_WINDOWS
#   elif defined(_M_X64)
#       define HACD_X64
#       define HACD_WINDOWS
#	elif defined(_M_PPC)
#		define HACD_PPC
#		define HACD_X360
#		define HACD_VMX
#	else
#		error "Unknown platform"
#	endif
#elif defined HACD_GNUC
#   ifdef __CELLOS_LV2__
#	define HACD_PS3
#   elif defined(__arm__)
#	define HACD_LINUX
#	define HACD_ARM
#   elif defined(__i386__)
#       define HACD_X86
#   elif defined(__x86_64__)
#       define HACD_X64
#   elif defined(__ppc__)
#       define HACD_PPC
#   elif defined(__ppc64__)
#       define HACD_PPC
#	define HACD_PPC64
#   else
#	error "Unknown platform"
#   endif
#	if defined(ANDROID)
#   	define HACD_ANDROID
#	elif defined(__linux__)
#   	define HACD_LINUX
#	elif defined(__APPLE__)
#   	define HACD_APPLE
#	elif defined(__CYGWIN__)
#   	define HACD_CYGWIN
#   	define HACD_LINUX
#	endif
#elif defined HACD_CW
#	if defined(__PPCGEKKO__)
#		if defined(RVL)
#			define HACD_WII
#		else
#			define HACD_GC
#		endif
#	else
#		error "Unknown platform"
#	endif
#endif


/**
DLL export macros
*/
#ifndef HACD_C_EXPORT
#define HACD_C_EXPORT extern "C"
#endif

/**
Calling convention
*/
#ifndef HACD_CALL_CONV
#	if defined HACD_WINDOWS
#		define HACD_CALL_CONV __cdecl
#	else
#		define HACD_CALL_CONV
#	endif
#endif

/**
Pack macros - disabled on SPU because they are not supported
*/
#if defined(HACD_VC)
#	define HACD_PUSH_PACK_DEFAULT	__pragma( pack(push, 8) )
#	define HACD_POP_PACK			__pragma( pack(pop) )
#elif defined(HACD_GNUC) && !defined(__SPU__)
#	define HACD_PUSH_PACK_DEFAULT	_Pragma("pack(push, 8)")
#	define HACD_POP_PACK			_Pragma("pack(pop)")
#else
#	define HACD_PUSH_PACK_DEFAULT
#	define HACD_POP_PACK
#endif

/**
Inline macro
*/
#if defined(HACD_WINDOWS) || defined(HACD_X360)
#	define HACD_INLINE inline
#	pragma inline_depth( 255 )
#else
#	define HACD_INLINE inline
#endif

/**
Force inline macro
*/
#if defined(HACD_VC)
#define HACD_FORCE_INLINE __forceinline
#elif defined(HACD_LINUX) // Workaround; Fedora Core 3 do not agree with force inline and PxcPool
#define HACD_FORCE_INLINE inline
#elif defined(HACD_GNUC)
#define HACD_FORCE_INLINE inline __attribute__((always_inline))
#else
#define HACD_FORCE_INLINE inline
#endif

/**
Noinline macro
*/
#if defined HACD_WINDOWS
#	define HACD_NOINLINE __declspec(noinline)
#elif defined(HACD_GNUC)
#	define HACD_NOINLINE __attribute__ ((noinline))
#else
#	define HACD_NOINLINE 
#endif


/*! restrict macro */
#if defined(HACD_GNUC) || defined(HACD_VC)
#	define HACD_RESTRICT __restrict
#elif defined(HACD_CW) && __STDC_VERSION__ >= 199901L
#	define HACD_RESTRICT restrict
#else
#	define HACD_RESTRICT
#endif

#if defined(HACD_WINDOWS) || defined(HACD_X360)
#define HACD_NOALIAS __declspec(noalias)
#else
#define HACD_NOALIAS
#endif


/**
Alignment macros

HACD_ALIGN_PREFIX and HACD_ALIGN_SUFFIX can be used for type alignment instead of aligning individual variables as follows:
HACD_ALIGN_PREFIX(16)
struct A {
...
} HACD_ALIGN_SUFFIX(16);
This declaration style is parsed correctly by Visual Assist.

*/
#ifndef HACD_ALIGN
#if defined(HACD_VC)
#define HACD_ALIGN(alignment, decl) __declspec(align(alignment)) decl
#define HACD_ALIGN_PREFIX(alignment) __declspec(align(alignment))
#define HACD_ALIGN_SUFFIX(alignment)
#elif defined(HACD_GNUC)
#define HACD_ALIGN(alignment, decl) decl __attribute__ ((aligned(alignment)))
#define HACD_ALIGN_PREFIX(alignment)
#define HACD_ALIGN_SUFFIX(alignment) __attribute__ ((aligned(alignment)))
#elif defined(HACD_CW)
#define HACD_ALIGN(alignment, decl) decl __attribute__ ((aligned(alignment)))
#define HACD_ALIGN_PREFIX(alignment)
#define HACD_ALIGN_SUFFIX(alignment) __attribute__ ((aligned(alignment)))
#else
#define HACD_ALIGN(alignment, decl)
#define HACD_ALIGN_PREFIX(alignment)
#define HACD_ALIGN_SUFFIX(alignment)
#endif
#endif

/**
Deprecated marco
*/
#if 0 // set to 1 to create warnings for deprecated functions
#	define HACD_DEPRECATED __declspec(deprecated)
#else 
#	define HACD_DEPRECATED
#endif

// VC6 no '__FUNCTION__' workaround
#if defined HACD_VC6 && !defined __FUNCTION__
#	define __FUNCTION__	"Undefined"
#endif

/**
General defines
*/

// static assert
#define HACD_JOIN_HELPER(X, Y) X##Y
#define HACD_JOIN(X, Y, Z) HACD_JOIN_HELPER(X, Y##Z)
#define HACD_COMPILE_TIME_ASSERT(exp)	typedef char HACD_JOIN(PxCompileTimeAssert, __FILE__ , __COUNTER__ )[(exp) ? 1 : -1]

#ifdef HACD_GNUC
#define HACD_OFFSET_OF(X, Y) __builtin_offsetof(X, Y)
#else
#define HACD_OFFSET_OF(X, Y) offsetof(X, Y)
#endif

// avoid unreferenced parameter warning (why not just disable it?)
// PT: or why not just omit the parameter's name from the declaration????
#define HACD_FORCE_PARAMETER_REFERENCE(_P) (void)(_P);
#define HACD_UNUSED(_P) HACD_FORCE_PARAMETER_REFERENCE(_P)

// check that exactly one of NDEBUG and _DEBUG is defined
#if !(defined NDEBUG ^ defined _DEBUG)
#error Exactly one of NDEBUG and _DEBUG needs to be defined by preprocessor
#endif

// make sure HACD_CHECKED is defined in all _DEBUG configurations as well
#if !defined(HACD_CHECKED) && _DEBUG
#define HACD_CHECKED
#endif

#ifdef __CUDACC__
#define HACD_CUDA_CALLABLE __host__ __device__
#else
#define HACD_CUDA_CALLABLE
#endif

#if PLATFORM_MAC || PLATFORM_LINUX // @todo Mac
#include <stdint.h>
#include <string.h>

#ifndef _aligned_malloc
#define _aligned_malloc(Size,Align) malloc(Size)
#define _aligned_realloc(Ptr,Size,Align) realloc(Ptr,Size)
#define _aligned_free(Ptr) free(Ptr)
#endif

#define _isnan isnan
#define _finite isfinite

#endif // PLATFORM_MAC || PLATFORM_LINUX

namespace hacd
{
#if PLATFORM_MAC || PLATFORM_LINUX // @todo Mac
	typedef int64_t				HaI64;
#else
	typedef signed __int64		HaI64;
#endif
	typedef signed int			HaI32;
	typedef signed short		HaI16;
	typedef signed char			HaI8;

#if PLATFORM_MAC || PLATFORM_LINUX // @todo Mac
	typedef uint64_t			HaU64;
#else
	typedef unsigned __int64	HaU64;
#endif
	typedef unsigned int		HaU32;
	typedef unsigned short		HaU16;
	typedef unsigned char		HaU8;

	typedef float				HaF32;
	typedef double				HaF64;



	class PxEmpty;

#define HACD_SIGN_BITMASK		0x80000000

extern size_t gAllocCount;
extern size_t gAllocSize;

HACD_INLINE void * AllocAligned(size_t size,size_t alignment)
{
	gAllocSize+=size;
	gAllocCount++;
	return ::_aligned_malloc(size,alignment);
}

#define HACD_ALLOC_ALIGNED(x,y) hacd::AllocAligned(x,y)
#define HACD_ALLOC(x) hacd::AllocAligned(x,16)
#define HACD_FREE(x) ::_aligned_free(x)

#define HACD_ASSERT(x) assert(x)
#define HACD_ALWAYS_ASSERT() assert(0)

#define HACD_PLACEMENT_NEW(p, T)  new(p) T

	class UserAllocated
	{
	public:
		HACD_INLINE void* operator new(size_t size,UserAllocated *t)
		{
			HACD_FORCE_PARAMETER_REFERENCE(size);
			return t;
		}

		HACD_INLINE void* operator new(size_t size,const char *className,const char* fileName, int lineno,size_t classSize)
		{
			HACD_FORCE_PARAMETER_REFERENCE(className);
			HACD_FORCE_PARAMETER_REFERENCE(fileName);
			HACD_FORCE_PARAMETER_REFERENCE(lineno);
			HACD_FORCE_PARAMETER_REFERENCE(classSize);
			return HACD_ALLOC(size);
		}

		inline void* operator new[](size_t size,const char *className,const char* fileName, int lineno,size_t classSize)
		{
			HACD_FORCE_PARAMETER_REFERENCE(className);
			HACD_FORCE_PARAMETER_REFERENCE(fileName);
			HACD_FORCE_PARAMETER_REFERENCE(lineno);
			HACD_FORCE_PARAMETER_REFERENCE(classSize);
			return HACD_ALLOC(size);
		}

		inline void  operator delete(void* p,UserAllocated *t)
		{
			HACD_FORCE_PARAMETER_REFERENCE(p);
			HACD_FORCE_PARAMETER_REFERENCE(t);
			HACD_ALWAYS_ASSERT(); // should never be executed
		}

		inline void  operator delete(void* p)
		{
			HACD_FREE(p);
		}

		inline void  operator delete[](void* p)
		{
			HACD_FREE(p);
		}

		inline void  operator delete(void *p,const char *className,const char* fileName, int line,size_t classSize)
		{
			HACD_FORCE_PARAMETER_REFERENCE(className);
			HACD_FORCE_PARAMETER_REFERENCE(fileName);
			HACD_FORCE_PARAMETER_REFERENCE(line);
			HACD_FORCE_PARAMETER_REFERENCE(classSize);
			HACD_FREE(p);
		}

		inline void  operator delete[](void *p,const char *className,const char* fileName, int line,size_t classSize)
		{
			HACD_FORCE_PARAMETER_REFERENCE(className);
			HACD_FORCE_PARAMETER_REFERENCE(fileName);
			HACD_FORCE_PARAMETER_REFERENCE(line);
			HACD_FORCE_PARAMETER_REFERENCE(classSize);
			HACD_FREE(p);
		}

	};

	class ICallback
	{
	public:
		virtual void ReportProgress(const char *, HaF32 progress) = 0;
		virtual bool Cancelled() = 0;
	};

#define HACD_NEW(T) new(#T,__FILE__,__LINE__,sizeof(T)) T

#if PLATFORM_MAC || PLATFORM_LINUX // @todo Mac
#define HACD_SPRINTF_S snprintf
#else
#define HACD_SPRINTF_S sprintf_s
#endif

#ifdef HACD_X64
typedef HaU64 HaSizeT;
#else
typedef HaU32 HaSizeT;
#endif

#define HACD_ISFINITE(x) _finite(x)

}; // end hacd namespace

#define UANS hacd	// the user allocator namespace

#include "PxVector.h"

#endif
