// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	DistanceFieldSpecularOcclusion.cpp
=============================================================================*/

#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "UniformBuffer.h"
#include "ShaderParameters.h"
#include "PostProcessing.h"
#include "SceneFilterRendering.h"
#include "DistanceFieldLightingShared.h"
#include "DistanceFieldSurfaceCacheLighting.h"
#include "DistanceFieldLightingPost.h"
#include "DistanceFieldGlobalIllumination.h"
#include "GlobalDistanceField.h"
#include "PostProcessAmbientOcclusion.h"
#include "RHICommandList.h"
#include "SceneUtils.h"
#include "OneColorShader.h"
#include "BasePassRendering.h"
#include "HeightfieldLighting.h"

int32 GSpecularOcclusionDownsampleFactor = 1;

FIntPoint GetBufferSizeForSpecularOcclusion()
{
	return FIntPoint::DivideAndRoundDown(GetBufferSizeForAO(), GSpecularOcclusionDownsampleFactor);
}

class FConeTraceSpecularOcclusionCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FConeTraceSpecularOcclusionCS,Global)
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && DoesPlatformSupportDistanceFieldAO(Platform);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform,OutEnvironment);
		OutEnvironment.SetDefine(TEXT("CULLED_TILE_SIZEX"), GDistanceFieldAOTileSizeX);
		OutEnvironment.SetDefine(TEXT("TRACE_DOWNSAMPLE_FACTOR"), GSpecularOcclusionDownsampleFactor);

		// To reduce shader compile time of compute shaders with shared memory, doesn't have an impact on generated code with current compiler (June 2010 DX SDK)
		OutEnvironment.CompilerFlags.Add(CFLAG_StandardOptimization);
	}

	FConeTraceSpecularOcclusionCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		DeferredParameters.Bind(Initializer.ParameterMap);
		ObjectParameters.Bind(Initializer.ParameterMap);
		AOParameters.Bind(Initializer.ParameterMap);
		ScreenGridParameters.Bind(Initializer.ParameterMap);
		TileHeadDataUnpacked.Bind(Initializer.ParameterMap, TEXT("TileHeadDataUnpacked"));
		TileArrayData.Bind(Initializer.ParameterMap, TEXT("TileArrayData"));
		TileConeDepthRanges.Bind(Initializer.ParameterMap, TEXT("TileConeDepthRanges"));
		TileListGroupSize.Bind(Initializer.ParameterMap, TEXT("TileListGroupSize"));
		TanConeHalfAngle.Bind(Initializer.ParameterMap, TEXT("TanConeHalfAngle"));
		BentNormalNormalizeFactor.Bind(Initializer.ParameterMap, TEXT("BentNormalNormalizeFactor"));
		SpecularOcclusion.Bind(Initializer.ParameterMap, TEXT("SpecularOcclusion"));
	}

	FConeTraceSpecularOcclusionCS()
	{
	}

	void SetParameters(
		FRHICommandList& RHICmdList, 
		const FViewInfo& View, 
		FIntPoint TileListGroupSizeValue, 
		FSceneRenderTargetItem& DistanceFieldNormal, 
		const FDistanceFieldAOParameters& Parameters,
		FSceneRenderTargetItem& SpecularOcclusionBuffer)
	{
		FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();
		FGlobalShader::SetParameters(RHICmdList, ShaderRHI, View);
		DeferredParameters.Set(RHICmdList, ShaderRHI, View);
		ObjectParameters.Set(RHICmdList, ShaderRHI, GAOCulledObjectBuffers.Buffers);
		AOParameters.Set(RHICmdList, ShaderRHI, Parameters);
		ScreenGridParameters.Set(RHICmdList, ShaderRHI, View, DistanceFieldNormal);

		FAOSampleData2 AOSampleData;

		TArray<FVector, TInlineAllocator<9> > SampleDirections;
		GetSpacedVectors(SampleDirections);

		for (int32 SampleIndex = 0; SampleIndex < NumConeSampleDirections; SampleIndex++)
		{
			AOSampleData.SampleDirections[SampleIndex] = FVector4(SampleDirections[SampleIndex]);
		}

		SetUniformBufferParameterImmediate(RHICmdList, ShaderRHI, GetUniformBufferParameter<FAOSampleData2>(), AOSampleData);

		FTileIntersectionResources* TileIntersectionResources = View.ViewState->AOTileIntersectionResources;

		SetSRVParameter(RHICmdList, ShaderRHI, TileHeadDataUnpacked, TileIntersectionResources->TileHeadDataUnpacked.SRV);
		SetSRVParameter(RHICmdList, ShaderRHI, TileArrayData, TileIntersectionResources->TileArrayData.SRV);
		SetSRVParameter(RHICmdList, ShaderRHI, TileConeDepthRanges, TileIntersectionResources->TileConeDepthRanges.SRV);

		SetShaderValue(RHICmdList, ShaderRHI, TileListGroupSize, TileListGroupSizeValue);

		extern float GAOConeHalfAngle;
		SetShaderValue(RHICmdList, ShaderRHI, TanConeHalfAngle, FMath::Tan(GAOConeHalfAngle));

		FVector UnoccludedVector(0);

		for (int32 SampleIndex = 0; SampleIndex < NumConeSampleDirections; SampleIndex++)
		{
			UnoccludedVector += SampleDirections[SampleIndex];
		}

		float BentNormalNormalizeFactorValue = 1.0f / (UnoccludedVector / NumConeSampleDirections).Size();
		SetShaderValue(RHICmdList, ShaderRHI, BentNormalNormalizeFactor, BentNormalNormalizeFactorValue);

		int32 NumOutUAVs = 0;
		FUnorderedAccessViewRHIParamRef OutUAVs[1];
		OutUAVs[NumOutUAVs++] = SpecularOcclusionBuffer.UAV;

		RHICmdList.TransitionResources(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EComputeToCompute, OutUAVs, NumOutUAVs);

		SpecularOcclusion.SetTexture(RHICmdList, ShaderRHI, SpecularOcclusionBuffer.ShaderResourceTexture, SpecularOcclusionBuffer.UAV);
	}

	void UnsetParameters(FRHICommandList& RHICmdList, FSceneRenderTargetItem& SpecularOcclusionBuffer)
	{
		SpecularOcclusion.UnsetUAV(RHICmdList, GetComputeShader());

		int32 NumOutUAVs = 0;
		FUnorderedAccessViewRHIParamRef OutUAVs[1];
		OutUAVs[NumOutUAVs++] = SpecularOcclusionBuffer.UAV;

		RHICmdList.TransitionResources(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToCompute, OutUAVs, NumOutUAVs);
	}

	virtual bool Serialize(FArchive& Ar) override
	{		
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << DeferredParameters;
		Ar << ObjectParameters;
		Ar << AOParameters;
		Ar << ScreenGridParameters;
		Ar << TileHeadDataUnpacked;
		Ar << TileArrayData;
		Ar << TileConeDepthRanges;
		Ar << TileListGroupSize;
		Ar << TanConeHalfAngle;
		Ar << BentNormalNormalizeFactor;
		Ar << SpecularOcclusion;
		return bShaderHasOutdatedParameters;
	}

private:

	FDeferredPixelShaderParameters DeferredParameters;
	FDistanceFieldCulledObjectBufferParameters ObjectParameters;
	FAOParameters AOParameters;
	FScreenGridParameters ScreenGridParameters;
	FShaderResourceParameter TileHeadDataUnpacked;
	FShaderResourceParameter TileArrayData;
	FShaderResourceParameter TileConeDepthRanges;
	FShaderParameter TileListGroupSize;
	FShaderParameter TanConeHalfAngle;
	FShaderParameter BentNormalNormalizeFactor;
	FRWShaderParameter SpecularOcclusion;
};

IMPLEMENT_SHADER_TYPE(,FConeTraceSpecularOcclusionCS,TEXT("DistanceFieldSpecularOcclusion"),TEXT("ConeTraceSpecularOcclusionCS"),SF_Compute);

void FDeferredShadingSceneRenderer::RenderDistanceFieldSpecularOcclusion(
	FRHICommandListImmediate& RHICmdList, 
	const FViewInfo& View,
	FIntPoint TileListGroupSize,
	const FDistanceFieldAOParameters& Parameters, 
	const TRefCountPtr<IPooledRenderTarget>& DistanceFieldNormal, 
	TRefCountPtr<IPooledRenderTarget>& OutSpecularOcclusion)
{
	extern int32 GDistanceFieldAOSpecularOcclusionMode;

	if (GDistanceFieldAOSpecularOcclusionMode == 2)
	{
		const FIntPoint ConeTraceBufferSize = GetBufferSizeForSpecularOcclusion();

		SetRenderTarget(RHICmdList, NULL, NULL);

		{
			FPooledRenderTargetDesc Desc(FPooledRenderTargetDesc::Create2DDesc(ConeTraceBufferSize, PF_R16F, FClearValueBinding::None, TexCreate_None, TexCreate_RenderTargetable | TexCreate_UAV, false));
			GRenderTargetPool.FindFreeElement(RHICmdList, Desc, OutSpecularOcclusion, TEXT("DownsampledSpecularOcclusion"));
		}

		{
			SCOPED_DRAW_EVENT(RHICmdList, ConeTraceSpecularOcclusion);

			const uint32 GroupSizeX = FMath::DivideAndRoundUp(View.ViewRect.Size().X / GAODownsampleFactor, GDistanceFieldAOTileSizeX / GSpecularOcclusionDownsampleFactor);
			const uint32 GroupSizeY = FMath::DivideAndRoundUp(View.ViewRect.Size().Y / GAODownsampleFactor, GDistanceFieldAOTileSizeY / GSpecularOcclusionDownsampleFactor);

			TShaderMapRef<FConeTraceSpecularOcclusionCS> ComputeShader(View.ShaderMap);

			RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());
			ComputeShader->SetParameters(RHICmdList, View, TileListGroupSize, DistanceFieldNormal->GetRenderTargetItem(), Parameters, OutSpecularOcclusion->GetRenderTargetItem());
			DispatchComputeShader(RHICmdList, *ComputeShader, GroupSizeX, GroupSizeY, 1);
			ComputeShader->UnsetParameters(RHICmdList, OutSpecularOcclusion->GetRenderTargetItem());
		}

		GRenderTargetPool.VisualizeTexture.SetCheckPoint(RHICmdList, OutSpecularOcclusion);
	}
}
