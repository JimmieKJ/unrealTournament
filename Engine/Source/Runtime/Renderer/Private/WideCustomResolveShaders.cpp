
// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "RendererPrivate.h"
#include "WideCustomResolveShaders.h"
#include "ShaderParameterUtils.h"

IMPLEMENT_SHADER_TYPE(, FWideCustomResolveVS, TEXT("WideCustomResolveShaders"), TEXT("WideCustomResolveVS"), SF_Vertex);

#define IMPLEMENT_RESOLVE_SHADER(Width, MSAA) \
	typedef FWideCustomResolvePS<Width,MSAA> FWideCustomResolve##Width##_##MSAA##xPS; \
	IMPLEMENT_SHADER_TYPE(template<>, FWideCustomResolve##Width##_##MSAA##xPS, TEXT("WideCustomResolveShaders"), TEXT("WideCustomResolvePS"), SF_Pixel)

IMPLEMENT_RESOLVE_SHADER(0, 1);
IMPLEMENT_RESOLVE_SHADER(2, 0);
IMPLEMENT_RESOLVE_SHADER(2, 1);
IMPLEMENT_RESOLVE_SHADER(2, 2);
IMPLEMENT_RESOLVE_SHADER(2, 3);
IMPLEMENT_RESOLVE_SHADER(4, 0);
IMPLEMENT_RESOLVE_SHADER(4, 1);
IMPLEMENT_RESOLVE_SHADER(4, 2);
IMPLEMENT_RESOLVE_SHADER(4, 3);


template <unsigned Width, unsigned MSAA>
static void ResolveColorWideInternal2(
	FRHICommandList& RHICmdList,
	const ERHIFeatureLevel::Type CurrentFeatureLevel,
	const FTextureRHIRef& SrcTexture,
	const FIntPoint& SrcOrigin)
{
	auto ShaderMap = GetGlobalShaderMap(CurrentFeatureLevel);

	TShaderMapRef<FWideCustomResolveVS> VertexShader(ShaderMap);
	TShaderMapRef<FWideCustomResolvePS<MSAA, Width>> PixelShader(ShaderMap);
	static FGlobalBoundShaderState BoundShaderState;
	SetGlobalBoundShaderState(RHICmdList, CurrentFeatureLevel, BoundShaderState, GetVertexDeclarationFVector4(), *VertexShader, *PixelShader);
	PixelShader->SetParameters(RHICmdList, SrcTexture, SrcOrigin);

	RHICmdList.DrawPrimitive(PT_TriangleList, 0, 1, 1);
}

template <unsigned MSAA>
static void ResolveColorWideInternal(
	FRHICommandList& RHICmdList, 
	const ERHIFeatureLevel::Type CurrentFeatureLevel,
	const FTextureRHIRef& SrcTexture,
	const FIntPoint& SrcOrigin, 
	int32 WideFilterWidth)
{
	switch (WideFilterWidth)
	{
	case 0: ResolveColorWideInternal2<0, MSAA>(RHICmdList, CurrentFeatureLevel, SrcTexture, SrcOrigin); break;
	case 1: ResolveColorWideInternal2<1, MSAA>(RHICmdList, CurrentFeatureLevel, SrcTexture, SrcOrigin); break;
	case 2: ResolveColorWideInternal2<2, MSAA>(RHICmdList, CurrentFeatureLevel, SrcTexture, SrcOrigin); break;
	case 3: ResolveColorWideInternal2<3, MSAA>(RHICmdList, CurrentFeatureLevel, SrcTexture, SrcOrigin); break;
	}
}

void ResolveFilterWide(
	FRHICommandList& RHICmdList, 
	const ERHIFeatureLevel::Type CurrentFeatureLevel,
	const FTextureRHIRef& SrcTexture,
	const FIntPoint& SrcOrigin, 
	int32 NumSamples,
	int32 WideFilterWidth)
{				
	if (NumSamples <= 1)
	{
		ResolveColorWideInternal2<1, 0>(RHICmdList, CurrentFeatureLevel, SrcTexture, SrcOrigin);
	}
	else if (NumSamples == 2)
	{
		ResolveColorWideInternal<2>(RHICmdList, CurrentFeatureLevel, SrcTexture, SrcOrigin, WideFilterWidth);
	}
	else if (NumSamples == 4)
	{
		ResolveColorWideInternal<4>(RHICmdList, CurrentFeatureLevel, SrcTexture, SrcOrigin, WideFilterWidth);
	}
	else
	{
		// Need to implement more.
		check(0);
	}
}