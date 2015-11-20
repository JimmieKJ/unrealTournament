// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
ShaderComplexityRendering.cpp: Contains definitions for rendering the shader complexity viewmode.
=============================================================================*/

#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "SceneFilterRendering.h"
#include "PostProcessVisualizeComplexity.h"
#include "SceneUtils.h"
#include "PostProcessing.h"

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
	const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

	float NormalizeMul = 1.0f / GetMaxShaderComplexityCount(InFeatureLevel);

	// normalize the complexity so we can fit it in a low precision scene color which is necessary on some platforms
	// late value is for overdraw which can be problmatic with a low precision float format, at some point the precision isn't there any more and it doesn't accumulate
	FVector Value = FVector(NumPixelInstructions * NormalizeMul, NumVertexInstructions * NormalizeMul, 1/32.0f);

	SetShaderValue(RHICmdList, ShaderRHI, NormalizedComplexity, Value);
}

IMPLEMENT_SHADER_TYPE(,FShaderComplexityAccumulatePS,TEXT("ShaderComplexityAccumulatePixelShader"),TEXT("Main"),SF_Pixel);

/**
* The number of shader complexity colors from the engine ini that will be passed to the shader. 
* Changing this requires a recompile of the FShaderComplexityApplyPS.
*/
const uint32 NumShaderComplexityColors = 9;

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
		MiniFontTexture.Bind(Initializer.ParameterMap, TEXT("MiniFontTexture"));
		ShaderComplexityParams.Bind(Initializer.ParameterMap, TEXT("ShaderComplexityParams"));
	}

	FShaderComplexityApplyPS() 
	{
	}

	virtual void SetParameters(
		const FRenderingCompositePassContext& Context,
		const TArray<FLinearColor>& Colors,
		bool bLegend
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

		{
			FVector4 Value(bLegend, 0, 0, 0);

			SetShaderValue(Context.RHICmdList, ShaderRHI, ShaderComplexityParams, Value);
		}

		SetTextureParameter(Context.RHICmdList, ShaderRHI, MiniFontTexture, GEngine->MiniFontTexture ? GEngine->MiniFontTexture->Resource->TextureRHI : GSystemTextures.WhiteDummy->GetRenderTargetItem().TargetableTexture);
	}

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return true;
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("NUM_COMPLEXITY_COLORS"), NumShaderComplexityColors);
	}

	bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter << ShaderComplexityColors << MiniFontTexture << ShaderComplexityParams;
		return bShaderHasOutdatedParameters;
	}

private:

	FPostProcessPassParameters PostprocessParameter;
	FShaderParameter ShaderComplexityColors;
	FShaderResourceParameter MiniFontTexture;
	FShaderParameter ShaderComplexityParams;
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
	TShaderMapRef<FPostProcessVS> VertexShader(Context.GetShaderMap());
	TShaderMapRef<FShaderComplexityApplyPS> PixelShader(Context.GetShaderMap());

	static FGlobalBoundShaderState ShaderComplexityBoundShaderState;
	
	SetGlobalBoundShaderState(Context.RHICmdList, Context.GetFeatureLevel(), ShaderComplexityBoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);

	PixelShader->SetParameters(Context, Colors, bLegend);
	
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

	if(bLegend)
	{
		// this is a helper class for FCanvas to be able to get screen size
		class FRenderTargetTemp : public FRenderTarget
		{
		public:
			const FSceneView& View;
			const FTexture2DRHIRef Texture;

			FRenderTargetTemp(const FSceneView& InView, const FTexture2DRHIRef InTexture)
				: View(InView), Texture(InTexture)
			{
			}
			virtual FIntPoint GetSizeXY() const
			{
				return View.ViewRect.Size();
			};
			virtual const FTexture2DRHIRef& GetRenderTargetTexture() const
			{
				return Texture;
			}
		} TempRenderTarget(View, (const FTexture2DRHIRef&)DestRenderTarget.TargetableTexture);

		FCanvas Canvas(&TempRenderTarget, NULL, ViewFamily.CurrentRealTime, ViewFamily.CurrentWorldTime, ViewFamily.DeltaWorldTime, Context.GetFeatureLevel());

//later?		Canvas.DrawShadowedString(View.ViewRect.Max.X - View.ViewRect.Width() / 3 - 64 + 8, View.ViewRect.Max.Y - 80, TEXT("Overdraw"), GetStatsFont(), FLinearColor(0.7f, 0.7f, 0.7f), FLinearColor(0,0,0,0));
//later?		Canvas.DrawShadowedString(View.ViewRect.Min.X + 64 + 4, View.ViewRect.Max.Y - 80, TEXT("VS Instructions"), GetStatsFont(), FLinearColor(0.0f, 0.0f, 0.0f), FLinearColor(0,0,0,0));

		Canvas.DrawShadowedString(View.ViewRect.Min.X + 63, View.ViewRect.Max.Y - 51, TEXT("Very good"), GetStatsFont(), FLinearColor(0.5f, 0.5f, 0.5f));
		Canvas.DrawShadowedString(View.ViewRect.Min.X + 63 + (int32)(View.ViewRect.Width() * 77.0f / 397.0f) - 16, View.ViewRect.Max.Y - 51, TEXT("Good"), GetStatsFont(), FLinearColor(0.5f, 0.5f, 0.5f));
		Canvas.DrawShadowedString(View.ViewRect.Max.X - 124 + 12, View.ViewRect.Max.Y - 51, TEXT("Very bad"), GetStatsFont(), FLinearColor(0.5f, 0.5f, 0.5f));

		Canvas.DrawShadowedString(View.ViewRect.Min.X + 62, View.ViewRect.Max.Y - 87, TEXT("0"), GetStatsFont(), FLinearColor(0.5f, 0.5f, 0.5f));

		FString Line;

		Line = FString::Printf(TEXT("MaxShaderComplexityCount=%d"), (int32)GetMaxShaderComplexityCount(Context.GetFeatureLevel()));
		Canvas.DrawShadowedString(View.ViewRect.Max.X - 260, View.ViewRect.Max.Y - 88, *Line, GetStatsFont(), FLinearColor(0.5f, 0.5f, 0.5f));

		Canvas.Flush_RenderThread(Context.RHICmdList);
	}
	
	Context.RHICmdList.CopyToResolveTarget(DestRenderTarget.TargetableTexture, DestRenderTarget.ShaderResourceTexture, false, FResolveParams());
}

FPooledRenderTargetDesc FRCPassPostProcessVisualizeComplexity::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	FPooledRenderTargetDesc Ret = GetInput(ePId_Input0)->GetOutput()->RenderTargetDesc;

	Ret.Reset();
	Ret.DebugName = TEXT("VisualizeComplexity");

	return Ret;
}

