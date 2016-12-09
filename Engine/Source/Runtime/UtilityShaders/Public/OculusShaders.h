// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ShaderParameters.h"
#include "Shader.h"
#include "GlobalShader.h"
#include "ShaderParameterUtils.h"

class FOculusVertexShader : public FGlobalShader
{
	DECLARE_EXPORTED_SHADER_TYPE(FOculusVertexShader, Global, UTILITYSHADERS_API);
public:

	static bool ShouldCache(EShaderPlatform Platform) { return true; }

	FOculusVertexShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer) :
		FGlobalShader(Initializer)
	{}
	FOculusVertexShader() {}
};

class FOculusWhiteShader : public FGlobalShader
{
	DECLARE_EXPORTED_SHADER_TYPE(FOculusWhiteShader, Global, UTILITYSHADERS_API);
public:

	static bool ShouldCache(EShaderPlatform Platform) { return true; }

	FOculusWhiteShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer) :
		FGlobalShader(Initializer)
	{
	}
	
	FOculusWhiteShader() {}
};

class FOculusBlackShader : public FGlobalShader
{
	DECLARE_EXPORTED_SHADER_TYPE(FOculusBlackShader, Global, UTILITYSHADERS_API);
public:

	static bool ShouldCache(EShaderPlatform Platform) { return true; }

	FOculusBlackShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer) :
		FGlobalShader(Initializer)
	{
	}

	FOculusBlackShader() {}
};

class FOculusAlphaInverseShader : public FGlobalShader
{
	DECLARE_EXPORTED_SHADER_TYPE(FOculusAlphaInverseShader, Global, UTILITYSHADERS_API);
public:

	static bool ShouldCache(EShaderPlatform Platform) { return true; }

	FOculusAlphaInverseShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer) :
		FGlobalShader(Initializer)
	{
		InTexture.Bind(Initializer.ParameterMap, TEXT("InTexture"), SPF_Mandatory);
		InTextureSampler.Bind(Initializer.ParameterMap, TEXT("InTextureSampler"));
	}
	FOculusAlphaInverseShader() {}

	UTILITYSHADERS_API void SetParameters(FRHICommandList& RHICmdList, const FTexture* Texture)
	{
		SetTextureParameter(RHICmdList, GetPixelShader(), InTexture, InTextureSampler, Texture);
	}

	UTILITYSHADERS_API void SetParameters(FRHICommandList& RHICmdList, FSamplerStateRHIParamRef SamplerStateRHI, FTextureRHIParamRef TextureRHI)
	{
		SetTextureParameter(RHICmdList, GetPixelShader(), InTexture, InTextureSampler, SamplerStateRHI, TextureRHI);
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << InTexture;
		Ar << InTextureSampler;
		return bShaderHasOutdatedParameters;
	}

private:
	FShaderResourceParameter InTexture;
	FShaderResourceParameter InTextureSampler;
};
