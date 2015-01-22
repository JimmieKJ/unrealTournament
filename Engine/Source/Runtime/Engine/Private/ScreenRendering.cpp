// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ScreenRendering.cpp: Screen rendering implementation.
=============================================================================*/

#include "EnginePrivate.h"
#include "ScreenRendering.h"
#include "RHIStaticStates.h"

/** Vertex declaration for screen-space rendering. */
TGlobalResource<FScreenVertexDeclaration> GScreenVertexDeclaration;

// Shader implementations.
IMPLEMENT_SHADER_TYPE(,FScreenPS,TEXT("ScreenPixelShader"),TEXT("Main"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(,FScreenVS,TEXT("ScreenVertexShader"),TEXT("Main"),SF_Vertex);
IMPLEMENT_SHADER_TYPE(,FScreenVSForGS,TEXT("ScreenVertexShader"),TEXT("MainForGS"),SF_Vertex);
