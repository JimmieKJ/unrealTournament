// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SlateRHIRendererPrivatePCH.h"
#include "SlateMaterialShader.h"
#include "RenderingCommon.h"

bool FSlateMaterialShaderPS::ShouldCache(EShaderPlatform Platform, const FMaterial* Material)
{
	return Material->IsUsedWithUI();
}


void FSlateMaterialShaderPS::ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
{
	// Set defines based on what this shader will be used for
	OutEnvironment.SetDefine( TEXT("USE_MATERIALS"), 1 );

	FMaterialShader::ModifyCompilationEnvironment( Platform, Material, OutEnvironment );
}

FSlateMaterialShaderPS::FSlateMaterialShaderPS(const FMaterialShaderType::CompiledShaderInitializerType& Initializer)
	: FMaterialShader(Initializer)
{
	ShaderParams.Bind(Initializer.ParameterMap, TEXT("ShaderParams"));
	GammaValues.Bind(Initializer.ParameterMap, TEXT("GammaValues"));
}

void FSlateMaterialShaderPS::SetParameters(FRHICommandList& RHICmdList, const FSceneView& View, const FMaterialRenderProxy* MaterialRenderProxy, const FMaterial* Material, float InDisplayGamma, const FVector4& InShaderParams )
{
	const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

	EBlendMode BlendMode = Material->GetBlendMode();

	switch (BlendMode)
	{
	default:
	case BLEND_Opaque:
		RHICmdList.SetBlendState(TStaticBlendState<>::GetRHI());
		break;
	case BLEND_Masked:
		RHICmdList.SetBlendState(TStaticBlendState<>::GetRHI());
		break;
	case BLEND_Translucent:
		RHICmdList.SetBlendState(TStaticBlendState<CW_RGBA, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha, BO_Add, BF_InverseDestAlpha, BF_One>::GetRHI());
		break;
	case BLEND_Additive:
		// Add to the existing scene color
		RHICmdList.SetBlendState(TStaticBlendState<CW_RGB, BO_Add, BF_One, BF_One, BO_Add, BF_Zero, BF_InverseSourceAlpha>::GetRHI());
		break;
	case BLEND_Modulate:
		// Modulate with the existing scene color
		RHICmdList.SetBlendState(TStaticBlendState<CW_RGB, BO_Add, BF_DestColor, BF_Zero>::GetRHI());
		break;
	};

	SetShaderValue( RHICmdList, ShaderRHI, ShaderParams, InShaderParams );

	const bool bDeferredPass = false;
	FMaterialShader::SetParameters<FPixelShaderRHIParamRef>(RHICmdList, ShaderRHI, MaterialRenderProxy, *Material, View, bDeferredPass, ESceneRenderTargetsMode::SetTextures);

}


void FSlateMaterialShaderPS::SetDisplayGamma(FRHICommandList& RHICmdList, float InDisplayGamma)
{
	FVector2D InGammaValues(2.2f / InDisplayGamma, 1.0f/InDisplayGamma);

	SetShaderValue(RHICmdList, GetPixelShader(), GammaValues, InGammaValues);
}

bool FSlateMaterialShaderPS::Serialize(FArchive& Ar)
{
	bool bShaderHasOutdatedParameters = FMaterialShader::Serialize(Ar);
	Ar << GammaValues;
	Ar << ShaderParams;
	return bShaderHasOutdatedParameters;
}

#define IMPLEMENT_SLATE_MATERIALSHADER_TYPE(ShaderType, bDrawDisabledEffect) \
	typedef TSlateMaterialShaderPS<ESlateShader::ShaderType, bDrawDisabledEffect> TSlateMaterialShaderPS##ShaderType##bDrawDisabledEffect; \
	IMPLEMENT_MATERIAL_SHADER_TYPE(template<>, TSlateMaterialShaderPS##ShaderType##bDrawDisabledEffect, TEXT("SlateElementPixelShader"), TEXT("Main"), SF_Pixel);


IMPLEMENT_SLATE_MATERIALSHADER_TYPE(Default,true);
IMPLEMENT_SLATE_MATERIALSHADER_TYPE(Default,false);
IMPLEMENT_SLATE_MATERIALSHADER_TYPE(Border,true);
IMPLEMENT_SLATE_MATERIALSHADER_TYPE(Border,false);
