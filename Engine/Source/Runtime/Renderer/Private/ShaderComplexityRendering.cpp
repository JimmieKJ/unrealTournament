// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
ShaderComplexityRendering.cpp: Contains definitions for rendering the shader complexity viewmode.
=============================================================================*/

#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "SceneFilterRendering.h"
#include "PostProcessVisualizeComplexity.h"
#include "SceneUtils.h"

/**
 * Gets the maximum shader complexity count from the ini settings.
 */
float GetMaxShaderComplexityCount(ERHIFeatureLevel::Type InFeatureType)
{
	return InFeatureType == ERHIFeatureLevel::ES2 ? GEngine->MaxES2PixelShaderAdditiveComplexityCount : GEngine->MaxPixelShaderAdditiveComplexityCount;
}

void FShaderComplexityAccumulatePS::SetParameters(
	FRHICommandList& RHICmdList, 
	uint32 NumVertexInstructions, 
	uint32 NumPixelInstructions,
	ERHIFeatureLevel::Type InFeatureLevel)
{
	//normalize the complexity so we can fit it in a low precision scene color which is necessary on some platforms
	const float NormalizedComplexityValue = float(NumPixelInstructions) / GetMaxShaderComplexityCount(InFeatureLevel);

	SetShaderValue(RHICmdList, GetPixelShader(), NormalizedComplexity, NormalizedComplexityValue);
}

IMPLEMENT_SHADER_TYPE(,FShaderComplexityAccumulatePS,TEXT("ShaderComplexityAccumulatePixelShader"),TEXT("Main"),SF_Pixel);

/**
* The number of shader complexity colors from the engine ini that will be passed to the shader. 
* Changing this requires a recompile of the FShaderComplexityApplyPS.
*/
const uint32 NumShaderComplexityColors = 9;

/**
* Vertex shader used for combining LDR translucency with scene color when floating point blending isn't supported
*/
class FShaderComplexityApplyVS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FShaderComplexityApplyVS,Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		//this is used for shader complexity, so needs to compile for all platforms
		return true;
	}

	/** Default constructor. */
	FShaderComplexityApplyVS() {}

public:

	/** Initialization constructor. */
	FShaderComplexityApplyVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		:	FGlobalShader(Initializer)
	{
	}
};

IMPLEMENT_SHADER_TYPE(,FShaderComplexityApplyVS,TEXT("ShaderComplexityApplyVertexShader"),TEXT("Main"),SF_Vertex);

/**
* Pixel shader that is used to map shader complexity stored in scene color into color.
*/
class FShaderComplexityApplyPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FShaderComplexityApplyPS,Global);
public:

	/** 
	* Constructor - binds all shader params
	* @param Initializer - init data from shader compiler
	*/
	FShaderComplexityApplyPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		:	FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
		ShaderComplexityColors.Bind(Initializer.ParameterMap,TEXT("ShaderComplexityColors"));
	}

	FShaderComplexityApplyPS() 
	{
	}

	virtual void SetParameters(
		const FRenderingCompositePassContext& Context,
		const TArray<FLinearColor>& Colors
		)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		FGlobalShader::SetParameters(Context.RHICmdList, ShaderRHI, Context.View);

		PostprocessParameter.SetPS(ShaderRHI, Context, TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI());

		//Make sure there are at least NumShaderComplexityColors colors specified in the ini.
		//If there are more than NumShaderComplexityColors they will be ignored.
		check(Colors.Num() >= NumShaderComplexityColors);

		//pass the complexity -> color mapping into the pixel shader
		for(int32 ColorIndex = 0; ColorIndex < NumShaderComplexityColors; ColorIndex ++)
		{
			SetShaderValue(Context.RHICmdList, ShaderRHI, ShaderComplexityColors, Colors[ColorIndex], ColorIndex);
		}
	}

	/**
	* @param Platform - hardware platform
	* @return true if this shader should be cached
	*/
	static bool ShouldCache(EShaderPlatform Platform)
	{
		return true;
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("NUM_COMPLEXITY_COLORS"), NumShaderComplexityColors);
	}

	/**
	* Serialize shader parameters for this shader
	* @param Ar - archive to serialize with
	*/
	bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter;
		Ar << ShaderComplexityColors;
		return bShaderHasOutdatedParameters;
	}

private:

	FPostProcessPassParameters PostprocessParameter;
	FShaderParameter ShaderComplexityColors;
};

IMPLEMENT_SHADER_TYPE(,FShaderComplexityApplyPS,TEXT("ShaderComplexityApplyPixelShader"),TEXT("Main"),SF_Pixel);

//reuse the generic filter vertex declaration
extern TGlobalResource<FFilterVertexDeclaration> GFilterVertexDeclaration;

void FRCPassPostProcessVisualizeComplexity::Process(FRenderingCompositePassContext& Context)
{
	SCOPED_DRAW_EVENT(Context.RHICmdList, PostProcessVisualizeComplexity);
	const FPooledRenderTargetDesc* InputDesc = GetInputDesc(ePId_Input0);

	if(!InputDesc)
	{
		// input is not hooked up correctly
		return;
	}

	const FSceneView& View = Context.View;
	const FSceneViewFamily& ViewFamily = *(View.Family);
	
	FIntRect SrcRect = View.ViewRect;
	FIntRect DestRect = View.UnscaledViewRect;
	FIntPoint SrcSize = InputDesc->Extent;

	const FSceneRenderTargetItem& DestRenderTarget = PassOutputs[0].RequestSurface(Context);

	// Set the view family's render target/viewport.
	SetRenderTarget(Context.RHICmdList, DestRenderTarget.TargetableTexture, FTextureRHIRef());
	Context.SetViewportAndCallRHI(DestRect);

	// turn off culling and blending
	Context.RHICmdList.SetRasterizerState(TStaticRasterizerState<FM_Solid, CM_None>::GetRHI());
	Context.RHICmdList.SetBlendState(TStaticBlendState<>::GetRHI());

	// turn off depth reads/writes
	Context.RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());

	//reuse this generic vertex shader
	TShaderMapRef<FShaderComplexityApplyVS> VertexShader(Context.GetShaderMap());
	TShaderMapRef<FShaderComplexityApplyPS> PixelShader(Context.GetShaderMap());

	static FGlobalBoundShaderState ShaderComplexityBoundShaderState;
	
	SetGlobalBoundShaderState(Context.RHICmdList, Context.GetFeatureLevel(), ShaderComplexityBoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);

	PixelShader->SetParameters(Context, Colors);
	
	DrawRectangle(
		Context.RHICmdList,
		0, 0,
		DestRect.Width(), DestRect.Height(),
		SrcRect.Min.X, SrcRect.Min.Y,
		SrcRect.Width(), SrcRect.Height(),
		DestRect.Size(),
		SrcSize,
		*VertexShader,
		EDRF_UseTriangleOptimization);

	Context.RHICmdList.CopyToResolveTarget(DestRenderTarget.TargetableTexture, DestRenderTarget.ShaderResourceTexture, false, FResolveParams());
}

FPooledRenderTargetDesc FRCPassPostProcessVisualizeComplexity::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	FPooledRenderTargetDesc Ret = PassInputs[0].GetOutput()->RenderTargetDesc;

	Ret.Reset();
	Ret.DebugName = TEXT("VisualizeComplexity");

	return Ret;
}

