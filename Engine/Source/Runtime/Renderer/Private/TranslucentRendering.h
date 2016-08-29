// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	TranslucentRendering.h: Translucent rendering definitions.
=============================================================================*/

#pragma once

/**
* Translucent draw policy factory.
* Creates the policies needed for rendering a mesh based on its material
*/
class FTranslucencyDrawingPolicyFactory
{
public:
	enum { bAllowSimpleElements = true };
	struct ContextType 
	{
		const FProjectedShadowInfo* TranslucentSelfShadow;
		ETranslucencyPass::Type TranslucenyPassType;
		bool bSceneColorCopyIsUpToDate;
		bool bPostAA;

		ContextType(const FProjectedShadowInfo* InTranslucentSelfShadow = NULL, ETranslucencyPass::Type InTranslucenyPassType = ETranslucencyPass::TPT_StandardTranslucency, bool bPostAAIn = false)
			: TranslucentSelfShadow(InTranslucentSelfShadow)
			, TranslucenyPassType(InTranslucenyPassType)
			, bSceneColorCopyIsUpToDate(false)
			, bPostAA(bPostAAIn)
		{}
	};

	/**
	* Render a dynamic mesh using a translucent draw policy 
	* @return true if the mesh rendered
	*/
	static bool DrawDynamicMesh(
		FRHICommandList& RHICmdList, 
		const FViewInfo& View,
		ContextType DrawingContext,
		const FMeshBatch& Mesh,
		bool bBackFace,
		bool bPreFog,
		const FPrimitiveSceneProxy* PrimitiveSceneProxy,
		FHitProxyId HitProxyId
		);

	/**
	* Render a dynamic mesh using a translucent draw policy 
	* @return true if the mesh rendered
	*/
	static bool DrawStaticMesh(
		FRHICommandList& RHICmdList, 
		const FViewInfo& View,
		ContextType DrawingContext,
		const FStaticMesh& StaticMesh,
		const uint64& BatchElementMask,
		bool bPreFog,
		const FPrimitiveSceneProxy* PrimitiveSceneProxy,
		FHitProxyId HitProxyId
		);

	/**
	* Resolves the scene color target and copies it for use as a source texture.
	*/
	static void CopySceneColor(FRHICommandList& RHICmdList, const FViewInfo& View, const FPrimitiveSceneProxy* PrimitiveSceneProxy);

private:
	/**
	* Render a dynamic or static mesh using a translucent draw policy
	* @return true if the mesh rendered
	*/
	static bool DrawMesh(
		FRHICommandList& RHICmdList,
		const FViewInfo& View,
		ContextType DrawingContext,
		const FMeshBatch& Mesh,
		const uint64& BatchElementMask,
		bool bBackFace,
		const FMeshDrawingRenderState& DrawRenderState,
		bool bPreFog,
		const FPrimitiveSceneProxy* PrimitiveSceneProxy,
		FHitProxyId HitProxyId
		);

};


/**
* Translucent draw policy factory.
* Creates the policies needed for rendering a mesh based on its material
*/
class FMobileTranslucencyDrawingPolicyFactory
{
public:
	enum { bAllowSimpleElements = true };
	struct ContextType 
	{
		bool bRenderingSeparateTranslucency;

		ContextType(bool InbRenderingSeparateTranslucency)
		:	bRenderingSeparateTranslucency(InbRenderingSeparateTranslucency)
		{}
	};

	/**
	* Render a dynamic mesh using a translucent draw policy 
	* @return true if the mesh rendered
	*/
	static bool DrawDynamicMesh(
		FRHICommandList& RHICmdList, 
		const FViewInfo& View,
		ContextType DrawingContext,
		const FMeshBatch& Mesh,
		bool bBackFace,
		bool bPreFog,
		const FPrimitiveSceneProxy* PrimitiveSceneProxy,
		FHitProxyId HitProxyId
		);

	/**
	* Render a dynamic mesh using a translucent draw policy 
	* @return true if the mesh rendered
	*/
	static bool DrawStaticMesh(
		FRHICommandList& RHICmdList, 
		const FViewInfo& View,
		ContextType DrawingContext,
		const FStaticMesh& StaticMesh,
		bool bPreFog,
		const FPrimitiveSceneProxy* PrimitiveSceneProxy,
		FHitProxyId HitProxyId
		);
};

/** Represents a subregion of a volume texture. */
struct FVolumeBounds
{
	int32 MinX, MinY, MinZ;
	int32 MaxX, MaxY, MaxZ;

	FVolumeBounds() :
		MinX(0),
		MinY(0),
		MinZ(0),
		MaxX(0),
		MaxY(0),
		MaxZ(0)
	{}

	FVolumeBounds(int32 Max) :
		MinX(0),
		MinY(0),
		MinZ(0),
		MaxX(Max),
		MaxY(Max),
		MaxZ(Max)
	{}

	bool IsValid() const
	{
		return MaxX > MinX && MaxY > MinY && MaxZ > MinZ;
	}
};

/** Vertex shader used to write to a range of slices of a 3d volume texture. */
class FWriteToSliceVS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FWriteToSliceVS,Global);
public:

	static bool ShouldCache(EShaderPlatform Platform) 
	{ 
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4); 
	}

	static void ModifyCompilationEnvironment( EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment )
	{
		FGlobalShader::ModifyCompilationEnvironment( Platform, OutEnvironment );
		OutEnvironment.CompilerFlags.Add( CFLAG_VertexToGeometryShader );
	}

	FWriteToSliceVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FGlobalShader(Initializer)
    {
        UVScaleBias.Bind(Initializer.ParameterMap, TEXT("UVScaleBias"));
        MinZ.Bind(Initializer.ParameterMap, TEXT("MinZ"));
	}

	FWriteToSliceVS() {}

	void SetParameters(FRHICommandList& RHICmdList, const FVolumeBounds& VolumeBounds, uint32 VolumeResolution)
	{
		const float InvVolumeResolution = 1.0f / VolumeResolution;
		SetShaderValue(RHICmdList, GetVertexShader(), UVScaleBias, FVector4(
			(VolumeBounds.MaxX - VolumeBounds.MinX) * InvVolumeResolution, 
			(VolumeBounds.MaxY - VolumeBounds.MinY) * InvVolumeResolution,
			VolumeBounds.MinX * InvVolumeResolution,
			VolumeBounds.MinY * InvVolumeResolution));
        SetShaderValue(RHICmdList, GetVertexShader(), MinZ, VolumeBounds.MinZ);
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << UVScaleBias;
        Ar << MinZ;
        return bShaderHasOutdatedParameters;
	}

private:
    FShaderParameter UVScaleBias;
    FShaderParameter MinZ;
};

/** Geometry shader used to write to a range of slices of a 3d volume texture. */
class FWriteToSliceGS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FWriteToSliceGS,Global);
public:

	static bool ShouldCache(EShaderPlatform Platform) 
	{ 
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4) && RHISupportsGeometryShaders(Platform); 
	}

	FWriteToSliceGS(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FGlobalShader(Initializer)
	{
		MinZ.Bind(Initializer.ParameterMap, TEXT("MinZ"));
	}
	FWriteToSliceGS() {}

	void SetParameters(FRHICommandList& RHICmdList, const FVolumeBounds& VolumeBounds)
	{
		SetShaderValue(RHICmdList, GetGeometryShader(), MinZ, VolumeBounds.MinZ);
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << MinZ;
		return bShaderHasOutdatedParameters;
	}

private:
	FShaderParameter MinZ;
};

extern void RasterizeToVolumeTexture(FRHICommandList& RHICmdList, FVolumeBounds VolumeBounds);
