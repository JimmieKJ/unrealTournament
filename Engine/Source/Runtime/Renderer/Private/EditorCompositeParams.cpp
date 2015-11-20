// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#include "RendererPrivate.h"
#include "EditorCompositeParams.h"

FEditorCompositingParameters::FEditorCompositingParameters()
{
}

void FEditorCompositingParameters::Bind(const FShaderParameterMap& ParameterMap)
{
	EditorCompositeDepthTestParameter.Bind(ParameterMap, TEXT("bEnableEditorPrimitiveDepthTest"));
	MSAASampleCount.Bind(ParameterMap, TEXT("MSAASampleCount"));
	FilteredSceneDepthTexture.Bind(ParameterMap, TEXT("FilteredSceneDepthTexture"));
	FilteredSceneDepthTextureSampler.Bind(ParameterMap, TEXT("FilteredSceneDepthTextureSampler"));
}

void FEditorCompositingParameters::SetParameters(
	FRHICommandList& RHICmdList,
	const FMaterial& MaterialResource,
	const FSceneView* View,
	bool bEnableEditorPrimitveDepthTest,
	const FPixelShaderRHIParamRef ShaderRHI)
{
#if WITH_EDITOR
		FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);
		if (GMaxRHIFeatureLevel >= ERHIFeatureLevel::SM4)
		{
			if (MaterialResource.IsUsedWithEditorCompositing())
			{
				// Compute parameters for converting from screen space to pixel 
				FIntRect DestRect = View->ViewRect;
				FIntPoint ViewportOffset = DestRect.Min;
				FIntPoint ViewportExtent = DestRect.Size();

				FVector4 ScreenPosToPixelValue(
					ViewportExtent.X * 0.5f,
					-ViewportExtent.Y * 0.5f,
					ViewportExtent.X * 0.5f - 0.5f + ViewportOffset.X,
					ViewportExtent.Y * 0.5f - 0.5f + ViewportOffset.Y);

				if (FilteredSceneDepthTexture.IsBound())
				{
					const FTexture2DRHIRef* DepthTexture = SceneContext.GetActualDepthTexture();
					check(DepthTexture != NULL);
					SetTextureParameter(
						RHICmdList,
						ShaderRHI,
						FilteredSceneDepthTexture,
						FilteredSceneDepthTextureSampler,
						TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI(),
						*DepthTexture
						);
				}

				SetShaderValue(RHICmdList, ShaderRHI, EditorCompositeDepthTestParameter, bEnableEditorPrimitveDepthTest);
				SetShaderValue(RHICmdList, ShaderRHI, MSAASampleCount, SceneContext.EditorPrimitivesColor ? SceneContext.EditorPrimitivesColor->GetDesc().NumSamples : 0);
			}
		}
#endif
}
