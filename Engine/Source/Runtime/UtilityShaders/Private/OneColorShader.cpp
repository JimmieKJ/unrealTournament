// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#include "UtilityShadersPrivatePCH.h"
#include "OneColorShader.h"

IMPLEMENT_SHADER_TYPE(template<> UTILITYSHADERS_API, TOneColorVS<true>,TEXT("OneColorShader"),TEXT("MainVertexShader"),SF_Vertex);
IMPLEMENT_SHADER_TYPE(template<> UTILITYSHADERS_API, TOneColorVS<false>,TEXT("OneColorShader"),TEXT("MainVertexShader"),SF_Vertex);
IMPLEMENT_SHADER_TYPE(,FOneColorPS,TEXT("OneColorShader"),TEXT("MainPixelShader"),SF_Pixel);
// Compiling a version for every number of MRT's
// On AMD PC hardware, outputting to a color index in the shader without a matching render target set has a significant performance hit
IMPLEMENT_SHADER_TYPE(template<> UTILITYSHADERS_API,TOneColorPixelShaderMRT<1>,TEXT("OneColorShader"),TEXT("MainPixelShaderMRT"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<> UTILITYSHADERS_API,TOneColorPixelShaderMRT<2>,TEXT("OneColorShader"),TEXT("MainPixelShaderMRT"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<> UTILITYSHADERS_API,TOneColorPixelShaderMRT<3>,TEXT("OneColorShader"),TEXT("MainPixelShaderMRT"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<> UTILITYSHADERS_API,TOneColorPixelShaderMRT<4>,TEXT("OneColorShader"),TEXT("MainPixelShaderMRT"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<> UTILITYSHADERS_API,TOneColorPixelShaderMRT<5>,TEXT("OneColorShader"),TEXT("MainPixelShaderMRT"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<> UTILITYSHADERS_API,TOneColorPixelShaderMRT<6>,TEXT("OneColorShader"),TEXT("MainPixelShaderMRT"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<> UTILITYSHADERS_API,TOneColorPixelShaderMRT<7>,TEXT("OneColorShader"),TEXT("MainPixelShaderMRT"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<> UTILITYSHADERS_API,TOneColorPixelShaderMRT<8>,TEXT("OneColorShader"),TEXT("MainPixelShaderMRT"),SF_Pixel);

IMPLEMENT_SHADER_TYPE(,FFillTextureCS,TEXT("OneColorShader"),TEXT("MainFillTextureCS"),SF_Compute);

IMPLEMENT_SHADER_TYPE(,FLongGPUTaskPS,TEXT("OneColorShader"),TEXT("MainLongGPUTask"),SF_Pixel);
