// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
LightMapRendering.cpp: Light map rendering implementations.
=============================================================================*/

#include "RendererPrivate.h"
#include "SceneManagement.h"

void FMovableDirectionalLightCSMLightingPolicy::ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
{
	OutEnvironment.SetDefine(TEXT("MOVABLE_DIRECTIONAL_LIGHT"), TEXT("1"));
	OutEnvironment.SetDefine(TEXT("MOVABLE_DIRECTIONAL_LIGHT_CSM"), TEXT("1"));
	OutEnvironment.SetDefine(TEXT(PREPROCESSOR_TO_STRING(MAX_FORWARD_SHADOWCASCADES)), MAX_FORWARD_SHADOWCASCADES);

	FNoLightMapPolicy::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
}