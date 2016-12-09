// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	DrawingPolicy.h: Drawing policy definitions.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "SceneView.h"
#include "MeshBatch.h"

class FPrimitiveSceneProxy;
class FStaticMesh;
class FViewInfo;

/**
 * A macro to compare members of two drawing policies(A and B), and return based on the result.
 * If the members are the same, the macro continues execution rather than returning to the caller.
 */
#define COMPAREDRAWINGPOLICYMEMBERS(MemberName) \
	if(A.MemberName < B.MemberName) { return -1; } \
	else if(A.MemberName > B.MemberName) { return +1; }

enum class EDrawingPolicyOverrideFlags
{
	None = 0,
	TwoSided = 1 << 0,
	DitheredLODTransition = 1 << 1,
	Wireframe = 1 << 2,
	ReverseCullMode = 1 << 3,
};
ENUM_CLASS_FLAGS(EDrawingPolicyOverrideFlags);

struct FDrawingPolicyRenderState
{
	FDrawingPolicyRenderState() = delete;

	//temporary tracking of the reset comandlist
	FDrawingPolicyRenderState(FRHICommandList* InDebugResetRHICmdList, const FSceneView& SceneView) : 
#if RHI_PSO_X_VALIDATION		
		DebugParentState(nullptr),
		DebugResetRHICmdList(InDebugResetRHICmdList),
#endif
		  BlendState(nullptr)
		, DepthStencilState(nullptr)
		, ViewUniformBufferPtr(&SceneView.ViewUniformBuffer)
		, StencilRef(0)
		, ViewOverrideFlags(EDrawingPolicyOverrideFlags::None)
		, DitheredLODTransitionAlpha(0.0f)
	{
		ViewOverrideFlags |= SceneView.bReverseCulling ? EDrawingPolicyOverrideFlags::ReverseCullMode : EDrawingPolicyOverrideFlags::None;
		ViewOverrideFlags |= SceneView.bRenderSceneTwoSided ? EDrawingPolicyOverrideFlags::TwoSided : EDrawingPolicyOverrideFlags::None;
	}

	//temporary deletion of default copy constructor
	FDrawingPolicyRenderState(const FDrawingPolicyRenderState& DrawRenderState) = delete;

	//temporary tracking of the reset comandlist
	FORCEINLINE_DEBUGGABLE FDrawingPolicyRenderState(FRHICommandList* InDebugResetRHICmdList, const FDrawingPolicyRenderState& DrawRenderState) :
#if RHI_PSO_X_VALIDATION
		DebugParentState(InDebugResetRHICmdList ? &DrawRenderState : nullptr),
		DebugResetRHICmdList(InDebugResetRHICmdList),
#endif
		  BlendState(DrawRenderState.BlendState)
		, DepthStencilState(DrawRenderState.DepthStencilState)
		, ViewUniformBufferPtr(DrawRenderState.ViewUniformBufferPtr)
		, StencilRef(DrawRenderState.StencilRef)
		, ViewOverrideFlags(DrawRenderState.ViewOverrideFlags)
		, DitheredLODTransitionAlpha(DrawRenderState.DitheredLODTransitionAlpha)
	{
	}

	~FDrawingPolicyRenderState()
	{
#if RHI_PSO_X_VALIDATION
		if (DebugParentState && DebugResetRHICmdList)
		{
			FScopedStrictGraphicsPipelineStateUse UsePSOOnly(*DebugResetRHICmdList, false);
			if (DebugParentState->BlendState && BlendState != DebugParentState->BlendState)
			{
				DebugResetRHICmdList->SetBlendState(DebugParentState->BlendState);
			}
			if (DebugParentState->DepthStencilState && DepthStencilState != DebugParentState->DepthStencilState)
			{
				DebugResetRHICmdList->SetDepthStencilState(DebugParentState->DepthStencilState, DebugParentState->StencilRef);
			}
		}
#endif
	}

public:
	FORCEINLINE_DEBUGGABLE void SetBlendState(FRHICommandList& RHICmdList, FBlendStateRHIParamRef InBlendState)
	{
		BlendState = InBlendState;

#if RHI_PSO_X_VALIDATION
		checkf(&RHICmdList == DebugResetRHICmdList, TEXT("If the State has been transfered to another comandlist it should be copied or recreated"));
		ensure(!RHICmdList.StrictGraphicsPipelineStateUse || !BlendState || RHICmdList.VerifyableBlendState == BlendState);
		FScopedStrictGraphicsPipelineStateUse UsePSOOnly(RHICmdList, false);
#else
		if (!RHICmdList.StrictGraphicsPipelineStateUse)
#endif
		{
			RHICmdList.SetBlendState(InBlendState);
		}
	}

	FORCEINLINE_DEBUGGABLE const FBlendStateRHIParamRef GetBlendState() const
	{
		return BlendState;
	}

	FORCEINLINE_DEBUGGABLE void SetDepthStencilState(FRHICommandList& RHICmdList, FDepthStencilStateRHIParamRef InDepthStencilState, uint32 InStencilRef = 0)
	{
		DepthStencilState = InDepthStencilState;
		StencilRef = InStencilRef;

#if RHI_PSO_X_VALIDATION
		checkf(&RHICmdList == DebugResetRHICmdList, TEXT("If the State has been transfered to another comandlist it should be copied or recreated"));
		ensure(!RHICmdList.StrictGraphicsPipelineStateUse || !DepthStencilState || RHICmdList.VerifyableDepthStencilState == DepthStencilState);	
		FScopedStrictGraphicsPipelineStateUse UsePSOOnly(RHICmdList, false);
#else
		if (!RHICmdList.StrictGraphicsPipelineStateUse)
#endif
		{
			RHICmdList.SetDepthStencilState(InDepthStencilState, InStencilRef);
		}
	}

	FORCEINLINE_DEBUGGABLE const FDepthStencilStateRHIParamRef GetDepthStencilState() const
	{
		return DepthStencilState;
	}

	FORCEINLINE_DEBUGGABLE void SetViewUniformBuffer(const TUniformBufferRef<FViewUniformShaderParameters>& InViewUniformBuffer)
	{
		ViewUniformBufferPtr = &InViewUniformBuffer;
	}

	FORCEINLINE_DEBUGGABLE const TUniformBufferRef<FViewUniformShaderParameters>& GetViewUniformBuffer() const
	{
		return *static_cast<const TUniformBufferRef<FViewUniformShaderParameters>*>(ViewUniformBufferPtr);
	}

	FORCEINLINE_DEBUGGABLE uint32 GetStencilRef() const
	{
		return StencilRef;
	}

	FORCEINLINE_DEBUGGABLE void SetDitheredLODTransitionAlpha(float InDitheredLODTransitionAlpha)
	{
		DitheredLODTransitionAlpha = InDitheredLODTransitionAlpha;
	}

	FORCEINLINE_DEBUGGABLE float GetDitheredLODTransitionAlpha() const
	{
		return DitheredLODTransitionAlpha;
	}


	FORCEINLINE_DEBUGGABLE EDrawingPolicyOverrideFlags& ModifyViewOverrideFlags()
	{
		return ViewOverrideFlags;
	}

	FORCEINLINE_DEBUGGABLE EDrawingPolicyOverrideFlags GetViewOverrideFlags() const
	{
		return ViewOverrideFlags;
	}

private:
	//start temporary code block
	friend class FParallelCommandListSet;
	void ReassignResetRHICmdList(FRHICommandList* InDebugResetRHICmdList)
	{
#if RHI_PSO_X_VALIDATION
		DebugResetRHICmdList = InDebugResetRHICmdList;
		DebugParentState = nullptr;
#endif
	}

#if RHI_PSO_X_VALIDATION
	const FDrawingPolicyRenderState*	DebugParentState;
	FRHICommandList*					DebugResetRHICmdList;
#endif

	FBlendStateRHIParamRef			BlendState;
	FDepthStencilStateRHIParamRef	DepthStencilState;

	//TODO this is evil, put prevents us from calling add and remove ref all the time
	const void*						ViewUniformBufferPtr;
	uint32							StencilRef;

	//not sure if those should belong here
	EDrawingPolicyOverrideFlags		ViewOverrideFlags;
	float							DitheredLODTransitionAlpha;
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

struct FMeshDrawingPolicyOverrideSettings
{
	EDrawingPolicyOverrideFlags	MeshOverrideFlags = EDrawingPolicyOverrideFlags::None;
	EPrimitiveType				MeshPrimitiveType = PT_TriangleList;
};

FORCEINLINE_DEBUGGABLE FMeshDrawingPolicyOverrideSettings ComputeMeshOverrideSettings(const FMeshBatch& Mesh)
{
	FMeshDrawingPolicyOverrideSettings OverrideSettings;
	OverrideSettings.MeshPrimitiveType = (EPrimitiveType)Mesh.Type;

	OverrideSettings.MeshOverrideFlags |= Mesh.bDisableBackfaceCulling ? EDrawingPolicyOverrideFlags::TwoSided : EDrawingPolicyOverrideFlags::None;
	OverrideSettings.MeshOverrideFlags |= Mesh.bDitheredLODTransition ? EDrawingPolicyOverrideFlags::DitheredLODTransition : EDrawingPolicyOverrideFlags::None;
	OverrideSettings.MeshOverrideFlags |= Mesh.bWireframe ? EDrawingPolicyOverrideFlags::Wireframe : EDrawingPolicyOverrideFlags::None;
	OverrideSettings.MeshOverrideFlags |= Mesh.ReverseCulling ? EDrawingPolicyOverrideFlags::ReverseCullMode : EDrawingPolicyOverrideFlags::None;
	return OverrideSettings;
}

/**
* Creates and sets the base PSO so that resources can be set. Generally best to call during SetSharedState.
*/
template<class DrawingPolicyType>
void CommitGraphicsPipelineState(FRHICommandList& RHICmdList, const DrawingPolicyType& DrawingPolicy, const FDrawingPolicyRenderState& DrawRenderState, ERHIFeatureLevel::Type	FeatureLevel)
{
#if RHI_PSO_X_VALIDATION
	RHICmdList.ValidatePsoState(DrawRenderState.GetBlendState(), DrawRenderState.GetDepthStencilState());
#endif

	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	GraphicsPSOInit.PrimitiveType = DrawingPolicy.GetPrimitiveType();
	GraphicsPSOInit.BoundShaderState = DrawingPolicy.GetBoundShaderStateInput(FeatureLevel);
	GraphicsPSOInit.RasterizerState = DrawingPolicy.ComputeRasterizerState(DrawRenderState.GetViewOverrideFlags());

	check(DrawRenderState.GetDepthStencilState());
	GraphicsPSOInit.DepthStencilState = DrawRenderState.GetDepthStencilState();
	GraphicsPSOInit.SetStencilRef(DrawRenderState.GetStencilRef());

	check(DrawRenderState.GetBlendState());
	GraphicsPSOInit.BlendState = DrawRenderState.GetBlendState();
	GraphicsPSOInit.SetBlendFactor(FLinearColor(1.0f, 1.0f, 1.0f));

	RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

	FLocalGraphicsPipelineState BaseGraphicsPSO = RHICmdList.BuildLocalGraphicsPipelineState(GraphicsPSOInit);
	RHICmdList.SetLocalGraphicsPipelineState(BaseGraphicsPSO);
}

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
		const FMeshDrawingPolicyOverrideSettings& InOverrideSettings,
		EDebugViewShaderMode InDebugViewShaderMode = DVSM_None
		);

	FMeshDrawingPolicy& operator = (const FMeshDrawingPolicy& Other)
	{ 
		VertexFactory = Other.VertexFactory;
		MaterialRenderProxy = Other.MaterialRenderProxy;
		MaterialResource = Other.MaterialResource;
		MeshFillMode = Other.MeshFillMode;
		MeshCullMode = Other.MeshCullMode;
		MeshPrimitiveType = Other.MeshPrimitiveType;
		bIsDitheredLODTransitionMaterial = Other.bIsDitheredLODTransitionMaterial;
		bUsePositionOnlyVS = Other.bUsePositionOnlyVS;
		DebugViewShaderMode = Other.DebugViewShaderMode;
		return *this; 
	}

	uint32 GetTypeHash() const
	{
		return PointerHash(VertexFactory, PointerHash(MaterialRenderProxy));
	}

	static void OnlyApplyDitheredLODTransitionState(FRHICommandList& RHICmdList, FDrawingPolicyRenderState& DrawRenderState, const FViewInfo& ViewInfo, const FStaticMesh& Mesh, const bool InAllowStencilDither);

	void ApplyDitheredLODTransitionState(FRHICommandList& RHICmdList, FDrawingPolicyRenderState& DrawRenderState, const FViewInfo& ViewInfo, const FStaticMesh& Mesh, const bool InAllowStencilDither)
	{
		OnlyApplyDitheredLODTransitionState(RHICmdList, DrawRenderState, ViewInfo, Mesh, InAllowStencilDither);
	}

	FDrawingPolicyMatchResult Matches(const FMeshDrawingPolicy& OtherDrawer) const
	{
		DRAWING_POLICY_MATCH_BEGIN
			DRAWING_POLICY_MATCH(VertexFactory == OtherDrawer.VertexFactory) &&
			DRAWING_POLICY_MATCH(MaterialRenderProxy == OtherDrawer.MaterialRenderProxy) &&
			DRAWING_POLICY_MATCH(bIsDitheredLODTransitionMaterial == OtherDrawer.bIsDitheredLODTransitionMaterial) &&
			DRAWING_POLICY_MATCH(bUsePositionOnlyVS == OtherDrawer.bUsePositionOnlyVS) &&
			DRAWING_POLICY_MATCH(MeshFillMode == OtherDrawer.MeshFillMode) &&
			DRAWING_POLICY_MATCH(MeshCullMode == OtherDrawer.MeshCullMode) &&
			DRAWING_POLICY_MATCH(MeshPrimitiveType == OtherDrawer.MeshPrimitiveType);
		DRAWING_POLICY_MATCH_END
	}

	static FORCEINLINE_DEBUGGABLE ERasterizerCullMode InverseCullMode(ERasterizerCullMode CullMode)
	{
		return CullMode == CM_None ? CM_None : (CullMode == CM_CCW ? CM_CW : CM_CCW);
	}

	FORCEINLINE_DEBUGGABLE FRasterizerStateRHIParamRef ComputeRasterizerState(EDrawingPolicyOverrideFlags InOverrideFlags) const
	{
		const bool bReverseCullMode = !!(InOverrideFlags & EDrawingPolicyOverrideFlags::ReverseCullMode); //(View.bReverseCulling ^ bBackFace)
		const bool bRenderSceneTwoSided = !!(InOverrideFlags & EDrawingPolicyOverrideFlags::TwoSided); //View.bRenderSceneTwoSided
		ERasterizerCullMode LocalCullMode = bRenderSceneTwoSided ? CM_None : bReverseCullMode ? InverseCullMode(MeshCullMode) : MeshCullMode;
		return GetStaticRasterizerState<true>(MeshFillMode, LocalCullMode);
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
		const FDrawingPolicyRenderState& DrawRenderState,
		const ElementDataType& ElementData,
		const ContextDataType PolicyContext
		) const
	{	
		RHICmdList.SetRasterizerState(ComputeRasterizerState(DrawRenderState.GetViewOverrideFlags()));
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
	void SetSharedState(FRHICommandList& RHICmdList, const FSceneView* View, const FMeshDrawingPolicy::ContextDataType PolicyContext, FDrawingPolicyRenderState& DrawRenderState) const;

	/**
	* Get the decl for this mesh policy type and vertexfactory
	*/
	const FVertexDeclarationRHIRef& GetVertexDeclaration() const;

	friend int32 CompareDrawingPolicy(const FMeshDrawingPolicy& A,const FMeshDrawingPolicy& B)
	{
		COMPAREDRAWINGPOLICYMEMBERS(VertexFactory);
		COMPAREDRAWINGPOLICYMEMBERS(MaterialRenderProxy);
		COMPAREDRAWINGPOLICYMEMBERS(bIsDitheredLODTransitionMaterial);
		return 0;
	}

	// Accessors.
	bool IsTwoSided() const
	{
		return MeshCullMode == CM_None;
	}
	bool IsDitheredLODTransition() const
	{
		return bIsDitheredLODTransitionMaterial;
	}
	bool IsWireframe() const
	{
		return MeshFillMode == FM_Wireframe;
	}

	EPrimitiveType GetPrimitiveType() const
	{
		return MeshPrimitiveType;
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
	
	ERasterizerFillMode MeshFillMode;
	ERasterizerCullMode MeshCullMode;
	EPrimitiveType		MeshPrimitiveType;

	uint32 bIsDitheredLODTransitionMaterial : 1;
	uint32 bUsePositionOnlyVS : 1;
	uint32 DebugViewShaderMode : 6; // EDebugViewShaderMode
};


uint32 GetTypeHash(const FBoundShaderStateRHIRef &Key);
