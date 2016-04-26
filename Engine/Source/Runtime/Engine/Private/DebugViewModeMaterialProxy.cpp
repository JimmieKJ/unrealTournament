// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
DebugViewModeMaterialProxy.cpp : Contains definitions the debug view mode material shaders.
=============================================================================*/

#include "EnginePrivate.h"
#include "DebugViewModeMaterialProxy.h"

ENGINE_API const FMaterial* GetDebugViewMaterialPS(EDebugViewShaderMode DebugViewShaderMode, const FMaterial* Material)
{
#if WITH_EDITORONLY_DATA
	return FDebugViewModeMaterialProxy::GetShader(DebugViewShaderMode, Material);
#else
	return nullptr;
#endif
}

ENGINE_API void ClearAllDebugViewMaterials()
{
#if WITH_EDITORONLY_DATA
	FDebugViewModeMaterialProxy::ClearAllShaders();
#endif
}

#if WITH_EDITORONLY_DATA

TMap<const FMaterial*, FDebugViewModeMaterialProxy*> FDebugViewModeMaterialProxy::DebugMaterialShaderMap;
volatile bool FDebugViewModeMaterialProxy::bReentrantCall = false;

void FDebugViewModeMaterialProxy::AddShader(UMaterialInterface* InMaterialInterface, EMaterialShaderMapUsage::Type InUsage)
{
	if (!InMaterialInterface) return;

	const FMaterial* Material = InMaterialInterface->GetMaterialResource(GMaxRHIFeatureLevel);
	if (!Material) return;

	if (!DebugMaterialShaderMap.Contains(Material))
	{
		DebugMaterialShaderMap.Add(Material, new FDebugViewModeMaterialProxy(InMaterialInterface, InUsage));
	}
}

const FMaterial* FDebugViewModeMaterialProxy::GetShader(EDebugViewShaderMode DebugViewShaderMode, const FMaterial* Material)
{
	if (DebugViewShaderMode == DVSM_TexCoordScaleAnalysis)
	{
		FDebugViewModeMaterialProxy** BoundMaterial = DebugMaterialShaderMap.Find(Material);
		if (BoundMaterial)
		{
			return *BoundMaterial;
		}
	}
	return nullptr;
}

void FDebugViewModeMaterialProxy::ClearAllShaders()
{
	if (bReentrantCall) return;

	bReentrantCall = true;
	for (TMap<const FMaterial*, FDebugViewModeMaterialProxy*>::TIterator It(DebugMaterialShaderMap); It; ++It)
	{
		FDebugViewModeMaterialProxy* Proxy = It.Value();
		delete Proxy;
	}
	DebugMaterialShaderMap.Empty();

	bReentrantCall = false;
}

FDebugViewModeMaterialProxy::FDebugViewModeMaterialProxy(UMaterialInterface* InMaterialInterface, EMaterialShaderMapUsage::Type InUsage)
	: FMaterial()
	, MaterialInterface(InMaterialInterface)
	, Material(nullptr)
	, Usage(InUsage)
{
	SetQualityLevelProperties(EMaterialQualityLevel::High, false, GMaxRHIFeatureLevel);
	Material = InMaterialInterface->GetMaterial();
	Material->AppendReferencedTextures(ReferencedTextures);

	FMaterialResource* Resource = InMaterialInterface->GetMaterialResource(GMaxRHIFeatureLevel);

	FMaterialShaderMapId ResourceId;
	Resource->GetShaderMapId(GMaxRHIShaderPlatform, ResourceId);

	{
		TArray<FShaderType*> ShaderTypes;
		TArray<FVertexFactoryType*> VFTypes;
		TArray<const FShaderPipelineType*> ShaderPipelineTypes;
		GetDependentShaderAndVFTypes(GMaxRHIShaderPlatform, ShaderTypes, ShaderPipelineTypes, VFTypes);

		// Overwrite the shader map Id's dependencies with ones that came from the FMaterial actually being compiled (this)
		// This is necessary as we change FMaterial attributes like GetShadingModel(), which factor into the ShouldCache functions that determine dependent shader types
		ResourceId.SetShaderDependencies(ShaderTypes, ShaderPipelineTypes, VFTypes);
	}

	ResourceId.Usage = InUsage;

	CacheShaders(ResourceId, GMaxRHIShaderPlatform, true);
}

const FMaterial* FDebugViewModeMaterialProxy::GetMaterial(ERHIFeatureLevel::Type FeatureLevel) const
{
	if (GetRenderingThreadShaderMap())
	{
		return this;
	}
	else
	{
		return UMaterial::GetDefaultMaterial(MD_Surface)->GetRenderProxy(false)->GetMaterial(FeatureLevel);
	}
}

bool FDebugViewModeMaterialProxy::GetVectorValue(const FName ParameterName, FLinearColor* OutValue, const FMaterialRenderContext& Context) const
{
	return MaterialInterface->GetRenderProxy(0)->GetVectorValue(ParameterName, OutValue, Context);
}

bool FDebugViewModeMaterialProxy::GetScalarValue(const FName ParameterName, float* OutValue, const FMaterialRenderContext& Context) const
{
	return MaterialInterface->GetRenderProxy(0)->GetScalarValue(ParameterName, OutValue, Context);
}

bool FDebugViewModeMaterialProxy::GetTextureValue(const FName ParameterName,const UTexture** OutValue, const FMaterialRenderContext& Context) const
{
	return MaterialInterface->GetRenderProxy(0)->GetTextureValue(ParameterName,OutValue,Context);
}

int32 FDebugViewModeMaterialProxy::GetMaterialDomain() const
{
	return Material ? (int32)Material->MaterialDomain : (int32)MD_Surface;
}

bool FDebugViewModeMaterialProxy::IsTwoSided() const
{ 
	return MaterialInterface && MaterialInterface->IsTwoSided();
}

bool FDebugViewModeMaterialProxy::IsDitheredLODTransition() const
{ 
	return MaterialInterface && MaterialInterface->IsDitheredLODTransition();
}

bool FDebugViewModeMaterialProxy::IsLightFunction() const
{
	return Material && Material->MaterialDomain == MD_LightFunction;
}

bool FDebugViewModeMaterialProxy::IsUsedWithDeferredDecal() const
{
	return	Material && Material->MaterialDomain == MD_DeferredDecal;
}

bool FDebugViewModeMaterialProxy::IsSpecialEngineMaterial() const
{
	return Material && Material->bUsedAsSpecialEngineMaterial;
}

bool FDebugViewModeMaterialProxy::IsWireframe() const
{
	return Material && Material->Wireframe;
}

bool FDebugViewModeMaterialProxy::IsMasked() const
{ 
	return Material && Material->IsMasked(); 
}

enum EBlendMode FDebugViewModeMaterialProxy::GetBlendMode() const
{ 
	return Material ? Material->GetBlendMode() : BLEND_Opaque; 
}

enum EMaterialShadingModel FDebugViewModeMaterialProxy::GetShadingModel() const
{ 
	return Material ? Material->GetShadingModel() : MSM_Unlit;
}

float FDebugViewModeMaterialProxy::GetOpacityMaskClipValue() const
{ 
	return Material ? Material->GetOpacityMaskClipValue() : .5f;
}

#endif