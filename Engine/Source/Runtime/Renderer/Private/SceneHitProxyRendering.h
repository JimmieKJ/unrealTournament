// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	SceneHitProxyRendering.h: Scene hit proxy rendering.
=============================================================================*/

#pragma once

/**
 * Outputs no color, but can be used to write the mesh's depth values to the depth buffer.
 */
class FHitProxyDrawingPolicy : public FMeshDrawingPolicy
{
public:

	typedef FHitProxyId ElementDataType;

	FHitProxyDrawingPolicy(
		const FVertexFactory* InVertexFactory,
		const FMaterialRenderProxy* InMaterialRenderProxy,
		ERHIFeatureLevel::Type InFeatureLevel
		);

	// FMeshDrawingPolicy interface.
	bool Matches(const FHitProxyDrawingPolicy& Other) const
	{
		return FMeshDrawingPolicy::Matches(Other) &&
			HullShader == Other.HullShader &&
			DomainShader == Other.DomainShader &&
			VertexShader == Other.VertexShader &&
			PixelShader == Other.PixelShader;
	}
	void SetSharedState(FRHICommandList& RHICmdList, const FSceneView* View, const ContextDataType PolicyContext) const;

	/** 
	* Create bound shader state using the vertex decl from the mesh draw policy
	* as well as the shaders needed to draw the mesh
	* @return new bound shader state object
	*/
	FBoundShaderStateInput GetBoundShaderStateInput(ERHIFeatureLevel::Type InFeatureLevel);

	void SetMeshRenderState(
		FRHICommandList& RHICmdList, 
		const FSceneView& View,
		const FPrimitiveSceneProxy* PrimitiveSceneProxy,
		const FMeshBatch& Mesh,
		int32 BatchElementIndex,
		bool bBackFace,
		float DitheredLODTransitionValue,
		const FHitProxyId HitProxyId,
		const ContextDataType PolicyContext
		) const;

private:
	class FHitProxyVS* VertexShader;
	class FHitProxyPS* PixelShader;
	class FHitProxyHS* HullShader;
	class FHitProxyDS* DomainShader;
};

/**
 * A drawing policy factory for the hit proxy drawing policy.
 */
class FHitProxyDrawingPolicyFactory
{
public:

	enum { bAllowSimpleElements = true };
	struct ContextType {};

	static void AddStaticMesh(FScene* Scene,FStaticMesh* StaticMesh,ContextType DrawingContext = ContextType());
	static bool DrawDynamicMesh(
		FRHICommandList& RHICmdList, 
		const FSceneView& View,
		ContextType DrawingContext,
		const FMeshBatch& Mesh,
		bool bBackFace,
		bool bPreFog,
		const FPrimitiveSceneProxy* PrimitiveSceneProxy,
		FHitProxyId HitProxyId
		);
};
