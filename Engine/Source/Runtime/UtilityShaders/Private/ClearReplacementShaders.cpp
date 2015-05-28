// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#include "UtilityShadersPrivatePCH.h"
#include "ClearReplacementShaders.h"
#include "ShaderParameterUtils.h"

IMPLEMENT_SHADER_TYPE(,FClearReplacementVS,TEXT("ClearReplacementShaders"),TEXT("ClearVS"),SF_Vertex);
IMPLEMENT_SHADER_TYPE(,FClearReplacementPS,TEXT("ClearReplacementShaders"),TEXT("ClearPS"),SF_Pixel);

IMPLEMENT_SHADER_TYPE(,FClearTexture2DReplacementCS,TEXT("ClearReplacementShaders"),TEXT("ClearTexture2DCS"),SF_Compute);
IMPLEMENT_SHADER_TYPE(,FClearTexture2DReplacementScissorCS,TEXT("ClearReplacementShaders"),TEXT("ClearTexture2DScissorCS"), SF_Compute);

IMPLEMENT_SHADER_TYPE(,FClearBufferReplacementCS,TEXT("ClearReplacementShaders"),TEXT("ClearBufferCS"),SF_Compute);

void FClearTexture2DReplacementCS::SetParameters( FRHICommandList& RHICmdList, FUnorderedAccessViewRHIParamRef TextureRW, FLinearColor Value )
{
	FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();
	SetShaderValue(RHICmdList, ComputeShaderRHI, ClearColor, Value);
	RHICmdList.SetUAVParameter(ComputeShaderRHI, ClearTextureRW.GetBaseIndex(), TextureRW);
}

void FClearTexture2DReplacementScissorCS::SetParameters(FRHICommandList& RHICmdList, FUnorderedAccessViewRHIParamRef TextureRW, FLinearColor InClearColor, const FVector4& InTargetBounds)
{
	FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();
	SetShaderValue(RHICmdList, ComputeShaderRHI, ClearColor, InClearColor);
	SetShaderValue(RHICmdList, ComputeShaderRHI, TargetBounds, InTargetBounds);
	RHICmdList.SetUAVParameter(ComputeShaderRHI, ClearTextureRW.GetBaseIndex(), TextureRW);
}

void FClearBufferReplacementCS::SetParameters( FRHICommandList& RHICmdList, FUnorderedAccessViewRHIParamRef BufferRW, uint32 Dword )
{
	FComputeShaderRHIParamRef ComputeShaderRHI = GetComputeShader();
	SetShaderValue(RHICmdList, ComputeShaderRHI, ClearDword, Dword);
	RHICmdList.SetUAVParameter(ComputeShaderRHI, ClearBufferRW.GetBaseIndex(), BufferRW);
}
