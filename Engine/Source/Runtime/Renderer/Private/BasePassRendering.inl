// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	BasePassRendering.inl: Base pass rendering implementations.
		(Due to forward declaration issues)
=============================================================================*/

#pragma once

template<typename VertexParametersType>
inline void TBasePassVertexShaderPolicyParamType<VertexParametersType>::SetMesh(FRHICommandList& RHICmdList, const FVertexFactory* VertexFactory,const FSceneView& View,const FPrimitiveSceneProxy* Proxy, const FMeshBatch& Mesh, const FMeshBatchElement& BatchElement, const FMeshDrawingRenderState& DrawRenderState)
{
	FVertexShaderRHIParamRef VertexShaderRHI = GetVertexShader();
	FMeshMaterialShader::SetMesh(RHICmdList, VertexShaderRHI, VertexFactory, View, Proxy, BatchElement, DrawRenderState);

	const bool bHasPreviousLocalToWorldParameter = PreviousLocalToWorldParameter.IsBound();
	const bool bHasSkipOutputVelocityParameter = SkipOutputVelocityParameter.IsBound();
    
    float SkipOutputVelocityValue = 1.0f;
    if (bHasPreviousLocalToWorldParameter)
    {
        FMatrix PreviousLocalToWorldMatrix;
        
        if (Proxy)
        {
            bool bHasPreviousLocalToWorldMatrix = false;
            const auto& ViewInfo = (const FViewInfo&)View;
            
            if (FVelocityDrawingPolicy::HasVelocityOnBasePass(ViewInfo, Proxy, Proxy->GetPrimitiveSceneInfo(), Mesh,
                                                              bHasPreviousLocalToWorldMatrix, PreviousLocalToWorldMatrix))
            {
                PreviousLocalToWorldMatrix = bHasPreviousLocalToWorldMatrix ? PreviousLocalToWorldMatrix : Proxy->GetLocalToWorld();
                
                SkipOutputVelocityValue = 0.0f;
            }
            else
            {
                PreviousLocalToWorldMatrix.SetIdentity();
            }
        }
        else
        {
            PreviousLocalToWorldMatrix.SetIdentity();
        }
        
        SetShaderValue(RHICmdList, VertexShaderRHI, PreviousLocalToWorldParameter, PreviousLocalToWorldMatrix);
    }
    
    if (bHasSkipOutputVelocityParameter)
    {
        SetShaderValue(RHICmdList, VertexShaderRHI, SkipOutputVelocityParameter, SkipOutputVelocityValue);
    }
}

template<typename VertexParametersType>
void TBasePassVertexShaderPolicyParamType<VertexParametersType>::SetInstancedEyeIndex(FRHICommandList& RHICmdList, const uint32 EyeIndex)
{
	if (EyeIndex > 0 && InstancedEyeIndexParameter.IsBound())
	{
		SetShaderValue(RHICmdList, GetVertexShader(), InstancedEyeIndexParameter, EyeIndex);
	}
}

template<typename PixelParametersType>
void TBasePassPixelShaderPolicyParamType<PixelParametersType>::SetMesh(FRHICommandList& RHICmdList, const FVertexFactory* VertexFactory, const FSceneView& View, const FPrimitiveSceneProxy* Proxy, const FMeshBatchElement& BatchElement, const FMeshDrawingRenderState& DrawRenderState, EBlendMode BlendMode)
{
	if (View.GetFeatureLevel() >= ERHIFeatureLevel::SM4)
	{
		ReflectionParameters.SetMesh(RHICmdList, GetPixelShader(), Proxy, View.GetFeatureLevel());
	}

	FMeshMaterialShader::SetMesh(RHICmdList, GetPixelShader(), VertexFactory, View, Proxy, BatchElement, DrawRenderState);
}
