// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

// Pre-system API include
#include "WinRT/PreWinRTApi.h"

#ifndef STRICT
#define STRICT
#endif

// System API include
#include "WinRT/MinWinRTApi.h"

// Post-system API include
#include "WinRT/PostWinRTApi.h"

// Set up compiler pragmas, etc
#include "WinRT/WinRTCompilerSetup.h"

// Macro for releasing COM objects
#define SAFE_RELEASE(p) { if(p) { (p)->Release(); (p)=NULL; } }

#include <tchar.h>

// SIMD intrinsics
#include <intrin.h>