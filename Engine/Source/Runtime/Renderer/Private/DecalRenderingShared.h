// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	DecalRenderingShared.h
=============================================================================*/

#pragma once

// Actual values are used in the shader so do not change
enum EDecalRenderStage
{
	// for DBuffer decals (get proper baked lighting)
	DRS_BeforeBasePass = 0,
	// for volumetrics to update the depth buffer
	DRS_AfterBasePass = 1,
	// for normal decals not modifying the depth buffer
	DRS_BeforeLighting = 2,
	// for rendering decals in forward shading
	DRS_ForwardShading = 3,

	// later we could add "after lighting" and multiply
};

/**
 * Compact decal data for rendering
 */
struct FTransientDecalRenderData
{
	const FMaterialRenderProxy* MaterialProxy;
	const FMaterial* MaterialResource;
	const FDeferredDecalProxy* DecalProxy;
	float FadeAlpha;
	float ConservativeRadius;
	EDecalBlendMode DecalBlendMode;
	bool bHasNormal;

	FTransientDecalRenderData(const FScene& InScene, const FDeferredDecalProxy* InDecalProxy, float InConservativeRadius);
};
	
typedef TArray<FTransientDecalRenderData, SceneRenderingAllocator> FTransientDecalRenderDataList;

/**
 * Shared decal functionality for deferred and forward shading
 */
struct FDecalRendering
{
	enum ERenderTargetMode
	{
		RTM_Unknown = -1,
		RTM_SceneColorAndGBuffer,
		RTM_SceneColorAndGBufferDepthWrite,
		RTM_DBuffer,
		RTM_GBufferNormal,
		RTM_SceneColor,
	};
	
	static void BuildVisibleDecalList(const FScene& Scene, const FViewInfo& View, EDecalRenderStage DecalRenderStage, FTransientDecalRenderDataList& OutVisibleDecals);
	static FMatrix ComputeComponentToClipMatrix(const FViewInfo& View, const FMatrix& DecalComponentToWorld);
	static ERenderTargetMode ComputeRenderTargetMode(EShaderPlatform Platfrom, EDecalBlendMode DecalBlendMode);
	static EDecalRenderStage ComputeRenderStage(EShaderPlatform Platfrom, EDecalBlendMode DecalBlendMode);
	static uint32 ComputeRenderTargetCount(EShaderPlatform Platfrom, ERenderTargetMode RenderTargetMode);
	static void SetShader(FRHICommandList& RHICmdList, const FViewInfo& View, bool bShaderComplexity, const FTransientDecalRenderData& DecalData, const FMatrix& FrustumComponentToClip);
	static void SetVertexShaderOnly(FRHICommandList& RHICmdList, const FViewInfo& View, const FMatrix& FrustumComponentToClip);
};
