// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*================================================================================
MediaShaders.h: Shaders for converting decoded media files to RGBA format
================================================================================*/

#include "Engine/EngineTypes.h"
#include "RenderCore.h"
#include "GlobalShader.h"
#include "ShaderParameters.h"
#include "ShaderParameterUtils.h"
#include "RHIStaticStates.h"

#pragma once

class FMediaShaderHelper
{
	//draws a simple full screen quad with 0-1 UVs.
	//user must set up shaders and global shader state before calling.
	static void DrawFullScreenQuad();
};

class FMedaShadersVS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FMedaShadersVS, Global);

public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4);
	}

	/** Default constructor. */
	FMedaShadersVS() {}

	/** Initialization constructor. */
	FMedaShadersVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		return bShaderHasOutdatedParameters;
	}	

private:
};

/** Shader to convert a full-res luma (Y) and half-res chroma (CbCr) texture to RGBA */
class FYCbCrConvertShaderPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FYCbCrConvertShaderPS, Global);

public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4);
	}

	/** Default constructor. */
	FYCbCrConvertShaderPS() {}

	/** Initialization constructor. */
	FYCbCrConvertShaderPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{		
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);		
		return bShaderHasOutdatedParameters;
	}

	void SetParameters(FRHICommandList& RHICmdList, FTextureRHIRef LumaTexture, FTextureRHIRef CbCrTexture);

private:

	
};