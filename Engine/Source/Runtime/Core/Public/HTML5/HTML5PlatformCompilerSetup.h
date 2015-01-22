// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


/*=============================================================================================
	HTML5CompilerSetup.h: pragmas, version checks and other things for the HTML5 compiler
==============================================================================================*/

#pragma once
#include "HAL/Platform.h"

#if PLATFORM_HTML5_WIN32
#pragma warning(disable : 4481) // nonstandard extension used: override specifier 'override'
// disable this now as it is annoying for generic platfrom implementations
#pragma warning(disable : 4100) // unreferenced formal parameter
#pragma warning(disable : 4996) // 'function' was was declared deprecated  (needed for the secure string functions)
#pragma warning(disable : 4309) // truncation of constant value
#pragma warning(disable : 4512) // assignment operator could not be generated                           
// Unwanted VC++ level 4 warnings to disable.
#pragma warning(disable : 4100) // unreferenced formal parameter										
#pragma warning(disable : 4127) // Conditional expression is constant									
#pragma warning(disable : 4200) // Zero-length array item at end of structure, a VC-specific extension	
#pragma warning(disable : 4201) // nonstandard extension used : nameless struct/union	
#pragma warning(disable : 4244) // 'expression' : conversion from 'type' to 'type', possible loss of data						
#pragma warning(disable : 4245) // 'initializing': conversion from 'type' to 'type', signed/unsigned mismatch 
#pragma warning(disable : 4251) // 'type' needs to have dll-interface to be used by clients of 'type'
#pragma warning(disable : 4291) // typedef-name '' used as synonym for class-name ''                    
//#pragma warning(disable : 4305) // 'argument' : truncation from 'double' to 'float' --- fp:precise builds only!!!!!!!
#pragma warning(disable : 4324) // structure was padded due to __declspec(align())						
#pragma warning(disable : 4355) // this used in base initializer list                                   
#pragma warning(disable : 4389) // signed/unsigned mismatch                                             
#pragma warning(disable : 4511) // copy constructor could not be generated                              
#pragma warning(disable : 4512) // assignment operator could not be generated                           
#pragma warning(disable : 4514) // unreferenced inline function has been removed						
#pragma warning(disable : 4699) // creating precompiled header											
#pragma warning(disable : 4702) // unreachable code in inline expanded function							
#pragma warning(disable : 4710) // inline function not expanded											
#pragma warning(disable : 4711) // function selected for automatic inlining								
#pragma warning(disable : 4714) // __forceinline function not expanded									
#pragma warning(disable : 4482) // nonstandard extension used: enum 'enum' used in qualified name (having hte enum name helps code readability and should be part of TR1 or TR2)
#pragma warning(disable : 4748)	// /GS can not protect parameters and local variables from local buffer overrun because optimizations are disabled in function
// NOTE: _mm_cvtpu8_ps will generate this falsely if it doesn't get inlined
#pragma warning(disable : 4799)	// Warning: function 'ident' has no EMMS instruction
#pragma warning(disable : 4275) // non - DLL-interface classkey 'identifier' used as base for DLL-interface classkey 'identifier'

// all of the /Wall warnings that we are able to enable
// @todo:  http://msdn2.microsoft.com/library/23k5d385(en-us,vs.80).aspx
#pragma warning(default : 4191) // 'operator/operation' : unsafe conversion from 'type of expression' to 'type required'
#pragma warning(disable : 4217) // 'operator' : member template functions cannot be used for copy-assignment or copy-construction
//#pragma warning(disable : 4242) // 'variable' : conversion from 'type' to 'type', possible loss of data
//#pragma warning(default : 4254) // 'operator' : conversion from 'type1' to 'type2', possible loss of data
#pragma warning(default : 4255) // 'function' : no function prototype given: converting '()' to '(void)'
#pragma warning(default : 4263) // 'function' : member function does not override any base class virtual member function
#pragma warning(default : 4264) // 'virtual_function' : no override available for virtual member function from base 'class'; function is hidden
#pragma warning(disable : 4267) // 'argument' : conversion from 'type1' to 'type2', possible loss of data
#pragma warning(default : 4287) // 'operator' : unsigned/negative constant mismatch
#pragma warning(default : 4289) // nonstandard extension used : 'var' : loop control variable declared in the for-loop is used outside the for-loop scope
#pragma warning(default : 4302) // 'conversion' : truncation from 'type 1' to 'type 2'
#pragma warning(disable : 4315) // 'this' pointer for member may not be aligned 8 as expected by the constructor
#pragma warning(disable : 4316) //  over aligned allocation warning 
//#pragma warning(disable : 4339) // 'type' : use of undefined type detected in CLR meta-data - use of this type may lead to a runtime exception
#pragma warning(disable : 4347) // behavior change: 'function template' is called instead of 'function
#pragma warning(disable : 4366) // The result of the unary '&' operator may be unaligned
#pragma warning(disable : 4514) // unreferenced inline/local function has been removed
#pragma warning(default : 4529) // 'member_name' : forming a pointer-to-member requires explicit use of the address-of operator ('&') and a qualified name
#pragma warning(default : 4536) // 'type name' : type-name exceeds meta-data limit of 'limit' characters
#pragma warning(default : 4545) // expression before comma evaluates to a function which is missing an argument list
#pragma warning(default : 4546) // function call before comma missing argument list
//#pragma warning(default : 4547) // 'operator' : operator before comma has no effect; expected operator with side-effect
//#pragma warning(default : 4548) // expression before comma has no effect; expected expression with side-effect  (needed as xlocale does not compile cleanly)
//#pragma warning(default : 4549) // 'operator' : operator before comma has no effect; did you intend 'operator'?
//#pragma warning(disable : 4555) // expression has no effect; expected expression with side-effect
#pragma warning(default : 4557) // '__assume' contains effect 'effect'
#pragma warning(disable : 4623) // 'derived class' : default constructor could not be generated because a base class default constructor is inaccessible
#pragma warning(disable : 4625) // 'derived class' : copy constructor could not be generated because a base class copy constructor is inaccessible
#pragma warning(disable : 4626) // 'derived class' : assignment operator could not be generated because a base class assignment operator is inaccessible
#pragma warning(default : 4628) // digraphs not supported with -Ze. Character sequence 'digraph' not interpreted as alternate token for 'char'
#pragma warning(disable : 4640) // 'instance' : construction of local static object is not thread-safe
#pragma warning(disable : 4668) // 'symbol' is not defined as a preprocessor macro, replacing with '0' for 'directives'
#pragma warning(default : 4682) // 'parameter' : no directional parameter attribute specified, defaulting to [in]
#pragma warning(default : 4686) // 'user-defined type' : possible change in behavior, change in UDT return calling convention
#pragma warning(disable : 4710) // 'function' : function not inlined / The given function was selected for inline expansion, but the compiler did not perform the inlining.
#pragma warning(default : 4786) // 'identifier' : identifier was truncated to 'number' characters in the debug information
#pragma warning(default : 4793) // native code generated for function 'function': 'reason'
#pragma warning(default : 4905) // wide string literal cast to 'LPSTR'
#pragma warning(default : 4906) // string literal cast to 'LPWSTR'
#pragma warning(disable : 4917) // 'declarator' : a GUID cannot only be associated with a class, interface or namespace ( ocid.h breaks this)
#pragma warning(default : 4931) // we are assuming the type library was built for number-bit pointers
#pragma warning(default : 4946) // reinterpret_cast used between related classes: 'class1' and 'class2'
#pragma warning(default : 4928) // illegal copy-initialization; more than one user-defined conversion has been implicitly applied
#pragma warning(disable : 4180) // qualifier applied to function type has no meaning; ignored
#pragma warning(disable : 4121) // 'symbol' : alignment of a member was sensitive to packing
#pragma warning(disable : 4345) // behavior change: an object of POD type constructed with an initializer of the form () will be default-initialized

#if UE_BUILD_DEBUG
// xstring.h causes this warning in debug builds
#pragma warning(disable : 4548) // expression before comma has no effect; expected expression with side-effect
#endif

// interesting ones to turn on and off at times
//#pragma warning(disable : 4265) // 'class' : class has virtual functions, but destructor is not virtual
//#pragma warning(disable : 4266) // '' : no override available for virtual member function from base ''; function is hidden
//#pragma warning(disable : 4296) // 'operator' : expression is always true / false
//#pragma warning(disable : 4820) // 'bytes' bytes padding added after member 'member'
// Mixing MMX/SSE intrinsics will cause this warning, even when it's done correctly.
//#pragma warning(disable : 4730) //mixing _m64 and floating point expressions may result in incorrect code

// It'd be nice to turn these on, but at the moment they can't be used in DEBUG due to the varargs stuff.	
#pragma warning(disable : 4189) // local variable is initialized but not referenced 
#pragma warning(disable : 4505) // unreferenced local function has been removed		

// If C++ exception handling is disabled, force guarding to be off.
#if !defined(_CPPUNWIND) && !defined(__INTELLISENSE__) && !defined(HACK_HEADER_GENERATOR)
#error "Bad VCC option: C++ exception handling must be enabled" //lint !e309 suppress as lint doesn't have this defined
#endif

// Make sure characters are unsigned.
#ifdef _CHAR_UNSIGNED
#error "Bad VC++ option: Characters must be signed" //lint !e309 suppress as lint doesn't have this defined
#endif
#endif
