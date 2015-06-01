// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VelocityRendering.cpp: Velocity rendering implementation.
=============================================================================*/

#include "RendererPrivate.h"
#include "../../Engine/Private/SkeletalRenderGPUSkin.h"		// GPrevPerBoneMotionBlur
#include "SceneUtils.h"

// Changing this causes a full shader recompile
static TAutoConsoleVariable<int32> CVarBasePassOutputsVelocity(
	TEXT("r.BasePassOutputsVelocity"),
	0,
	TEXT("Enables rendering WPO velocities on the base pass.\n") \
	TEXT(" 0: Renders in a separate pass/rendertarget, all movable static meshes + dynamic.\n") \
	TEXT(" 1: Renders during the regular base pass adding an extra GBuffer, but allowing motion blur on materials with Time-based WPO."),
	ECVF_ReadOnly | ECVF_RenderThreadSafe);

//=============================================================================
/** Encapsulates the Velocity vertex shader. */
class FVelocityVS : public FMeshMaterialShader
{
	DECLARE_SHADER_TYPE(FVelocityVS,MeshMaterial);

public:

	void SetParameters(FRHICommandList& RHICmdList, const FVertexFactory* VertexFactory,const FMaterialRenderProxy* MaterialRenderProxy,const FViewInfo& View)
	{
		FMeshMaterialShader::SetParameters(RHICmdList, GetVertexShader(), MaterialRenderProxy, *MaterialRenderProxy->GetMaterial(View.GetFeatureLevel()), View, ESceneRenderTargetsMode::DontSet);
	}

	void SetMesh(FRHICommandList& RHICmdList, const FVertexFactory* VertexFactory, const FMeshBatch& Mesh, int32 BatchElementIndex, const FViewInfo& View, const FPrimitiveSceneProxy* Proxy, const FMatrix& InPreviousLocalToWorld)
	{
		FMeshMaterialShader::SetMesh(RHICmdList, GetVertexShader(), VertexFactory, View, Proxy, Mesh.Elements[BatchElementIndex]);

		SetShaderValue(RHICmdList, GetVertexShader(), PreviousLocalToWorld, InPreviousLocalToWorld.ConcatTranslation(View.PrevViewMatrices.PreViewTranslation));
	}

	bool SupportsVelocity() const
	{
		return PreviousLocalToWorld.IsBound();
	}

	static bool ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		//Only compile the velocity shaders for the default material or if it's masked,
		return ((Material->IsSpecialEngineMaterial() || Material->IsMasked() 
			//or if the material is opaque and two-sided,
			|| (Material->IsTwoSided() && !IsTranslucentBlendMode(Material->GetBlendMode()))
			// or if the material modifies meshes
			|| Material->MaterialMayModifyMeshPosition()))
			&& IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4) && !FVelocityRendering::OutputsToGBuffer();
	}

protected:
	FVelocityVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FMeshMaterialShader(Initializer)
	{
		PreviousLocalToWorld.Bind(Initializer.ParameterMap,TEXT("PreviousLocalToWorld"));
	}
	FVelocityVS() {}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FMeshMaterialShader::Serialize(Ar);
		Ar << PreviousLocalToWorld;
		return bShaderHasOutdatedParameters;
	}

private:
	FShaderParameter PreviousLocalToWorld;
};


//=============================================================================
/** Encapsulates the Velocity hull shader. */
class FVelocityHS : public FBaseHS
{
	DECLARE_SHADER_TYPE(FVelocityHS,MeshMaterial);

protected:
	FVelocityHS(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FBaseHS(Initializer)
	{}

	FVelocityHS() {}

	static bool ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		return FBaseHS::ShouldCache(Platform, Material, VertexFactoryType) &&
				FVelocityVS::ShouldCache(Platform, Material, VertexFactoryType); // same rules as VS
	}
};

//=============================================================================
/** Encapsulates the Velocity domain shader. */
class FVelocityDS : public FBaseDS
{
	DECLARE_SHADER_TYPE(FVelocityDS,MeshMaterial);

public:

	void SetParameters(FRHICommandList& RHICmdList, const FMaterialRenderProxy* MaterialRenderProxy,const FViewInfo& View)
	{
		FMeshMaterialShader::SetParameters(RHICmdList, (FDomainShaderRHIParamRef)GetDomainShader(), MaterialRenderProxy, *MaterialRenderProxy->GetMaterial(View.GetFeatureLevel()), View, ESceneRenderTargetsMode::DontSet);
	}

protected:
	FVelocityDS(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FBaseDS(Initializer)
	{}

	FVelocityDS() {}

	static bool ShouldCache(EShaderPlatform Platform, const FMaterial* Material, const FVertexFactoryType* VertexFactoryType)
	{
		return FBaseDS::ShouldCache(Platform, Material, VertexFactoryType) &&
				FVelocityVS::ShouldCache(Platform, Material, VertexFactoryType); // same rules as VS
	}
};

IMPLEMENT_MATERIAL_SHADER_TYPE(,FVelocityVS,TEXT("VelocityShader"),TEXT("MainVertexShader"),SF_Vertex); 
IMPLEMENT_MATERIAL_SHADER_TYPE(,FVelocityHS,TEXT("VelocityShader"),TEXT("MainHull"),SF_Hull); 
IMPLEMENT_MATERIAL_SHADER_TYPE(,FVelocityDS,TEXT("VelocityShader"),TEXT("MainDomain"),SF_Domain);

//=============================================================================
/** Encapsulates the Velocity pixel shader. */
class FVelocityPS : public FMeshMaterialShader
{
	DECLARE_SHADER_TYPE(FVelocityPS,MeshMaterial);
public:
	static bool ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		//Only compile the velocity shaders for the default material or if it's masked,
		return ((Material->IsSpecialEngineMaterial() || Material->IsMasked() 
			//or if the material is opaque and two-sided,
			|| (Material->IsTwoSided() && !IsTranslucentBlendMode(Material->GetBlendMode()))
			// or if the material modifies meshes
			|| Material->MaterialMayModifyMeshPosition()))
			&& IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4) && !FVelocityRendering::OutputsToGBuffer();
	}

	static void ModifyCompilationEnvironment( EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment )
	{
		FMaterialShader::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
		OutEnvironment.SetRenderTargetOutputFormat(0, PF_G16R16);
	}

	FVelocityPS() {}

	FVelocityPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FMeshMaterialShader(Initializer)
	{
	}

	void SetParameters(FRHICommandList& RHICmdList, const FVertexFactory* VertexFactory,const FMaterialRenderProxy* MaterialRenderProxy,const FViewInfo& View)
	{
		FMeshMaterialShader::SetParameters(RHICmdList, GetPixelShader(), MaterialRenderProxy, *MaterialRenderProxy->GetMaterial(View.GetFeatureLevel()), View, ESceneRenderTargetsMode::DontSet);
	}

	void SetMesh(FRHICommandList& RHICmdList, const FVertexFactory* VertexFactory, const FMeshBatch& Mesh,int32 BatchElementIndex,const FViewInfo& View, const FPrimitiveSceneProxy* Proxy, bool bBackFace)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();
		
		FMeshMaterialShader::SetMesh(RHICmdList, ShaderRHI, VertexFactory, View, Proxy, Mesh.Elements[BatchElementIndex]);
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FMeshMaterialShader::Serialize(Ar);
		return bShaderHasOutdatedParameters;
	}
};

IMPLEMENT_MATERIAL_SHADER_TYPE(,FVelocityPS,TEXT("VelocityShader"),TEXT("MainPixelShader"),SF_Pixel);


//=============================================================================
/** FVelocityDrawingPolicy - Policy to wrap FMeshDrawingPolicy with new shaders. */

FVelocityDrawingPolicy::FVelocityDrawingPolicy(
	const FVertexFactory* InVertexFactory,
	const FMaterialRenderProxy* InMaterialRenderProxy,
	const FMaterial& InMaterialResource,
	ERHIFeatureLevel::Type InFeatureLevel
	)
	:	FMeshDrawingPolicy(InVertexFactory,InMaterialRenderProxy,InMaterialResource)	
{
	const FMaterialShaderMap* MaterialShaderIndex = InMaterialResource.GetRenderingThreadShaderMap();
	const FMeshMaterialShaderMap* MeshShaderIndex = MaterialShaderIndex->GetMeshShaderMap(InVertexFactory->GetType());

	HullShader = NULL;
	DomainShader = NULL;

	const EMaterialTessellationMode MaterialTessellationMode = InMaterialResource.GetTessellationMode();
	if (RHISupportsTessellation(GShaderPlatformForFeatureLevel[InFeatureLevel])
		&& InVertexFactory->GetType()->SupportsTessellationShaders() 
		&& MaterialTessellationMode != MTM_NoTessellation)
	{
		bool HasHullShader = MeshShaderIndex->HasShader(&FVelocityHS::StaticType);
		bool HasDomainShader = MeshShaderIndex->HasShader(&FVelocityDS::StaticType);

		HullShader = HasHullShader ? MeshShaderIndex->GetShader<FVelocityHS>() : NULL;
		DomainShader = HasDomainShader ? MeshShaderIndex->GetShader<FVelocityDS>() : NULL;
	}

	bool HasVertexShader = MeshShaderIndex->HasShader(&FVelocityVS::StaticType);
	VertexShader = HasVertexShader ? MeshShaderIndex->GetShader<FVelocityVS>() : NULL;

	bool HasPixelShader = MeshShaderIndex->HasShader(&FVelocityPS::StaticType);
	PixelShader = HasPixelShader ? MeshShaderIndex->GetShader<FVelocityPS>() : NULL;
}

bool FVelocityDrawingPolicy::SupportsVelocity() const
{
	if (VertexShader && PixelShader && VertexShader->SupportsVelocity() && GPixelFormats[PF_G16R16].Supported)
	{
		return true;
	}
	else
	{
		return false;
	}
}

void FVelocityDrawingPolicy::SetSharedState(FRHICommandList& RHICmdList, const FSceneView* SceneView, const ContextDataType PolicyContext) const
{
	// NOTE: Assuming this cast is always safe!
	FViewInfo* View = (FViewInfo*)SceneView;

	VertexShader->SetParameters(RHICmdList, VertexFactory, MaterialRenderProxy, *View);
	PixelShader->SetParameters(RHICmdList, VertexFactory, MaterialRenderProxy, *View);

	if(HullShader && DomainShader)
	{
		HullShader->SetParameters(RHICmdList, MaterialRenderProxy, *View);
		DomainShader->SetParameters(RHICmdList, MaterialRenderProxy, *View);
	}

	// Set the shared mesh resources.
	FMeshDrawingPolicy::SetSharedState(RHICmdList, View, PolicyContext);
}

void FVelocityDrawingPolicy::SetMeshRenderState(
	FRHICommandList& RHICmdList, 
	const FSceneView& SceneView,
	const FPrimitiveSceneProxy* PrimitiveSceneProxy,
	const FMeshBatch& Mesh,
	int32 BatchElementIndex,
	bool bBackFace,
	const ElementDataType& ElementData,
	const ContextDataType PolicyContext
	) const
{
	const FMeshBatchElement& BatchElement = Mesh.Elements[BatchElementIndex];
	FMatrix PreviousLocalToWorld;
	
	FViewInfo& View = (FViewInfo&)SceneView;	// NOTE: Assuming this cast is always safe!

	// hack
	FScene* Scene = (FScene*)&PrimitiveSceneProxy->GetScene();

	// previous transform can be stored in the scene for each primitive
	if(Scene->MotionBlurInfoData.GetPrimitiveMotionBlurInfo(PrimitiveSceneProxy->GetPrimitiveSceneInfo(), PreviousLocalToWorld))
	{
		VertexShader->SetMesh(RHICmdList, VertexFactory, Mesh, BatchElementIndex, View, PrimitiveSceneProxy, PreviousLocalToWorld);
	}	
	else
	{
		const FMatrix& LocalToWorld = PrimitiveSceneProxy->GetLocalToWorld();		// different implementation from UE4
		VertexShader->SetMesh(RHICmdList, VertexFactory, Mesh, BatchElementIndex, View, PrimitiveSceneProxy, LocalToWorld);
	}

	if (HullShader && DomainShader)
	{
		DomainShader->SetMesh(RHICmdList, VertexFactory, View, PrimitiveSceneProxy, Mesh.Elements[BatchElementIndex]);
		HullShader->SetMesh(RHICmdList, VertexFactory, View, PrimitiveSceneProxy, Mesh.Elements[BatchElementIndex]);
	}

	PixelShader->SetMesh(RHICmdList, VertexFactory, Mesh, BatchElementIndex, View, PrimitiveSceneProxy, bBackFace);
	FMeshDrawingPolicy::SetMeshRenderState(RHICmdList, View, PrimitiveSceneProxy, Mesh, BatchElementIndex, bBackFace, ElementData, PolicyContext);
}

bool FVelocityDrawingPolicy::HasVelocity(const FViewInfo& View, const FPrimitiveSceneInfo* PrimitiveSceneInfo)
{
	checkSlow(IsInParallelRenderingThread());

	// No velocity if motionblur is off, or if it's a non-moving object (treat as background in that case)
	if (View.bCameraCut || !PrimitiveSceneInfo->Proxy->IsMovable())
	{
		return false;
	}

	if (PrimitiveSceneInfo->Proxy->AlwaysHasVelocity())
	{
		return true;
	}

	if (PrimitiveSceneInfo->bVelocityIsSupressed)
	{
		return false;
	}

	// check if the primitive has moved
	{
		FMatrix PreviousLocalToWorld;

		// hack
		FScene* Scene = PrimitiveSceneInfo->Scene;

		if (Scene->MotionBlurInfoData.GetPrimitiveMotionBlurInfo(PrimitiveSceneInfo, PreviousLocalToWorld))
		{
			check(PrimitiveSceneInfo->Proxy);

			const FMatrix& LocalToWorld = PrimitiveSceneInfo->Proxy->GetLocalToWorld();

			// Hasn't moved (treat as background by not rendering any special velocities)?
			if (LocalToWorld.Equals(PreviousLocalToWorld, 0.0001f))
			{
				return false;
			}
		}
		else 
		{
			return false;
		}
	}

	return true;
}

bool FVelocityDrawingPolicy::HasVelocityOnBasePass(const FViewInfo& View,const FPrimitiveSceneProxy* PrimitiveSceneProxy, const FPrimitiveSceneInfo* PrimitiveSceneInfo, const FMeshBatch& Mesh, bool& bOutHasTransform, FMatrix& OutTransform)
{
	checkSlow(IsInParallelRenderingThread());
	// No velocity if motionblur is off, or if it's a non-moving object (treat as background in that case)
	if (View.bCameraCut)
	{
		return false;
	}

	//@todo-rco: Where is this set?
	if (PrimitiveSceneInfo->bVelocityIsSupressed)
	{
		return false;
	}

	// hack
	FScene* Scene = PrimitiveSceneInfo->Scene;
	if (Scene->MotionBlurInfoData.GetPrimitiveMotionBlurInfo(PrimitiveSceneInfo, OutTransform))
	{
		bOutHasTransform = true;
/*
		const FMatrix& LocalToWorld = PrimitiveSceneProxy->GetLocalToWorld();
		// Hasn't moved (treat as background by not rendering any special velocities)?
		if (LocalToWorld.Equals(OutTransform, 0.0001f))
		{
			return false;
		}
*/
		return true;
	}

	bOutHasTransform = false;
	if (PrimitiveSceneProxy->IsMovable())
	{
		return true;
	}

	//@todo-rco: Optimize finding WPO!
	auto* Material = Mesh.MaterialRenderProxy->GetMaterial(Scene->GetFeatureLevel());
	return Material->MaterialModifiesMeshPosition_RenderThread() && Material->OutputsVelocityOnBasePass();
}

FBoundShaderStateInput FVelocityDrawingPolicy::GetBoundShaderStateInput(ERHIFeatureLevel::Type InFeatureLevel)
{
	return FBoundShaderStateInput(
		FMeshDrawingPolicy::GetVertexDeclaration(), 
		VertexShader->GetVertexShader(),
		GETSAFERHISHADER_HULL(HullShader), 
		GETSAFERHISHADER_DOMAIN(DomainShader),
		PixelShader->GetPixelShader(),
		FGeometryShaderRHIRef());
}

int32 Compare(const FVelocityDrawingPolicy& A,const FVelocityDrawingPolicy& B)
{
	COMPAREDRAWINGPOLICYMEMBERS(VertexShader);
	COMPAREDRAWINGPOLICYMEMBERS(PixelShader);
	COMPAREDRAWINGPOLICYMEMBERS(HullShader);
	COMPAREDRAWINGPOLICYMEMBERS(DomainShader);
	COMPAREDRAWINGPOLICYMEMBERS(VertexFactory);
	COMPAREDRAWINGPOLICYMEMBERS(MaterialRenderProxy);
	return 0;
}


//=============================================================================
/** Policy to wrap FMeshDrawingPolicy with new shaders. */

void FVelocityDrawingPolicyFactory::AddStaticMesh(FScene* Scene, FStaticMesh* StaticMesh, ContextType)
{
	const auto FeatureLevel = Scene->GetFeatureLevel();
	const FMaterialRenderProxy* MaterialRenderProxy = StaticMesh->MaterialRenderProxy;
	const FMaterial* Material = MaterialRenderProxy->GetMaterial(FeatureLevel);

	// Velocity only needs to be directly rendered for movable meshes
	if (StaticMesh->PrimitiveSceneInfo->Proxy->IsMovable())
	{
	    EBlendMode BlendMode = Material->GetBlendMode();
	    if(BlendMode == BLEND_Opaque || BlendMode == BLEND_Masked)
	    {
			if (!Material->IsMasked() && !Material->IsTwoSided() && !Material->MaterialModifiesMeshPosition_RenderThread())
		    {
			    // Default material doesn't handle masked or mesh-mod, and doesn't have the correct bIsTwoSided setting.
			    MaterialRenderProxy = UMaterial::GetDefaultMaterial(MD_Surface)->GetRenderProxy(false);
		    }

			FVelocityDrawingPolicy DrawingPolicy(StaticMesh->VertexFactory, MaterialRenderProxy, *MaterialRenderProxy->GetMaterial(FeatureLevel), FeatureLevel);
			if (DrawingPolicy.SupportsVelocity())
			{
				// Add the static mesh to the depth-only draw list.
				Scene->VelocityDrawList.AddMesh(StaticMesh, FVelocityDrawingPolicy::ElementDataType(), DrawingPolicy, FeatureLevel);
			}
	    }
	}
}

bool FVelocityDrawingPolicyFactory::DrawDynamicMesh(
	FRHICommandList& RHICmdList, 
	const FViewInfo& View,
	ContextType DrawingContext,
	const FMeshBatch& Mesh,
	bool bBackFace,
	bool bPreFog,
	const FPrimitiveSceneProxy* PrimitiveSceneProxy,
	FHitProxyId HitProxyId
	)
{
	// Only draw opaque materials in the depth pass.
	const auto FeatureLevel = View.GetFeatureLevel();
	const FMaterialRenderProxy* MaterialRenderProxy = Mesh.MaterialRenderProxy;
	const FMaterial* Material = MaterialRenderProxy->GetMaterial(FeatureLevel);
	EBlendMode BlendMode = Material->GetBlendMode();

	if(BlendMode == BLEND_Opaque || BlendMode == BLEND_Masked)
	{
		// This should be enforced at a higher level
		//@todo - figure out why this is failing and re-enable
		//check(FVelocityDrawingPolicy::HasVelocity(View, PrimitiveSceneInfo));
		if (!Material->IsMasked() && !Material->IsTwoSided() && !Material->MaterialModifiesMeshPosition_RenderThread())
		{
			// Default material doesn't handle masked, and doesn't have the correct bIsTwoSided setting.
			MaterialRenderProxy = UMaterial::GetDefaultMaterial(MD_Surface)->GetRenderProxy(false);
		}
		FVelocityDrawingPolicy DrawingPolicy(Mesh.VertexFactory, MaterialRenderProxy, *MaterialRenderProxy->GetMaterial(FeatureLevel), FeatureLevel);
		if(DrawingPolicy.SupportsVelocity())
		{			
			RHICmdList.BuildAndSetLocalBoundShaderState(DrawingPolicy.GetBoundShaderStateInput(FeatureLevel));
			DrawingPolicy.SetSharedState(RHICmdList, &View, FVelocityDrawingPolicy::ContextDataType());
			for (int32 BatchElementIndex = 0, BatchElementCount = Mesh.Elements.Num(); BatchElementIndex < BatchElementCount; ++BatchElementIndex)
			{
				DrawingPolicy.SetMeshRenderState(RHICmdList, View, PrimitiveSceneProxy, Mesh, BatchElementIndex, bBackFace, FMeshDrawingPolicy::ElementDataType(), FVelocityDrawingPolicy::ContextDataType());
				DrawingPolicy.DrawMesh(RHICmdList, Mesh, BatchElementIndex);
			}
			return true;
		}
	}

	return false;
}


int32 GetMotionBlurQualityFromCVar()
{
	int32 MotionBlurQuality;

	static const auto ICVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.MotionBlurQuality"));
	MotionBlurQuality = FMath::Clamp(ICVar->GetValueOnRenderThread(), 0, 4);

	return MotionBlurQuality;
}

bool IsMotionBlurEnabled(const FViewInfo& View)
{
	if (!(View.GetFeatureLevel() >= ERHIFeatureLevel::SM4))
	{
		return false; 
	}

	int32 MotionBlurQuality = GetMotionBlurQualityFromCVar();

	return View.Family->EngineShowFlags.PostProcessing
		&& View.Family->EngineShowFlags.MotionBlur
		&& View.FinalPostProcessSettings.MotionBlurAmount > 0.001f
		&& View.FinalPostProcessSettings.MotionBlurMax > 0.001f
		&& View.Family->bRealtimeUpdate
		&& MotionBlurQuality > 0
		&& !View.bIsSceneCapture
		&& !(View.Family->Views.Num() > 1);
}

void FDeferredShadingSceneRenderer::RenderDynamicVelocitiesMeshElementsInner(FRHICommandList& RHICmdList, const FViewInfo& View, int32 FirstIndex, int32 LastIndex)
{
	FVelocityDrawingPolicyFactory::ContextType Context(DDM_AllOccluders);

	for (int32 MeshBatchIndex = FirstIndex; MeshBatchIndex <= LastIndex; MeshBatchIndex++)
	{
		const FMeshBatchAndRelevance& MeshBatchAndRelevance = View.DynamicMeshElements[MeshBatchIndex];

		if (MeshBatchAndRelevance.bHasOpaqueOrMaskedMaterial
			&& MeshBatchAndRelevance.PrimitiveSceneProxy->GetPrimitiveSceneInfo()->ShouldRenderVelocity(View))
		{
			const FMeshBatch& MeshBatch = *MeshBatchAndRelevance.Mesh;
			FVelocityDrawingPolicyFactory::DrawDynamicMesh(RHICmdList, View, Context, MeshBatch, false, true, MeshBatchAndRelevance.PrimitiveSceneProxy, MeshBatch.BatchHitProxyId);
		}
	}
}

class FRenderVelocityDynamicThreadTask
{
	FDeferredShadingSceneRenderer& ThisRenderer;
	FRHICommandList& RHICmdList;
	const FViewInfo& View;
	int32 FirstIndex;
	int32 LastIndex;

public:

	FRenderVelocityDynamicThreadTask(
		FDeferredShadingSceneRenderer& InThisRenderer,
		FRHICommandList& InRHICmdList,
		const FViewInfo& InView,
		int32 InFirstIndex, int32 InLastIndex
		)
		: ThisRenderer(InThisRenderer)
		, RHICmdList(InRHICmdList)
		, View(InView)
		, FirstIndex(InFirstIndex)
		, LastIndex(InLastIndex)
	{
	}

	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FRenderVelocityDynamicThreadTask, STATGROUP_TaskGraphTasks);
	}

	ENamedThreads::Type GetDesiredThread()
	{
		return ENamedThreads::AnyThread;
	}

	static ESubsequentsMode::Type GetSubsequentsMode() { return ESubsequentsMode::TrackSubsequents; }

	void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
	{
		ThisRenderer.RenderDynamicVelocitiesMeshElementsInner(RHICmdList, View, FirstIndex, LastIndex);
		RHICmdList.HandleRTThreadTaskCompletion(MyCompletionGraphEvent);
	}
};

static void BeginVelocityRendering(FRHICommandList& RHICmdList, TRefCountPtr<IPooledRenderTarget>& VelocityRT, bool bPerformClear)
{
	FTextureRHIRef VelocityTexture = VelocityRT->GetRenderTargetItem().TargetableTexture;
	FTexture2DRHIRef DepthTexture = GSceneRenderTargets.GetSceneDepthTexture();
	FLinearColor VelocityClearColor = FLinearColor::Black;
	if (bPerformClear)
	{
		// now make the FRHISetRenderTargetsInfo that encapsulates all of the info
		FRHIRenderTargetView ColorView(VelocityTexture, 0, -1, ERenderTargetLoadAction::EClear, ERenderTargetStoreAction::EStore);
		FRHIDepthRenderTargetView DepthView(DepthTexture, ERenderTargetLoadAction::ELoad, ERenderTargetStoreAction::ENoAction, FExclusiveDepthStencil::DepthRead_StencilWrite);

		FRHISetRenderTargetsInfo Info(1, &ColorView, DepthView);
		Info.ClearColors[0] = VelocityClearColor;

		// Clear the velocity buffer (0.0f means "use static background velocity").
		RHICmdList.SetRenderTargetsAndClear(Info);		
	}
	else
	{
		SetRenderTarget(RHICmdList, VelocityTexture, DepthTexture, ESimpleRenderTargetMode::EExistingColorAndDepth, FExclusiveDepthStencil::DepthRead_StencilWrite);

		// some platforms need the clear color when rendertargets transition to SRVs.  We propagate here to allow parallel rendering to always
		// have the proper mapping when the RT is transitioned.
		RHICmdList.BindClearMRTValues(true, 1, &VelocityClearColor, false, (float)ERHIZBuffer::FarPlane, false, 0);
	}
}

static void SetVelocitiesState(FRHICommandList& RHICmdList, const FViewInfo& View, TRefCountPtr<IPooledRenderTarget>& VelocityRT)
{
	const FIntPoint BufferSize = GSceneRenderTargets.GetBufferSizeXY();
	const FIntPoint VelocityBufferSize = BufferSize;		// full resolution so we can reuse the existing full res z buffer

	const uint32 MinX = View.ViewRect.Min.X * VelocityBufferSize.X / BufferSize.X;
	const uint32 MinY = View.ViewRect.Min.Y * VelocityBufferSize.Y / BufferSize.Y;
	const uint32 MaxX = View.ViewRect.Max.X * VelocityBufferSize.X / BufferSize.X;
	const uint32 MaxY = View.ViewRect.Max.Y * VelocityBufferSize.Y / BufferSize.Y;

	RHICmdList.SetViewport(MinX, MinY, 0.0f, MaxX, MaxY, 1.0f);

	RHICmdList.SetBlendState(TStaticBlendState<CW_RGBA>::GetRHI());
	RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false,CF_DepthNearOrEqual>::GetRHI());
	RHICmdList.SetRasterizerState(GetStaticRasterizerState<true>(FM_Solid, CM_CW));
}

class FVelocityPassParallelCommandListSet : public FParallelCommandListSet
{
	TRefCountPtr<IPooledRenderTarget>& VelocityRT;
public:
	FVelocityPassParallelCommandListSet(const FViewInfo& InView, FRHICommandList& InParentCmdList, bool* InOutDirty, bool bInParallelExecute, TRefCountPtr<IPooledRenderTarget>& InVelocityRT)
		: FParallelCommandListSet(InView, InParentCmdList, InOutDirty, bInParallelExecute)
		, VelocityRT(InVelocityRT)
	{
		SetStateOnCommandList(ParentCmdList);
	}

	virtual ~FVelocityPassParallelCommandListSet()
	{
		Dispatch();
	}	

	virtual void SetStateOnCommandList(FRHICommandList& CmdList) override
	{
		BeginVelocityRendering(CmdList, VelocityRT, false);
		SetVelocitiesState(CmdList, View, VelocityRT);
	}
};

static TAutoConsoleVariable<int32> CVarRHICmdVelocityPassDeferredContexts(
	TEXT("r.RHICmdVelocityPassDeferredContexts"),
	1,
	TEXT("True to use deferred contexts to parallelize velocity pass command list execution."));

void FDeferredShadingSceneRenderer::RenderVelocitiesInnerParallel(FRHICommandListImmediate& RHICmdList, TRefCountPtr<IPooledRenderTarget>& VelocityRT)
{
	// parallel version
	FScopedCommandListWaitForTasks Flusher(RHICmdList);

	for(int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		const FViewInfo& View = Views[ViewIndex];

		FVelocityPassParallelCommandListSet ParallelCommandListSet(View, RHICmdList, nullptr, CVarRHICmdVelocityPassDeferredContexts.GetValueOnRenderThread() > 0, VelocityRT);

		Scene->VelocityDrawList.DrawVisibleParallel(View.StaticMeshVelocityMap, View.StaticMeshBatchVisibility, ParallelCommandListSet);

		int32 NumPrims = View.DynamicMeshElements.Num();
		int32 EffectiveThreads = FMath::Min<int32>(NumPrims, ParallelCommandListSet.Width);

		int32 Start = 0;
		if (EffectiveThreads)
		{
			check(IsInRenderingThread());

			int32 NumPer = NumPrims / EffectiveThreads;
			int32 Extra = NumPrims - NumPer * EffectiveThreads;


			for (int32 ThreadIndex = 0; ThreadIndex < EffectiveThreads; ThreadIndex++)
			{
				int32 Last = Start + (NumPer - 1) + (ThreadIndex < Extra);
				check(Last >= Start);

				FRHICommandList* CmdList = ParallelCommandListSet.NewParallelCommandList();

				FGraphEventRef AnyThreadCompletionEvent = TGraphTask<FRenderVelocityDynamicThreadTask>::CreateTask(nullptr, ENamedThreads::RenderThread)
					.ConstructAndDispatchWhenReady(*this, *CmdList, View, Start, Last);

				ParallelCommandListSet.AddParallelCommandList(CmdList, AnyThreadCompletionEvent);

				Start = Last + 1;
			}
		}
	}
}

void FDeferredShadingSceneRenderer::RenderVelocitiesInner(FRHICommandListImmediate& RHICmdList, TRefCountPtr<IPooledRenderTarget>& VelocityRT)
{
	for(int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		const FViewInfo& View = Views[ViewIndex];

		SetVelocitiesState(RHICmdList, View, VelocityRT);
		// Draw velocities for movable static meshes.
		Scene->VelocityDrawList.DrawVisible(RHICmdList, View, View.StaticMeshVelocityMap, View.StaticMeshBatchVisibility);

		RenderDynamicVelocitiesMeshElementsInner(RHICmdList, View, 0, View.DynamicMeshElements.Num() - 1);
	}
}



static TAutoConsoleVariable<int32> CVarParallelVelocity(
	TEXT("r.ParallelVelocity"),
	1,  
	TEXT("Toggles parallel velocity rendering. Parallel rendering must be enabled for this to have an effect."),
	ECVF_RenderThreadSafe
	);

bool FDeferredShadingSceneRenderer::ShouldRenderVelocities() const
{
	if (!GPixelFormats[PF_G16R16].Supported)
	{
		return false;
	}

	bool bNeedsVelocity = false;
	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		const FViewInfo& View = Views[ViewIndex];

		bool bTemporalAA = (View.FinalPostProcessSettings.AntiAliasingMethod == AAM_TemporalAA) && !View.bCameraCut;
		bool bMotionBlur = IsMotionBlurEnabled(View);
		bool bDistanceFieldAO = ShouldPrepareForDistanceFieldAO();

		bNeedsVelocity |= (bMotionBlur || bTemporalAA || bDistanceFieldAO) && !View.bIsSceneCapture;
	}

	return bNeedsVelocity;
}

void FDeferredShadingSceneRenderer::RenderVelocities(FRHICommandListImmediate& RHICmdList, TRefCountPtr<IPooledRenderTarget>& VelocityRT)
{
	check(FeatureLevel >= ERHIFeatureLevel::SM4);
	SCOPE_CYCLE_COUNTER(STAT_RenderVelocities);

	if (!ShouldRenderVelocities())
	{
		return;
	}

	// this is not supported
	check(!Views[0].bIsSceneCapture);

	SCOPED_DRAW_EVENT(RHICmdList, RenderVelocities);

	FPooledRenderTargetDesc Desc = FVelocityRendering::GetRenderTargetDesc();
	GRenderTargetPool.FindFreeElement(Desc, VelocityRT, TEXT("Velocity"));

	{
		static const auto MotionBlurDebugVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.MotionBlurDebug"));

		if(MotionBlurDebugVar->GetValueOnRenderThread())
		{
			UE_LOG(LogEngine, Log, TEXT("r.MotionBlurDebug: FrameNumber=%d Pause=%d"), ViewFamily.FrameNumber, ViewFamily.bWorldIsPaused ? 1 : 0);
		}
	}

	{
		GPrevPerBoneMotionBlur.StartAppend(ViewFamily.bWorldIsPaused);

		BeginVelocityRendering(RHICmdList, VelocityRT, true);

		if (GRHICommandList.UseParallelAlgorithms() && CVarParallelVelocity.GetValueOnRenderThread())
		{
			RenderVelocitiesInnerParallel(RHICmdList, VelocityRT);
		}
		else
		{
			RenderVelocitiesInner(RHICmdList, VelocityRT);
		}

		RHICmdList.CopyToResolveTarget(VelocityRT->GetRenderTargetItem().TargetableTexture, VelocityRT->GetRenderTargetItem().ShaderResourceTexture, false, FResolveParams());

		GPrevPerBoneMotionBlur.EndAppend();
	}

	// restore any color write state changes
	RHICmdList.SetBlendState(TStaticBlendState<>::GetRHI());
	RHICmdList.SetRasterizerState(TStaticRasterizerState<>::GetRHI());

	// to be able to observe results with VisualizeTexture
	GRenderTargetPool.VisualizeTexture.SetCheckPoint(RHICmdList, VelocityRT);
}

FPooledRenderTargetDesc FVelocityRendering::GetRenderTargetDesc()
{
	const FIntPoint BufferSize = GSceneRenderTargets.GetBufferSizeXY();
	const FIntPoint VelocityBufferSize = BufferSize;		// full resolution so we can reuse the existing full res z buffer
	return FPooledRenderTargetDesc(FPooledRenderTargetDesc::Create2DDesc(VelocityBufferSize, PF_G16R16, TexCreate_None, TexCreate_RenderTargetable, false));
}

bool FVelocityRendering::OutputsToGBuffer()
{
	return CVarBasePassOutputsVelocity.GetValueOnAnyThread() == 1;
}
