// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UtilityShadersPrivatePCH.h"
#include "MediaShaders.h"
#include "ShaderParameterUtils.h"


TGlobalResource<FMediaVertexDeclaration> GMediaVertexDeclaration;


/* FMediaShadersVS shader
 *****************************************************************************/

IMPLEMENT_SHADER_TYPE(, FMediaShadersVS, TEXT("MediaShaders"), TEXT("MainVertexShader"), SF_Vertex);


/* FAYUVConvertPS shader
 *****************************************************************************/

BEGIN_UNIFORM_BUFFER_STRUCT(FAYUVConvertUB, )
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_TEXTURE(Texture2D, AYUVTexture)
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_SAMPLER(SamplerState, AYUVSampler)
END_UNIFORM_BUFFER_STRUCT(FAYUVConvertUB)

IMPLEMENT_UNIFORM_BUFFER_STRUCT(FAYUVConvertUB, TEXT("AYUVConvertUB"));
IMPLEMENT_SHADER_TYPE(, FAYUVConvertPS, TEXT("MediaShaders"), TEXT("AYUVConvertPS"), SF_Pixel);


void FAYUVConvertPS::SetParameters(FRHICommandList& RHICmdList, TRefCountPtr<FRHITexture2D> AYUVTexture)
{
	FAYUVConvertUB UB;
	{
		UB.AYUVTexture = AYUVTexture;
		UB.AYUVSampler = TStaticSamplerState<SF_Bilinear>::GetRHI();
	}

	TUniformBufferRef<FAYUVConvertUB> Data = TUniformBufferRef<FAYUVConvertUB>::CreateUniformBufferImmediate(UB, UniformBuffer_SingleFrame);
	SetUniformBufferParameter(RHICmdList, GetPixelShader(), GetUniformBufferParameter<FAYUVConvertUB>(), Data);
}


/* FNV12ConvertPS shader
 *****************************************************************************/

BEGIN_UNIFORM_BUFFER_STRUCT(FNV12ConvertUB, )
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(uint32, Width)
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_TEXTURE(Texture2D, NV12Texture)
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_SAMPLER(SamplerState, NV12Sampler)
END_UNIFORM_BUFFER_STRUCT(FNV12ConvertUB)

IMPLEMENT_UNIFORM_BUFFER_STRUCT(FNV12ConvertUB, TEXT("NV12ConvertUB"));
IMPLEMENT_SHADER_TYPE(, FNV12ConvertPS, TEXT("MediaShaders"), TEXT("NV12ConvertPS"), SF_Pixel);


void FNV12ConvertPS::SetParameters(FRHICommandList& RHICmdList, TRefCountPtr<FRHITexture2D> NV12Texture)
{
	FNV12ConvertUB UB;
	{
		UB.Width = NV12Texture->GetSizeX();
		UB.NV12Texture = NV12Texture;
		UB.NV12Sampler = TStaticSamplerState<SF_Bilinear>::GetRHI();
	}

	TUniformBufferRef<FNV12ConvertUB> Data = TUniformBufferRef<FNV12ConvertUB>::CreateUniformBufferImmediate(UB, UniformBuffer_SingleFrame);
	SetUniformBufferParameter(RHICmdList, GetPixelShader(), GetUniformBufferParameter<FNV12ConvertUB>(), Data);
}


/* FNV21ConvertPS shader
 *****************************************************************************/

BEGIN_UNIFORM_BUFFER_STRUCT(FNV21ConvertUB, )
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(uint32, Width)
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_TEXTURE(Texture2D, NV21Texture)
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_SAMPLER(SamplerState, NV21Sampler)
END_UNIFORM_BUFFER_STRUCT(FNV21ConvertUB)

IMPLEMENT_UNIFORM_BUFFER_STRUCT(FNV21ConvertUB, TEXT("NV21ConvertUB"));
IMPLEMENT_SHADER_TYPE(, FNV21ConvertPS, TEXT("MediaShaders"), TEXT("NV21ConvertPS"), SF_Pixel);


void FNV21ConvertPS::SetParameters(FRHICommandList& RHICmdList, TRefCountPtr<FRHITexture2D> NV21Texture)
{
	FNV21ConvertUB UB;
	{
		UB.Width = NV21Texture->GetSizeX();
		UB.NV21Texture = NV21Texture;
		UB.NV21Sampler = TStaticSamplerState<SF_Bilinear>::GetRHI();
	}

	TUniformBufferRef<FNV21ConvertUB> Data = TUniformBufferRef<FNV21ConvertUB>::CreateUniformBufferImmediate(UB, UniformBuffer_SingleFrame);
	SetUniformBufferParameter(RHICmdList, GetPixelShader(), GetUniformBufferParameter<FNV21ConvertUB>(), Data);
}


/* FYCbCrConvertUB shader
 *****************************************************************************/

BEGIN_UNIFORM_BUFFER_STRUCT(FYCbCrConvertUB, )
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_TEXTURE(Texture2D, LumaTexture)
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_TEXTURE(Texture2D, CbCrTexture)
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_SAMPLER(SamplerState, LumaSampler)
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_SAMPLER(SamplerState, CbCrSampler)
END_UNIFORM_BUFFER_STRUCT(FYCbCrConvertUB)

IMPLEMENT_UNIFORM_BUFFER_STRUCT(FYCbCrConvertUB, TEXT("YCbCrConvertUB"));
IMPLEMENT_SHADER_TYPE(, FYCbCrConvertPS, TEXT("MediaShaders"), TEXT("YCbCrConvertPS"), SF_Pixel);


void FYCbCrConvertPS::SetParameters(FRHICommandList& RHICmdList, TRefCountPtr<FRHITexture2D> LumaTexture, TRefCountPtr<FRHITexture2D> CbCrTexture)
{
	FYCbCrConvertUB UB;
	{
		UB.LumaTexture = LumaTexture;
		UB.CbCrTexture = CbCrTexture;
		UB.LumaSampler = TStaticSamplerState<SF_Bilinear>::GetRHI();
		UB.CbCrSampler = TStaticSamplerState<SF_Bilinear>::GetRHI();
	}

	TUniformBufferRef<FYCbCrConvertUB> Data = TUniformBufferRef<FYCbCrConvertUB>::CreateUniformBufferImmediate(UB, UniformBuffer_SingleFrame);
	SetUniformBufferParameter(RHICmdList, GetPixelShader(), GetUniformBufferParameter<FYCbCrConvertUB>(), Data);	
}


/* FUYVYConvertPS shader
 *****************************************************************************/

BEGIN_UNIFORM_BUFFER_STRUCT(FUYVYConvertUB, )
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(uint32, Width)
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_TEXTURE(Texture2D, UYVYTexture)
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_SAMPLER(SamplerState, UYVYSampler)
END_UNIFORM_BUFFER_STRUCT(FUYVYConvertUB)

IMPLEMENT_UNIFORM_BUFFER_STRUCT(FUYVYConvertUB, TEXT("UYVYConvertUB"));
IMPLEMENT_SHADER_TYPE(, FUYVYConvertPS, TEXT("MediaShaders"), TEXT("UYVYConvertPS"), SF_Pixel);


void FUYVYConvertPS::SetParameters(FRHICommandList& RHICmdList, TRefCountPtr<FRHITexture2D> UYVYTexture)
{
	FUYVYConvertUB UB;
	{
		UB.Width = UYVYTexture->GetSizeX();
		UB.UYVYTexture = UYVYTexture;
		UB.UYVYSampler = TStaticSamplerState<SF_Bilinear>::GetRHI();
	}

	TUniformBufferRef<FUYVYConvertUB> Data = TUniformBufferRef<FUYVYConvertUB>::CreateUniformBufferImmediate(UB, UniformBuffer_SingleFrame);
	SetUniformBufferParameter(RHICmdList, GetPixelShader(), GetUniformBufferParameter<FUYVYConvertUB>(), Data);
}


/* FYUVConvertPS shader
 *****************************************************************************/

BEGIN_UNIFORM_BUFFER_STRUCT(FYUVConvertUB, )
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_TEXTURE(Texture2D, YTexture)
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_TEXTURE(Texture2D, UTexture)
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_TEXTURE(Texture2D, VTexture)
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_SAMPLER(SamplerState, YSampler)
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_SAMPLER(SamplerState, USampler)
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_SAMPLER(SamplerState, VSampler)
END_UNIFORM_BUFFER_STRUCT(FYUVConvertUB)

IMPLEMENT_UNIFORM_BUFFER_STRUCT(FYUVConvertUB, TEXT("YUVConvertUB"));
IMPLEMENT_SHADER_TYPE(, FYUVConvertPS, TEXT("MediaShaders"), TEXT("YUVConvertPS"), SF_Pixel);


void FYUVConvertPS::SetParameters(FRHICommandList& RHICmdList, TRefCountPtr<FRHITexture2D> YTexture, TRefCountPtr<FRHITexture2D> UTexture, TRefCountPtr<FRHITexture2D> VTexture)
{
	FYUVConvertUB UB;
	{
		UB.YTexture = YTexture;
		UB.UTexture = UTexture;
		UB.VTexture = VTexture;
		UB.YSampler = TStaticSamplerState<SF_Bilinear>::GetRHI();
		UB.USampler = TStaticSamplerState<SF_Bilinear>::GetRHI();
		UB.VSampler = TStaticSamplerState<SF_Bilinear>::GetRHI();
	}

	TUniformBufferRef<FYUVConvertUB> Data = TUniformBufferRef<FYUVConvertUB>::CreateUniformBufferImmediate(UB, UniformBuffer_SingleFrame);
	SetUniformBufferParameter(RHICmdList, GetPixelShader(), GetUniformBufferParameter<FYUVConvertUB>(), Data);
}


/* FYUY2ConvertPS shader
 *****************************************************************************/

BEGIN_UNIFORM_BUFFER_STRUCT(FYUY2ConvertUB, )
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(uint32, Width)
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_TEXTURE(Texture2D, YUY2Texture)
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_SAMPLER(SamplerState, YUY2Sampler)
END_UNIFORM_BUFFER_STRUCT(FYUY2ConvertUB)

IMPLEMENT_UNIFORM_BUFFER_STRUCT(FYUY2ConvertUB, TEXT("YUY2ConvertUB"));
IMPLEMENT_SHADER_TYPE(, FYUY2ConvertPS, TEXT("MediaShaders"), TEXT("YUY2ConvertPS"), SF_Pixel);


void FYUY2ConvertPS::SetParameters(FRHICommandList& RHICmdList, TRefCountPtr<FRHITexture2D> YUY2Texture)
{
	FYUY2ConvertUB UB;
	{
		UB.Width = YUY2Texture->GetSizeX();
		UB.YUY2Texture = YUY2Texture;
		UB.YUY2Sampler = TStaticSamplerState<SF_Bilinear>::GetRHI();
	}

	TUniformBufferRef<FYUY2ConvertUB> Data = TUniformBufferRef<FYUY2ConvertUB>::CreateUniformBufferImmediate(UB, UniformBuffer_SingleFrame);
	SetUniformBufferParameter(RHICmdList, GetPixelShader(), GetUniformBufferParameter<FYUY2ConvertUB>(), Data);
}


/* FUYVYConvertPS shader
 *****************************************************************************/

BEGIN_UNIFORM_BUFFER_STRUCT(FYVYUConvertUB, )
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(uint32, Width)
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_TEXTURE(Texture2D, YVYUTexture)
DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_SAMPLER(SamplerState, YVYUSampler)
END_UNIFORM_BUFFER_STRUCT(FYVYUConvertUB)

IMPLEMENT_UNIFORM_BUFFER_STRUCT(FYVYUConvertUB, TEXT("YVYUConvertUB"));
IMPLEMENT_SHADER_TYPE(, FYVYUConvertPS, TEXT("MediaShaders"), TEXT("YVYUConvertPS"), SF_Pixel);


void FYVYUConvertPS::SetParameters(FRHICommandList& RHICmdList, TRefCountPtr<FRHITexture2D> YVYUTexture)
{
	FYVYUConvertUB UB;
	{
		UB.Width = YVYUTexture->GetSizeX();
		UB.YVYUTexture = YVYUTexture;
		UB.YVYUSampler = TStaticSamplerState<SF_Bilinear>::GetRHI();
	}

	TUniformBufferRef<FYVYUConvertUB> Data = TUniformBufferRef<FYVYUConvertUB>::CreateUniformBufferImmediate(UB, UniformBuffer_SingleFrame);
	SetUniformBufferParameter(RHICmdList, GetPixelShader(), GetUniformBufferParameter<FYVYUConvertUB>(), Data);
}
