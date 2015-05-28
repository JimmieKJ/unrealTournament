// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "GlobalShader.h"
#include "ShaderParameters.h"

class FClearReplacementVS : public FGlobalShader
{
	DECLARE_EXPORTED_SHADER_TYPE(FClearReplacementVS, Global, UTILITYSHADERS_API);
public:
	FClearReplacementVS() {}
	FClearReplacementVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
	: FGlobalShader( Initializer )
	{
	}
	
	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		return bShaderHasOutdatedParameters;
	}
	
	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4);
	}
};

class FClearReplacementPS : public FGlobalShader
{
	DECLARE_EXPORTED_SHADER_TYPE(FClearReplacementPS, Global, UTILITYSHADERS_API);
public:
	FClearReplacementPS() {}
	FClearReplacementPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
	: FGlobalShader( Initializer )
	{
		ClearColor.Bind(Initializer.ParameterMap, TEXT("ClearColor"), SPF_Mandatory);
	}
	
	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << ClearColor;
		return bShaderHasOutdatedParameters;
	}
	
	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4);
	}
	
	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
		OutEnvironment.SetRenderTargetOutputFormat(0, PF_A32B32G32R32F);
	}
	
protected:
	FShaderParameter ClearColor;
};

class FClearTexture2DReplacementCS : public FGlobalShader
{
	DECLARE_EXPORTED_SHADER_TYPE(FClearTexture2DReplacementCS, Global, UTILITYSHADERS_API);
public:
	FClearTexture2DReplacementCS() {}
	FClearTexture2DReplacementCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
	: FGlobalShader( Initializer )
	{
		ClearColor.Bind(Initializer.ParameterMap, TEXT("ClearColor"), SPF_Mandatory);
		ClearTextureRW.Bind(Initializer.ParameterMap, TEXT("ClearTextureRW"), SPF_Mandatory);
	}
	
	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << ClearColor << ClearTextureRW;
		return bShaderHasOutdatedParameters;
	}
	
	void SetParameters(FRHICommandList& RHICmdList, FUnorderedAccessViewRHIParamRef TextureRW, FLinearColor Value);
	
	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5);
	}
	
	const FShaderParameter& GetClearColorParameter()
	{
		return ClearColor;
	}
	
	const FShaderResourceParameter& GetClearTextureRWParameter()
	{
		return ClearTextureRW;
	}
	
protected:
	FShaderParameter ClearColor;
	FShaderResourceParameter ClearTextureRW;
};

class FClearTexture2DReplacementScissorCS : public FGlobalShader
{
	DECLARE_EXPORTED_SHADER_TYPE(FClearTexture2DReplacementScissorCS, Global, UTILITYSHADERS_API);
public:
	FClearTexture2DReplacementScissorCS() {}
	FClearTexture2DReplacementScissorCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		ClearColor.Bind(Initializer.ParameterMap, TEXT("ClearColor"), SPF_Mandatory);
		TargetBounds.Bind(Initializer.ParameterMap, TEXT("TargetBounds"), SPF_Mandatory);
		ClearTextureRW.Bind(Initializer.ParameterMap, TEXT("ClearTextureRW"), SPF_Mandatory);
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << ClearColor << TargetBounds << ClearTextureRW;
		return bShaderHasOutdatedParameters;
	}

	void SetParameters(FRHICommandList& RHICmdList, FUnorderedAccessViewRHIParamRef TextureRW, FLinearColor ClearColor, const FVector4& InTargetBounds);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5);
	}

	const FShaderParameter& GetClearColorParameter()
	{
		return ClearColor;
	}

	const FShaderParameter& GetTargetBoundsParameter()
	{
		return TargetBounds;
	}

	const FShaderResourceParameter& GetClearTextureRWParameter()
	{
		return ClearTextureRW;
	}

protected:
	FShaderParameter ClearColor;
	FShaderParameter TargetBounds;
	FShaderResourceParameter ClearTextureRW;
};

class FClearBufferReplacementCS : public FGlobalShader
{
	DECLARE_EXPORTED_SHADER_TYPE(FClearBufferReplacementCS, Global, UTILITYSHADERS_API);
public:
	FClearBufferReplacementCS() {}
	FClearBufferReplacementCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
	: FGlobalShader( Initializer )
	{
		ClearDword.Bind(Initializer.ParameterMap, TEXT("ClearDword"), SPF_Mandatory);
		ClearBufferRW.Bind(Initializer.ParameterMap, TEXT("ClearBufferRW"), SPF_Mandatory);
	}
	
	void SetParameters(FRHICommandList& RHICmdList, FUnorderedAccessViewRHIParamRef BufferRW, uint32 Dword);
	
	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << ClearDword << ClearBufferRW;
		return bShaderHasOutdatedParameters;
	}
	
	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5);
	}
	
	//protected:
	FShaderParameter ClearDword;
	FShaderResourceParameter ClearBufferRW;
};
