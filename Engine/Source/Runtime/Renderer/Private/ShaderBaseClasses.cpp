// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ShaderBaseClasses.cpp: Shader base classes
=============================================================================*/

#include "RendererPrivate.h"
#include "ScenePrivate.h"

/** If true, cached uniform expressions are allowed. */
int32 FMaterialShader::bAllowCachedUniformExpressions = true;

/** Console variable ref to toggle cached uniform expressions. */
FAutoConsoleVariableRef FMaterialShader::CVarAllowCachedUniformExpressions(
	TEXT("r.AllowCachedUniformExpressions"),
	bAllowCachedUniformExpressions,
	TEXT("Allow uniform expressions to be cached."),
	ECVF_RenderThreadSafe);

FMaterialShader::FMaterialShader(const FMaterialShaderType::CompiledShaderInitializerType& Initializer)
:	FShader(Initializer)
,	DebugUniformExpressionSet(Initializer.UniformExpressionSet)
,	DebugDescription(Initializer.DebugDescription)
{
	check(!DebugDescription.IsEmpty());

	// Bind the material uniform buffer parameter.
	MaterialUniformBuffer.Bind(Initializer.ParameterMap,TEXT("Material"));

	for (int32 CollectionIndex = 0; CollectionIndex < Initializer.UniformExpressionSet.ParameterCollections.Num(); CollectionIndex++)
	{
		FShaderUniformBufferParameter CollectionParameter;
		CollectionParameter.Bind(Initializer.ParameterMap,*FString::Printf(TEXT("MaterialCollection%u"), CollectionIndex));
		ParameterCollectionUniformBuffers.Add(CollectionParameter);
	}

	for (int32 Index = 0; Index < Initializer.UniformExpressionSet.PerFrameUniformScalarExpressions.Num(); Index++)
	{
		FShaderParameter Parameter;
		Parameter.Bind(Initializer.ParameterMap, *FString::Printf(TEXT("UE_Material_PerFrameScalarExpression%u"), Index));
		PerFrameScalarExpressions.Add(Parameter);
	}

	for (int32 Index = 0; Index < Initializer.UniformExpressionSet.PerFrameUniformVectorExpressions.Num(); Index++)
	{
		FShaderParameter Parameter;
		Parameter.Bind(Initializer.ParameterMap, *FString::Printf(TEXT("UE_Material_PerFrameVectorExpression%u"), Index));
		PerFrameVectorExpressions.Add(Parameter);
	}

	for (int32 Index = 0; Index < Initializer.UniformExpressionSet.PerFramePrevUniformScalarExpressions.Num(); Index++)
	{
		FShaderParameter Parameter;
		Parameter.Bind(Initializer.ParameterMap, *FString::Printf(TEXT("UE_Material_PerFramePrevScalarExpression%u"), Index));
		PerFramePrevScalarExpressions.Add(Parameter);
	}

	for (int32 Index = 0; Index < Initializer.UniformExpressionSet.PerFramePrevUniformVectorExpressions.Num(); Index++)
	{
		FShaderParameter Parameter;
		Parameter.Bind(Initializer.ParameterMap, *FString::Printf(TEXT("UE_Material_PerFramePrevVectorExpression%u"), Index));
		PerFramePrevVectorExpressions.Add(Parameter);
	}

	DeferredParameters.Bind(Initializer.ParameterMap);
	LightAttenuation.Bind(Initializer.ParameterMap, TEXT("LightAttenuationTexture"));
	LightAttenuationSampler.Bind(Initializer.ParameterMap, TEXT("LightAttenuationTextureSampler"));
	AtmosphericFogTextureParameters.Bind(Initializer.ParameterMap);
	EyeAdaptation.Bind(Initializer.ParameterMap, TEXT("EyeAdaptation"));
	// only used it Material has expression that requires PerlinNoiseGradientTexture
	PerlinNoiseGradientTexture.Bind(Initializer.ParameterMap,TEXT("PerlinNoiseGradientTexture"));
	PerlinNoiseGradientTextureSampler.Bind(Initializer.ParameterMap,TEXT("PerlinNoiseGradientTextureSampler"));
	// only used it Material has expression that requires PerlinNoise3DTexture
	PerlinNoise3DTexture.Bind(Initializer.ParameterMap,TEXT("PerlinNoise3DTexture"));
	PerlinNoise3DTextureSampler.Bind(Initializer.ParameterMap,TEXT("PerlinNoise3DTextureSampler"));
}

FUniformBufferRHIParamRef FMaterialShader::GetParameterCollectionBuffer(const FGuid& Id, const FSceneInterface* SceneInterface) const
{
	const FScene* Scene = (const FScene*)SceneInterface;
	return Scene ? Scene->GetParameterCollectionBuffer(Id) : FUniformBufferRHIParamRef();
}

template<typename ShaderRHIParamRef>
void FMaterialShader::SetParameters(
	FRHICommandList& RHICmdList,
	const ShaderRHIParamRef ShaderRHI, 
	const FMaterialRenderProxy* MaterialRenderProxy, 
	const FMaterial& Material,
	const FSceneView& View, 
	bool bDeferredPass, 
	ESceneRenderTargetsMode::Type TextureMode)
{
	ERHIFeatureLevel::Type FeatureLevel = View.GetFeatureLevel();
	FUniformExpressionCache TempUniformExpressionCache;
	const FUniformExpressionCache* UniformExpressionCache = &MaterialRenderProxy->UniformExpressionCache[FeatureLevel];

	SetParameters(RHICmdList, ShaderRHI, View);

	// If the material has cached uniform expressions for selection or hover
	// and that is being overridden by show flags in the editor, recache
	// expressions for this draw call.
	const bool bOverrideSelection =
		GIsEditor &&
		!View.Family->EngineShowFlags.Selection &&
		(MaterialRenderProxy->IsSelected() || MaterialRenderProxy->IsHovered());

	if (!bAllowCachedUniformExpressions || !UniformExpressionCache->bUpToDate || bOverrideSelection)
	{
		FMaterialRenderContext MaterialRenderContext(MaterialRenderProxy, Material, &View);
		MaterialRenderProxy->EvaluateUniformExpressions(TempUniformExpressionCache, MaterialRenderContext, &RHICmdList);
		UniformExpressionCache = &TempUniformExpressionCache;
	}

	check(Material.GetRenderingThreadShaderMap());
	check(Material.GetRenderingThreadShaderMap()->IsValidForRendering());
	check(Material.GetFeatureLevel() == FeatureLevel);

	// Validate that the shader is being used for a material that matches the uniform expression set the shader was compiled for.
	const FUniformExpressionSet& MaterialUniformExpressionSet = Material.GetRenderingThreadShaderMap()->GetUniformExpressionSet();

#if NO_LOGGING == 0
	
	const bool bUniformExpressionSetMismatch = !DebugUniformExpressionSet.Matches(MaterialUniformExpressionSet)
		|| UniformExpressionCache->CachedUniformExpressionShaderMap != Material.GetRenderingThreadShaderMap();

	if (bUniformExpressionSetMismatch)
	{
		UE_LOG(
			LogShaders,
			Fatal,
			TEXT("%s shader uniform expression set mismatch for material %s/%s.\n")
			TEXT("Shader compilation info:                %s\n")
			TEXT("Material render proxy compilation info: %s\n")
			TEXT("Shader uniform expression set:   %u vectors, %u scalars, %u 2D textures, %u cube textures, %u scalars/frame, %u vectors/frame, shader map %p\n")
			TEXT("Material uniform expression set: %u vectors, %u scalars, %u 2D textures, %u cube textures, %u scalars/frame, %u vectors/frame, shader map %p\n"),
			GetType()->GetName(),
			*MaterialRenderProxy->GetFriendlyName(),
			*Material.GetFriendlyName(),
			*DebugDescription,
			*Material.GetRenderingThreadShaderMap()->GetDebugDescription(),
			DebugUniformExpressionSet.NumVectorExpressions,
			DebugUniformExpressionSet.NumScalarExpressions,
			DebugUniformExpressionSet.Num2DTextureExpressions,
			DebugUniformExpressionSet.NumCubeTextureExpressions,
			DebugUniformExpressionSet.NumPerFrameScalarExpressions,
			DebugUniformExpressionSet.NumPerFrameVectorExpressions,
			UniformExpressionCache->CachedUniformExpressionShaderMap,
			MaterialUniformExpressionSet.UniformVectorExpressions.Num(),
			MaterialUniformExpressionSet.UniformScalarExpressions.Num(),
			MaterialUniformExpressionSet.Uniform2DTextureExpressions.Num(),
			MaterialUniformExpressionSet.UniformCubeTextureExpressions.Num(),
			MaterialUniformExpressionSet.PerFrameUniformScalarExpressions.Num(),
			MaterialUniformExpressionSet.PerFrameUniformVectorExpressions.Num(),
			Material.GetRenderingThreadShaderMap()
			);
	}
#endif

	if (UniformExpressionCache->LocalUniformBuffer.IsValid())
	{
		// Set the material uniform buffer.
		SetLocalUniformBufferParameter(RHICmdList, ShaderRHI, MaterialUniformBuffer, UniformExpressionCache->LocalUniformBuffer);
	}
	else
	{
		// Set the material uniform buffer.
		SetUniformBufferParameter(RHICmdList, ShaderRHI, MaterialUniformBuffer, UniformExpressionCache->UniformBuffer);
	}

	{
		const TArray<FGuid>& ParameterCollections = UniformExpressionCache->ParameterCollections;
		const int32 ParameterCollectionsNum = ParameterCollections.Num();

		check(ParameterCollectionUniformBuffers.Num() >= ParameterCollectionsNum);

		// Find each referenced parameter collection's uniform buffer in the scene and set the parameter
		for (int32 CollectionIndex = 0; CollectionIndex < ParameterCollectionsNum; CollectionIndex++)
		{
			FUniformBufferRHIParamRef UniformBuffer = GetParameterCollectionBuffer(ParameterCollections[CollectionIndex], View.Family->Scene);
			SetUniformBufferParameter(RHICmdList, ShaderRHI,ParameterCollectionUniformBuffers[CollectionIndex],UniformBuffer);
		}
	}

	{
		// Per frame material expressions
		const int32 NumScalarExpressions = PerFrameScalarExpressions.Num();
		const int32 NumVectorExpressions = PerFrameVectorExpressions.Num();

		if (NumScalarExpressions > 0 || NumVectorExpressions > 0)
		{
			FMaterialRenderContext MaterialRenderContext(MaterialRenderProxy, Material, &View);
			MaterialRenderContext.Time = View.Family->CurrentWorldTime;
			MaterialRenderContext.RealTime = View.Family->CurrentRealTime;
			for (int32 Index = 0; Index < NumScalarExpressions; ++Index)
			{
				auto& Parameter = PerFrameScalarExpressions[Index];
				if (Parameter.IsBound())
				{
					FLinearColor TempValue;
					MaterialUniformExpressionSet.PerFrameUniformScalarExpressions[Index]->GetNumberValue(MaterialRenderContext, TempValue);
					SetShaderValue(RHICmdList, ShaderRHI, Parameter, TempValue.R);
				}
			}

			for (int32 Index = 0; Index < NumVectorExpressions; ++Index)
			{
				auto& Parameter = PerFrameVectorExpressions[Index];
				if (Parameter.IsBound())
				{
					FLinearColor TempValue;
					MaterialUniformExpressionSet.PerFrameUniformVectorExpressions[Index]->GetNumberValue(MaterialRenderContext, TempValue);
					SetShaderValue(RHICmdList, ShaderRHI, Parameter, TempValue);
				}
			}

			// Now previous frame's expressions
			const int32 NumPrevScalarExpressions = PerFramePrevScalarExpressions.Num();
			const int32 NumPrevVectorExpressions = PerFramePrevVectorExpressions.Num();
			if (NumPrevScalarExpressions > 0 || NumPrevVectorExpressions > 0)
			{
				MaterialRenderContext.Time = View.Family->CurrentWorldTime - View.Family->DeltaWorldTime;
				MaterialRenderContext.RealTime = View.Family->CurrentRealTime - View.Family->DeltaWorldTime;

				for (int32 Index = 0; Index < NumPrevScalarExpressions; ++Index)
				{
					auto& Parameter = PerFramePrevScalarExpressions[Index];
					if (Parameter.IsBound())
					{
						FLinearColor TempValue;
						MaterialUniformExpressionSet.PerFramePrevUniformScalarExpressions[Index]->GetNumberValue(MaterialRenderContext, TempValue);
						SetShaderValue(RHICmdList, ShaderRHI, Parameter, TempValue.R);
					}
				}

				for (int32 Index = 0; Index < NumPrevVectorExpressions; ++Index)
				{
					auto& Parameter = PerFramePrevVectorExpressions[Index];
					if (Parameter.IsBound())
					{
						FLinearColor TempValue;
						MaterialUniformExpressionSet.PerFramePrevUniformVectorExpressions[Index]->GetNumberValue(MaterialRenderContext, TempValue);
						SetShaderValue(RHICmdList, ShaderRHI, Parameter, TempValue);
					}
				}
			}
		}
	}

	DeferredParameters.Set(RHICmdList, ShaderRHI, View, TextureMode);

	AtmosphericFogTextureParameters.Set(RHICmdList, ShaderRHI, View);

	if (FeatureLevel >= ERHIFeatureLevel::SM4)
	{
		// for copied scene color
		if(LightAttenuation.IsBound())
		{
			SetTextureParameter(
				RHICmdList,
				ShaderRHI,
				LightAttenuation,
				LightAttenuationSampler,
				TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(),
				GSceneRenderTargets.GetLightAttenuationTexture());
		}
	}

	//Use of the eye adaptation texture here is experimental and potentially dangerous as it can introduce a feedback loop. May be removed.
	if(EyeAdaptation.IsBound())
	{
		FTextureRHIRef& EyeAdaptationTex = GetEyeAdaptation(View);
		SetTextureParameter(RHICmdList, ShaderRHI, EyeAdaptation, EyeAdaptationTex);
	}

	if (PerlinNoiseGradientTexture.IsBound() && IsValidRef(GSystemTextures.PerlinNoiseGradient))
	{
		const FTexture2DRHIRef& Texture = (FTexture2DRHIRef&)GSystemTextures.PerlinNoiseGradient->GetRenderTargetItem().ShaderResourceTexture;
		// Bind the PerlinNoiseGradientTexture as a texture
		SetTextureParameter(
			RHICmdList,
			ShaderRHI,
			PerlinNoiseGradientTexture,
			PerlinNoiseGradientTextureSampler,
			TStaticSamplerState<SF_Point,AM_Wrap,AM_Wrap,AM_Wrap>::GetRHI(),
			Texture
			);
	}

	if (PerlinNoise3DTexture.IsBound() && IsValidRef(GSystemTextures.PerlinNoise3D))
	{
		const FTexture3DRHIRef& Texture = (FTexture3DRHIRef&)GSystemTextures.PerlinNoise3D->GetRenderTargetItem().ShaderResourceTexture;
		// Bind the PerlinNoise3DTexture as a texture
		SetTextureParameter(
			RHICmdList,
			ShaderRHI,
			PerlinNoise3DTexture,
			PerlinNoise3DTextureSampler,
			TStaticSamplerState<SF_Bilinear,AM_Wrap,AM_Wrap,AM_Wrap>::GetRHI(),
			Texture
			);
	}
}

// Doxygen struggles to parse these explicit specializations. Just ignore them for now.
#if !UE_BUILD_DOCS

#define IMPLEMENT_MATERIAL_SHADER_SetParameters( ShaderRHIParamRef ) \
	template RENDERER_API void FMaterialShader::SetParameters< ShaderRHIParamRef >( \
		FRHICommandList& RHICmdList,					\
		const ShaderRHIParamRef ShaderRHI,				\
		const FMaterialRenderProxy* MaterialRenderProxy,\
		const FMaterial& Material,						\
		const FSceneView& View,							\
		bool bDeferredPass,								\
		ESceneRenderTargetsMode::Type TextureMode		\
	);

IMPLEMENT_MATERIAL_SHADER_SetParameters( FVertexShaderRHIParamRef );
IMPLEMENT_MATERIAL_SHADER_SetParameters( FHullShaderRHIParamRef );
IMPLEMENT_MATERIAL_SHADER_SetParameters( FDomainShaderRHIParamRef );
IMPLEMENT_MATERIAL_SHADER_SetParameters( FGeometryShaderRHIParamRef );
IMPLEMENT_MATERIAL_SHADER_SetParameters( FPixelShaderRHIParamRef );
IMPLEMENT_MATERIAL_SHADER_SetParameters( FComputeShaderRHIParamRef );

#endif

bool FMaterialShader::Serialize(FArchive& Ar)
{
	const bool bShaderHasOutdatedParameters = FShader::Serialize(Ar);
	Ar << MaterialUniformBuffer;
	Ar << ParameterCollectionUniformBuffers;
	Ar << DeferredParameters;
	Ar << LightAttenuation;
	Ar << LightAttenuationSampler;
	if (Ar.UE4Ver() >= VER_UE4_SMALLER_DEBUG_MATERIALSHADER_UNIFORM_EXPRESSIONS)
	{
		Ar << DebugUniformExpressionSet;
		Ar << DebugDescription;
	}
	else if (Ar.IsLoading())
	{
		FUniformExpressionSet TempExpressionSet;
		TempExpressionSet.Serialize(Ar);
		DebugUniformExpressionSet.InitFromExpressionSet(TempExpressionSet);

		Ar << DebugDescription;
	}
	Ar << AtmosphericFogTextureParameters;
	Ar << EyeAdaptation;
	Ar << PerlinNoiseGradientTexture;
	Ar << PerlinNoiseGradientTextureSampler;
	Ar << PerlinNoise3DTexture;
	Ar << PerlinNoise3DTextureSampler;

	Ar << PerFrameScalarExpressions;
	Ar << PerFrameVectorExpressions;
	Ar << PerFramePrevScalarExpressions;
	Ar << PerFramePrevVectorExpressions;

	return bShaderHasOutdatedParameters;
}

FTextureRHIRef& FMaterialShader::GetEyeAdaptation(const FSceneView& View)
{
	IPooledRenderTarget* EyeAdaptationRT = NULL;
	if( View.bIsViewInfo )
	{
		const FViewInfo& ViewInfo = static_cast<const FViewInfo&>(View);
		EyeAdaptationRT = ViewInfo.GetEyeAdaptation();
	}

	if( EyeAdaptationRT )
	{
		return EyeAdaptationRT->GetRenderTargetItem().TargetableTexture;
	}
	
	return GWhiteTexture->TextureRHI;
}

uint32 FMaterialShader::GetAllocatedSize() const
{
	return FShader::GetAllocatedSize()
		+ ParameterCollectionUniformBuffers.GetAllocatedSize()
		+ DebugDescription.GetAllocatedSize();
}


template< typename ShaderRHIParamRef >
void FMeshMaterialShader::SetMesh(
	FRHICommandList& RHICmdList,
	const ShaderRHIParamRef ShaderRHI,
	const FVertexFactory* VertexFactory,
	const FSceneView& View,
	const FPrimitiveSceneProxy* Proxy,
	const FMeshBatchElement& BatchElement,
	uint32 DataFlags )
{
	// Set the mesh for the vertex factory
	VertexFactoryParameters.SetMesh(RHICmdList, this,VertexFactory,View,BatchElement, DataFlags);
		
	if(IsValidRef(BatchElement.PrimitiveUniformBuffer))
	{
		SetUniformBufferParameter(RHICmdList, ShaderRHI,GetUniformBufferParameter<FPrimitiveUniformShaderParameters>(),BatchElement.PrimitiveUniformBuffer);
	}
	else
	{
		check(BatchElement.PrimitiveUniformBufferResource);
		SetUniformBufferParameter(RHICmdList, ShaderRHI,GetUniformBufferParameter<FPrimitiveUniformShaderParameters>(),*BatchElement.PrimitiveUniformBufferResource);
	}

	TShaderUniformBufferParameter<FDistanceCullFadeUniformShaderParameters> LODParameter = GetUniformBufferParameter<FDistanceCullFadeUniformShaderParameters>();
	if( LODParameter.IsBound() )
	{
		SetUniformBufferParameter(RHICmdList, ShaderRHI,LODParameter,GetPrimitiveFadeUniformBufferParameter(View, Proxy));
	}
}

#define IMPLEMENT_MESH_MATERIAL_SHADER_SetMesh( ShaderRHIParamRef ) \
	template RENDERER_API void FMeshMaterialShader::SetMesh< ShaderRHIParamRef >( \
		FRHICommandList& RHICmdList,			\
		const ShaderRHIParamRef ShaderRHI,		\
		const FVertexFactory* VertexFactory,	\
		const FSceneView& View,					\
		const FPrimitiveSceneProxy* Proxy,		\
		const FMeshBatchElement& BatchElement,	\
		uint32 DataFlags						\
	);

IMPLEMENT_MESH_MATERIAL_SHADER_SetMesh( FVertexShaderRHIParamRef );
IMPLEMENT_MESH_MATERIAL_SHADER_SetMesh( FHullShaderRHIParamRef );
IMPLEMENT_MESH_MATERIAL_SHADER_SetMesh( FDomainShaderRHIParamRef );
IMPLEMENT_MESH_MATERIAL_SHADER_SetMesh( FGeometryShaderRHIParamRef );
IMPLEMENT_MESH_MATERIAL_SHADER_SetMesh( FPixelShaderRHIParamRef );
IMPLEMENT_MESH_MATERIAL_SHADER_SetMesh( FComputeShaderRHIParamRef );

bool FMeshMaterialShader::Serialize(FArchive& Ar)
{
	bool bShaderHasOutdatedParameters = FMaterialShader::Serialize(Ar);
	bShaderHasOutdatedParameters |= Ar << VertexFactoryParameters;
	return bShaderHasOutdatedParameters;
}

uint32 FMeshMaterialShader::GetAllocatedSize() const
{
	return FMaterialShader::GetAllocatedSize()
		+ VertexFactoryParameters.GetAllocatedSize();
}

FUniformBufferRHIParamRef FMeshMaterialShader::GetPrimitiveFadeUniformBufferParameter(const FSceneView& View, const FPrimitiveSceneProxy* Proxy)
{
	FUniformBufferRHIParamRef FadeUniformBuffer = NULL;
	if( Proxy != NULL )
	{
		const FPrimitiveSceneInfo* PrimitiveSceneInfo = Proxy->GetPrimitiveSceneInfo();
		int32 PrimitiveIndex = PrimitiveSceneInfo->GetIndex();

		// This cast should always be safe. Check it :)
		checkSlow(View.bIsViewInfo);
		const FViewInfo& ViewInfo = (const FViewInfo&)View;
		FadeUniformBuffer = ViewInfo.PrimitiveFadeUniformBuffers[PrimitiveIndex];
	}
	if (FadeUniformBuffer == NULL)
	{
		FadeUniformBuffer = GDistanceCullFadedInUniformBuffer.GetUniformBufferRHI();
	}
	return FadeUniformBuffer;
}
