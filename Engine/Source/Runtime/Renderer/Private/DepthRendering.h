// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	DepthRendering.h: Depth rendering definitions.
=============================================================================*/

#pragma once

enum EDepthDrawingMode
{
	// tested at a higher level
	DDM_None,
	//
	DDM_NonMaskedOnly,
	//
	DDM_AllOccluders,
	//
	DDM_AllOpaque,
};

template<bool>
class TDepthOnlyVS;

class FDepthOnlyPS;

/**
 * Used to write out depth for opaque and masked materials during the depth-only pass.
 */
class FDepthDrawingPolicy : public FMeshDrawingPolicy
{
public:

	FDepthDrawingPolicy(
		const FVertexFactory* InVertexFactory,
		const FMaterialRenderProxy* InMaterialRenderProxy,
		const FMaterial& InMaterialResource,
		bool bIsTwoSided,
		ERHIFeatureLevel::Type InFeatureLevel
		);

	// FMeshDrawingPolicy interface.
	bool Matches(const FDepthDrawingPolicy& Other) const
	{
		return FMeshDrawingPolicy::Matches(Other) 
			&& bNeedsPixelShader == Other.bNeedsPixelShader
			&& VertexShader == Other.VertexShader
			&& PixelShader == Other.PixelShader;
	}

	void SetSharedState(FRHICommandList& RHICmdList, const FSceneView* View, const ContextDataType PolicyContext) const;

	/** 
	* Create bound shader state using the vertex decl from the mesh draw policy
	* as well as the shaders needed to draw the mesh
	* @param DynamicStride - optional stride for dynamic vertex data
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
		const ElementDataType& ElementData,
		const ContextDataType PolicyContext
		) const;

	friend int32 CompareDrawingPolicy(const FDepthDrawingPolicy& A,const FDepthDrawingPolicy& B);

private:
	bool bNeedsPixelShader;
	class FDepthOnlyHS *HullShader;
	class FDepthOnlyDS *DomainShader;

	TDepthOnlyVS<false>* VertexShader;
	FDepthOnlyPS* PixelShader;
};

/**
 * Writes out depth for opaque materials on meshes which support a position-only vertex buffer.
 * Using the position-only vertex buffer saves vertex fetch bandwidth during the z prepass.
 */
class FPositionOnlyDepthDrawingPolicy : public FMeshDrawingPolicy
{
public:

	FPositionOnlyDepthDrawingPolicy(
		const FVertexFactory* InVertexFactory,
		const FMaterialRenderProxy* InMaterialRenderProxy,
		const FMaterial& InMaterialResource,
		bool bIsTwoSided,
		bool bIsWireframe
		);

	// FMeshDrawingPolicy interface.
	bool Matches(const FPositionOnlyDepthDrawingPolicy& Other) const
	{
		return FMeshDrawingPolicy::Matches(Other) && VertexShader == Other.VertexShader;
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
		const ElementDataType& ElementData,
		const ContextDataType PolicyContext
		) const;

	friend int32 CompareDrawingPolicy(const FPositionOnlyDepthDrawingPolicy& A,const FPositionOnlyDepthDrawingPolicy& B);

private:
	TDepthOnlyVS<true> * VertexShader;
};

/**
 * A drawing policy factory for the depth drawing policy.
 */
class FDepthDrawingPolicyFactory
{
public:

	enum { bAllowSimpleElements = false };
	struct ContextType
	{
		EDepthDrawingMode DepthDrawingMode;

		ContextType(EDepthDrawingMode InDepthDrawingMode) :
			DepthDrawingMode(InDepthDrawingMode)
		{}
	};

	static void AddStaticMesh(FScene* Scene,FStaticMesh* StaticMesh);
	static bool DrawDynamicMesh(
		FRHICommandList& RHICmdList, 
		const FViewInfo& View,
		ContextType DrawingContext,
		const FMeshBatch& Mesh,
		bool bBackFace,
		bool bPreFog,
		const FPrimitiveSceneProxy* PrimitiveSceneProxy,
		FHitProxyId HitProxyId
		);

	static bool DrawStaticMesh(
		FRHICommandList& RHICmdList, 
		const FViewInfo& View,
		ContextType DrawingContext,
		const FStaticMesh& StaticMesh,
		const uint64& BatchElementMask,
		bool bPreFog,
		float DitheredLODTransitionValue,
		const FPrimitiveSceneProxy* PrimitiveSceneProxy,
		FHitProxyId HitProxyId
		);

private:
	/**
	* Render a dynamic or static mesh using a depth draw policy
	* @return true if the mesh rendered
	*/
	static bool DrawMesh(
		FRHICommandList& RHICmdList, 
		const FViewInfo& View,
		ContextType DrawingContext,
		const FMeshBatch& Mesh,
		const uint64& BatchElementMask,
		bool bBackFace,
		float DitheredLODTransitionValue,
		bool bPreFog,
		const FPrimitiveSceneProxy* PrimitiveSceneProxy,
		FHitProxyId HitProxyId
		);
};
