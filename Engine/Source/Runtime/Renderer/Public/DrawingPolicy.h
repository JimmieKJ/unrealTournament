// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	DrawingPolicy.h: Drawing policy definitions.
=============================================================================*/

#pragma once

#include "RHICommandList.h"


/**
 * A macro to compare members of two drawing policies(A and B), and return based on the result.
 * If the members are the same, the macro continues execution rather than returning to the caller.
 */
#define COMPAREDRAWINGPOLICYMEMBERS(MemberName) \
	if(A.MemberName < B.MemberName) { return -1; } \
	else if(A.MemberName > B.MemberName) { return +1; }

/**
 * Helper structure used to store mesh drawing state
 */
enum class EDitheredLODState
{
	None,
	FadeIn,
	FadeOut,
};

struct FMeshDrawingRenderState
{
	FMeshDrawingRenderState(float DitherAlpha = 0.0f)
	: DitheredLODTransitionAlpha(DitherAlpha)
	, DitheredLODState(EDitheredLODState::None)
	, bAllowStencilDither(false)
	{
	}

	FMeshDrawingRenderState(EDitheredLODState DitherState, bool InAllowStencilDither = false)
	: DitheredLODTransitionAlpha(0.0f)
	, DitheredLODState(DitherState)
	, bAllowStencilDither(InAllowStencilDither)
	{
	}

	float				DitheredLODTransitionAlpha;
	EDitheredLODState	DitheredLODState;
	bool				bAllowStencilDither;
};

class FDrawingPolicyMatchResult
{
public:
	FDrawingPolicyMatchResult()
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		: Matches(0)
#else
		: bLastResult(false)
#endif
	{
	}

	bool Append(const FDrawingPolicyMatchResult& Result, const TCHAR* condition)
	{
		bLastResult = Result.bLastResult;

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		TestResults = Result.TestResults;
		TestCondition = Result.TestCondition;
		Matches = Result.Matches;
#endif

		return bLastResult;
	}

	bool Append(bool Result, const TCHAR* condition)
	{
		bLastResult = Result;

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		TestResults.Add(Result);
		TestCondition.Add(condition);
		Matches += Result;
#endif

		return bLastResult;
	}

	bool Result() const
	{
		return bLastResult;
	}

	int32 MatchCount() const
	{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		return Matches;
#else
		return 0;
#endif
	}

	uint32 bLastResult : 1;

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	int32					Matches;
	TBitArray<>				TestResults;
	TArray<const TCHAR*>	TestCondition;
#endif
};

#define DRAWING_POLICY_MATCH_BEGIN FDrawingPolicyMatchResult Result; {
#define DRAWING_POLICY_MATCH(MatchExp) Result.Append((MatchExp), TEXT(#MatchExp))
#define DRAWING_POLICY_MATCH_END } return Result;

/**
 * The base mesh drawing policy.  Subclasses are used to draw meshes with type-specific context variables.
 * May be used either simply as a helper to render a dynamic mesh, or as a static instance shared between
 * similar meshs.
 */
class RENDERER_API FMeshDrawingPolicy
{
public:
	/** Per-element data required by the drawing policy that static mesh draw lists will cache. */
	struct ElementDataType {};

	/** Context data required by the drawing policy that is not known when caching policies in static mesh draw lists. */
	struct ContextDataType
	{
		ContextDataType(const bool InbIsInstancedStereo) : bIsInstancedStereo(InbIsInstancedStereo) {};
		ContextDataType() : bIsInstancedStereo(false) {};
		bool bIsInstancedStereo;
	};

	FMeshDrawingPolicy(
		const FVertexFactory* InVertexFactory,
		const FMaterialRenderProxy* InMaterialRenderProxy,
		const FMaterial& InMaterialResource,
		EDebugViewShaderMode InDebugViewShaderMode = DVSM_None,
		bool bInTwoSidedOverride = false,
		bool bInDitheredLODTransitionOverride = false,
		bool bInWireframeOverride = false
		);

	FMeshDrawingPolicy& operator = (const FMeshDrawingPolicy& Other)
	{ 
		VertexFactory = Other.VertexFactory;
		MaterialRenderProxy = Other.MaterialRenderProxy;
		MaterialResource = Other.MaterialResource;
		bIsTwoSidedMaterial = Other.bIsTwoSidedMaterial;
		bIsDitheredLODTransitionMaterial = Other.bIsDitheredLODTransitionMaterial;
		bIsWireframeMaterial = Other.bIsWireframeMaterial;
		bNeedsBackfacePass = Other.bNeedsBackfacePass;
		bUsePositionOnlyVS = Other.bUsePositionOnlyVS;
		DebugViewShaderMode = Other.DebugViewShaderMode;
		return *this; 
	}

	uint32 GetTypeHash() const
	{
		return PointerHash(VertexFactory,PointerHash(MaterialRenderProxy));
	}

	FDrawingPolicyMatchResult Matches(const FMeshDrawingPolicy& OtherDrawer) const
	{
		DRAWING_POLICY_MATCH_BEGIN
			DRAWING_POLICY_MATCH(VertexFactory == OtherDrawer.VertexFactory) &&
			DRAWING_POLICY_MATCH(MaterialRenderProxy == OtherDrawer.MaterialRenderProxy) &&
			DRAWING_POLICY_MATCH(bIsTwoSidedMaterial == OtherDrawer.bIsTwoSidedMaterial) &&
			DRAWING_POLICY_MATCH(bIsDitheredLODTransitionMaterial == OtherDrawer.bIsDitheredLODTransitionMaterial) &&
			DRAWING_POLICY_MATCH(bUsePositionOnlyVS == OtherDrawer.bUsePositionOnlyVS) &&
			DRAWING_POLICY_MATCH(bIsWireframeMaterial == OtherDrawer.bIsWireframeMaterial);
		DRAWING_POLICY_MATCH_END
	}

	/**
	 * Sets the render states for drawing a mesh.
	 * @param PrimitiveSceneProxy - The primitive drawing the dynamic mesh.  If this is a view element, this will be NULL.
	 */
	FORCEINLINE_DEBUGGABLE void SetMeshRenderState(
		FRHICommandList& RHICmdList, 
		const FSceneView& View,
		const FPrimitiveSceneProxy* PrimitiveSceneProxy,
		const FMeshBatch& Mesh,
		int32 BatchElementIndex,
		bool bBackFace,
		const FMeshDrawingRenderState& DrawRenderState,
		const ElementDataType& ElementData,
		const ContextDataType PolicyContext
		) const
	{
		const bool bRenderTwoSided = (IsTwoSided() && !NeedsBackfacePass()) || View.bRenderSceneTwoSided || Mesh.bDisableBackfaceCulling;

		// Use bitwise logic ops to avoid branches
		RHICmdList.SetRasterizerState(GetStaticRasterizerState<true>(
			(Mesh.bWireframe || IsWireframe()) ? FM_Wireframe : FM_Solid, 
			bRenderTwoSided ? CM_None : (((View.bReverseCulling ^ bBackFace) ^ Mesh.ReverseCulling) ? CM_CCW : CM_CW)
			));
	}

	/**
	 * Executes the draw commands for a mesh.
	 */
	void DrawMesh(FRHICommandList& RHICmdList, const FMeshBatch& Mesh, int32 BatchElementIndex, const bool bIsInstancedStereo = false) const;

	/** 
	 * Sets the instanced eye index shader uniform value where supported. 
	 * Used for explicitly setting which eye an instanced mesh will render to when rendering with instanced stereo.
	 * @param EyeIndex - Eye to render to: 0 = Left, 1 = Right
	 */
	void SetInstancedEyeIndex(FRHICommandList& RHICmdList, const uint32 EyeIndex) const { /*Empty*/ };

	/**
	 * Executes the draw commands which can be shared between any meshes using this drawer.
	 * @param CI - The command interface to execute the draw commands on.
	 * @param View - The view of the scene being drawn.
	 */
	void SetSharedState(FRHICommandList& RHICmdList, const FSceneView* View, const ContextDataType PolicyContext) const;

	/**
	* Get the decl for this mesh policy type and vertexfactory
	*/
	const FVertexDeclarationRHIRef& GetVertexDeclaration() const;

	friend int32 CompareDrawingPolicy(const FMeshDrawingPolicy& A,const FMeshDrawingPolicy& B)
	{
		COMPAREDRAWINGPOLICYMEMBERS(VertexFactory);
		COMPAREDRAWINGPOLICYMEMBERS(MaterialRenderProxy);
		COMPAREDRAWINGPOLICYMEMBERS(bIsTwoSidedMaterial);
		COMPAREDRAWINGPOLICYMEMBERS(bIsDitheredLODTransitionMaterial);
		return 0;
	}

	// Accessors.
	bool IsTwoSided() const
	{
		return bIsTwoSidedMaterial;
	}
	bool IsDitheredLODTransition() const
	{
		return bIsDitheredLODTransitionMaterial;
	}
	bool IsWireframe() const
	{
		return bIsWireframeMaterial;
	}
	bool NeedsBackfacePass() const
	{
		return bNeedsBackfacePass;
	}

	const FVertexFactory* GetVertexFactory() const { return VertexFactory; }
	const FMaterialRenderProxy* GetMaterialRenderProxy() const { return MaterialRenderProxy; }

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	FORCEINLINE EDebugViewShaderMode GetDebugViewShaderMode() const { return (EDebugViewShaderMode)DebugViewShaderMode; }
	FORCEINLINE bool UseDebugViewPS() const { return DebugViewShaderMode != DVSM_None; }
#else
	FORCEINLINE EDebugViewShaderMode GetDebugViewShaderMode() const { return DVSM_None; }
	FORCEINLINE bool UseDebugViewPS() const { return false; }
#endif

protected:
	const FVertexFactory* VertexFactory;
	const FMaterialRenderProxy* MaterialRenderProxy;
	const FMaterial* MaterialResource;
	uint32 bIsTwoSidedMaterial : 1;
	uint32 bIsDitheredLODTransitionMaterial : 1;
	uint32 bIsWireframeMaterial : 1;
	uint32 bNeedsBackfacePass : 1;
	uint32 bUsePositionOnlyVS : 1;
	uint32 DebugViewShaderMode : 6; // EDebugViewShaderMode
};


uint32 GetTypeHash(const FBoundShaderStateRHIRef &Key);

