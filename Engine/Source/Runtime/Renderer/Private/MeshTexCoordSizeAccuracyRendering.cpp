// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
MeshTexCoordSizeAccuracyRendering.cpp: Contains definitions for rendering the viewmode.
=============================================================================*/

#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "MeshTexCoordSizeAccuracyRendering.h"

IMPLEMENT_SHADER_TYPE(,FMeshTexCoordSizeAccuracyPS,TEXT("MeshTexCoordSizeAccuracyPixelShader"),TEXT("Main"),SF_Pixel);

void FMeshTexCoordSizeAccuracyPS::SetParameters(
	FRHICommandList& RHICmdList, 
	const FShader* OriginalVS, 
	const FShader* OriginalPS, 
	const FMaterialRenderProxy* MaterialRenderProxy,
	const FMaterial& Material,
	const FSceneView& View
	)
{
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

	// Bind view params
	FGlobalShader::SetParameters(RHICmdList, FGlobalShader::GetPixelShader(), View);
}

void FMeshTexCoordSizeAccuracyPS::SetMesh(
	FRHICommandList& RHICmdList, 
	const FVertexFactory* VertexFactory,
	const FSceneView& View,
	const FPrimitiveSceneProxy* Proxy,
	int32 VisualizeLODIndex,
	const FMeshBatchElement& BatchElement, 
	const FMeshDrawingRenderState& DrawRenderState
	)
{
	float CPUTexelFactor = -1.f;
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	float ComponentExtraScale = 1.f, MeshExtraScale = 1.f;
	const FStreamingSectionBuildInfo* SectionData = Proxy ? Proxy->GetStreamingSectionData(ComponentExtraScale, MeshExtraScale, VisualizeLODIndex, BatchElement.VisualizeElementIndex) : nullptr;
	if (SectionData)
	{
		CPUTexelFactor = MeshExtraScale * SectionData->TexelFactors[0];
	}
#endif
	SetShaderValue(RHICmdList, FGlobalShader::GetPixelShader(), CPUTexelFactorParameter, CPUTexelFactor);
	SetShaderValue(RHICmdList, FGlobalShader::GetPixelShader(), PrimitiveAlphaParameter, (!Proxy || Proxy->IsSelected()) ? 1.f : .2f);
}

void FMeshTexCoordSizeAccuracyPS::SetMesh(FRHICommandList& RHICmdList, const FSceneView& View)
{
	SetShaderValue(RHICmdList, FGlobalShader::GetPixelShader(), CPUTexelFactorParameter, -1.f);
	SetShaderValue(RHICmdList, FGlobalShader::GetPixelShader(), PrimitiveAlphaParameter, 1.f);
}
