// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Runtime/Renderer/Public/MaterialShader.h"


class FSlateMaterialShaderPS : public FMaterialShader
{
public:

	/** Only compile shaders used with UI. */
	static bool ShouldCache(EShaderPlatform Platform, const FMaterial* Material);

	/** Modifies the compilation of this shader. */
	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment);

	FSlateMaterialShaderPS() {}
	FSlateMaterialShaderPS(const FMaterialShaderType::CompiledShaderInitializerType& Initializer);

	void SetParameters(FRHICommandList& RHICmdList, const FSceneView& View, const FMaterialRenderProxy* MaterialRenderProxy, const FMaterial* Material, float InDisplayGamma, const FVector4& InShaderParams );

	void SetDisplayGamma(FRHICommandList& RHICmdList, float InDisplayGamma);

	virtual bool Serialize(FArchive& Ar) override;

private:

	FShaderParameter GammaValues;
	FShaderParameter ShaderParams;
};


template<ESlateShader::Type ShaderType,bool bDrawDisabledEffect> 
class TSlateMaterialShaderPS : public FSlateMaterialShaderPS
{
public:
	DECLARE_SHADER_TYPE(TSlateMaterialShaderPS,Material);

	TSlateMaterialShaderPS() { }

	TSlateMaterialShaderPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FSlateMaterialShaderPS( Initializer )
	{ }
	
	/** Only compile shaders used with UI. */
	static bool ShouldCache(EShaderPlatform Platform, const FMaterial* Material)
	{
		return FSlateMaterialShaderPS::ShouldCache(Platform,Material);
	}

	/** Modifies the compilation of this shader. */
	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		FSlateMaterialShaderPS::ModifyCompilationEnvironment(Platform, Material,OutEnvironment);

		OutEnvironment.SetDefine(TEXT("SHADER_TYPE"), (uint32)ShaderType);
		OutEnvironment.SetDefine(TEXT("DRAW_DISABLED_EFFECT"), (uint32)(bDrawDisabledEffect ? 1 : 0));
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		return FSlateMaterialShaderPS::Serialize( Ar );
	}
};
