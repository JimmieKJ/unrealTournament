/************************************************************************************

PublicHeader:   OVR_Kernel.h
Filename    :   OVR_Types.h
Content     :   Standard library defines and simple types
Created     :   September 19, 2012
Notes       : 

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

************************************************************************************/

#ifndef OVR_Types_h
#define OVR_Types_h

#include "OVR_Compiler.h"


// Unsupported compiler configurations
#if _MSC_VER == 0x1600
#  if _MSC_FULL_VER < 160040219
#     error "Oculus does not support VS2010 without SP1 installed: It will crash in Release mode"
#  endif
#endif


//-----------------------------------------------------------------------------------
// ****** Operating system identification
//
// Type definitions exist for the following operating systems: (OVR_OS_x)
//
//    WIN32    - Win32 (Windows 95/98/ME and Windows NT/2000/XP)
//    DARWIN   - Darwin OS (Mac OS X)
//    LINUX    - Linux
//    ANDROID  - Android
//    IPHONE   - iPhone

#if (defined(__APPLE__) && (defined(__GNUC__) ||\
     defined(__xlC__) || defined(__xlc__))) || defined(__MACOS__)
#  if (defined(__ENVIRONMENT_IPHONE_OS_VERSION_MIN_REQUIRED__) || defined(__IPHONE_OS_VERSION_MIN_REQUIRED))
#    define OVR_OS_IPHONE
#  else
#    define OVR_OS_DARWIN
#    define OVR_OS_MAC
#  endif
#elif (defined(WIN64) || defined(_WIN64) || defined(__WIN64__))
#  define OVR_OS_WIN32
#elif (defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__))
#  define OVR_OS_WIN32
#elif defined(__linux__) || defined(__linux)
#  define OVR_OS_LINUX
#else
#  define OVR_OS_OTHER
#endif

#if defined(ANDROID)
#  define OVR_OS_ANDROID
#endif


//-----------------------------------------------------------------------------------
// ***** CPU Architecture
//
// The following CPUs are defined: (OVR_CPU_x)
//
//    X86        - x86 (IA-32)
//    X86_64     - x86_64 (amd64)
//    PPC        - PowerPC
//    PPC64      - PowerPC64
//    MIPS       - MIPS
//    OTHER      - CPU for which no special support is present or needed


#if defined(__x86_64__) || defined(WIN64) || defined(_WIN64) || defined(__WIN64__)
#  define OVR_CPU_X86_64
#  define OVR_64BIT_POINTERS
#elif defined(__i386__) || defined(OVR_OS_WIN32)
#  define OVR_CPU_X86
#elif defined(__powerpc64__)
#  define OVR_CPU_PPC64
#elif defined(__ppc__)
#  define OVR_CPU_PPC
#elif defined(__mips__) || defined(__MIPSEL__)
#  define OVR_CPU_MIPS
#elif defined(__arm__)
#  define OVR_CPU_ARM
#else
#  define OVR_CPU_OTHER
#endif

//-----------------------------------------------------------------------------------
// ***** Co-Processor Architecture
//
// The following co-processors are defined: (OVR_CPU_x)
//
//    SSE        - Available on all modern x86 processors.
//    Altivec    - Available on all modern ppc processors.
//    Neon       - Available on some armv7+ processors.

#if defined(__SSE__) || defined(OVR_OS_WIN32)
#  define  OVR_CPU_SSE
#endif // __SSE__

#if defined( __ALTIVEC__ )
#  define OVR_CPU_ALTIVEC
#endif // __ALTIVEC__

#if defined(__ARM_NEON__)
#  define OVR_CPU_ARM_NEON
#endif // __ARM_NEON__


#if defined( OVR_CPP11 )
#define OVR_OVERRIDE override
#else
#define OVR_OVERRIDE
#endif

//-----------------------------------------------------------------------------------
// ***** Compiler Warnings

// Disable MSVC warnings
#if defined(OVR_CC_MSVC)
#  pragma warning(disable : 4127)    // Inconsistent dll linkage
#  pragma warning(disable : 4530)    // Exception handling
#  pragma warning(disable : 4351)	 // new behavior: elements of array will be default initialized
#  if (OVR_CC_MSVC<1300)
#    pragma warning(disable : 4514)  // Unreferenced inline function has been removed
#    pragma warning(disable : 4710)  // Function not inlined
#    pragma warning(disable : 4714)  // _force_inline not inlined
#    pragma warning(disable : 4786)  // Debug variable name longer than 255 chars
#  endif // (OVR_CC_MSVC<1300)
#endif // (OVR_CC_MSVC)



// *** Linux Unicode - must come before Standard Includes

#ifdef OVR_OS_LINUX
// Use glibc unicode functions on linux.
#  ifndef  _GNU_SOURCE
#    define _GNU_SOURCE
#  endif
#endif

//-----------------------------------------------------------------------------------
// ***** Standard Includes
//
#include    <stddef.h>
#include    <limits.h>
#include    <float.h>


// MSVC Based Memory Leak checking - for now
#if defined(OVR_CC_MSVC) && defined(OVR_BUILD_DEBUG)
#  define _CRTDBG_MAP_ALLOC
#  include <stdlib.h>
#  include <crtdbg.h>
#endif


//-----------------------------------------------------------------------------------
// ***** int8_t, int16_t, etc.

#if defined(OVR_CC_MSVC) && (OVR_CC_VERSION <= 1500) // VS2008 and earlier
    typedef signed char        int8_t;
    typedef unsigned char     uint8_t;
    typedef signed short      int16_t;
    typedef unsigned short   uint16_t;
    typedef signed int        int32_t;
    typedef unsigned int     uint32_t;
    typedef signed __int64    int64_t;
    typedef unsigned __int64 uint64_t;
#else
    #include <stdint.h>
#endif


//-----------------------------------------------------------------------------------
// ***** Type definitions for Common Systems

namespace OVR {

typedef char            Char;

// Pointer-sized integer
typedef size_t          UPInt;
typedef ptrdiff_t       SPInt;


#if defined(OVR_OS_WIN32)

typedef char            SByte;  // 8 bit Integer (Byte)
typedef unsigned char   UByte;
typedef short           SInt16; // 16 bit Integer (Word)
typedef unsigned short  UInt16;
typedef long            SInt32; // 32 bit Integer
typedef unsigned long   UInt32;
typedef __int64         SInt64; // 64 bit Integer (QWord)
typedef unsigned __int64 UInt64;

 
#elif defined(OVR_OS_MAC) || defined(OVR_OS_IPHONE) || defined(OVR_CC_GNU)

typedef int             SByte  __attribute__((__mode__ (__QI__)));
typedef unsigned int    UByte  __attribute__((__mode__ (__QI__)));
typedef int             SInt16 __attribute__((__mode__ (__HI__)));
typedef unsigned int    UInt16 __attribute__((__mode__ (__HI__)));
typedef int             SInt32 __attribute__((__mode__ (__SI__)));
typedef unsigned int    UInt32 __attribute__((__mode__ (__SI__)));
typedef int             SInt64 __attribute__((__mode__ (__DI__)));
typedef unsigned int    UInt64 __attribute__((__mode__ (__DI__)));

#else

#include <sys/types.h>
typedef int8_t          SByte;
typedef uint8_t         UByte;
typedef int16_t         SInt16;
typedef uint16_t        UInt16;
typedef int32_t         SInt32;
typedef uint32_t        UInt32;
typedef int64_t         SInt64;
typedef uint64_t        UInt64;

#endif
    
    
//osx PID is a signed int32 (already defined to pid_t in OSX framework)
//linux PID is a signed int32 (already defined)
//win32 PID is an unsigned int64
#ifdef OVR_OS_WIN32
    //process ID representation
    typedef unsigned long pid_t;
#endif


} // namespace OVR

#if defined( OVR_OS_ANDROID )
#include <jni.h>
#elif defined( __cplusplus )
typedef struct _JNIEnv JNIEnv;
typedef struct _JavaVM JavaVM;
typedef struct _jmethodID * jmethodID;
typedef class _jobject * jobject;
typedef class _jobject * jclass;
typedef class _jobject * jstring;
typedef long long jlong;
typedef OVR::SInt32 jint;
typedef OVR::UByte jboolean;
#else
typedef const struct JNINativeInterface * JNIEnv;
typedef const struct JNIInvokeInterface * JavaVM;
typedef const struct _jmethodID * jmethodID;
void * jobject;
typedef jobject jclass;
typedef jobject jstring;
typedef long long jlong;
typedef int jint;
typedef unsigned char jboolean;
#endif

//-----------------------------------------------------------------------------------
// ***** Macro Definitions
//
// We define the following:
//
//  OVR_BYTE_ORDER      - Defined to either OVR_LITTLE_ENDIAN or OVR_BIG_ENDIAN
//  OVR_FORCE_INLINE    - Forces inline expansion of function
//  OVR_ASM             - Assembly language prefix
//  OVR_STR             - Prefixes string with L"" if building unicode
// 
//  OVR_STDCALL         - Use stdcall calling convention (Pascal arg order)
//  OVR_CDECL           - Use cdecl calling convention (C argument order)
//  OVR_FASTCALL        - Use fastcall calling convention (registers)
//

// Byte order constants, OVR_BYTE_ORDER is defined to be one of these.
#define OVR_LITTLE_ENDIAN       1
#define OVR_BIG_ENDIAN          2


#if defined(OVR_OS_WIN32)
    
    // ***** Win32

	// _DEBUG is added by default to debug projects in Visual Studio, so auto-define OVR_BUILD_DEBUG if it's set.
#	if defined( _DEBUG ) && !defined( OVR_BUILD_DEBUG )	
#		define OVR_BUILD_DEBUG
#	endif
    // Byte order
    #define OVR_BYTE_ORDER    OVR_LITTLE_ENDIAN

    // Calling convention - goes after function return type but before function name
    #ifdef __cplusplus_cli
    #  define OVR_FASTCALL      __stdcall
    #else
    #  define OVR_FASTCALL      __fastcall
    #endif

    #define OVR_STDCALL         __stdcall
    #define OVR_CDECL           __cdecl


    // Assembly macros
    #if defined(OVR_CC_MSVC)
    #  define OVR_ASM           _asm
    #else
    #  define OVR_ASM           asm
    #endif // (OVR_CC_MSVC)

    #ifdef UNICODE
    #  define OVR_STR(str)      L##str
    #else
    #  define OVR_STR(str)      str
    #endif // UNICODE

#else

    // **** Standard systems

    #if (defined(BYTE_ORDER) && (BYTE_ORDER == BIG_ENDIAN))|| \
        (defined(_BYTE_ORDER) && (_BYTE_ORDER == _BIG_ENDIAN))
    #  define OVR_BYTE_ORDER    OVR_BIG_ENDIAN
    #elif (defined(__ARMEB__) || defined(OVR_CPU_PPC) || defined(OVR_CPU_PPC64))
    #  define OVR_BYTE_ORDER    OVR_BIG_ENDIAN
    #else
    #  define OVR_BYTE_ORDER    OVR_LITTLE_ENDIAN
    #endif
    
    // Assembly macros
    #define OVR_ASM                  __asm__
    #define OVR_ASM_PROC(procname)   OVR_ASM
    #define OVR_ASM_END              OVR_ASM
    
    // Calling convention - goes after function return type but before function name
    #define OVR_FASTCALL
    #define OVR_STDCALL
    #define OVR_CDECL

#endif // defined(OVR_OS_WIN32)


// ------------------------------------------------------------------------
// ***** OVR_FORCE_INLINE
//
// Force inline substitute - goes before function declaration
// Example usage:
//     OVR_FORCE_INLINE void Test();

#if defined(OVR_CC_MSVC)
#  define OVR_FORCE_INLINE  __forceinline
#elif defined(OVR_CC_GNU)
#  define OVR_FORCE_INLINE  inline __attribute__((always_inline))
#else
#  define OVR_FORCE_INLINE  inline
#endif  // OVR_CC_MSVC

//-----------------------------------------------------------------------------------
// ***** OVR_DEBUG_BREAK,
//       OVR_ASSERT,
//
// Macros have effect only in debug builds.
//
// Example OVR_DEBUG_BREAK usage (note the lack of parentheses):
//     #define MY_ASSERT(expression) do { if (!(expression)) { OVR_DEBUG_BREAK; } } while(0)
//
//
// Example OVR_ASSERT usage:
//     OVR_ASSERT(count < 100);
//
#ifndef OVR_BUILD_DEBUG

#  define OVR_DEBUG_BREAK  ((void)0)
#  define OVR_ASSERT(p)    ((void)0)

#else 

// Microsoft Win32 specific debugging support
#if defined(OVR_OS_WIN32)
#  ifdef OVR_CPU_X86
#    if defined(__cplusplus_cli)
#      define OVR_DEBUG_BREAK   do { __debugbreak(); } while(0)
#    elif defined(OVR_CC_GNU)
#      define OVR_DEBUG_BREAK   do { OVR_ASM("int $3\n\t"); } while(0)
#    else
#      define OVR_DEBUG_BREAK   do { OVR_ASM int 3 } while (0)
#    endif
#  else
#    define OVR_DEBUG_BREAK     do { __debugbreak(); } while(0)
#  endif
// Android specific debugging support
#elif defined(OVR_OS_ANDROID)
#  include <android/log.h>
#  define OVR_EXPAND1( s )		#s
#  define OVR_EXPAND( s ) OVR_EXPAND1( s )
#  define OVR_DEBUG_BREAK		do { __builtin_trap(); } while(0)
#  define OVR_ASSERT(p)			do { if (!(p)) { __android_log_write( ANDROID_LOG_WARN, "OVR", "ASSERT@ " __FILE__ "(" OVR_EXPAND( __LINE__ ) "): " #p ); OVR_DEBUG_BREAK; } } while(0)
// Unix specific debugging support
#elif defined(OVR_CPU_X86) || defined(OVR_CPU_X86_64)
#  define OVR_DEBUG_BREAK       do { OVR_ASM("int $3\n\t"); } while(0)
#else
#  define OVR_DEBUG_BREAK       do { *((int *) 0) = 1; } while(0)
#endif

#if !defined( OVR_ASSERT ) // Android currently defines its own version of OVR_ASSERT() with logging
// This will cause compiler breakpoint
#define OVR_ASSERT(p)           do { if (!(p))  { OVR_DEBUG_BREAK; } } while(0)
#endif

#endif // OVR_BUILD_DEBUG

// ------------------------------------------------------------------------
// ***** OVR_COMPILER_ASSERT
//
// Compile-time assert; produces compiler error if condition is false.
// The expression must be a compile-time constant expression.
// This macro is deprecated in favor of static_assert, which provides better
// compiler output and works in a broader range of contexts.
// 
// Example usage:
//     OVR_COMPILER_ASSERT(sizeof(int32_t == 4));

#define OVR_COMPILER_ASSERT(x)  { int zero = 0; switch(zero) {case 0: case x:;} }

// ------------------------------------------------------------------------
// ***** OVR_VERIFY_ARRAY_SIZE
// An enumeration type must have a corresponding array of items where the 
// number of items exactly matches the number of enums in the enumeration type.
// This macro causes a compile-time error if the number of elements in the array 
// does not match the value of num_elements_. Use of this macro ensures that 
// enum types and corresponding arrays do not get out of sync.
// Note that the array must be defined as:
// 
// type Array[] = { } 
// vs.
// type Array[Maxvalue] = { }
// 
// Otherwise sizeof( Array ) would always return MaxValue, independent of how
// many entries were initialized in the array.
#define OVR_VERIFY_ARRAY_SIZE( array_, num_elements_ )	\
static_assert( sizeof( array_ ) / sizeof( array_[0] ) == num_elements_, "Array " #array_ " must have " #num_elements_ " elements!" )


//-----------------------------------------------------------------------------------
// ***** OVR_UNUSED - Unused Argument handling
// Macro to quiet compiler warnings about unused parameters/variables.
//
// Example usage:
//     void Test() {
//         int x = SomeFunction();
//         OVR_UNUSED(x);
//     }
//

#if defined(OVR_CC_GNU)
#  define   OVR_UNUSED(a)   do {__typeof__ (&a) __attribute__ ((unused)) __tmp = &a; } while(0)
#else
#  define   OVR_UNUSED(a)   (a)
#endif

#define     OVR_UNUSED1(a1) OVR_UNUSED(a1)
#define     OVR_UNUSED2(a1,a2) OVR_UNUSED(a1); OVR_UNUSED(a2)
#define     OVR_UNUSED3(a1,a2,a3) OVR_UNUSED2(a1,a2); OVR_UNUSED(a3)
#define     OVR_UNUSED4(a1,a2,a3,a4) OVR_UNUSED3(a1,a2,a3); OVR_UNUSED(a4)
#define     OVR_UNUSED5(a1,a2,a3,a4,a5) OVR_UNUSED4(a1,a2,a3,a4); OVR_UNUSED(a5)
#define     OVR_UNUSED6(a1,a2,a3,a4,a5,a6) OVR_UNUSED4(a1,a2,a3,a4); OVR_UNUSED2(a5,a6)
#define     OVR_UNUSED7(a1,a2,a3,a4,a5,a6,a7) OVR_UNUSED4(a1,a2,a3,a4); OVR_UNUSED3(a5,a6,a7)
#define     OVR_UNUSED8(a1,a2,a3,a4,a5,a6,a7,a8) OVR_UNUSED4(a1,a2,a3,a4); OVR_UNUSED4(a5,a6,a7,a8)
#define     OVR_UNUSED9(a1,a2,a3,a4,a5,a6,a7,a8,a9) OVR_UNUSED4(a1,a2,a3,a4); OVR_UNUSED5(a5,a6,a7,a8,a9)


//-----------------------------------------------------------------------------------
// ***** Configuration Macros
//
// Expands to the current build type as a const char string literal.
// Acts as the following declaration: const char OVR_BUILD_STRING[];

#ifdef OVR_BUILD_DEBUG
#  define OVR_BUILD_STRING  "Debug"
#else
#  define OVR_BUILD_STRING  "Release"
#endif


//// Enables SF Debugging information
//# define OVR_BUILD_DEBUG

// OVR_DEBUG_STATEMENT injects a statement only in debug builds.
// OVR_DEBUG_SELECT injects first argument in debug builds, second argument otherwise.
#ifdef OVR_BUILD_DEBUG
#define OVR_DEBUG_STATEMENT(s)   s
#define OVR_DEBUG_SELECT(d, nd)  d
#else
#define OVR_DEBUG_STATEMENT(s)
#define OVR_DEBUG_SELECT(d, nd)  nd
#endif


#define OVR_ENABLE_THREADS
//
// Prevents OVR from defining new within
// type macros, so developers can override
// new using the #define new new(...) trick
// - used with OVR_DEFINE_NEW macro
//# define OVR_BUILD_DEFINE_NEW
//


#endif  // OVR_Types_h
