// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
PrimitiveDistanceAccuracyRendering.cpp: Contains definitions for rendering the viewmode.
=============================================================================*/

#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "PrimitiveDistanceAccuracyRendering.h"

IMPLEMENT_SHADER_TYPE(,FPrimitiveDistanceAccuracyPS,TEXT("PrimitiveDistanceAccuracyPixelShader"),TEXT("Main"),SF_Pixel);

void FPrimitiveDistanceAccuracyPS::SetParameters(
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

void FPrimitiveDistanceAccuracyPS::SetMesh(
	FRHICommandList& RHICmdList, 
	const FVertexFactory* VertexFactory,
	const FSceneView& View,
	const FPrimitiveSceneProxy* Proxy,
	int32 VisualizeLODIndex,
	const FMeshBatchElement& BatchElement, 
	const FMeshDrawingRenderState& DrawRenderState
	)
{
	float CPULogDistance = -1.f;
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	const bool bUseNewMetrics = CVarStreamingUseNewMetrics.GetValueOnRenderThread() != 0;
	float ComponentExtraScale = 1.f, MeshExtraScale = 1.f;
	const FStreamingSectionBuildInfo* SectionData = Proxy ? Proxy->GetStreamingSectionData(ComponentExtraScale, MeshExtraScale, VisualizeLODIndex, BatchElement.VisualizeElementIndex) : nullptr;
	if (SectionData)
	{
		FVector ViewToObject = SectionData->BoxOrigin - View.ViewMatrices.ViewOrigin;

		float DistSqMinusRadiusSq = 0;
		if (bUseNewMetrics)
		{
			ViewToObject = ViewToObject.GetAbs();
			FVector BoxViewToObject = ViewToObject.ComponentMin(SectionData->BoxExtent);
			DistSqMinusRadiusSq = FVector::DistSquared(BoxViewToObject, ViewToObject);
		}
		else
		{
			float Distance = ViewToObject.Size();
			DistSqMinusRadiusSq = FMath::Square(Distance) - SectionData->BoxExtent.SizeSquared();
		}

		const float OneOverDistanceMultiplier = 1.f / FMath::Max<float>(SMALL_NUMBER, ComponentExtraScale);
		CPULogDistance =  FMath::Max<float>(0.f, FMath::Log2(OneOverDistanceMultiplier * FMath::Sqrt(FMath::Max<float>(1.f, DistSqMinusRadiusSq))));
	}
#endif
	// Because the streamer use FMath::FloorToFloat, here we need to use -1 to have a useful result.
	SetShaderValue(RHICmdList, FGlobalShader::GetPixelShader(), CPULogDistanceParameter, CPULogDistance);
	SetShaderValue(RHICmdList, FGlobalShader::GetPixelShader(), PrimitiveAlphaParameter, (!Proxy || Proxy->IsSelected()) ? 1.f : .2f);
}

void FPrimitiveDistanceAccuracyPS::SetMesh(FRHICommandList& RHICmdList, const FSceneView& View)
{
	SetShaderValue(RHICmdList, FGlobalShader::GetPixelShader(), CPULogDistanceParameter, -1.f);
	SetShaderValue(RHICmdList, FGlobalShader::GetPixelShader(), PrimitiveAlphaParameter, 1.f);
}
