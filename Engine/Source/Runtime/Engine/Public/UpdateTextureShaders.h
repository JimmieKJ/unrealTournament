// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "GlobalShader.h"
#include "ShaderParameters.h"

class FUpdateTexture2DSubresouceCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FUpdateTexture2DSubresouceCS,Global);
public:
	FUpdateTexture2DSubresouceCS() {}
	FUpdateTexture2DSubresouceCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader( Initializer )
	{
		SrcPitchParameter.Bind(Initializer.ParameterMap, TEXT("SrcPitch"), SPF_Mandatory);
		SrcBuffer.Bind(Initializer.ParameterMap, TEXT("SrcBuffer"), SPF_Mandatory);
		DestPosSizeParameter.Bind(Initializer.ParameterMap, TEXT("DestPosSize"), SPF_Mandatory);
		DestTexture.Bind(Initializer.ParameterMap, TEXT("DestTexture"), SPF_Mandatory);
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar)
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << SrcPitchParameter << SrcBuffer << DestPosSizeParameter << DestTexture;
		return bShaderHasOutdatedParameters;
	}

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5);
	}

//protected:
	FShaderParameter SrcPitchParameter;
	FShaderResourceParameter SrcBuffer;
	FShaderParameter DestPosSizeParameter;
	FShaderResourceParameter DestTexture;
};

class FCopyTexture2DCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FCopyTexture2DCS,Global);
public:
	FCopyTexture2DCS() {}
	FCopyTexture2DCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader( Initializer )
	{
		SrcTexture.Bind(Initializer.ParameterMap, TEXT("SrcTexture"), SPF_Mandatory);
		DestTexture.Bind(Initializer.ParameterMap, TEXT("DestTexture"), SPF_Mandatory);
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar)
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << SrcTexture << DestTexture;
		return bShaderHasOutdatedParameters;
	}

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5);
	}

//protected:
	FShaderResourceParameter SrcTexture;
	FShaderResourceParameter DestTexture;
};
