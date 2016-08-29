// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	DepthRendering.h: Depth rendering definitions.
=============================================================================*/

#pragma once

enum EDepthDrawingMode
{
	// tested at a higher level
	DDM_None			= 0,
	//
	DDM_NonMaskedOnly	= 1,
	//
	DDM_AllOccluders	= 2,
	//
	DDM_AllOpaque		= 3,
};

extern const TCHAR* GetDepthDrawingModeString(EDepthDrawingMode Mode);

template<bool>
class TDepthOnlyVS;

class FDepthOnlyPS;

/**
 * Used to write out depth for opaque and masked materials during the depth-only pass.
 */
class FDepthDrawingPolicy : public FMeshDrawingPolicy
{
public:

	struct ContextDataType : public FMeshDrawingPolicy::ContextDataType
	{
		explicit ContextDataType(const bool InbIsInstancedStereo) : FMeshDrawingPolicy::ContextDataType(InbIsInstancedStereo), bIsInstancedStereoEmulated(false) {};
		ContextDataType(const bool InbIsInstancedStereo, const bool InbIsInstancedStereoEmulated) : FMeshDrawingPolicy::ContextDataType(InbIsInstancedStereo), bIsInstancedStereoEmulated(InbIsInstancedStereoEmulated) {};
		ContextDataType() : bIsInstancedStereoEmulated(false) {};
		bool bIsInstancedStereoEmulated;
	};

	FDepthDrawingPolicy(
		const FVertexFactory* InVertexFactory,
		const FMaterialRenderProxy* InMaterialRenderProxy,
		const FMaterial& InMaterialResource,
		bool bIsTwoSided,
		ERHIFeatureLevel::Type InFeatureLevel
		);

	// FMeshDrawingPolicy interface.
	FDrawingPolicyMatchResult Matches(const FDepthDrawingPolicy& Other) const
	{
		DRAWING_POLICY_MATCH_BEGIN
		 	DRAWING_POLICY_MATCH(FMeshDrawingPolicy::Matches(Other)) &&
			DRAWING_POLICY_MATCH(bNeedsPixelShader == Other.bNeedsPixelShader) &&
			DRAWING_POLICY_MATCH(VertexShader == Other.VertexShader) &&
			DRAWING_POLICY_MATCH(PixelShader == Other.PixelShader);
		DRAWING_POLICY_MATCH_END
	}

	void SetInstancedEyeIndex(FRHICommandList& RHICmdList, const uint32 EyeIndex) const;

	void SetSharedState(FRHICommandList& RHICmdList, const FSceneView* View, const FDepthDrawingPolicy::ContextDataType PolicyContext) const;

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
		const FMeshDrawingRenderState& DrawRenderState,
		const ElementDataType& ElementData,
		const ContextDataType PolicyContext
		) const;

	friend int32 CompareDrawingPolicy(const FDepthDrawingPolicy& A,const FDepthDrawingPolicy& B);

private:
	bool bNeedsPixelShader;
	class FDepthOnlyHS *HullShader;
	class FDepthOnlyDS *DomainShader;

	FShaderPipeline* ShaderPipeline;
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

	struct ContextDataType : public FMeshDrawingPolicy::ContextDataType
	{
		explicit ContextDataType(const bool InbIsInstancedStereo) : FMeshDrawingPolicy::ContextDataType(InbIsInstancedStereo), bIsInstancedStereoEmulated(false) {};
		ContextDataType(const bool InbIsInstancedStereo, const bool InbIsInstancedStereoEmulated) : FMeshDrawingPolicy::ContextDataType(InbIsInstancedStereo), bIsInstancedStereoEmulated(InbIsInstancedStereoEmulated) {};
		ContextDataType() : bIsInstancedStereoEmulated(false) {};
		bool bIsInstancedStereoEmulated;
	};

	FPositionOnlyDepthDrawingPolicy(
		const FVertexFactory* InVertexFactory,
		const FMaterialRenderProxy* InMaterialRenderProxy,
		const FMaterial& InMaterialResource,
		bool bIsTwoSided,
		bool bIsWireframe
		);

	// FMeshDrawingPolicy interface.
	FDrawingPolicyMatchResult Matches(const FPositionOnlyDepthDrawingPolicy& Other) const
	{
		DRAWING_POLICY_MATCH_BEGIN
			DRAWING_POLICY_MATCH(FMeshDrawingPolicy::Matches(Other)) && 
			DRAWING_POLICY_MATCH(VertexShader == Other.VertexShader);
		DRAWING_POLICY_MATCH_END
	}

	void SetSharedState(FRHICommandList& RHICmdList, const FSceneView* View, const FPositionOnlyDepthDrawingPolicy::ContextDataType PolicyContext) const;

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
		const ElementDataType& ElementData,
		const ContextDataType PolicyContext
		) const;

	void SetInstancedEyeIndex(FRHICommandList& RHICmdList, const uint32 EyeIndex) const;

	friend int32 CompareDrawingPolicy(const FPositionOnlyDepthDrawingPolicy& A,const FPositionOnlyDepthDrawingPolicy& B);

private:
	FShaderPipeline* ShaderPipeline;
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

		/** Uses of FDepthDrawingPolicyFactory that are not the depth pre-pass will not want the bUseAsOccluder flag to interfere. */
		bool bRespectUseAsOccluderFlag;

		ContextType(EDepthDrawingMode InDepthDrawingMode, bool bInRespectUseAsOccluderFlag) :
			DepthDrawingMode(InDepthDrawingMode),
			bRespectUseAsOccluderFlag(bInRespectUseAsOccluderFlag)
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
		FHitProxyId HitProxyId,
		const bool bIsInstancedStereo = false,
		const bool bIsInstancedStereoEmulated = false
		);

	static bool DrawStaticMesh(
		FRHICommandList& RHICmdList, 
		const FViewInfo& View,
		ContextType DrawingContext,
		const FStaticMesh& StaticMesh,
		const uint64& BatchElementMask,
		bool bPreFog,
		const FMeshDrawingRenderState& DrawRenderState,
		const FPrimitiveSceneProxy* PrimitiveSceneProxy,
		FHitProxyId HitProxyId, 
		const bool bIsInstancedStereo = false,
		const bool bIsInstancedStereoEmulated = false
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
		const FMeshDrawingRenderState& DrawRenderState,
		bool bPreFog,
		const FPrimitiveSceneProxy* PrimitiveSceneProxy,
		FHitProxyId HitProxyId, 
		const bool bIsInstancedStereo = false, 
		const bool bIsInstancedStereoEmulated = false
		);
};
