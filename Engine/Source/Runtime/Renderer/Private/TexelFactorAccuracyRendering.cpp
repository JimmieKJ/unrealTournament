// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
TexelFactorAccuracyRendering.cpp: Contains definitions for rendering the viewmode.
=============================================================================*/

#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "TexelFactorAccuracyRendering.h"

IMPLEMENT_SHADER_TYPE(,FTexelFactorAccuracyPS,TEXT("TexelFactorAccuracyPixelShader"),TEXT("Main"),SF_Pixel);

extern ENGINE_API TAutoConsoleVariable<int32> CVarStreamingUseNewMetrics;


void FTexelFactorAccuracyPS::SetParameters(
	FRHICommandList& RHICmdList, 
	const FShader* OriginalVS, 
	const FShader* OriginalPS, 
	const FMaterialRenderProxy* MaterialRenderProxy,
	const FMaterial& Material,
	const FSceneView& View
	)
{
	const auto& ViewUniformBufferParameter = GetUniformBufferParameter<FViewUniformShaderParameters>();
	SetUniformBufferParameter(RHICmdList, FGlobalShader::GetPixelShader(), ViewUniformBufferParameter, View.ViewUniformBuffer);

	const int32 NumEngineColors = FMath::Min<int32>(GEngine->StreamingAccuracyColors.Num(), NumStreamingAccuracyColors);
	int32 ColorIndex = 0;
	for (; ColorIndex < NumEngineColors; ++ColorIndex)
	{
		SetShaderValue(RHICmdList, FGlobalShader::GetPixelShader(), AccuracyColorsParameter, GEngine->StreamingAccuracyColors[ColorIndex], ColorIndex);
	}
	for (; ColorIndex < NumStreamingAccuracyColors; ++ColorIndex)
	{
		SetShaderValue(RHICmdList, FGlobalShader::GetPixelShader(), AccuracyColorsParameter, FLinearColor::Black, ColorIndex);
	}
}

void FTexelFactorAccuracyPS::SetMesh(
	FRHICommandList& RHICmdList, 
	const FVertexFactory* VertexFactory,
	const FSceneView& View,
	const FPrimitiveSceneProxy* Proxy,
	int32 VisualizeLODIndex,
	const FMeshBatchElement& BatchElement, 
	const FMeshDrawingRenderState& DrawRenderState
	)
{
	const bool bUseMetrics = CVarStreamingUseNewMetrics.GetValueOnRenderThread() != 0;

	float CPUTexelFactor = -1.f;
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	FStreamingTexturePrimitiveInfo Info; 
	if (Proxy && Proxy->GetStreamingTextureInfo(Info, bUseMetrics ? VisualizeLODIndex : INDEX_NONE, bUseMetrics ? BatchElement.VisualizeElementIndex : INDEX_NONE))
	{
		CPUTexelFactor = Info.TexelFactor;
	}
#endif
	SetShaderValue(RHICmdList, FGlobalShader::GetPixelShader(), CPUTexelFactorParameter, CPUTexelFactor);
}

void FTexelFactorAccuracyPS::SetMesh(FRHICommandList& RHICmdList, const FSceneView& View)
{
	SetShaderValue(RHICmdList, FGlobalShader::GetPixelShader(), CPUTexelFactorParameter, -1.f);
}
