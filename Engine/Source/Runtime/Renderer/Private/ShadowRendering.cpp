// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ShadowRendering.cpp: Shadow rendering implementation
=============================================================================*/

#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "TextureLayout.h"
#include "LightPropagationVolume.h"
#include "SceneUtils.h"

static TAutoConsoleVariable<float> CVarCSMShadowDepthBias(
	TEXT("r.Shadow.CSMDepthBias"),
	20.0f,
	TEXT("Constant depth bias used by CSM"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<float> CVarCSMSplitPenumbraScale(
	TEXT("r.Shadow.CSMSplitPenumbraScale"),
	0.5f,
	TEXT("Scale applied to the penumbra size of Cascaded Shadow Map splits, useful for minimizing the transition between splits"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarCSMDepthBoundsTest(
	TEXT("r.Shadow.CSMDepthBoundsTest"),
	1,
	TEXT("Whether to use depth bounds tests rather than stencil tests for the CSM bounds"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<float> CVarSpotLightShadowTransitionScale(
	TEXT("r.Shadow.SpotLightTransitionScale"),
	60.0f,
	TEXT("Transition scale for spotlights"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<float> CVarShadowTransitionScale(
	TEXT("r.Shadow.TransitionScale"),
	60.0f,
	TEXT("This controls the 'fade in' region between a caster and where his shadow shows up.  Larger values make a smaller region which will have more self shadowing artifacts"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<float> CVarPointLightShadowDepthBias(
	TEXT("r.Shadow.PointLightDepthBias"),
	0.05f,
	TEXT("Depth bias that is applied in the depth pass for shadows from point lights. (0.03 avoids peter paning but has some shadow acne)"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<float> CVarSpotLightShadowDepthBias(
	TEXT("r.Shadow.SpotLightDepthBias"),
	5.0f,
	TEXT("Depth bias that is applied in the depth pass for per object projected shadows from spot lights"),
	ECVF_RenderThreadSafe);

// 0:off, 1:low, 2:med, 3:high, 4:very high, 5:max
uint32 GetShadowQuality()
{
	static const auto ICVarQuality = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.ShadowQuality"));

	int Ret = ICVarQuality->GetValueOnRenderThread();

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	static const auto ICVarLimit = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.LimitRenderingFeatures"));
	if(ICVarLimit)
	{
		int32 Limit = ICVarLimit->GetValueOnRenderThread();

		if(Limit > 2)
		{
			Ret = 0;
		}
	}
#endif

	return FMath::Clamp(Ret, 0, 5);
}

/**
 * A vertex shader for rendering the depth of a mesh.
 */
class FShadowDepthVS : public FMeshMaterialShader
{
	DECLARE_SHADER_TYPE(FShadowDepthVS,MeshMaterial);
public:

	static bool ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		return false;
	}

	FShadowDepthVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FMeshMaterialShader(Initializer)
	{
		ShadowParameters.Bind(Initializer.ParameterMap);
	}

	FShadowDepthVS() {}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FMeshMaterialShader::Serialize(Ar);
		Ar << ShadowParameters;
		return bShaderHasOutdatedParameters;
	}

	void SetParameters(
		FRHICommandList& RHICmdList, 
		const FMaterialRenderProxy* MaterialRenderProxy,
		const FMaterial& Material,
		const FSceneView& View,
		const FProjectedShadowInfo* ShadowInfo
		)
	{
		FMeshMaterialShader::SetParameters(RHICmdList, GetVertexShader(),MaterialRenderProxy,Material,View,ESceneRenderTargetsMode::DontSet);
		ShadowParameters.SetVertexShader(RHICmdList, this, View, ShadowInfo, MaterialRenderProxy);
	}

	void SetMesh(FRHICommandList& RHICmdList, const FVertexFactory* VertexFactory,const FSceneView& View,const FPrimitiveSceneProxy* Proxy,const FMeshBatchElement& BatchElement)
	{
		FMeshMaterialShader::SetMesh(RHICmdList, GetVertexShader(),VertexFactory,View,Proxy,BatchElement);
	}

private:
	FShadowDepthShaderParameters ShadowParameters;
};

enum EShadowDepthVertexShaderMode
{
	VertexShadowDepth_PerspectiveCorrect,
	VertexShadowDepth_OutputDepth,
	VertexShadowDepth_OnePassPointLight
};

/**
 * A vertex shader for rendering the depth of a mesh.
 */
template <EShadowDepthVertexShaderMode ShaderMode, bool bRenderReflectiveShadowMap, bool bUsePositionOnlyStream, bool bIsForGeometryShader=false>
class TShadowDepthVS : public FShadowDepthVS
{
	DECLARE_SHADER_TYPE(TShadowDepthVS,MeshMaterial);
public:

	TShadowDepthVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FShadowDepthVS(Initializer)
	{
	}

	TShadowDepthVS() {}

	static bool ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		if (bIsForGeometryShader && !RHISupportsGeometryShaders(Platform))
		{
			return false;
		}

		//Note: This logic needs to stay in sync with OverrideWithDefaultMaterialForShadowDepth!
		// Compile for special engine materials.
		if(bRenderReflectiveShadowMap)
		{
			// Reflective shadow map shaders must be compiled for every material because they access the material normal
			return !bUsePositionOnlyStream
				// Don't render ShadowDepth for translucent unlit materials, unless we're injecting emissive
				&& ((!IsTranslucentBlendMode(Material->GetBlendMode()) && Material->GetShadingModel() != MSM_Unlit) || Material->ShouldInjectEmissiveIntoLPV() )
				&& IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5);
		}
		else
		{
			return (Material->IsSpecialEngineMaterial()
						// Masked and WPO materials need their shaders but cannot be used with a position only stream.
						|| ((Material->IsMasked() || Material->MaterialMayModifyMeshPosition()) && !bUsePositionOnlyStream))
					// Only compile one pass point light shaders for feature levels >= SM4
					&& (ShaderMode != VertexShadowDepth_OnePassPointLight || IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4))
					// Only compile position-only shaders for vertex factories that support it.
					&& (!bUsePositionOnlyStream || VertexFactoryType->SupportsPositionOnly())
					// Don't render ShadowDepth for translucent unlit materials
					&& (!IsTranslucentBlendMode(Material->GetBlendMode()) && Material->GetShadingModel() != MSM_Unlit)
					// Only compile perspective correct light shaders for feature levels >= SM4
					&& (ShaderMode != VertexShadowDepth_PerspectiveCorrect || IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4));
		}
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		FShadowDepthVS::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("PERSPECTIVE_CORRECT_DEPTH"), (uint32)(ShaderMode == VertexShadowDepth_PerspectiveCorrect));
		OutEnvironment.SetDefine(TEXT("ONEPASS_POINTLIGHT_SHADOW"), (uint32)(ShaderMode == VertexShadowDepth_OnePassPointLight));
		OutEnvironment.SetDefine(TEXT("REFLECTIVE_SHADOW_MAP"), (uint32)bRenderReflectiveShadowMap);
		OutEnvironment.SetDefine(TEXT("POSITION_ONLY"), (uint32)bUsePositionOnlyStream);
	}
};


/**
 * A Hull shader for rendering the depth of a mesh.
 */
template <EShadowDepthVertexShaderMode ShaderMode, bool bRenderReflectiveShadowMap> 
class TShadowDepthHS : public FBaseHS
{
	DECLARE_SHADER_TYPE(TShadowDepthHS,MeshMaterial);
public:

	
	TShadowDepthHS(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FBaseHS(Initializer)
	{}

	TShadowDepthHS() {}

	static bool ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		// Re-use ShouldCache from vertex shader
		return FBaseHS::ShouldCache(Platform, Material, VertexFactoryType)
			&& TShadowDepthVS<ShaderMode, bRenderReflectiveShadowMap, false>::ShouldCache(Platform, Material, VertexFactoryType);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		// Re-use compilation env from vertex shader

		TShadowDepthVS<ShaderMode, bRenderReflectiveShadowMap, false>::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
	}
};

/**
 * A domain shader for rendering the depth of a mesh.
 */
class FShadowDepthDS : public FBaseDS
{
	DECLARE_SHADER_TYPE(FShadowDepthDS,MeshMaterial);
public:

	FShadowDepthDS(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FBaseDS(Initializer)
	{
		ShadowParameters.Bind(Initializer.ParameterMap);
	}

	FShadowDepthDS() {}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FBaseDS::Serialize(Ar);
		Ar << ShadowParameters;
		return bShaderHasOutdatedParameters;
	}

	void SetParameters(
		FRHICommandList& RHICmdList, 
		const FMaterialRenderProxy* MaterialRenderProxy,
		const FSceneView& View,
		const FProjectedShadowInfo* ShadowInfo
		)
	{
		FBaseDS::SetParameters(RHICmdList, MaterialRenderProxy, View);
		ShadowParameters.SetDomainShader(RHICmdList, this, View, ShadowInfo, MaterialRenderProxy);
	}

private:
	FShadowDepthShaderParameters ShadowParameters;
};

/**
 * A Domain shader for rendering the depth of a mesh.
 */
template <EShadowDepthVertexShaderMode ShaderMode, bool bRenderReflectiveShadowMap> 
class TShadowDepthDS : public FShadowDepthDS
{
	DECLARE_SHADER_TYPE(TShadowDepthDS,MeshMaterial);
public:

	TShadowDepthDS(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FShadowDepthDS(Initializer)
	{}

	TShadowDepthDS() {}

	static bool ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		// Re-use ShouldCache from vertex shader
		return FBaseDS::ShouldCache(Platform, Material, VertexFactoryType)
			&& TShadowDepthVS<ShaderMode, bRenderReflectiveShadowMap, false>::ShouldCache(Platform, Material, VertexFactoryType);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		// Re-use compilation env from vertex shader
		TShadowDepthVS<ShaderMode, bRenderReflectiveShadowMap, false>::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
	}
};

/** Geometry shader that allows one pass point light shadows by cloning triangles to all faces of the cube map. */
class FOnePassPointShadowProjectionGS : public FMeshMaterialShader
{
	DECLARE_SHADER_TYPE(FOnePassPointShadowProjectionGS,MeshMaterial);
public:

	static bool ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		return TShadowDepthVS<VertexShadowDepth_OnePassPointLight, false, false, true>::ShouldCache(Platform, Material, VertexFactoryType);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		FMeshMaterialShader::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
		TShadowDepthVS<VertexShadowDepth_OnePassPointLight, false, false, true>::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
	}

	FOnePassPointShadowProjectionGS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FMeshMaterialShader(Initializer)
	{
		ShadowViewProjectionMatrices.Bind(Initializer.ParameterMap, TEXT("ShadowViewProjectionMatrices"));
		MeshVisibleToFace.Bind(Initializer.ParameterMap, TEXT("MeshVisibleToFace"));
	}

	FOnePassPointShadowProjectionGS() {}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FMeshMaterialShader::Serialize(Ar);
		Ar << ShadowViewProjectionMatrices;
		Ar << MeshVisibleToFace;
		return bShaderHasOutdatedParameters;
	}

	void SetParameters(
		FRHICommandList& RHICmdList, 
		const FSceneView& View,
		const FProjectedShadowInfo* ShadowInfo
		)
	{
		FMaterialShader::SetParameters(RHICmdList, GetGeometryShader(),View);

		const FMatrix Translation = FTranslationMatrix(-View.ViewMatrices.PreViewTranslation);

		FMatrix TranslatedShadowViewProjectionMatrices[6];
		for (int32 FaceIndex = 0; FaceIndex < 6; FaceIndex++)
		{
			// Have to apply the pre-view translation to the view - projection matrices
			TranslatedShadowViewProjectionMatrices[FaceIndex] = Translation * ShadowInfo->OnePassShadowViewProjectionMatrices[FaceIndex];
		}

		// Set the view projection matrices that will transform positions from world to cube map face space
		SetShaderValueArray<FGeometryShaderRHIParamRef, FMatrix>(
			RHICmdList, 
			GetGeometryShader(),
			ShadowViewProjectionMatrices,
			TranslatedShadowViewProjectionMatrices,
			ARRAY_COUNT(TranslatedShadowViewProjectionMatrices)
			);
	}

	void SetMesh(FRHICommandList& RHICmdList, const FPrimitiveSceneProxy* PrimitiveSceneProxy, const FProjectedShadowInfo* ShadowInfo, const FSceneView& View)
	{
		if (MeshVisibleToFace.IsBound())
		{
			const FBoxSphereBounds& PrimitiveBounds = PrimitiveSceneProxy->GetBounds();

			FVector4 MeshVisibleToFaceValue[6];
			for (int32 FaceIndex = 0; FaceIndex < 6; FaceIndex++)
			{
				MeshVisibleToFaceValue[FaceIndex] = FVector4(ShadowInfo->OnePassShadowFrustums[FaceIndex].IntersectBox(PrimitiveBounds.Origin,PrimitiveBounds.BoxExtent), 0, 0, 0);
			}

			// Set the view projection matrices that will transform positions from world to cube map face space
			SetShaderValueArray<FGeometryShaderRHIParamRef, FVector4>(
				RHICmdList, 
				GetGeometryShader(),
				MeshVisibleToFace,
				MeshVisibleToFaceValue,
				ARRAY_COUNT(MeshVisibleToFaceValue)
				);
		}
	}

private:
	FShaderParameter ShadowViewProjectionMatrices;
	FShaderParameter MeshVisibleToFace;
};

#define IMPLEMENT_SHADOW_DEPTH_SHADERMODE_SHADERS(ShaderMode,bRenderReflectiveShadowMap) \
	typedef TShadowDepthVS<ShaderMode, bRenderReflectiveShadowMap, false> TShadowDepthVS##ShaderMode##bRenderReflectiveShadowMap;	\
	IMPLEMENT_MATERIAL_SHADER_TYPE(template<>,TShadowDepthVS##ShaderMode##bRenderReflectiveShadowMap,TEXT("ShadowDepthVertexShader"),TEXT("Main"),SF_Vertex);	\
	typedef TShadowDepthVS<ShaderMode, bRenderReflectiveShadowMap, false, true> TShadowDepthVSForGS##ShaderMode##bRenderReflectiveShadowMap;	\
	IMPLEMENT_MATERIAL_SHADER_TYPE(template<>,TShadowDepthVSForGS##ShaderMode##bRenderReflectiveShadowMap,TEXT("ShadowDepthVertexShader"),TEXT("MainForGS"),SF_Vertex);	\
	typedef TShadowDepthHS<ShaderMode, bRenderReflectiveShadowMap> TShadowDepthHS##ShaderMode##bRenderReflectiveShadowMap;	\
	IMPLEMENT_MATERIAL_SHADER_TYPE(template<>,TShadowDepthHS##ShaderMode##bRenderReflectiveShadowMap,TEXT("ShadowDepthVertexShader"),TEXT("MainHull"),SF_Hull);	\
	typedef TShadowDepthDS<ShaderMode, bRenderReflectiveShadowMap> TShadowDepthDS##ShaderMode##bRenderReflectiveShadowMap;	\
	IMPLEMENT_MATERIAL_SHADER_TYPE(template<>,TShadowDepthDS##ShaderMode##bRenderReflectiveShadowMap,TEXT("ShadowDepthVertexShader"),TEXT("MainDomain"),SF_Domain);

IMPLEMENT_SHADER_TYPE(,FOnePassPointShadowProjectionGS,TEXT("ShadowDepthVertexShader"),TEXT("MainOnePassPointLightGS"),SF_Geometry);

IMPLEMENT_SHADOW_DEPTH_SHADERMODE_SHADERS(VertexShadowDepth_PerspectiveCorrect, true); 
IMPLEMENT_SHADOW_DEPTH_SHADERMODE_SHADERS(VertexShadowDepth_PerspectiveCorrect, false); 
IMPLEMENT_SHADOW_DEPTH_SHADERMODE_SHADERS(VertexShadowDepth_OutputDepth, true); 
IMPLEMENT_SHADOW_DEPTH_SHADERMODE_SHADERS(VertexShadowDepth_OutputDepth, false); 
IMPLEMENT_SHADOW_DEPTH_SHADERMODE_SHADERS(VertexShadowDepth_OnePassPointLight, false);

// Position only vertex shaders.
typedef TShadowDepthVS<VertexShadowDepth_PerspectiveCorrect, false, true> TShadowDepthVSVertexShadowDepth_PerspectiveCorrectPositionOnly;
IMPLEMENT_MATERIAL_SHADER_TYPE(template<>,TShadowDepthVSVertexShadowDepth_PerspectiveCorrectPositionOnly,TEXT("ShadowDepthVertexShader"),TEXT("PositionOnlyMain"),SF_Vertex);
typedef TShadowDepthVS<VertexShadowDepth_OutputDepth, false, true> TShadowDepthVSVertexShadowDepth_OutputDepthPositionOnly;
IMPLEMENT_MATERIAL_SHADER_TYPE(template<>,TShadowDepthVSVertexShadowDepth_OutputDepthPositionOnly,TEXT("ShadowDepthVertexShader"),TEXT("PositionOnlyMain"),SF_Vertex);
typedef TShadowDepthVS<VertexShadowDepth_OnePassPointLight, false, true> TShadowDepthVSVertexShadowDepth_OnePassPointLightPositionOnly;
IMPLEMENT_MATERIAL_SHADER_TYPE(template<>,TShadowDepthVSVertexShadowDepth_OnePassPointLightPositionOnly,TEXT("ShadowDepthVertexShader"),TEXT("PositionOnlyMain"),SF_Vertex);
typedef TShadowDepthVS<VertexShadowDepth_OnePassPointLight, false, true, true> TShadowDepthVSForGSVertexShadowDepth_OnePassPointLightPositionOnly;
IMPLEMENT_MATERIAL_SHADER_TYPE(template<>,TShadowDepthVSForGSVertexShadowDepth_OnePassPointLightPositionOnly,TEXT("ShadowDepthVertexShader"),TEXT("PositionOnlyMainForGS"),SF_Vertex);

/**
 * A pixel shader for rendering the depth of a mesh.
 */
template <bool bRenderReflectiveShadowMap>
class TShadowDepthBasePS : public FMeshMaterialShader
{
	DECLARE_SHADER_TYPE(TShadowDepthBasePS,MeshMaterial);
public:

	TShadowDepthBasePS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FMeshMaterialShader(Initializer)
	{
		ShadowParams.Bind(Initializer.ParameterMap,TEXT("ShadowParams"));
		TransmissionStrength.Bind(Initializer.ParameterMap,TEXT("TransmissionStrength"));
		ReflectiveShadowMapTextureResolution.Bind(Initializer.ParameterMap,TEXT("ReflectiveShadowMapTextureResolution"));
		ProjectionMatrixParameter.Bind(Initializer.ParameterMap,TEXT("ProjectionMatrix"));
		GvListBuffer.Bind(Initializer.ParameterMap,TEXT("GvListBuffer"));
		GvListHeadBuffer.Bind(Initializer.ParameterMap,TEXT("GvListHeadBuffer"));
		VplListBuffer.Bind(Initializer.ParameterMap,TEXT("VplListBuffer"));
		VplListHeadBuffer.Bind(Initializer.ParameterMap,TEXT("VplListHeadBuffer"));
	}

	TShadowDepthBasePS() {}

	void SetParameters(
		FRHICommandList& RHICmdList, 
		const FMaterialRenderProxy* MaterialRenderProxy,
		const FMaterial& Material,
		const FSceneView& View,
		const FProjectedShadowInfo* ShadowInfo
		)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		FMeshMaterialShader::SetParameters(RHICmdList, ShaderRHI, MaterialRenderProxy, Material, View, ESceneRenderTargetsMode::DontSet);

		SetShaderValue(RHICmdList, ShaderRHI, ShadowParams, FVector2D(ShadowInfo->GetShaderDepthBias(), ShadowInfo->InvMaxSubjectDepth));

		if(bRenderReflectiveShadowMap)
		{
			// LPV also propagates light transmission (for transmissive materials)
			SetShaderValue(RHICmdList, ShaderRHI,TransmissionStrength,View.FinalPostProcessSettings.LPVTransmissionIntensity);
			SetShaderValue(RHICmdList, ShaderRHI,ReflectiveShadowMapTextureResolution,FVector2D(GSceneRenderTargets.GetReflectiveShadowMapTextureResolution()));
			SetShaderValue(
				RHICmdList, 
				ShaderRHI,
				ProjectionMatrixParameter,
				FTranslationMatrix(ShadowInfo->PreShadowTranslation - View.ViewMatrices.PreViewTranslation) * ShadowInfo->SubjectAndReceiverMatrix
				);

			const FSceneViewState* ViewState = (const FSceneViewState*)View.State;
			if(ViewState)
			{
				const FLightPropagationVolume* Lpv = ViewState->GetLightPropagationVolume();

				if(Lpv)
				{
					SetUniformBufferParameter(RHICmdList, ShaderRHI, GetUniformBufferParameter<FLpvWriteUniformBufferParameters>(), Lpv->GetWriteUniformBuffer());
				}
			}
		}
	}

	void SetMesh(FRHICommandList& RHICmdList, const FVertexFactory* VertexFactory,const FSceneView& View,const FPrimitiveSceneProxy* Proxy,const FMeshBatchElement& BatchElement)
	{
		FMeshMaterialShader::SetMesh(RHICmdList, GetPixelShader(),VertexFactory,View,Proxy,BatchElement);
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FMeshMaterialShader::Serialize(Ar);

		Ar << ShadowParams;

		Ar << TransmissionStrength;
		Ar << ReflectiveShadowMapTextureResolution;
		Ar << ProjectionMatrixParameter;
		Ar << GvListBuffer;
		Ar << GvListHeadBuffer;
		Ar << VplListBuffer;
		Ar << VplListHeadBuffer;

		return bShaderHasOutdatedParameters;
	}

private:

	FShaderParameter ShadowParams;

	FShaderParameter TransmissionStrength;
	FShaderParameter ReflectiveShadowMapTextureResolution;
	FShaderParameter ProjectionMatrixParameter;

	FRWShaderParameter GvListBuffer;
	FRWShaderParameter GvListHeadBuffer;
	FRWShaderParameter VplListBuffer;
	FRWShaderParameter VplListHeadBuffer;
};

enum EShadowDepthPixelShaderMode
{
	PixelShadowDepth_NonPerspectiveCorrect,
	PixelShadowDepth_PerspectiveCorrect,
	PixelShadowDepth_OnePassPointLight
};

template <EShadowDepthPixelShaderMode ShaderMode, bool bRenderReflectiveShadowMap>
class TShadowDepthPS : public TShadowDepthBasePS<bRenderReflectiveShadowMap>
{
	DECLARE_SHADER_TYPE(TShadowDepthPS, MeshMaterial);
public:

	static bool ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		if (!IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4))
		{
			return (Material->IsSpecialEngineMaterial()
				// Only compile for masked or lit translucent materials
				|| Material->IsMasked())
				&& ShaderMode == PixelShadowDepth_NonPerspectiveCorrect
				// Don't render ShadowDepth for translucent unlit materials
				&& (!IsTranslucentBlendMode(Material->GetBlendMode()) && Material->GetShadingModel() != MSM_Unlit)
				&& !bRenderReflectiveShadowMap;
		}

		if ( bRenderReflectiveShadowMap )
		{
			//Note: This logic needs to stay in sync with OverrideWithDefaultMaterialForShadowDepth!
			// Reflective shadow map shaders must be compiled for every material because they access the material normal
			return 
				// Only compile one pass point light shaders for feature levels >= SM4
				( (!IsTranslucentBlendMode(Material->GetBlendMode()) && Material->GetShadingModel() != MSM_Unlit) || Material->ShouldInjectEmissiveIntoLPV() )
				&& IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5);
		}
		else
		{
			//Note: This logic needs to stay in sync with OverrideWithDefaultMaterialForShadowDepth!
			return (Material->IsSpecialEngineMaterial()
					// Only compile for masked or lit translucent materials
					|| Material->IsMasked()
					|| (Material->MaterialMayModifyMeshPosition() && Material->IsUsedWithInstancedStaticMeshes())
					// Perspective correct rendering needs a pixel shader and WPO materials can't be overridden with default material.
					|| (ShaderMode == PixelShadowDepth_PerspectiveCorrect && Material->MaterialMayModifyMeshPosition()))
				// Only compile one pass point light shaders for feature levels >= SM4
				&& (ShaderMode != PixelShadowDepth_OnePassPointLight || IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4))
				// Don't render ShadowDepth for translucent unlit materials
				&& (!IsTranslucentBlendMode(Material->GetBlendMode()) && Material->GetShadingModel() != MSM_Unlit)
				&& IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4);
		}
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		TShadowDepthBasePS<bRenderReflectiveShadowMap>::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("PERSPECTIVE_CORRECT_DEPTH"), (uint32)(ShaderMode == PixelShadowDepth_PerspectiveCorrect));
		OutEnvironment.SetDefine(TEXT("ONEPASS_POINTLIGHT_SHADOW"), (uint32)(ShaderMode == PixelShadowDepth_OnePassPointLight));
		OutEnvironment.SetDefine(TEXT("REFLECTIVE_SHADOW_MAP"), (uint32)bRenderReflectiveShadowMap);
	}

	TShadowDepthPS()
	{
	}

	TShadowDepthPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: TShadowDepthBasePS<bRenderReflectiveShadowMap>(Initializer)
	{
	}
};

// typedef required to get around macro expansion failure due to commas in template argument list for TShadowDepthPixelShader
#define IMPLEMENT_SHADOWDEPTHPASS_PIXELSHADER_TYPE(ShaderMode, bRenderReflectiveShadowMap) \
	typedef TShadowDepthPS<ShaderMode, bRenderReflectiveShadowMap> TShadowDepthPS##ShaderMode##bRenderReflectiveShadowMap; \
	IMPLEMENT_MATERIAL_SHADER_TYPE(template<>,TShadowDepthPS##ShaderMode##bRenderReflectiveShadowMap,TEXT("ShadowDepthPixelShader"),TEXT("Main"),SF_Pixel);

IMPLEMENT_SHADOWDEPTHPASS_PIXELSHADER_TYPE(PixelShadowDepth_NonPerspectiveCorrect, true);
IMPLEMENT_SHADOWDEPTHPASS_PIXELSHADER_TYPE(PixelShadowDepth_NonPerspectiveCorrect, false);
IMPLEMENT_SHADOWDEPTHPASS_PIXELSHADER_TYPE(PixelShadowDepth_PerspectiveCorrect, true);
IMPLEMENT_SHADOWDEPTHPASS_PIXELSHADER_TYPE(PixelShadowDepth_PerspectiveCorrect, false);
IMPLEMENT_SHADOWDEPTHPASS_PIXELSHADER_TYPE(PixelShadowDepth_OnePassPointLight, true);
IMPLEMENT_SHADOWDEPTHPASS_PIXELSHADER_TYPE(PixelShadowDepth_OnePassPointLight, false);

/** The stencil sphere vertex buffer. */
TGlobalResource<StencilingGeometry::TStencilSphereVertexBuffer<18, 12> > StencilingGeometry::GStencilSphereVertexBuffer;

/** The stencil sphere index buffer. */
TGlobalResource<StencilingGeometry::TStencilSphereIndexBuffer<18, 12> > StencilingGeometry::GStencilSphereIndexBuffer;

TGlobalResource<StencilingGeometry::TStencilSphereVertexBuffer<4, 4> > StencilingGeometry::GLowPolyStencilSphereVertexBuffer;
TGlobalResource<StencilingGeometry::TStencilSphereIndexBuffer<4, 4> > StencilingGeometry::GLowPolyStencilSphereIndexBuffer;

/** The (dummy) stencil cone vertex buffer. */
TGlobalResource<StencilingGeometry::FStencilConeVertexBuffer> StencilingGeometry::GStencilConeVertexBuffer;

/** The stencil cone index buffer. */
TGlobalResource<StencilingGeometry::FStencilConeIndexBuffer> StencilingGeometry::GStencilConeIndexBuffer;

/*-----------------------------------------------------------------------------
	FShadowProjectionVS
-----------------------------------------------------------------------------*/

bool FShadowProjectionVS::ShouldCache(EShaderPlatform Platform)
{
	return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4);
}

void FShadowProjectionVS::SetParameters(FRHICommandList& RHICmdList, const FSceneView& View, const FProjectedShadowInfo* ShadowInfo)
{
	FGlobalShader::SetParameters(RHICmdList, GetVertexShader(),View);
	
	if(ShadowInfo->IsWholeSceneDirectionalShadow())
	{
		// Calculate bounding geometry transform for whole scene directional shadow.
		// Use a pair of pre-transformed planes for stenciling.
		StencilingGeometryParameters.Set(RHICmdList, this, FVector4(0,0,0,1));
	}
	else if(ShadowInfo->IsWholeScenePointLightShadow())
	{
		// Handle stenciling sphere for point light.
		StencilingGeometryParameters.Set(RHICmdList, this, View, ShadowInfo->LightSceneInfo);
	}
	else
	{
		// Other bounding geometry types are pre-transformed.
		StencilingGeometryParameters.Set(RHICmdList, this, FVector4(0,0,0,1));
	}
}

IMPLEMENT_SHADER_TYPE(,FShadowProjectionNoTransformVS,TEXT("ShadowProjectionVertexShader"),TEXT("Main"),SF_Vertex);

IMPLEMENT_SHADER_TYPE(,FShadowProjectionVS,TEXT("ShadowProjectionVertexShader"),TEXT("Main"),SF_Vertex);

/**
 * Implementations for TShadowProjectionPS.  
 */
#if !UE_BUILD_DOCS
#define IMPLEMENT_SHADOW_PROJECTION_PIXEL_SHADER(Quality,UseFadePlane) \
	typedef TShadowProjectionPS<Quality, UseFadePlane> FShadowProjectionPS##Quality##UseFadePlane; \
	IMPLEMENT_SHADER_TYPE(template<>,FShadowProjectionPS##Quality##UseFadePlane,TEXT("ShadowProjectionPixelShader"),TEXT("Main"),SF_Pixel);

// Projection shaders without the distance fade, with different quality levels.
IMPLEMENT_SHADOW_PROJECTION_PIXEL_SHADER(1,false);
IMPLEMENT_SHADOW_PROJECTION_PIXEL_SHADER(2,false);
IMPLEMENT_SHADOW_PROJECTION_PIXEL_SHADER(3,false);
IMPLEMENT_SHADOW_PROJECTION_PIXEL_SHADER(4,false);
IMPLEMENT_SHADOW_PROJECTION_PIXEL_SHADER(5,false);

// Projection shaders with the distance fade, with different quality levels.
IMPLEMENT_SHADOW_PROJECTION_PIXEL_SHADER(1,true);
IMPLEMENT_SHADOW_PROJECTION_PIXEL_SHADER(2,true);
IMPLEMENT_SHADOW_PROJECTION_PIXEL_SHADER(3,true);
IMPLEMENT_SHADOW_PROJECTION_PIXEL_SHADER(4,true);
IMPLEMENT_SHADOW_PROJECTION_PIXEL_SHADER(5,true);
#endif

// with different quality levels
IMPLEMENT_SHADER_TYPE(template<>,TShadowProjectionFromTranslucencyPS<1>,TEXT("ShadowProjectionPixelShader"),TEXT("Main"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>,TShadowProjectionFromTranslucencyPS<2>,TEXT("ShadowProjectionPixelShader"),TEXT("Main"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>,TShadowProjectionFromTranslucencyPS<3>,TEXT("ShadowProjectionPixelShader"),TEXT("Main"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>,TShadowProjectionFromTranslucencyPS<4>,TEXT("ShadowProjectionPixelShader"),TEXT("Main"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>,TShadowProjectionFromTranslucencyPS<5>,TEXT("ShadowProjectionPixelShader"),TEXT("Main"),SF_Pixel);

// Implement a pixel shader for rendering one pass point light shadows with different quality levels
IMPLEMENT_SHADER_TYPE(template<>,TOnePassPointShadowProjectionPS<1>,TEXT("ShadowProjectionPixelShader"),TEXT("MainOnePassPointLightPS"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>,TOnePassPointShadowProjectionPS<2>,TEXT("ShadowProjectionPixelShader"),TEXT("MainOnePassPointLightPS"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>,TOnePassPointShadowProjectionPS<3>,TEXT("ShadowProjectionPixelShader"),TEXT("MainOnePassPointLightPS"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>,TOnePassPointShadowProjectionPS<4>,TEXT("ShadowProjectionPixelShader"),TEXT("MainOnePassPointLightPS"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>,TOnePassPointShadowProjectionPS<5>,TEXT("ShadowProjectionPixelShader"),TEXT("MainOnePassPointLightPS"),SF_Pixel);

/** 
 * Overrides a material used for shadow depth rendering with the default material when appropriate.
 * Overriding in this manner can reduce state switches and the number of shaders that have to be compiled.
 * This logic needs to stay in sync with shadow depth shader ShouldCache logic.
 */
void OverrideWithDefaultMaterialForShadowDepth(
	const FMaterialRenderProxy*& InOutMaterialRenderProxy, 
	const FMaterial*& InOutMaterialResource,
	bool bReflectiveShadowmap,
	ERHIFeatureLevel::Type InFeatureLevel)
{
	// Override with the default material when possible.
	if (!InOutMaterialResource->IsMasked() &&						// Don't override masked materials.
		!InOutMaterialResource->MaterialModifiesMeshPosition_RenderThread() &&	// Don't override materials using world position offset.
		!bReflectiveShadowmap)										// Don't override when rendering reflective shadow maps.
	{
		const FMaterialRenderProxy* DefaultProxy = UMaterial::GetDefaultMaterial(MD_Surface)->GetRenderProxy(false);
		const FMaterial* DefaultMaterialResource = DefaultProxy->GetMaterial(InFeatureLevel);

		// Override with the default material for opaque materials that don't modify mesh position.
		InOutMaterialRenderProxy = DefaultProxy;
		InOutMaterialResource = DefaultMaterialResource;
	}
}

/*-----------------------------------------------------------------------------
	FShadowDepthDrawingPolicy
-----------------------------------------------------------------------------*/

template <bool bRenderingReflectiveShadowMaps>
void FShadowDepthDrawingPolicy<bRenderingReflectiveShadowMaps>::UpdateElementState(FShadowStaticMeshElement& State, ERHIFeatureLevel::Type InFeatureLevel)
{
	// can be optimized
	*this = FShadowDepthDrawingPolicy(
		State.MaterialResource,
		bDirectionalLight,
		bOnePassPointLightShadow,
		bPreShadow,
		InFeatureLevel,
		State.Mesh->VertexFactory,
		State.RenderProxy,
		State.bIsTwoSided,
		State.Mesh->ReverseCulling);
}

template <bool bRenderingReflectiveShadowMaps>
FShadowDepthDrawingPolicy<bRenderingReflectiveShadowMaps>::FShadowDepthDrawingPolicy(
	const FMaterial* InMaterialResource,
	bool bInDirectionalLight,
	bool bInOnePassPointLightShadow,
	bool bInPreShadow,
	ERHIFeatureLevel::Type InFeatureLevel,
	const FVertexFactory* InVertexFactory,
	const FMaterialRenderProxy* InMaterialRenderProxy,
	bool bInCastShadowAsTwoSided,
	bool bInReverseCulling
	):
	FMeshDrawingPolicy(InVertexFactory,InMaterialRenderProxy,*InMaterialResource,false,bInCastShadowAsTwoSided),
	GeometryShader(NULL),
	FeatureLevel(InFeatureLevel),
	bDirectionalLight(bInDirectionalLight),
	bReverseCulling(bInReverseCulling),
	bOnePassPointLightShadow(bInOnePassPointLightShadow),
	bPreShadow(bInPreShadow)
{
	check(!bInOnePassPointLightShadow || !bRenderingReflectiveShadowMaps);

	if(!InVertexFactory)
	{
		// dummy object, needs call to UpdateElementState() to be fully initialized
		return;
	}

	// Use perspective correct shadow depths for shadow types which typically render low poly meshes into the shadow depth buffer.
	// Depth will be interpolated to the pixel shader and written out, which disables HiZ and double speed Z.
	// Directional light shadows use an ortho projection and can use the non-perspective correct path without artifacts.
	// One pass point lights don't output a linear depth, so they are already perspective correct.
	const bool bUsePerspectiveCorrectShadowDepths = !bInDirectionalLight && !bInOnePassPointLightShadow;

	HullShader = NULL;
	DomainShader = NULL;

	FVertexFactoryType* VFType = InVertexFactory->GetType();

	const bool bInitializeTessellationShaders = 
		MaterialResource->GetTessellationMode() != MTM_NoTessellation
		&& RHISupportsTessellation(GShaderPlatformForFeatureLevel[InFeatureLevel])
		&& VFType->SupportsTessellationShaders();

	bUsePositionOnlyVS = !bRenderingReflectiveShadowMaps
		&& VertexFactory->SupportsPositionOnlyStream()
		&& !MaterialResource->IsMasked()
		&& !MaterialResource->MaterialModifiesMeshPosition_RenderThread();

	// Vertex related shaders
	if (bOnePassPointLightShadow)
	{
		if (bUsePositionOnlyVS)
		{
			VertexShader = MaterialResource->GetShader<TShadowDepthVS<VertexShadowDepth_OnePassPointLight, false, true, true> >(VFType);
		}
		else
		{
			VertexShader = MaterialResource->GetShader<TShadowDepthVS<VertexShadowDepth_OnePassPointLight, false, false, true> >(VFType);
		}
		// Use the geometry shader which will clone output triangles to all faces of the cube map
		GeometryShader = MaterialResource->GetShader<FOnePassPointShadowProjectionGS>(VFType);
		if(bInitializeTessellationShaders)
		{
			HullShader = MaterialResource->GetShader<TShadowDepthHS<VertexShadowDepth_OnePassPointLight, false> >(VFType);	
			DomainShader = MaterialResource->GetShader<TShadowDepthDS<VertexShadowDepth_OnePassPointLight, false> >(VFType);	
		}
	}
	else if (bUsePerspectiveCorrectShadowDepths)
	{
		if (bRenderingReflectiveShadowMaps)
		{
			VertexShader = MaterialResource->GetShader<TShadowDepthVS<VertexShadowDepth_PerspectiveCorrect, true, false> >(VFType);	
		}
		else
		{
			if (bUsePositionOnlyVS)
			{
				VertexShader = MaterialResource->GetShader<TShadowDepthVS<VertexShadowDepth_PerspectiveCorrect, false, true> >(VFType);
			}
			else
			{
				VertexShader = MaterialResource->GetShader<TShadowDepthVS<VertexShadowDepth_PerspectiveCorrect, false, false> >(VFType);
			}
		}
		if(bInitializeTessellationShaders)
		{
			HullShader = MaterialResource->GetShader<TShadowDepthHS<VertexShadowDepth_PerspectiveCorrect, bRenderingReflectiveShadowMaps> >(VFType);	
			DomainShader = MaterialResource->GetShader<TShadowDepthDS<VertexShadowDepth_PerspectiveCorrect, bRenderingReflectiveShadowMaps> >(VFType);	
		}
	}
	else
	{
		if (bRenderingReflectiveShadowMaps)
		{
			VertexShader = MaterialResource->GetShader<TShadowDepthVS<VertexShadowDepth_OutputDepth, true, false> >(VFType);	
			if(bInitializeTessellationShaders)
			{
				HullShader = MaterialResource->GetShader<TShadowDepthHS<VertexShadowDepth_OutputDepth, true> >(VFType);	
				DomainShader = MaterialResource->GetShader<TShadowDepthDS<VertexShadowDepth_OutputDepth, true> >(VFType);	
			}
		}
		else
		{
			if (bUsePositionOnlyVS)
			{
				VertexShader = MaterialResource->GetShader<TShadowDepthVS<VertexShadowDepth_OutputDepth, false, true> >(VFType);
			}
			else
			{
				VertexShader = MaterialResource->GetShader<TShadowDepthVS<VertexShadowDepth_OutputDepth, false, false> >(VFType);
			}
			if(bInitializeTessellationShaders)
			{
				HullShader = MaterialResource->GetShader<TShadowDepthHS<VertexShadowDepth_OutputDepth, false> >(VFType);	
				DomainShader = MaterialResource->GetShader<TShadowDepthDS<VertexShadowDepth_OutputDepth, false> >(VFType);	
			}
		}
	}

	// Pixel shaders
	if (!MaterialResource->IsMasked() && !bUsePerspectiveCorrectShadowDepths && !bRenderingReflectiveShadowMaps && VertexFactory->SupportsNullPixelShader())
	{
		// No pixel shader necessary.
		PixelShader = NULL;
	}
	else
	{
		if (bUsePerspectiveCorrectShadowDepths)
		{
			PixelShader = (TShadowDepthBasePS<bRenderingReflectiveShadowMaps> *)MaterialResource->GetShader<TShadowDepthPS<PixelShadowDepth_PerspectiveCorrect, bRenderingReflectiveShadowMaps> >(VFType);
		}
		else if (bOnePassPointLightShadow)
		{
			PixelShader = (TShadowDepthBasePS<bRenderingReflectiveShadowMaps> *)MaterialResource->GetShader<TShadowDepthPS<PixelShadowDepth_OnePassPointLight, false> >(VFType);
		}
		else
		{
			PixelShader = (TShadowDepthBasePS<bRenderingReflectiveShadowMaps> *)MaterialResource->GetShader<TShadowDepthPS<PixelShadowDepth_NonPerspectiveCorrect, bRenderingReflectiveShadowMaps> >(VFType);
		}
	}
}

template <bool bRenderingReflectiveShadowMaps>
void FShadowDepthDrawingPolicy<bRenderingReflectiveShadowMaps>::SetSharedState(FRHICommandList& RHICmdList, const FSceneView* View, const ContextDataType PolicyContext) const
{
	checkSlow(bDirectionalLight == PolicyContext.ShadowInfo->bDirectionalLight && bPreShadow == PolicyContext.ShadowInfo->bPreShadow);

	VertexShader->SetParameters(RHICmdList, MaterialRenderProxy,*MaterialResource,*View,PolicyContext.ShadowInfo);

	if (GeometryShader)
	{
		GeometryShader->SetParameters(RHICmdList, *View,PolicyContext.ShadowInfo);
	}

	if(HullShader && DomainShader)
	{
		HullShader->SetParameters(RHICmdList, MaterialRenderProxy,*View);
		DomainShader->SetParameters(RHICmdList, MaterialRenderProxy,*View,PolicyContext.ShadowInfo);
	}

	if (PixelShader)
	{
		PixelShader->SetParameters(RHICmdList, MaterialRenderProxy,*MaterialResource,*View,PolicyContext.ShadowInfo);
	}

	// Set the shared mesh resources.
	if (bUsePositionOnlyVS)
	{
		VertexFactory->SetPositionStream(RHICmdList);
	}
	else
	{
		FMeshDrawingPolicy::SetSharedState(RHICmdList, View, PolicyContext);
	}

	// @TODO: only render directional light shadows as two sided, and only when blocking is enabled (required by geometry volume injection)
	bool bIsTwoSided = IsTwoSided();
	if ( PolicyContext.ShadowInfo->bReflectiveShadowmap )
	{
		bIsTwoSided = true;
	}

	// Set the rasterizer state only once per draw list bucket, instead of once per mesh in SetMeshRenderState as an optimization
	RHICmdList.SetRasterizerState(GetStaticRasterizerState<true>(
		FM_Solid,
		bIsTwoSided ? CM_None :
			// Have to flip culling when doing one pass point light shadows for some reason
			(XOR(View->bReverseCulling, XOR(bReverseCulling, bOnePassPointLightShadow)) ? CM_CCW : CM_CW)
		));
}

/** 
 * Create bound shader state using the vertex decl from the mesh draw policy
 * as well as the shaders needed to draw the mesh
 * @return new bound shader state object
 */
template <bool bRenderingReflectiveShadowMaps>
FBoundShaderStateInput FShadowDepthDrawingPolicy<bRenderingReflectiveShadowMaps>::GetBoundShaderStateInput(ERHIFeatureLevel::Type InFeatureLevel)
{
	FVertexDeclarationRHIRef VertexDeclaration;
	if (bUsePositionOnlyVS)
	{
		VertexDeclaration = VertexFactory->GetPositionDeclaration();
	}
	else
	{
		VertexDeclaration = FMeshDrawingPolicy::GetVertexDeclaration();
	}

	return FBoundShaderStateInput(
		VertexDeclaration, 
		VertexShader->GetVertexShader(),
		GETSAFERHISHADER_HULL(HullShader), 
		GETSAFERHISHADER_DOMAIN(DomainShader),
		GETSAFERHISHADER_PIXEL(PixelShader),
		GETSAFERHISHADER_GEOMETRY(GeometryShader));
}

template <bool bRenderingReflectiveShadowMaps>
void FShadowDepthDrawingPolicy<bRenderingReflectiveShadowMaps>::SetMeshRenderState(
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

	VertexShader->SetMesh(RHICmdList, VertexFactory,View,PrimitiveSceneProxy,BatchElement);

	if( HullShader && DomainShader )
	{
		HullShader->SetMesh(RHICmdList, VertexFactory,View,PrimitiveSceneProxy,BatchElement);
		DomainShader->SetMesh(RHICmdList, VertexFactory,View,PrimitiveSceneProxy,BatchElement);
	}
	if (GeometryShader)
	{
		GeometryShader->SetMesh(RHICmdList, PrimitiveSceneProxy, PolicyContext.ShadowInfo, View);
	}
	if (PixelShader)
	{
		PixelShader->SetMesh(RHICmdList, VertexFactory,View,PrimitiveSceneProxy,BatchElement);
	}
	// Not calling FMeshDrawingPolicy::SetMeshRenderState as SetSharedState sets the rasterizer state
}

template <bool bRenderingReflectiveShadowMaps>
int32 CompareDrawingPolicy(const FShadowDepthDrawingPolicy<bRenderingReflectiveShadowMaps>& A,const FShadowDepthDrawingPolicy<bRenderingReflectiveShadowMaps>& B)
{
	COMPAREDRAWINGPOLICYMEMBERS(VertexShader);
	COMPAREDRAWINGPOLICYMEMBERS(HullShader);
	COMPAREDRAWINGPOLICYMEMBERS(DomainShader);
	COMPAREDRAWINGPOLICYMEMBERS(GeometryShader);
	COMPAREDRAWINGPOLICYMEMBERS(PixelShader);
	COMPAREDRAWINGPOLICYMEMBERS(VertexFactory);
	COMPAREDRAWINGPOLICYMEMBERS(MaterialRenderProxy);
	COMPAREDRAWINGPOLICYMEMBERS(bDirectionalLight);
	// Have to sort on two sidedness because rasterizer state is set in SetSharedState and not SetMeshRenderState
	COMPAREDRAWINGPOLICYMEMBERS(bIsTwoSidedMaterial);
	COMPAREDRAWINGPOLICYMEMBERS(bReverseCulling);
	COMPAREDRAWINGPOLICYMEMBERS(bOnePassPointLightShadow);
	COMPAREDRAWINGPOLICYMEMBERS(bUsePositionOnlyVS);
	COMPAREDRAWINGPOLICYMEMBERS(bPreShadow);
	return 0;
}

void FShadowDepthDrawingPolicyFactory::AddStaticMesh(FScene* Scene,FStaticMesh* StaticMesh)
{
	if (StaticMesh->CastShadow)
	{
		const auto FeatureLevel = Scene->GetFeatureLevel();
		const FMaterialRenderProxy* MaterialRenderProxy = StaticMesh->MaterialRenderProxy;
		const FMaterial* Material = MaterialRenderProxy->GetMaterial(FeatureLevel);
		const EBlendMode BlendMode = Material->GetBlendMode();
		const EMaterialShadingModel ShadingModel = Material->GetShadingModel();

		const bool bLightPropagationVolume = UseLightPropagationVolumeRT(FeatureLevel);
		const bool bTwoSided  = Material->IsTwoSided() || StaticMesh->PrimitiveSceneInfo->Proxy->CastsShadowAsTwoSided();
		const bool bLitOpaque = !IsTranslucentBlendMode(BlendMode) && ShadingModel != MSM_Unlit;
		if(bLightPropagationVolume && (bLitOpaque || Material->ShouldInjectEmissiveIntoLPV()))
		{
			// Add the static mesh to the shadow's subject draw list.
			if ( StaticMesh->PrimitiveSceneInfo->Proxy->AffectsDynamicIndirectLighting() )
			{
				Scene->WholeSceneReflectiveShadowMapDrawList.AddMesh(
					StaticMesh,
					FShadowDepthDrawingPolicy<true>::ElementDataType(),
					FShadowDepthDrawingPolicy<true>(
						Material,
						true,
						false,
						false,
						FeatureLevel,
						StaticMesh->VertexFactory,
						MaterialRenderProxy,
						bTwoSided,
						StaticMesh->ReverseCulling),
					FeatureLevel
					);
			}
		}
		if ( bLitOpaque )
		{
			OverrideWithDefaultMaterialForShadowDepth(MaterialRenderProxy, Material, false, FeatureLevel); 

			// Add the static mesh to the shadow's subject draw list.
			Scene->WholeSceneShadowDepthDrawList.AddMesh(
				StaticMesh,
				FShadowDepthDrawingPolicy<false>::ElementDataType(),
				FShadowDepthDrawingPolicy<false>(
					Material,
					true,
					false,
					false,
					FeatureLevel,
					StaticMesh->VertexFactory,
					MaterialRenderProxy,
					bTwoSided,
					StaticMesh->ReverseCulling),
				FeatureLevel
				);
		}
	}
}

bool FShadowDepthDrawingPolicyFactory::DrawDynamicMesh(
	FRHICommandList& RHICmdList, 
	const FSceneView& View,
	ContextType Context,
	const FMeshBatch& Mesh,
	bool bBackFace,
	bool bPreFog,
	const FPrimitiveSceneProxy* PrimitiveSceneProxy,
	FHitProxyId HitProxyId
	)
{
	bool bDirty = false;

	// Use a per-FMeshBatch check on top of the per-primitive check because dynamic primitives can submit multiple FMeshElements.
	if (Mesh.CastShadow)
	{
		const auto FeatureLevel = View.GetFeatureLevel();
		const FMaterialRenderProxy* MaterialRenderProxy = Mesh.MaterialRenderProxy;
		const FMaterial* Material = MaterialRenderProxy->GetMaterial(FeatureLevel);
		const EBlendMode BlendMode = Material->GetBlendMode();
		const EMaterialShadingModel ShadingModel = Material->GetShadingModel();

		const bool bLocalOnePassPointLightShadow = Context.ShadowInfo->CascadeSettings.bOnePassPointLightShadow;
		const bool bReflectiveShadowmap = Context.ShadowInfo->bReflectiveShadowmap && !bLocalOnePassPointLightShadow && Material->ShouldInjectEmissiveIntoLPV();

		if ( (!IsTranslucentBlendMode(BlendMode) || bReflectiveShadowmap ) && ShadingModel != MSM_Unlit )
		{
			const bool bLocalDirectionalLight = Context.ShadowInfo->bDirectionalLight;
			const bool bPreShadow = Context.ShadowInfo->bPreShadow;
			const bool bTwoSided = Material->IsTwoSided() || PrimitiveSceneProxy->CastsShadowAsTwoSided();
			const FShadowDepthDrawingPolicyContext PolicyContext(Context.ShadowInfo);

			OverrideWithDefaultMaterialForShadowDepth(MaterialRenderProxy, Material, bReflectiveShadowmap, FeatureLevel);

			if(bReflectiveShadowmap)
			{
				FShadowDepthDrawingPolicy<true> DrawingPolicy(
					MaterialRenderProxy->GetMaterial(FeatureLevel),
					bLocalDirectionalLight,
					bLocalOnePassPointLightShadow,
					bPreShadow,
					FeatureLevel,
					Mesh.VertexFactory,
					MaterialRenderProxy,
					bTwoSided,
					Mesh.ReverseCulling
					);

				RHICmdList.BuildAndSetLocalBoundShaderState(DrawingPolicy.GetBoundShaderStateInput(FeatureLevel));
				DrawingPolicy.SetSharedState(RHICmdList, &View, PolicyContext);
				for (int32 BatchElementIndex = 0, Num = Mesh.Elements.Num(); BatchElementIndex < Num; BatchElementIndex++)
				{
					DrawingPolicy.SetMeshRenderState(RHICmdList, View,PrimitiveSceneProxy,Mesh,BatchElementIndex,bBackFace,FMeshDrawingPolicy::ElementDataType(),PolicyContext);
					DrawingPolicy.DrawMesh(RHICmdList, Mesh,BatchElementIndex);
				}
			}
			else
			{
				FShadowDepthDrawingPolicy<false> DrawingPolicy(
					MaterialRenderProxy->GetMaterial(FeatureLevel),
					bLocalDirectionalLight,
					bLocalOnePassPointLightShadow,
					bPreShadow,
					FeatureLevel,
					Mesh.VertexFactory,
					MaterialRenderProxy,
					bTwoSided,
					Mesh.ReverseCulling
					);

				RHICmdList.BuildAndSetLocalBoundShaderState(DrawingPolicy.GetBoundShaderStateInput(FeatureLevel));
				DrawingPolicy.SetSharedState(RHICmdList, &View, PolicyContext);
				for (int32 BatchElementIndex = 0; BatchElementIndex < Mesh.Elements.Num(); BatchElementIndex++)
				{
					DrawingPolicy.SetMeshRenderState(RHICmdList, View,PrimitiveSceneProxy,Mesh,BatchElementIndex,bBackFace,FMeshDrawingPolicy::ElementDataType(),PolicyContext);
					DrawingPolicy.DrawMesh(RHICmdList, Mesh,BatchElementIndex);
				}
			}

			
			bDirty = true;
		}
	}
	
	return bDirty;
}

/*-----------------------------------------------------------------------------
	FProjectedShadowInfo
-----------------------------------------------------------------------------*/

static void CheckShadowDepthMaterials(const FMaterialRenderProxy* InRenderProxy, const FMaterial* InMaterial, bool bReflectiveShadowmap, ERHIFeatureLevel::Type InFeatureLevel)
{
	const FMaterialRenderProxy* RenderProxy = InRenderProxy;
	const FMaterial* Material = InMaterial;
	OverrideWithDefaultMaterialForShadowDepth(RenderProxy, Material, bReflectiveShadowmap, InFeatureLevel);
	check(RenderProxy == InRenderProxy);
	check(Material == InMaterial);
}

void FProjectedShadowInfo::ClearDepth(FRHICommandList& RHICmdList, FDeferredShadingSceneRenderer* SceneRenderer, bool bPerformClear)
{
	uint32 ViewportMinX;
	uint32 ViewportMinY;
	float ViewportMinZ;
	uint32 ViewportMaxX;
	uint32 ViewportMaxY;
	float ViewportMaxZ;

	int32 NumClearColors;
	bool bClearColor;
	FLinearColor Colors[2];
	bool bClearDepth;
	float Depth;
	bool bClearStencil;
	uint32 Stencil;
	FIntRect ExcludeRect;

	if (CascadeSettings.bOnePassPointLightShadow)
	{
		// Set the viewport to the whole render target since it's a cube map, don't leave any border space
		ViewportMinX = 0;
		ViewportMinY = 0;
		ViewportMinZ = 0.0f;
		ViewportMaxX = ResolutionX;
		ViewportMaxY = ResolutionY;
		ViewportMaxZ = 1.0f;
		
		
		// Clear depth only.
		bClearColor = false;
		Colors[0] = FLinearColor::White;
		bClearDepth = true;
		Depth = 1.0f;
		bClearStencil = false;
		Stencil = 0;
		NumClearColors = 1;
	}
	else
	{
		if (bReflectiveShadowmap)
		{
			// Set the viewport for the shadow.
			ViewportMinX = X;
			ViewportMinY = Y;
			ViewportMinZ = 0.0f;
			ViewportMaxX = X + ResolutionX;
			ViewportMaxY = Y + ResolutionY;
			ViewportMaxZ = 1.0f;
			
			// Clear color and depth targets			
			bClearColor = true;
			Colors[0] = FLinearColor(0, 0, 1, 0);
			Colors[1] = FLinearColor(0, 0, 0, 0);
		
			bClearDepth = true;
			Depth = 1.0f;
			bClearStencil = false;
			Stencil = 0;
			NumClearColors = 2;			
		}
		else
		{
			// Set the viewport for the shadow.
			ViewportMinX = X;
			ViewportMinY = Y;
			ViewportMinZ = 0.0f;
			ViewportMaxX = X + SHADOW_BORDER * 2 + ResolutionX;
			ViewportMaxY = Y + SHADOW_BORDER * 2 + ResolutionY;
			ViewportMaxZ = 1.0f;

			// Clear depth only.
			bClearColor = false;
			Colors[0] = FLinearColor::White;
			bClearDepth = true;
			Depth = 1.0f;
			bClearStencil = false;
			Stencil = 0;
			NumClearColors = 1;			
		}
	}

	if (bPerformClear)
	{
		RHICmdList.SetViewport(
			ViewportMinX,
			ViewportMinY,
			ViewportMinZ,
			ViewportMaxX,
			ViewportMaxY,
			ViewportMaxZ
			);
		
		RHICmdList.ClearMRT(bClearColor, NumClearColors, Colors, bClearDepth, Depth, bClearStencil, Stencil, FIntRect());
	}
	else
	{
		RHICmdList.BindClearMRTValues(bClearColor, NumClearColors, Colors, bClearDepth, Depth, bClearStencil, Stencil);
	}
}



template <bool bReflectiveShadowmap>
void DrawMeshElements(FRHICommandList& RHICmdList, FShadowDepthDrawingPolicy<bReflectiveShadowmap>& SharedDrawingPolicy, const FShadowStaticMeshElement& State, const FViewInfo& View, FShadowDepthDrawingPolicyContext PolicyContext, const FStaticMesh* Mesh)
{
#if UE_BUILD_DEBUG
	// During shadow setup we should have already overridden materials with default material where needed.
	// Make sure of it!
	CheckShadowDepthMaterials(State.RenderProxy, State.MaterialResource, bReflectiveShadowmap, View.GetFeatureLevel());
#endif

#if UE_BUILD_DEBUG
	FShadowDepthDrawingPolicy<bReflectiveShadowmap> DebugPolicy(
		State.MaterialResource,
		SharedDrawingPolicy.bDirectionalLight,
		SharedDrawingPolicy.bOnePassPointLightShadow,
		SharedDrawingPolicy.bPreShadow,
		View.GetFeatureLevel(),
		State.Mesh->VertexFactory,
		State.RenderProxy,
		State.bIsTwoSided,
		State.Mesh->ReverseCulling);
	// Verify that SharedDrawingPolicy can be used to draw this mesh without artifacts by checking the comparison functions that static draw lists use
	checkSlow(DebugPolicy.Matches(SharedDrawingPolicy));
	checkSlow(CompareDrawingPolicy(DebugPolicy, SharedDrawingPolicy) == 0);
#endif

	// Render only those batch elements that match the current LOD
	uint64 BatchElementMask = Mesh->Elements.Num() == 1 ? 1 : View.StaticMeshBatchVisibility[Mesh->Id];
	int32 BatchElementIndex = 0;
	do
	{
		if(BatchElementMask & 1)
		{
			SharedDrawingPolicy.SetMeshRenderState(RHICmdList, View, Mesh->PrimitiveSceneInfo->Proxy, *Mesh, BatchElementIndex, false, FMeshDrawingPolicy::ElementDataType(),PolicyContext);
			SharedDrawingPolicy.DrawMesh(RHICmdList, *Mesh, BatchElementIndex);
			INC_DWORD_STAT(STAT_ShadowDynamicPathDrawCalls);
		}

		BatchElementMask >>= 1;
		BatchElementIndex++;
	} while(BatchElementMask);
}

template <bool bReflectiveShadowmap>
void DrawShadowMeshElements(FRHICommandList& RHICmdList, const FViewInfo& View, const FProjectedShadowInfo& ShadowInfo)
{
	FShadowDepthDrawingPolicyContext PolicyContext(&ShadowInfo);
	const FShadowStaticMeshElement& FirstShadowMesh = ShadowInfo.SubjectMeshElements[0];
	const FMaterial* FirstMaterialResource = FirstShadowMesh.MaterialResource;
	auto FeatureLevel = View.GetFeatureLevel();

	FShadowDepthDrawingPolicy<bReflectiveShadowmap> SharedDrawingPolicy(
		FirstMaterialResource,
		ShadowInfo.bDirectionalLight,
		ShadowInfo.CascadeSettings.bOnePassPointLightShadow,
		ShadowInfo.bPreShadow,
		FeatureLevel);

	FShadowStaticMeshElement OldState;

	uint32 ElementCount = ShadowInfo.SubjectMeshElements.Num();
	for(uint32 ElementIndex = 0; ElementIndex < ElementCount; ++ElementIndex)
	{
		const FShadowStaticMeshElement& ShadowMesh = ShadowInfo.SubjectMeshElements[ElementIndex];

		if(!View.StaticMeshShadowDepthMap[ShadowMesh.Mesh->Id])
		{
			// not visible
			continue;
		}

		FShadowStaticMeshElement CurrentState(ShadowMesh.RenderProxy, ShadowMesh.MaterialResource, ShadowMesh.Mesh, ShadowMesh.bIsTwoSided);

		// Only call draw shared when the vertex factory or material have changed
		if(OldState.DoesDeltaRequireADrawSharedCall(CurrentState))
		{
			OldState = CurrentState;

			SharedDrawingPolicy.UpdateElementState(CurrentState, FeatureLevel);
			RHICmdList.BuildAndSetLocalBoundShaderState(SharedDrawingPolicy.GetBoundShaderStateInput(View.GetFeatureLevel()));
			SharedDrawingPolicy.SetSharedState(RHICmdList, &View, PolicyContext);
		}

		DrawMeshElements(RHICmdList, SharedDrawingPolicy, OldState, View, PolicyContext, ShadowMesh.Mesh);
	}
}

void FProjectedShadowInfo::RenderDepthDynamic(FRHICommandList& RHICmdList, FSceneRenderer* SceneRenderer, const FViewInfo* FoundView)
{
	// Draw the subject's dynamic elements.
	SCOPE_CYCLE_COUNTER(STAT_WholeSceneDynamicShadowDepthsTime);

	FShadowDepthDrawingPolicyFactory::ContextType Context(this);

	for (int32 MeshBatchIndex = 0; MeshBatchIndex < DynamicSubjectMeshElements.Num(); MeshBatchIndex++)
	{
		const FMeshBatchAndRelevance& MeshBatchAndRelevance = DynamicSubjectMeshElements[MeshBatchIndex];
		const FMeshBatch& MeshBatch = *MeshBatchAndRelevance.Mesh;
		FShadowDepthDrawingPolicyFactory::DrawDynamicMesh(RHICmdList, *FoundView, Context, MeshBatch, false, true, MeshBatchAndRelevance.PrimitiveSceneProxy, MeshBatch.BatchHitProxyId);
	}
}

class FDrawShadowMeshElementsThreadTask
{
	FProjectedShadowInfo& ThisRenderer;
	FRHICommandList& RHICmdList;
	const FViewInfo& View;
	bool bReflective;

public:

	FDrawShadowMeshElementsThreadTask(
		FProjectedShadowInfo& InThisRenderer,
		FRHICommandList& InRHICmdList,
		const FViewInfo& InView,
		bool InbReflective
		)
		: ThisRenderer(InThisRenderer)
		, RHICmdList(InRHICmdList)
		, View(InView)
		, bReflective(InbReflective)
	{
	}

	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FDrawShadowMeshElementsThreadTask, STATGROUP_TaskGraphTasks);
	}

	ENamedThreads::Type GetDesiredThread()
	{
		return ENamedThreads::AnyThread;
	}

	static ESubsequentsMode::Type GetSubsequentsMode() { return ESubsequentsMode::TrackSubsequents; }

	void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
	{
		SCOPE_CYCLE_COUNTER(STAT_WholeSceneStaticShadowDepthsTime);

		if (bReflective)
		{
			// reflective shadow map
			DrawShadowMeshElements<true>(RHICmdList, View, ThisRenderer);
		}
		else
		{
			// normal shadow map
			DrawShadowMeshElements<false>(RHICmdList, View, ThisRenderer);
		}
		RHICmdList.HandleRTThreadTaskCompletion(MyCompletionGraphEvent);
	}
};

class FRenderDepthDynamicThreadTask
{
	FProjectedShadowInfo& ThisRenderer;
	FRHICommandList& RHICmdList;
	const FViewInfo& View;
	FSceneRenderer* SceneRenderer;

public:

	FRenderDepthDynamicThreadTask(
		FProjectedShadowInfo& InThisRenderer,
		FRHICommandList& InRHICmdList,
		const FViewInfo& InView,
		FSceneRenderer* InSceneRenderer
		)
		: ThisRenderer(InThisRenderer)
		, RHICmdList(InRHICmdList)
		, View(InView)
		, SceneRenderer(InSceneRenderer)
	{
	}

	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FRenderDepthDynamicThreadTask, STATGROUP_TaskGraphTasks);
	}

	ENamedThreads::Type GetDesiredThread()
	{
		return ENamedThreads::AnyThread;
	}

	static ESubsequentsMode::Type GetSubsequentsMode() { return ESubsequentsMode::TrackSubsequents; }

	void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
	{
		ThisRenderer.RenderDepthDynamic(RHICmdList, SceneRenderer, &View);
		RHICmdList.HandleRTThreadTaskCompletion(MyCompletionGraphEvent);
	}
};

void FProjectedShadowInfo::SetStateForDepth(FRHICommandList& RHICmdList)
{
	check(bAllocated);

	if (CascadeSettings.bOnePassPointLightShadow)
	{
		// Set the viewport to the whole render target since it's a cube map, don't leave any border space
		RHICmdList.SetViewport(
			0,
			0,
			0.0f,
			ResolutionX,
			ResolutionY,
			1.0f
			);
	}
	else
	{
		// Do not use a shadow border for RSMs.
		if(bReflectiveShadowmap)
		{
			RHICmdList.SetViewport(
				X,
				Y,
				0.0f,
				X + ResolutionX,
				Y + ResolutionY,
				1.0f
				);
		}
		else
		{
			RHICmdList.SetViewport(
				X + SHADOW_BORDER,
				Y + SHADOW_BORDER,
				0.0f,
				X + SHADOW_BORDER + ResolutionX,
				Y + SHADOW_BORDER + ResolutionY,
				1.0f
				);
		}
	}

	if (bReflectiveShadowmap && !CascadeSettings.bOnePassPointLightShadow)
	{
		// Enable color writes to the reflective shadow map targets with opaque blending
		RHICmdList.SetBlendState(TStaticBlendStateWriteMask<CW_RGBA, CW_RGBA>::GetRHI());
	}
	else
	{
		// Disable color writes
		RHICmdList.SetBlendState(TStaticBlendState<CW_NONE>::GetRHI());
	}

	RHICmdList.SetDepthStencilState(TStaticDepthStencilState<true, CF_LessEqual>::GetRHI());
}


static TAutoConsoleVariable<int32> CVarParallelShadows(
	TEXT("r.ParallelShadows"),
	0,
	TEXT("Toggles parallel shadow rendering. Parallel rendering must be enabled for this to have an effect."),
	ECVF_RenderThreadSafe
	);

static TAutoConsoleVariable<int32> CVarRHICmdShadowDeferredContexts(
	TEXT("r.RHICmdShadowDeferredContexts"),
	0,
	TEXT("True to use deferred contexts to parallelize shadow command list execution."));

class FShadowParallelCommandListSet : public FParallelCommandListSet
{
	FProjectedShadowInfo& ProjectedShadowInfo;
	TFunctionRef<void (FRHICommandList& RHICmdList)> SetShadowRenderTargets;
public:
	FShadowParallelCommandListSet(const FViewInfo& InView, FRHICommandList& InParentCmdList, bool* InOutDirty, bool bInParallelExecute, FProjectedShadowInfo& InProjectedShadowInfo, TFunctionRef<void (FRHICommandList& RHICmdList)> InSetShadowRenderTargets)
		: FParallelCommandListSet(InView, InParentCmdList, InOutDirty, bInParallelExecute)
		, ProjectedShadowInfo(InProjectedShadowInfo)
		, SetShadowRenderTargets(InSetShadowRenderTargets)
	{
		SetStateOnCommandList(ParentCmdList);
	}

	virtual ~FShadowParallelCommandListSet()
	{
		Dispatch();
	}

	virtual void SetStateOnCommandList(FRHICommandList& CmdList) override
	{	
		SetShadowRenderTargets(CmdList);
		ProjectedShadowInfo.SetStateForDepth(CmdList);
	}
};

void FProjectedShadowInfo::RenderDepthInner(FRHICommandList& RHICmdList, FSceneRenderer* SceneRenderer, const FViewInfo* FoundView, TFunctionRef<void (FRHICommandList& RHICmdList)> SetShadowRenderTargets)
{
	FShadowDepthDrawingPolicyContext PolicyContext(this);

	if (RHICmdList.IsImmediate() &&  // translucent shadows are draw on the render thread, using a recursive cmdlist
		GRHICommandList.UseParallelAlgorithms() && CVarParallelShadows.GetValueOnRenderThread())
	{
		// parallel version
		FScopedCommandListWaitForTasks Flusher;

		{
			FShadowParallelCommandListSet ParallelCommandListSet(*FoundView, RHICmdList, nullptr, CVarRHICmdShadowDeferredContexts.GetValueOnRenderThread() > 0, *this, SetShadowRenderTargets);

			// Draw the subject's static elements using static draw lists
			if (IsWholeSceneDirectionalShadow())
			{
				SCOPE_CYCLE_COUNTER(STAT_WholeSceneStaticDrawListShadowDepthsTime);

				if (bReflectiveShadowmap)
				{
					SceneRenderer->Scene->WholeSceneReflectiveShadowMapDrawList.DrawVisibleParallel(PolicyContext, StaticMeshWholeSceneShadowDepthMap, StaticMeshWholeSceneShadowBatchVisibility, ParallelCommandListSet);
				}
				else
				{
					// Use the scene's shadow depth draw list with this shadow's visibility map
					SceneRenderer->Scene->WholeSceneShadowDepthDrawList.DrawVisibleParallel(PolicyContext, StaticMeshWholeSceneShadowDepthMap, StaticMeshWholeSceneShadowBatchVisibility, ParallelCommandListSet);
				}
			}
			// Draw the subject's static elements using manual state filtering
			else if (SubjectMeshElements.Num() > 0)
			{
				FRHICommandList* CmdList = ParallelCommandListSet.NewParallelCommandList();

				FGraphEventRef AnyThreadCompletionEvent = TGraphTask<FDrawShadowMeshElementsThreadTask>::CreateTask(nullptr, ENamedThreads::RenderThread)
					.ConstructAndDispatchWhenReady(*this, *CmdList, *FoundView, bReflectiveShadowmap && !CascadeSettings.bOnePassPointLightShadow);

				ParallelCommandListSet.AddParallelCommandList(CmdList, AnyThreadCompletionEvent);
			}
			{
				FRHICommandList* CmdList = ParallelCommandListSet.NewParallelCommandList();

				FGraphEventRef AnyThreadCompletionEvent = TGraphTask<FRenderDepthDynamicThreadTask>::CreateTask(nullptr, ENamedThreads::RenderThread)
					.ConstructAndDispatchWhenReady(*this, *CmdList, *FoundView, SceneRenderer);

				ParallelCommandListSet.AddParallelCommandList(CmdList, AnyThreadCompletionEvent);
			}
		}
	}
	else
	{
		// single threaded version
		SetStateForDepth(RHICmdList);

		// Draw the subject's static elements using static draw lists
		if (IsWholeSceneDirectionalShadow())
		{
			SCOPE_CYCLE_COUNTER(STAT_WholeSceneStaticDrawListShadowDepthsTime);

			if (bReflectiveShadowmap)
			{
				SceneRenderer->Scene->WholeSceneReflectiveShadowMapDrawList.DrawVisible(RHICmdList, *FoundView, PolicyContext, StaticMeshWholeSceneShadowDepthMap, StaticMeshWholeSceneShadowBatchVisibility);
			}
			else
			{
				// Use the scene's shadow depth draw list with this shadow's visibility map
				SceneRenderer->Scene->WholeSceneShadowDepthDrawList.DrawVisible(RHICmdList, *FoundView, PolicyContext, StaticMeshWholeSceneShadowDepthMap, StaticMeshWholeSceneShadowBatchVisibility);
			}
		}
		// Draw the subject's static elements using manual state filtering
		else if (SubjectMeshElements.Num() > 0)
		{
			SCOPE_CYCLE_COUNTER(STAT_WholeSceneStaticShadowDepthsTime);

			if (bReflectiveShadowmap && !CascadeSettings.bOnePassPointLightShadow)
			{
				// reflective shadow map
				DrawShadowMeshElements<true>(RHICmdList, *FoundView, *this);
			}
			else
			{
				// normal shadow map
				DrawShadowMeshElements<false>(RHICmdList, *FoundView, *this);
			}
		}
		RenderDepthDynamic(RHICmdList, SceneRenderer, FoundView);
	}
}

void FProjectedShadowInfo::RenderDepth(FRHICommandList& RHICmdList, FSceneRenderer* SceneRenderer, TFunctionRef<void (FRHICommandList& RHICmdList)> SetShadowRenderTargets)
{
#if WANTS_DRAW_MESH_EVENTS
	FString EventName;
	GetShadowTypeNameForDrawEvent(EventName);
	SCOPED_DRAW_EVENTF(RHICmdList, EventShadowDepthActor, *EventName);
#endif

	CONDITIONAL_SCOPE_CYCLE_COUNTER(STAT_RenderWholeSceneShadowDepthsTime, bWholeSceneShadow);
	CONDITIONAL_SCOPE_CYCLE_COUNTER(STAT_RenderPerObjectShadowDepthsTime, !bWholeSceneShadow);

	// Choose an arbitrary view where this shadow's subject is relevant.
	FViewInfo* FoundView = NULL;
	for(int32 ViewIndex = 0;ViewIndex < SceneRenderer->Views.Num();ViewIndex++)
	{
		FViewInfo* CheckView = &SceneRenderer->Views[ViewIndex];
		const FVisibleLightViewInfo& VisibleLightViewInfo = CheckView->VisibleLightInfos[LightSceneInfo->Id];
		FPrimitiveViewRelevance ViewRel = VisibleLightViewInfo.ProjectedShadowViewRelevanceMap[ShadowId];
		if (ViewRel.bShadowRelevance)
		{
			FoundView = CheckView;
			break;
		}
	}
	check(FoundView 
		&& IsInRenderingThread()); // we should not hack the view in parallel

	// Backup properties of the view that we will override
	TUniformBufferRef<FViewUniformShaderParameters> OriginalUniformBuffer = FoundView->UniformBuffer;
	FMatrix OriginalViewMatrix = FoundView->ViewMatrices.ViewMatrix;
	FIntRect OriginalViewRect = FoundView->ViewRect;
	FoundView->ViewRect.Min.X = 0;
	FoundView->ViewRect.Min.Y = 0;
	FoundView->ViewRect.Max.X = ResolutionX;
	FoundView->ViewRect.Max.Y =  ResolutionY;

	float JitterX = FoundView->ViewMatrices.ProjMatrix.M[2][0];
	float JitterY = FoundView->ViewMatrices.ProjMatrix.M[2][1];

	FoundView->ViewMatrices.ProjMatrix.M[2][0] = 0.0f;
	FoundView->ViewMatrices.ProjMatrix.M[2][1] = 0.0f;

	{
		// Compute the view projection matrix and its inverse.
		FoundView->ViewProjectionMatrix = FoundView->ViewMatrices.ViewMatrix * FoundView->ViewMatrices.ProjMatrix;
		FoundView->InvViewProjectionMatrix = FoundView->ViewMatrices.GetInvProjMatrix() * FoundView->InvViewMatrix;

		/** The view transform, starting from world-space points translated by -ViewOrigin. */
		FMatrix TranslatedViewMatrix = FTranslationMatrix(-FoundView->ViewMatrices.PreViewTranslation) * FoundView->ViewMatrices.ViewMatrix;

		// Compute a transform from view origin centered world-space to clip space.
		FoundView->ViewMatrices.TranslatedViewProjectionMatrix = TranslatedViewMatrix * FoundView->ViewMatrices.ProjMatrix;
		FoundView->ViewMatrices.InvTranslatedViewProjectionMatrix = FoundView->ViewMatrices.TranslatedViewProjectionMatrix.Inverse();
	}


	// Override the view matrix so that billboarding primitives will be aligned to the light
	//@todo - creating a new uniform buffer is expensive, only do this when the vertex factory needs an accurate view matrix (particle sprites)
	FoundView->ViewMatrices.ViewMatrix = ShadowViewMatrix;
	FBox VolumeBounds[TVC_MAX];
	FoundView->UniformBuffer = FoundView->CreateUniformBuffer(
		nullptr,
		ShadowViewMatrix, 
		ShadowViewMatrix.Inverse(),
		VolumeBounds,
		TVC_MAX);

	// we are going to set this back now because we only want the correct view rect for the uniform buffer. For LOD calculations, we want the rendering viewrect and proj matrix.
	FoundView->ViewRect = OriginalViewRect;

	// Prevent materials from getting overridden during shadow casting in viewmodes like lighting only
	// Lighting only should only affect the material used with direct lighting, not the indirect lighting
	FoundView->bForceShowMaterials = true;

	RenderDepthInner(RHICmdList, SceneRenderer, FoundView, SetShadowRenderTargets);

	FoundView->bForceShowMaterials = false;
	FoundView->UniformBuffer = OriginalUniformBuffer;
	FoundView->ViewMatrices.ViewMatrix = OriginalViewMatrix;

	FoundView->ViewMatrices.ProjMatrix.M[2][0] = JitterX;
	FoundView->ViewMatrices.ProjMatrix.M[2][1] = JitterY;

	{
		// Compute the view projection matrix and its inverse.
		FoundView->ViewProjectionMatrix = FoundView->ViewMatrices.ViewMatrix * FoundView->ViewMatrices.ProjMatrix;
		FoundView->InvViewProjectionMatrix = FoundView->ViewMatrices.GetInvProjMatrix() * FoundView->InvViewMatrix;

		/** The view transform, starting from world-space points translated by -ViewOrigin. */
		FMatrix TranslatedViewMatrix = FTranslationMatrix(-FoundView->ViewMatrices.PreViewTranslation) * FoundView->ViewMatrices.ViewMatrix;

		// Compute a transform from view origin centered world-space to clip space.
		FoundView->ViewMatrices.TranslatedViewProjectionMatrix = TranslatedViewMatrix * FoundView->ViewMatrices.ProjMatrix;
		FoundView->ViewMatrices.InvTranslatedViewProjectionMatrix = FoundView->ViewMatrices.TranslatedViewProjectionMatrix.Inverse();
	}
}

void StencilingGeometry::DrawSphere(FRHICommandList& RHICmdList)
{
	RHICmdList.SetStreamSource(0, StencilingGeometry::GStencilSphereVertexBuffer.VertexBufferRHI, sizeof(FVector4), 0);
	RHICmdList.DrawIndexedPrimitive(StencilingGeometry::GStencilSphereIndexBuffer.IndexBufferRHI, PT_TriangleList, 0, 0,
		StencilingGeometry::GStencilSphereVertexBuffer.GetVertexCount(), 0, 
		StencilingGeometry::GStencilSphereIndexBuffer.GetIndexCount() / 3, 1);
}

void StencilingGeometry::DrawCone(FRHICommandList& RHICmdList)
{
	// No Stream Source needed since it will generate vertices on the fly
	RHICmdList.SetStreamSource(0, StencilingGeometry::GStencilConeVertexBuffer.VertexBufferRHI, sizeof(FVector4), 0);

	RHICmdList.DrawIndexedPrimitive(StencilingGeometry::GStencilConeIndexBuffer.IndexBufferRHI, PT_TriangleList, 0, 0,
		FStencilConeIndexBuffer::NumVerts, 0, StencilingGeometry::GStencilConeIndexBuffer.GetIndexCount() / 3, 1);
}

/** bound shader state for stencil masking the shadow projection [0]:FShadowProjectionNoTransformVS [1]:FShadowProjectionVS */
static FGlobalBoundShaderState MaskBoundShaderState[2];

template <uint32 Quality>
static void SetShadowProjectionShaderTemplNew(FRHICommandList& RHICmdList, int32 ViewIndex, const FViewInfo& View, const FProjectedShadowInfo* ShadowInfo)
{
	if (ShadowInfo->bTranslucentShadow)
	{
		// Get the Shadow Projection Vertex Shader (with transforms)
		FShadowProjectionVS* ShadowProjVS = View.ShaderMap->GetShader<FShadowProjectionVS>();

		// Get the translucency pixel shader
		FShadowProjectionPixelShaderInterface* ShadowProjPS = View.ShaderMap->GetShader<TShadowProjectionFromTranslucencyPS<Quality> >();

		// Bind shader
		static FGlobalBoundShaderState BoundShaderState;
		
		SetGlobalBoundShaderState(RHICmdList, View.GetFeatureLevel(), BoundShaderState, GetVertexDeclarationFVector4(), ShadowProjVS, ShadowProjPS);

		// Set shader parameters
		ShadowProjVS->SetParameters(RHICmdList, View, ShadowInfo);
		ShadowProjPS->SetParameters(RHICmdList, ViewIndex, View, ShadowInfo);
	}
	else if (ShadowInfo->IsWholeSceneDirectionalShadow())
	{
		// Get the Shadow Projection Vertex Shader which does not use a transform
		FShadowProjectionNoTransformVS* ShadowProjVS = View.ShaderMap->GetShader<FShadowProjectionNoTransformVS>();

		// Get the Shadow Projection Pixel Shader for PSSM
		if (ShadowInfo->CascadeSettings.FadePlaneLength > 0)
		{
			// This shader fades the shadow towards the end of the split subfrustum.
			FShadowProjectionPixelShaderInterface* ShadowProjPS = View.ShaderMap->GetShader<TShadowProjectionPS<Quality, true> >();

			static FGlobalBoundShaderState BoundShaderState;
			
			SetGlobalBoundShaderState(RHICmdList, View.GetFeatureLevel(), BoundShaderState, GetVertexDeclarationFVector4(), ShadowProjVS, ShadowProjPS);

			ShadowProjPS->SetParameters(RHICmdList, ViewIndex, View, ShadowInfo);
		}
		else
		{
			// Do not use the fade plane shader if the fade plane region length is 0 (avoids divide by 0).
			FShadowProjectionPixelShaderInterface* ShadowProjPS = View.ShaderMap->GetShader<TShadowProjectionPS<Quality, false> >();

			static FGlobalBoundShaderState BoundShaderState;
			
			SetGlobalBoundShaderState(RHICmdList, View.GetFeatureLevel(), BoundShaderState, GetVertexDeclarationFVector4(), ShadowProjVS, ShadowProjPS);

			ShadowProjPS->SetParameters(RHICmdList, ViewIndex, View, ShadowInfo);
		}

		ShadowProjVS->SetParameters(RHICmdList, View);
	}
	else
	{
		// Get the Shadow Projection Vertex Shader
		FShadowProjectionVS* ShadowProjVS = View.ShaderMap->GetShader<FShadowProjectionVS>();

		// Get the Shadow Projection Pixel Shader
		// This shader is the ordinary projection shader used by point/spot lights.		
		FShadowProjectionPixelShaderInterface* ShadowProjPS = View.ShaderMap->GetShader<TShadowProjectionPS<Quality, false> >();

		static FGlobalBoundShaderState BoundShaderState;
		
		SetGlobalBoundShaderState(RHICmdList, View.GetFeatureLevel(), BoundShaderState, GetVertexDeclarationFVector4(), ShadowProjVS, ShadowProjPS);

		ShadowProjVS->SetParameters(RHICmdList, View, ShadowInfo);
		ShadowProjPS->SetParameters(RHICmdList, ViewIndex, View, ShadowInfo);
	}
}

void FProjectedShadowInfo::RenderProjection(FRHICommandListImmediate& RHICmdList, int32 ViewIndex, const FViewInfo* View) const
{
#if WANTS_DRAW_MESH_EVENTS
	FString EventName;
	GetShadowTypeNameForDrawEvent(EventName);
	SCOPED_DRAW_EVENTF(RHICmdList, EventShadowProjectionActor, *EventName);
#endif

	FScopeCycleCounter Scope(bWholeSceneShadow ? GET_STATID(STAT_RenderWholeSceneShadowProjectionsTime) : GET_STATID(STAT_RenderPerObjectShadowProjectionsTime));

	// Find the shadow's view relevance.
	const FVisibleLightViewInfo& VisibleLightViewInfo = View->VisibleLightInfos[LightSceneInfo->Id];
	{
		FPrimitiveViewRelevance ViewRelevance = VisibleLightViewInfo.ProjectedShadowViewRelevanceMap[ShadowId];

		// Don't render shadows for subjects which aren't view relevant.
		if (ViewRelevance.bShadowRelevance == false)
		{
			return;
		}
	}

	if (CascadeSettings.bRayTracedDistanceField)
	{
		RenderRayTracedDistanceFieldProjection(RHICmdList, *View);
		return;
	}

	FVector4 FrustumVertices[8];

	// Calculate whether the camera is inside the shadow frustum, or the near plane is potentially intersecting the frustum.
	bool bCameraInsideShadowFrustum = true;

	if (!IsWholeSceneDirectionalShadow())
	{
		// The shadow transforms and view transforms are relative to different origins, so the world coordinates need to be translated.
		const FVector PreShadowToPreViewTranslation(View->ViewMatrices.PreViewTranslation - PreShadowTranslation);

		// fill out the frustum vertices (this is only needed in the non-whole scene case)
		for(uint32 vZ = 0;vZ < 2;vZ++)
		{
			for(uint32 vY = 0;vY < 2;vY++)
			{
				for(uint32 vX = 0;vX < 2;vX++)
				{
					const FVector4 UnprojectedVertex = InvReceiverMatrix.TransformFVector4(
						FVector4(
							(vX ? -1.0f : 1.0f),
							(vY ? -1.0f : 1.0f),
							(vZ ?  0.0f : 1.0f),
							1.0f
							)
						);
					const FVector ProjectedVertex = UnprojectedVertex / UnprojectedVertex.W + PreShadowToPreViewTranslation;
					FrustumVertices[GetCubeVertexIndex(vX,vY,vZ)] = FVector4(ProjectedVertex, 0);
				}
			}
		}

		const FVector FrontTopRight = FrustumVertices[GetCubeVertexIndex(0,0,1)] - View->ViewMatrices.PreViewTranslation;
		const FVector FrontTopLeft = FrustumVertices[GetCubeVertexIndex(1,0,1)] - View->ViewMatrices.PreViewTranslation;
		const FVector FrontBottomLeft = FrustumVertices[GetCubeVertexIndex(1,1,1)] - View->ViewMatrices.PreViewTranslation;
		const FVector FrontBottomRight = FrustumVertices[GetCubeVertexIndex(0,1,1)] - View->ViewMatrices.PreViewTranslation;
		const FVector BackTopRight = FrustumVertices[GetCubeVertexIndex(0,0,0)] - View->ViewMatrices.PreViewTranslation;
		const FVector BackTopLeft = FrustumVertices[GetCubeVertexIndex(1,0,0)] - View->ViewMatrices.PreViewTranslation;
		const FVector BackBottomLeft = FrustumVertices[GetCubeVertexIndex(1,1,0)] - View->ViewMatrices.PreViewTranslation;
		const FVector BackBottomRight = FrustumVertices[GetCubeVertexIndex(0,1,0)] - View->ViewMatrices.PreViewTranslation;

		const FPlane Front(FrontTopRight, FrontTopLeft, FrontBottomLeft);
		const float FrontDistance = Front.PlaneDot(View->ViewMatrices.ViewOrigin);

		const FPlane Right(BackTopRight, FrontTopRight, FrontBottomRight);
		const float RightDistance = Right.PlaneDot(View->ViewMatrices.ViewOrigin);

		const FPlane Back(BackTopLeft, BackTopRight, BackBottomRight);
		const float BackDistance = Back.PlaneDot(View->ViewMatrices.ViewOrigin);

		const FPlane Left(FrontTopLeft, BackTopLeft, BackBottomLeft);
		const float LeftDistance = Left.PlaneDot(View->ViewMatrices.ViewOrigin);

		const FPlane Top(BackTopRight, BackTopLeft, FrontTopLeft);
		const float TopDistance = Top.PlaneDot(View->ViewMatrices.ViewOrigin);

		const FPlane Bottom(FrontBottomRight, FrontBottomLeft, BackBottomLeft);
		const float BottomDistance = Bottom.PlaneDot(View->ViewMatrices.ViewOrigin);

		// Use a distance threshold to treat the case where the near plane is intersecting the frustum as the camera being inside
		// The near plane handling is not exact since it just needs to be conservative about saying the camera is outside the frustum
		const float DistanceThreshold = -View->NearClippingDistance * 3.0f;

		bCameraInsideShadowFrustum = 
			FrontDistance > DistanceThreshold && 
			RightDistance > DistanceThreshold && 
			BackDistance > DistanceThreshold && 
			LeftDistance > DistanceThreshold && 
			TopDistance > DistanceThreshold && 
			BottomDistance > DistanceThreshold;
	}

	// Depth test wo/ writes, no color writing.
	RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false, CF_DepthNearOrEqual>::GetRHI());
	RHICmdList.SetBlendState(TStaticBlendState<CW_NONE>::GetRHI());
	
	bool bDepthBoundsTestEnabled = false;

	// If this is a preshadow, mask the projection by the receiver primitives.
	if (bPreShadow || bSelfShadowOnly)
	{
		SCOPED_DRAW_EVENTF(RHICmdList, EventMaskSubjects, TEXT("Stencil Mask Subjects"));

		// Set stencil to one.
		RHICmdList.SetDepthStencilState(TStaticDepthStencilState<
			false,CF_DepthNearOrEqual,
			true,CF_Always,SO_Keep,SO_Keep,SO_Replace,
			false,CF_Always,SO_Keep,SO_Keep,SO_Keep,
			0xff,0xff
			>::GetRHI(), 1);

		// Pre-shadows mask by receiver elements, self-shadow mask by subject elements.
		// Note that self-shadow pre-shadows still mask by receiver elements.
		const TArray<FMeshBatchAndRelevance,SceneRenderingAllocator>& DynamicMeshElements = bPreShadow ? DynamicReceiverMeshElements : DynamicSubjectMeshElements;

		FDepthDrawingPolicyFactory::ContextType Context(DDM_AllOccluders);

		for (int32 MeshBatchIndex = 0; MeshBatchIndex < DynamicMeshElements.Num(); MeshBatchIndex++)
		{
			const FMeshBatchAndRelevance& MeshBatchAndRelevance = DynamicMeshElements[MeshBatchIndex];
			const FMeshBatch& MeshBatch = *MeshBatchAndRelevance.Mesh;
			FDepthDrawingPolicyFactory::DrawDynamicMesh(RHICmdList, *View, Context, MeshBatch, false, true, MeshBatchAndRelevance.PrimitiveSceneProxy, MeshBatch.BatchHitProxyId);
		}

		// Pre-shadows mask by receiver elements, self-shadow mask by subject elements.
		// Note that self-shadow pre-shadows still mask by receiver elements.
		const PrimitiveArrayType& MaskPrimitives = bPreShadow ? ReceiverPrimitives : SubjectPrimitives;

		for (int32 PrimitiveIndex = 0, PrimitiveCount = MaskPrimitives.Num(); PrimitiveIndex < PrimitiveCount; PrimitiveIndex++)
		{
			const FPrimitiveSceneInfo* ReceiverPrimitiveSceneInfo = MaskPrimitives[PrimitiveIndex];

			if (View->PrimitiveVisibilityMap[ReceiverPrimitiveSceneInfo->GetIndex()])
			{
				const FPrimitiveViewRelevance& ViewRelevance = View->PrimitiveViewRelevanceMap[ReceiverPrimitiveSceneInfo->GetIndex()];

				if (ViewRelevance.bRenderInMainPass && ViewRelevance.bStaticRelevance)
				{
					for (int32 StaticMeshIdx = 0; StaticMeshIdx < ReceiverPrimitiveSceneInfo->StaticMeshes.Num(); StaticMeshIdx++)
					{
						const FStaticMesh& StaticMesh = ReceiverPrimitiveSceneInfo->StaticMeshes[StaticMeshIdx];

						if (View->StaticMeshVisibilityMap[StaticMesh.Id])
						{
							FDepthDrawingPolicyFactory::DrawStaticMesh(
								RHICmdList, 
								*View,
								FDepthDrawingPolicyFactory::ContextType(DDM_AllOccluders),
								StaticMesh,
								StaticMesh.Elements.Num() == 1 ? 1 : View->StaticMeshBatchVisibility[StaticMesh.Id],
								true,
								ReceiverPrimitiveSceneInfo->Proxy,
								StaticMesh.BatchHitProxyId
								);
						}
					}
				}
			}
		}
	}
	else if (IsWholeSceneDirectionalShadow())
	{
		if( GSupportsDepthBoundsTest && CVarCSMDepthBoundsTest.GetValueOnRenderThread() != 0 )
		{
			FVector4 Near = View->ViewMatrices.ProjMatrix.TransformFVector4(FVector4(0, 0, CascadeSettings.SplitNear));
			FVector4 Far = View->ViewMatrices.ProjMatrix.TransformFVector4(FVector4(0, 0, CascadeSettings.SplitFar));
			float DepthNear = Near.Z / Near.W;
			float DepthFar = Far.Z / Far.W;

			DepthFar = FMath::Clamp( DepthFar, 0.0f, 1.0f );
			DepthNear = FMath::Clamp( DepthNear, 0.0f, 1.0f );

			if( DepthNear <= DepthFar )
			{
				DepthNear = 1.0f;
				DepthFar = 0.0f;
			}

			// Note, using a reversed z depth surface
			RHICmdList.EnableDepthBoundsTest(true, DepthFar, DepthNear);
			bDepthBoundsTestEnabled = true;
		}
		else
		{
		// Increment stencil on front-facing zfail, decrement on back-facing zfail.
			RHICmdList.SetDepthStencilState(TStaticDepthStencilState<
			false,CF_DepthNearOrEqual,
			true,CF_Always,SO_Keep,SO_Increment,SO_Keep,
			true,CF_Always,SO_Keep,SO_Decrement,SO_Keep,
			0xff,0xff
			>::GetRHI());

			RHICmdList.SetRasterizerState(TStaticRasterizerState<FM_Solid, CM_None>::GetRHI());

			checkSlow(CascadeSettings.ShadowSplitIndex >= 0);
			checkSlow(bDirectionalLight);

			// Draw 2 fullscreen planes, front facing one at the near subfrustum plane, and back facing one at the far.

			// Find the projection shaders.
			TShaderMapRef<FShadowProjectionNoTransformVS> VertexShaderNoTransform(View->ShaderMap);
			VertexShaderNoTransform->SetParameters(RHICmdList, *View);
			SetGlobalBoundShaderState(RHICmdList, View->GetFeatureLevel(), MaskBoundShaderState[0], GetVertexDeclarationFVector4(), *VertexShaderNoTransform, nullptr);

			FVector4 Near = View->ViewMatrices.ProjMatrix.TransformFVector4(FVector4(0, 0, CascadeSettings.SplitNear));
			FVector4 Far = View->ViewMatrices.ProjMatrix.TransformFVector4(FVector4(0, 0, CascadeSettings.SplitFar));
			float StencilNear = Near.Z / Near.W;
			float StencilFar = Far.Z / Far.W;

			FVector4 Verts[] =
			{
				// Far Plane
				FVector4( 1,  1,  StencilFar),
				FVector4(-1,  1,  StencilFar),
				FVector4( 1, -1,  StencilFar),
				FVector4( 1, -1,  StencilFar),
				FVector4(-1,  1,  StencilFar),
				FVector4(-1, -1,  StencilFar),

				// Near Plane
				FVector4(-1,  1, StencilNear),
				FVector4( 1,  1, StencilNear),
				FVector4(-1, -1, StencilNear),
				FVector4(-1, -1, StencilNear),
				FVector4( 1,  1, StencilNear),
				FVector4( 1, -1, StencilNear),
			};

			// Only draw the near plane if this is not the nearest split
			DrawPrimitiveUP(RHICmdList, PT_TriangleList, (CascadeSettings.ShadowSplitIndex > 0) ? 4 : 2, Verts, sizeof(FVector4));
		}
	}
	// Not a preshadow, mask the projection to any pixels inside the frustum.
	else
	{
		// Solid rasterization wo/ backface culling.
		RHICmdList.SetRasterizerState(TStaticRasterizerState<FM_Solid, CM_None>::GetRHI());

		if (bCameraInsideShadowFrustum)
		{
			// Use zfail stenciling when the camera is inside the frustum or the near plane is potentially clipping, 
			// Because zfail handles these cases while zpass does not.
			// zfail stenciling is somewhat slower than zpass because on xbox 360 HiZ will be disabled when setting up stencil.
			// Increment stencil on front-facing zfail, decrement on back-facing zfail.
			RHICmdList.SetDepthStencilState(TStaticDepthStencilState<
				false,CF_DepthNearOrEqual,
				true,CF_Always,SO_Keep,SO_Increment,SO_Keep,
				true,CF_Always,SO_Keep,SO_Decrement,SO_Keep,
				0xff,0xff
				>::GetRHI());
		}
		else
		{
			// Increment stencil on front-facing zpass, decrement on back-facing zpass.
			// HiZ will be enabled on xbox 360 which will save a little GPU time.
			RHICmdList.SetDepthStencilState(TStaticDepthStencilState<
				false,CF_DepthNearOrEqual,
				true,CF_Always,SO_Keep,SO_Keep,SO_Increment,
				true,CF_Always,SO_Keep,SO_Keep,SO_Decrement,
				0xff,0xff
				>::GetRHI());
		}
		
		// Find the projection shaders.
		TShaderMapRef<FShadowProjectionVS> VertexShader(View->ShaderMap);

		// Cache the bound shader state
		SetGlobalBoundShaderState(RHICmdList, View->GetFeatureLevel(), MaskBoundShaderState[1], GetVertexDeclarationFVector4(), *VertexShader, NULL);

		// Set the projection vertex shader parameters
		VertexShader->SetParameters(RHICmdList, *View, this);

		// Draw the frustum using the stencil buffer to mask just the pixels which are inside the shadow frustum.
		DrawIndexedPrimitiveUP(RHICmdList, PT_TriangleList, 0, 8, 12, GCubeIndices, sizeof(uint16), FrustumVertices, sizeof(FVector4));
	}

	// solid rasterization w/ back-face culling.
	RHICmdList.SetRasterizerState(
		XOR(View->bReverseCulling, IsWholeSceneDirectionalShadow()) ? TStaticRasterizerState<FM_Solid,CM_CCW>::GetRHI() : TStaticRasterizerState<FM_Solid,CM_CW>::GetRHI());

	if( bDepthBoundsTestEnabled )
	{
		// no depth test or writes
		RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());
	}
	else
	{
		// no depth test or writes, Test stencil for non-zero.
		RHICmdList.SetDepthStencilState(TStaticDepthStencilState<
			false,CF_Always,
			true,CF_NotEqual,SO_Keep,SO_Keep,SO_Keep,
			false,CF_Always,SO_Keep,SO_Keep,SO_Keep,
			0xff,0xff
			>::GetRHI());
	}

	// Light Attenuation channel assignment:
	//  R:     WholeSceneShadows, non SSS
	//  G:     WholeSceneShadows,     SSS
	//  B: non WholeSceneShadows, non SSS
	//  A: non WholeSceneShadows,     SSS
	//
	// SSS: SubsurfaceScattering materials
	// non SSS: shadow for opaque materials
	// WholeSceneShadows: directional light
	// non WholeSceneShadows: spotlight, per object shadows, translucency lighting, omni-directional lights

	//@todo - get rid of this extra channel requirement by ordering the light mask accumulation

	if (IsWholeSceneDirectionalShadow())
	{
		// use R and G in Light Attenuation (see above)

		if(CascadeSettings.FadePlaneLength > 0)
		{
			// alpha is used to fade between cascades, we don't don't need to do BO_Min as we leave B and A untouched which has translucency shadow
			RHICmdList.SetBlendState(TStaticBlendState<CW_RG, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha>::GetRHI());
		}
		else
		{
			// first cascade rendered or old method doesn't require fading (CO_Min is needed to combine multiple shadow passes)
			RHICmdList.SetBlendState(TStaticBlendState<CW_RG, BO_Min, BF_One, BF_One>::GetRHI());
		}
	}
	else
	{
		// use B and A in Light Attenuation (see above)

		// (CO_Min is needed to combine multiple shadow passes)
		RHICmdList.SetBlendState(TStaticBlendState<CW_BA, BO_Min, BF_One, BF_One, BO_Min, BF_One, BF_One>::GetRHI());
	}
	
	{
		uint32 LocalQuality = GetShadowQuality();

		if (LocalQuality > 1)
		{
			if (IsWholeSceneDirectionalShadow() && CascadeSettings.ShadowSplitIndex > 0)
			{
				// adjust kernel size so that the penumbra size of distant splits will better match up with the closer ones
				const float SizeScale = CascadeSettings.ShadowSplitIndex / FMath::Max(0.001f, CVarCSMSplitPenumbraScale.GetValueOnRenderThread());
			}
			else if (LocalQuality > 2 && !bWholeSceneShadow)
			{
				static auto CVarPreShadowResolutionFactor = IConsoleManager::Get().FindTConsoleVariableDataFloat(TEXT("r.Shadow.PreShadowResolutionFactor"));
				const int32 TargetResolution = bPreShadow ? FMath::TruncToInt(512 * CVarPreShadowResolutionFactor->GetValueOnRenderThread()) : 512;

				int32 Reduce = 0;

				{
					int32 Res = ResolutionX;

					while (Res < TargetResolution)
					{
						Res *= 2;
						++Reduce;
					}
				}

				// Never drop to quality 1 due to low resolution, aliasing is too bad
				LocalQuality = FMath::Clamp((int32)LocalQuality - Reduce, 3, 5);
			}
		}

		switch(LocalQuality)
		{
			case 1: SetShadowProjectionShaderTemplNew<1>(RHICmdList, ViewIndex, *View, this); break;
			case 2: SetShadowProjectionShaderTemplNew<2>(RHICmdList, ViewIndex, *View, this); break;
			case 3: SetShadowProjectionShaderTemplNew<3>(RHICmdList, ViewIndex, *View, this); break;
			case 4: SetShadowProjectionShaderTemplNew<4>(RHICmdList, ViewIndex, *View, this); break;
			case 5: SetShadowProjectionShaderTemplNew<5>(RHICmdList, ViewIndex, *View, this); break;
			default:
				check(0);
		}
	}

	if (IsWholeSceneDirectionalShadow())
	{
		// Render a full screen quad.
		FVector4 Verts[4] =
		{
			FVector4(-1.0f, 1.0f, 0.0f),
			FVector4(1.0f, 1.0f, 0.0f),
			FVector4(-1.0f, -1.0f, 0.0f),
			FVector4(1.0f, -1.0f, 0.0f),
		};
		DrawPrimitiveUP(RHICmdList, PT_TriangleStrip, 2, Verts, sizeof(FVector4));
	}
	else
	{
		// Draw the frustum using the projection shader..
		DrawIndexedPrimitiveUP(RHICmdList, PT_TriangleList, 0, 8, 12, GCubeIndices, sizeof(uint16), FrustumVertices, sizeof(FVector4));
	}

	if( bDepthBoundsTestEnabled )
	{
		// Disable depth bounds testing
		RHICmdList.EnableDepthBoundsTest(false, 0, 1);
	}
	else
	{
		// Clear the stencil buffer to 0.
		RHICmdList.Clear(false, FColor(0, 0, 0), false, 0, true, 0, FIntRect());
	}
}


template <uint32 Quality>
static void SetPointLightShaderTempl(FRHICommandList& RHICmdList, int32 ViewIndex, const FViewInfo& View, const FProjectedShadowInfo* ShadowInfo)
{
	TShaderMapRef<FShadowProjectionVS> VertexShader(View.ShaderMap);
	TShaderMapRef<TOnePassPointShadowProjectionPS<Quality> > PixelShader(View.ShaderMap);

	static FGlobalBoundShaderState BoundShaderState;
	
	SetGlobalBoundShaderState(RHICmdList, View.GetFeatureLevel(), BoundShaderState, GetVertexDeclarationFVector4(), *VertexShader, *PixelShader);

	VertexShader->SetParameters(RHICmdList, View, ShadowInfo);
	PixelShader->SetParameters(RHICmdList, ViewIndex, View, ShadowInfo);
}

/** Render one pass point light shadow projections. */
void FProjectedShadowInfo::RenderOnePassPointLightProjection(FRHICommandListImmediate& RHICmdList, int32 ViewIndex, const FViewInfo& View) const
{
	SCOPE_CYCLE_COUNTER(STAT_RenderWholeSceneShadowProjectionsTime);

	checkSlow(CascadeSettings.bOnePassPointLightShadow);
	
	const FSphere LightBounds = LightSceneInfo->Proxy->GetBoundingSphere();

	// Use min blending to RGB since shadows may overlap, preserve Alpha
	RHICmdList.SetBlendState(TStaticBlendState<CW_RGBA, BO_Min, BF_One, BF_One, BO_Add, BF_One, BF_Zero>::GetRHI());

	const bool bCameraInsideLightGeometry = ((FVector)View.ViewMatrices.ViewOrigin - LightBounds.Center).SizeSquared() < FMath::Square(LightBounds.W * 1.05f + View.NearClippingDistance * 2.0f);

	if (bCameraInsideLightGeometry)
	{
		RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());
		// Render backfaces with depth tests disabled since the camera is inside (or close to inside) the light geometry
		RHICmdList.SetRasterizerState(View.bReverseCulling ? TStaticRasterizerState<FM_Solid, CM_CW>::GetRHI() : TStaticRasterizerState<FM_Solid, CM_CCW>::GetRHI());
	}
	else
	{
		// Render frontfaces with depth tests on to get the speedup from HiZ since the camera is outside the light geometry
		RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false, CF_DepthNearOrEqual>::GetRHI());
		RHICmdList.SetRasterizerState(View.bReverseCulling ? TStaticRasterizerState<FM_Solid, CM_CCW>::GetRHI() : TStaticRasterizerState<FM_Solid, CM_CW>::GetRHI());
	}

	{
		uint32 LocalQuality = GetShadowQuality();

		if(LocalQuality > 1)
		{
			// adjust kernel size so that the penumbra size of distant splits will better match up with the closer ones
			//const float SizeScale = ShadowInfo->ResolutionX;
			int32 Reduce = 0;

			{
				int32 Res = ResolutionX;

				while(Res < 512)
				{
					Res *= 2;
					++Reduce;
				}
			}
		}

		switch(LocalQuality)
		{
			case 1: SetPointLightShaderTempl<1>(RHICmdList, ViewIndex, View, this); break;
			case 2: SetPointLightShaderTempl<2>(RHICmdList, ViewIndex, View, this); break;
			case 3: SetPointLightShaderTempl<3>(RHICmdList, ViewIndex, View, this); break;
			case 4: SetPointLightShaderTempl<4>(RHICmdList, ViewIndex, View, this); break;
			case 5: SetPointLightShaderTempl<5>(RHICmdList, ViewIndex, View, this); break;
			default:
				check(0);
		}
	}

	// Project the point light shadow with some approximately bounding geometry, 
	// So we can get speedups from depth testing and not processing pixels outside of the light's influence.
	StencilingGeometry::DrawSphere(RHICmdList);
}

void FProjectedShadowInfo::RenderFrustumWireframe(FPrimitiveDrawInterface* PDI) const
{
	// Find the ID of an arbitrary subject primitive to use to color the shadow frustum.
	int32 SubjectPrimitiveId = 0;
	if(SubjectPrimitives.Num())
	{
		SubjectPrimitiveId = SubjectPrimitives[0]->GetIndex();
	}

	const FMatrix InvShadowTransform = (bWholeSceneShadow || bPreShadow) ? SubjectAndReceiverMatrix.InverseFast() : InvReceiverMatrix;

	FColor Color;

	if(IsWholeSceneDirectionalShadow())
	{
		Color = FColor::White;
		switch(CascadeSettings.ShadowSplitIndex)
		{
			case 0: Color = FColor::Red; break;
			case 1: Color = FColor::Yellow; break;
			case 2: Color = FColor::Green; break;
			case 3: Color = FColor::Blue; break;
		}
	}
	else
	{
		Color = FLinearColor::FGetHSV(((SubjectPrimitiveId + LightSceneInfo->Id) * 31) & 255,0,255);
	}

	// Render the wireframe for the frustum derived from ReceiverMatrix.
	DrawFrustumWireframe(
		PDI,
		InvShadowTransform * FTranslationMatrix(-PreShadowTranslation),
		Color,
		SDPG_World
		);
}

FMatrix FProjectedShadowInfo::GetScreenToShadowMatrix(const FSceneView& View) const
{
	const FIntPoint ShadowBufferResolution = GetShadowBufferResolution();
	const float InvBufferResolutionX = 1.0f / (float)ShadowBufferResolution.X;
	const float ShadowResolutionFractionX = 0.5f * (float)ResolutionX * InvBufferResolutionX;
	const float InvBufferResolutionY = 1.0f / (float)ShadowBufferResolution.Y;
	const float ShadowResolutionFractionY = 0.5f * (float)ResolutionY * InvBufferResolutionY;
	// Calculate the matrix to transform a screenspace position into shadow map space
	FMatrix ScreenToShadow = 
		// Z of the position being transformed is actually view space Z, 
		// Transform it into post projection space by applying the projection matrix,
		// Which is the required space before applying View.InvTranslatedViewProjectionMatrix
		FMatrix(
			FPlane(1,0,0,0),
			FPlane(0,1,0,0),
			FPlane(0,0,View.ViewMatrices.ProjMatrix.M[2][2],1),
			FPlane(0,0,View.ViewMatrices.ProjMatrix.M[3][2],0)) * 
		// Transform the post projection space position into translated world space
		// Translated world space is normal world space translated to the view's origin, 
		// Which prevents floating point imprecision far from the world origin.
		View.ViewMatrices.InvTranslatedViewProjectionMatrix *
		// Translate to the origin of the shadow's translated world space
		FTranslationMatrix(PreShadowTranslation - View.ViewMatrices.PreViewTranslation) *
		// Transform into the shadow's post projection space
		// This has to be the same transform used to render the shadow depths
		SubjectAndReceiverMatrix *
		// Scale and translate x and y to be texture coordinates into the ShadowInfo's rectangle in the shadow depth buffer
		// Normalize z by MaxSubjectDepth, as was done when writing shadow depths
		FMatrix(
			FPlane(ShadowResolutionFractionX,0,							0,									0),
			FPlane(0,						 -ShadowResolutionFractionY,0,									0),
			FPlane(0,						0,							InvMaxSubjectDepth,	0),
			FPlane(
				(X + SHADOW_BORDER) * InvBufferResolutionX + ShadowResolutionFractionX,
				(Y + SHADOW_BORDER) * InvBufferResolutionY + ShadowResolutionFractionY,
				0,
				1
			)
		);
	return ScreenToShadow;
}

FMatrix FProjectedShadowInfo::GetWorldToShadowMatrix(FVector4& ShadowmapMinMax, const FIntPoint* ShadowBufferResolutionOverride, bool bHasShadowBorder ) const
{
	FIntPoint ShadowBufferResolution = ( ShadowBufferResolutionOverride ) ? *ShadowBufferResolutionOverride : GetShadowBufferResolution();
	float ShadowBorder = (bHasShadowBorder) ? SHADOW_BORDER : 0.0f;

	const float InvBufferResolutionX = 1.0f / (float)ShadowBufferResolution.X;
	const float ShadowResolutionFractionX = 0.5f * (float)ResolutionX * InvBufferResolutionX;
	const float InvBufferResolutionY = 1.0f / (float)ShadowBufferResolution.Y;
	const float ShadowResolutionFractionY = 0.5f * (float)ResolutionY * InvBufferResolutionY;

	const FMatrix WorldToShadowMatrix =
		// Translate to the origin of the shadow's translated world space
		FTranslationMatrix(PreShadowTranslation) *
		// Transform into the shadow's post projection space
		// This has to be the same transform used to render the shadow depths
		SubjectAndReceiverMatrix *
		// Scale and translate x and y to be texture coordinates into the ShadowInfo's rectangle in the shadow depth buffer
		// Normalize z by MaxSubjectDepth, as was done when writing shadow depths
		FMatrix(
			FPlane(ShadowResolutionFractionX,0,							0,									0),
			FPlane(0,						 -ShadowResolutionFractionY,0,									0),
			FPlane(0,						0,							InvMaxSubjectDepth,	0),
			FPlane(
				(X + ShadowBorder) * InvBufferResolutionX + ShadowResolutionFractionX,
				(Y + ShadowBorder) * InvBufferResolutionY + ShadowResolutionFractionY,
				0,
				1
			)
		);

	ShadowmapMinMax = FVector4(
		(X + ShadowBorder) * InvBufferResolutionX, 
		(Y + ShadowBorder) * InvBufferResolutionY,
		(X + ShadowBorder * 2 + ResolutionX) * InvBufferResolutionX, 
		(Y + ShadowBorder * 2 + ResolutionY) * InvBufferResolutionY);

	return WorldToShadowMatrix;
}

/** Returns the resolution of the shadow buffer used for this shadow, based on the shadow's type. */
FIntPoint FProjectedShadowInfo::GetShadowBufferResolution() const
{
	if (bTranslucentShadow)
	{
		return GSceneRenderTargets.GetTranslucentShadowDepthTextureResolution();
	}

	const FTexture2DRHIRef& ShadowTexture = GSceneRenderTargets.GetShadowDepthZTexture(bAllocatedInPreshadowCache);

	//prefer to return the actual size of the allocated texture if possible.  It may be larger than the size of a single shadowmap due to atlasing (see forward renderer CSM handling in InitDynamicShadows).
	if (ShadowTexture)
	{
		return FIntPoint(ShadowTexture->GetSizeX(), ShadowTexture->GetSizeY());
	}
	else
	{
		return bAllocatedInPreshadowCache ? GSceneRenderTargets.GetPreShadowCacheTextureResolution() : GSceneRenderTargets.GetShadowDepthTextureResolution();
	}
}

void FProjectedShadowInfo::UpdateShaderDepthBias()
{
	float DepthBias = 0;

	if (IsWholeScenePointLightShadow())
	{
		DepthBias = CVarPointLightShadowDepthBias.GetValueOnRenderThread() * 512.0f / FMath::Max(ResolutionX, ResolutionY);
		// * 2.0f to be compatible with the system we had before ShadowBias
		DepthBias *= 2.0f * LightSceneInfo->Proxy->GetUserShadowBias();
	}
	else if (IsWholeSceneDirectionalShadow())
	{
		check(CascadeSettings.ShadowSplitIndex >= 0);

		// the z range is adjusted to we need to adjust here as well
		DepthBias = CVarCSMShadowDepthBias.GetValueOnRenderThread() / (MaxSubjectZ - MinSubjectZ);

		float WorldSpaceTexelScale = ShadowBounds.W / ResolutionX;
		
		DepthBias *= WorldSpaceTexelScale;
		DepthBias *= LightSceneInfo->Proxy->GetUserShadowBias();
	}
	else if (bPreShadow)
	{
		// Preshadows don't need a depth bias since there is no self shadowing
		DepthBias = 0;
	}
	else
	{
		// per object shadows
		if(bDirectionalLight)
		{
			// we use CSMShadowDepthBias cvar but this is per object shadows, maybe we want to use different settings

			// the z range is adjusted to we need to adjust here as well
			DepthBias = CVarCSMShadowDepthBias.GetValueOnRenderThread() / (MaxSubjectZ - MinSubjectZ);

			float WorldSpaceTexelScale = ShadowBounds.W / FMath::Max(ResolutionX, ResolutionY);
		
			DepthBias *= WorldSpaceTexelScale;
			DepthBias *= 0.5f;	// avg GetUserShadowBias, in that case we don't want this adjustable
		}
		else
		{
			// spot lights (old code, might need to be improved)
			const float LightTypeDepthBias = CVarSpotLightShadowDepthBias.GetValueOnRenderThread();
			DepthBias = LightTypeDepthBias * 512.0f / ((MaxSubjectZ - MinSubjectZ) * FMath::Max(ResolutionX, ResolutionY));
			// * 2.0f to be compatible with the system we had before ShadowBias
			DepthBias *= 2.0f * LightSceneInfo->Proxy->GetUserShadowBias();
		}
	}

	ShaderDepthBias = FMath::Max(DepthBias, 0.0f);
}

float FProjectedShadowInfo::ComputeTransitionSize() const
{
	float TransitionSize = 1;

	if (IsWholeScenePointLightShadow())
	{
		// todo: optimize
		TransitionSize = bDirectionalLight ? (1.0f / CVarShadowTransitionScale.GetValueOnRenderThread()) : (1.0f / CVarSpotLightShadowTransitionScale.GetValueOnRenderThread());
		// * 2.0f to be compatible with the system we had before ShadowBias
		TransitionSize *= 2.0f * LightSceneInfo->Proxy->GetUserShadowBias();
	}
	else if (IsWholeSceneDirectionalShadow())
	{
		check(CascadeSettings.ShadowSplitIndex >= 0);

		// todo: remove GetShadowTransitionScale()
		// make 1/ ShadowTransitionScale, SpotLightShadowTransitionScale

		// the z range is adjusted to we need to adjust here as well
		TransitionSize = CVarCSMShadowDepthBias.GetValueOnRenderThread() / (MaxSubjectZ - MinSubjectZ);

		float WorldSpaceTexelScale = ShadowBounds.W / ResolutionX;

		TransitionSize *= WorldSpaceTexelScale;
		TransitionSize *= LightSceneInfo->Proxy->GetUserShadowBias();
	}
	else if (bPreShadow)
	{
		// Preshadows don't have self shadowing, so make sure the shadow starts as close to the caster as possible
		TransitionSize = 0.00001f;
	}
	else
	{
		// todo: optimize
		TransitionSize = bDirectionalLight ? (1.0f / CVarShadowTransitionScale.GetValueOnRenderThread()) : (1.0f / CVarSpotLightShadowTransitionScale.GetValueOnRenderThread());
		// * 2.0f to be compatible with the system we had before ShadowBias
		TransitionSize *= 2.0f * LightSceneInfo->Proxy->GetUserShadowBias();
	}

	return TransitionSize;
}

void FProjectedShadowInfo::SortSubjectMeshElements()
{
	// Note: this should match the criteria in FProjectedShadowInfo::RenderDepth for deciding when to call SetSharedState on a static mesh element for best performance
	struct FCompareFShadowStaticMeshElement
	{
		FORCEINLINE bool operator()( const FShadowStaticMeshElement& A, const FShadowStaticMeshElement& B ) const
		{
			if( A.Mesh->VertexFactory != B.Mesh->VertexFactory ) return A.Mesh->VertexFactory < B.Mesh->VertexFactory;
			if( A.RenderProxy != B.RenderProxy ) return A.RenderProxy < B.RenderProxy;
			if( A.bIsTwoSided != B.bIsTwoSided ) return A.bIsTwoSided < B.bIsTwoSided;
			if( A.Mesh->ReverseCulling != B.Mesh->ReverseCulling ) return A.Mesh->ReverseCulling < B.Mesh->ReverseCulling;

			return false;
		}
	};

	SubjectMeshElements.Sort( FCompareFShadowStaticMeshElement() );
}

void FProjectedShadowInfo::GetShadowTypeNameForDrawEvent(FString& TypeName) const
{
	if (GEmitDrawEvents)
	{
		const FName ParentName = ParentSceneInfo ? ParentSceneInfo->Proxy->GetOwnerName() : NAME_None;

		if (bWholeSceneShadow)
		{
			if (CascadeSettings.ShadowSplitIndex >= 0)
			{
				TypeName = FString(TEXT("WholeScene split ")) + FString::FromInt(CascadeSettings.ShadowSplitIndex);
			}
			else
			{
				TypeName = FString(TEXT("WholeScene"));
			}
		}
		else if (bPreShadow)
		{
			TypeName = FString(TEXT("PreShadow ")) + ParentName.ToString();
		}
		else
		{
			TypeName = FString(TEXT("PerObject ")) + ParentName.ToString();
		}
	}
}

/*-----------------------------------------------------------------------------
FDeferredShadingSceneRenderer
-----------------------------------------------------------------------------*/

/**
 * Used by RenderLights to figure out if projected shadows need to be rendered to the attenuation buffer.
 *
 * @param LightSceneInfo Represents the current light
 * @return true if anything needs to be rendered
 */
bool FSceneRenderer::CheckForProjectedShadows( const FLightSceneInfo* LightSceneInfo ) const
{
	// Find the projected shadows cast by this light.
	const FVisibleLightInfo& VisibleLightInfo = VisibleLightInfos[LightSceneInfo->Id];
	for( int32 ShadowIndex=0; ShadowIndex<VisibleLightInfo.AllProjectedShadows.Num(); ShadowIndex++ )
	{
		const FProjectedShadowInfo* ProjectedShadowInfo = VisibleLightInfo.AllProjectedShadows[ShadowIndex];

		// Check that the shadow is visible in at least one view before rendering it.
		bool bShadowIsVisible = false;
		for(int32 ViewIndex = 0;ViewIndex < Views.Num();ViewIndex++)
		{
			const FViewInfo& View = Views[ViewIndex];
			if (ProjectedShadowInfo->DependentView && ProjectedShadowInfo->DependentView != &View)
			{
				continue;
			}
			const FVisibleLightViewInfo& VisibleLightViewInfo = View.VisibleLightInfos[LightSceneInfo->Id];
			bShadowIsVisible |= VisibleLightViewInfo.ProjectedShadowVisibilityMap[ShadowIndex];
		}

		if(bShadowIsVisible)
		{
			return true;
		}
	}
	return false;
}

/** Renders one pass point light shadows. */
bool FDeferredShadingSceneRenderer::RenderOnePassPointLightShadows(FRHICommandListImmediate& RHICmdList, const FLightSceneInfo* LightSceneInfo, bool bRenderedTranslucentObjectShadows, bool& bInjectedTranslucentVolume)
{
	bool bDirty = false;
	FVisibleLightInfo& VisibleLightInfo = VisibleLightInfos[LightSceneInfo->Id];

	for (int32 ShadowIndex = 0; ShadowIndex < VisibleLightInfo.AllProjectedShadows.Num(); ShadowIndex++)
	{
		FProjectedShadowInfo* ProjectedShadowInfo = VisibleLightInfo.AllProjectedShadows[ShadowIndex];

		// Check that the shadow is visible in at least one view before rendering it.
		bool bShadowIsVisible = false;
		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			const FViewInfo& View = Views[ViewIndex];
			const FVisibleLightViewInfo& VisibleLightViewInfo = View.VisibleLightInfos[LightSceneInfo->Id];
			const FPrimitiveViewRelevance ViewRelevance = VisibleLightViewInfo.ProjectedShadowViewRelevanceMap[ShadowIndex];
			bShadowIsVisible |= (ViewRelevance.bOpaqueRelevance	&& VisibleLightViewInfo.ProjectedShadowVisibilityMap[ShadowIndex]);
		}

		if (bShadowIsVisible && ProjectedShadowInfo->CascadeSettings.bOnePassPointLightShadow)
		{
			INC_DWORD_STAT(STAT_WholeSceneShadows);

			if (!ProjectedShadowInfo->CascadeSettings.bRayTracedDistanceField)
			{
				SCOPED_DRAW_EVENT(RHICmdList, ShadowDepthsFromOpaquePointLight);

				bool bPerformClear = true;
				auto SetShadowRenderTargets = [this, &bPerformClear, ProjectedShadowInfo](FRHICommandList& InRHICmdList)
				{
					GSceneRenderTargets.BeginRenderingCubeShadowDepth(InRHICmdList, ProjectedShadowInfo->ResolutionX);
					ProjectedShadowInfo->ClearDepth(InRHICmdList, this, bPerformClear);
				};

				ProjectedShadowInfo->bAllocated = true;
				SetShadowRenderTargets(RHICmdList);  // run it now, maybe run it later for parallel command lists								
				bPerformClear = false;

				ProjectedShadowInfo->RenderDepth(RHICmdList, this, SetShadowRenderTargets);
				GSceneRenderTargets.FinishRenderingCubeShadowDepth(RHICmdList, ProjectedShadowInfo->ResolutionX);
			}

			{
				GSceneRenderTargets.BeginRenderingLightAttenuation(RHICmdList);

				SCOPED_DRAW_EVENT(RHICmdList, ShadowProjectionOnOpaque);

				for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
				{
					SCOPED_CONDITIONAL_DRAW_EVENTF(RHICmdList, EventView, Views.Num() > 1, TEXT("View%d"), ViewIndex);

					const FViewInfo& View = Views[ViewIndex];

					// Set the device viewport for the view.
					RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0.0f, View.ViewRect.Max.X, View.ViewRect.Max.Y, 1.0f);
					
					if (ProjectedShadowInfo->CascadeSettings.bRayTracedDistanceField)
					{
						ProjectedShadowInfo->RenderRayTracedDistanceFieldProjection(RHICmdList, View);
					}
					else
					{
						ProjectedShadowInfo->RenderOnePassPointLightProjection(RHICmdList, ViewIndex, View);
					}
				}
			}

			// Don't inject shadowed lighting with whole scene shadows used for previewing a light with static shadows,
			// Since that would cause a mismatch with the built lighting
			if (!LightSceneInfo->Proxy->HasStaticShadowing() && !ProjectedShadowInfo->CascadeSettings.bRayTracedDistanceField)
			{
				bInjectedTranslucentVolume = true;
				SCOPED_DRAW_EVENT(RHICmdList, InjectTranslucentVolume);
				// Inject the shadowed light into the translucency lighting volumes
				InjectTranslucentVolumeLighting(RHICmdList, *LightSceneInfo, ProjectedShadowInfo);
			}

			bDirty = true;
		}
	}
	
	return bDirty;
}

/** Renders the projections of the given Shadows to the appropriate color render target. */
void FDeferredShadingSceneRenderer::RenderProjections(
	FRHICommandListImmediate& RHICmdList,
	const FLightSceneInfo* LightSceneInfo, 
	const TArray<FProjectedShadowInfo*,SceneRenderingAllocator>& Shadows
	)
{
	// Normal shadows render to light attenuation
	GSceneRenderTargets.BeginRenderingLightAttenuation(RHICmdList);

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		SCOPED_CONDITIONAL_DRAW_EVENTF(RHICmdList, EventView, Views.Num() > 1, TEXT("View%d"), ViewIndex);

		const FViewInfo& View = Views[ViewIndex];

		// Set the device viewport for the view.
		RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0.0f, View.ViewRect.Max.X, View.ViewRect.Max.Y, 1.0f);

		// Set the light's scissor rectangle.
		LightSceneInfo->Proxy->SetScissorRect(RHICmdList, View);

		// Project the shadow depth buffers onto the scene.
		for (int32 ShadowIndex = 0; ShadowIndex < Shadows.Num(); ShadowIndex++)
		{
			FProjectedShadowInfo* ProjectedShadowInfo = Shadows[ShadowIndex];
			if (ProjectedShadowInfo->bAllocated)
			{
				// Only project the shadow if it's large enough in this particular view (split screen, etc... may have shadows that are large in one view but irrelevantly small in others)
				if (ProjectedShadowInfo->FadeAlphas[ViewIndex] > 1.0f / 256.0f)
				{
					ProjectedShadowInfo->RenderProjection(RHICmdList, ViewIndex, &View);

					GRenderTargetPool.VisualizeTexture.SetCheckPoint(RHICmdList, GSceneRenderTargets.GetLightAttenuation());
				}
			}
		}

		// Reset the scissor rectangle.
		RHICmdList.SetScissorRect(false, 0, 0, 0, 0);
	}
}

// Sort by descending resolution
struct FCompareFProjectedShadowInfoByResolution
{
	FORCEINLINE bool operator()( const FProjectedShadowInfo& A, const FProjectedShadowInfo& B ) const
	{
		return (B.ResolutionX*B.ResolutionY < A.ResolutionX*A.ResolutionY);
	}
};

// Sort by shadow type (CSMs first, then other types).
// Then sort CSMs by descending split index, and other shadows by resolution.
// Used to render shadow cascades in far to near order, whilst preserving the
// descending resolution sort behavior for other shadow types.
struct FCompareFProjectedShadowInfoBySplitIndex
{
	FORCEINLINE bool operator()( const FProjectedShadowInfo& A, const FProjectedShadowInfo& B ) const
	{
		if (A.IsWholeSceneDirectionalShadow())
		{
			if (B.IsWholeSceneDirectionalShadow())
			{
				// Both A and B are CSMs
				// Compare Split Indexes, to order them far to near.
				return (B.CascadeSettings.ShadowSplitIndex < A.CascadeSettings.ShadowSplitIndex);
			}

			// A is a CSM, B is per-object shadow etc.
			// B should be rendered after A.
			return true;
		}
		else
		{
			if (B.IsWholeSceneDirectionalShadow())
			{
				// B should be rendered after A.
				return false;
			}
			
			// Neither shadow is a CSM
			// Sort by descending resolution.
			return FCompareFProjectedShadowInfoByResolution()(A, B);
		}
	}
};

bool FDeferredShadingSceneRenderer::RenderTranslucentProjectedShadows(FRHICommandListImmediate& RHICmdList, const FLightSceneInfo* LightSceneInfo)
{
	SCOPE_CYCLE_COUNTER(STAT_ProjectedShadowDrawTime);

	bool bRenderedToVolumeObjectShadowing = false;

	// Find the projected shadows cast by this light.
	FVisibleLightInfo& VisibleLightInfo = VisibleLightInfos[LightSceneInfo->Id];
	TArray<FProjectedShadowInfo*, SceneRenderingAllocator> Shadows;
	for (int32 ShadowIndex = 0; ShadowIndex < VisibleLightInfo.AllProjectedShadows.Num(); ShadowIndex++)
	{
		FProjectedShadowInfo* ProjectedShadowInfo = VisibleLightInfo.AllProjectedShadows[ShadowIndex];

		// Check that the shadow is visible in at least one view before rendering it.
		bool bShadowIsVisible = false;
		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			const FViewInfo& View = Views[ViewIndex];
			if (ProjectedShadowInfo->DependentView && ProjectedShadowInfo->DependentView != &View)
			{
				continue;
			}
			const FVisibleLightViewInfo& VisibleLightViewInfo = View.VisibleLightInfos[LightSceneInfo->Id];
			const FPrimitiveViewRelevance ViewRelevance = VisibleLightViewInfo.ProjectedShadowViewRelevanceMap[ShadowIndex];

			// Only operating on translucent shadows in this pass
			bShadowIsVisible |= ProjectedShadowInfo->bTranslucentShadow && ViewRelevance.HasTranslucency()
				&& VisibleLightViewInfo.ProjectedShadowVisibilityMap[ShadowIndex];
		}

		// Skip shadows which will be handled in RenderOnePassPointLightShadows
		if (ProjectedShadowInfo->CascadeSettings.bOnePassPointLightShadow)
		{
			bShadowIsVisible = false;
		}

		if (bShadowIsVisible)
		{
			Shadows.Add(ProjectedShadowInfo);
		}
	}
	
	// Sort the projected shadows by split index.
	Shadows.Sort( FCompareFProjectedShadowInfoBySplitIndex() );

	for (int32 ShadowIndex = 0; ShadowIndex < Shadows.Num(); ShadowIndex++)
	{
		FProjectedShadowInfo* ProjectedShadowInfo = Shadows[ShadowIndex];
		check(ProjectedShadowInfo->bTranslucentShadow);
		ProjectedShadowInfo->bRendered = false;
	}

	if (Shadows.Num() > 0)
	{
		// Record this light as being the only light which can use TranslucentSelfShadowLayout
		CachedTranslucentSelfShadowLightId = LightSceneInfo->Id;
	}

	int32 NumShadowsRendered = 0;

	while (NumShadowsRendered < Shadows.Num())
	{
		for (int32 ShadowIndex = 0; ShadowIndex < Shadows.Num(); ShadowIndex++)
		{
			FProjectedShadowInfo* ProjectedShadowInfo = Shadows[ShadowIndex];
			// Clear any existing allocations
			ProjectedShadowInfo->bAllocatedInTranslucentLayout = false;
		}

		int32 NumAllocatedShadows = 0;

		// Reset the translucent shadow allocations
		TranslucentSelfShadowLayout = FTextureLayout(1, 1, GSceneRenderTargets.GetTranslucentShadowDepthTextureResolution().X, GSceneRenderTargets.GetTranslucentShadowDepthTextureResolution().Y, false, false);

		for (int32 ShadowIndex = 0; ShadowIndex < Shadows.Num(); ShadowIndex++)
		{
			FProjectedShadowInfo* ProjectedShadowInfo = Shadows[ShadowIndex];

			if (!ProjectedShadowInfo->bRendered)
			{
				// Try to allocate this shadow in the layout
				if (TranslucentSelfShadowLayout.AddElement(
					ProjectedShadowInfo->X,
					ProjectedShadowInfo->Y,
					ProjectedShadowInfo->ResolutionX + SHADOW_BORDER * 2,
					ProjectedShadowInfo->ResolutionY + SHADOW_BORDER * 2)
					)
				{
					ProjectedShadowInfo->bAllocated = true;
					ProjectedShadowInfo->bAllocatedInTranslucentLayout = true;
					NumAllocatedShadows++;
				}
			}
		}

		// Abort if we encounter a shadow that doesn't fit in the render target.
		if (!NumAllocatedShadows)
		{
			break;
		}

		{
			// Render the translucent shadow depths.
			SCOPED_DRAW_EVENT(RHICmdList, TranslucencyShadowMapGeneration);

			for (int32 ShadowIndex = 0; ShadowIndex < Shadows.Num(); ShadowIndex++)
			{
				FProjectedShadowInfo* ProjectedShadowInfo = Shadows[ShadowIndex];

				if (ProjectedShadowInfo->bAllocated)
				{
					ProjectedShadowInfo->RenderTranslucencyDepths(RHICmdList, this);
				}
			}
		}

		// Render the shadow projections.
		{
			{
				SCOPED_DRAW_EVENT(RHICmdList, ShadowProjectionOnOpaque);
				RenderProjections(RHICmdList, LightSceneInfo, Shadows);
			}

			for (int32 ShadowIndex = 0; ShadowIndex < Shadows.Num(); ShadowIndex++)
			{
				FProjectedShadowInfo* ProjectedShadowInfo = Shadows[ShadowIndex];

				if (ProjectedShadowInfo->bAllocated 
					&& !ProjectedShadowInfo->bDirectionalLight)
				{
					// Accumulate this object shadow's contribution on the translucency lighting volume
					AccumulateTranslucentVolumeObjectShadowing(RHICmdList, ProjectedShadowInfo, !bRenderedToVolumeObjectShadowing);
					bRenderedToVolumeObjectShadowing = true;
				}
			}

			// Mark and count the rendered shadows.
			for (int32 ShadowIndex = 0; ShadowIndex < Shadows.Num(); ShadowIndex++)
			{
				FProjectedShadowInfo* ProjectedShadowInfo = Shadows[ShadowIndex];
				if (ProjectedShadowInfo->bAllocated)
				{
					ProjectedShadowInfo->bAllocated = false;
					ProjectedShadowInfo->bRendered = true;
					NumShadowsRendered++;
				}
			}
		}
	}

	return bRenderedToVolumeObjectShadowing;
}

/**
* Used by RenderLights to render reflective shadow maps for the LightPropagationVolume
*
* @param LightSceneInfo Represents the current light
* @return true if anything got rendered
*/
bool FDeferredShadingSceneRenderer::RenderReflectiveShadowMaps(FRHICommandListImmediate& RHICmdList, const FLightSceneInfo* LightSceneInfo)
{
	SCOPE_CYCLE_COUNTER(STAT_ReflectiveShadowMapDrawTime);
	
	if (FeatureLevel < ERHIFeatureLevel::SM5)
	{
		return false;
	}

	// Find the projected shadows cast by this light.
	FVisibleLightInfo& VisibleLightInfo = VisibleLightInfos[LightSceneInfo->Id];
	TArray<FProjectedShadowInfo*, SceneRenderingAllocator> Shadows;
	for (int32 ShadowIndex = 0; ShadowIndex < VisibleLightInfo.ReflectiveShadowMaps.Num(); ShadowIndex++)
	{
		FProjectedShadowInfo* ProjectedShadowInfo = VisibleLightInfo.ReflectiveShadowMaps[ShadowIndex];

		// Check that the shadow is visible in at least one view before rendering it.
		bool bShadowIsVisible = false;
		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			const FViewInfo& View = Views[ViewIndex];
			if (ProjectedShadowInfo->DependentView && ProjectedShadowInfo->DependentView != &View)
			{
				continue;
			}
			const FVisibleLightViewInfo& VisibleLightViewInfo = View.VisibleLightInfos[LightSceneInfo->Id];
			const FPrimitiveViewRelevance ViewRelevance = VisibleLightViewInfo.ProjectedShadowViewRelevanceMap[ShadowIndex];
			bShadowIsVisible |= !ProjectedShadowInfo->bTranslucentShadow && ViewRelevance.bOpaqueRelevance 
				&& VisibleLightViewInfo.ProjectedShadowVisibilityMap[ShadowIndex];
		}

		// Skip shadows which are not supported
		if (!ProjectedShadowInfo->bWholeSceneShadow)
		{
			bShadowIsVisible = false;
		}

		if (ProjectedShadowInfo->bPreShadow && !LightSceneInfo->Proxy->HasStaticShadowing())
		{
			bShadowIsVisible = false;
		}

		if (bShadowIsVisible
			&& (!ProjectedShadowInfo->bPreShadow || ProjectedShadowInfo->HasSubjectPrims())
			&& !ProjectedShadowInfo->bAllocatedInPreshadowCache)
		{
			// Add the shadow to the list of visible shadows cast by this light.
#if STATS
			INC_DWORD_STAT(STAT_ReflectiveShadowMaps);
#endif
			Shadows.Add(ProjectedShadowInfo);
		}
	}

	for (int32 ShadowIndex = 0; ShadowIndex < Shadows.Num(); ShadowIndex++)
	{
		FProjectedShadowInfo* ProjectedShadowInfo = Shadows[ShadowIndex];
		check(!ProjectedShadowInfo->bTranslucentShadow);
		ProjectedShadowInfo->bRendered = false;
	}

	int32 NumShadowsRendered	= 0;

	while (NumShadowsRendered < Shadows.Num())
	{
		int32 NumAllocatedShadows = 0;

		// Allocate shadow texture space to the shadows.
		//@TODO: Is this code all necessary? 
		const FIntPoint ShadowBufferResolution = GSceneRenderTargets.GetShadowDepthTextureResolution();
		FTextureLayout ShadowLayout(1, 1, ShadowBufferResolution.X, ShadowBufferResolution.Y, false, false);

		for (int32 ShadowIndex = 0; ShadowIndex < Shadows.Num(); ShadowIndex++)
		{
			FProjectedShadowInfo* ProjectedShadowInfo = Shadows[ShadowIndex];
			if (!ProjectedShadowInfo->bRendered)
			{
				if (ShadowLayout.AddElement(
					ProjectedShadowInfo->X,
					ProjectedShadowInfo->Y,
					ProjectedShadowInfo->ResolutionX,
					ProjectedShadowInfo->ResolutionY)
					)
				{
					ProjectedShadowInfo->bAllocated = true;
					NumAllocatedShadows++;
				}
			}
		}

		// Abort if we encounter a shadow that doesn't fit in the render target.
		if (!NumAllocatedShadows)
		{
			break;
		}


		{
			// Render the shadow depths.
			SCOPED_DRAW_EVENT(RHICmdList, ReflectiveShadowMapsFromOpaque);

			// render the RSMs
			for (int32 ShadowIndex = 0; ShadowIndex < Shadows.Num(); ShadowIndex++)
			{
				FProjectedShadowInfo* ProjectedShadowInfo = Shadows[ShadowIndex];
				FSceneViewState* ViewState = (FSceneViewState*)ProjectedShadowInfo->DependentView->State;

				FLightPropagationVolume* LightPropagationVolume = ViewState->GetLightPropagationVolume();

				check(LightPropagationVolume);
				auto SetShadowRenderTargets = [this, LightPropagationVolume, ProjectedShadowInfo](FRHICommandList& InRHICmdList)
				{
					GSceneRenderTargets.BeginRenderingReflectiveShadowMap(InRHICmdList, LightPropagationVolume);
					ProjectedShadowInfo->ClearDepth(InRHICmdList, this, false);
				};
				SetShadowRenderTargets(RHICmdList);  // run it now, maybe run it later for parallel command lists				

				LightPropagationVolume->SetVplInjectionConstants( *ProjectedShadowInfo, LightSceneInfo->Proxy );

				if (ProjectedShadowInfo->bAllocated && !ProjectedShadowInfo->bTranslucentShadow)
				{
					ProjectedShadowInfo->ClearDepth(RHICmdList, this, true);
					ProjectedShadowInfo->RenderDepth(RHICmdList, this, SetShadowRenderTargets);
				}

				GSceneRenderTargets.FinishRenderingReflectiveShadowMap(RHICmdList);
			}
		}

		// Inject the RSM into the LPVs
		for (int32 ShadowIndex = 0; ShadowIndex < Shadows.Num(); ShadowIndex++)
		{
			FProjectedShadowInfo* ProjectedShadowInfo = Shadows[ShadowIndex];

			if ( ProjectedShadowInfo->bAllocated && ProjectedShadowInfo->DependentView )
			{
				FSceneViewState* ViewState = (FSceneViewState*)ProjectedShadowInfo->DependentView->State;

				FLightPropagationVolume* LightPropagationVolume = ViewState ? ViewState->GetLightPropagationVolume() : NULL;

				if ( LightPropagationVolume )
				{
					if ( ProjectedShadowInfo->bWholeSceneShadow )
					{
						LightPropagationVolume->InjectDirectionalLightRSM( 
							RHICmdList, 
							*ProjectedShadowInfo->DependentView,
							GSceneRenderTargets.GetReflectiveShadowMapDiffuseTexture(), 
							GSceneRenderTargets.GetReflectiveShadowMapNormalTexture(),
							GSceneRenderTargets.GetReflectiveShadowMapDepthTexture(),
							*ProjectedShadowInfo, 
							LightSceneInfo->Proxy->GetColor() );
					}
				}
			}
		}

		// Mark and count the rendered shadows.
		for (int32 ShadowIndex = 0; ShadowIndex < Shadows.Num(); ShadowIndex++)
		{
			FProjectedShadowInfo* ProjectedShadowInfo = Shadows[ShadowIndex];
			if (ProjectedShadowInfo->bAllocated)
			{
				ProjectedShadowInfo->bAllocated = false;
				ProjectedShadowInfo->bRendered = true;
				NumShadowsRendered++;
			}
		}
	}
	return true;
}

/**
 * Used by RenderLights to render shadows to the attenuation buffer.
 *
 * @param LightSceneInfo Represents the current light
 * @return true if anything got rendered
 */
bool FDeferredShadingSceneRenderer::RenderProjectedShadows(FRHICommandListImmediate& RHICmdList, const FLightSceneInfo* LightSceneInfo, bool bRenderedTranslucentObjectShadows, bool& bInjectedTranslucentVolume)
{
	SCOPE_CYCLE_COUNTER(STAT_ProjectedShadowDrawTime);

	bool bAttenuationBufferDirty = false;

	// Find the projected shadows cast by this light.
	FVisibleLightInfo& VisibleLightInfo = VisibleLightInfos[LightSceneInfo->Id];
	TArray<FProjectedShadowInfo*, SceneRenderingAllocator> Shadows;
	for (int32 ShadowIndex = 0; ShadowIndex < VisibleLightInfo.AllProjectedShadows.Num(); ShadowIndex++)
	{
		FProjectedShadowInfo* ProjectedShadowInfo = VisibleLightInfo.AllProjectedShadows[ShadowIndex];

		// Check that the shadow is visible in at least one view before rendering it.
		bool bShadowIsVisible = false;
		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			const FViewInfo& View = Views[ViewIndex];
			if (ProjectedShadowInfo->DependentView && ProjectedShadowInfo->DependentView != &View)
			{
				continue;
			}
			const FVisibleLightViewInfo& VisibleLightViewInfo = View.VisibleLightInfos[LightSceneInfo->Id];
			const FPrimitiveViewRelevance ViewRelevance = VisibleLightViewInfo.ProjectedShadowViewRelevanceMap[ShadowIndex];
			bShadowIsVisible |= !ProjectedShadowInfo->bTranslucentShadow && ViewRelevance.bOpaqueRelevance 
				&& VisibleLightViewInfo.ProjectedShadowVisibilityMap[ShadowIndex];
		}

		// Skip shadows which will be handled in RenderOnePassPointLightShadows or RenderReflectiveShadowmaps
		if (ProjectedShadowInfo->CascadeSettings.bOnePassPointLightShadow || ProjectedShadowInfo->bReflectiveShadowmap)
		{
			bShadowIsVisible = false;
		}

		if (ProjectedShadowInfo->bPreShadow && !LightSceneInfo->Proxy->HasStaticShadowing())
		{
			bShadowIsVisible = false;
		}

		if (bShadowIsVisible
			&& (!ProjectedShadowInfo->bPreShadow || ProjectedShadowInfo->HasSubjectPrims())
			&& !ProjectedShadowInfo->bAllocatedInPreshadowCache)
		{
			// Add the shadow to the list of visible shadows cast by this light.
			if (ProjectedShadowInfo->bWholeSceneShadow)
			{
				INC_DWORD_STAT(STAT_WholeSceneShadows);
			}
			else if (ProjectedShadowInfo->bPreShadow)
			{
				INC_DWORD_STAT(STAT_PreShadows);
			}
			else
			{
				INC_DWORD_STAT(STAT_PerObjectShadows);
			}
			Shadows.Add(ProjectedShadowInfo);
		}
	}

	// Sort the projected shadows by resolution.
	Shadows.Sort( FCompareFProjectedShadowInfoBySplitIndex() );

	for (int32 ShadowIndex = 0; ShadowIndex < Shadows.Num(); ShadowIndex++)
	{
		FProjectedShadowInfo* ProjectedShadowInfo = Shadows[ShadowIndex];
		check(!ProjectedShadowInfo->bTranslucentShadow);
		ProjectedShadowInfo->bRendered = false;
	}

	int32 NumShadowsRendered = 0;

	while (NumShadowsRendered < Shadows.Num())
	{
		int32 NumAllocatedShadows = 0;

		// Allocate shadow texture space to the shadows.
		const FIntPoint ShadowBufferResolution = GSceneRenderTargets.GetShadowDepthTextureResolution();
		FTextureLayout ShadowLayout(1, 1, ShadowBufferResolution.X, ShadowBufferResolution.Y, false, false);

		for (int32 ShadowIndex = 0; ShadowIndex < Shadows.Num(); ShadowIndex++)
		{
			FProjectedShadowInfo* ProjectedShadowInfo = Shadows[ShadowIndex];
			if (!ProjectedShadowInfo->bRendered)
			{
				if (ShadowLayout.AddElement(
					ProjectedShadowInfo->X,
					ProjectedShadowInfo->Y,
					ProjectedShadowInfo->ResolutionX + SHADOW_BORDER * 2,
					ProjectedShadowInfo->ResolutionY + SHADOW_BORDER * 2)
					)
				{
					ProjectedShadowInfo->bAllocated = true;
					NumAllocatedShadows++;
				}
			}
		}

		// Abort if we encounter a shadow that doesn't fit in the render target.
		if (!NumAllocatedShadows)
		{
			break;
		}

		{
			// Render the shadow depths.
			SCOPED_DRAW_EVENT(RHICmdList, ShadowDepthsFromOpaqueProjected);

			bool bPerformClear = true;
			auto SetShadowRenderTargets = [&bPerformClear](FRHICommandList& InRHICmdList)
			{
				GSceneRenderTargets.BeginRenderingShadowDepth(InRHICmdList, bPerformClear);
			};

			SetShadowRenderTargets(RHICmdList);  // run it now, maybe run it later for parallel command lists
			bPerformClear = false;

			// render depth for each shadow
			for (int32 ShadowIndex = 0; ShadowIndex < Shadows.Num(); ShadowIndex++)
			{
				FProjectedShadowInfo* ProjectedShadowInfo = Shadows[ShadowIndex];
				if (ProjectedShadowInfo->bAllocated && !ProjectedShadowInfo->bTranslucentShadow && !ProjectedShadowInfo->CascadeSettings.bRayTracedDistanceField)
				{
					ProjectedShadowInfo->RenderDepth(RHICmdList, this, SetShadowRenderTargets);
				}
			}

			GSceneRenderTargets.FinishRenderingShadowDepth(RHICmdList);
		}

		// Render the shadow projections.
		{
			{
				SCOPED_DRAW_EVENT(RHICmdList, ShadowProjectionOnOpaque);
				RenderProjections(RHICmdList, LightSceneInfo, Shadows);
			}
			
			for (int32 ShadowIndex = 0; ShadowIndex < Shadows.Num(); ShadowIndex++)
			{
				FProjectedShadowInfo* ProjectedShadowInfo = Shadows[ShadowIndex];

				if (ProjectedShadowInfo->bAllocated
					&& ProjectedShadowInfo->bWholeSceneShadow
					// Not supported on translucency yet
					&& !ProjectedShadowInfo->CascadeSettings.bRayTracedDistanceField
					// Don't inject shadowed lighting with whole scene shadows used for previewing a light with static shadows,
					// Since that would cause a mismatch with the built lighting
					// However, stationary directional lights allow whole scene shadows that blend with precomputed shadowing
					&& (!LightSceneInfo->Proxy->HasStaticShadowing() || ProjectedShadowInfo->IsWholeSceneDirectionalShadow()))
				{
					bInjectedTranslucentVolume = true;
					SCOPED_DRAW_EVENT(RHICmdList, InjectTranslucentVolume);
					// Inject the shadowed light into the translucency lighting volumes
					InjectTranslucentVolumeLighting(RHICmdList, *LightSceneInfo, ProjectedShadowInfo);
				}
			}

			
			for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
			{
				const FViewInfo& View = Views[ViewIndex];

				for (int32 ShadowIndex = 0; ShadowIndex < Shadows.Num(); ShadowIndex++)
				{
					FProjectedShadowInfo* ProjectedShadowInfo = Shadows[ShadowIndex];

					if (ProjectedShadowInfo->bAllocated
						&& ProjectedShadowInfo->bWholeSceneShadow)
					{
						View.HeightfieldLightingViewInfo.ComputeShadowMapShadowing(View, RHICmdList, ProjectedShadowInfo);
					}
				}
			}

			// Mark and count the rendered shadows.
			for (int32 ShadowIndex = 0; ShadowIndex < Shadows.Num(); ShadowIndex++)
			{
				FProjectedShadowInfo* ProjectedShadowInfo = Shadows[ShadowIndex];
				if (ProjectedShadowInfo->bAllocated)
				{
					ProjectedShadowInfo->bAllocated = false;
					ProjectedShadowInfo->bRendered = true;
					NumShadowsRendered++;
				}
			}

			// Mark the attenuation buffer as dirty.
			bAttenuationBufferDirty = true;
		}
	}

	bAttenuationBufferDirty |= RenderCachedPreshadows(RHICmdList, LightSceneInfo);

	if (Scene->GetFeatureLevel() >= ERHIFeatureLevel::SM4)
	{
		bAttenuationBufferDirty |= RenderOnePassPointLightShadows(RHICmdList, LightSceneInfo, bRenderedTranslucentObjectShadows, bInjectedTranslucentVolume);
	}

	return bAttenuationBufferDirty;
}

/** Renders preshadow depths for any preshadows whose depths aren't cached yet, and renders the projections of preshadows with opaque relevance. */
bool FDeferredShadingSceneRenderer::RenderCachedPreshadows(FRHICommandListImmediate& RHICmdList, const FLightSceneInfo* LightSceneInfo)
{
	bool bAttenuationBufferDirty = false;

	FVisibleLightInfo& VisibleLightInfo = VisibleLightInfos[LightSceneInfo->Id];
	TArray<FProjectedShadowInfo*, SceneRenderingAllocator> CachedPreshadows;
	TArray<FProjectedShadowInfo*, SceneRenderingAllocator> OpaqueCachedPreshadows;
	bool bHasDepthsToRender = false;

	for (int32 ShadowIndex = 0; ShadowIndex < VisibleLightInfo.ProjectedPreShadows.Num(); ShadowIndex++)
	{
		FProjectedShadowInfo* ProjectedShadowInfo = VisibleLightInfo.ProjectedPreShadows[ShadowIndex];

		// Check that the shadow is visible in at least one view before rendering it.
		bool bShadowIsVisible = false;
		bool bOpaqueRelevance = false;

		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			const FViewInfo& View = Views[ViewIndex];
			const FVisibleLightViewInfo& VisibleLightViewInfo = View.VisibleLightInfos[LightSceneInfo->Id];
			const FPrimitiveViewRelevance ViewRelevance = VisibleLightViewInfo.ProjectedShadowViewRelevanceMap[ProjectedShadowInfo->ShadowId];

			bShadowIsVisible |= VisibleLightViewInfo.ProjectedShadowVisibilityMap[ProjectedShadowInfo->ShadowId];
			bOpaqueRelevance |= ViewRelevance.bOpaqueRelevance;
		}

		if (ProjectedShadowInfo->bPreShadow && !LightSceneInfo->Proxy->HasStaticShadowing())
		{
			bShadowIsVisible = false;
		}

		if (ProjectedShadowInfo->bAllocatedInPreshadowCache && bShadowIsVisible)
		{
			if (ProjectedShadowInfo->bDepthsCached)
			{
				INC_DWORD_STAT(STAT_CachedPreShadows);
			}
			else
			{
				INC_DWORD_STAT(STAT_PreShadows);
			}

			// Build the list of cached preshadows which need their depths rendered now
			CachedPreshadows.Add(ProjectedShadowInfo);
			bHasDepthsToRender |= !ProjectedShadowInfo->bDepthsCached;

			if (bOpaqueRelevance)
			{
				// Build the list of cached preshadows which need their projections rendered, which is the subset of preshadows affecting opaque materials
				OpaqueCachedPreshadows.Add(ProjectedShadowInfo);
			}
		}
	}

	if (CachedPreshadows.Num() > 0)
	{
		if (bHasDepthsToRender)
		{
			SCOPED_DRAW_EVENTF(RHICmdList, EventShadowDepths, TEXT("Preshadow Cache Depths"));

			// Render depth for each shadow
			for (int32 ShadowIndex = 0; ShadowIndex < CachedPreshadows.Num(); ShadowIndex++)
			{
				FProjectedShadowInfo* ProjectedShadowInfo = CachedPreshadows[ShadowIndex];
				// Only render depths for shadows which haven't already cached their depths
				if (!ProjectedShadowInfo->bDepthsCached)
				{
					check(ProjectedShadowInfo->bAllocated);
					
					//
					bool bPerformClear = true;;
					auto SetShadowRenderTargets = [this, &bPerformClear, ProjectedShadowInfo](FRHICommandList& InRHICmdList)
					{
						SetRenderTarget(InRHICmdList, FTextureRHIRef(), GSceneRenderTargets.PreShadowCacheDepthZ->GetRenderTargetItem().TargetableTexture);
						ProjectedShadowInfo->ClearDepth(InRHICmdList, this, bPerformClear);
					};					
					SetShadowRenderTargets(RHICmdList); // run it now, maybe run it later for parallel command lists
					bPerformClear = false;

					ProjectedShadowInfo->RenderDepth(RHICmdList, this, SetShadowRenderTargets);
					ProjectedShadowInfo->bDepthsCached = true;

					const FResolveParams ResolveParams(FResolveRect(
						ProjectedShadowInfo->X,
						ProjectedShadowInfo->Y,
						ProjectedShadowInfo->X + ProjectedShadowInfo->ResolutionX + SHADOW_BORDER * 2,
						ProjectedShadowInfo->Y + ProjectedShadowInfo->ResolutionY + SHADOW_BORDER * 2));

					RHICmdList.CopyToResolveTarget(
						GSceneRenderTargets.PreShadowCacheDepthZ->GetRenderTargetItem().TargetableTexture, 
						GSceneRenderTargets.PreShadowCacheDepthZ->GetRenderTargetItem().ShaderResourceTexture,
						false, 
						ResolveParams);
				}
			}
		}

		// Render the shadow projections.
		{
			SCOPED_DRAW_EVENTF(RHICmdList, EventShadowProj, TEXT("Cached Preshadow Projections"));
			RenderProjections(RHICmdList, LightSceneInfo, OpaqueCachedPreshadows);

			// Mark the attenuation buffer as dirty.
			bAttenuationBufferDirty = true;
		}
	}

	return bAttenuationBufferDirty;
}

/**
 * Used by RenderLights to render shadows to the attenuation buffer.
 *
 * @param LightSceneInfo Represents the current light
 * @return true if anything got rendered
 */
bool FForwardShadingSceneRenderer::RenderShadowDepthMap(FRHICommandListImmediate& RHICmdList, const FLightSceneInfo* LightSceneInfo)
{
	SCOPE_CYCLE_COUNTER(STAT_ProjectedShadowDrawTime);	

	bool bAttenuationBufferDirty = false;

	// Find the projected shadows cast by this light.
	FVisibleLightInfo& VisibleLightInfo = VisibleLightInfos[LightSceneInfo->Id];
	TArray<FProjectedShadowInfo*, SceneRenderingAllocator> Shadows;
	for (int32 ShadowIndex = 0; ShadowIndex < VisibleLightInfo.AllProjectedShadows.Num(); ShadowIndex++)
	{
		FProjectedShadowInfo* ProjectedShadowInfo = VisibleLightInfo.AllProjectedShadows[ShadowIndex];

		// Check that the shadow is visible in at least one view before rendering it.
		bool bShadowIsVisible = false;
		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			const FViewInfo& View = Views[ViewIndex];
			if (ProjectedShadowInfo->DependentView && ProjectedShadowInfo->DependentView != &View)
			{
				continue;
			}
			const FVisibleLightViewInfo& VisibleLightViewInfo = View.VisibleLightInfos[LightSceneInfo->Id];
			const FPrimitiveViewRelevance ViewRelevance = VisibleLightViewInfo.ProjectedShadowViewRelevanceMap[ShadowIndex];
			bShadowIsVisible |= !ProjectedShadowInfo->bTranslucentShadow && ViewRelevance.bOpaqueRelevance 
				&& VisibleLightViewInfo.ProjectedShadowVisibilityMap[ShadowIndex];
		}

		// Skip shadows which will be handled in RenderOnePassPointLightShadows or RenderReflectiveShadowmaps
		if (ProjectedShadowInfo->CascadeSettings.bOnePassPointLightShadow || ProjectedShadowInfo->bReflectiveShadowmap)
		{
			bShadowIsVisible = false;
		}

		if (ProjectedShadowInfo->bPreShadow && !LightSceneInfo->Proxy->HasStaticShadowing())
		{
			bShadowIsVisible = false;
		}

		if (bShadowIsVisible
			&& (!ProjectedShadowInfo->bPreShadow || ProjectedShadowInfo->HasSubjectPrims())
			&& !ProjectedShadowInfo->bAllocatedInPreshadowCache)
		{
			// Add the shadow to the list of visible shadows cast by this light.
			if (ProjectedShadowInfo->bWholeSceneShadow)
			{
				INC_DWORD_STAT(STAT_WholeSceneShadows);
			}
			else if (ProjectedShadowInfo->bPreShadow)
			{
				INC_DWORD_STAT(STAT_PreShadows);
			}
			else
			{
				INC_DWORD_STAT(STAT_PerObjectShadows);
			}
			Shadows.Add(ProjectedShadowInfo);
		}
	}

	// Sort the projected shadows by resolution.
	Shadows.Sort( FCompareFProjectedShadowInfoBySplitIndex() );	

	{
		// Render the shadow depths.
		SCOPED_DRAW_EVENT(RHICmdList, ShadowDepthsFromOpaqueForward);

		bool bFirst = true;
		auto SetShadowRenderTargets = [&bFirst](FRHICommandList& InRHICmdList)
		{
			GSceneRenderTargets.BeginRenderingShadowDepth(InRHICmdList, bFirst);
		};

		SetShadowRenderTargets(RHICmdList);  // run it now, maybe run it later for parallel command lists
		bFirst = false;

		// render depth for each shadow
		for (int32 ShadowIndex = 0; ShadowIndex < Shadows.Num(); ShadowIndex++)
		{
			FProjectedShadowInfo* ProjectedShadowInfo = Shadows[ShadowIndex];
			if (ProjectedShadowInfo->bAllocated)
			{
				check(!ProjectedShadowInfo->bTranslucentShadow);
				ProjectedShadowInfo->RenderDepth(RHICmdList, this, SetShadowRenderTargets);
				ProjectedShadowInfo->bRendered = true;
				ProjectedShadowInfo->bAllocated = false;
				bAttenuationBufferDirty = true;
			}
		}

		GSceneRenderTargets.FinishRenderingShadowDepth(RHICmdList);
	}

	return bAttenuationBufferDirty;
}

void FForwardShadingSceneRenderer::RenderShadowDepthMaps(FRHICommandListImmediate& RHICmdList)
{
	SCOPED_DRAW_EVENT(RHICmdList, Lights);

	bool bDynamicShadows = ViewFamily.EngineShowFlags.DynamicShadows && GetShadowQuality() > 0;
	if (IsSimpleDynamicLightingEnabled() || !bDynamicShadows)
	{
		return;
	}	

	// render shadowmaps for relevant lights.
	if (Scene->SimpleDirectionalLight)
	{		
		const FLightSceneInfo& LightSceneInfo = *Scene->SimpleDirectionalLight;

		if (LightSceneInfo.ShouldRenderViewIndependentWholeSceneShadows()
			// Only render movable shadowcasting lights
			&& !LightSceneInfo.Proxy->HasStaticShadowing()
			&& CheckForProjectedShadows(&LightSceneInfo))
		{
			bool bRender = false;
			// Check if the light is visible in any of the views.
			for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
			{
				if (LightSceneInfo.ShouldRenderLight(Views[ViewIndex]))
				{
					bRender = true;
					break;
				}
			}			

			if (bRender)
			{				
				FScopeCycleCounter Context(LightSceneInfo.Proxy->GetStatId());

				FString LightNameWithLevel;
				GetLightNameForDrawEvent(LightSceneInfo.Proxy, LightNameWithLevel);
				SCOPED_DRAW_EVENTF(RHICmdList, EventLightPass, *LightNameWithLevel);
				
				INC_DWORD_STAT(STAT_NumShadowedLights);

				//render shadowmap depth				
				RenderShadowDepthMap(RHICmdList, &LightSceneInfo);				
			}					
		}		
	}	
}
