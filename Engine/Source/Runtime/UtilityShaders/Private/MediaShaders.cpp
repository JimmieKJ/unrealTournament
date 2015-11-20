// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UtilityShadersPrivatePCH.h"
#include "MediaShaders.h"
#include "ShaderParameterUtils.h"

BEGIN_UNIFORM_BUFFER_STRUCT(FYCbCrConvertUB, )
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_TEXTURE(Texture2D, LumaTexture)
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_TEXTURE(Texture2D, CbCrTexture)
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_SAMPLER(SamplerState, LumaSampler)
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_SAMPLER(SamplerState, CbCrSampler)
END_UNIFORM_BUFFER_STRUCT(FYCbCrConvertUB)

IMPLEMENT_UNIFORM_BUFFER_STRUCT(FYCbCrConvertUB, TEXT("YCbCrConvertUB"));

IMPLEMENT_SHADER_TYPE(, FMedaShadersVS, TEXT("MediaShaders"), TEXT("MainVertexShader"), SF_Vertex);

IMPLEMENT_SHADER_TYPE(, FYCbCrConvertShaderPS, TEXT("MediaShaders"), TEXT("YCbCrConvertPS"), SF_Pixel);

void FYCbCrConvertShaderPS::SetParameters(FRHICommandList& RHICmdList, FTextureRHIRef LumaTexture, FTextureRHIRef CbCrTexture)
{
	FYCbCrConvertUB YCbCrUB;
	YCbCrUB.LumaTexture = LumaTexture;
	YCbCrUB.CbCrTexture = CbCrTexture;
	YCbCrUB.LumaSampler = TStaticSamplerState<SF_Bilinear>::GetRHI();
	YCbCrUB.CbCrSampler = TStaticSamplerState<SF_Bilinear>::GetRHI();

	TUniformBufferRef<FYCbCrConvertUB> Data = TUniformBufferRef<FYCbCrConvertUB>::CreateUniformBufferImmediate(YCbCrUB, UniformBuffer_SingleFrame);
	SetUniformBufferParameter(RHICmdList, GetPixelShader(), GetUniformBufferParameter<FYCbCrConvertUB>(), Data);	
}
