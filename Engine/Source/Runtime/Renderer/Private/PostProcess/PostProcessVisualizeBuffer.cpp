// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PostProcessVisualizeBuffer.cpp: Post processing VisualizeBuffer implementation.
=============================================================================*/

#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "SceneFilterRendering.h"
#include "PostProcessVisualizeBuffer.h"
#include "PostProcessing.h"
#include "PostProcessHistogram.h"
#include "PostProcessEyeAdaptation.h"
#include "SceneUtils.h"

/** Encapsulates the post processing Buffer visualization pixel shader. */
template<bool bDrawingTile>
class FPostProcessVisualizeBufferPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessVisualizeBufferPS, Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::ES3_1);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("DRAWING_TILE"), bDrawingTile ? TEXT("1") : TEXT("0"));
	}

	/** Default constructor. */
	FPostProcessVisualizeBufferPS() {}

public:
	FPostProcessPassParameters PostprocessParameter;
	FDeferredPixelShaderParameters DeferredParameters;
	FShaderResourceParameter SourceTexture;
	FShaderResourceParameter SourceTextureSampler;

	/** Initialization constructor. */
	FPostProcessVisualizeBufferPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
		DeferredParameters.Bind(Initializer.ParameterMap);

		if (bDrawingTile)
		{
			SourceTexture.Bind(Initializer.ParameterMap, TEXT("PostprocessInput0"));
			SourceTextureSampler.Bind(Initializer.ParameterMap, TEXT("PostprocessInput0Sampler"));
		}
	}

	void SetPS(const FRenderingCompositePassContext& Context)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		FGlobalShader::SetParameters(Context.RHICmdList, ShaderRHI, Context.View);
		PostprocessParameter.SetPS(ShaderRHI, Context, TStaticSamplerState<SF_Point,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI());
		DeferredParameters.Set(Context.RHICmdList, ShaderRHI, Context.View);
	}

	void SetSourceTexture(FRHICommandList& RHICmdList, FTextureRHIRef Texture)
	{
		if (bDrawingTile && SourceTexture.IsBound())
		{
			const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

			SetTextureParameter(
				RHICmdList, 
				ShaderRHI,
				SourceTexture,
				SourceTextureSampler,
				TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(),
				Texture);
		}
	}
	
	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter << DeferredParameters << SourceTexture << SourceTextureSampler;
		return bShaderHasOutdatedParameters;
	}

	static const TCHAR* GetSourceFilename()
	{
		return TEXT("PostProcessVisualizeBuffer");
	}

	static const TCHAR* GetFunctionName()
	{
		return TEXT("MainPS");
	}
};

void FRCPassPostProcessVisualizeBuffer::AddVisualizationBuffer(FRenderingCompositeOutputRef InSource, const FString& InName)
{
	Tiles.Add(TileData(InSource, InName));

	if (InSource.IsValid())
	{
		AddDependency(InSource);
	}
}

IMPLEMENT_SHADER_TYPE2(FPostProcessVisualizeBufferPS<true>, SF_Pixel);
IMPLEMENT_SHADER_TYPE2(FPostProcessVisualizeBufferPS<false>, SF_Pixel);

template <bool bDrawingTile>
FShader* FRCPassPostProcessVisualizeBuffer::SetShaderTempl(const FRenderingCompositePassContext& Context)
{
	TShaderMapRef<FPostProcessVS> VertexShader(Context.GetShaderMap());
	TShaderMapRef<FPostProcessVisualizeBufferPS<bDrawingTile> > PixelShader(Context.GetShaderMap());

	static FGlobalBoundShaderState BoundShaderState;
	

	SetGlobalBoundShaderState(Context.RHICmdList, Context.GetFeatureLevel(), BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);

	PixelShader->SetPS(Context);

	return *VertexShader;
}

void FRCPassPostProcessVisualizeBuffer::Process(FRenderingCompositePassContext& Context)
{
	SCOPED_DRAW_EVENT(Context.RHICmdList, VisualizeBuffer);
	const FPooledRenderTargetDesc* InputDesc = GetInputDesc(ePId_Input0);

	if(!InputDesc)
	{
		// input is not hooked up correctly
		return;
	}

	const FSceneView& View = Context.View;
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

	{
		FShader* VertexShader = SetShaderTempl<false>(Context);

	// Draw a quad mapping scene color to the view's render target
	DrawRectangle(
		Context.RHICmdList,
		0, 0,
		DestRect.Width(), DestRect.Height(),
		SrcRect.Min.X, SrcRect.Min.Y,
		SrcRect.Width(), SrcRect.Height(),
		DestRect.Size(),
		SrcSize,
			VertexShader,
		EDRF_UseTriangleOptimization);
	}

	Context.RHICmdList.SetBlendState(TStaticBlendState<CW_RGB, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha>::GetRHI());

	TShaderMapRef<FPostProcessVS> VertexShader(Context.GetShaderMap());
	TShaderMapRef<FPostProcessVisualizeBufferPS<true> > PixelShader(Context.GetShaderMap());

	static FGlobalBoundShaderState BoundShaderState;
	
	SetGlobalBoundShaderState(Context.RHICmdList, Context.GetFeatureLevel(), BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);

	PixelShader->SetPS(Context);

	// Track the name and position of each tile we draw so we can write text labels over them
	struct LabelRecord
	{
		FString Label;
		int32 LocationX;
		int32 LocationY;
	};

	TArray<LabelRecord> Labels;

	const int32 MaxTilesX = 4;
	const int32 MaxTilesY = 4;
	const int32 TileWidth = DestRect.Width() / MaxTilesX;
	const int32 TileHeight = DestRect.Height() / MaxTilesY;
	int32 CurrentTileIndex = 0; 

	for (TArray<TileData>::TConstIterator It = Tiles.CreateConstIterator(); It; ++It, ++CurrentTileIndex)
	{
		FRenderingCompositeOutputRef Tile = It->Source;

		if (Tile.IsValid())
		{
			FTextureRHIRef Texture = Tile.GetOutput()->PooledRenderTarget->GetRenderTargetItem().TargetableTexture;

			int32 TileX = CurrentTileIndex % MaxTilesX;
			int32 TileY = CurrentTileIndex / MaxTilesX;

			PixelShader->SetSourceTexture(Context.RHICmdList, Texture);

			DrawRectangle(
				Context.RHICmdList,
				TileX * TileWidth, TileY * TileHeight,
				TileWidth, TileHeight,
				SrcRect.Min.X, SrcRect.Min.Y,
				SrcRect.Width(), SrcRect.Height(),
				DestRect.Size(),
				SrcSize,
				*VertexShader,
				EDRF_Default
				);

			Labels.Add(LabelRecord());
			Labels.Last().Label = It->Name;
			Labels.Last().LocationX = 8 + TileX * TileWidth;
			Labels.Last().LocationY = (TileY + 1) * TileHeight - 19;
		}
	}

	// Draw tile labels

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
	FLinearColor LabelColor(1, 1, 0);
	for (auto It = Labels.CreateConstIterator(); It; ++It)
	{
		Canvas.DrawShadowedString(It->LocationX, It->LocationY, *It->Label, GetStatsFont(), LabelColor);
	}
	Canvas.Flush_RenderThread(Context.RHICmdList);


	Context.RHICmdList.CopyToResolveTarget(DestRenderTarget.TargetableTexture, DestRenderTarget.ShaderResourceTexture, false, FResolveParams());
}

FPooledRenderTargetDesc FRCPassPostProcessVisualizeBuffer::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	FPooledRenderTargetDesc Ret = GetInput(ePId_Input0)->GetOutput()->RenderTargetDesc;

	Ret.Reset();
	Ret.DebugName = TEXT("VisualizeBuffer");

	return Ret;
}
