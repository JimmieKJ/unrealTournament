// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
DebugViewModeMaterialProxy.h : Contains definitions the debug view mode material shaders.
=============================================================================*/

#pragma once

#if WITH_EDITORONLY_DATA

/**
 * Material proxy for debug viewmode. Used to prevent debug view mode shaders from being stored in the default material map.
 */
class FDebugViewModeMaterialProxy : public FMaterial, public FMaterialRenderProxy
{
public:
	FDebugViewModeMaterialProxy()
		: FMaterial()
		, MaterialInterface(nullptr)
		, Material(nullptr)
	{
		SetQualityLevelProperties(EMaterialQualityLevel::High, false, GMaxRHIFeatureLevel);
	}

	FDebugViewModeMaterialProxy(UMaterialInterface* InMaterialInterface, EMaterialShaderMapUsage::Type InUsage);

	virtual bool RequiresSynchronousCompilation() const override { return false; };

	/**
	* Should shaders compiled for this material be saved to disk?
	*/
	virtual bool IsPersistent() const override { return false; }

	// Normally this would cause a bug as the shader map would try to be shared by both, but GetShaderMapUsage() allows this to work
	virtual FGuid GetMaterialId() const override { return Material->StateId; }

	virtual EMaterialShaderMapUsage::Type GetShaderMapUsage() const override { return Usage; }


	virtual bool ShouldCache(EShaderPlatform Platform, const FShaderType* ShaderType, const FVertexFactoryType* VertexFactoryType) const override
	{
		return FString(ShaderType->GetName()).Contains(TEXT("FTexCoordScaleAnalysisPS"));
	}

	virtual const TArray<UTexture*>& GetReferencedTextures() const override
	{
		return ReferencedTextures;
	}

	// Material properties.
	/** Entry point for compiling a specific material property.  This must call SetMaterialProperty. */
	virtual int32 CompilePropertyAndSetMaterialProperty(EMaterialProperty Property, FMaterialCompiler* Compiler, EShaderFrequency OverrideShaderFrequency, bool bUsePreviousFrameTime) const override
	{
		return MaterialInterface ? MaterialInterface->GetMaterialResource(GMaxRHIFeatureLevel)->CompilePropertyAndSetMaterialProperty(Property, Compiler, OverrideShaderFrequency, bUsePreviousFrameTime) : INDEX_NONE;
	}

	virtual FString GetMaterialUsageDescription() const override
	{
		return FString::Printf(TEXT("FDebugViewModeMaterialProxy %s"), MaterialInterface ? *MaterialInterface->GetName() : TEXT("NULL"));
	}

	virtual FString GetFriendlyName() const override { return FString::Printf(TEXT("FDebugViewModeMaterialProxy %s"), MaterialInterface ? *MaterialInterface->GetName() : TEXT("NULL")); }

	const UMaterialInterface* GetMaterialInterface() const
	{
		return MaterialInterface;
	}

	friend FArchive& operator<< ( FArchive& Ar, FDebugViewModeMaterialProxy& V )
	{
		return Ar << V.MaterialInterface;
	}

	////////////////
	// FMaterialRenderProxy interface.
	virtual const FMaterial* GetMaterial(ERHIFeatureLevel::Type FeatureLevel) const override;
	virtual bool GetVectorValue(const FName ParameterName, FLinearColor* OutValue, const FMaterialRenderContext& Context) const override;
	virtual bool GetScalarValue(const FName ParameterName, float* OutValue, const FMaterialRenderContext& Context) const override;
	virtual bool GetTextureValue(const FName ParameterName,const UTexture** OutValue, const FMaterialRenderContext& Context) const override;

	virtual int32 GetMaterialDomain() const override;
	virtual bool IsTwoSided() const  override;
	virtual bool IsDitheredLODTransition() const  override;
	virtual bool IsLightFunction() const override;
	virtual bool IsUsedWithDeferredDecal() const override;
	virtual bool IsSpecialEngineMaterial() const override;
	virtual bool IsWireframe() const override;
	virtual bool IsMasked() const override;
	virtual enum EBlendMode GetBlendMode() const override;
	virtual enum EMaterialShadingModel GetShadingModel() const override;
	virtual float GetOpacityMaskClipValue() const override;

	////////////////

	static void AddShader(UMaterialInterface* InMaterialInterface, EMaterialShaderMapUsage::Type InUsage);
	static const FMaterial* GetShader(EDebugViewShaderMode DebugViewShaderMode, const FMaterial* Material);
	static void ClearAllShaders();

private:

	/** The material interface for this proxy */
	UMaterialInterface* MaterialInterface;
	UMaterial* Material;	
	TArray<UTexture*> ReferencedTextures;
	EMaterialShaderMapUsage::Type Usage;

	static volatile bool bReentrantCall;
	static TMap<const FMaterial*, FDebugViewModeMaterialProxy*> DebugMaterialShaderMap;
};

#endif