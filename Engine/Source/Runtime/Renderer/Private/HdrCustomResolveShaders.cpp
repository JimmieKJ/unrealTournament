// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "RendererPrivate.h"
#include "HdrCustomResolveShaders.h"
#include "ShaderParameterUtils.h"

IMPLEMENT_SHADER_TYPE(,FHdrCustomResolveVS,TEXT("HdrCustomResolveShaders"),TEXT("HdrCustomResolveVS"),SF_Vertex);
IMPLEMENT_SHADER_TYPE(,FHdrCustomResolve2xPS,TEXT("HdrCustomResolveShaders"),TEXT("HdrCustomResolve2xPS"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(,FHdrCustomResolve4xPS,TEXT("HdrCustomResolveShaders"),TEXT("HdrCustomResolve4xPS"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(,FHdrCustomResolve8xPS,TEXT("HdrCustomResolveShaders"),TEXT("HdrCustomResolve8xPS"),SF_Pixel);

