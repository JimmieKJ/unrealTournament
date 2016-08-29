// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	DistortionRendering.cpp: Distortion rendering implementation.
=============================================================================*/

#include "RendererPrivate.h"
#include "Engine.h"
#include "ScenePrivate.h"
#include "ScreenRendering.h"
#include "PostProcessing.h"
#include "RenderingCompositionGraph.h"
#include "SceneFilterRendering.h"
#include "SceneUtils.h"

DECLARE_FLOAT_COUNTER_STAT(TEXT("Distortion"), Stat_GPU_Distortion, STATGROUP_GPU);

/**
* A pixel shader for rendering the full screen refraction pass
*/
class FDistortionApplyScreenPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FDistortionApplyScreenPS,Global);
public:

	static bool ShouldCache(EShaderPlatform Platform) { return true; }

	FDistortionApplyScreenPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		DistortionTexture.Bind(Initializer.ParameterMap,TEXT("DistortionTexture"));
		DistortionTextureSampler.Bind(Initializer.ParameterMap,TEXT("DistortionTextureSampler"));
		SceneColorTexture.Bind(Initializer.ParameterMap,TEXT("SceneColorTexture"));
		SceneColorTextureSampler.Bind(Initializer.ParameterMap,TEXT("SceneColorTextureSampler"));
		SceneColorRect.Bind(Initializer.ParameterMap,TEXT("SceneColorRect"));
	}
	FDistortionApplyScreenPS() {}

	void SetParameters(const FRenderingCompositePassContext& Context, IPooledRenderTarget& DistortionRT)
	{
		FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(Context.RHICmdList);
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		FTextureRHIParamRef DistortionTextureValue = DistortionRT.GetRenderTargetItem().ShaderResourceTexture;
		FTextureRHIParamRef SceneColorTextureValue = SceneContext.GetSceneColor()->GetRenderTargetItem().ShaderResourceTexture;

		// Here we use SF_Point as in fullscreen the pixels are 1:1 mapped.
		SetTextureParameter(
			Context.RHICmdList,
			ShaderRHI,
			DistortionTexture,
			DistortionTextureSampler,
			TStaticSamplerState<SF_Point,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(),
			DistortionTextureValue
			);

		SetTextureParameter(
			Context.RHICmdList,
			ShaderRHI,
			SceneColorTexture,
			SceneColorTextureSampler,
			TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(),
			SceneColorTextureValue
			);

		FIntPoint SceneBufferSize = SceneContext.GetBufferSizeXY();
		FIntRect ViewportRect = Context.GetViewport();
		FVector4 SceneColorRectValue = FVector4((float)ViewportRect.Min.X/SceneBufferSize.X,
												(float)ViewportRect.Min.Y/SceneBufferSize.Y,
												(float)ViewportRect.Max.X/SceneBufferSize.X,
												(float)ViewportRect.Max.Y/SceneBufferSize.Y);
		SetShaderValue(Context.RHICmdList, ShaderRHI, SceneColorRect, SceneColorRectValue);
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << DistortionTexture << DistortionTextureSampler << SceneColorTexture << SceneColorTextureSampler << SceneColorRect;
		return bShaderHasOutdatedParameters;
	}

private:
	FShaderResourceParameter DistortionTexture;
	FShaderResourceParameter DistortionTextureSampler;
	FShaderResourceParameter SceneColorTexture;
	FShaderResourceParameter SceneColorTextureSampler;
	FShaderParameter SceneColorRect;
	FShaderParameter DistortionParams;
};

/** distortion apply screen pixel shader implementation */
IMPLEMENT_SHADER_TYPE(,FDistortionApplyScreenPS,TEXT("DistortApplyScreenPS"),TEXT("Main"),SF_Pixel);

/**
* A pixel shader that applies the distorted image to the scene
*/
class FDistortionMergePS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FDistortionMergePS,Global);
public:

	static bool ShouldCache(EShaderPlatform Platform) { return true; }

	FDistortionMergePS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		SceneColorTexture.Bind(Initializer.ParameterMap,TEXT("SceneColorTexture"));
		SceneColorTextureSampler.Bind(Initializer.ParameterMap,TEXT("SceneColorTextureSampler"));
	}
	FDistortionMergePS() {}

	void SetParameters(const FRenderingCompositePassContext& Context, const FTextureRHIParamRef& PassTexture)
	{
		FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(Context.RHICmdList);
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		SetTextureParameter(
			Context.RHICmdList,
			ShaderRHI,
			SceneColorTexture,
			SceneColorTextureSampler,
			TStaticSamplerState<SF_Point,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(),
			PassTexture
			);
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << SceneColorTexture << SceneColorTextureSampler;
		return bShaderHasOutdatedParameters;
	}

private:
	FShaderResourceParameter SceneColorTexture;
	FShaderResourceParameter SceneColorTextureSampler;
};

/** distortion merge to scene pixel shader implementation */
IMPLEMENT_SHADER_TYPE(,FDistortionMergePS,TEXT("DistortApplyScreenPS"),TEXT("Merge"),SF_Pixel);

/**
* Policy for drawing distortion mesh accumulated offsets
*/
class FDistortMeshAccumulatePolicy
{	
public:
	static bool ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		return Material && IsTranslucentBlendMode(Material->GetBlendMode()) && Material->IsDistorted();
	}
};

/**
* A vertex shader for rendering distortion meshes
*/
template<class DistortMeshPolicy>
class TDistortionMeshVS : public FMeshMaterialShader
{
	DECLARE_SHADER_TYPE(TDistortionMeshVS,MeshMaterial);

protected:

	TDistortionMeshVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		:	FMeshMaterialShader(Initializer)
	{
	}

	TDistortionMeshVS()
	{
	}

	static bool ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		return DistortMeshPolicy::ShouldCache(Platform,Material,VertexFactoryType);
	}

public:
	
	void SetParameters(FRHICommandList& RHICmdList, const FVertexFactory* VertexFactory,const FMaterialRenderProxy* MaterialRenderProxy,const FSceneView* View)
	{
		FMeshMaterialShader::SetParameters(RHICmdList, GetVertexShader(), MaterialRenderProxy, *MaterialRenderProxy->GetMaterial(View->GetFeatureLevel()), *View, View->ViewUniformBuffer, ESceneRenderTargetsMode::SetTextures);
	}

	void SetMesh(FRHICommandList& RHICmdList, const FVertexFactory* VertexFactory,const FSceneView& View,const FPrimitiveSceneProxy* Proxy,const FMeshBatchElement& BatchElement,const FMeshDrawingRenderState& DrawRenderState)
	{
		FMeshMaterialShader::SetMesh(RHICmdList, GetVertexShader(),VertexFactory,View,Proxy,BatchElement,DrawRenderState);
	}
};


/**
 * A hull shader for rendering distortion meshes
 */
template<class DistortMeshPolicy>
class TDistortionMeshHS : public FBaseHS
{
	DECLARE_SHADER_TYPE(TDistortionMeshHS,MeshMaterial);

protected:

	TDistortionMeshHS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FBaseHS(Initializer)
	{}

	TDistortionMeshHS() {}

	static bool ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		return FBaseHS::ShouldCache(Platform, Material, VertexFactoryType)
			&& DistortMeshPolicy::ShouldCache(Platform, Material, VertexFactoryType);
	}
};

/**
 * A domain shader for rendering distortion meshes
 */
template<class DistortMeshPolicy>
class TDistortionMeshDS : public FBaseDS
{
	DECLARE_SHADER_TYPE(TDistortionMeshDS,MeshMaterial);

protected:

	TDistortionMeshDS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FBaseDS(Initializer)
	{}

	TDistortionMeshDS() {}

	static bool ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		return FBaseDS::ShouldCache(Platform, Material, VertexFactoryType)
			&& DistortMeshPolicy::ShouldCache(Platform, Material, VertexFactoryType);
	}
};


IMPLEMENT_MATERIAL_SHADER_TYPE(template<>,TDistortionMeshVS<FDistortMeshAccumulatePolicy>,TEXT("DistortAccumulateVS"),TEXT("Main"),SF_Vertex); 
IMPLEMENT_MATERIAL_SHADER_TYPE(template<>,TDistortionMeshHS<FDistortMeshAccumulatePolicy>,TEXT("DistortAccumulateVS"),TEXT("MainHull"),SF_Hull); 
IMPLEMENT_MATERIAL_SHADER_TYPE(template<>,TDistortionMeshDS<FDistortMeshAccumulatePolicy>,TEXT("DistortAccumulateVS"),TEXT("MainDomain"),SF_Domain);


/**
* A pixel shader to render distortion meshes
*/
template<class DistortMeshPolicy>
class TDistortionMeshPS : public FMeshMaterialShader
{
	DECLARE_SHADER_TYPE(TDistortionMeshPS,MeshMaterial);

public:
	static bool ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		return DistortMeshPolicy::ShouldCache(Platform,Material,VertexFactoryType);
	}

	TDistortionMeshPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		:	FMeshMaterialShader(Initializer)
	{
//later?		MaterialParameters.Bind(Initializer.ParameterMap);
		DistortionParams.Bind(Initializer.ParameterMap,TEXT("DistortionParams"));
	}

	TDistortionMeshPS()
	{
	}

	void SetParameters(
		FRHICommandList& RHICmdList, 
		const FMaterialRenderProxy* MaterialRenderProxy,
		const FSceneView& View
		)
	{
		FMeshMaterialShader::SetParameters(RHICmdList, GetPixelShader(), MaterialRenderProxy, *MaterialRenderProxy->GetMaterial(View.GetFeatureLevel()), View, View.ViewUniformBuffer, ESceneRenderTargetsMode::SetTextures);

		float Ratio = View.UnscaledViewRect.Width() / (float)View.UnscaledViewRect.Height();
		float Params[4];
		Params[0] = View.ViewMatrices.ProjMatrix.M[0][0];
		Params[1] = Ratio;
		Params[2] = (float)View.UnscaledViewRect.Width();
		Params[3] = (float)View.UnscaledViewRect.Height();

		SetShaderValue(RHICmdList, GetPixelShader(), DistortionParams, Params);

	}

	void SetMesh(FRHICommandList& RHICmdList, const FVertexFactory* VertexFactory,const FSceneView& View,const FPrimitiveSceneProxy* Proxy,const FMeshBatchElement& BatchElement,const FMeshDrawingRenderState& DrawRenderState)
	{
		FMeshMaterialShader::SetMesh(RHICmdList, GetPixelShader(),VertexFactory,View,Proxy,BatchElement,DrawRenderState);
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FMeshMaterialShader::Serialize(Ar);
		Ar << DistortionParams;
		return bShaderHasOutdatedParameters;
	}

private:
	FShaderParameter DistortionParams;
};

//** distortion accumulate pixel shader type implementation */
IMPLEMENT_MATERIAL_SHADER_TYPE(template<>,TDistortionMeshPS<FDistortMeshAccumulatePolicy>,TEXT("DistortAccumulatePS"),TEXT("Main"),SF_Pixel);

/*-----------------------------------------------------------------------------
TDistortionMeshDrawingPolicy
-----------------------------------------------------------------------------*/

/**
* Distortion mesh drawing policy
*/
template<class DistortMeshPolicy>
class TDistortionMeshDrawingPolicy : public FMeshDrawingPolicy
{
public:
	/** context type */
	typedef FMeshDrawingPolicy::ElementDataType ElementDataType;

	/**
	* Constructor
	* @param InIndexBuffer - index buffer for rendering
	* @param InVertexFactory - vertex factory for rendering
	* @param InMaterialRenderProxy - material instance for rendering
	* @param bInOverrideWithShaderComplexity - whether to override with shader complexity
	*/
	TDistortionMeshDrawingPolicy(
		const FVertexFactory* InVertexFactory,
		const FMaterialRenderProxy* InMaterialRenderProxy,
		const FMaterial& MaterialResouce,
		bool bInitializeOffsets,
		EDebugViewShaderMode InDebugViewShaderMode,
		ERHIFeatureLevel::Type InFeatureLevel
		);

	// FMeshDrawingPolicy interface.

	/**
	* Match two draw policies
	* @param Other - draw policy to compare
	* @return true if the draw policies are a match
	*/
	FDrawingPolicyMatchResult Matches(const TDistortionMeshDrawingPolicy& Other) const;

	/**
	* Executes the draw commands which can be shared between any meshes using this drawer.
	* @param CI - The command interface to execute the draw commands on.
	* @param View - The view of the scene being drawn.
	*/
	void SetSharedState(FRHICommandList& RHICmdList, const FSceneView* View, const ContextDataType PolicyContext) const;
	
	/** 
	* Create bound shader state using the vertex decl from the mesh draw policy
	* as well as the shaders needed to draw the mesh
	* @return new bound shader state object
	*/
	FBoundShaderStateInput GetBoundShaderStateInput(ERHIFeatureLevel::Type InFeatureLevel);

	/**
	* Sets the render states for drawing a mesh.
	* @param PrimitiveSceneProxy - The primitive drawing the dynamic mesh.  If this is a view element, this will be NULL.
	* @param Mesh - mesh element with data needed for rendering
	* @param ElementData - context specific data for mesh rendering
	*/
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

private:
	/** vertex shader based on policy type */
	TDistortionMeshVS<DistortMeshPolicy>* VertexShader;

	TDistortionMeshHS<DistortMeshPolicy>* HullShader;
	TDistortionMeshDS<DistortMeshPolicy>* DomainShader;

	/** whether we are initializing offsets or accumulating them */
	bool bInitializeOffsets;
	/** pixel shader based on policy type */
	TDistortionMeshPS<DistortMeshPolicy>* DistortPixelShader;
	/** pixel shader used to initialize offsets */
//later	FShaderComplexityAccumulatePixelShader* InitializePixelShader;
};

/**
* Constructor
* @param InIndexBuffer - index buffer for rendering
* @param InVertexFactory - vertex factory for rendering
* @param InMaterialRenderProxy - material instance for rendering
*/
template<class DistortMeshPolicy> 
TDistortionMeshDrawingPolicy<DistortMeshPolicy>::TDistortionMeshDrawingPolicy(
	const FVertexFactory* InVertexFactory,
	const FMaterialRenderProxy* InMaterialRenderProxy,
	const FMaterial& InMaterialResource,
	bool bInInitializeOffsets,
	EDebugViewShaderMode InDebugViewShaderMode,
	ERHIFeatureLevel::Type InFeatureLevel
	)
:	FMeshDrawingPolicy(InVertexFactory,InMaterialRenderProxy,InMaterialResource,InDebugViewShaderMode)
,	bInitializeOffsets(bInInitializeOffsets)
{
	HullShader = NULL;
	DomainShader = NULL;

	const EMaterialTessellationMode MaterialTessellationMode = MaterialResource->GetTessellationMode();
	if (RHISupportsTessellation(GShaderPlatformForFeatureLevel[InFeatureLevel])
		&& InVertexFactory->GetType()->SupportsTessellationShaders() 
		&& MaterialTessellationMode != MTM_NoTessellation)
	{
		HullShader = InMaterialResource.GetShader<TDistortionMeshHS<DistortMeshPolicy>>(VertexFactory->GetType());
		DomainShader = InMaterialResource.GetShader<TDistortionMeshDS<DistortMeshPolicy>>(VertexFactory->GetType());
	}

	VertexShader = InMaterialResource.GetShader<TDistortionMeshVS<DistortMeshPolicy> >(InVertexFactory->GetType());

	if (bInitializeOffsets)
	{
//later		InitializePixelShader = GetGlobalShaderMap(View.ShaderMap)->GetShader<FShaderComplexityAccumulatePixelShader>();
		DistortPixelShader = NULL;
	}
	else
	{
		DistortPixelShader = InMaterialResource.GetShader<TDistortionMeshPS<DistortMeshPolicy> >(InVertexFactory->GetType());
//later		InitializePixelShader = NULL;
	}
}

/**
* Match two draw policies
* @param Other - draw policy to compare
* @return true if the draw policies are a match
*/
template<class DistortMeshPolicy>
FDrawingPolicyMatchResult TDistortionMeshDrawingPolicy<DistortMeshPolicy>::Matches(
	const TDistortionMeshDrawingPolicy& Other
	) const
{
	DRAWING_POLICY_MATCH_BEGIN
		DRAWING_POLICY_MATCH(FMeshDrawingPolicy::Matches(Other)) &&
		DRAWING_POLICY_MATCH(VertexShader == Other.VertexShader) &&
		DRAWING_POLICY_MATCH(HullShader == Other.HullShader) &&
		DRAWING_POLICY_MATCH(DomainShader == Other.DomainShader) &&
		DRAWING_POLICY_MATCH(bInitializeOffsets == Other.bInitializeOffsets) &&
		DRAWING_POLICY_MATCH(DistortPixelShader == Other.DistortPixelShader); //&&
	//later	InitializePixelShader == Other.InitializePixelShader;
	DRAWING_POLICY_MATCH_END
}

/**
* Executes the draw commands which can be shared between any meshes using this drawer.
* @param CI - The command interface to execute the draw commands on.
* @param View - The view of the scene being drawn.
*/
template<class DistortMeshPolicy>
void TDistortionMeshDrawingPolicy<DistortMeshPolicy>::SetSharedState(
	FRHICommandList& RHICmdList, 
	const FSceneView* View,
	const ContextDataType PolicyContext
	) const
{
	// Set shared mesh resources
	FMeshDrawingPolicy::SetSharedState(RHICmdList, View, PolicyContext);

	// Set the translucent shader parameters for the material instance
	VertexShader->SetParameters(RHICmdList, VertexFactory,MaterialRenderProxy,View);

	if(HullShader && DomainShader)
	{
		HullShader->SetParameters(RHICmdList, MaterialRenderProxy,*View);
		DomainShader->SetParameters(RHICmdList, MaterialRenderProxy,*View);
	}

	if (UseDebugViewPS())
	{
		check(!bInitializeOffsets);
//later		TShaderMapRef<FShaderComplexityAccumulatePixelShader> ShaderComplexityPixelShader(GetGlobalShaderMap(View->ShaderMap));
		//don't add any vertex complexity
//later		ShaderComplexityPixelShader->SetParameters(0, DistortPixelShader->GetNumInstructions());
	}
	if (bInitializeOffsets)
	{
//later			InitializePixelShader->SetParameters(0, 0);
	}
	else
	{
		DistortPixelShader->SetParameters(RHICmdList, MaterialRenderProxy,*View);
	}
}

/** 
* Create bound shader state using the vertex decl from the mesh draw policy
* as well as the shaders needed to draw the mesh
* @return new bound shader state object
*/
template<class DistortMeshPolicy>
FBoundShaderStateInput TDistortionMeshDrawingPolicy<DistortMeshPolicy>::GetBoundShaderStateInput(ERHIFeatureLevel::Type InFeatureLevel)
{
	FPixelShaderRHIParamRef PixelShaderRHIRef = NULL;

	if (UseDebugViewPS())
	{
		check(!bInitializeOffsets);
//later		TShaderMapRef<FShaderComplexityAccumulatePixelShader> ShaderComplexityAccumulatePixelShader(GetGlobalShaderMap(InFeatureLevel));
//later		PixelShaderRHIRef = ShaderComplexityAccumulatePixelShader->GetPixelShader();
	}

	if (bInitializeOffsets)
	{
//later		PixelShaderRHIRef = InitializePixelShader->GetPixelShader();
	}
	else
	{
		PixelShaderRHIRef = DistortPixelShader->GetPixelShader();
	}

	return FBoundShaderStateInput(
		FMeshDrawingPolicy::GetVertexDeclaration(), 
		VertexShader->GetVertexShader(),
		GETSAFERHISHADER_HULL(HullShader), 
		GETSAFERHISHADER_DOMAIN(DomainShader),
		PixelShaderRHIRef,
		FGeometryShaderRHIRef());

}

/**
* Sets the render states for drawing a mesh.
* @param PrimitiveSceneProxy - The primitive drawing the dynamic mesh.  If this is a view element, this will be NULL.
* @param Mesh - mesh element with data needed for rendering
* @param ElementData - context specific data for mesh rendering
*/
template<class DistortMeshPolicy>
void TDistortionMeshDrawingPolicy<DistortMeshPolicy>::SetMeshRenderState(
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

	const FMeshBatchElement& BatchElement = Mesh.Elements[BatchElementIndex];

	// Set transforms
	VertexShader->SetMesh(RHICmdList, VertexFactory,View,PrimitiveSceneProxy,BatchElement,DrawRenderState);

	if(HullShader && DomainShader)
	{
		HullShader->SetMesh(RHICmdList, VertexFactory,View,PrimitiveSceneProxy,BatchElement,DrawRenderState);
		DomainShader->SetMesh(RHICmdList, VertexFactory,View,PrimitiveSceneProxy,BatchElement,DrawRenderState);
	}
	

	// Don't set pixel shader constants if we are overriding with the shader complexity pixel shader
	if (!bInitializeOffsets && !UseDebugViewPS())
	{
		DistortPixelShader->SetMesh(RHICmdList, VertexFactory,View,PrimitiveSceneProxy,BatchElement,DrawRenderState);
	}
	
	// Set rasterizer state.
	RHICmdList.SetRasterizerState(GetStaticRasterizerState<true>(
		(Mesh.bWireframe || IsWireframe()) ? FM_Wireframe : FM_Solid,
		IsTwoSided() ? CM_None : (XOR( XOR(View.bReverseCulling,bBackFace), Mesh.ReverseCulling) ? CM_CCW : CM_CW)));
}

/*-----------------------------------------------------------------------------
TDistortionMeshDrawingPolicyFactory
-----------------------------------------------------------------------------*/

/**
* Distortion mesh draw policy factory. 
* Creates the policies needed for rendering a mesh based on its material
*/
template<class DistortMeshPolicy>
class TDistortionMeshDrawingPolicyFactory
{
public:
	enum { bAllowSimpleElements = false };
	typedef bool ContextType;

	/**
	* Render a dynamic mesh using a distortion mesh draw policy 
	* @return true if the mesh rendered
	*/
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

	/**
	* Render a dynamic mesh using a distortion mesh draw policy 
	* @return true if the mesh rendered
	*/
	static bool DrawStaticMesh(
		FRHICommandList& RHICmdList, 
		const FViewInfo* View,
		ContextType DrawingContext,
		const FStaticMesh& StaticMesh,
		uint64 BatchElementMask,
		bool bBackFace,
		const FPrimitiveSceneProxy* PrimitiveSceneProxy,
		FHitProxyId HitProxyId
		);
};

/**
* Render a dynamic mesh using a distortion mesh draw policy 
* @return true if the mesh rendered
*/
template<class DistortMeshPolicy>
bool TDistortionMeshDrawingPolicyFactory<DistortMeshPolicy>::DrawDynamicMesh(
	FRHICommandList& RHICmdList, 
	const FSceneView& View,
	ContextType bInitializeOffsets,
	const FMeshBatch& Mesh,
	bool bBackFace,
	bool bPreFog,
	const FPrimitiveSceneProxy* PrimitiveSceneProxy,
	FHitProxyId HitProxyId
	)
{
	const auto FeatureLevel = View.GetFeatureLevel();
	bool bDistorted = Mesh.MaterialRenderProxy && Mesh.MaterialRenderProxy->GetMaterial(FeatureLevel)->IsDistorted();

	if(bDistorted && !bBackFace)
	{
		// draw dynamic mesh element using distortion mesh policy
		TDistortionMeshDrawingPolicy<DistortMeshPolicy> DrawingPolicy(
			Mesh.VertexFactory,
			Mesh.MaterialRenderProxy,
			*Mesh.MaterialRenderProxy->GetMaterial(FeatureLevel),
			bInitializeOffsets,
			View.Family->GetDebugViewShaderMode(),
			FeatureLevel
			);
		RHICmdList.BuildAndSetLocalBoundShaderState(DrawingPolicy.GetBoundShaderStateInput(View.GetFeatureLevel()));
		DrawingPolicy.SetSharedState(RHICmdList, &View, typename TDistortionMeshDrawingPolicy<DistortMeshPolicy>::ContextDataType());
		const FMeshDrawingRenderState DrawRenderState(Mesh.DitheredLODTransitionAlpha);
		for (int32 BatchElementIndex = 0; BatchElementIndex < Mesh.Elements.Num(); BatchElementIndex++)
		{
			TDrawEvent<FRHICommandList> MeshEvent;
			BeginMeshDrawEvent(RHICmdList, PrimitiveSceneProxy, Mesh, MeshEvent);

			DrawingPolicy.SetMeshRenderState(RHICmdList, View,PrimitiveSceneProxy,Mesh,BatchElementIndex,bBackFace,DrawRenderState,typename TDistortionMeshDrawingPolicy<DistortMeshPolicy>::ElementDataType(), typename TDistortionMeshDrawingPolicy<DistortMeshPolicy>::ContextDataType());
			DrawingPolicy.DrawMesh(RHICmdList, Mesh,BatchElementIndex);
		}

		return true;
	}
	else
	{
		return false;
	}
}

/**
* Render a dynamic mesh using a distortion mesh draw policy 
* @return true if the mesh rendered
*/
template<class DistortMeshPolicy>
bool TDistortionMeshDrawingPolicyFactory<DistortMeshPolicy>::DrawStaticMesh(
	FRHICommandList& RHICmdList, 
	const FViewInfo* View,
	ContextType bInitializeOffsets,
	const FStaticMesh& StaticMesh,
	uint64 BatchElementMask,
	bool bBackFace,
	const FPrimitiveSceneProxy* PrimitiveSceneProxy,
	FHitProxyId HitProxyId
	)
{
	const auto FeatureLevel = View->GetFeatureLevel();
	bool bDistorted = StaticMesh.MaterialRenderProxy && StaticMesh.MaterialRenderProxy->GetMaterial(FeatureLevel)->IsDistorted();
	
	if(bDistorted && !bBackFace)
	{
		// draw static mesh element using distortion mesh policy
		TDistortionMeshDrawingPolicy<DistortMeshPolicy> DrawingPolicy(
			StaticMesh.VertexFactory,
			StaticMesh.MaterialRenderProxy,
			*StaticMesh.MaterialRenderProxy->GetMaterial(FeatureLevel),
			bInitializeOffsets,
			View->Family->GetDebugViewShaderMode(),
			FeatureLevel
			);
		RHICmdList.BuildAndSetLocalBoundShaderState(DrawingPolicy.GetBoundShaderStateInput(View->GetFeatureLevel()));
		DrawingPolicy.SetSharedState(RHICmdList, View, typename TDistortionMeshDrawingPolicy<DistortMeshPolicy>::ContextDataType());
		const FMeshDrawingRenderState DrawRenderState(View->GetDitheredLODTransitionState(StaticMesh));
		int32 BatchElementIndex = 0;
		do
		{
			if(BatchElementMask & 1)
			{
				TDrawEvent<FRHICommandList> MeshEvent;
				BeginMeshDrawEvent(RHICmdList, PrimitiveSceneProxy, StaticMesh, MeshEvent);


				DrawingPolicy.SetMeshRenderState(RHICmdList, *View,PrimitiveSceneProxy,StaticMesh,BatchElementIndex,bBackFace,DrawRenderState,
					typename TDistortionMeshDrawingPolicy<DistortMeshPolicy>::ElementDataType(),
					typename TDistortionMeshDrawingPolicy<DistortMeshPolicy>::ContextDataType()
					);
				DrawingPolicy.DrawMesh(RHICmdList, StaticMesh, BatchElementIndex);
			}
			BatchElementMask >>= 1;
			BatchElementIndex++;
		} while(BatchElementMask);

		return true;
	}
	else
	{
		return false;
	}
}

/*-----------------------------------------------------------------------------
	FDistortionPrimSet
-----------------------------------------------------------------------------*/

bool FDistortionPrimSet::DrawAccumulatedOffsets(FRHICommandListImmediate& RHICmdList, const FViewInfo& View, bool bInitializeOffsets)
{
	bool bDirty = false;

	QUICK_SCOPE_CYCLE_COUNTER(STAT_FDistortionPrimSet_DrawAccumulatedOffsets);

	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_FDistortionPrimSet_DrawAccumulatedOffsets_View);
		// Draw the view's elements with the distortion drawing policy.
		bDirty |= DrawViewElements<TDistortionMeshDrawingPolicyFactory<FDistortMeshAccumulatePolicy> >(
			RHICmdList,
			View,
			bInitializeOffsets,
			0,	// DPG Index?
			false // Distortion is rendered post fog.
			);
	}

	if( Prims.Num() )
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_FDistortionPrimSet_DrawAccumulatedOffsets_Prims);
		
		// Draw scene prims
		for( int32 PrimIdx = 0; PrimIdx < Prims.Num(); PrimIdx++ )
		{
			FPrimitiveSceneProxy* PrimitiveSceneProxy = Prims[PrimIdx];
			const FPrimitiveViewRelevance& ViewRelevance = View.PrimitiveViewRelevanceMap[PrimitiveSceneProxy->GetPrimitiveSceneInfo()->GetIndex()];
			FPrimitiveSceneInfo* PrimitiveSceneInfo = PrimitiveSceneProxy->GetPrimitiveSceneInfo();

			TDistortionMeshDrawingPolicyFactory<FDistortMeshAccumulatePolicy>::ContextType Context(bInitializeOffsets);

			// Note: As for distortion rendering the order doesn't matter we actually could iterate View.DynamicMeshElements without this indirection
			{

				// range in View.DynamicMeshElements[]
				FInt32Range range = View.GetDynamicMeshElementRange(PrimitiveSceneInfo->GetIndex());

				for (int32 MeshBatchIndex = range.GetLowerBoundValue(); MeshBatchIndex < range.GetUpperBoundValue(); MeshBatchIndex++)
				{
					const FMeshBatchAndRelevance& MeshBatchAndRelevance = View.DynamicMeshElements[MeshBatchIndex];

					checkSlow(MeshBatchAndRelevance.PrimitiveSceneProxy == PrimitiveSceneProxy);

					const FMeshBatch& MeshBatch = *MeshBatchAndRelevance.Mesh;
					bDirty |= TDistortionMeshDrawingPolicyFactory<FDistortMeshAccumulatePolicy>::DrawDynamicMesh(RHICmdList, View, Context, MeshBatch, false, false, MeshBatchAndRelevance.PrimitiveSceneProxy, MeshBatch.BatchHitProxyId);
				}
			}

			// Render static scene prim
			if( ViewRelevance.bStaticRelevance )
			{
				// Render static meshes from static scene prim
				for( int32 StaticMeshIdx = 0; StaticMeshIdx < PrimitiveSceneInfo->StaticMeshes.Num(); StaticMeshIdx++ )
				{
					FStaticMesh& StaticMesh = PrimitiveSceneInfo->StaticMeshes[StaticMeshIdx];
					if( View.StaticMeshVisibilityMap[StaticMesh.Id]
						// Only render static mesh elements using translucent materials
						&& StaticMesh.IsTranslucent(View.GetFeatureLevel()) )
					{
						bDirty |= TDistortionMeshDrawingPolicyFactory<FDistortMeshAccumulatePolicy>::DrawStaticMesh(
							RHICmdList, 
							&View,
							bInitializeOffsets,
							StaticMesh,
							StaticMesh.bRequiresPerElementVisibility ? View.StaticMeshBatchVisibility[StaticMesh.Id] : ((1ull << StaticMesh.Elements.Num()) - 1),
							false,
							PrimitiveSceneProxy,
							StaticMesh.BatchHitProxyId
							);
					}
				}
			}
		}
	}
	return bDirty;
}

int32 FSceneRenderer::GetRefractionQuality(const FSceneViewFamily& ViewFamily)
{
	static const auto ICVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.RefractionQuality"));
	
	int32 Value = 0;
	
	if(ViewFamily.EngineShowFlags.Refraction)
	{
		Value = ICVar->GetValueOnRenderThread();
	}
	
	return Value;
}

/** 
 * Renders the scene's distortion 
 */
void FSceneRenderer::RenderDistortion(FRHICommandListImmediate& RHICmdList)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FSceneRenderer_RenderDistortion);
	SCOPED_DRAW_EVENT(RHICmdList, Distortion);
	SCOPED_GPU_STAT(RHICmdList, Stat_GPU_Distortion);

	// do we need to render the distortion pass?
	bool bRender=false;
	for(int32 ViewIndex = 0;ViewIndex < Views.Num();ViewIndex++)
	{
		const FViewInfo& View = Views[ViewIndex];
		if( View.DistortionPrimSet.NumPrims() > 0)
		{
			bRender=true;
			break;
		}
	}

	bool bDirty = false;

	TRefCountPtr<IPooledRenderTarget> DistortionRT;
	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);
	
	// Use stencil mask to optimize cases with lower screen coverage.
	// Note: This adds an extra pass which is actually slower as distortion tends towards full-screen.
	//       It could be worth testing object screen bounds then reverting to a target flip and single pass.
	const uint8 StencilMaskBit = 0x80;

	// Render accumulated distortion offsets
	if( bRender)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_FSceneRenderer_RenderDistortion_Render);
		SCOPED_DRAW_EVENT(RHICmdList, DistortionAccum);

		// Create a texture to store the resolved light attenuation values, and a render-targetable surface to hold the unresolved light attenuation values.
		{
			FPooledRenderTargetDesc Desc(FPooledRenderTargetDesc::Create2DDesc(SceneContext.GetBufferSizeXY(), PF_B8G8R8A8, FClearValueBinding::Transparent, TexCreate_None, TexCreate_RenderTargetable, false));
			Desc.Flags |= TexCreate_FastVRAM;
			Desc.NumSamples = SceneContext.SceneDepthZ->GetDesc().NumSamples;
			GRenderTargetPool.FindFreeElement(RHICmdList, Desc, DistortionRT, TEXT("Distortion"));

			// use RGBA8 light target for accumulating distortion offsets	
			// R = positive X offset
			// G = positive Y offset
			// B = negative X offset
			// A = negative Y offset
		}

		// DistortionRT==0 should never happen but better we don't crash
		if(DistortionRT)
		{
			FRHIRenderTargetView ColorView( DistortionRT->GetRenderTargetItem().TargetableTexture, 0, -1, ERenderTargetLoadAction::EClear, ERenderTargetStoreAction::EStore );
			FRHIDepthRenderTargetView DepthView( SceneContext.GetSceneDepthSurface(), ERenderTargetLoadAction::ELoad, ERenderTargetStoreAction::ENoAction, FExclusiveDepthStencil::DepthRead_StencilWrite );
			FRHISetRenderTargetsInfo Info( 1, &ColorView, DepthView );		

			RHICmdList.SetRenderTargetsAndClear(Info);

			for(int32 ViewIndex = 0;ViewIndex < Views.Num();ViewIndex++)
			{
				SCOPED_CONDITIONAL_DRAW_EVENTF(RHICmdList, EventView, Views.Num() > 1, TEXT("View%d"), ViewIndex);

				FViewInfo& View = Views[ViewIndex];
				// viewport to match view size
				RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0.0f, View.ViewRect.Max.X, View.ViewRect.Max.Y, 1.0f);

				// test against depth and write stencil mask
				RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false, CF_DepthNearOrEqual, true, CF_Always, SO_Keep, SO_Keep, SO_Replace, false, CF_Always, SO_Keep, SO_Keep, SO_Keep, StencilMaskBit, StencilMaskBit>::GetRHI(), StencilMaskBit);

				// additive blending of offsets (or complexity if the shader complexity viewmode is enabled)
				RHICmdList.SetBlendState(TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_One, BO_Add, BF_One, BF_One>::GetRHI());

				// draw only distortion meshes to accumulate their offsets
				bDirty |= View.DistortionPrimSet.DrawAccumulatedOffsets(RHICmdList, View, false);
			}

			if (bDirty)
			{
				// Ideally we skip the EliminateFastClear since we don't need pixels with no stencil set to be cleared
				RHICmdList.TransitionResource( EResourceTransitionAccess::EReadable, DistortionRT->GetRenderTargetItem().TargetableTexture );
				// to be able to observe results with VisualizeTexture
				GRenderTargetPool.VisualizeTexture.SetCheckPoint(RHICmdList, DistortionRT);
			}
		}
	}

	if(bDirty)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_FSceneRenderer_RenderDistortion_Post);
		SCOPED_DRAW_EVENT(RHICmdList, DistortionApply);

		SceneContext.ResolveSceneColor(RHICmdList, FResolveRect(0, 0, ViewFamily.FamilySizeX, ViewFamily.FamilySizeY));

// OCULUS BEGIN: select ONE render target for all views (eyes)
		TRefCountPtr<IPooledRenderTarget> NewSceneColor;
		// we don't create a new name to make it easier to use "vis SceneColor" and get the last HDRSceneColor
		GRenderTargetPool.FindFreeElement(RHICmdList, SceneContext.GetSceneColor()->GetDesc(), NewSceneColor, TEXT("SceneColor"));
		const FSceneRenderTargetItem& DestRenderTarget = NewSceneColor->GetRenderTargetItem();
// OCULUS END

		// Apply distortion and store off-screen
		SetRenderTarget(RHICmdList, DestRenderTarget.TargetableTexture, SceneContext.GetSceneDepthSurface(), ESimpleRenderTargetMode::EExistingColorAndDepth, FExclusiveDepthStencil::DepthRead_StencilWrite);

		for(int32 ViewIndex = 0, Num = Views.Num(); ViewIndex < Num; ++ViewIndex)
		{
			QUICK_SCOPE_CYCLE_COUNTER(STAT_FSceneRenderer_RenderDistortion_PostView1);
			SCOPED_CONDITIONAL_DRAW_EVENTF(RHICmdList, EventView, Views.Num() > 1, TEXT("View%d"), ViewIndex);

			FViewInfo& View = Views[ViewIndex];

			// useful when we move this into the compositing graph
			FRenderingCompositePassContext Context(RHICmdList, View);

			// Set the view family's render target/viewport.
			Context.SetViewportAndCallRHI(View.ViewRect);

			// test against stencil mask
			RHICmdList.SetBlendState(TStaticBlendState<>::GetRHI());
			RHICmdList.SetRasterizerState(TStaticRasterizerState<>::GetRHI());
			RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always, true, CF_Equal, SO_Keep, SO_Keep, SO_Keep, false, CF_Always, SO_Keep, SO_Keep, SO_Keep, StencilMaskBit, StencilMaskBit>::GetRHI(), StencilMaskBit);

			TShaderMapRef<FPostProcessVS> VertexShader(View.ShaderMap);
			TShaderMapRef<FDistortionApplyScreenPS> PixelShader(View.ShaderMap);

			static FGlobalBoundShaderState BoundShaderState;
				
			SetGlobalBoundShaderState(RHICmdList, View.GetFeatureLevel(), BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);

			VertexShader->SetParameters(Context);
			PixelShader->SetParameters(Context, *DistortionRT);

			// Draw a quad mapping scene color to the view's render target
			DrawRectangle(
				RHICmdList,
				0, 0,
				View.ViewRect.Width(), View.ViewRect.Height(),
				View.ViewRect.Min.X, View.ViewRect.Min.Y, 
				View.ViewRect.Width(), View.ViewRect.Height(),
				View.ViewRect.Size(),
				SceneContext.GetBufferSizeXY(),
				*VertexShader,
				EDRF_UseTriangleOptimization);
		}

		// Resolve and merge distortion to main scene
		RHICmdList.CopyToResolveTarget(DestRenderTarget.TargetableTexture, DestRenderTarget.ShaderResourceTexture, false, FResolveParams());
		SetRenderTarget(RHICmdList, SceneContext.GetSceneColor()->GetRenderTargetItem().TargetableTexture, SceneContext.GetSceneDepthSurface(), ESimpleRenderTargetMode::EExistingColorAndDepth, FExclusiveDepthStencil::DepthRead_StencilWrite, true);

		for(int32 ViewIndex = 0, Num = Views.Num(); ViewIndex < Num; ++ViewIndex)
		{
			QUICK_SCOPE_CYCLE_COUNTER(STAT_FSceneRenderer_RenderDistortion_PostView2);
			SCOPED_CONDITIONAL_DRAW_EVENTF(RHICmdList, EventView, Views.Num() > 1, TEXT("View%d"), ViewIndex);

			FViewInfo& View = Views[ViewIndex];

			// useful when we move this into the compositing graph
			FRenderingCompositePassContext Context(RHICmdList, View);

			// Set the view family's render target/viewport.
			Context.SetViewportAndCallRHI(View.ViewRect);				

			// test against stencil mask and clear it
			RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always, true, CF_Equal, SO_Keep, SO_Keep, SO_Zero, false, CF_Always, SO_Keep, SO_Keep, SO_Keep, StencilMaskBit, StencilMaskBit>::GetRHI(), StencilMaskBit);
					
			static FGlobalBoundShaderState BoundShaderState;
				
			TShaderMapRef<FPostProcessVS> VertexShader(View.ShaderMap);
			TShaderMapRef<FDistortionMergePS> PixelShader(View.ShaderMap);
			SetGlobalBoundShaderState(RHICmdList, View.GetFeatureLevel(), BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);

			VertexShader->SetParameters(Context);
			PixelShader->SetParameters(Context, DestRenderTarget.ShaderResourceTexture);

			DrawRectangle(
				RHICmdList,
				0, 0,
				View.ViewRect.Width(), View.ViewRect.Height(),
				View.ViewRect.Min.X, View.ViewRect.Min.Y, 
				View.ViewRect.Width(), View.ViewRect.Height(),
				View.ViewRect.Size(),
				SceneContext.GetBufferSizeXY(),
				*VertexShader,
				EDRF_UseTriangleOptimization);
		}

		// restore state
		RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false, CF_DepthNearOrEqual>::GetRHI());		
			
		// Distortions RT is no longer needed, buffer can be reused by the pool, see BeginRenderingDistortionAccumulation() call above
		SceneContext.FinishRenderingSceneColor(RHICmdList);
	}
}

void FSceneRenderer::RenderDistortionES2(FRHICommandListImmediate& RHICmdList)
{
	// We need access to HDR scene color
	if (!IsMobileHDR() || IsMobileHDRMosaic())
	{
		return;
	}

	// do we need to render the distortion pass?
	bool bRender=false;
	for(int32 ViewIndex = 0;ViewIndex < Views.Num();ViewIndex++)
	{
		const FViewInfo& View = Views[ViewIndex];
		if( View.DistortionPrimSet.NumPrims() > 0)
		{
			bRender=true;
			break;
		}
	}
			
	if (bRender)
	{
		// Apply distortion
		SCOPED_DRAW_EVENT(RHICmdList, Distortion);
		
		FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

		SceneContext.ResolveSceneColor(RHICmdList, FResolveRect(0, 0, ViewFamily.FamilySizeX, ViewFamily.FamilySizeY));
	
		TRefCountPtr<IPooledRenderTarget> SceneColorDistorted;
		GRenderTargetPool.FindFreeElement(RHICmdList, SceneContext.GetSceneColor()->GetDesc(), SceneColorDistorted, TEXT("SceneColorDistorted"));
		const FSceneRenderTargetItem& DistortedRenderTarget = SceneColorDistorted->GetRenderTargetItem();

		SetRenderTarget(RHICmdList, DistortedRenderTarget.TargetableTexture, SceneContext.GetSceneDepthSurface(), ESimpleRenderTargetMode::EClearColorExistingDepth, FExclusiveDepthStencil::DepthRead_StencilNop);

		// Copy scene color to a new render target
		for(int32 ViewIndex = 0, Num = Views.Num(); ViewIndex < Num; ++ViewIndex)
		{
			SCOPED_CONDITIONAL_DRAW_EVENTF(RHICmdList, EventView, Views.Num() > 1, TEXT("View%d"), ViewIndex);

			FViewInfo& View = Views[ViewIndex];

			// useful when we move this into the compositing graph
			FRenderingCompositePassContext Context(RHICmdList, View);

			// Set the view family's render target/viewport.
			Context.SetViewportAndCallRHI(View.ViewRect);				

			RHICmdList.SetBlendState(TStaticBlendState<>::GetRHI());
			RHICmdList.SetRasterizerState(TStaticRasterizerState<>::GetRHI());
			RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());
					
			static FGlobalBoundShaderState BoundShaderState;
				
			TShaderMapRef<FPostProcessVS> VertexShader(View.ShaderMap);
			TShaderMapRef<FDistortionMergePS> PixelShader(View.ShaderMap);
			SetGlobalBoundShaderState(RHICmdList, View.GetFeatureLevel(), BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);

			VertexShader->SetParameters(Context);
			PixelShader->SetParameters(Context, SceneContext.GetSceneColor()->GetRenderTargetItem().ShaderResourceTexture);

			DrawRectangle(
				RHICmdList,
				0, 0,
				View.ViewRect.Width(), View.ViewRect.Height(),
				View.ViewRect.Min.X, View.ViewRect.Min.Y, 
				View.ViewRect.Width(), View.ViewRect.Height(),
				View.ViewRect.Size(),
				SceneContext.GetBufferSizeXY(),
				*VertexShader,
				EDRF_UseTriangleOptimization);
		}
	
		// Distort scene color in place
		for(int32 ViewIndex = 0, Num = Views.Num(); ViewIndex < Num; ++ViewIndex)
		{
			SCOPED_CONDITIONAL_DRAW_EVENTF(RHICmdList, EventView, Views.Num() > 1, TEXT("View%d"), ViewIndex);

			FViewInfo& View = Views[ViewIndex];

			// useful when we move this into the compositing graph
			FRenderingCompositePassContext Context(RHICmdList, View);

			// Set the view family's render target/viewport.
			Context.SetViewportAndCallRHI(View.ViewRect);

			// test against depth
			RHICmdList.SetBlendState(TStaticBlendState<>::GetRHI());
			RHICmdList.SetRasterizerState(TStaticRasterizerState<>::GetRHI());
			RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false, CF_DepthNearOrEqual>::GetRHI());

			// draw only distortion meshes
			View.DistortionPrimSet.DrawAccumulatedOffsets(RHICmdList, View, false);
		}
	
		// Set distorted scene color as main
		SceneContext.SetSceneColor(SceneColorDistorted);
	}
}