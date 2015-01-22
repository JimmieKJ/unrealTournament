// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#include "EnginePrivate.h"
#include "OneColorShader.h"

IMPLEMENT_SHADER_TYPE(template<>,TOneColorVS<true>,TEXT("OneColorShader"),TEXT("MainVertexShader"),SF_Vertex);
IMPLEMENT_SHADER_TYPE(template<>,TOneColorVS<false>,TEXT("OneColorShader"),TEXT("MainVertexShader"),SF_Vertex);
IMPLEMENT_SHADER_TYPE(,FOneColorPS,TEXT("OneColorShader"),TEXT("MainPixelShader"),SF_Pixel);
// Compiling a version for every number of MRT's
// On AMD PC hardware, outputting to a color index in the shader without a matching render target set has a significant performance hit
IMPLEMENT_SHADER_TYPE(template<>,TOneColorPixelShaderMRT<1>,TEXT("OneColorShader"),TEXT("MainPixelShaderMRT"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>,TOneColorPixelShaderMRT<2>,TEXT("OneColorShader"),TEXT("MainPixelShaderMRT"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>,TOneColorPixelShaderMRT<3>,TEXT("OneColorShader"),TEXT("MainPixelShaderMRT"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>,TOneColorPixelShaderMRT<4>,TEXT("OneColorShader"),TEXT("MainPixelShaderMRT"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>,TOneColorPixelShaderMRT<5>,TEXT("OneColorShader"),TEXT("MainPixelShaderMRT"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>,TOneColorPixelShaderMRT<6>,TEXT("OneColorShader"),TEXT("MainPixelShaderMRT"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>,TOneColorPixelShaderMRT<7>,TEXT("OneColorShader"),TEXT("MainPixelShaderMRT"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>,TOneColorPixelShaderMRT<8>,TEXT("OneColorShader"),TEXT("MainPixelShaderMRT"),SF_Pixel);

IMPLEMENT_SHADER_TYPE(,FFillTextureCS,TEXT("OneColorShader"),TEXT("MainFillTextureCS"),SF_Compute);
