// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Windows/WindowsPlatformCompilerSetup.h"
#include "Windows/WindowsHWrapper.h"

// Macro for releasing COM objects
#define SAFE_RELEASE(p) { if(p) { (p)->Release(); (p)=NULL; } }

// Current instance
extern "C" CORE_API HINSTANCE hInstance;

// SIMD intrinsics
#include <intrin.h>

#include <stdint.h>
#include <tchar.h>

// @todo clang: When compiling under Clang for Windows or with VS2015, these headers cause us particular problems.  We need to make sure
// they're included before we pull in our 'DoNotUseOldUE4Type' namespace.  This is because these headers will redeclare various numeric
// typedefs, but under the Clang and 2015 compilers it is not allowed to define a typedef with a global scope operator in it (such as ::INT).
// So we'll get these headers included early on to avoid compiler errors with that.
#if defined(__clang__) || (defined(_MSC_VER) && (_MSC_VER >= 1900))
#include <intsafe.h>
#include <strsafe.h>
#endif

#if USING_CODE_ANALYSIS
// Source annotation support
#include <CodeAnalysis/SourceAnnotations.h>

// Allows for disabling code analysis warnings by defining ALL_CODE_ANALYSIS_WARNINGS
#include <CodeAnalysis/Warnings.h>

// We import the vc_attributes namespace so we can use annotations more easily.  It only defines
// MSVC-specific attributes so there should never be collisions.
using namespace vc_attributes;
#endif
