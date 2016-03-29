// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	DepthRendering.cpp: Depth rendering implementation.
=============================================================================*/

#include "RendererPrivate.h"
#include "ScenePrivate.h"

/**
 * A vertex shader for rendering the depth of a mesh.
 */
template <bool bUsePositionOnlyStream>
class TDepthOnlyVS : public FMeshMaterialShader
{
	DECLARE_SHADER_TYPE(TDepthOnlyVS,MeshMaterial);
protected:

	TDepthOnlyVS() {}
	TDepthOnlyVS(const FMeshMaterialShaderType::CompiledShaderInitializerType& Initializer) :
		FMeshMaterialShader(Initializer)
	{
		InstancedEyeIndexParameter.Bind(Initializer.ParameterMap, TEXT("InstancedEyeIndex"));
		IsInstancedStereoParameter.Bind(Initializer.ParameterMap, TEXT("bIsInstancedStereo"));
		NeedsInstancedStereoBiasParameter.Bind(Initializer.ParameterMap, TEXT("bNeedsInstancedStereoBias"));
	}

private:

	FShaderParameter InstancedEyeIndexParameter;
	FShaderParameter IsInstancedStereoParameter;
	FShaderParameter NeedsInstancedStereoBiasParameter;

public:

	static bool ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		// Only the local vertex factory supports the position-only stream
		if (bUsePositionOnlyStream)
		{
			return VertexFactoryType->SupportsPositionOnly() && Material->IsSpecialEngineMaterial();
		}

		// Only compile for the default material and masked materials
		return (Material->IsSpecialEngineMaterial() || !Material->WritesEveryPixel() || Material->MaterialMayModifyMeshPosition());
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		const bool result = FMeshMaterialShader::Serialize(Ar);
		Ar << InstancedEyeIndexParameter;
		Ar << IsInstancedStereoParameter;
		Ar << NeedsInstancedStereoBiasParameter;
		return result;
	}

	void SetParameters(
		FRHICommandList& RHICmdList, 
		const FMaterialRenderProxy* MaterialRenderProxy, 
		const FMaterial& MaterialResource, 
		const FSceneView& View, 
		const bool bIsInstancedStereo, 
		const bool bNeedsInstancedStereoBias
		)
	{
		FMeshMaterialShader::SetParameters(RHICmdList, GetVertexShader(),MaterialRenderProxy,MaterialResource,View,ESceneRenderTargetsMode::DontSet);
		
		if (IsInstancedStereoParameter.IsBound())
		{
			SetShaderValue(RHICmdList, GetVertexShader(), IsInstancedStereoParameter, bIsInstancedStereo);
		}

		if (NeedsInstancedStereoBiasParameter.IsBound())
		{
			SetShaderValue(RHICmdList, GetVertexShader(), NeedsInstancedStereoBiasParameter, bNeedsInstancedStereoBias);
		}
	}

	void SetMesh(FRHICommandList& RHICmdList, const FVertexFactory* VertexFactory,const FSceneView& View,const FPrimitiveSceneProxy* Proxy,const FMeshBatchElement& BatchElement,const FMeshDrawingRenderState& DrawRenderState)
	{
		FMeshMaterialShader::SetMesh(RHICmdList, GetVertexShader(),VertexFactory,View,Proxy,BatchElement,DrawRenderState);
	}

	void SetInstancedEyeIndex(FRHICommandList& RHICmdList, const uint32 EyeIndex) {
		if (InstancedEyeIndexParameter.IsBound())
		{
			SetShaderValue(RHICmdList, GetVertexShader(), InstancedEyeIndexParameter, EyeIndex);
		}
	}
};

/**
 * Hull shader for depth rendering
 */
class FDepthOnlyHS : public FBaseHS
{
	DECLARE_SHADER_TYPE(FDepthOnlyHS,MeshMaterial);
public:

	static bool ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		return FBaseHS::ShouldCache(Platform, Material, VertexFactoryType)
			&& TDepthOnlyVS<false>::ShouldCache(Platform,Material,VertexFactoryType);
	}

	FDepthOnlyHS(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FBaseHS(Initializer)
	{}

	FDepthOnlyHS() {}
};

/**
 * Domain shader for depth rendering
 */
class FDepthOnlyDS : public FBaseDS
{
	DECLARE_SHADER_TYPE(FDepthOnlyDS,MeshMaterial);
public:

	static bool ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		return FBaseDS::ShouldCache(Platform, Material, VertexFactoryType)
			&& TDepthOnlyVS<false>::ShouldCache(Platform,Material,VertexFactoryType);		
	}

	FDepthOnlyDS(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FBaseDS(Initializer)
	{}

	FDepthOnlyDS() {}
};

IMPLEMENT_MATERIAL_SHADER_TYPE(template<>,TDepthOnlyVS<true>,TEXT("PositionOnlyDepthVertexShader"),TEXT("Main"),SF_Vertex);
IMPLEMENT_MATERIAL_SHADER_TYPE(template<>,TDepthOnlyVS<false>,TEXT("DepthOnlyVertexShader"),TEXT("Main"),SF_Vertex);
IMPLEMENT_MATERIAL_SHADER_TYPE(,FDepthOnlyHS,TEXT("DepthOnlyVertexShader"),TEXT("MainHull"),SF_Hull);	
IMPLEMENT_MATERIAL_SHADER_TYPE(,FDepthOnlyDS,TEXT("DepthOnlyVertexShader"),TEXT("MainDomain"),SF_Domain);

/**
* A pixel shader for rendering the depth of a mesh.
*/
class FDepthOnlyPS : public FMeshMaterialShader
{
	DECLARE_SHADER_TYPE(FDepthOnlyPS,MeshMaterial);
public:

	static bool ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		// Compile for materials that are masked.
		return (!Material->WritesEveryPixel() || Material->HasPixelDepthOffsetConnected());
	}

	FDepthOnlyPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FMeshMaterialShader(Initializer)
	{}

	FDepthOnlyPS() {}

	void SetParameters(FRHICommandList& RHICmdList, const FMaterialRenderProxy* MaterialRenderProxy,const FMaterial& MaterialResource,const FSceneView* View)
	{
		FMeshMaterialShader::SetParameters(RHICmdList, GetPixelShader(),MaterialRenderProxy,MaterialResource,*View,ESceneRenderTargetsMode::DontSet);
	}

	void SetMesh(FRHICommandList& RHICmdList, const FVertexFactory* VertexFactory,const FSceneView& View,const FPrimitiveSceneProxy* Proxy,const FMeshBatchElement& BatchElement,const FMeshDrawingRenderState& DrawRenderState)
	{
		FMeshMaterialShader::SetMesh(RHICmdList, GetPixelShader(),VertexFactory,View,Proxy,BatchElement,DrawRenderState);
	}
};

IMPLEMENT_MATERIAL_SHADER_TYPE(,FDepthOnlyPS,TEXT("DepthOnlyPixelShader"),TEXT("Main"),SF_Pixel);

IMPLEMENT_SHADERPIPELINE_TYPE_VS(DepthNoPixelPipeline, TDepthOnlyVS<false>, true);
IMPLEMENT_SHADERPIPELINE_TYPE_VS(DepthPosOnlyNoPixelPipeline, TDepthOnlyVS<true>, true);
IMPLEMENT_SHADERPIPELINE_TYPE_VSPS(DepthPipeline, TDepthOnlyVS<false>, FDepthOnlyPS, true);


static FORCEINLINE bool UseShaderPipelines()
{
	static const auto* CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.ShaderPipelines"));
	return CVar && CVar->GetValueOnAnyThread() != 0;
}

FDepthDrawingPolicy::FDepthDrawingPolicy(
	const FVertexFactory* InVertexFactory,
	const FMaterialRenderProxy* InMaterialRenderProxy,
	const FMaterial& InMaterialResource,
	bool bIsTwoSided,
	ERHIFeatureLevel::Type InFeatureLevel
	):
	FMeshDrawingPolicy(InVertexFactory,InMaterialRenderProxy,InMaterialResource,DVSM_None,/*bInTwoSidedOverride=*/ bIsTwoSided)
{
	bNeedsPixelShader = (!InMaterialResource.WritesEveryPixel() || InMaterialResource.HasPixelDepthOffsetConnected());
	if (!bNeedsPixelShader)
	{
		PixelShader = nullptr;
	}

	const EMaterialTessellationMode TessellationMode = InMaterialResource.GetTessellationMode();
	if (RHISupportsTessellation(GShaderPlatformForFeatureLevel[InFeatureLevel])
		&& InVertexFactory->GetType()->SupportsTessellationShaders() 
		&& TessellationMode != MTM_NoTessellation)
	{
		ShaderPipeline = nullptr;
		VertexShader = InMaterialResource.GetShader<TDepthOnlyVS<false> >(VertexFactory->GetType());
		HullShader = InMaterialResource.GetShader<FDepthOnlyHS>(VertexFactory->GetType());
		DomainShader = InMaterialResource.GetShader<FDepthOnlyDS>(VertexFactory->GetType());
		if (bNeedsPixelShader)
		{
			PixelShader = InMaterialResource.GetShader<FDepthOnlyPS>(InVertexFactory->GetType());
		}
	}
	else
	{
		HullShader = nullptr;
		DomainShader = nullptr;
		bool bUseShaderPipelines = UseShaderPipelines();
		if (bNeedsPixelShader)
		{
			ShaderPipeline = bUseShaderPipelines ? InMaterialResource.GetShaderPipeline(&DepthPipeline, InVertexFactory->GetType()) : nullptr;
	}
	else
	{
			ShaderPipeline = bUseShaderPipelines ? InMaterialResource.GetShaderPipeline(&DepthNoPixelPipeline, InVertexFactory->GetType()) : nullptr;
	}

		if (ShaderPipeline)
		{
			VertexShader = ShaderPipeline->GetShader<TDepthOnlyVS<false> >();
			if (bNeedsPixelShader)
			{
				PixelShader = ShaderPipeline->GetShader<FDepthOnlyPS>();
			}
		}
		else
		{
			VertexShader = InMaterialResource.GetShader<TDepthOnlyVS<false> >(VertexFactory->GetType());
			if (bNeedsPixelShader)
			{
				PixelShader = InMaterialResource.GetShader<FDepthOnlyPS>(InVertexFactory->GetType());
			}
		}
	}
}

void FDepthDrawingPolicy::SetInstancedEyeIndex(FRHICommandList& RHICmdList, const uint32 EyeIndex) const {
	VertexShader->SetInstancedEyeIndex(RHICmdList, EyeIndex);
}

void FDepthDrawingPolicy::SetSharedState(FRHICommandList& RHICmdList, const FSceneView* View, const ContextDataType PolicyContext) const
{
	// Set the depth-only shader parameters for the material.
	VertexShader->SetParameters(RHICmdList, MaterialRenderProxy, *MaterialResource, *View, PolicyContext.bIsInstancedStereo, PolicyContext.bNeedsInstancedStereoBias);
	if(HullShader && DomainShader)
	{
		HullShader->SetParameters(RHICmdList, MaterialRenderProxy,*View);
		DomainShader->SetParameters(RHICmdList, MaterialRenderProxy,*View);
	}

	if (bNeedsPixelShader)
	{
		PixelShader->SetParameters(RHICmdList, MaterialRenderProxy,*MaterialResource,View);
	}

	// Set the shared mesh resources.
	FMeshDrawingPolicy::SetSharedState(RHICmdList, View, PolicyContext);
}

/**
* Sets the correct depth-stencil state for dithered LOD transitions using the stencil optimization
* @return Was a new state was set
*/
FORCEINLINE bool SetDitheredLODDepthStencilState(FRHICommandList& RHICmdList, const FMeshDrawingRenderState& DrawRenderState)
{
	if (DrawRenderState.bAllowStencilDither)
	{
		if (DrawRenderState.DitheredLODState == EDitheredLODState::None)
		{
			RHICmdList.SetDepthStencilState(TStaticDepthStencilState<>::GetRHI());
		}
		else
		{
			const uint32 StencilRef = (DrawRenderState.DitheredLODState == EDitheredLODState::FadeOut) ? STENCIL_SANDBOX_MASK : 0;

			RHICmdList.SetDepthStencilState(TStaticDepthStencilState<true, CF_DepthNearOrEqual,
				true, CF_Equal, SO_Keep, SO_Keep, SO_Keep,
				false, CF_Always, SO_Keep, SO_Keep, SO_Keep,
				STENCIL_SANDBOX_MASK, STENCIL_SANDBOX_MASK>::GetRHI(), StencilRef);
		}

		return true;
	}

	return false;
}

/** 
* Create bound shader state using the vertex decl from the mesh draw policy
* as well as the shaders needed to draw the mesh
* @return new bound shader state object
*/
FBoundShaderStateInput FDepthDrawingPolicy::GetBoundShaderStateInput(ERHIFeatureLevel::Type InFeatureLevel)
{
	return FBoundShaderStateInput(
		FMeshDrawingPolicy::GetVertexDeclaration(), 
		VertexShader->GetVertexShader(),
		GETSAFERHISHADER_HULL(HullShader), 
		GETSAFERHISHADER_DOMAIN(DomainShader),
		bNeedsPixelShader ? PixelShader->GetPixelShader() : NULL,
		NULL);
}

void FDepthDrawingPolicy::SetMeshRenderState(
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
	VertexShader->SetMesh(RHICmdList, VertexFactory,View,PrimitiveSceneProxy,BatchElement,DrawRenderState);
	if(HullShader && DomainShader)
	{
		HullShader->SetMesh(RHICmdList, VertexFactory,View,PrimitiveSceneProxy,BatchElement,DrawRenderState);
		DomainShader->SetMesh(RHICmdList, VertexFactory,View,PrimitiveSceneProxy,BatchElement,DrawRenderState);
	}

	if (bNeedsPixelShader)
	{
		PixelShader->SetMesh(RHICmdList, VertexFactory,View,PrimitiveSceneProxy,BatchElement,DrawRenderState);
	}
	FMeshDrawingPolicy::SetMeshRenderState(RHICmdList, View,PrimitiveSceneProxy,Mesh,BatchElementIndex,bBackFace,DrawRenderState,ElementData,PolicyContext);
	
	SetDitheredLODDepthStencilState(RHICmdList, DrawRenderState);
}

int32 CompareDrawingPolicy(const FDepthDrawingPolicy& A,const FDepthDrawingPolicy& B)
{
	COMPAREDRAWINGPOLICYMEMBERS(VertexShader);
	COMPAREDRAWINGPOLICYMEMBERS(HullShader);
	COMPAREDRAWINGPOLICYMEMBERS(DomainShader);
	COMPAREDRAWINGPOLICYMEMBERS(bNeedsPixelShader);
	COMPAREDRAWINGPOLICYMEMBERS(PixelShader);
	COMPAREDRAWINGPOLICYMEMBERS(VertexFactory);
	COMPAREDRAWINGPOLICYMEMBERS(MaterialRenderProxy);
	COMPAREDRAWINGPOLICYMEMBERS(bIsTwoSidedMaterial);
	return 0;
}

FPositionOnlyDepthDrawingPolicy::FPositionOnlyDepthDrawingPolicy(
	const FVertexFactory* InVertexFactory,
	const FMaterialRenderProxy* InMaterialRenderProxy,
	const FMaterial& InMaterialResource,
	bool bIsTwoSided,
	bool bIsWireframe
	):
	FMeshDrawingPolicy(InVertexFactory,InMaterialRenderProxy,InMaterialResource,DVSM_None,bIsTwoSided,bIsWireframe)
{
	ShaderPipeline = UseShaderPipelines() ? InMaterialResource.GetShaderPipeline(&DepthPosOnlyNoPixelPipeline, VertexFactory->GetType()) : nullptr;
	VertexShader = ShaderPipeline
		? ShaderPipeline->GetShader<TDepthOnlyVS<true> >()
		: InMaterialResource.GetShader<TDepthOnlyVS<true> >(InVertexFactory->GetType());
	bUsePositionOnlyVS = true;
}

void FPositionOnlyDepthDrawingPolicy::SetSharedState(FRHICommandList& RHICmdList, const FSceneView* View, const ContextDataType PolicyContext) const
{
	// Set the depth-only shader parameters for the material.
	VertexShader->SetParameters(RHICmdList, MaterialRenderProxy, *MaterialResource, *View, PolicyContext.bIsInstancedStereo, PolicyContext.bNeedsInstancedStereoBias);

	// Set the shared mesh resources.
	VertexFactory->SetPositionStream(RHICmdList);
}

/** 
* Create bound shader state using the vertex decl from the mesh draw policy
* as well as the shaders needed to draw the mesh
* @return new bound shader state object
*/
FBoundShaderStateInput FPositionOnlyDepthDrawingPolicy::GetBoundShaderStateInput(ERHIFeatureLevel::Type InFeatureLevel)
{
	FVertexDeclarationRHIParamRef VertexDeclaration;
	VertexDeclaration = VertexFactory->GetPositionDeclaration();

	checkSlow(MaterialRenderProxy->GetMaterial(InFeatureLevel)->GetBlendMode() == BLEND_Opaque);
	return FBoundShaderStateInput(VertexDeclaration, VertexShader->GetVertexShader(), FHullShaderRHIRef(), FDomainShaderRHIRef(), FPixelShaderRHIRef(), FGeometryShaderRHIRef());
}

void FPositionOnlyDepthDrawingPolicy::SetMeshRenderState(
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
	VertexShader->SetMesh(RHICmdList, VertexFactory,View,PrimitiveSceneProxy,Mesh.Elements[BatchElementIndex],DrawRenderState);
	FMeshDrawingPolicy::SetMeshRenderState(RHICmdList, View,PrimitiveSceneProxy,Mesh,BatchElementIndex,bBackFace,DrawRenderState,ElementData,PolicyContext);
	
	SetDitheredLODDepthStencilState(RHICmdList, DrawRenderState);
}

void FPositionOnlyDepthDrawingPolicy::SetInstancedEyeIndex(FRHICommandList& RHICmdList, const uint32 EyeIndex) const {
	VertexShader->SetInstancedEyeIndex(RHICmdList, EyeIndex);
}

int32 CompareDrawingPolicy(const FPositionOnlyDepthDrawingPolicy& A,const FPositionOnlyDepthDrawingPolicy& B)
{
	COMPAREDRAWINGPOLICYMEMBERS(VertexShader);
	COMPAREDRAWINGPOLICYMEMBERS(VertexFactory);
	COMPAREDRAWINGPOLICYMEMBERS(MaterialRenderProxy);
	COMPAREDRAWINGPOLICYMEMBERS(bIsTwoSidedMaterial);
	return 0;
}

void FDepthDrawingPolicyFactory::AddStaticMesh(FScene* Scene,FStaticMesh* StaticMesh)
{
	const FMaterialRenderProxy* MaterialRenderProxy = StaticMesh->MaterialRenderProxy;
	const FMaterial* Material = MaterialRenderProxy->GetMaterial(Scene->GetFeatureLevel());
	const EBlendMode BlendMode = Material->GetBlendMode();
	const auto FeatureLevel = Scene->GetFeatureLevel();

	if (!Material->WritesEveryPixel() || Material->HasPixelDepthOffsetConnected())
	{
		// only draw if required
		Scene->MaskedDepthDrawList.AddMesh(
			StaticMesh,
			FDepthDrawingPolicy::ElementDataType(),
			FDepthDrawingPolicy(
				StaticMesh->VertexFactory,
				MaterialRenderProxy,
				*Material,
				Material->IsTwoSided(),
				FeatureLevel
				),
			FeatureLevel
			);
	}
	else
	{
		if (StaticMesh->VertexFactory->SupportsPositionOnlyStream() 
			&& !Material->MaterialModifiesMeshPosition_RenderThread())
		{
			const FMaterialRenderProxy* DefaultProxy = UMaterial::GetDefaultMaterial(MD_Surface)->GetRenderProxy(false);
			// Add the static mesh to the position-only depth draw list.
			Scene->PositionOnlyDepthDrawList.AddMesh(
				StaticMesh,
				FPositionOnlyDepthDrawingPolicy::ElementDataType(),
				FPositionOnlyDepthDrawingPolicy(
					StaticMesh->VertexFactory,
					DefaultProxy,
					*DefaultProxy->GetMaterial(Scene->GetFeatureLevel()),
					Material->IsTwoSided(),
					Material->IsWireframe()
					),
				FeatureLevel
				);
		}
		else
		{
			if (!Material->MaterialModifiesMeshPosition_RenderThread())
			{
				// Override with the default material for everything but opaque two sided materials
				MaterialRenderProxy = UMaterial::GetDefaultMaterial(MD_Surface)->GetRenderProxy(false);
			}

			// Add the static mesh to the opaque depth-only draw list.
			Scene->DepthDrawList.AddMesh(
				StaticMesh,
				FDepthDrawingPolicy::ElementDataType(),
				FDepthDrawingPolicy(
					StaticMesh->VertexFactory,
					MaterialRenderProxy,
					*MaterialRenderProxy->GetMaterial(Scene->GetFeatureLevel()),
					Material->IsTwoSided(),
					FeatureLevel
					),
				FeatureLevel
				);
		}
	}
}

bool FDepthDrawingPolicyFactory::DrawMesh(
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
	const bool bIsInstancedStereo, 
	const bool bNeedsInstancedStereoBias
	)
{
	bool bDirty = false;

	//Do a per-FMeshBatch check on top of the proxy check in RenderPrePass to handle the case where a proxy that is relevant 
	//to the depth only pass has to submit multiple FMeshElements but only some of them should be used as occluders.
	if (Mesh.bUseAsOccluder || !DrawingContext.bRespectUseAsOccluderFlag || DrawingContext.DepthDrawingMode == DDM_AllOpaque)
	{
		const FMaterialRenderProxy* MaterialRenderProxy = Mesh.MaterialRenderProxy;
		const FMaterial* Material = MaterialRenderProxy->GetMaterial(View.GetFeatureLevel());
		const EBlendMode BlendMode = Material->GetBlendMode();

		// Check to see if the primitive is currently fading in or out using the screen door effect.  If it is,
		// then we can't assume the object is opaque as it may be forcibly masked.
		const FSceneViewState* SceneViewState = static_cast<const FSceneViewState*>( View.State );

		if ( BlendMode == BLEND_Opaque 
			&& Mesh.VertexFactory->SupportsPositionOnlyStream() 
			&& !Material->MaterialModifiesMeshPosition_RenderThread()
			&& Material->WritesEveryPixel()
			)
		{
			//render opaque primitives that support a separate position-only vertex buffer
			const FMaterialRenderProxy* DefaultProxy = UMaterial::GetDefaultMaterial(MD_Surface)->GetRenderProxy(false);
			FPositionOnlyDepthDrawingPolicy DrawingPolicy(Mesh.VertexFactory, DefaultProxy, *DefaultProxy->GetMaterial(View.GetFeatureLevel()), Material->IsTwoSided(), Material->IsWireframe());
			RHICmdList.BuildAndSetLocalBoundShaderState(DrawingPolicy.GetBoundShaderStateInput(View.GetFeatureLevel()));
			DrawingPolicy.SetSharedState(RHICmdList, &View, FDepthDrawingPolicy::ContextDataType(bIsInstancedStereo, bNeedsInstancedStereoBias));

			int32 BatchElementIndex = 0;
			uint64 Mask = BatchElementMask;
			do
			{
				if(Mask & 1)
				{
					TDrawEvent<FRHICommandList> MeshEvent;
					BeginMeshDrawEvent(RHICmdList, PrimitiveSceneProxy, Mesh, MeshEvent);

					DrawingPolicy.SetMeshRenderState(RHICmdList, View,PrimitiveSceneProxy,Mesh,BatchElementIndex,bBackFace,DrawRenderState,FPositionOnlyDepthDrawingPolicy::ElementDataType(),FDepthDrawingPolicy::ContextDataType());
					DrawingPolicy.DrawMesh(RHICmdList, Mesh,BatchElementIndex, bIsInstancedStereo);
				}
				Mask >>= 1;
				BatchElementIndex++;
			} while(Mask);

			bDirty = true;
		}
		else if (!IsTranslucentBlendMode(BlendMode))
		{
			const bool bMaterialMasked = !Material->WritesEveryPixel();

			bool bDraw = true;

			switch(DrawingContext.DepthDrawingMode)
			{
			case DDM_AllOpaque:
				break;
			case DDM_AllOccluders: 
				break;
			case DDM_NonMaskedOnly: 
				bDraw = !bMaterialMasked;
				break;
			default:
				check(!"Unrecognized DepthDrawingMode");
			}

			if(bDraw)
			{
				if (!bMaterialMasked && !Material->MaterialModifiesMeshPosition_RenderThread())
				{
					// Override with the default material for opaque materials that are not two sided
					MaterialRenderProxy = UMaterial::GetDefaultMaterial(MD_Surface)->GetRenderProxy(false);
				}

				FDepthDrawingPolicy DrawingPolicy(Mesh.VertexFactory, MaterialRenderProxy, *MaterialRenderProxy->GetMaterial(View.GetFeatureLevel()), Material->IsTwoSided(), View.GetFeatureLevel());
				RHICmdList.BuildAndSetLocalBoundShaderState(DrawingPolicy.GetBoundShaderStateInput(View.GetFeatureLevel()));
				DrawingPolicy.SetSharedState(RHICmdList, &View, FDepthDrawingPolicy::ContextDataType(bIsInstancedStereo, bNeedsInstancedStereoBias));

				int32 BatchElementIndex = 0;
				uint64 Mask = BatchElementMask;
				do
				{
					if(Mask & 1)
					{
						TDrawEvent<FRHICommandList> MeshEvent;
						BeginMeshDrawEvent(RHICmdList, PrimitiveSceneProxy, Mesh, MeshEvent);

						DrawingPolicy.SetMeshRenderState(RHICmdList, View,PrimitiveSceneProxy,Mesh,BatchElementIndex,bBackFace,DrawRenderState,FMeshDrawingPolicy::ElementDataType(),FDepthDrawingPolicy::ContextDataType());
						DrawingPolicy.DrawMesh(RHICmdList, Mesh, BatchElementIndex, bIsInstancedStereo);
					}
					Mask >>= 1;
					BatchElementIndex++;
				} while(Mask);

				bDirty = true;
			}
		}
	}

	return bDirty;
}

bool FDepthDrawingPolicyFactory::DrawDynamicMesh(
	FRHICommandList& RHICmdList, 
	const FViewInfo& View,
	ContextType DrawingContext,
	const FMeshBatch& Mesh,
	bool bBackFace,
	bool bPreFog,
	const FPrimitiveSceneProxy* PrimitiveSceneProxy,
	FHitProxyId HitProxyId, 
	const bool bIsInstancedStereo, 
	const bool bNeedsInstancedStereoBias
	)
{
	return DrawMesh(
		RHICmdList, 
		View,
		DrawingContext,
		Mesh,
		Mesh.Elements.Num()==1 ? 1 : (1<<Mesh.Elements.Num())-1,	// 1 bit set for each mesh element
		bBackFace,
		FMeshDrawingRenderState(),
		bPreFog,
		PrimitiveSceneProxy,
		HitProxyId, 
		bIsInstancedStereo, 
		bNeedsInstancedStereoBias);
}


bool FDepthDrawingPolicyFactory::DrawStaticMesh(
	FRHICommandList& RHICmdList, 
	const FViewInfo& View,
	ContextType DrawingContext,
	const FStaticMesh& StaticMesh,
	const uint64& BatchElementMask,
	bool bPreFog,
	const FMeshDrawingRenderState& DrawRenderState,
	const FPrimitiveSceneProxy* PrimitiveSceneProxy,
	FHitProxyId HitProxyId, 
	const bool bNeedsInstancedStereoBias
	)
{
	bool bDirty = false;

	const FMaterial* Material = StaticMesh.MaterialRenderProxy->GetMaterial(View.GetFeatureLevel());
	const EMaterialShadingModel ShadingModel = Material->GetShadingModel();
	bDirty |= DrawMesh(
		RHICmdList, 
		View,
		DrawingContext,
		StaticMesh,
		BatchElementMask,
		false,
		DrawRenderState,
		bPreFog,
		PrimitiveSceneProxy,
		HitProxyId, 
		false, 
		bNeedsInstancedStereoBias
		);

	return bDirty;
}
