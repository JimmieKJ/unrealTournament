// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	Reflection Environment common declarations
=============================================================================*/

#pragma once

/** 
 * Number of reflection captures to allocate uniform buffer space for. 
 * This is currently limited by the array texture max size of 2048 for d3d11 (each cubemap is 6 slices).
 * Must touch the reflection shaders to propagate changes.
 */
static const int32 GMaxNumReflectionCaptures = 341;

extern bool IsReflectionEnvironmentAvailable(ERHIFeatureLevel::Type InFeatureLevel);
extern bool IsReflectionCaptureAvailable();
