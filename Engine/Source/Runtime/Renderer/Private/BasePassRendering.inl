// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	BasePassRendering.inl: Base pass rendering implementations.
		(Due to forward declaration issues)
=============================================================================*/

#pragma once

template<typename LightMapPolicyType>
inline void TBasePassVertexShaderBaseType<LightMapPolicyType>::SetMesh(FRHICommandList& RHICmdList, const FVertexFactory* VertexFactory,const FSceneView& View,const FPrimitiveSceneProxy* Proxy, const FMeshBatch& Mesh, const FMeshBatchElement& BatchElement)
{
	FVertexShaderRHIParamRef VertexShaderRHI = GetVertexShader();
	FMeshMaterialShader::SetMesh(RHICmdList, VertexShaderRHI, VertexFactory, View, Proxy, BatchElement);

	const bool bHasPreviousLocalToWorldParameter = PreviousLocalToWorldParameter.IsBound();
	const bool bHasSkipOutputVelocityParameter = SkipOutputVelocityParameter.IsBound();
	if ((bHasSkipOutputVelocityParameter || bHasPreviousLocalToWorldParameter) && Proxy)
	{
//		check(SkipOutputVelocityParameter.IsBound());

		FMatrix PreviousLocalToWorldMatrix;
		bool bHasPreviousLocalToWorldMatrix = false;
		const auto& ViewInfo = (const FViewInfo&)View;
		float SkipOutputVelocityValue;
		if (FVelocityDrawingPolicy::HasVelocityOnBasePass(ViewInfo, Proxy, Proxy->GetPrimitiveSceneInfo(), Mesh,
			bHasPreviousLocalToWorldMatrix, PreviousLocalToWorldMatrix))
		{
			if (bHasPreviousLocalToWorldParameter)
			{
				const FMatrix& Transform = bHasPreviousLocalToWorldMatrix ? PreviousLocalToWorldMatrix : Proxy->GetLocalToWorld();
				SetShaderValue(RHICmdList, VertexShaderRHI, PreviousLocalToWorldParameter, Transform.ConcatTranslation(ViewInfo.PrevViewMatrices.PreViewTranslation));
			}

			SkipOutputVelocityValue = 0.0f;
		}
		else
		{
			SkipOutputVelocityValue = 1.0f;
		}

		if (bHasSkipOutputVelocityParameter)
		{
			SetShaderValue(RHICmdList, VertexShaderRHI, SkipOutputVelocityParameter, SkipOutputVelocityValue);
		}
	}
}

template<typename LightMapPolicyType>
void TBasePassPixelShaderBaseType<LightMapPolicyType>::SetMesh(FRHICommandList& RHICmdList, const FVertexFactory* VertexFactory, const FSceneView& View, const FPrimitiveSceneProxy* Proxy, const FMeshBatchElement& BatchElement, EBlendMode BlendMode)
{
	if (View.GetFeatureLevel() >= ERHIFeatureLevel::SM4
		&& IsTranslucentBlendMode(BlendMode))
	{
		TranslucentLightingParameters.SetMesh(RHICmdList, this, Proxy, View.GetFeatureLevel());
	}

	FMeshMaterialShader::SetMesh(RHICmdList, GetPixelShader(), VertexFactory, View, Proxy, BatchElement);
}
