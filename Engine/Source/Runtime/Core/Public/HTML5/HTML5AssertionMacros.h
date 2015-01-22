// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once 

// scratch pad for HTML5 assertion macros macros. 

#if !PLATFORM_HTML5_WIN32
extern "C" {
void emscripten_log(int flags, ...);
}
#endif

#if DO_CHECK && !PLATFORM_HTML5_WIN32

// For the asm.js builds, use emscripten-specific versions of these macros

#undef checkCode
#undef check
#undef checkf
#undef checkNoEntry
#undef checkNoReentry
#undef checkNoRecursion
#undef verify
#undef verifyf

#define checkCode(...)
#define checkNoEntry(...)
#define checkNoReentry(...)
#define checkNoRecursion(...)

#define check(expr)			{ if (!(expr)) { emscripten_log(255, "Expression '" #expr "' failed in " __FILE__ ":" PREPROCESSOR_TO_STRING(__LINE__) "!\n"); } }
#define checkf(expr, ...)	{ if (!(expr)) { emscripten_log(255, "Expression '" #expr "' failed in " __FILE__ ":" PREPROCESSOR_TO_STRING(__LINE__) "!\n"); emscripten_log(255, ##__VA_ARGS__); FDebug::AssertFailed( #expr, __FILE__, __LINE__, ##__VA_ARGS__ ); } CA_ASSUME(expr); }
#define verify(expr)		{ if(!(expr)) { emscripten_log(255, "Expression '" #expr "' failed in " __FILE__ ":" PREPROCESSOR_TO_STRING(__LINE__) "!\n"); } }
#define verifyf(expr, ...)	{ if(!(expr)) { emscripten_log(255, "Expression '" #expr "' failed in " __FILE__ ":" PREPROCESSOR_TO_STRING(__LINE__) "!\n"); emscripten_log(255, ##__VA_ARGS__); } }

#endif

#if DO_GUARD_SLOW && !PLATFORM_HTML5_WIN32

#undef checkSlow
#undef checkfSlow
#undef verifySlow

#define checkSlow(expr, ...)   {if(!(expr)) { emscripten_log(255, "Expression '" #expr "' failed in " __FILE__ ":" PREPROCESSOR_TO_STRING(__LINE__) "!\n"); emscripten_log(255, ##__VA_ARGS__); FDebug::AssertFailed( #expr, __FILE__, __LINE__ ); } CA_ASSUME(expr); }
#define checkfSlow(expr, ...)	{ if(!(expr)) { emscripten_log(255, "Expression '" #expr "' failed in " __FILE__ ":" PREPROCESSOR_TO_STRING(__LINE__) "!\n"); emscripten_log(255, ##__VA_ARGS__); FDebug::AssertFailed( #expr, __FILE__, __LINE__, __VA_ARGS__ ); } CA_ASSUME(expr); }
#define verifySlow(expr)  {if(!(expr)) { emscripten_log(255, "Expression '" #expr "' failed in " __FILE__ ":" PREPROCESSOR_TO_STRING(__LINE__) "!\n"); FDebug::AssertFailed( #expr, __FILE__, __LINE__ ); } }

#endif
