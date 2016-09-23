// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	SceneHitProxyRendering.cpp: Scene hit proxy rendering.
=============================================================================*/

#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "RenderResource.h"

/**
 * A vertex shader for rendering the depth of a mesh.
 */
class FHitProxyVS : public FMeshMaterialShader
{
	DECLARE_SHADER_TYPE(FHitProxyVS,MeshMaterial);

public:

	void SetParameters(FRHICommandList& RHICmdList, const FMaterialRenderProxy* MaterialRenderProxy,const FSceneView& View)
	{
		FMeshMaterialShader::SetParameters(RHICmdList, GetVertexShader(), MaterialRenderProxy, *MaterialRenderProxy->GetMaterial(View.GetFeatureLevel()), View, View.ViewUniformBuffer, ESceneRenderTargetsMode::SetTextures);
	}

	void SetMesh(FRHICommandList& RHICmdList, const FVertexFactory* VertexFactory,const FSceneView& View,const FPrimitiveSceneProxy* Proxy,const FMeshBatchElement& BatchElement,const FMeshDrawingRenderState& DrawRenderState)
	{
		FMeshMaterialShader::SetMesh(RHICmdList, GetVertexShader(),VertexFactory,View,Proxy,BatchElement,DrawRenderState);
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FMeshMaterialShader::Serialize(Ar);
		return bShaderHasOutdatedParameters;
	}

	static bool ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		// Only compile the hit proxy vertex shader on PC
		return IsPCPlatform(Platform)
			// and only compile for the default material or materials that are masked.
			&& (Material->IsSpecialEngineMaterial() || !Material->WritesEveryPixel() || Material->MaterialMayModifyMeshPosition() || Material->IsTwoSided());
	}

protected:

	FHitProxyVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer) :
		FMeshMaterialShader(Initializer)
	{}
	FHitProxyVS() {}
};

IMPLEMENT_MATERIAL_SHADER_TYPE(,FHitProxyVS,TEXT("HitProxyVertexShader"),TEXT("Main"),SF_Vertex); 

/**
 * A hull shader for rendering the depth of a mesh.
 */
class FHitProxyHS : public FBaseHS
{
	DECLARE_SHADER_TYPE(FHitProxyHS,MeshMaterial);
protected:

	FHitProxyHS() {}

	FHitProxyHS(const ShaderMetaType::CompiledShaderInitializerType& Initializer) :
		FBaseHS(Initializer)
	{}

	static bool ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		return FBaseHS::ShouldCache(Platform, Material, VertexFactoryType)
			&& FHitProxyVS::ShouldCache(Platform,Material,VertexFactoryType);
	}
};

/**
 * A domain shader for rendering the depth of a mesh.
 */
class FHitProxyDS : public FBaseDS
{
	DECLARE_SHADER_TYPE(FHitProxyDS,MeshMaterial);

protected:

	FHitProxyDS() {}

	FHitProxyDS(const ShaderMetaType::CompiledShaderInitializerType& Initializer) :
		FBaseDS(Initializer)
	{}

	static bool ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		return FBaseDS::ShouldCache(Platform, Material, VertexFactoryType)
			&& FHitProxyVS::ShouldCache(Platform,Material,VertexFactoryType);
	}
};

IMPLEMENT_MATERIAL_SHADER_TYPE(,FHitProxyHS,TEXT("HitProxyVertexShader"),TEXT("MainHull"),SF_Hull); 
IMPLEMENT_MATERIAL_SHADER_TYPE(,FHitProxyDS,TEXT("HitProxyVertexShader"),TEXT("MainDomain"),SF_Domain);

/**
 * A pixel shader for rendering the HitProxyId of an object as a unique color in the scene.
 */
class FHitProxyPS : public FMeshMaterialShader
{
	DECLARE_SHADER_TYPE(FHitProxyPS,MeshMaterial);
public:

	static bool ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		// Only compile the hit proxy vertex shader on PC
		return IsPCPlatform(Platform) 
			// and only compile for default materials or materials that are masked.
			&& (Material->IsSpecialEngineMaterial() || !Material->WritesEveryPixel() || Material->MaterialMayModifyMeshPosition() || Material->IsTwoSided());
	}

	FHitProxyPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FMeshMaterialShader(Initializer)
	{
		HitProxyId.Bind(Initializer.ParameterMap,TEXT("HitProxyId"), SPF_Mandatory);
	}

	FHitProxyPS() {}

	void SetParameters(FRHICommandList& RHICmdList, const FMaterialRenderProxy* MaterialRenderProxy,const FSceneView& View)
	{
		FMeshMaterialShader::SetParameters(RHICmdList, GetPixelShader(), MaterialRenderProxy, *MaterialRenderProxy->GetMaterial(View.GetFeatureLevel()), View, View.ViewUniformBuffer, ESceneRenderTargetsMode::SetTextures);
	}

	void SetMesh(FRHICommandList& RHICmdList, const FVertexFactory* VertexFactory,const FSceneView& View,const FPrimitiveSceneProxy* Proxy,const FMeshBatchElement& BatchElement,const FMeshDrawingRenderState& DrawRenderState)
	{
		FMeshMaterialShader::SetMesh(RHICmdList, GetPixelShader(),VertexFactory,View,Proxy,BatchElement,DrawRenderState);
	}

	void SetHitProxyId(FRHICommandList& RHICmdList, FHitProxyId HitProxyIdValue)
	{
		SetShaderValue(RHICmdList, GetPixelShader(), HitProxyId, HitProxyIdValue.GetColor().ReinterpretAsLinear());
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FMeshMaterialShader::Serialize(Ar);
		Ar << HitProxyId;
		return bShaderHasOutdatedParameters;
	}

private:
	FShaderParameter HitProxyId;
};

IMPLEMENT_MATERIAL_SHADER_TYPE(,FHitProxyPS,TEXT("HitProxyPixelShader"),TEXT("Main"),SF_Pixel);

FHitProxyDrawingPolicy::FHitProxyDrawingPolicy(
	const FVertexFactory* InVertexFactory,
	const FMaterialRenderProxy* InMaterialRenderProxy,
	ERHIFeatureLevel::Type InFeatureLevel ):
	FMeshDrawingPolicy(InVertexFactory, InMaterialRenderProxy, *InMaterialRenderProxy->GetMaterial(InFeatureLevel))
{
	HullShader = NULL;
	DomainShader = NULL;

	EMaterialTessellationMode MaterialTessellationMode = MaterialResource->GetTessellationMode();
	if (RHISupportsTessellation(GShaderPlatformForFeatureLevel[InFeatureLevel])
		&& InVertexFactory->GetType()->SupportsTessellationShaders() 
		&& MaterialTessellationMode != MTM_NoTessellation)
	{
		HullShader = MaterialResource->GetShader<FHitProxyHS>(VertexFactory->GetType());
		DomainShader = MaterialResource->GetShader<FHitProxyDS>(VertexFactory->GetType());
	}
	VertexShader = MaterialResource->GetShader<FHitProxyVS>(InVertexFactory->GetType());
	PixelShader = MaterialResource->GetShader<FHitProxyPS>(InVertexFactory->GetType());
}

void FHitProxyDrawingPolicy::SetSharedState(FRHICommandList& RHICmdList, const FSceneView* View, const ContextDataType PolicyContext) const
{
	// Set the depth-only shader parameters for the material.
	VertexShader->SetParameters(RHICmdList, MaterialRenderProxy,*View);
	PixelShader->SetParameters(RHICmdList, MaterialRenderProxy,*View);

	if(HullShader && DomainShader)
	{
		HullShader->SetParameters(RHICmdList, MaterialRenderProxy,*View);
		DomainShader->SetParameters(RHICmdList, MaterialRenderProxy,*View);
	}

	// Set the shared mesh resources.
	FMeshDrawingPolicy::SetSharedState(RHICmdList, View, PolicyContext);
}

/** 
* Create bound shader state using the vertex decl from the mesh draw policy
* as well as the shaders needed to draw the mesh
* @return new bound shader state object
*/
FBoundShaderStateInput FHitProxyDrawingPolicy::GetBoundShaderStateInput(ERHIFeatureLevel::Type InFeatureLevel)
{
	return FBoundShaderStateInput(
		FMeshDrawingPolicy::GetVertexDeclaration(), 
		VertexShader->GetVertexShader(),
		GETSAFERHISHADER_HULL(HullShader), 
		GETSAFERHISHADER_DOMAIN(DomainShader), 
		PixelShader->GetPixelShader(),
		FGeometryShaderRHIRef());
}

void FHitProxyDrawingPolicy::SetMeshRenderState(
	FRHICommandList& RHICmdList, 
	const FSceneView& View,
	const FPrimitiveSceneProxy* PrimitiveSceneProxy,
	const FMeshBatch& Mesh,
	int32 BatchElementIndex,
	bool bBackFace,
	const FMeshDrawingRenderState& DrawRenderState,
	const FHitProxyId HitProxyId,
	const ContextDataType PolicyContext
	) const
{
	const FMeshBatchElement& BatchElement = Mesh.Elements[BatchElementIndex];

	VertexShader->SetMesh(RHICmdList, VertexFactory, View, PrimitiveSceneProxy, BatchElement, DrawRenderState);

	if(HullShader && DomainShader)
	{
		HullShader->SetMesh(RHICmdList, VertexFactory, View, PrimitiveSceneProxy, BatchElement, DrawRenderState);
		DomainShader->SetMesh(RHICmdList, VertexFactory, View, PrimitiveSceneProxy, BatchElement, DrawRenderState);
	}

	PixelShader->SetMesh(RHICmdList, VertexFactory, View, PrimitiveSceneProxy, BatchElement, DrawRenderState);
	// Per-instance hitproxies are supplied by the vertex factory
	if( PrimitiveSceneProxy && PrimitiveSceneProxy->HasPerInstanceHitProxies() )
	{
		PixelShader->SetHitProxyId(RHICmdList, FColor(0) );
	}
	else
	{
		PixelShader->SetHitProxyId(RHICmdList, HitProxyId);	
	}

	RHICmdList.SetRasterizerState(GetStaticRasterizerState<false>(
		(Mesh.bWireframe || IsWireframe()) ? FM_Wireframe : FM_Solid,
		((IsTwoSided() && !NeedsBackfacePass()) || Mesh.bDisableBackfaceCulling) ? CM_None :
			(XOR(XOR(View.bReverseCulling,bBackFace), Mesh.ReverseCulling) ? CM_CCW : CM_CW)
		));
}

#if WITH_EDITOR

int32 FEditorSelectionDrawingPolicy::PrimitiveSelectionIndex;
int32 FEditorSelectionDrawingPolicy::IndividuallySelectedProxyIndex;

void FEditorSelectionDrawingPolicy::SetMeshRenderState( FRHICommandList& RHICmdList, const FSceneView& View, const FPrimitiveSceneProxy* PrimitiveSceneProxy, const FMeshBatch& Mesh, int32 BatchElementIndex, bool bBackFace, const FMeshDrawingRenderState& DrawRenderState, const FHitProxyId HitProxyId, const ContextDataType PolicyContext )
{
	FHitProxyDrawingPolicy::SetMeshRenderState(RHICmdList, View, PrimitiveSceneProxy, Mesh, BatchElementIndex, bBackFace, DrawRenderState, HitProxyId, PolicyContext);

	int32 StencilValue = GetStencilValue(View, PrimitiveSceneProxy);
		
	RHICmdList.SetDepthStencilState(TStaticDepthStencilState<true, CF_DepthNearOrEqual, true, CF_Always, SO_Keep, SO_Keep, SO_Replace>::GetRHI(), StencilValue);

	//RHICmdList.SetRasterizerState(TStaticRasterizerState<>::GetRHI());
	RHICmdList.SetBlendState(TStaticBlendStateWriteMask<CW_NONE, CW_NONE, CW_NONE, CW_NONE>::GetRHI());

}

int32 FEditorSelectionDrawingPolicy::GetStencilValue(const FSceneView& View, const FPrimitiveSceneProxy* PrimitiveSceneProxy)
{
	const bool bActorSelectionColorIsSubdued = View.bHasSelectedComponents;

	int32 StencilValue = 0;
	if(PrimitiveSceneProxy->IsIndividuallySelected())
	{
		// Any component that is individually selected should have a stencil value of < 128 so that it can have a unique color.  We offset the value by 2 because 0 means no selection and 1 is for bsp
		StencilValue = IndividuallySelectedProxyIndex % 126 + 2;
		++IndividuallySelectedProxyIndex;
	}
	else
	{
			
		// If we are subduing actor color highlight then use the top level bits to indicate that to the shader.  
		StencilValue = bActorSelectionColorIsSubdued ? PrimitiveSelectionIndex % 128 + 128 : PrimitiveSelectionIndex % 126 + 2;
		++PrimitiveSelectionIndex;
	}

	return StencilValue;
}

void FEditorSelectionDrawingPolicy::ResetStencilValues()
{
	PrimitiveSelectionIndex = 0;
	IndividuallySelectedProxyIndex = 0;
}

#endif

void FHitProxyDrawingPolicyFactory::AddStaticMesh(FScene* Scene,FStaticMesh* StaticMesh,ContextType)
{
	checkSlow( Scene->RequiresHitProxies() );

	// Add the static mesh to the DPG's hit proxy draw list.
	const FMaterialRenderProxy* MaterialRenderProxy = StaticMesh->MaterialRenderProxy;
	const FMaterial* Material = MaterialRenderProxy->GetMaterial(Scene->GetFeatureLevel());
	if (Material->WritesEveryPixel() && !Material->IsTwoSided() && !Material->MaterialModifiesMeshPosition_RenderThread())
	{
		// Default material doesn't handle masked, and doesn't have the correct bIsTwoSided setting.
		MaterialRenderProxy = UMaterial::GetDefaultMaterial(MD_Surface)->GetRenderProxy(false);
	}
	
	Scene->HitProxyDrawList.AddMesh(
		StaticMesh,
		StaticMesh->BatchHitProxyId,
		FHitProxyDrawingPolicy(StaticMesh->VertexFactory, MaterialRenderProxy, Scene->GetFeatureLevel()),
		Scene->GetFeatureLevel()
		);

#if WITH_EDITOR

	Scene->EditorSelectionDrawList.AddMesh(
		StaticMesh,
		StaticMesh->BatchHitProxyId,
		FEditorSelectionDrawingPolicy(StaticMesh->VertexFactory, MaterialRenderProxy, Scene->GetFeatureLevel()),
		Scene->GetFeatureLevel());

	// If the mesh isn't translucent then we'll also add it to the "opaque-only" draw list.  Depending
	// on user preferences in the editor, we may use this draw list to disallow selection of
	// translucent objects in perspective viewports
	if( !IsTranslucentBlendMode( Material->GetBlendMode() ) )
	{
		Scene->HitProxyDrawList_OpaqueOnly.AddMesh(
			StaticMesh,
			StaticMesh->BatchHitProxyId,
			FHitProxyDrawingPolicy(StaticMesh->VertexFactory, MaterialRenderProxy, Scene->GetFeatureLevel()),
			Scene->GetFeatureLevel()
			);
	}
#endif

}

bool FHitProxyDrawingPolicyFactory::DrawDynamicMesh(
	FRHICommandList& RHICmdList, 
	const FSceneView& View,
	ContextType DrawingContext,
	const FMeshBatch& Mesh,
	bool bBackFace,
	bool bPreFog,
	const FPrimitiveSceneProxy* PrimitiveSceneProxy,
	FHitProxyId HitProxyId
	)
{
	if (!PrimitiveSceneProxy || PrimitiveSceneProxy->IsSelectable())
	{
		const FMaterialRenderProxy* MaterialRenderProxy = Mesh.MaterialRenderProxy;
		const FMaterial* Material = MaterialRenderProxy->GetMaterial(View.GetFeatureLevel());
		if( HitProxyId != FHitProxyId::InvisibleHitProxyId )
		{
#if WITH_EDITOR
			const HHitProxy* HitProxy = GetHitProxyById( HitProxyId );
			// Only draw translucent primitives to the hit proxy if the user wants those objects to be selectable
			if( View.bAllowTranslucentPrimitivesInHitProxy || !IsTranslucentBlendMode( Material->GetBlendMode() ) || ( HitProxy && HitProxy->AlwaysAllowsTranslucentPrimitives() ) )
#endif
		    {
			    if (Material->WritesEveryPixel() && !Material->IsTwoSided() && !Material->MaterialModifiesMeshPosition_RenderThread())
			    {
				    // Default material doesn't handle masked, and doesn't have the correct bIsTwoSided setting.
				    MaterialRenderProxy = UMaterial::GetDefaultMaterial(MD_Surface)->GetRenderProxy(false);
			    }
			    FHitProxyDrawingPolicy DrawingPolicy( Mesh.VertexFactory, MaterialRenderProxy, View.GetFeatureLevel() );
			    RHICmdList.BuildAndSetLocalBoundShaderState(DrawingPolicy.GetBoundShaderStateInput(View.GetFeatureLevel()));
			    DrawingPolicy.SetSharedState(RHICmdList, &View, FHitProxyDrawingPolicy::ContextDataType());
			    const FMeshDrawingRenderState DrawRenderState(Mesh.DitheredLODTransitionAlpha);
			    for (int32 BatchElementIndex = 0; BatchElementIndex < Mesh.Elements.Num(); BatchElementIndex++)
			    {
				    DrawingPolicy.SetMeshRenderState(RHICmdList, View, PrimitiveSceneProxy, Mesh, BatchElementIndex, bBackFace, DrawRenderState, HitProxyId, FHitProxyDrawingPolicy::ContextDataType());
				    DrawingPolicy.DrawMesh(RHICmdList, Mesh,BatchElementIndex);
			    }
			    return true;
			}
		}
	}
	return false;
}

#if WITH_EDITOR

TRefCountPtr<IPooledRenderTarget> InitHitProxyRender(FRHICommandListImmediate& RHICmdList, const FSceneRenderer* SceneRenderer)
{
	auto& ViewFamily = SceneRenderer->ViewFamily;
	auto FeatureLevel = ViewFamily.Scene->GetFeatureLevel();

	// Initialize global system textures (pass-through if already initialized).
	GSystemTextures.InitializeTextures(RHICmdList, FeatureLevel);

	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);
	// Allocate the maximum scene render target space for the current view family.
	SceneContext.Allocate(RHICmdList, ViewFamily);

	TRefCountPtr<IPooledRenderTarget> HitProxyRT;
	TRefCountPtr<IPooledRenderTarget> HitProxyDepthRT;

	// Create a texture to store the resolved light attenuation values, and a render-targetable surface to hold the unresolved light attenuation values.
	{
		FPooledRenderTargetDesc Desc(FPooledRenderTargetDesc::Create2DDesc(SceneContext.GetBufferSizeXY(), PF_B8G8R8A8, FClearValueBinding::None, TexCreate_None, TexCreate_RenderTargetable, false));
		Desc.Flags |= TexCreate_FastVRAM;
		GRenderTargetPool.FindFreeElement(RHICmdList, Desc, HitProxyRT, TEXT("HitProxy"));

		// create non-MSAA version for hit proxies on PC if needed
		const EShaderPlatform CurrentShaderPlatform = GShaderPlatformForFeatureLevel[FeatureLevel];
		FPooledRenderTargetDesc DepthDesc = SceneContext.SceneDepthZ->GetDesc();

		if (DepthDesc.NumSamples > 1 && RHISupportsSeparateMSAAAndResolveTextures(CurrentShaderPlatform))
		{
			DepthDesc.NumSamples = 1;
			GRenderTargetPool.FindFreeElement(RHICmdList, DepthDesc, HitProxyDepthRT, TEXT("NoMSAASceneDepthZ"));
		}
		else
		{
			HitProxyDepthRT = SceneContext.SceneDepthZ;
		}
	}

	if (!HitProxyRT)
	{
		// HitProxyRT==0 should never happen but better we don't crash
		return HitProxyRT;
	}

	SetRenderTarget(RHICmdList, HitProxyRT->GetRenderTargetItem().TargetableTexture, HitProxyDepthRT->GetRenderTargetItem().TargetableTexture, ESimpleRenderTargetMode::EExistingColorAndDepth, FExclusiveDepthStencil::DepthWrite_StencilWrite, true);

	// Clear color for each view.
	auto& Views = SceneRenderer->Views;
	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		const FViewInfo& View = Views[ViewIndex];
		RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0.0f, View.ViewRect.Max.X, View.ViewRect.Max.Y, 1.0f);
		RHICmdList.Clear(true, FLinearColor::White, false, (float)ERHIZBuffer::FarPlane, false, 0, FIntRect());
	}
	return HitProxyRT;
}

void RenderHitProxies(FRHICommandListImmediate& RHICmdList, const FSceneRenderer* SceneRenderer, TRefCountPtr<IPooledRenderTarget> HitProxyRT)
{
	auto & ViewFamily = SceneRenderer->ViewFamily;
	auto & Views = SceneRenderer->Views;

	const auto FeatureLevel = SceneRenderer->FeatureLevel;

	// Dynamic vertex and index buffers need to be committed before rendering.
	FGlobalDynamicVertexBuffer::Get().Commit();
	FGlobalDynamicIndexBuffer::Get().Commit();

	// Depth tests + writes, no alpha blending.
	RHICmdList.SetDepthStencilState(TStaticDepthStencilState<true, CF_DepthNearOrEqual>::GetRHI());
	RHICmdList.SetBlendState(TStaticBlendState<>::GetRHI());

	const bool bNeedToSwitchVerticalAxis = RHINeedsToSwitchVerticalAxis(GShaderPlatformForFeatureLevel[SceneRenderer->FeatureLevel]);
	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		const FViewInfo& View = Views[ViewIndex];

		// Set the device viewport for the view.
		RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0.0f, View.ViewRect.Max.X, View.ViewRect.Max.Y, 1.0f);

		// Clear the depth buffer for each DPG.
		RHICmdList.Clear(false, FLinearColor::Black, true, (float)ERHIZBuffer::FarPlane, true, 0, FIntRect());

		// Adjust the visibility map for this view
		if (!View.bAllowTranslucentPrimitivesInHitProxy)
		{
			// Draw the scene's hit proxy draw lists. (opaque primitives only)
			SceneRenderer->Scene->HitProxyDrawList_OpaqueOnly.DrawVisible(RHICmdList, View, View.StaticMeshVisibilityMap, View.StaticMeshBatchVisibility);
		}
		else
		{
			// Draw the scene's hit proxy draw lists.
			SceneRenderer->Scene->HitProxyDrawList.DrawVisible(RHICmdList, View, View.StaticMeshVisibilityMap, View.StaticMeshBatchVisibility);
		}

		const bool bPreFog = true;
		const bool bHitTesting = true;

		FHitProxyDrawingPolicyFactory::ContextType DrawingContext;

		for (int32 MeshBatchIndex = 0; MeshBatchIndex < View.DynamicMeshElements.Num(); MeshBatchIndex++)
		{
			const FMeshBatchAndRelevance& MeshBatchAndRelevance = View.DynamicMeshElements[MeshBatchIndex];
			const FMeshBatch& MeshBatch = *MeshBatchAndRelevance.Mesh;

			if (MeshBatch.bSelectable)
			{
				const FHitProxyId EffectiveHitProxyId = MeshBatch.BatchHitProxyId == FHitProxyId() ? MeshBatchAndRelevance.PrimitiveSceneProxy->GetPrimitiveSceneInfo()->DefaultDynamicHitProxyId : MeshBatch.BatchHitProxyId;
				FHitProxyDrawingPolicyFactory::DrawDynamicMesh(RHICmdList, View, DrawingContext, MeshBatch, false, bPreFog, MeshBatchAndRelevance.PrimitiveSceneProxy, EffectiveHitProxyId);
			}
		}

		View.SimpleElementCollector.DrawBatchedElements(RHICmdList, View, FTexture2DRHIRef(), EBlendModeFilter::All);

		for (int32 MeshBatchIndex = 0; MeshBatchIndex < View.DynamicEditorMeshElements.Num(); MeshBatchIndex++)
		{
			const FMeshBatchAndRelevance& MeshBatchAndRelevance = View.DynamicEditorMeshElements[MeshBatchIndex];
			const FMeshBatch& MeshBatch = *MeshBatchAndRelevance.Mesh;

			if (MeshBatch.bSelectable)
			{
				const FHitProxyId EffectiveHitProxyId = MeshBatch.BatchHitProxyId == FHitProxyId() ? MeshBatchAndRelevance.PrimitiveSceneProxy->GetPrimitiveSceneInfo()->DefaultDynamicHitProxyId : MeshBatch.BatchHitProxyId;
				FHitProxyDrawingPolicyFactory::DrawDynamicMesh(RHICmdList, View, DrawingContext, MeshBatch, false, bPreFog, MeshBatchAndRelevance.PrimitiveSceneProxy, EffectiveHitProxyId);
			}
		}

		View.EditorSimpleElementCollector.DrawBatchedElements(RHICmdList, View, FTexture2DRHIRef(), EBlendModeFilter::All);
		
		// Draw the view's elements.
		DrawViewElements<FHitProxyDrawingPolicyFactory>(RHICmdList, View, FHitProxyDrawingPolicyFactory::ContextType(), SDPG_World, bPreFog);

		// Draw the view's batched simple elements(lines, sprites, etc).
		View.BatchedViewElements.Draw(RHICmdList, FeatureLevel, bNeedToSwitchVerticalAxis, View.ViewProjectionMatrix, View.ViewRect.Width(), View.ViewRect.Height(), true);

		// Some elements should never be occluded (e.g. gizmos).
		// So we render those twice, first to overwrite potentially nearer objects,
		// then again to allows proper occlusion within those elements.
		RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());
		// Draw the view's foreground elements last.
		DrawViewElements<FHitProxyDrawingPolicyFactory>(RHICmdList, View, FHitProxyDrawingPolicyFactory::ContextType(), SDPG_Foreground, bPreFog);

		View.TopBatchedViewElements.Draw(RHICmdList, FeatureLevel, bNeedToSwitchVerticalAxis, View.ViewProjectionMatrix, View.ViewRect.Width(), View.ViewRect.Height(), true);

		RHICmdList.SetDepthStencilState(TStaticDepthStencilState<true, CF_DepthNearOrEqual>::GetRHI());
		// Draw the view's foreground elements last.
		DrawViewElements<FHitProxyDrawingPolicyFactory>(RHICmdList, View, FHitProxyDrawingPolicyFactory::ContextType(), SDPG_Foreground, bPreFog);

		View.TopBatchedViewElements.Draw(RHICmdList, FeatureLevel, bNeedToSwitchVerticalAxis, View.ViewProjectionMatrix, View.ViewRect.Width(), View.ViewRect.Height(), true);
	}

	// Finish drawing to the hit proxy render target.
	RHICmdList.CopyToResolveTarget(HitProxyRT->GetRenderTargetItem().TargetableTexture, HitProxyRT->GetRenderTargetItem().ShaderResourceTexture, false, FResolveParams());
	RHICmdList.CopyToResolveTarget(SceneContext.GetSceneDepthSurface(), SceneContext.GetSceneDepthSurface(), true, FResolveParams());

	// to be able to observe results with VisualizeTexture
	GRenderTargetPool.VisualizeTexture.SetCheckPoint(RHICmdList, HitProxyRT);

	// After scene rendering, disable the depth buffer.
	RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());

	//
	// Copy the hit proxy buffer into the view family's render target.
	//
	
	// Set up a FTexture that is used to draw the hit proxy buffer to the view family's render target.
	FTexture HitProxyRenderTargetTexture;
	HitProxyRenderTargetTexture.TextureRHI = HitProxyRT->GetRenderTargetItem().ShaderResourceTexture;
	HitProxyRenderTargetTexture.SamplerStateRHI = TStaticSamplerState<>::GetRHI();

	// Generate the vertices and triangles mapping the hit proxy RT pixels into the view family's RT pixels.
	FBatchedElements BatchedElements;
	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		const FViewInfo& View = Views[ViewIndex];

		FIntPoint BufferSize = SceneContext.GetBufferSizeXY();
		float InvBufferSizeX = 1.0f / BufferSize.X;
		float InvBufferSizeY = 1.0f / BufferSize.Y;

		const float U0 = View.ViewRect.Min.X * InvBufferSizeX;
		const float V0 = View.ViewRect.Min.Y * InvBufferSizeY;
		const float U1 = View.ViewRect.Max.X * InvBufferSizeX;
		const float V1 = View.ViewRect.Max.Y * InvBufferSizeY;

		const int32 V00 = BatchedElements.AddVertex(FVector4(View.ViewRect.Min.X,	View.ViewRect.Min.Y, 0, 1),FVector2D(U0, V0),	FLinearColor::White, FHitProxyId());
		const int32 V10 = BatchedElements.AddVertex(FVector4(View.ViewRect.Max.X,	View.ViewRect.Min.Y,	0, 1),FVector2D(U1, V0),	FLinearColor::White, FHitProxyId());
		const int32 V01 = BatchedElements.AddVertex(FVector4(View.ViewRect.Min.X,	View.ViewRect.Max.Y,	0, 1),FVector2D(U0, V1),	FLinearColor::White, FHitProxyId());
		const int32 V11 = BatchedElements.AddVertex(FVector4(View.ViewRect.Max.X,	View.ViewRect.Max.Y,	0, 1),FVector2D(U1, V1),	FLinearColor::White, FHitProxyId());

		BatchedElements.AddTriangle(V00,V10,V11,&HitProxyRenderTargetTexture,BLEND_Opaque);
		BatchedElements.AddTriangle(V00,V11,V01,&HitProxyRenderTargetTexture,BLEND_Opaque);
	}

	// Generate a transform which maps from view family RT pixel coordinates to Normalized Device Coordinates.
	FIntPoint RenderTargetSize = ViewFamily.RenderTarget->GetSizeXY();

	const FMatrix PixelToView =
		FTranslationMatrix(FVector(0, 0, 0)) *
			FMatrix(
				FPlane(	1.0f / ((float)RenderTargetSize.X / 2.0f),	0.0,										0.0f,	0.0f	),
				FPlane(	0.0f,										-GProjectionSignY / ((float)RenderTargetSize.Y / 2.0f),	0.0f,	0.0f	),
				FPlane(	0.0f,										0.0f,										1.0f,	0.0f	),
				FPlane(	-1.0f,										GProjectionSignY,										0.0f,	1.0f	)
				);

	// Draw the triangles to the view family's render target.
	SetRenderTarget(RHICmdList, ViewFamily.RenderTarget->GetRenderTargetTexture(), FTextureRHIRef());
	BatchedElements.Draw(
				RHICmdList,
				FeatureLevel,
				bNeedToSwitchVerticalAxis,
				PixelToView,
				RenderTargetSize.X,
				RenderTargetSize.Y,
				false,
				1.0f
				);

	RHICmdList.EndScene();
}
#endif

void FMobileSceneRenderer::RenderHitProxies(FRHICommandListImmediate& RHICmdList)
{
#if WITH_EDITOR
	TRefCountPtr<IPooledRenderTarget> HitProxyRT = ::InitHitProxyRender(RHICmdList, this);
	// HitProxyRT==0 should never happen but better we don't crash
	if (HitProxyRT)
	{
		// Find the visible primitives.
		InitViews(RHICmdList);
		::RenderHitProxies(RHICmdList, this, HitProxyRT);
	}
#endif
}

void FDeferredShadingSceneRenderer::RenderHitProxies(FRHICommandListImmediate& RHICmdList)
{
#if WITH_EDITOR
	TRefCountPtr<IPooledRenderTarget> HitProxyRT = ::InitHitProxyRender(RHICmdList, this);
	// HitProxyRT==0 should never happen but better we don't crash
	if (HitProxyRT)
	{
		// Find the visible primitives.
		FGraphEventArray SortEvents;
		FILCUpdatePrimTaskData ILCTaskData;
		bool bDoInitViewAftersPrepass = InitViews(RHICmdList, ILCTaskData, SortEvents);
		if (bDoInitViewAftersPrepass)
		{
			InitViewsPossiblyAfterPrepass(RHICmdList, ILCTaskData, SortEvents);
		}
		::RenderHitProxies(RHICmdList, this, HitProxyRT);
		ClearPrimitiveSingleFramePrecomputedLightingBuffers();
	}
#endif
}
