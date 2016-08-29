// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	DepthRendering.cpp: Depth rendering implementation.
=============================================================================*/

#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "OneColorShader.h"
#include "IHeadMountedDisplay.h"
#include "ScreenRendering.h"
#include "SceneFilterRendering.h"

static TAutoConsoleVariable<int32> CVarRHICmdPrePassDeferredContexts(
	TEXT("r.RHICmdPrePassDeferredContexts"),
	1,
	TEXT("True to use deferred contexts to parallelize prepass command list execution."));
static TAutoConsoleVariable<int32> CVarParallelPrePass(
	TEXT("r.ParallelPrePass"),
	1,
	TEXT("Toggles parallel zprepass rendering. Parallel rendering must be enabled for this to have an effect."),
	ECVF_RenderThreadSafe
	);
static TAutoConsoleVariable<int32> CVarRHICmdFlushRenderThreadTasksPrePass(
	TEXT("r.RHICmdFlushRenderThreadTasksPrePass"),
	0,
	TEXT("Wait for completion of parallel render thread tasks at the end of the pre pass.  A more granular version of r.RHICmdFlushRenderThreadTasks. If either r.RHICmdFlushRenderThreadTasks or r.RHICmdFlushRenderThreadTasksPrePass is > 0 we will flush."));

const TCHAR* GetDepthDrawingModeString(EDepthDrawingMode Mode)
{
	switch (Mode)
	{
	case DDM_None:
		return TEXT("DDM_None");
	case DDM_NonMaskedOnly:
		return TEXT("DDM_NonMaskedOnly");
	case DDM_AllOccluders:
		return TEXT("DDM_AllOccluders");
	case DDM_AllOpaque:
		return TEXT("DDM_AllOpaque");
	default:
		check(0);
	}

	return TEXT("");
}

DECLARE_FLOAT_COUNTER_STAT(TEXT("Prepass"), Stat_GPU_Prepass, STATGROUP_GPU);

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
		IsInstancedStereoEmulatedParameter.Bind(Initializer.ParameterMap, TEXT("bIsInstancedStereoEmulated"));
	}

private:

	FShaderParameter InstancedEyeIndexParameter;
	FShaderParameter IsInstancedStereoParameter;
	FShaderParameter IsInstancedStereoEmulatedParameter;

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
		Ar << IsInstancedStereoEmulatedParameter;
		return result;
	}

	void SetParameters(
		FRHICommandList& RHICmdList, 
		const FMaterialRenderProxy* MaterialRenderProxy, 
		const FMaterial& MaterialResource, 
		const FSceneView& View, 
		const bool bIsInstancedStereo, 
		const bool bIsInstancedStereoEmulated
		)
	{
		FMeshMaterialShader::SetParameters(RHICmdList, GetVertexShader(),MaterialRenderProxy,MaterialResource,View,View.ViewUniformBuffer,ESceneRenderTargetsMode::DontSet);
		
		if (IsInstancedStereoParameter.IsBound())
		{
			SetShaderValue(RHICmdList, GetVertexShader(), IsInstancedStereoParameter, bIsInstancedStereo);
		}

		if (IsInstancedStereoEmulatedParameter.IsBound())
		{
			SetShaderValue(RHICmdList, GetVertexShader(), IsInstancedStereoEmulatedParameter, bIsInstancedStereoEmulated);
		}

		if (InstancedEyeIndexParameter.IsBound())
		{
			SetShaderValue(RHICmdList, GetVertexShader(), InstancedEyeIndexParameter, 0);
		}
	}

	void SetMesh(FRHICommandList& RHICmdList, const FVertexFactory* VertexFactory,const FSceneView& View,const FPrimitiveSceneProxy* Proxy,const FMeshBatchElement& BatchElement,const FMeshDrawingRenderState& DrawRenderState)
	{
		FMeshMaterialShader::SetMesh(RHICmdList, GetVertexShader(),VertexFactory,View,Proxy,BatchElement,DrawRenderState);
	}

	void SetInstancedEyeIndex(FRHICommandList& RHICmdList, const uint32 EyeIndex)
	{
		if (EyeIndex > 0 && InstancedEyeIndexParameter.IsBound())
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
	{
		ApplyDepthOffsetParameter.Bind(Initializer.ParameterMap, TEXT("bApplyDepthOffset"));
	}

	FDepthOnlyPS() {}

	void SetParameters(FRHICommandList& RHICmdList, const FMaterialRenderProxy* MaterialRenderProxy,const FMaterial& MaterialResource,const FSceneView* View)
	{
		FMeshMaterialShader::SetParameters(RHICmdList, GetPixelShader(),MaterialRenderProxy,MaterialResource,*View,View->ViewUniformBuffer,ESceneRenderTargetsMode::DontSet);

		// For debug view shaders, don't apply the depth offset as their base pass PS are using global shaders with depth equal.
		SetShaderValue(RHICmdList, GetPixelShader(), ApplyDepthOffsetParameter, !View || !View->Family->UseDebugViewPS());
	}

	void SetMesh(FRHICommandList& RHICmdList, const FVertexFactory* VertexFactory,const FSceneView& View,const FPrimitiveSceneProxy* Proxy,const FMeshBatchElement& BatchElement,const FMeshDrawingRenderState& DrawRenderState)
	{
		FMeshMaterialShader::SetMesh(RHICmdList, GetPixelShader(),VertexFactory,View,Proxy,BatchElement,DrawRenderState);
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FMeshMaterialShader::Serialize(Ar);
		Ar << ApplyDepthOffsetParameter;
		return bShaderHasOutdatedParameters;
	}

	FShaderParameter ApplyDepthOffsetParameter;
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
	bNeedsPixelShader = (!InMaterialResource.WritesEveryPixel() || InMaterialResource.MaterialUsesPixelDepthOffset());
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

void FDepthDrawingPolicy::SetInstancedEyeIndex(FRHICommandList& RHICmdList, const uint32 EyeIndex) const
{
	VertexShader->SetInstancedEyeIndex(RHICmdList, EyeIndex);
}

void FDepthDrawingPolicy::SetSharedState(FRHICommandList& RHICmdList, const FSceneView* View, const FDepthDrawingPolicy::ContextDataType PolicyContext) const
{
	// Set the depth-only shader parameters for the material.
	VertexShader->SetParameters(RHICmdList, MaterialRenderProxy, *MaterialResource, *View, PolicyContext.bIsInstancedStereo, PolicyContext.bIsInstancedStereoEmulated);
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

void FPositionOnlyDepthDrawingPolicy::SetSharedState(FRHICommandList& RHICmdList, const FSceneView* View, const FPositionOnlyDepthDrawingPolicy::ContextDataType PolicyContext) const
{
	// Set the depth-only shader parameters for the material.
	VertexShader->SetParameters(RHICmdList, MaterialRenderProxy, *MaterialResource, *View, PolicyContext.bIsInstancedStereo, PolicyContext.bIsInstancedStereoEmulated);

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

void FPositionOnlyDepthDrawingPolicy::SetInstancedEyeIndex(FRHICommandList& RHICmdList, const uint32 EyeIndex) const
{
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

	if (!Material->WritesEveryPixel() || Material->MaterialUsesPixelDepthOffset())
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
	const bool bIsInstancedStereoEmulated
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
			DrawingPolicy.SetSharedState(RHICmdList, &View, FPositionOnlyDepthDrawingPolicy::ContextDataType(bIsInstancedStereo, bIsInstancedStereoEmulated));

			int32 BatchElementIndex = 0;
			uint64 Mask = BatchElementMask;
			do
			{
				if(Mask & 1)
				{
					// We draw instanced static meshes twice when rendering with instanced stereo. Once for each eye.
					const bool bIsInstancedMesh = Mesh.Elements[BatchElementIndex].bIsInstancedMesh;
					const uint32 InstancedStereoDrawCount = (bIsInstancedStereo && bIsInstancedMesh) ? 2 : 1;
					for (uint32 DrawCountIter = 0; DrawCountIter < InstancedStereoDrawCount; ++DrawCountIter)
					{
						DrawingPolicy.SetInstancedEyeIndex(RHICmdList, DrawCountIter);

						TDrawEvent<FRHICommandList> MeshEvent;
						BeginMeshDrawEvent(RHICmdList, PrimitiveSceneProxy, Mesh, MeshEvent);

						DrawingPolicy.SetMeshRenderState(RHICmdList, View, PrimitiveSceneProxy, Mesh, BatchElementIndex, bBackFace, DrawRenderState, FPositionOnlyDepthDrawingPolicy::ElementDataType(), FPositionOnlyDepthDrawingPolicy::ContextDataType());
						DrawingPolicy.DrawMesh(RHICmdList, Mesh, BatchElementIndex, bIsInstancedStereo);
					}
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
				DrawingPolicy.SetSharedState(RHICmdList, &View, FDepthDrawingPolicy::ContextDataType(bIsInstancedStereo, bIsInstancedStereoEmulated));

				int32 BatchElementIndex = 0;
				uint64 Mask = BatchElementMask;
				do
				{
					if(Mask & 1)
					{
						// We draw instanced static meshes twice when rendering with instanced stereo. Once for each eye.
						const bool bIsInstancedMesh = Mesh.Elements[BatchElementIndex].bIsInstancedMesh;
						const uint32 InstancedStereoDrawCount = (bIsInstancedStereo && bIsInstancedMesh) ? 2 : 1;
						for (uint32 DrawCountIter = 0; DrawCountIter < InstancedStereoDrawCount; ++DrawCountIter)
						{
							DrawingPolicy.SetInstancedEyeIndex(RHICmdList, DrawCountIter);

							TDrawEvent<FRHICommandList> MeshEvent;
							BeginMeshDrawEvent(RHICmdList, PrimitiveSceneProxy, Mesh, MeshEvent);

							DrawingPolicy.SetMeshRenderState(RHICmdList, View, PrimitiveSceneProxy, Mesh, BatchElementIndex, bBackFace, DrawRenderState, FMeshDrawingPolicy::ElementDataType(), FDepthDrawingPolicy::ContextDataType());
							DrawingPolicy.DrawMesh(RHICmdList, Mesh, BatchElementIndex, bIsInstancedStereo);
						}
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
	const bool bIsInstancedStereoEmulated
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
		bIsInstancedStereoEmulated
		);
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
	const bool bIsInstancedStereo,
	const bool bIsInstancedStereoEmulated
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
		bIsInstancedStereo, 
		bIsInstancedStereoEmulated
		);

	return bDirty;
}

bool FDeferredShadingSceneRenderer::RenderPrePassViewDynamic(FRHICommandList& RHICmdList, const FViewInfo& View)
{
	FDepthDrawingPolicyFactory::ContextType Context(EarlyZPassMode, true);

	for (int32 MeshBatchIndex = 0; MeshBatchIndex < View.DynamicMeshElements.Num(); MeshBatchIndex++)
	{
		const FMeshBatchAndRelevance& MeshBatchAndRelevance = View.DynamicMeshElements[MeshBatchIndex];

		if (MeshBatchAndRelevance.GetHasOpaqueOrMaskedMaterial() && MeshBatchAndRelevance.GetRenderInMainPass())
		{
			const FMeshBatch& MeshBatch = *MeshBatchAndRelevance.Mesh;
			const FPrimitiveSceneProxy* PrimitiveSceneProxy = MeshBatchAndRelevance.PrimitiveSceneProxy;
			bool bShouldUseAsOccluder = true;

			if (EarlyZPassMode < DDM_AllOccluders)
			{
				extern float GMinScreenRadiusForDepthPrepass;
				//@todo - move these proxy properties into FMeshBatchAndRelevance so we don't have to dereference the proxy in order to reject a mesh
				const float LODFactorDistanceSquared = (PrimitiveSceneProxy->GetBounds().Origin - View.ViewMatrices.ViewOrigin).SizeSquared() * FMath::Square(View.LODDistanceFactor);

				// Only render primitives marked as occluders
				bShouldUseAsOccluder = PrimitiveSceneProxy->ShouldUseAsOccluder()
					// Only render static objects unless movable are requested
					&& (!PrimitiveSceneProxy->IsMovable() || bEarlyZPassMovable)
					&& (FMath::Square(PrimitiveSceneProxy->GetBounds().SphereRadius) > GMinScreenRadiusForDepthPrepass * GMinScreenRadiusForDepthPrepass * LODFactorDistanceSquared);
			}

			if (bShouldUseAsOccluder)
			{
				FDepthDrawingPolicyFactory::DrawDynamicMesh(RHICmdList, View, Context, MeshBatch, false, true, PrimitiveSceneProxy, MeshBatch.BatchHitProxyId, View.IsInstancedStereoPass());
			}
		}
	}

	return true;
}

static void SetupPrePassView(FRHICommandList& RHICmdList, const FViewInfo& View)
{
	// Disable color writes, enable depth tests and writes.
	RHICmdList.SetBlendState(TStaticBlendState<CW_NONE>::GetRHI());
	RHICmdList.SetDepthStencilState(TStaticDepthStencilState<true, CF_DepthNearOrEqual>::GetRHI());
	
	RHICmdList.SetRasterizerState(TStaticRasterizerState<FM_Solid, CM_None>::GetRHI());
	RHICmdList.SetScissorRect(false, 0, 0, 0, 0);

	if (!View.IsInstancedStereoPass())
	{
		RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0.0f, View.ViewRect.Max.X, View.ViewRect.Max.Y, 1.0f);
	}
	else
	{
		if (View.bIsMultiViewEnabled)
		{
			const uint32 LeftMinX = View.Family->Views[0]->ViewRect.Min.X;
			const uint32 LeftMaxX = View.Family->Views[0]->ViewRect.Max.X;
			const uint32 RightMinX = View.Family->Views[1]->ViewRect.Min.X;
			const uint32 RightMaxX = View.Family->Views[1]->ViewRect.Max.X;
			RHICmdList.SetStereoViewport(LeftMinX, RightMinX, 0, 0.0f, LeftMaxX, RightMaxX, View.ViewRect.Max.Y, 1.0f);
		}
		else
		{
			RHICmdList.SetViewport(0, 0, 0, View.Family->InstancedStereoWidth, View.ViewRect.Max.Y, 1);
		}
	}
}

static void RenderHiddenAreaMaskView(FRHICommandList& RHICmdList, const FViewInfo& View)
{
	const auto FeatureLevel = GMaxRHIFeatureLevel;
	const auto ShaderMap = GetGlobalShaderMap(FeatureLevel);
	TShaderMapRef<TOneColorVS<true> > VertexShader(ShaderMap);
	static FGlobalBoundShaderState BoundShaderState;
	SetGlobalBoundShaderState(RHICmdList, FeatureLevel, BoundShaderState, GetVertexDeclarationFVector4(), *VertexShader, nullptr);
	GEngine->HMDDevice->DrawHiddenAreaMesh_RenderThread(RHICmdList, View.StereoPass);
}

bool FDeferredShadingSceneRenderer::RenderPrePassView(FRHICommandList& RHICmdList, const FViewInfo& View)
{
	bool bDirty = false;

	SetupPrePassView(RHICmdList, View);

	// Draw the static occluder primitives using a depth drawing policy.

	if (!View.IsInstancedStereoPass())
	{
		{
			// Draw opaque occluders which support a separate position-only
			// vertex buffer to minimize vertex fetch bandwidth, which is
			// often the bottleneck during the depth only pass.
			SCOPED_DRAW_EVENT(RHICmdList, PosOnlyOpaque);
			bDirty |= Scene->PositionOnlyDepthDrawList.DrawVisible(RHICmdList, View, View.StaticMeshOccluderMap, View.StaticMeshBatchVisibility);
		}
		{
			// Draw opaque occluders, using double speed z where supported.
			SCOPED_DRAW_EVENT(RHICmdList, Opaque);
			bDirty |= Scene->DepthDrawList.DrawVisible(RHICmdList, View, View.StaticMeshOccluderMap, View.StaticMeshBatchVisibility);
		}

		if (EarlyZPassMode >= DDM_AllOccluders)
		{
			// Draw opaque occluders with masked materials
			SCOPED_DRAW_EVENT(RHICmdList, Masked);
			bDirty |= Scene->MaskedDepthDrawList.DrawVisible(RHICmdList, View, View.StaticMeshOccluderMap, View.StaticMeshBatchVisibility);
		}
	}
	else
	{
		const StereoPair StereoView(Views[0], Views[1], Views[0].StaticMeshOccluderMap, Views[1].StaticMeshOccluderMap, Views[0].StaticMeshBatchVisibility, Views[1].StaticMeshBatchVisibility);
		{
			SCOPED_DRAW_EVENT(RHICmdList, PosOnlyOpaque);
			bDirty |= Scene->PositionOnlyDepthDrawList.DrawVisibleInstancedStereo(RHICmdList, StereoView);
		}
		{
			SCOPED_DRAW_EVENT(RHICmdList, Opaque);
			bDirty |= Scene->DepthDrawList.DrawVisibleInstancedStereo(RHICmdList, StereoView);
		}

		if (EarlyZPassMode >= DDM_AllOccluders)
		{
			SCOPED_DRAW_EVENT(RHICmdList, Masked);
			bDirty |= Scene->MaskedDepthDrawList.DrawVisibleInstancedStereo(RHICmdList, StereoView);
		}
	}
	
	{
		SCOPED_DRAW_EVENT(RHICmdList, Dynamic);
		bDirty |= RenderPrePassViewDynamic(RHICmdList, View);
	}

	return bDirty;
}

class FRenderPrepassDynamicDataThreadTask : public FRenderTask
{
	FDeferredShadingSceneRenderer& ThisRenderer;
	FRHICommandList& RHICmdList;
	const FViewInfo& View;

public:

	FRenderPrepassDynamicDataThreadTask(
		FDeferredShadingSceneRenderer& InThisRenderer,
		FRHICommandList& InRHICmdList,
		const FViewInfo& InView
		)
		: ThisRenderer(InThisRenderer)
		, RHICmdList(InRHICmdList)
		, View(InView)
	{
	}

	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FRenderPrepassDynamicDataThreadTask, STATGROUP_TaskGraphTasks);
	}

	static ESubsequentsMode::Type GetSubsequentsMode() { return ESubsequentsMode::TrackSubsequents; }

	void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
	{
		ThisRenderer.RenderPrePassViewDynamic(RHICmdList, View);
		RHICmdList.HandleRTThreadTaskCompletion(MyCompletionGraphEvent);
	}
};

DECLARE_CYCLE_STAT(TEXT("Prepass"), STAT_CLP_Prepass, STATGROUP_ParallelCommandListMarkers);

class FPrePassParallelCommandListSet : public FParallelCommandListSet
{
public:
	FPrePassParallelCommandListSet(const FViewInfo& InView, FRHICommandListImmediate& InParentCmdList, bool bInParallelExecute, bool bInCreateSceneContext)
		: FParallelCommandListSet(GET_STATID(STAT_CLP_Prepass), InView, InParentCmdList, bInParallelExecute, bInCreateSceneContext)
	{
		// Do not copy-paste. this is a very unusual FParallelCommandListSet because it is a prepass and we want to do some work after starting some tasks
	}

	virtual ~FPrePassParallelCommandListSet()
	{
		// Do not copy-paste. this is a very unusual FParallelCommandListSet because it is a prepass and we want to do some work after starting some tasks
		SetStateOnCommandList(ParentCmdList);
		Dispatch(true);
	}

	virtual void SetStateOnCommandList(FRHICommandList& CmdList) override
	{
		FSceneRenderTargets::Get(CmdList).BeginRenderingPrePass(CmdList, false);
		SetupPrePassView(CmdList, View);
	}
};

bool FDeferredShadingSceneRenderer::RenderPrePassViewParallel(const FViewInfo& View, FRHICommandListImmediate& ParentCmdList, TFunctionRef<void()> AfterTasksAreStarted, bool bDoPrePre)
{
	bool bDepthWasCleared = false;
	FPrePassParallelCommandListSet ParallelCommandListSet(View, ParentCmdList,
		CVarRHICmdPrePassDeferredContexts.GetValueOnRenderThread() > 0, 
		CVarRHICmdFlushRenderThreadTasksPrePass.GetValueOnRenderThread() == 0  && CVarRHICmdFlushRenderThreadTasks.GetValueOnRenderThread() == 0);

	if (!View.IsInstancedStereoPass())
	{
		// Draw the static occluder primitives using a depth drawing policy.
		// Draw opaque occluders which support a separate position-only
		// vertex buffer to minimize vertex fetch bandwidth, which is
		// often the bottleneck during the depth only pass.
		Scene->PositionOnlyDepthDrawList.DrawVisibleParallel(View.StaticMeshOccluderMap, View.StaticMeshBatchVisibility, ParallelCommandListSet);

		// Draw opaque occluders, using double speed z where supported.
		Scene->DepthDrawList.DrawVisibleParallel(View.StaticMeshOccluderMap, View.StaticMeshBatchVisibility, ParallelCommandListSet);

		// Draw opaque occluders with masked materials
		if (EarlyZPassMode >= DDM_AllOccluders)
		{			
			Scene->MaskedDepthDrawList.DrawVisibleParallel(View.StaticMeshOccluderMap, View.StaticMeshBatchVisibility, ParallelCommandListSet);
		}
	}
	else
	{
		const StereoPair StereoView(Views[0], Views[1], Views[0].StaticMeshOccluderMap, Views[1].StaticMeshOccluderMap, Views[0].StaticMeshBatchVisibility, Views[1].StaticMeshBatchVisibility);

		Scene->PositionOnlyDepthDrawList.DrawVisibleParallelInstancedStereo(StereoView, ParallelCommandListSet);
		Scene->DepthDrawList.DrawVisibleParallelInstancedStereo(StereoView, ParallelCommandListSet);

		if (EarlyZPassMode >= DDM_AllOccluders)
		{
			Scene->MaskedDepthDrawList.DrawVisibleParallelInstancedStereo(StereoView, ParallelCommandListSet);
		}
	}

	// we do this step here (awkwardly) so that the above tasks can be in flight while we get the particles (which must be dynamic) setup.
	if (bDoPrePre)
	{
		AfterTasksAreStarted();
		bDepthWasCleared = PreRenderPrePass(ParentCmdList);
	}

	// Dynamic
	FRHICommandList* CmdList = ParallelCommandListSet.NewParallelCommandList();

	FGraphEventRef AnyThreadCompletionEvent = TGraphTask<FRenderPrepassDynamicDataThreadTask>::CreateTask(ParallelCommandListSet.GetPrereqs(), ENamedThreads::RenderThread)
		.ConstructAndDispatchWhenReady(*this, *CmdList, View);

	ParallelCommandListSet.AddParallelCommandList(CmdList, AnyThreadCompletionEvent);

	return bDepthWasCleared;
}

/** A pixel shader used to fill the stencil buffer with the current dithered transition mask. */
class FDitheredTransitionStencilPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FDitheredTransitionStencilPS, Global);
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{ 
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4);
	}

	FDitheredTransitionStencilPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
	: FGlobalShader(Initializer)
	{
		DitheredTransitionFactorParameter.Bind(Initializer.ParameterMap, TEXT("DitheredTransitionFactor"));
	}

	FDitheredTransitionStencilPS()
	{
	}

	void SetParameters(FRHICommandList& RHICmdList, const FSceneView& View)
	{
		FGlobalShader::SetParameters(RHICmdList, GetPixelShader(), View);

		const float DitherFactor = View.GetTemporalLODTransition();
		SetShaderValue(RHICmdList, GetPixelShader(), DitheredTransitionFactorParameter, DitherFactor);
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << DitheredTransitionFactorParameter;
		return bShaderHasOutdatedParameters;
	}

	FShaderParameter DitheredTransitionFactorParameter;
};

IMPLEMENT_SHADER_TYPE(, FDitheredTransitionStencilPS, TEXT("DitheredTransitionStencil"), TEXT("Main"), SF_Pixel);
FGlobalBoundShaderState DitheredTransitionStencilBoundShaderState;

/** Possibly do the FX prerender and setup the prepass*/
bool FDeferredShadingSceneRenderer::PreRenderPrePass(FRHICommandListImmediate& RHICmdList)
{
	RHICmdList.SetCurrentStat(GET_STATID(STAT_CLM_PrePass));
	bool bDepthWasCleared = RenderPrePassHMD(RHICmdList);

	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

	SceneContext.BeginRenderingPrePass(RHICmdList, !bDepthWasCleared);
	bDepthWasCleared = true;

	// Dithered transition stencil mask fill
	if (bDitheredLODTransitionsUseStencil)
	{
		SCOPED_DRAW_EVENT(RHICmdList, DitheredStencilPrePass);
		FIntPoint BufferSizeXY = SceneContext.GetBufferSizeXY();

		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ++ViewIndex)
		{
			SCOPED_CONDITIONAL_DRAW_EVENTF(RHICmdList, EventView, Views.Num() > 1, TEXT("View%d"), ViewIndex);

			FViewInfo& View = Views[ViewIndex];
			RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0.0f, View.ViewRect.Max.X, View.ViewRect.Max.Y, 1.0f);

			// Set shaders, states
			TShaderMapRef<FScreenVS> ScreenVertexShader(View.ShaderMap);
			TShaderMapRef<FDitheredTransitionStencilPS> PixelShader(View.ShaderMap);
			extern TGlobalResource<FFilterVertexDeclaration> GFilterVertexDeclaration;
			SetGlobalBoundShaderState(RHICmdList, FeatureLevel, DitheredTransitionStencilBoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *ScreenVertexShader, *PixelShader);

			PixelShader->SetParameters(RHICmdList, View);

			RHICmdList.SetRasterizerState(TStaticRasterizerState<>::GetRHI());
			RHICmdList.SetBlendState(TStaticBlendState<>::GetRHI());
			RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always,
				true, CF_Always, SO_Keep, SO_Keep, SO_Replace,
				false, CF_Always, SO_Keep, SO_Keep, SO_Keep,
				STENCIL_SANDBOX_MASK, STENCIL_SANDBOX_MASK>::GetRHI(), STENCIL_SANDBOX_MASK);

			DrawRectangle(
				RHICmdList,
				0, 0,
				BufferSizeXY.X, BufferSizeXY.Y,
				View.ViewRect.Min.X, View.ViewRect.Min.Y,
				View.ViewRect.Width(), View.ViewRect.Height(),
				BufferSizeXY,
				BufferSizeXY,
				*ScreenVertexShader,
				EDRF_UseTriangleOptimization);
		}
	}
	return bDepthWasCleared;
}

bool FDeferredShadingSceneRenderer::RenderPrePass(FRHICommandListImmediate& RHICmdList, TFunctionRef<void()> AfterTasksAreStarted)
{
	bool bDepthWasCleared = false;

	extern const TCHAR* GetDepthPassReason(bool bDitheredLODTransitionsUseStencil, ERHIFeatureLevel::Type FeatureLevel);
	SCOPED_DRAW_EVENTF(RHICmdList, PrePass, TEXT("PrePass %s %s"), GetDepthDrawingModeString(EarlyZPassMode), GetDepthPassReason(bDitheredLODTransitionsUseStencil, FeatureLevel));

	SCOPE_CYCLE_COUNTER(STAT_DepthDrawTime);
	SCOPED_GPU_STAT(RHICmdList, Stat_GPU_Prepass);

	bool bDirty = false;
	bool bDidPrePre = false;
	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

	bool bParallel = GRHICommandList.UseParallelAlgorithms() && CVarParallelPrePass.GetValueOnRenderThread();

	if (!bParallel)
	{
		// nothing to be gained by delaying this.
		AfterTasksAreStarted();
		bDepthWasCleared = PreRenderPrePass(RHICmdList);
		bDidPrePre = true;
	}
	else
	{
		SceneContext.GetSceneDepthSurface(); // this probably isn't needed, but if there was some lazy allocation of the depth surface going on, we want it allocated now before we go wide. We may not have called BeginRenderingPrePass yet if bDoFXPrerender is true
	}

	// Draw a depth pass to avoid overdraw in the other passes.
	if(EarlyZPassMode != DDM_None)
	{
		if (bParallel)
		{
			FScopedCommandListWaitForTasks Flusher(CVarRHICmdFlushRenderThreadTasksPrePass.GetValueOnRenderThread() > 0 || CVarRHICmdFlushRenderThreadTasks.GetValueOnRenderThread() > 0, RHICmdList);
			for(int32 ViewIndex = 0;ViewIndex < Views.Num();ViewIndex++)
			{
				SCOPED_CONDITIONAL_DRAW_EVENTF(RHICmdList, EventView, Views.Num() > 1, TEXT("View%d"), ViewIndex);
				const FViewInfo& View = Views[ViewIndex];
				if (View.ShouldRenderView())
				{
					bDepthWasCleared = RenderPrePassViewParallel(View, RHICmdList, AfterTasksAreStarted, !bDidPrePre) || bDepthWasCleared;
					bDirty = true; // assume dirty since we are not going to wait
					bDidPrePre = true;
				}
			}
		}
		else
		{
			for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
			{
				SCOPED_CONDITIONAL_DRAW_EVENTF(RHICmdList, EventView, Views.Num() > 1, TEXT("View%d"), ViewIndex);
				const FViewInfo& View = Views[ViewIndex];
				if (View.ShouldRenderView())
				{
					bDirty |= RenderPrePassView(RHICmdList, View);
				}
			}
		}
	}
	if (!bDidPrePre)
	{
		// For some reason we haven't done this yet. Best do it now for consistency with the old code.
		AfterTasksAreStarted();
		bDepthWasCleared = PreRenderPrePass(RHICmdList);
		bDidPrePre = true;
	}

	// Dithered transition stencil mask clear, accounting for all active viewports
	if (bDitheredLODTransitionsUseStencil)
	{
		if (Views.Num() > 1)
		{
			FIntRect FullViewRect = Views[0].ViewRect;
			for (int32 ViewIndex = 1; ViewIndex < Views.Num(); ++ViewIndex)
			{
				FullViewRect.Union(Views[ViewIndex].ViewRect);
			}
			RHICmdList.SetViewport(FullViewRect.Min.X, FullViewRect.Min.Y, 0, FullViewRect.Max.X, FullViewRect.Max.Y, 1);
		}
		RHICmdList.Clear(false, FLinearColor::Black, false, 0.f, true, 0, FIntRect());
	}

	SceneContext.FinishRenderingPrePass(RHICmdList);

	return bDepthWasCleared;
}

/**
 * Returns true if there's a hidden area mask available
 */
static FORCEINLINE bool HasHiddenAreaMask()
{
	static const auto* const HiddenAreaMaskCVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("vr.HiddenAreaMask"));
	return (HiddenAreaMaskCVar != nullptr &&
		HiddenAreaMaskCVar->GetValueOnRenderThread() == 1 &&
		GEngine &&
		GEngine->HMDDevice.IsValid() &&
		GEngine->HMDDevice->HasHiddenAreaMesh());
}

bool FDeferredShadingSceneRenderer::RenderPrePassHMD(FRHICommandListImmediate& RHICmdList)
{
	// Early out before we change any state if there's not a mask to render
	if (!HasHiddenAreaMask())
	{
		return false;
	}

	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);
	SceneContext.BeginRenderingPrePass(RHICmdList, true);

	RHICmdList.SetBlendState(TStaticBlendState<CW_NONE>::GetRHI());
	RHICmdList.SetDepthStencilState(TStaticDepthStencilState<true, CF_DepthNearOrEqual>::GetRHI());
	RHICmdList.SetRasterizerState(TStaticRasterizerState<FM_Solid, CM_None>::GetRHI());
	RHICmdList.SetScissorRect(false, 0, 0, 0, 0);

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ++ViewIndex)
	{
		const FViewInfo& View = Views[ViewIndex];
		if (View.StereoPass != eSSP_FULL)
		{
			RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0.0f, View.ViewRect.Max.X, View.ViewRect.Max.Y, 1.0f);
			RenderHiddenAreaMaskView(RHICmdList, View);
		}
	}

	SceneContext.FinishRenderingPrePass(RHICmdList);

	return true;
}