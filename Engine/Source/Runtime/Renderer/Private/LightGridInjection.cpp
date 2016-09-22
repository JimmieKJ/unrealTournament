// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	LightGridInjection.cpp
=============================================================================*/

#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "SceneUtils.h"

int32 GLightGridPixelSize = 64;
FAutoConsoleVariableRef CVarLightGridPixelSize(
	TEXT("r.Forward.LightGridPixelSize"),
	GLightGridPixelSize,
	TEXT("Size of a cell in the light grid, in pixels."),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

int32 GLightGridSizeZ = 32;
FAutoConsoleVariableRef CVarLightGridSizeZ(
	TEXT("r.Forward.LightGridSizeZ"),
	GLightGridSizeZ,
	TEXT("Number of Z slices in the light grid."),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

int32 GMaxCulledLightsPerCell = 32;
FAutoConsoleVariableRef CVarMaxCulledLightsPerCell(
	TEXT("r.Forward.MaxCulledLightsPerCell"),
	GMaxCulledLightsPerCell,
	TEXT("Controls how much memory is allocated for each cell for light culling.  When r.Forward.LightLinkedListCulling is enabled, this is used to compute a global max instead of a per-cell limit on culled lights."),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

int32 GLightLinkedListCulling = 1;
FAutoConsoleVariableRef CVarLightLinkedListCulling(
	TEXT("r.Forward.LightLinkedListCulling"),
	GLightLinkedListCulling,
	TEXT("Uses a reverse linked list to store culled lights, removing the fixed limit on how many lights can affect a cell - it becomes a global limit instead."),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

IMPLEMENT_UNIFORM_BUFFER_STRUCT(FForwardGlobalLightData,TEXT("ForwardGlobalLightData"));

FForwardGlobalLightData::FForwardGlobalLightData()
{
	NumLocalLights = 0;
	HasDirectionalLight = 0;
}

int32 NumCulledLightsGridStride = 2;
int32 NumCulledGridPrimitiveTypes = 2;
int32 LightLinkStride = 2;
// 65k indexable light limit
typedef uint16 FLightIndexType;

/**  */
class FForwardCullingParameters
{
public:

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("LIGHT_LINK_STRIDE"), LightLinkStride);
	}

	void Bind(const FShaderParameterMap& ParameterMap)
	{
		NextCulledLightLink.Bind(ParameterMap, TEXT("NextCulledLightLink"));
		StartOffsetGrid.Bind(ParameterMap, TEXT("StartOffsetGrid"));
		CulledLightLinks.Bind(ParameterMap, TEXT("CulledLightLinks"));
		NextCulledLightData.Bind(ParameterMap, TEXT("NextCulledLightData"));
	}

	template<typename ShaderRHIParamRef>
	void Set(FRHICommandList& RHICmdList, const ShaderRHIParamRef& ShaderRHI, const FViewInfo& View)
	{
		NextCulledLightLink.SetBuffer(RHICmdList, ShaderRHI, View.ForwardLightingResources->NextCulledLightLink);
		StartOffsetGrid.SetBuffer(RHICmdList, ShaderRHI, View.ForwardLightingResources->StartOffsetGrid);
		CulledLightLinks.SetBuffer(RHICmdList, ShaderRHI, View.ForwardLightingResources->CulledLightLinks);
		NextCulledLightData.SetBuffer(RHICmdList, ShaderRHI, View.ForwardLightingResources->NextCulledLightData);
	}

	template<typename ShaderRHIParamRef>
	void UnsetParameters(FRHICommandList& RHICmdList, const ShaderRHIParamRef& ShaderRHI, const FViewInfo& View)
	{
		NextCulledLightLink.UnsetUAV(RHICmdList, ShaderRHI);
		StartOffsetGrid.UnsetUAV(RHICmdList, ShaderRHI);
		CulledLightLinks.UnsetUAV(RHICmdList, ShaderRHI);
		NextCulledLightData.UnsetUAV(RHICmdList, ShaderRHI);

		TArray<FUnorderedAccessViewRHIParamRef, TInlineAllocator<4>> OutUAVs;

		if (NextCulledLightLink.IsBound())
		{
			OutUAVs.Add(View.ForwardLightingResources->NextCulledLightLink.UAV);
		}

		if (StartOffsetGrid.IsBound())
		{
			OutUAVs.Add(View.ForwardLightingResources->StartOffsetGrid.UAV);
		}

		if (CulledLightLinks.IsBound())
		{
			OutUAVs.Add(View.ForwardLightingResources->CulledLightLinks.UAV);
		}

		if (NextCulledLightData.IsBound())
		{
			OutUAVs.Add(View.ForwardLightingResources->NextCulledLightData.UAV);
		}

		if (OutUAVs.Num() > 0)
		{
			RHICmdList.TransitionResources(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToGfx, OutUAVs.GetData(), OutUAVs.Num());
		}
	}

	/** Serializer. */
	friend FArchive& operator<<(FArchive& Ar,FForwardCullingParameters& P)
	{
		Ar << P.NextCulledLightLink;
		Ar << P.StartOffsetGrid;
		Ar << P.CulledLightLinks;
		Ar << P.NextCulledLightData;
		return Ar;
	}

private:

	FRWShaderParameter NextCulledLightLink;
	FRWShaderParameter StartOffsetGrid;
	FRWShaderParameter CulledLightLinks;
	FRWShaderParameter NextCulledLightData;
};


uint32 LightGridInjectionGroupSize = 4;

template<bool bLightLinkedListCulling>
class TLightGridInjectionCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(TLightGridInjectionCS,Global)
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform,OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), LightGridInjectionGroupSize);
		FForwardLightingParameters::ModifyCompilationEnvironment(Platform, OutEnvironment);
		FForwardCullingParameters::ModifyCompilationEnvironment(Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("USE_LINKED_CULL_LIST"), bLightLinkedListCulling);
	}

	TLightGridInjectionCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		ForwardLightingParameters.Bind(Initializer.ParameterMap);
		ForwardCullingParameters.Bind(Initializer.ParameterMap);
	}

	TLightGridInjectionCS()
	{
	}

	void SetParameters(FRHICommandList& RHICmdList, const FViewInfo& View)
	{
		FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();
		FGlobalShader::SetParameters(RHICmdList, ShaderRHI, View);
		ForwardLightingParameters.Set(RHICmdList, ShaderRHI, View);
		ForwardCullingParameters.Set(RHICmdList, ShaderRHI, View);
	}

	void UnsetParameters(FRHICommandList& RHICmdList, const FViewInfo& View)
	{
		ForwardLightingParameters.UnsetParameters(RHICmdList, GetComputeShader(), View);
		ForwardCullingParameters.UnsetParameters(RHICmdList, GetComputeShader(), View);
	}

	virtual bool Serialize(FArchive& Ar)
	{		
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << ForwardLightingParameters;
		Ar << ForwardCullingParameters;
		return bShaderHasOutdatedParameters;
	}

private:

	FForwardLightingParameters ForwardLightingParameters;
	FForwardCullingParameters ForwardCullingParameters;
};

IMPLEMENT_SHADER_TYPE(template<>,TLightGridInjectionCS<true>,TEXT("LightGridInjection"),TEXT("LightGridInjectionCS"),SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>,TLightGridInjectionCS<false>,TEXT("LightGridInjection"),TEXT("LightGridInjectionCS"),SF_Compute);

class FLightGridCompactCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FLightGridCompactCS,Global)
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform,OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), LightGridInjectionGroupSize);
		FForwardLightingParameters::ModifyCompilationEnvironment(Platform, OutEnvironment);
		FForwardCullingParameters::ModifyCompilationEnvironment(Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("MAX_CAPTURES"), GMaxNumReflectionCaptures);
	}

	FLightGridCompactCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		ForwardLightingParameters.Bind(Initializer.ParameterMap);
		ForwardCullingParameters.Bind(Initializer.ParameterMap);
	}

	FLightGridCompactCS()
	{
	}

	void SetParameters(FRHICommandList& RHICmdList, const FViewInfo& View)
	{
		FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();
		FGlobalShader::SetParameters(RHICmdList, ShaderRHI, View);
		ForwardLightingParameters.Set(RHICmdList, ShaderRHI, View);
		ForwardCullingParameters.Set(RHICmdList, ShaderRHI, View);
	}

	void UnsetParameters(FRHICommandList& RHICmdList, const FViewInfo& View)
	{
		ForwardLightingParameters.UnsetParameters(RHICmdList, GetComputeShader(), View);
		ForwardCullingParameters.UnsetParameters(RHICmdList, GetComputeShader(), View);
	}

	virtual bool Serialize(FArchive& Ar)
	{		
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << ForwardLightingParameters;
		Ar << ForwardCullingParameters;
		return bShaderHasOutdatedParameters;
	}

private:

	FForwardLightingParameters ForwardLightingParameters;
	FForwardCullingParameters ForwardCullingParameters;
};

IMPLEMENT_SHADER_TYPE(,FLightGridCompactCS,TEXT("LightGridInjection"),TEXT("LightGridCompactCS"),SF_Compute);

FVector GetLightGridZParams(float NearPlane, float FarPlane)
{
	// S = distribution scale
	// B, O are solved for given the z distances of the first+last slice, and the # of slices.
	//
	// slice = log2(z*B + O) * S

	// Don't spend lots of resolution right in front of the near plane
	double NearOffset = .095 * 100;
	// Space out the slices so they aren't all clustered at the near plane
	double S = 4.05;

	double N = NearPlane + NearOffset;
	double F = FarPlane;

	double O = (F - N * exp2((GLightGridSizeZ - 1) / S)) / (F - N);
	double B = (1 - O) / N;

	return FVector(B, O, S);
}

void FDeferredShadingSceneRenderer::ComputeLightGrid(FRHICommandListImmediate& RHICmdList)
{
	if (FeatureLevel >= ERHIFeatureLevel::SM5)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_ComputeLightGrid);
		SCOPED_DRAW_EVENT(RHICmdList, ComputeLightGrid);

		static const auto AllowStaticLightingVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.AllowStaticLighting"));
		const bool bAllowStaticLighting = (!AllowStaticLightingVar || AllowStaticLightingVar->GetValueOnRenderThread() != 0);

		bool bAnyViewUsesTranslucentSurfaceLighting = false;

		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			const FViewInfo& View = Views[ViewIndex];
			bAnyViewUsesTranslucentSurfaceLighting |= View.bTranslucentSurfaceLighting;
		}

		const bool bCullLightsToGrid = IsForwardShadingEnabled(FeatureLevel) || bAnyViewUsesTranslucentSurfaceLighting;

		FSimpleLightArray SimpleLights;

		if (bCullLightsToGrid)
		{
			GatherSimpleLights(ViewFamily, Views, SimpleLights);
		}

		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			FViewInfo& View = Views[ViewIndex];

			FForwardGlobalLightData GlobalLightData;
			TArray<FForwardLocalLightData, SceneRenderingAllocator> ForwardLocalLightData;
			float FurthestLight = 1000;

			if (bCullLightsToGrid)
			{
				ForwardLocalLightData.Empty(Scene->Lights.Num());

				for (TSparseArray<FLightSceneInfoCompact>::TConstIterator LightIt(Scene->Lights); LightIt; ++LightIt)
				{
					const FLightSceneInfoCompact& LightSceneInfoCompact = *LightIt;
					const FLightSceneInfo* const LightSceneInfo = LightSceneInfoCompact.LightSceneInfo;

					if (LightSceneInfo->ShouldRenderLightViewIndependent()
						&& LightSceneInfo->ShouldRenderLight(View)
						// Reflection override skips direct specular because it tends to be blindingly bright with a perfectly smooth surface
						&& !ViewFamily.EngineShowFlags.ReflectionOverride)
					{
						FVector4 LightPositionAndInvRadius;
						FVector4 LightColorAndFalloffExponent;
						FVector NormalizedLightDirection;
						FVector2D SpotAngles;
						float SourceRadius;
						float SourceLength;
						float MinRoughness;

						// Get the light parameters
						LightSceneInfo->Proxy->GetParameters(
							LightPositionAndInvRadius,
							LightColorAndFalloffExponent,
							NormalizedLightDirection,
							SpotAngles,
							SourceRadius,
							SourceLength,
							MinRoughness);

						if (LightSceneInfo->Proxy->IsInverseSquared())
						{
							// Correction for lumen units
							LightColorAndFalloffExponent.X *= 16.0f;
							LightColorAndFalloffExponent.Y *= 16.0f;
							LightColorAndFalloffExponent.Z *= 16.0f;
							LightColorAndFalloffExponent.W = 0;
						}

						// When rendering reflection captures, the direct lighting of the light is actually the indirect specular from the main view
						if (View.bIsReflectionCapture)
						{
							LightColorAndFalloffExponent.X *= LightSceneInfo->Proxy->GetIndirectLightingScale();
							LightColorAndFalloffExponent.Y *= LightSceneInfo->Proxy->GetIndirectLightingScale();
							LightColorAndFalloffExponent.Z *= LightSceneInfo->Proxy->GetIndirectLightingScale();
						}

						int32 ShadowMapChannel = LightSceneInfo->Proxy->GetShadowMapChannel();
						int32 PreviewShadowMapChannel = LightSceneInfo->Proxy->GetPreviewShadowMapChannel();

						if (!bAllowStaticLighting)
						{
							ShadowMapChannel = INDEX_NONE;
							PreviewShadowMapChannel = INDEX_NONE;
						}

						// Static shadowing uses ShadowMapChannel, dynamic shadows are packed into light attenuation using PreviewShadowMapChannel
						const uint32 ShadowMapChannelMaskPacked =
							(ShadowMapChannel == 0 ? 1 : 0) |
							(ShadowMapChannel == 1 ? 2 : 0) |
							(ShadowMapChannel == 2 ? 4 : 0) |
							(ShadowMapChannel == 3 ? 8 : 0) |
							(PreviewShadowMapChannel == 0 ? 16 : 0) |
							(PreviewShadowMapChannel == 1 ? 32 : 0) |
							(PreviewShadowMapChannel == 2 ? 64 : 0) |
							(PreviewShadowMapChannel == 3 ? 128 : 0);

						if (LightSceneInfoCompact.LightType == LightType_Point || LightSceneInfoCompact.LightType == LightType_Spot)
						{
							ForwardLocalLightData.AddUninitialized(1);
							FForwardLocalLightData& LightData = ForwardLocalLightData.Last();

							const float LightFade = GetLightFadeFactor(View, LightSceneInfo->Proxy);
							LightColorAndFalloffExponent.X *= LightFade;
							LightColorAndFalloffExponent.Y *= LightFade;
							LightColorAndFalloffExponent.Z *= LightFade;

							LightData.LightPositionAndInvRadius = LightPositionAndInvRadius;
							LightData.LightColorAndFalloffExponent = LightColorAndFalloffExponent;
							LightData.LightDirectionAndShadowMapChannelMask = FVector4(NormalizedLightDirection, *((float*)&ShadowMapChannelMaskPacked));
							LightData.SpotAnglesAndSourceRadius = FVector4(SpotAngles.X, SpotAngles.Y, SourceRadius, SourceLength);

							const FSphere BoundingSphere = LightSceneInfo->Proxy->GetBoundingSphere();
							const float Distance = View.ViewMatrices.ViewMatrix.TransformPosition(BoundingSphere.Center).Z + BoundingSphere.W;
							FurthestLight = FMath::Max(FurthestLight, Distance);
						}
						else if (LightSceneInfoCompact.LightType == LightType_Directional)
						{
							GlobalLightData.HasDirectionalLight = 1;
							GlobalLightData.DirectionalLightColor = LightColorAndFalloffExponent;
							GlobalLightData.DirectionalLightDirection = NormalizedLightDirection;
							GlobalLightData.DirectionalLightShadowMapChannelMask = ShadowMapChannelMaskPacked;

							const FVector2D FadeParams = LightSceneInfo->Proxy->GetDirectionalLightDistanceFadeParameters(View.GetFeatureLevel(), LightSceneInfo->IsPrecomputedLightingValid());

							GlobalLightData.DirectionalLightDistanceFadeMAD = FVector2D(FadeParams.Y, -FadeParams.X * FadeParams.Y);
						}
					}
				}

				for (int32 SimpleLightIndex = 0; SimpleLightIndex < SimpleLights.InstanceData.Num(); SimpleLightIndex++)
				{	
					ForwardLocalLightData.AddUninitialized(1);
					FForwardLocalLightData& LightData = ForwardLocalLightData.Last();

					const FSimpleLightEntry& SimpleLight = SimpleLights.InstanceData[SimpleLightIndex];
					const FSimpleLightPerViewEntry& SimpleLightPerViewData = SimpleLights.GetViewDependentData(SimpleLightIndex, ViewIndex, Views.Num());
					LightData.LightPositionAndInvRadius = FVector4(SimpleLightPerViewData.Position, 1.0f / FMath::Max(SimpleLight.Radius, KINDA_SMALL_NUMBER));
					LightData.LightColorAndFalloffExponent = FVector4(SimpleLight.Color, SimpleLight.Exponent);

					uint32 ShadowMapChannelMask = 0;
					LightData.LightDirectionAndShadowMapChannelMask = FVector4(FVector(1, 0, 0), *((float*)&ShadowMapChannelMask));
					LightData.SpotAnglesAndSourceRadius = FVector4(-2, 1, 0, 0);

					if( SimpleLight.Exponent == 0.0f )
					{
						// Correction for lumen units
						LightData.LightColorAndFalloffExponent *= 16.0f;
					}
				}
			}

			// Store off the number of lights before we add a fake entry
			const int32 NumLocalLightsFinal = ForwardLocalLightData.Num();

			if (ForwardLocalLightData.Num() == 0)
			{
				// Make sure the buffer gets created even though we're not going to read from it in the shader, for platforms like PS4 that assert on null resources being bound
				ForwardLocalLightData.AddZeroed();
			}

			{
				const uint32 NumBytesRequired = ForwardLocalLightData.Num() * ForwardLocalLightData.GetTypeSize();

				if (View.ForwardLightingResources->ForwardLocalLightBuffer.NumBytes < NumBytesRequired)
				{
					View.ForwardLightingResources->ForwardLocalLightBuffer.Release();
					View.ForwardLightingResources->ForwardLocalLightBuffer.Initialize(sizeof(FVector4), NumBytesRequired / sizeof(FVector4), PF_A32B32G32R32F, BUF_Volatile);
				}

				View.ForwardLightingResources->ForwardLocalLightBuffer.Lock();
				FPlatformMemory::Memcpy(View.ForwardLightingResources->ForwardLocalLightBuffer.MappedBuffer, ForwardLocalLightData.GetData(), ForwardLocalLightData.Num() * ForwardLocalLightData.GetTypeSize());
				View.ForwardLightingResources->ForwardLocalLightBuffer.Unlock();
			}

			const FIntPoint LightGridSizeXY = FIntPoint::DivideAndRoundUp(View.ViewRect.Size(), GLightGridPixelSize);
			GlobalLightData.NumLocalLights = NumLocalLightsFinal;
			GlobalLightData.NumReflectionCaptures = View.NumBoxReflectionCaptures + View.NumSphereReflectionCaptures;
			GlobalLightData.NumGridCells = LightGridSizeXY.X * LightGridSizeXY.Y * GLightGridSizeZ;
			GlobalLightData.CulledGridSize = FIntVector(LightGridSizeXY.X, LightGridSizeXY.Y, GLightGridSizeZ);
			GlobalLightData.MaxCulledLightsPerCell = GMaxCulledLightsPerCell;
			GlobalLightData.LightGridPixelSizeShift = FMath::FloorLog2(GLightGridPixelSize);

			// Clamp far plane to something reasonable
			float FarPlane = FMath::Min(FMath::Max(FurthestLight, View.FurthestReflectionCaptureDistance), (float)HALF_WORLD_MAX / 5.0f);
			FVector ZParams = GetLightGridZParams(View.NearClippingDistance, FarPlane + 10.f);
			GlobalLightData.LightGridZParams = ZParams;

			const uint32 NumIndexableLights = 1 << (sizeof(FLightIndexType) * 8);

			if ((uint32)ForwardLocalLightData.Num() > NumIndexableLights)
			{
				static bool bWarned = false;

				if (!bWarned)
				{
					UE_LOG(LogRenderer, Warning, TEXT("Exceeded indexable light count, glitches will be visible (%u / %u)"), ForwardLocalLightData.Num(), NumIndexableLights);
					bWarned = true;
				}
			}

			View.ForwardLightingResources->ForwardGlobalLightData = TUniformBufferRef<FForwardGlobalLightData>::CreateUniformBufferImmediate(GlobalLightData, UniformBuffer_SingleFrame);
		}

		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			FViewInfo& View = Views[ViewIndex];

			const FIntPoint LightGridSizeXY = FIntPoint::DivideAndRoundUp(View.ViewRect.Size(), GLightGridPixelSize);
			const int32 NumCells = LightGridSizeXY.X * LightGridSizeXY.Y * GLightGridSizeZ * NumCulledGridPrimitiveTypes;

			if (View.ForwardLightingResources->NumCulledLightsGrid.NumBytes != NumCells * NumCulledLightsGridStride * sizeof(uint32))
			{
				View.ForwardLightingResources->NumCulledLightsGrid.Initialize(sizeof(uint32), NumCells * NumCulledLightsGridStride, PF_R32_UINT);
				View.ForwardLightingResources->NextCulledLightLink.Initialize(sizeof(uint32), 1, PF_R32_UINT);
				View.ForwardLightingResources->StartOffsetGrid.Initialize(sizeof(uint32), NumCells, PF_R32_UINT);
				View.ForwardLightingResources->NextCulledLightData.Initialize(sizeof(uint32), 1, PF_R32_UINT);
			}

			if (View.ForwardLightingResources->CulledLightDataGrid.NumBytes != NumCells * GMaxCulledLightsPerCell * sizeof(FLightIndexType))
			{
				View.ForwardLightingResources->CulledLightDataGrid.Initialize(sizeof(FLightIndexType), NumCells * GMaxCulledLightsPerCell, sizeof(FLightIndexType) == sizeof(uint16) ? PF_R16_UINT : PF_R32_UINT);
				View.ForwardLightingResources->CulledLightLinks.Initialize(sizeof(uint32), NumCells * GMaxCulledLightsPerCell * LightLinkStride, PF_R32_UINT);
			}

			const FIntVector NumGroups = FIntVector::DivideAndRoundUp(FIntVector(LightGridSizeXY.X, LightGridSizeXY.Y, GLightGridSizeZ), LightGridInjectionGroupSize);

			{
				SCOPED_DRAW_EVENT(RHICmdList, CullLights);

				TArray<FUnorderedAccessViewRHIParamRef, TInlineAllocator<6>> OutUAVs;
				OutUAVs.Add(View.ForwardLightingResources->NumCulledLightsGrid.UAV);
				OutUAVs.Add(View.ForwardLightingResources->CulledLightDataGrid.UAV);
				OutUAVs.Add(View.ForwardLightingResources->NextCulledLightLink.UAV);
				OutUAVs.Add(View.ForwardLightingResources->StartOffsetGrid.UAV);
				OutUAVs.Add(View.ForwardLightingResources->CulledLightLinks.UAV);
				OutUAVs.Add(View.ForwardLightingResources->NextCulledLightData.UAV);
				RHICmdList.TransitionResources(EResourceTransitionAccess::EWritable, EResourceTransitionPipeline::EGfxToCompute, OutUAVs.GetData(), OutUAVs.Num());

				if (GLightLinkedListCulling)
				{
					uint32 LinkClearValues[4] = { 0xFFFFFFFF };
					RHICmdList.ClearUAV(View.ForwardLightingResources->StartOffsetGrid.UAV, LinkClearValues);

					uint32 ClearValues[4] = { 0 };
					RHICmdList.ClearUAV(View.ForwardLightingResources->NextCulledLightLink.UAV, ClearValues);
					RHICmdList.ClearUAV(View.ForwardLightingResources->NextCulledLightData.UAV, ClearValues);

					TShaderMapRef<TLightGridInjectionCS<true> > ComputeShader(View.ShaderMap);
					RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());
					ComputeShader->SetParameters(RHICmdList, View);
					DispatchComputeShader(RHICmdList, *ComputeShader, NumGroups.X, NumGroups.Y, NumGroups.Z);
					ComputeShader->UnsetParameters(RHICmdList, View);
				}
				else
				{
					uint32 ClearValues[4] = { 0 };
					RHICmdList.ClearUAV(View.ForwardLightingResources->NumCulledLightsGrid.UAV, ClearValues);

					TShaderMapRef<TLightGridInjectionCS<false> > ComputeShader(View.ShaderMap);
					RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());
					ComputeShader->SetParameters(RHICmdList, View);
					DispatchComputeShader(RHICmdList, *ComputeShader, NumGroups.X, NumGroups.Y, NumGroups.Z);
					ComputeShader->UnsetParameters(RHICmdList, View);
				}
			}

			if (GLightLinkedListCulling)
			{
				SCOPED_DRAW_EVENT(RHICmdList, Compact);

				TShaderMapRef<FLightGridCompactCS> ComputeShader(View.ShaderMap);
				RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());
				ComputeShader->SetParameters(RHICmdList, View);
				DispatchComputeShader(RHICmdList, *ComputeShader, NumGroups.X, NumGroups.Y, NumGroups.Z);
				ComputeShader->UnsetParameters(RHICmdList, View);
			}
		}
	}
}

void FDeferredShadingSceneRenderer::RenderForwardShadingShadowProjections(FRHICommandListImmediate& RHICmdList)
{
	bool bLightAttenuationNeeded = false;

	for (TSparseArray<FLightSceneInfoCompact>::TConstIterator LightIt(Scene->Lights); LightIt; ++LightIt)
	{
		const FLightSceneInfoCompact& LightSceneInfoCompact = *LightIt;
		const FLightSceneInfo* const LightSceneInfo = LightSceneInfoCompact.LightSceneInfo;
		const FVisibleLightInfo& VisibleLightInfo = VisibleLightInfos[LightSceneInfo->Id];

		bLightAttenuationNeeded = bLightAttenuationNeeded || VisibleLightInfo.ShadowsToProject.Num() > 0 || VisibleLightInfo.CapsuleShadowsToProject.Num() > 0;
	}

	FSceneRenderTargets& SceneRenderTargets = FSceneRenderTargets::Get(RHICmdList);
	SceneRenderTargets.SetLightAttenuationMode(bLightAttenuationNeeded);

	if (bLightAttenuationNeeded)
	{
		SCOPED_DRAW_EVENT(RHICmdList, ShadowProjectionOnOpaque);

		// All shadows render with min blending
		bool bClearToWhite = true;
		SceneRenderTargets.BeginRenderingLightAttenuation(RHICmdList, bClearToWhite);

		for (TSparseArray<FLightSceneInfoCompact>::TConstIterator LightIt(Scene->Lights); LightIt; ++LightIt)
		{
			const FLightSceneInfoCompact& LightSceneInfoCompact = *LightIt;
			const FLightSceneInfo* const LightSceneInfo = LightSceneInfoCompact.LightSceneInfo;
			FVisibleLightInfo& VisibleLightInfo = VisibleLightInfos[LightSceneInfo->Id];

			if (VisibleLightInfo.ShadowsToProject.Num() > 0)
			{
				FSceneRenderer::RenderShadowProjections(RHICmdList, LightSceneInfo, true, false);
			}
		
			RenderCapsuleDirectShadows(*LightSceneInfo, RHICmdList, VisibleLightInfo.CapsuleShadowsToProject, true);
		}

		SceneRenderTargets.FinishRenderingLightAttenuation(RHICmdList);
	}
}