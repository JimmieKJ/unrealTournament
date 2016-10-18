// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	DecalRenderingShared.h
=============================================================================*/

#pragma once

#include "DecalRenderingCommon.h"

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
	static FMatrix ComputeComponentToClipMatrix(const FViewInfo& View, const FMatrix& DecalComponentToWorld);
	static void SetVertexShaderOnly(FRHICommandList& RHICmdList, const FViewInfo& View, const FMatrix& FrustumComponentToClip);
	static void BuildVisibleDecalList(const FScene& Scene, const FViewInfo& View, EDecalRenderStage DecalRenderStage, FTransientDecalRenderDataList& OutVisibleDecals);
	static void SetShader(FRHICommandList& RHICmdList, const FViewInfo& View, const FTransientDecalRenderData& DecalData, const FMatrix& FrustumComponentToClip);
};
