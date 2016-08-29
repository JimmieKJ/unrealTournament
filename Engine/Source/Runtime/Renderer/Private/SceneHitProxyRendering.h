// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

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
	FDrawingPolicyMatchResult Matches(const FHitProxyDrawingPolicy& Other) const
	{
		DRAWING_POLICY_MATCH_BEGIN
			DRAWING_POLICY_MATCH(FMeshDrawingPolicy::Matches(Other)) &&
			DRAWING_POLICY_MATCH(HullShader == Other.HullShader) &&
			DRAWING_POLICY_MATCH(DomainShader == Other.DomainShader) &&
			DRAWING_POLICY_MATCH(VertexShader == Other.VertexShader) &&
			DRAWING_POLICY_MATCH(PixelShader == Other.PixelShader);
		DRAWING_POLICY_MATCH_END
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
		const FMeshDrawingRenderState& DrawRenderState,
		const FHitProxyId HitProxyId,
		const ContextDataType PolicyContext
		) const;

private:
	class FHitProxyVS* VertexShader;
	class FHitProxyPS* PixelShader;
	class FHitProxyHS* HullShader;
	class FHitProxyDS* DomainShader;
};

#if WITH_EDITOR
class FEditorSelectionDrawingPolicy : public FHitProxyDrawingPolicy
{
public:
	FEditorSelectionDrawingPolicy(const FVertexFactory* InVertexFactory, const FMaterialRenderProxy* InMaterialRenderProxy, ERHIFeatureLevel::Type InFeatureLevel)
		: FHitProxyDrawingPolicy(InVertexFactory, InMaterialRenderProxy, InFeatureLevel)
	{}

	// FMeshDrawingPolicy interface.
	void SetMeshRenderState( FRHICommandList& RHICmdList, const FSceneView& View, const FPrimitiveSceneProxy* PrimitiveSceneProxy, const FMeshBatch& Mesh, int32 BatchElementIndex, bool bBackFace, const FMeshDrawingRenderState& DrawRenderState, const FHitProxyId HitProxyId, const ContextDataType PolicyContext );

	/**
	 * Gets the value that should be written into the editor selection stencil buffer for a given primitive
	 * There will be a unique stencil value for each primitive until the max stencil buffer value is written and then the values will repeat
	 *
	 * @param View					The view being rendered
	 * @param PrimitiveSceneProxy	The scene proxy for the primitive being rendered
	 * @return the stencil value that should be written
	 */
	static int32 GetStencilValue(const FSceneView& View, const FPrimitiveSceneProxy* PrimitiveSceneProxy);

	/**
	 * Resets all unique stencil values
	 */
	static void ResetStencilValues();

private:
	/** Current selection index for a primitive */
	static int32 PrimitiveSelectionIndex;
	/** Current selection index used when components are selected directly */
	static int32 IndividuallySelectedProxyIndex;
};
#endif

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
