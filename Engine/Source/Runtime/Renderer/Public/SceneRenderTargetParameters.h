// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	SceneRenderTargetParameters.h: Shader base classes
=============================================================================*/

#pragma once

namespace ESceneRenderTargetsMode
{
	enum Type
	{
		SetTextures,
		DontSet,
		DontSetIgnoreBoundByEditorCompositing,
	};
}

/** Encapsulates scene texture shader parameter bindings. */
class FSceneTextureShaderParameters
{
public:
	/** Binds the parameters using a compiled shader's parameter map. */
	void Bind(const FShaderParameterMap& ParameterMap);

	/** Sets the scene texture parameters for the given view. */
	template< typename ShaderRHIParamRef, typename TRHICmdList >
	void Set(
		TRHICmdList& RHICmdList,
		const ShaderRHIParamRef& ShaderRHI,
		const FSceneView& View,
		ESceneRenderTargetsMode::Type TextureMode = ESceneRenderTargetsMode::SetTextures,
		ESamplerFilter ColorFilter = SF_Point
	) const;

	/** Serializer. */
	friend FArchive& operator<<(FArchive& Ar,FSceneTextureShaderParameters& P);

private:
	/** The SceneColorTexture parameter for materials that use SceneColor */
	FShaderResourceParameter SceneColorTextureParameter;
	FShaderResourceParameter SceneColorTextureParameterSampler;
	/** The SceneDepthTexture parameter for materials that use SceneDepth */
	FShaderResourceParameter SceneDepthTextureParameter;
	FShaderResourceParameter SceneDepthTextureParameterSampler;
	/** The SceneAlphaCopyTexture parameter for materials that use SceneAlphaCopy */
	FShaderResourceParameter SceneAlphaCopyTextureParameter;
	FShaderResourceParameter SceneAlphaCopyTextureParameterSampler;

	/** for MSAA access to the scene color */
	FShaderResourceParameter SceneColorSurfaceParameter;
	/** The SceneColorTextureMSAA parameter for materials that use SceneColorTextureMSAA */
	FShaderResourceParameter SceneDepthSurfaceParameter;
	/**  */
	FShaderResourceParameter SceneDepthTextureNonMS;
	FShaderResourceParameter DirectionalOcclusionSampler;
	FShaderResourceParameter DirectionalOcclusionTexture;
};

/** Pixel shader parameters needed for deferred passes. */
class FDeferredPixelShaderParameters
{
public:
	void Bind(const FShaderParameterMap& ParameterMap);

	template< typename ShaderRHIParamRef, typename TRHICmdList >
	void Set(
		TRHICmdList& RHICmdList,
		const ShaderRHIParamRef ShaderRHI,
		const FSceneView& View,
		ESceneRenderTargetsMode::Type TextureMode = ESceneRenderTargetsMode::SetTextures
	) const;

	friend FArchive& operator<<(FArchive& Ar,FDeferredPixelShaderParameters& P);

private:
	FSceneTextureShaderParameters SceneTextureParameters;
	FShaderUniformBufferParameter GBufferResources;
	FShaderResourceParameter DBufferATextureMS;
	FShaderResourceParameter DBufferBTextureMS;
	FShaderResourceParameter DBufferCTextureMS;
	FShaderResourceParameter ScreenSpaceAOTextureMS;
	FShaderResourceParameter DBufferATextureNonMS;
	FShaderResourceParameter DBufferBTextureNonMS;
	FShaderResourceParameter DBufferCTextureNonMS;
	FShaderResourceParameter ScreenSpaceAOTextureNonMS;
	FShaderResourceParameter CustomDepthTextureNonMS;
	FShaderResourceParameter DBufferATexture;
	FShaderResourceParameter DBufferRenderMask;
	FShaderResourceParameter DBufferATextureSampler;
	FShaderResourceParameter DBufferBTexture;
	FShaderResourceParameter DBufferBTextureSampler;
	FShaderResourceParameter DBufferCTexture;
	FShaderResourceParameter DBufferCTextureSampler;
	FShaderResourceParameter ScreenSpaceAOTexture;
	FShaderResourceParameter ScreenSpaceAOTextureSampler;
	FShaderResourceParameter CustomDepthTexture;
	FShaderResourceParameter CustomDepthTextureSampler;
	FShaderResourceParameter CustomStencilTexture;
};