// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#include "UtilityShadersPrivatePCH.h"
#include "OneColorShader.h"
#include "ShaderParameterUtils.h"

BEGIN_UNIFORM_BUFFER_STRUCT(FClearShaderUB, )
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_ARRAY(FVector4, DrawColorMRT, [8] )
END_UNIFORM_BUFFER_STRUCT(FClearShaderUB)

IMPLEMENT_UNIFORM_BUFFER_STRUCT(FClearShaderUB, TEXT("ClearShaderUB"));

void FOneColorPS::SetColors(FRHICommandList& RHICmdList, const FLinearColor* Colors, int32 NumColors)
{
	check(NumColors <= MaxSimultaneousRenderTargets);

	const FShaderUniformBufferParameter& ClearUBParam = GetUniformBufferParameter<FClearShaderUB>();
	if (ClearUBParam.IsInitialized())
	{
		if (ClearUBParam.IsBound())
		{
			FClearShaderUB ClearData;
			FMemory::Memzero(ClearData.DrawColorMRT);			
			for (int32 i = 0; i < NumColors; ++i)
			{
				ClearData.DrawColorMRT[i].X = Colors[i].R;
				ClearData.DrawColorMRT[i].Y = Colors[i].G;
				ClearData.DrawColorMRT[i].Z = Colors[i].B;
				ClearData.DrawColorMRT[i].W = Colors[i].A;
			}

			FLocalUniformBuffer LocalUB = TUniformBufferRef<FClearShaderUB>::CreateLocalUniformBuffer(RHICmdList, ClearData, UniformBuffer_SingleFrame);	
			auto& Parameter = GetUniformBufferParameter<FClearShaderUB>();
			RHICmdList.SetLocalShaderUniformBuffer(GetPixelShader(), Parameter.GetBaseIndex(), LocalUB);
		}
	}

	
}

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
