// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PostProcessVisualizeShadingModels.cpp: Post processing VisualizeShadingModels implementation.
=============================================================================*/

#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "SceneFilterRendering.h"
#include "VisualizeShadingModels.h"
#include "PostProcessing.h"
#include "SceneUtils.h"

/** Encapsulates the post processing eye adaptation pixel shader. */
class FPostProcessVisualizeShadingModelsPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessVisualizeShadingModelsPS, Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
	}

	/** Default constructor. */
	FPostProcessVisualizeShadingModelsPS() {}

public:
	FPostProcessPassParameters PostprocessParameter;
	FDeferredPixelShaderParameters DeferredParameters;
	FShaderParameter ShadingModelMaskInView;

	/** Initialization constructor. */
	FPostProcessVisualizeShadingModelsPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
		DeferredParameters.Bind(Initializer.ParameterMap);
		ShadingModelMaskInView.Bind(Initializer.ParameterMap,TEXT("ShadingModelMaskInView"));
	}

	void SetPS(const FRenderingCompositePassContext& Context, uint16 InShadingModelMaskInView)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();
		
		FGlobalShader::SetParameters(Context.RHICmdList, ShaderRHI, Context.View);

		PostprocessParameter.SetPS(ShaderRHI, Context, TStaticSamplerState<SF_Point,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI());

		DeferredParameters.Set(Context.RHICmdList, ShaderRHI, Context.View);

		static FLinearColor SoftBits[sizeof(InShadingModelMaskInView) * 8] = {};	// init with 0.0f

		for(uint32 i = 0; i < sizeof(InShadingModelMaskInView) * 8; ++i)
		{
			float& ref = SoftBits[i].R;

			ref -= Context.View.Family->DeltaWorldTime;
			ref = FMath::Max(0.0f, ref);

			if(InShadingModelMaskInView & (1 << i))
			{
				ref = 1.0f;
			}
		}

		SetShaderValueArray(Context.RHICmdList, ShaderRHI, ShadingModelMaskInView, SoftBits, sizeof(InShadingModelMaskInView) * 8);
	}
	
	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter << DeferredParameters << ShadingModelMaskInView;
		return bShaderHasOutdatedParameters;
	}
};

IMPLEMENT_SHADER_TYPE(,FPostProcessVisualizeShadingModelsPS,TEXT("VisualizeShadingModels"),TEXT("MainPS"),SF_Pixel);

FRCPassPostProcessVisualizeShadingModels::FRCPassPostProcessVisualizeShadingModels(FRHICommandList& RHICmdList)
{
	// AdjustGBufferRefCount(-1) call is done when the pass gets executed
	FSceneRenderTargets::Get_Todo_PassContext().AdjustGBufferRefCount(RHICmdList, 1);
}

void FRCPassPostProcessVisualizeShadingModels::Process(FRenderingCompositePassContext& Context)
{
	SCOPED_DRAW_EVENT(Context.RHICmdList, PostProcessVisualizeShadingModels);
	const FPooledRenderTargetDesc* InputDesc = GetInputDesc(ePId_Input0);

	const FSceneView& View = Context.View;
	const FViewInfo& ViewInfo = Context.View;
	const FSceneViewFamily& ViewFamily = *(View.Family);
	
	FIntRect SrcRect = View.ViewRect;
	FIntRect DestRect = View.ViewRect;
	FIntPoint SrcSize = InputDesc->Extent;

	const FSceneRenderTargetItem& DestRenderTarget = PassOutputs[0].RequestSurface(Context);

	// Set the view family's render target/viewport.
	SetRenderTarget(Context.RHICmdList, DestRenderTarget.TargetableTexture, FTextureRHIRef());
	Context.SetViewportAndCallRHI(DestRect);

	// set the state
	Context.RHICmdList.SetBlendState(TStaticBlendState<>::GetRHI());
	Context.RHICmdList.SetRasterizerState(TStaticRasterizerState<>::GetRHI());
	Context.RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());

	TShaderMapRef<FPostProcessVS> VertexShader(Context.GetShaderMap());
	TShaderMapRef<FPostProcessVisualizeShadingModelsPS> PixelShader(Context.GetShaderMap());

	static FGlobalBoundShaderState BoundShaderState;	

	SetGlobalBoundShaderState(Context.RHICmdList, Context.GetFeatureLevel(), BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);

	PixelShader->SetPS(Context, ((FViewInfo&)View).ShadingModelMaskInView);

	// Draw a quad mapping scene color to the view's render target
	DrawRectangle(
		Context.RHICmdList,
		DestRect.Min.X, DestRect.Min.Y,
		DestRect.Width(), DestRect.Height(),
		SrcRect.Min.X, SrcRect.Min.Y,
		SrcRect.Width(), SrcRect.Height(),
		DestRect.Size(),
		SrcSize,
		*VertexShader,
		EDRF_UseTriangleOptimization);

	FRenderTargetTemp TempRenderTarget(View, (const FTexture2DRHIRef&)DestRenderTarget.TargetableTexture);
	FCanvas Canvas(&TempRenderTarget, NULL, 0, 0, 0, Context.GetFeatureLevel());

	float X = 30;
	float Y = 28;
	const float YStep = 14;
	const float ColumnWidth = 250;

	FString Line;

	Canvas.DrawShadowedString( X, Y += YStep, TEXT("Visualize ShadingModels (mostly to track down bugs)"), GetStatsFont(), FLinearColor(1, 1, 1));

	Y = 160 - YStep - 4;
	
	uint32 Value = ((FViewInfo&)View).ShadingModelMaskInView;

	Line = FString::Printf(TEXT("View.ShadingModelMaskInView = 0x%x"), Value);
	Canvas.DrawShadowedString( X, Y, *Line, GetStatsFont(), FLinearColor(0.5f, 0.5f, 0.5f));
	Y += YStep;

	UEnum* Enum = FindObject<UEnum>(NULL, TEXT("Engine.EMaterialShadingModel"));
	check(Enum);

	Y += 5;

	for(uint32 i = 0; i < MSM_MAX; ++i)
	{
		FString Name = Enum->GetEnumName(i);
		Line = FString::Printf(TEXT("%d.  %s"), i, *Name);

		bool bThere = (Value & (1 << i)) != 0;

		Canvas.DrawShadowedString(X + 30, Y, *Line, GetStatsFont(), bThere ? FLinearColor(1, 1, 1) : FLinearColor(0, 0, 0) );
		Y += 20;
	}

	Line = FString::Printf(TEXT("(On CPU, based on what gets rendered)"));
	Canvas.DrawShadowedString( X, Y, *Line, GetStatsFont(), FLinearColor(0.5f, 0.5f, 0.5f)); Y += YStep;

	Canvas.Flush_RenderThread(Context.RHICmdList);

	Context.RHICmdList.CopyToResolveTarget(DestRenderTarget.TargetableTexture, DestRenderTarget.ShaderResourceTexture, false, FResolveParams());

	// AdjustGBufferRefCount(1) call is done in constructor
	FSceneRenderTargets::Get(Context.RHICmdList).AdjustGBufferRefCount(Context.RHICmdList, -1);
}

FPooledRenderTargetDesc FRCPassPostProcessVisualizeShadingModels::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	FPooledRenderTargetDesc Ret = GetInput(ePId_Input0)->GetOutput()->RenderTargetDesc;

	Ret.Reset();
	Ret.DebugName = TEXT("VisualizeShadingModels");

	return Ret;
}
