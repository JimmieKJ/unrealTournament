// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
MaterialTexCoordScalesRendering.cpp: Contains definitions for rendering the viewmode.
=============================================================================*/

#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "MaterialTexCoordScalesRendering.h"

static TAutoConsoleVariable<int32> CVarStreamingAnalysisIndex(
	TEXT("r.Streaming.AnalysisIndex"),
	FMaterialTexCoordScalePS::MAX_NUM_TEXTURE_REGISTER,
	TEXT("Analysis index of the texture coordinate scale accuracy viewmode."),
	ECVF_Default);

IMPLEMENT_MATERIAL_SHADER_TYPE(,FMaterialTexCoordScalePS,TEXT("MaterialTexCoordScalesPixelShader"),TEXT("Main"),SF_Pixel);

void FMaterialTexCoordScalePS::SetParameters(
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
		SetShaderValue(RHICmdList, FMeshMaterialShader::GetPixelShader(), AccuracyColorsParameter, GEngine->StreamingAccuracyColors[ColorIndex], ColorIndex);
	}
	for (; ColorIndex < NumStreamingAccuracyColors; ++ColorIndex)
	{
		SetShaderValue(RHICmdList, FMeshMaterialShader::GetPixelShader(), AccuracyColorsParameter, FLinearColor::Black, ColorIndex);
	}

	FMeshMaterialShader::SetParameters(RHICmdList, FMeshMaterialShader::GetPixelShader(), MaterialRenderProxy, Material, View, View.ViewUniformBuffer, ESceneRenderTargetsMode::SetTextures);
}

void FMaterialTexCoordScalePS::SetMesh(
	FRHICommandList& RHICmdList, 
	const FVertexFactory* VertexFactory,
	const FSceneView& View,
	const FPrimitiveSceneProxy* Proxy,
	int32 VisualizeLODIndex,
	const FMeshBatchElement& BatchElement, 
	const FMeshDrawingRenderState& DrawRenderState
	)
{
	int32 AnalysisIndex = CVarStreamingAnalysisIndex.GetValueOnRenderThread();
	if (AnalysisIndex >= 0 && AnalysisIndex < MAX_NUM_TEXTURE_REGISTER)
	{
		AnalysisIndex = FMath::Clamp<int32>(AnalysisIndex, 0, MAX_NUM_TEXTURE_REGISTER - 1);;
	}
	else
	{
		AnalysisIndex = MAX_NUM_TEXTURE_REGISTER;
	}

	FVector4 OneOverCPUTexCoordScales[MAX_NUM_TEXTURE_REGISTER / 4];
	FMemory::Memzero(OneOverCPUTexCoordScales); // 0 remap to irrelevant data.

	FIntVector4 TexCoordIndices[MAX_NUM_TEXTURE_REGISTER / 4];
	FMemory::Memzero(TexCoordIndices);

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	float ComponentExtraScale = 1.f, MeshExtraScale = 1.f;
	const FStreamingSectionBuildInfo* SectionData = Proxy ? Proxy->GetStreamingSectionData(ComponentExtraScale, MeshExtraScale, VisualizeLODIndex, BatchElement.VisualizeElementIndex) : nullptr;
	if (SectionData)
	{
		for (int32 ScaleIndex = 0; ScaleIndex < MAX_NUM_TEXTURE_REGISTER && ScaleIndex < SectionData->TexCoordData.Num(); ++ScaleIndex)
		{
			float Scale = SectionData->TexCoordData[ScaleIndex].Scale;
			if (Scale > 0)
			{
				OneOverCPUTexCoordScales[ScaleIndex / 4][ScaleIndex % 4] = 1.f / Scale;
				TexCoordIndices[ScaleIndex / 4][ScaleIndex % 4] = SectionData->TexCoordData[ScaleIndex].Index;
			}
		}
	}
#endif

	const bool bOutputScales = View.Family->GetDebugViewShaderMode() == DVSM_MaterialTexCoordScalesAnalysis;

	SetShaderValueArray(RHICmdList, FMeshMaterialShader::GetPixelShader(), OneOverCPUTexCoordScalesParameter, OneOverCPUTexCoordScales, ARRAY_COUNT(OneOverCPUTexCoordScales));
	SetShaderValueArray(RHICmdList, FMeshMaterialShader::GetPixelShader(), TexCoordIndicesParameter, TexCoordIndices, ARRAY_COUNT(TexCoordIndices));
	SetShaderValue(RHICmdList, FMeshMaterialShader::GetPixelShader(), TextureAnalysisIndexParameter, bOutputScales ? (int32)INDEX_NONE : AnalysisIndex);
	SetShaderValue(RHICmdList, FMeshMaterialShader::GetPixelShader(), PrimitiveAlphaParameter, (!Proxy || Proxy->IsSelected()) ? 1.f : .2f);

	FMeshMaterialShader::SetMesh(RHICmdList, FMeshMaterialShader::GetPixelShader(), VertexFactory, View, Proxy, BatchElement, DrawRenderState);
}
