// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	CapsuleShadowRendering.cpp: Functionality for rendering shadows from capsules
=============================================================================*/

#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "OneColorShader.h"
#include "LightRendering.h"
#include "SceneFilterRendering.h"
#include "ScreenRendering.h"
#include "SceneUtils.h"
#include "PostProcessing.h"

int32 GCapsuleShadows = 1;
FAutoConsoleVariableRef CVarCapsuleShadows(
	TEXT("r.CapsuleShadows"),
	GCapsuleShadows,
	TEXT("Whether to allow capsule shadowing on skinned components with bCastCapsuleDirectShadow or bCastCapsuleIndirectShadow enabled."),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

int32 GCapsuleShadowsFullResolution = 0;
FAutoConsoleVariableRef CVarCapsuleShadowsFullResolution(
	TEXT("r.CapsuleShadowsFullResolution"),
	GCapsuleShadowsFullResolution,
	TEXT("Whether to compute capsule shadows at full resolution."),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

float GCapsuleMaxDirectOcclusionDistance = 400;
FAutoConsoleVariableRef CVarCapsuleMaxDirectOcclusionDistance(
	TEXT("r.CapsuleMaxDirectOcclusionDistance"),
	GCapsuleMaxDirectOcclusionDistance,
	TEXT("Maximum cast distance for direct shadows from capsules.  This has a big impact on performance."),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

float GCapsuleMaxIndirectOcclusionDistance = 200;
FAutoConsoleVariableRef CVarCapsuleMaxIndirectOcclusionDistance(
	TEXT("r.CapsuleMaxIndirectOcclusionDistance"),
	GCapsuleMaxIndirectOcclusionDistance,
	TEXT("Maximum cast distance for indirect shadows from capsules.  This has a big impact on performance."),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

float GCapsuleShadowFadeAngleFromVertical = PI / 3;
FAutoConsoleVariableRef CVarCapsuleShadowFadeAngleFromVertical(
	TEXT("r.CapsuleShadowFadeAngleFromVertical"),
	GCapsuleShadowFadeAngleFromVertical,
	TEXT("Angle from vertical up to start fading out the indirect shadow, to avoid self shadowing artifacts."),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

float GCapsuleIndirectConeAngle = PI / 8;
FAutoConsoleVariableRef CVarCapsuleIndirectConeAngle(
	TEXT("r.CapsuleIndirectConeAngle"),
	GCapsuleIndirectConeAngle,
	TEXT("Light source angle used when the indirect shadow direction is derived from precomputed indirect lighting (no stationary skylight present)"),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

float GCapsuleSkyAngleScale = .6f;
FAutoConsoleVariableRef CVarCapsuleSkyAngleScale(
	TEXT("r.CapsuleSkyAngleScale"),
	GCapsuleSkyAngleScale,
	TEXT("Scales the light source angle derived from the precomputed unoccluded sky vector (stationary skylight present)"),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

float GCapsuleIndirectShadowMinVisibility = .1f;
FAutoConsoleVariableRef CVarCapsuleIndirectShadowMinVisibility(
	TEXT("r.CapsuleIndirectShadowMinVisibility"),
	GCapsuleIndirectShadowMinVisibility,
	TEXT("Minimum visibility caused by capsule indirect shadows.  This is used to keep the indirect shadows from going fully black."),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

int32 GShadowShapeTileSize = 8;

int32 GetCapsuleShadowDownsampleFactor()
{
	return GCapsuleShadowsFullResolution ? 1 : 2;
}

FIntPoint GetBufferSizeForCapsuleShadows()
{
	return FIntPoint::DivideAndRoundDown(FSceneRenderTargets::Get_FrameConstantsOnly().GetBufferSizeXY(), GetCapsuleShadowDownsampleFactor());
}

bool DoesPlatformSupportCapsuleShadows(EShaderPlatform Platform)
{
	// Hasn't been tested elsewhere yet
	return Platform == SP_PCD3D_SM5 || Platform == SP_PS4;
}

enum ECapsuleShadowingType
{
	ShapeShadow_DirectionalLightTiledCulling,
	ShapeShadow_PointLightTiledCulling,
	ShapeShadow_IndirectTiledCulling
};

template<ECapsuleShadowingType ShadowingType>
class TCapsuleShadowingCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(TCapsuleShadowingCS, Global);
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && DoesPlatformSupportCapsuleShadows(Platform);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEX"), GShadowShapeTileSize);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEY"), GShadowShapeTileSize);
		OutEnvironment.SetDefine(TEXT("POINT_LIGHT"), ShadowingType == ShapeShadow_PointLightTiledCulling ? TEXT("1") : TEXT("0"));
		OutEnvironment.SetDefine(TEXT("COHERENT_LIGHT_SOURCE"), (ShadowingType == ShapeShadow_DirectionalLightTiledCulling || ShadowingType == ShapeShadow_PointLightTiledCulling)? TEXT("1") : TEXT("0"));
		OutEnvironment.CompilerFlags.Add(CFLAG_StandardOptimization);
	}

	/** Default constructor. */
	TCapsuleShadowingCS() {}

	/** Initialization constructor. */
	TCapsuleShadowingCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		ShadowFactors.Bind(Initializer.ParameterMap, TEXT("ShadowFactors"));
		NumGroups.Bind(Initializer.ParameterMap, TEXT("NumGroups"));
		LightDirection.Bind(Initializer.ParameterMap, TEXT("LightDirection"));
		LightSourceRadius.Bind(Initializer.ParameterMap, TEXT("LightSourceRadius"));
		RayStartOffsetDepthScale.Bind(Initializer.ParameterMap, TEXT("RayStartOffsetDepthScale"));
		LightPositionAndInvRadius.Bind(Initializer.ParameterMap, TEXT("LightPositionAndInvRadius"));
		LightAngleAndNormalThreshold.Bind(Initializer.ParameterMap, TEXT("LightAngleAndNormalThreshold"));
		ScissorRectMinAndSize.Bind(Initializer.ParameterMap, TEXT("ScissorRectMinAndSize"));
		DeferredParameters.Bind(Initializer.ParameterMap);
		DownsampleFactor.Bind(Initializer.ParameterMap, TEXT("DownsampleFactor"));
		NumShadowSpheres.Bind(Initializer.ParameterMap, TEXT("NumShadowSpheres"));
		ShadowSphereShapes.Bind(Initializer.ParameterMap, TEXT("ShadowSphereShapes"));
		NumShadowCapsules.Bind(Initializer.ParameterMap, TEXT("NumShadowCapsules"));
		ShadowCapsuleShapes.Bind(Initializer.ParameterMap, TEXT("ShadowCapsuleShapes"));
		MaxOcclusionDistance.Bind(Initializer.ParameterMap, TEXT("MaxOcclusionDistance"));
		LightDirectionData.Bind(Initializer.ParameterMap, TEXT("LightDirectionData"));
		MinVisibility.Bind(Initializer.ParameterMap, TEXT("MinVisibility"));
	}

	void SetParameters(
		FRHICommandList& RHICmdList, 
		const FSceneView& View, 
		const FLightSceneInfo* LightSceneInfo,
		FSceneRenderTargetItem& ShadowFactorsValue, 
		FVector2D NumGroupsValue,
		float MaxOcclusionDistanceValue,
		const FIntRect& ScissorRect,
		int32 NumShadowSpheresValue,
		FShaderResourceViewRHIParamRef ShadowSphereShapesSRV,
		int32 NumShadowCapsulesValue,
		FShaderResourceViewRHIParamRef ShadowCapsuleShapesSRV,
		FShaderResourceViewRHIParamRef LightDirectionDataSRV)
	{
		const FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();

		FGlobalShader::SetParameters(RHICmdList, ShaderRHI, View);

		RHICmdList.TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EComputeToCompute, ShadowFactorsValue.UAV);
		ShadowFactors.SetTexture(RHICmdList, ShaderRHI, ShadowFactorsValue.ShaderResourceTexture, ShadowFactorsValue.UAV);

		DeferredParameters.Set(RHICmdList, ShaderRHI, View);

		SetShaderValue(RHICmdList, ShaderRHI, NumGroups, NumGroupsValue);

		if (LightSceneInfo)
		{
			check(ShadowingType == ShapeShadow_DirectionalLightTiledCulling || ShadowingType == ShapeShadow_PointLightTiledCulling);
			FVector4 LightPositionAndInvRadiusValue;
			FVector4 LightColorAndFalloffExponent;
			FVector NormalizedLightDirection;
			FVector2D SpotAngles;
			float LightSourceRadiusValue;
			float LightSourceLength;
			float LightMinRoughness;

			const FLightSceneProxy& LightProxy = *LightSceneInfo->Proxy;

			LightProxy.GetParameters(LightPositionAndInvRadiusValue, LightColorAndFalloffExponent, NormalizedLightDirection, SpotAngles, LightSourceRadiusValue, LightSourceLength, LightMinRoughness);

			SetShaderValue(RHICmdList, ShaderRHI, LightDirection, NormalizedLightDirection);
			SetShaderValue(RHICmdList, ShaderRHI, LightPositionAndInvRadius, LightPositionAndInvRadiusValue);
			// Default light source radius of 0 gives poor results
			SetShaderValue(RHICmdList, ShaderRHI, LightSourceRadius, LightSourceRadiusValue == 0 ? 20 : FMath::Clamp(LightSourceRadiusValue, .001f, 1.0f / (4 * LightPositionAndInvRadiusValue.W)));

			SetShaderValue(RHICmdList, ShaderRHI, RayStartOffsetDepthScale, LightProxy.GetRayStartOffsetDepthScale());

			const float LightSourceAngle = FMath::Clamp(LightProxy.GetLightSourceAngle() * 5, 1.0f, 30.0f) * PI / 180.0f;
			const FVector LightAngleAndNormalThresholdValue(LightSourceAngle, FMath::Cos(PI / 2 + LightSourceAngle), LightProxy.GetTraceDistance());
			SetShaderValue(RHICmdList, ShaderRHI, LightAngleAndNormalThreshold, LightAngleAndNormalThresholdValue);
		}
		else
		{
			check(ShadowingType == ShapeShadow_IndirectTiledCulling);
			check(!LightDirection.IsBound() && !LightPositionAndInvRadius.IsBound());
		}

		SetShaderValue(RHICmdList, ShaderRHI, ScissorRectMinAndSize, FIntRect(ScissorRect.Min, ScissorRect.Size()));
		SetShaderValue(RHICmdList, ShaderRHI, DownsampleFactor, GetCapsuleShadowDownsampleFactor());

		SetShaderValue(RHICmdList, ShaderRHI, NumShadowSpheres, NumShadowSpheresValue);
		SetSRVParameter(RHICmdList, ShaderRHI, ShadowSphereShapes, ShadowSphereShapesSRV);

		SetShaderValue(RHICmdList, ShaderRHI, NumShadowCapsules, NumShadowCapsulesValue);
		SetSRVParameter(RHICmdList, ShaderRHI, ShadowCapsuleShapes, ShadowCapsuleShapesSRV);

		SetShaderValue(RHICmdList, ShaderRHI, MaxOcclusionDistance, MaxOcclusionDistanceValue);
		SetSRVParameter(RHICmdList, ShaderRHI, LightDirectionData, LightDirectionDataSRV);

		SetShaderValue(RHICmdList, ShaderRHI, MinVisibility, GCapsuleIndirectShadowMinVisibility);
	}

	void UnsetParameters(FRHICommandList& RHICmdList, FSceneRenderTargetItem& ShadowFactorsValue)
	{
		ShadowFactors.UnsetUAV(RHICmdList, GetComputeShader());
		RHICmdList.TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EComputeToCompute, ShadowFactorsValue.UAV);
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << ShadowFactors;
		Ar << NumGroups;
		Ar << LightDirection;
		Ar << LightPositionAndInvRadius;
		Ar << LightSourceRadius;
		Ar << RayStartOffsetDepthScale;
		Ar << LightAngleAndNormalThreshold;
		Ar << ScissorRectMinAndSize;
		Ar << DeferredParameters;
		Ar << DownsampleFactor;
		Ar << NumShadowSpheres;
		Ar << NumShadowSpheres;
		Ar << ShadowSphereShapes;
		Ar << NumShadowCapsules;
		Ar << ShadowCapsuleShapes;
		Ar << MaxOcclusionDistance;
		Ar << LightDirectionData;
		Ar << MinVisibility;
		return bShaderHasOutdatedParameters;
	}

private:

	FRWShaderParameter ShadowFactors;
	FShaderParameter NumGroups;
	FShaderParameter LightDirection;
	FShaderParameter LightPositionAndInvRadius;
	FShaderParameter LightSourceRadius;
	FShaderParameter RayStartOffsetDepthScale;
	FShaderParameter LightAngleAndNormalThreshold;
	FShaderParameter ScissorRectMinAndSize;
	FDeferredPixelShaderParameters DeferredParameters;
	FShaderParameter DownsampleFactor;
	FShaderParameter NumShadowSpheres;
	FShaderResourceParameter ShadowSphereShapes;
	FShaderParameter NumShadowCapsules;
	FShaderResourceParameter ShadowCapsuleShapes;
	FShaderParameter MaxOcclusionDistance;
	FShaderResourceParameter LightDirectionData;
	FShaderParameter MinVisibility;
};

IMPLEMENT_SHADER_TYPE(template<>,TCapsuleShadowingCS<ShapeShadow_DirectionalLightTiledCulling>,TEXT("CapsuleShadowShaders"),TEXT("CapsuleShadowingCS"),SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>,TCapsuleShadowingCS<ShapeShadow_PointLightTiledCulling>,TEXT("CapsuleShadowShaders"),TEXT("CapsuleShadowingCS"),SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>,TCapsuleShadowingCS<ShapeShadow_IndirectTiledCulling>,TEXT("CapsuleShadowShaders"),TEXT("CapsuleShadowingCS"),SF_Compute);

template<bool bUpsampleRequired>
class TCapsuleShadowingUpsamplePS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(TCapsuleShadowingUpsamplePS, Global);
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && DoesPlatformSupportCapsuleShadows(Platform);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("DOWNSAMPLE_FACTOR"), 2);
		OutEnvironment.SetDefine(TEXT("UPSAMPLE_REQUIRED"), bUpsampleRequired ? TEXT("1") : TEXT("0"));
	}

	/** Default constructor. */
	TCapsuleShadowingUpsamplePS() {}

	/** Initialization constructor. */
	TCapsuleShadowingUpsamplePS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		DeferredParameters.Bind(Initializer.ParameterMap);
		ShadowFactorsTexture.Bind(Initializer.ParameterMap,TEXT("ShadowFactorsTexture"));
		ShadowFactorsSampler.Bind(Initializer.ParameterMap,TEXT("ShadowFactorsSampler"));
		ScissorRectMinAndSize.Bind(Initializer.ParameterMap,TEXT("ScissorRectMinAndSize"));
		OutputtingToLightAttenuation.Bind(Initializer.ParameterMap,TEXT("OutputtingToLightAttenuation"));
	}

	void SetParameters(FRHICommandList& RHICmdList, const FSceneView& View, const FIntRect& ScissorRect, TRefCountPtr<IPooledRenderTarget>& ShadowFactorsTextureValue, bool bOutputtingToLightAttenuation)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		FGlobalShader::SetParameters(RHICmdList, ShaderRHI, View);
		DeferredParameters.Set(RHICmdList, ShaderRHI, View);

		SetTextureParameter(RHICmdList, ShaderRHI, ShadowFactorsTexture, ShadowFactorsSampler, TStaticSamplerState<SF_Bilinear>::GetRHI(), ShadowFactorsTextureValue->GetRenderTargetItem().ShaderResourceTexture);
	
		SetShaderValue(RHICmdList, ShaderRHI, ScissorRectMinAndSize, FIntRect(ScissorRect.Min, ScissorRect.Size()));
		SetShaderValue(RHICmdList, ShaderRHI, OutputtingToLightAttenuation, bOutputtingToLightAttenuation ? 1.0f : 0.0f);
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << DeferredParameters;
		Ar << ShadowFactorsTexture;
		Ar << ShadowFactorsSampler;
		Ar << ScissorRectMinAndSize;
		Ar << OutputtingToLightAttenuation;
		return bShaderHasOutdatedParameters;
	}

private:

	FDeferredPixelShaderParameters DeferredParameters;
	FShaderResourceParameter ShadowFactorsTexture;
	FShaderResourceParameter ShadowFactorsSampler;
	FShaderParameter ScissorRectMinAndSize;
	FShaderParameter OutputtingToLightAttenuation;
};

IMPLEMENT_SHADER_TYPE(template<>,TCapsuleShadowingUpsamplePS<true>,TEXT("CapsuleShadowShaders"),TEXT("CapsuleShadowingUpsamplePS"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>,TCapsuleShadowingUpsamplePS<false>,TEXT("CapsuleShadowShaders"),TEXT("CapsuleShadowingUpsamplePS"),SF_Pixel);

bool SupportsCapsuleShadows(ERHIFeatureLevel::Type FeatureLevel, EShaderPlatform ShaderPlatform)
{
	return GCapsuleShadows
		&& FeatureLevel >= ERHIFeatureLevel::SM5
		&& DoesPlatformSupportCapsuleShadows(ShaderPlatform);
}

bool FDeferredShadingSceneRenderer::RenderCapsuleDirectShadows(
	const FLightSceneInfo& LightSceneInfo,
	FRHICommandListImmediate& RHICmdList, 
	const TArray<FProjectedShadowInfo*, SceneRenderingAllocator>& CapsuleShadows) const
{
	//@todo - splitscreen
	const int32 ViewIndex = 0;
	const FViewInfo& View = Views[ViewIndex];

	if (SupportsCapsuleShadows(View.GetFeatureLevel(), View.GetShaderPlatform()) && CapsuleShadows.Num() > 0)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_RenderCapsuleShadows);
		SCOPED_DRAW_EVENT(RHICmdList, CapsuleShadows);

		static TArray<FSphere> SphereShapeData;
		static TArray<FCapsuleShape> CapsuleShapeData;
		SphereShapeData.Reset();
		CapsuleShapeData.Reset();

		for (int32 ShadowIndex = 0; ShadowIndex < CapsuleShadows.Num(); ShadowIndex++)
		{
			FProjectedShadowInfo* Shadow = CapsuleShadows[ShadowIndex];

			int32 OriginalSphereIndex = SphereShapeData.Num();
			int32 OriginalCapsuleIndex = CapsuleShapeData.Num();

			TArray<const FPrimitiveSceneInfo*, SceneRenderingAllocator> ShadowGroupPrimitives;
			Shadow->GetParentSceneInfo()->GatherLightingAttachmentGroupPrimitives(ShadowGroupPrimitives);

			for (int32 ChildIndex = 0; ChildIndex < ShadowGroupPrimitives.Num(); ChildIndex++)
			{
				const FPrimitiveSceneInfo* PrimitiveSceneInfo = ShadowGroupPrimitives[ChildIndex];

				if (PrimitiveSceneInfo->Proxy->CastsDynamicShadow())
				{
					PrimitiveSceneInfo->Proxy->GetShadowShapes(SphereShapeData, CapsuleShapeData);
				}
			}

			const float FadeRadiusScale = Shadow->FadeAlphas[ViewIndex];

			for (int32 ShapeIndex = OriginalSphereIndex; ShapeIndex < SphereShapeData.Num(); ShapeIndex++)
			{
				SphereShapeData[ShapeIndex].W *= FadeRadiusScale;
			}

			for (int32 ShapeIndex = OriginalCapsuleIndex; ShapeIndex < CapsuleShapeData.Num(); ShapeIndex++)
			{
				CapsuleShapeData[ShapeIndex].Radius *= FadeRadiusScale;
			}
		}

		if (SphereShapeData.Num() > 0 || CapsuleShapeData.Num() > 0)
		{
			if (SphereShapeData.Num() > 0)
			{
				const int32 DataSize = SphereShapeData.Num() * SphereShapeData.GetTypeSize();

				if (!IsValidRef(LightSceneInfo.ShadowSphereShapesVertexBuffer) || (int32)LightSceneInfo.ShadowSphereShapesVertexBuffer->GetSize() < DataSize)
				{
					LightSceneInfo.ShadowSphereShapesVertexBuffer.SafeRelease();
					LightSceneInfo.ShadowSphereShapesSRV.SafeRelease();
					FRHIResourceCreateInfo CreateInfo;
					LightSceneInfo.ShadowSphereShapesVertexBuffer = RHICreateVertexBuffer(DataSize, BUF_Volatile | BUF_ShaderResource, CreateInfo);
					LightSceneInfo.ShadowSphereShapesSRV = RHICreateShaderResourceView(LightSceneInfo.ShadowSphereShapesVertexBuffer, SphereShapeData.GetTypeSize(), PF_A32B32G32R32F);
				}

				FSphere* SphereShapeLockedData = (FSphere*)RHILockVertexBuffer(LightSceneInfo.ShadowSphereShapesVertexBuffer, 0, DataSize, RLM_WriteOnly);
				FPlatformMemory::Memcpy(SphereShapeLockedData, SphereShapeData.GetData(), DataSize);
				RHIUnlockVertexBuffer(LightSceneInfo.ShadowSphereShapesVertexBuffer);
			}
			
			if (CapsuleShapeData.Num() > 0)
			{
				static_assert(sizeof(FCapsuleShape) == sizeof(FVector4) * 2, "FCapsuleShape has padding");
				const int32 DataSize = CapsuleShapeData.Num() * CapsuleShapeData.GetTypeSize();

				if (!IsValidRef(LightSceneInfo.ShadowCapsuleShapesVertexBuffer) || (int32)LightSceneInfo.ShadowCapsuleShapesVertexBuffer->GetSize() < DataSize)
				{
					LightSceneInfo.ShadowCapsuleShapesVertexBuffer.SafeRelease();
					LightSceneInfo.ShadowCapsuleShapesSRV.SafeRelease();
					FRHIResourceCreateInfo CreateInfo;
					LightSceneInfo.ShadowCapsuleShapesVertexBuffer = RHICreateVertexBuffer(DataSize, BUF_Volatile | BUF_ShaderResource, CreateInfo);
					LightSceneInfo.ShadowCapsuleShapesSRV = RHICreateShaderResourceView(LightSceneInfo.ShadowCapsuleShapesVertexBuffer, sizeof(FVector4), PF_A32B32G32R32F);
				}

				void* CapsuleShapeLockedData = RHILockVertexBuffer(LightSceneInfo.ShadowCapsuleShapesVertexBuffer, 0, DataSize, RLM_WriteOnly);
				FPlatformMemory::Memcpy(CapsuleShapeLockedData, CapsuleShapeData.GetData(), DataSize);
				RHIUnlockVertexBuffer(LightSceneInfo.ShadowCapsuleShapesVertexBuffer);
			}

			FSceneRenderTargets::Get(RHICmdList).FinishRenderingLightAttenuation(RHICmdList);
			SetRenderTarget(RHICmdList, NULL, NULL);

			TRefCountPtr<IPooledRenderTarget> RayTracedShadowsRT;

			{
				const FIntPoint BufferSize = GetBufferSizeForCapsuleShadows();
				FPooledRenderTargetDesc Desc(FPooledRenderTargetDesc::Create2DDesc(BufferSize, PF_G16R16F, FClearValueBinding::None, TexCreate_None, TexCreate_RenderTargetable | TexCreate_UAV, false));
				GRenderTargetPool.FindFreeElement(RHICmdList, Desc, RayTracedShadowsRT, TEXT("RayTracedShadows"));
			}

			const bool bDirectionalLight = LightSceneInfo.Proxy->GetLightType() == LightType_Directional;
			FIntRect ScissorRect;

			{
				if (!LightSceneInfo.Proxy->GetScissorRect(ScissorRect, View))
				{
					ScissorRect = View.ViewRect;
				}

				uint32 GroupSizeX = FMath::DivideAndRoundUp(ScissorRect.Size().X / GetCapsuleShadowDownsampleFactor(), GShadowShapeTileSize);
				uint32 GroupSizeY = FMath::DivideAndRoundUp(ScissorRect.Size().Y / GetCapsuleShadowDownsampleFactor(), GShadowShapeTileSize);

				{
					SCOPED_DRAW_EVENT(RHICmdList, TiledCapsuleShadowing);

					FSceneRenderTargetItem& RayTracedShadowsRTI = RayTracedShadowsRT->GetRenderTargetItem();
					if (bDirectionalLight)
					{
						TShaderMapRef<TCapsuleShadowingCS<ShapeShadow_DirectionalLightTiledCulling> > ComputeShader(View.ShaderMap);
						RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());

						ComputeShader->SetParameters(
							RHICmdList, 
							View, 
							&LightSceneInfo, 
							RayTracedShadowsRTI, 
							FVector2D(GroupSizeX, GroupSizeY), 
							GCapsuleMaxDirectOcclusionDistance, 
							ScissorRect, 
							SphereShapeData.Num(), 
							LightSceneInfo.ShadowSphereShapesSRV.GetReference(), 
							CapsuleShapeData.Num(), 
							LightSceneInfo.ShadowCapsuleShapesSRV.GetReference(),
							NULL);

						DispatchComputeShader(RHICmdList, *ComputeShader, GroupSizeX, GroupSizeY, 1);
						ComputeShader->UnsetParameters(RHICmdList, RayTracedShadowsRTI);
					}
					else
					{
						TShaderMapRef<TCapsuleShadowingCS<ShapeShadow_PointLightTiledCulling> > ComputeShader(View.ShaderMap);
						RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());

						ComputeShader->SetParameters(
							RHICmdList, 
							View, 
							&LightSceneInfo, 
							RayTracedShadowsRTI, 
							FVector2D(GroupSizeX, GroupSizeY), 
							GCapsuleMaxDirectOcclusionDistance, 
							ScissorRect, 
							SphereShapeData.Num(), 
							LightSceneInfo.ShadowSphereShapesSRV.GetReference(), 
							CapsuleShapeData.Num(), 
							LightSceneInfo.ShadowCapsuleShapesSRV.GetReference(),
							NULL);

						DispatchComputeShader(RHICmdList, *ComputeShader, GroupSizeX, GroupSizeY, 1);
						ComputeShader->UnsetParameters(RHICmdList, RayTracedShadowsRTI);
					}
				}
			}

			{
				FSceneRenderTargets::Get(RHICmdList).BeginRenderingLightAttenuation(RHICmdList);

				SCOPED_DRAW_EVENT(RHICmdList, Upsample);

				RHICmdList.SetViewport(ScissorRect.Min.X, ScissorRect.Min.Y, 0.0f, ScissorRect.Max.X, ScissorRect.Max.Y, 1.0f);
				RHICmdList.SetRasterizerState(TStaticRasterizerState<FM_Solid, CM_None>::GetRHI());
				RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());
				
				// use B and A in Light Attenuation for per-object shadows
				// CO_Min is needed to combine multiple shadow passes
				RHICmdList.SetBlendState(TStaticBlendState<CW_BA, BO_Min, BF_One, BF_One, BO_Min, BF_One, BF_One>::GetRHI());

				TShaderMapRef<FPostProcessVS> VertexShader(View.ShaderMap);

				if (GCapsuleShadowsFullResolution)
				{
					TShaderMapRef<TCapsuleShadowingUpsamplePS<false> > PixelShader(View.ShaderMap);

					static FGlobalBoundShaderState BoundShaderState;
					SetGlobalBoundShaderState(RHICmdList, View.GetFeatureLevel(), BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);

					VertexShader->SetParameters(RHICmdList, View);
					PixelShader->SetParameters(RHICmdList, View, ScissorRect, RayTracedShadowsRT, true);
				}
				else
				{
					TShaderMapRef<TCapsuleShadowingUpsamplePS<true> > PixelShader(View.ShaderMap);

					static FGlobalBoundShaderState BoundShaderState;
					SetGlobalBoundShaderState(RHICmdList, View.GetFeatureLevel(), BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);

					VertexShader->SetParameters(RHICmdList, View);
					PixelShader->SetParameters(RHICmdList, View, ScissorRect, RayTracedShadowsRT, true);
				}

				DrawRectangle( 
					RHICmdList,
					0, 0, 
					ScissorRect.Width(), ScissorRect.Height(),
					ScissorRect.Min.X / GetCapsuleShadowDownsampleFactor(), ScissorRect.Min.Y / GetCapsuleShadowDownsampleFactor(), 
					ScissorRect.Width() / GetCapsuleShadowDownsampleFactor(), ScissorRect.Height() / GetCapsuleShadowDownsampleFactor(),
					FIntPoint(ScissorRect.Width(), ScissorRect.Height()),
					GetBufferSizeForCapsuleShadows(),
					*VertexShader);
			}

			return true;
		}
	}

	return false;
}

void FDeferredShadingSceneRenderer::CreateIndirectCapsuleShadows()
{
	FViewInfo& View = Views[0];

	for (FScenePrimitiveOctree::TConstIterator<SceneRenderingAllocator> PrimitiveOctreeIt(Scene->PrimitiveOctree);
		PrimitiveOctreeIt.HasPendingNodes();
		PrimitiveOctreeIt.Advance())
	{
		const FScenePrimitiveOctree::FNode& PrimitiveOctreeNode = PrimitiveOctreeIt.GetCurrentNode();
		const FOctreeNodeContext& PrimitiveOctreeNodeContext = PrimitiveOctreeIt.GetCurrentContext();

		// Find children of this octree node that may contain relevant primitives.
		FOREACH_OCTREE_CHILD_NODE(ChildRef)
		{
			if(PrimitiveOctreeNode.HasChild(ChildRef))
			{
				// Check that the child node is in the frustum
				const FOctreeNodeContext ChildContext = PrimitiveOctreeNodeContext.GetChildContext(ChildRef);

				if (View.ViewFrustum.IntersectBox(ChildContext.Bounds.Center, ChildContext.Bounds.Extent + FVector(GCapsuleMaxIndirectOcclusionDistance)))
				{
					PrimitiveOctreeIt.PushChild(ChildRef);
				}
			}
		}

		// Check all the primitives in this octree node.
		for (FScenePrimitiveOctree::ElementConstIt NodePrimitiveIt(PrimitiveOctreeNode.GetElementIt());NodePrimitiveIt;++NodePrimitiveIt)
		{
			const FPrimitiveSceneInfoCompact& PrimitiveSceneInfoCompact = *NodePrimitiveIt;

			if (PrimitiveSceneInfoCompact.bCastDynamicShadow)
			{
				FPrimitiveSceneInfo* PrimitiveSceneInfo = PrimitiveSceneInfoCompact.PrimitiveSceneInfo;
				FPrimitiveSceneProxy* PrimitiveProxy = PrimitiveSceneInfoCompact.Proxy;

				if (PrimitiveProxy->CastsCapsuleIndirectShadow())
				{
					TArray<FPrimitiveSceneInfo*, SceneRenderingAllocator> ShadowGroupPrimitives;
					PrimitiveSceneInfo->GatherLightingAttachmentGroupPrimitives(ShadowGroupPrimitives);

					// Compute the composite bounds of this group of shadow primitives.
					FBoxSphereBounds LightingGroupBounds = ShadowGroupPrimitives[0]->Proxy->GetBounds();

					for (int32 ChildIndex = 1; ChildIndex < ShadowGroupPrimitives.Num(); ChildIndex++)
					{
						const FPrimitiveSceneInfo* ShadowChild = ShadowGroupPrimitives[ChildIndex];

						if (ShadowChild->Proxy->CastsDynamicShadow())
						{
							LightingGroupBounds = LightingGroupBounds + ShadowChild->Proxy->GetBounds();
						}
					}

					if (View.ViewFrustum.IntersectBox(LightingGroupBounds.Origin, LightingGroupBounds.BoxExtent + FVector(GCapsuleMaxIndirectOcclusionDistance)))
					{
						View.IndirectShadowPrimitives.Add(PrimitiveSceneInfo);
					}
				}
			}
		}
	}
}

bool DWUseSHDirection = false;

void FDeferredShadingSceneRenderer::RenderIndirectCapsuleShadows(FRHICommandListImmediate& RHICmdList) const
{
	//@todo - splitscreen
	const FViewInfo& View = Views[0];

	if (SupportsCapsuleShadows(View.GetFeatureLevel(), View.GetShaderPlatform()) 
		&& View.IndirectShadowPrimitives.Num() > 0
		&& View.ViewState)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_RenderIndirectCapsuleShadows);
		SCOPED_DRAW_EVENT(RHICmdList, IndirectCapsuleShadows);
		const float CosFadeStartAngle = FMath::Cos(GCapsuleShadowFadeAngleFromVertical);

		static TArray<FSphere> SphereShapeData;
		static TArray<FCapsuleShape> CapsuleShapeData;
		static TArray<FVector4> SphereLightSourceData;
		static TArray<FVector4> CapsuleLightSourceData;

		SphereShapeData.Reset();
		CapsuleShapeData.Reset();
		SphereLightSourceData.Reset();
		CapsuleLightSourceData.Reset();

		for (int32 PrimitiveIndex = 0; PrimitiveIndex < View.IndirectShadowPrimitives.Num(); PrimitiveIndex++)
		{
			FPrimitiveSceneInfo* PrimitiveSceneInfo = View.IndirectShadowPrimitives[PrimitiveIndex];
			const FIndirectLightingCacheAllocation* Allocation = PrimitiveSceneInfo->IndirectLightingCacheAllocation;

			FVector4 PackedLightDirection(0, 0, 1, PI / 8);
			float ShapeFadeAlpha = 1;

			if (Allocation)
			{
				if (Scene 
					&& Scene->SkyLight 
					// Skylights with static lighting already had their diffuse contribution baked into lightmaps
					&& !Scene->SkyLight->bHasStaticLighting
					&& View.Family->EngineShowFlags.SkyLighting)
				{
					// Get the indirect shadow direction from the unoccluded sky direction
					PackedLightDirection = FVector4(Allocation->CurrentSkyBentNormal, Allocation->CurrentSkyBentNormal.W * GCapsuleSkyAngleScale * .5f * PI);
				}
				else
				{
					FSHVectorRGB2 IndirectLighting;
					IndirectLighting.R = FSHVector2(Allocation->TargetSamplePacked[0]);
					IndirectLighting.G = FSHVector2(Allocation->TargetSamplePacked[1]);
					IndirectLighting.B = FSHVector2(Allocation->TargetSamplePacked[2]);
					const FSHVector2 IndirectLightingIntensity = IndirectLighting.GetLuminance();
					const FVector ExtractedMaxDirection = IndirectLightingIntensity.GetMaximumDirection();

					// Get the indirect shadow direction from the primary indirect lighting direction
					PackedLightDirection = FVector4(ExtractedMaxDirection, GCapsuleIndirectConeAngle);
				}

				if (CosFadeStartAngle < 1)
				{
					// Fade out when nearly vertical up due to self shadowing artifacts
					ShapeFadeAlpha = 1 - FMath::Clamp(2 * (-PackedLightDirection.Z - CosFadeStartAngle) / (1 - CosFadeStartAngle), 0.0f, 1.0f);
				}
			}
			
			if (ShapeFadeAlpha > 0)
			{
				const int32 OriginalNumSphereShapes = SphereShapeData.Num();
				const int32 OriginalNumCapsuleShapes = CapsuleShapeData.Num();

				TArray<const FPrimitiveSceneInfo*, SceneRenderingAllocator> ShadowGroupPrimitives;
				PrimitiveSceneInfo->GatherLightingAttachmentGroupPrimitives(ShadowGroupPrimitives);

				for (int32 ChildIndex = 0; ChildIndex < ShadowGroupPrimitives.Num(); ChildIndex++)
				{
					const FPrimitiveSceneInfo* GroupPrimitiveSceneInfo = ShadowGroupPrimitives[ChildIndex];

					if (GroupPrimitiveSceneInfo->Proxy->CastsDynamicShadow())
					{
						GroupPrimitiveSceneInfo->Proxy->GetShadowShapes(SphereShapeData, CapsuleShapeData);
					}
				}

				//@todo - remove entries with 0 fade alpha
				for (int32 ShapeIndex = OriginalNumSphereShapes; ShapeIndex < SphereShapeData.Num(); ShapeIndex++)
				{
					SphereShapeData[ShapeIndex].W *= ShapeFadeAlpha;
					SphereLightSourceData.Add(PackedLightDirection);
				}

				for (int32 ShapeIndex = OriginalNumCapsuleShapes; ShapeIndex < CapsuleShapeData.Num(); ShapeIndex++)
				{
					CapsuleShapeData[ShapeIndex].Radius *= ShapeFadeAlpha;
					CapsuleLightSourceData.Add(PackedLightDirection);
				}
			}
		}

		if (SphereShapeData.Num() > 0 || CapsuleShapeData.Num() > 0)
		{
			if (SphereShapeData.Num() > 0)
			{
				const int32 DataSize = SphereShapeData.Num() * SphereShapeData.GetTypeSize();

				if (!IsValidRef(View.ViewState->IndirectShadowSphereShapesVertexBuffer) || (int32)View.ViewState->IndirectShadowSphereShapesVertexBuffer->GetSize() < DataSize)
				{
					View.ViewState->IndirectShadowSphereShapesVertexBuffer.SafeRelease();
					View.ViewState->IndirectShadowSphereShapesSRV.SafeRelease();
					FRHIResourceCreateInfo CreateInfo;
					View.ViewState->IndirectShadowSphereShapesVertexBuffer = RHICreateVertexBuffer(DataSize, BUF_Volatile | BUF_ShaderResource, CreateInfo);
					View.ViewState->IndirectShadowSphereShapesSRV = RHICreateShaderResourceView(View.ViewState->IndirectShadowSphereShapesVertexBuffer, SphereShapeData.GetTypeSize(), PF_A32B32G32R32F);
				}

				FSphere* SphereShapeLockedData = (FSphere*)RHILockVertexBuffer(View.ViewState->IndirectShadowSphereShapesVertexBuffer, 0, DataSize, RLM_WriteOnly);
				FPlatformMemory::Memcpy(SphereShapeLockedData, SphereShapeData.GetData(), DataSize);
				RHIUnlockVertexBuffer(View.ViewState->IndirectShadowSphereShapesVertexBuffer);
			}
			
			if (CapsuleShapeData.Num() > 0)
			{
				static_assert(sizeof(FCapsuleShape) == sizeof(FVector4) * 2, "FCapsuleShape has padding");
				const int32 DataSize = CapsuleShapeData.Num() * CapsuleShapeData.GetTypeSize();

				if (!IsValidRef(View.ViewState->IndirectShadowCapsuleShapesVertexBuffer) || (int32)View.ViewState->IndirectShadowCapsuleShapesVertexBuffer->GetSize() < DataSize)
				{
					View.ViewState->IndirectShadowCapsuleShapesVertexBuffer.SafeRelease();
					View.ViewState->IndirectShadowCapsuleShapesSRV.SafeRelease();
					FRHIResourceCreateInfo CreateInfo;
					View.ViewState->IndirectShadowCapsuleShapesVertexBuffer = RHICreateVertexBuffer(DataSize, BUF_Volatile | BUF_ShaderResource, CreateInfo);
					View.ViewState->IndirectShadowCapsuleShapesSRV = RHICreateShaderResourceView(View.ViewState->IndirectShadowCapsuleShapesVertexBuffer, sizeof(FVector4), PF_A32B32G32R32F);
				}

				void* CapsuleShapeLockedData = RHILockVertexBuffer(View.ViewState->IndirectShadowCapsuleShapesVertexBuffer, 0, DataSize, RLM_WriteOnly);
				FPlatformMemory::Memcpy(CapsuleShapeLockedData, CapsuleShapeData.GetData(), DataSize);
				RHIUnlockVertexBuffer(View.ViewState->IndirectShadowCapsuleShapesVertexBuffer);
			}

			{
				const int32 DataSize = SphereLightSourceData.Num() * SphereLightSourceData.GetTypeSize() + CapsuleLightSourceData.Num() * CapsuleLightSourceData.GetTypeSize();

				if (!IsValidRef(View.ViewState->IndirectShadowLightDirectionVertexBuffer) || (int32)View.ViewState->IndirectShadowLightDirectionVertexBuffer->GetSize() < DataSize)
				{
					View.ViewState->IndirectShadowLightDirectionVertexBuffer.SafeRelease();
					View.ViewState->IndirectShadowLightDirectionSRV.SafeRelease();
					FRHIResourceCreateInfo CreateInfo;
					View.ViewState->IndirectShadowLightDirectionVertexBuffer = RHICreateVertexBuffer(DataSize, BUF_Volatile | BUF_ShaderResource, CreateInfo);
					View.ViewState->IndirectShadowLightDirectionSRV = RHICreateShaderResourceView(View.ViewState->IndirectShadowLightDirectionVertexBuffer, sizeof(FVector4), PF_A32B32G32R32F);
				}

				FVector4* LightDirectionLockedData = (FVector4*)RHILockVertexBuffer(View.ViewState->IndirectShadowLightDirectionVertexBuffer, 0, DataSize, RLM_WriteOnly);
				FPlatformMemory::Memcpy(LightDirectionLockedData, SphereLightSourceData.GetData(), SphereLightSourceData.Num() * SphereLightSourceData.GetTypeSize());
				FPlatformMemory::Memcpy(LightDirectionLockedData + SphereLightSourceData.Num(), CapsuleLightSourceData.GetData(), CapsuleLightSourceData.Num() * CapsuleLightSourceData.GetTypeSize());
				RHIUnlockVertexBuffer(View.ViewState->IndirectShadowLightDirectionVertexBuffer);
			}

			FSceneRenderTargets::Get(RHICmdList).FinishRenderingSceneColor(RHICmdList);
			SetRenderTarget(RHICmdList, NULL, NULL);

			TRefCountPtr<IPooledRenderTarget> RayTracedShadowsRT;

			{
				const FIntPoint BufferSize = GetBufferSizeForCapsuleShadows();
				FPooledRenderTargetDesc Desc(FPooledRenderTargetDesc::Create2DDesc(BufferSize, PF_G16R16F, FClearValueBinding::None, TexCreate_None, TexCreate_RenderTargetable | TexCreate_UAV, false));
				GRenderTargetPool.FindFreeElement(RHICmdList, Desc, RayTracedShadowsRT, TEXT("RayTracedShadows"));
			}

			FIntRect ScissorRect = View.ViewRect;

			{
				uint32 GroupSizeX = FMath::DivideAndRoundUp(ScissorRect.Size().X / GetCapsuleShadowDownsampleFactor(), GShadowShapeTileSize);
				uint32 GroupSizeY = FMath::DivideAndRoundUp(ScissorRect.Size().Y / GetCapsuleShadowDownsampleFactor(), GShadowShapeTileSize);

				{
					SCOPED_DRAW_EVENT(RHICmdList, TiledCapsuleShadowing);

					FSceneRenderTargetItem& RayTracedShadowsRTI = RayTracedShadowsRT->GetRenderTargetItem();
					{
						TShaderMapRef<TCapsuleShadowingCS<ShapeShadow_IndirectTiledCulling> > ComputeShader(View.ShaderMap);
						RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());

						ComputeShader->SetParameters(
							RHICmdList, 
							View, 
							NULL, 
							RayTracedShadowsRTI, 
							FVector2D(GroupSizeX, GroupSizeY), 
							GCapsuleMaxIndirectOcclusionDistance, 
							ScissorRect, 
							SphereShapeData.Num(), 
							View.ViewState->IndirectShadowSphereShapesSRV.GetReference(), 
							CapsuleShapeData.Num(), 
							View.ViewState->IndirectShadowCapsuleShapesSRV.GetReference(),
							View.ViewState->IndirectShadowLightDirectionSRV);

						DispatchComputeShader(RHICmdList, *ComputeShader, GroupSizeX, GroupSizeY, 1);
						ComputeShader->UnsetParameters(RHICmdList, RayTracedShadowsRTI);
					}
				}
			}

			
			{
				FSceneRenderTargets::Get(RHICmdList).BeginRenderingSceneColor(RHICmdList, ESimpleRenderTargetMode::EExistingColorAndDepth, FExclusiveDepthStencil::DepthRead_StencilNop);

				SCOPED_DRAW_EVENT(RHICmdList, Upsample);

				RHICmdList.SetViewport(ScissorRect.Min.X, ScissorRect.Min.Y, 0.0f, ScissorRect.Max.X, ScissorRect.Max.Y, 1.0f);
				RHICmdList.SetRasterizerState(TStaticRasterizerState<FM_Solid, CM_None>::GetRHI());
				RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());
				// Modulative blending
				RHICmdList.SetBlendState(TStaticBlendState<CW_RGB, BO_Add, BF_DestColor, BF_Zero>::GetRHI());

				TShaderMapRef<FPostProcessVS> VertexShader(View.ShaderMap);

				if (GCapsuleShadowsFullResolution)
				{
					TShaderMapRef<TCapsuleShadowingUpsamplePS<false> > PixelShader(View.ShaderMap);

					static FGlobalBoundShaderState BoundShaderState;
					SetGlobalBoundShaderState(RHICmdList, View.GetFeatureLevel(), BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);

					VertexShader->SetParameters(RHICmdList, View);
					PixelShader->SetParameters(RHICmdList, View, ScissorRect, RayTracedShadowsRT, false);
				}
				else
				{
					TShaderMapRef<TCapsuleShadowingUpsamplePS<true> > PixelShader(View.ShaderMap);

					static FGlobalBoundShaderState BoundShaderState;
					SetGlobalBoundShaderState(RHICmdList, View.GetFeatureLevel(), BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);

					VertexShader->SetParameters(RHICmdList, View);
					PixelShader->SetParameters(RHICmdList, View, ScissorRect, RayTracedShadowsRT, false);
				}

				DrawRectangle( 
					RHICmdList,
					0, 0, 
					ScissorRect.Width(), ScissorRect.Height(),
					ScissorRect.Min.X / GetCapsuleShadowDownsampleFactor(), ScissorRect.Min.Y / GetCapsuleShadowDownsampleFactor(), 
					ScissorRect.Width() / GetCapsuleShadowDownsampleFactor(), ScissorRect.Height() / GetCapsuleShadowDownsampleFactor(),
					FIntPoint(ScissorRect.Width(), ScissorRect.Height()),
					GetBufferSizeForCapsuleShadows(),
					*VertexShader);
			}
		}
	}
}