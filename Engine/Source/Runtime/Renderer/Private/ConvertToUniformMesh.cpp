// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	SurfelTree.cpp
=============================================================================*/

#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "UniformBuffer.h"
#include "ShaderParameters.h"
#include "PostProcessing.h"
#include "SceneFilterRendering.h"
#include "DistanceFieldLightingShared.h"
#include "DistanceFieldSurfaceCacheLighting.h"
#include "DistanceFieldGlobalIllumination.h"
#include "RHICommandList.h"
#include "SceneUtils.h"
#include "DistanceFieldAtlas.h"


class FConvertToUniformMeshVS : public FMeshMaterialShader
{
	DECLARE_SHADER_TYPE(FConvertToUniformMeshVS,MeshMaterial);

protected:

	FConvertToUniformMeshVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		:	FMeshMaterialShader(Initializer)
	{
	}

	FConvertToUniformMeshVS()
	{
	}

	static bool ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) 
			&& DoesPlatformSupportDistanceFieldGI(Platform)
			&& (FCString::Strstr(VertexFactoryType->GetName(), TEXT("LocalVertexFactory")) != NULL
				|| FCString::Strstr(VertexFactoryType->GetName(), TEXT("InstancedStaticMeshVertexFactory")) != NULL);
	}

public:
	
	void SetParameters(FRHICommandList& RHICmdList, const FVertexFactory* VertexFactory,const FMaterialRenderProxy* MaterialRenderProxy,const FSceneView* View)
	{
		FMeshMaterialShader::SetParameters(RHICmdList, GetVertexShader(), MaterialRenderProxy, *MaterialRenderProxy->GetMaterial(View->GetFeatureLevel()), *View, ESceneRenderTargetsMode::SetTextures);
	}

	void SetMesh(FRHICommandList& RHICmdList, const FVertexFactory* VertexFactory,const FSceneView& View,const FPrimitiveSceneProxy* Proxy,const FMeshBatchElement& BatchElement)
	{
		FMeshMaterialShader::SetMesh(RHICmdList, GetVertexShader(),VertexFactory,View,Proxy,BatchElement);
	}
};

IMPLEMENT_MATERIAL_SHADER_TYPE(,FConvertToUniformMeshVS,TEXT("ConvertToUniformMesh"),TEXT("ConvertToUniformMeshVS"),SF_Vertex); 

void GetUniformMeshStreamOutLayout(FStreamOutElementList& Layout)
{
	Layout.Add(FStreamOutElement(0, "SV_Position", 0, 4, 0));
	Layout.Add(FStreamOutElement(0, "Tangent", 0, 3, 0));
	Layout.Add(FStreamOutElement(0, "Tangent", 1, 3, 0));
	Layout.Add(FStreamOutElement(0, "Tangent", 2, 3, 0));
	Layout.Add(FStreamOutElement(0, "UV", 0, 2, 0));
	Layout.Add(FStreamOutElement(0, "UV", 1, 2, 0));
	Layout.Add(FStreamOutElement(0, "VertexColor", 0, 4, 0));
}

// In float4's, must match usf
int32 FSurfelBuffers::InterpolatedVertexDataStride = 6;

/** Returns number of float's in the uniform vertex. */
int32 ComputeUniformVertexStride()
{
	FStreamOutElementList Layout;
	int32 StreamStride = 0;

	GetUniformMeshStreamOutLayout(Layout);

	for (int32 ElementIndex = 0; ElementIndex < Layout.Num(); ElementIndex++)
	{
		StreamStride += Layout[ElementIndex].ComponentCount;
	}

	// D3D11 stream out buffer element stride must be a factor of 4
	return FMath::DivideAndRoundUp(StreamStride, 4) * 4;
}

void FUniformMeshBuffers::Initialize()
{
	if (MaxElements > 0)
	{
		const int32 VertexStride = ComputeUniformVertexStride();
		FRHIResourceCreateInfo CreateInfo;
		TriangleData = RHICreateVertexBuffer(MaxElements * VertexStride * GPixelFormats[PF_R32_FLOAT].BlockBytes, BUF_ShaderResource | BUF_StreamOutput, CreateInfo);
		TriangleDataSRV = RHICreateShaderResourceView(TriangleData, GPixelFormats[PF_R32_FLOAT].BlockBytes, PF_R32_FLOAT);

		TriangleAreas.Initialize(sizeof(float), MaxElements, PF_R32_FLOAT);
		TriangleCDFs.Initialize(sizeof(float), MaxElements, PF_R32_FLOAT);
	}
}

class FConvertToUniformMeshGS : public FMeshMaterialShader
{
	DECLARE_SHADER_TYPE(FConvertToUniformMeshGS,MeshMaterial);

protected:

	FConvertToUniformMeshGS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		:	FMeshMaterialShader(Initializer)
	{
	}

	FConvertToUniformMeshGS()
	{
	}

	static bool ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) 
			&& DoesPlatformSupportDistanceFieldGI(Platform)
			&& (FCString::Strstr(VertexFactoryType->GetName(), TEXT("LocalVertexFactory")) != NULL
				|| FCString::Strstr(VertexFactoryType->GetName(), TEXT("InstancedStaticMeshVertexFactory")) != NULL);
	}

	static void GetStreamOutElements(FStreamOutElementList& ElementList, TArray<uint32>& StreamStrides, int32& RasterizedStream)
	{
		StreamStrides.Add(ComputeUniformVertexStride() * 4);
		GetUniformMeshStreamOutLayout(ElementList);
		RasterizedStream = -1;
	}

public:
	
	void SetParameters(FRHICommandList& RHICmdList, const FVertexFactory* VertexFactory,const FMaterialRenderProxy* MaterialRenderProxy,const FSceneView* View)
	{
		FMeshMaterialShader::SetParameters(RHICmdList, (FGeometryShaderRHIParamRef)GetGeometryShader(), MaterialRenderProxy, *MaterialRenderProxy->GetMaterial(View->GetFeatureLevel()), *View, ESceneRenderTargetsMode::SetTextures);
	}

	void SetMesh(FRHICommandList& RHICmdList, const FVertexFactory* VertexFactory,const FSceneView& View,const FPrimitiveSceneProxy* Proxy,const FMeshBatchElement& BatchElement)
	{
		FMeshMaterialShader::SetMesh(RHICmdList, (FGeometryShaderRHIParamRef)GetGeometryShader(),VertexFactory,View,Proxy,BatchElement);
	}
};

IMPLEMENT_MATERIAL_SHADER_TYPE(,FConvertToUniformMeshGS,TEXT("ConvertToUniformMesh"),TEXT("ConvertToUniformMeshGS"),SF_Geometry); 


class FConvertToUniformMeshDrawingPolicy : public FMeshDrawingPolicy
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
	FConvertToUniformMeshDrawingPolicy(
		const FVertexFactory* InVertexFactory,
		const FMaterialRenderProxy* InMaterialRenderProxy,
		const FMaterial& MaterialResouce,
		ERHIFeatureLevel::Type InFeatureLevel
		);

	// FMeshDrawingPolicy interface.

	/**
	* Match two draw policies
	* @param Other - draw policy to compare
	* @return true if the draw policies are a match
	*/
	bool Matches(const FConvertToUniformMeshDrawingPolicy& Other) const;

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
		const ElementDataType& ElementData,
		const ContextDataType PolicyContext
		) const;

private:
	FConvertToUniformMeshVS* VertexShader;
	FConvertToUniformMeshGS* GeometryShader;
};

FConvertToUniformMeshDrawingPolicy::FConvertToUniformMeshDrawingPolicy(
	const FVertexFactory* InVertexFactory,
	const FMaterialRenderProxy* InMaterialRenderProxy,
	const FMaterial& InMaterialResource,
	ERHIFeatureLevel::Type InFeatureLevel
	)
:	FMeshDrawingPolicy(InVertexFactory,InMaterialRenderProxy,InMaterialResource,false)
{
	VertexShader = InMaterialResource.GetShader<FConvertToUniformMeshVS>(InVertexFactory->GetType());
	GeometryShader = InMaterialResource.GetShader<FConvertToUniformMeshGS>(InVertexFactory->GetType());
}

bool FConvertToUniformMeshDrawingPolicy::Matches(
	const FConvertToUniformMeshDrawingPolicy& Other
	) const
{
	return FMeshDrawingPolicy::Matches(Other) &&
		VertexShader == Other.VertexShader &&
		GeometryShader == Other.GeometryShader;
}

void FConvertToUniformMeshDrawingPolicy::SetSharedState(
	FRHICommandList& RHICmdList, 
	const FSceneView* View,
	const ContextDataType PolicyContext
	) const
{
	// Set shared mesh resources
	FMeshDrawingPolicy::SetSharedState(RHICmdList, View, PolicyContext);

	VertexShader->SetParameters(RHICmdList, VertexFactory, MaterialRenderProxy, View);
	GeometryShader->SetParameters(RHICmdList, VertexFactory, MaterialRenderProxy, View);
}

FBoundShaderStateInput FConvertToUniformMeshDrawingPolicy::GetBoundShaderStateInput(ERHIFeatureLevel::Type InFeatureLevel)
{
	return FBoundShaderStateInput(
		FMeshDrawingPolicy::GetVertexDeclaration(), 
		VertexShader->GetVertexShader(),
		NULL, 
		NULL,
		NULL,
		GeometryShader->GetGeometryShader());

}

void FConvertToUniformMeshDrawingPolicy::SetMeshRenderState(
	FRHICommandList& RHICmdList, 
	const FSceneView& View,
	const FPrimitiveSceneProxy* PrimitiveSceneProxy,
	const FMeshBatch& Mesh,
	int32 BatchElementIndex,
	bool bBackFace,
	const ElementDataType& ElementData,
	const ContextDataType PolicyContext
	) const
{
	const FMeshBatchElement& BatchElement = Mesh.Elements[BatchElementIndex];

	EmitMeshDrawEvents(RHICmdList, PrimitiveSceneProxy, Mesh);

	// Set transforms
	VertexShader->SetMesh(RHICmdList, VertexFactory,View,PrimitiveSceneProxy,BatchElement);
	GeometryShader->SetMesh(RHICmdList, VertexFactory,View,PrimitiveSceneProxy,BatchElement);
	
	// Set rasterizer state.
	RHICmdList.SetRasterizerState(GetStaticRasterizerState<true>(
		(Mesh.bWireframe || IsWireframe()) ? FM_Wireframe : FM_Solid,
		IsTwoSided() ? CM_None : (XOR( XOR(View.bReverseCulling,bBackFace), Mesh.ReverseCulling) ? CM_CCW : CM_CW)));
}

bool ShouldGenerateSurfelsOnMesh(const FMeshBatch& Mesh, ERHIFeatureLevel::Type FeatureLevel)
{
	//@todo - support for tessellated meshes
	return Mesh.Type == PT_TriangleList 
		&& !Mesh.IsTranslucent(FeatureLevel) 
		&& Mesh.MaterialRenderProxy->GetMaterial(FeatureLevel)->GetShadingModel() != MSM_Unlit;
}

FUniformMeshBuffers GUniformMeshTemporaryBuffers;

TArray<TArray<FMeshBatchAndRelevance,SceneRenderingAllocator>> GDynamicMeshElements;

int32 FUniformMeshConverter::Convert(
	FRHICommandListImmediate& RHICmdList, 
	FSceneRenderer& Renderer,
	FViewInfo& View, 
	const FPrimitiveSceneInfo* PrimitiveSceneInfo, 
	FUniformMeshBuffers*& OutUniformMeshBuffers,
	const FMaterialRenderProxy*& OutMaterialRenderProxy,
	FUniformBufferRHIParamRef& OutPrimitiveUniformBuffer)
{
	const FPrimitiveSceneProxy* PrimitiveSceneProxy = PrimitiveSceneInfo->Proxy;
	const auto FeatureLevel = View.GetFeatureLevel();

	if (GDynamicMeshElements.Num() == 0)
	{
		GDynamicMeshElements.Empty(10000);
	}

	FSimpleElementCollector DynamicSimpleElements;
	GDynamicMeshElements.Add(TArray<FMeshBatchAndRelevance,SceneRenderingAllocator>());
	TArray<FMeshBatchAndRelevance,SceneRenderingAllocator>& DynamicMeshElements = GDynamicMeshElements.Last();
	TArray<const FSceneView*> ViewsArray;
	ViewsArray.Add(&View);

	Renderer.MeshCollector.ClearViewMeshArrays();
	Renderer.MeshCollector.AddViewMeshArrays(&View, &DynamicMeshElements, &DynamicSimpleElements, FeatureLevel);
	
	View.bRenderFirstInstanceOnly = true;
	Renderer.MeshCollector.SetPrimitive(PrimitiveSceneInfo->Proxy, PrimitiveSceneInfo->DefaultDynamicHitProxyId);
	PrimitiveSceneInfo->Proxy->GetDynamicMeshElements(ViewsArray, *(View.Family), 0x1, Renderer.MeshCollector);
	View.bRenderFirstInstanceOnly = false;

	int32 NumTriangles = 0;

	FPrimitiveViewRelevance ViewRelevance = PrimitiveSceneInfo->Proxy->GetViewRelevance(&View);

	if (ViewRelevance.bDynamicRelevance)
	{
		for (int32 MeshBatchIndex = 0; MeshBatchIndex < DynamicMeshElements.Num(); MeshBatchIndex++)
		{
			const FMeshBatchAndRelevance& MeshBatchAndRelevance = DynamicMeshElements[MeshBatchIndex];

			if (ShouldGenerateSurfelsOnMesh(*MeshBatchAndRelevance.Mesh, FeatureLevel))
			{
				NumTriangles += MeshBatchAndRelevance.Mesh->GetNumPrimitives();
			}
		}
	}

	if (ViewRelevance.bStaticRelevance)
	{
		float LargestScreenSize = 0;

		for (int32 StaticMeshIdx=0; StaticMeshIdx < PrimitiveSceneInfo->StaticMeshes.Num(); StaticMeshIdx++)
		{
			const FStaticMesh& StaticMesh = PrimitiveSceneInfo->StaticMeshes[StaticMeshIdx];
			LargestScreenSize = FMath::Max(LargestScreenSize, StaticMesh.ScreenSize);
		}

		for (int32 StaticMeshIdx=0; StaticMeshIdx < PrimitiveSceneInfo->StaticMeshes.Num(); StaticMeshIdx++)
		{
			const FStaticMesh& StaticMesh = PrimitiveSceneInfo->StaticMeshes[StaticMeshIdx];

			if (ShouldGenerateSurfelsOnMesh(StaticMesh, FeatureLevel)
				&& !StaticMesh.bShadowOnly
				// LOD0 only
				&& StaticMesh.ScreenSize == LargestScreenSize)
			{
				NumTriangles += StaticMesh.GetNumPrimitives();
			}
		}
	}

	if (NumTriangles > 0)
	{
		if (GUniformMeshTemporaryBuffers.MaxElements < NumTriangles * 3)
		{
			GUniformMeshTemporaryBuffers.MaxElements = NumTriangles * 3;
			GUniformMeshTemporaryBuffers.Release();
			GUniformMeshTemporaryBuffers.Initialize();
		}

		RHICmdList.SetRenderTargets(0, (const FRHIRenderTargetView*)NULL, NULL, 0, (const FUnorderedAccessViewRHIParamRef*)NULL);

		uint32 Offsets[1] = {0};
		const FVertexBufferRHIParamRef StreamOutTargets[1] = {GUniformMeshTemporaryBuffers.TriangleData.GetReference()};
		RHICmdList.SetStreamOutTargets(1, StreamOutTargets, Offsets);

		if (ViewRelevance.bDynamicRelevance)
		{
			for (int32 MeshBatchIndex = 0; MeshBatchIndex < DynamicMeshElements.Num(); MeshBatchIndex++)
			{
				const FMeshBatchAndRelevance& MeshBatchAndRelevance = DynamicMeshElements[MeshBatchIndex];

				if (ShouldGenerateSurfelsOnMesh(*MeshBatchAndRelevance.Mesh, FeatureLevel))
				{
					const FMeshBatch& Mesh = *MeshBatchAndRelevance.Mesh;

					FConvertToUniformMeshDrawingPolicy DrawingPolicy(
						Mesh.VertexFactory,
						Mesh.MaterialRenderProxy,
						*Mesh.MaterialRenderProxy->GetMaterial(FeatureLevel),
						FeatureLevel);

					//@todo - fix
					OutMaterialRenderProxy = Mesh.MaterialRenderProxy;

					RHICmdList.BuildAndSetLocalBoundShaderState(DrawingPolicy.GetBoundShaderStateInput(FeatureLevel));
					DrawingPolicy.SetSharedState(RHICmdList, &View, FConvertToUniformMeshDrawingPolicy::ContextDataType());

					for (int32 BatchElementIndex = 0; BatchElementIndex < Mesh.Elements.Num(); BatchElementIndex++)
					{
						//@todo - fix
						OutPrimitiveUniformBuffer = IsValidRef(Mesh.Elements[BatchElementIndex].PrimitiveUniformBuffer) 
							? Mesh.Elements[BatchElementIndex].PrimitiveUniformBuffer
							: *Mesh.Elements[BatchElementIndex].PrimitiveUniformBufferResource;

						DrawingPolicy.SetMeshRenderState(RHICmdList, View,PrimitiveSceneProxy,Mesh,BatchElementIndex,false,FConvertToUniformMeshDrawingPolicy::ElementDataType(), FConvertToUniformMeshDrawingPolicy::ContextDataType());
						DrawingPolicy.DrawMesh(RHICmdList, Mesh, BatchElementIndex);
					}
				}
			}
		}

		if (ViewRelevance.bStaticRelevance)
		{
			float LargestScreenSize = 0;

			for (int32 StaticMeshIdx=0; StaticMeshIdx < PrimitiveSceneInfo->StaticMeshes.Num(); StaticMeshIdx++)
			{
				const FStaticMesh& StaticMesh = PrimitiveSceneInfo->StaticMeshes[StaticMeshIdx];
				LargestScreenSize = FMath::Max(LargestScreenSize, StaticMesh.ScreenSize);
			}

			for (int32 StaticMeshIdx = 0; StaticMeshIdx < PrimitiveSceneInfo->StaticMeshes.Num(); StaticMeshIdx++)
			{
				const FStaticMesh& StaticMesh = PrimitiveSceneInfo->StaticMeshes[StaticMeshIdx];

				if (ShouldGenerateSurfelsOnMesh(StaticMesh, FeatureLevel) 
					&& !StaticMesh.bShadowOnly
					&& StaticMesh.ScreenSize == LargestScreenSize)
				{
					FConvertToUniformMeshDrawingPolicy DrawingPolicy(
						StaticMesh.VertexFactory,
						StaticMesh.MaterialRenderProxy,
						*StaticMesh.MaterialRenderProxy->GetMaterial(FeatureLevel),
						FeatureLevel
						);
					RHICmdList.BuildAndSetLocalBoundShaderState(DrawingPolicy.GetBoundShaderStateInput(FeatureLevel));
					DrawingPolicy.SetSharedState(RHICmdList, &View, FConvertToUniformMeshDrawingPolicy::ContextDataType());

					//@todo - fix
					OutMaterialRenderProxy = StaticMesh.MaterialRenderProxy;

					for (int32 BatchElementIndex = 0; BatchElementIndex < StaticMesh.Elements.Num(); BatchElementIndex++)
					{
						//@todo - fix
						OutPrimitiveUniformBuffer = IsValidRef(StaticMesh.Elements[BatchElementIndex].PrimitiveUniformBuffer) 
							? StaticMesh.Elements[BatchElementIndex].PrimitiveUniformBuffer
							: *StaticMesh.Elements[BatchElementIndex].PrimitiveUniformBufferResource;

						DrawingPolicy.SetMeshRenderState(RHICmdList, View,PrimitiveSceneProxy,StaticMesh,BatchElementIndex,false,
							FConvertToUniformMeshDrawingPolicy::ElementDataType(),
							FConvertToUniformMeshDrawingPolicy::ContextDataType()
							);
						DrawingPolicy.DrawMesh(RHICmdList, StaticMesh, BatchElementIndex);
					}
				}
			}
		}

		RHICmdList.SetStreamOutTargets(1, (const FVertexBufferRHIParamRef*)NULL, Offsets);
	}

	OutUniformMeshBuffers = &GUniformMeshTemporaryBuffers;
	return NumTriangles;
}

int32 GEvaluateSurfelMaterialGroupSize = 64;

class FEvaluateSurfelMaterialCS : public FMaterialShader
{
	DECLARE_SHADER_TYPE(FEvaluateSurfelMaterialCS,Material)
public:

	static bool ShouldCache(EShaderPlatform Platform, const FMaterial* Material)
	{
		//@todo - lit materials only 
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && DoesPlatformSupportDistanceFieldGI(Platform);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		FMaterialShader::ModifyCompilationEnvironment(Platform,OutEnvironment);
		OutEnvironment.SetDefine(TEXT("EVALUATE_SURFEL_MATERIAL_GROUP_SIZE"), GEvaluateSurfelMaterialGroupSize);
		OutEnvironment.SetDefine(TEXT("HAS_PRIMITIVE_UNIFORM_BUFFER"), 1);
	}

	FEvaluateSurfelMaterialCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FMaterialShader(Initializer)
	{
		SurfelBufferParameters.Bind(Initializer.ParameterMap);
		SurfelStartIndex.Bind(Initializer.ParameterMap, TEXT("SurfelStartIndex"));
		NumSurfelsToGenerate.Bind(Initializer.ParameterMap, TEXT("NumSurfelsToGenerate"));
		Instance0InverseTransform.Bind(Initializer.ParameterMap, TEXT("Instance0InverseTransform"));
	}

	FEvaluateSurfelMaterialCS()
	{
	}

	void SetParameters(
		FRHICommandList& RHICmdList, 
		const FSceneView& View, 
		int32 SurfelStartIndexValue,
		int32 NumSurfelsToGenerateValue,
		const FMaterialRenderProxy* MaterialProxy,
		FUniformBufferRHIParamRef PrimitiveUniformBuffer,
		const FMatrix& Instance0Transform
		)
	{
		FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();
		FMaterialShader::SetParameters(RHICmdList, ShaderRHI, MaterialProxy, *MaterialProxy->GetMaterial(View.GetFeatureLevel()), View, true, ESceneRenderTargetsMode::SetTextures);

		SetUniformBufferParameter(RHICmdList, ShaderRHI,GetUniformBufferParameter<FPrimitiveUniformShaderParameters>(),PrimitiveUniformBuffer);

		const FScene* Scene = (const FScene*)View.Family->Scene;
		SurfelBufferParameters.Set(RHICmdList, ShaderRHI, *Scene->DistanceFieldSceneData.SurfelBuffers, *Scene->DistanceFieldSceneData.InstancedSurfelBuffers);
		
		SetShaderValue(RHICmdList, ShaderRHI, SurfelStartIndex, SurfelStartIndexValue);
		SetShaderValue(RHICmdList, ShaderRHI, NumSurfelsToGenerate, NumSurfelsToGenerateValue);

		SetShaderValue(RHICmdList, ShaderRHI, Instance0InverseTransform, Instance0Transform.Inverse());
	}

	void UnsetParameters(FRHICommandList& RHICmdList)
	{
		FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();
		SurfelBufferParameters.UnsetParameters(RHICmdList, ShaderRHI);
	}

	virtual bool Serialize(FArchive& Ar) override
	{		
		bool bShaderHasOutdatedParameters = FMaterialShader::Serialize(Ar);
		Ar << SurfelBufferParameters;
		Ar << SurfelStartIndex;
		Ar << NumSurfelsToGenerate;
		Ar << Instance0InverseTransform;
		return bShaderHasOutdatedParameters;
	}

private:

	FSurfelBufferParameters SurfelBufferParameters;
	FShaderParameter SurfelStartIndex;
	FShaderParameter NumSurfelsToGenerate;
	FShaderParameter Instance0InverseTransform;
};

IMPLEMENT_MATERIAL_SHADER_TYPE(,FEvaluateSurfelMaterialCS,TEXT("EvaluateSurfelMaterial"),TEXT("EvaluateSurfelMaterialCS"),SF_Compute);

void FUniformMeshConverter::GenerateSurfels(
	FRHICommandListImmediate& RHICmdList, 
	FViewInfo& View, 
	const FPrimitiveSceneInfo* PrimitiveSceneInfo, 
	const FMaterialRenderProxy* MaterialProxy,
	FUniformBufferRHIParamRef PrimitiveUniformBuffer, 
	const FMatrix& Instance0Transform,
	int32 SurfelOffset,
	int32 NumSurfels)
{
	const FMaterial* Material = MaterialProxy->GetMaterial(View.GetFeatureLevel());
	const FMaterialShaderMap* MaterialShaderMap = Material->GetRenderingThreadShaderMap();
	FEvaluateSurfelMaterialCS* ComputeShader = MaterialShaderMap->GetShader<FEvaluateSurfelMaterialCS>();

	RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());
	ComputeShader->SetParameters(RHICmdList, View, SurfelOffset, NumSurfels, MaterialProxy, PrimitiveUniformBuffer, Instance0Transform);
	DispatchComputeShader(RHICmdList, ComputeShader, FMath::DivideAndRoundUp(NumSurfels, GEvaluateSurfelMaterialGroupSize), 1, 1);

	ComputeShader->UnsetParameters(RHICmdList);
}
