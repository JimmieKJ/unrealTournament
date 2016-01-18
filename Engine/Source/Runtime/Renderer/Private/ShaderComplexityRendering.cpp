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

IMPLEMENT_MATERIAL_SHADER_TYPE(,FQuadOverdrawVS,TEXT("QuadOverdrawVertexShader"),TEXT("Main"),SF_Vertex);	
IMPLEMENT_MATERIAL_SHADER_TYPE(,FQuadOverdrawHS,TEXT("QuadOverdrawVertexShader"),TEXT("MainHull"),SF_Hull);	
IMPLEMENT_MATERIAL_SHADER_TYPE(,FQuadOverdrawDS,TEXT("QuadOverdrawVertexShader"),TEXT("MainDomain"),SF_Domain);

/**
* The number of shader complexity colors from the engine ini that will be passed to the shader. 
* Changing this requires a recompile of the FShaderComplexityApplyPS.
*/
const uint32 NumShaderComplexityColors = 9;
const float NormalizedQuadComplexityValue = 1.f / (float)(NumShaderComplexityColors - 1);

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
	EQuadOverdrawMode QuadOverdrawMode,
	ERHIFeatureLevel::Type InFeatureLevel)
{
	float NormalizeMul = 1.0f / GetMaxShaderComplexityCount(InFeatureLevel);

	// normalize the complexity so we can fit it in a low precision scene color which is necessary on some platforms
	// late value is for overdraw which can be problmatic with a low precision float format, at some point the precision isn't there any more and it doesn't accumulate
	FVector Value = QuadOverdrawMode == QOM_QuadComplexity ? FVector(NormalizedQuadComplexityValue) : FVector(NumPixelInstructions * NormalizeMul, NumVertexInstructions * NormalizeMul, 1/32.0f);

	// Disable UAVs if something is wrong
	if (QuadOverdrawMode != QOM_None)
	{
		const FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);
		if (QuadBufferUAV.IsBound() && SceneContext.GetQuadOverdrawIndex() != QuadBufferUAV.GetBaseIndex())
		{
			QuadOverdrawMode = QOM_None;
		}
	}

	SetShaderValue(RHICmdList, FGlobalShader::GetPixelShader(), NormalizedComplexity, Value);
	SetShaderValue(RHICmdList, FGlobalShader::GetPixelShader(), ShowQuadOverdraw, QuadOverdrawMode != QOM_None);
}

template <bool bQuadComplexity>
void TComplexityAccumulatePS<bQuadComplexity>::SetParameters(
		TShaderMap<FGlobalShaderType>* ShaderMap,
		FRHICommandList& RHICmdList, 
		uint32 NumVertexInstructions, 
		uint32 NumPixelInstructions,
		EQuadOverdrawMode QuadOverdrawMode,
		ERHIFeatureLevel::Type InFeatureLevel)
{
	TShaderMapRef< TComplexityAccumulatePS<bQuadComplexity> > ShaderRef(ShaderMap);
	ShaderRef->FShaderComplexityAccumulatePS::SetParameters(RHICmdList, NumVertexInstructions, NumPixelInstructions, QuadOverdrawMode, InFeatureLevel);
}

template <bool bQuadComplexity>
FShader* TComplexityAccumulatePS<bQuadComplexity>::GetPixelShader(TShaderMap<FGlobalShaderType>* ShaderMap)
{
	TShaderMapRef< TComplexityAccumulatePS<bQuadComplexity> > ShaderRef(ShaderMap);
	return *ShaderRef;
}

void FShaderComplexityAccumulatePS::SetParameters(
		TShaderMap<FGlobalShaderType>* ShaderMap,
		FRHICommandList& RHICmdList, 
		uint32 NumVertexInstructions, 
		uint32 NumPixelInstructions,
		EQuadOverdrawMode QuadOverdrawMode,
		ERHIFeatureLevel::Type InFeatureLevel)
{
	if (QuadOverdrawMode == QOM_QuadComplexity || QuadOverdrawMode == QOM_ShaderComplexityBleeding)
	{
		TQuadComplexityAccumulatePS::SetParameters(ShaderMap, RHICmdList, NumVertexInstructions, NumPixelInstructions, QuadOverdrawMode, InFeatureLevel);
	}
	else
	{
		TShaderComplexityAccumulatePS::SetParameters(ShaderMap, RHICmdList, NumVertexInstructions, NumPixelInstructions, QuadOverdrawMode, InFeatureLevel);
	}
}

FShader* FShaderComplexityAccumulatePS::GetPixelShader(TShaderMap<FGlobalShaderType>* ShaderMap, EQuadOverdrawMode QuadOverdrawMode)
{
	if (QuadOverdrawMode == QOM_QuadComplexity || QuadOverdrawMode == QOM_ShaderComplexityBleeding)
	{
		return TQuadComplexityAccumulatePS::GetPixelShader(ShaderMap);
	}
	else
	{
		return TShaderComplexityAccumulatePS::GetPixelShader(ShaderMap);
	}
}

IMPLEMENT_SHADER_TYPE(template<>,TShaderComplexityAccumulatePS,TEXT("ShaderComplexityAccumulatePixelShader"),TEXT("Main"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>,TQuadComplexityAccumulatePS,TEXT("QuadComplexityAccumulatePixelShader"),TEXT("Main"),SF_Pixel);

void PatchBoundShaderStateInputForQuadOverdraw(
		FBoundShaderStateInput& BoundShaderStateInput,
		const FMaterial* MaterialResource,
		const FVertexFactory* VertexFactory,
		ERHIFeatureLevel::Type FeatureLevel, 
		EQuadOverdrawMode QuadOverdrawMode
		)
{
	TShaderMap<FGlobalShaderType>* GlobalShaderMap = GetGlobalShaderMap(FeatureLevel);
	FVertexFactoryType* VertexFactoryType = VertexFactory->GetType();

	if (!MaterialResource->HasVertexPositionOffsetConnected() && MaterialResource->GetTessellationMode() == MTM_NoTessellation)
	{
		MaterialResource = UMaterial::GetDefaultMaterial(MD_Surface)->GetRenderProxy(false)->GetMaterial(FeatureLevel);
	}

	BoundShaderStateInput.VertexShaderRHI = MaterialResource->GetShader<FQuadOverdrawVS>(VertexFactoryType)->GetVertexShader();

	if (BoundShaderStateInput.HullShaderRHI)
	{
		BoundShaderStateInput.HullShaderRHI = MaterialResource->GetShader<FQuadOverdrawHS>(VertexFactoryType)->GetHullShader();
	}
	if (BoundShaderStateInput.DomainShaderRHI)
	{
		BoundShaderStateInput.DomainShaderRHI = MaterialResource->GetShader<FQuadOverdrawDS>(VertexFactoryType)->GetDomainShader();
	}

	if (QuadOverdrawMode == QOM_QuadComplexity || QuadOverdrawMode == QOM_ShaderComplexityBleeding)
	{
		BoundShaderStateInput.PixelShaderRHI = TQuadComplexityAccumulatePS::GetPixelShader(GlobalShaderMap)->GetPixelShader();
	}
	else
	{
		BoundShaderStateInput.PixelShaderRHI = TShaderComplexityAccumulatePS::GetPixelShader(GlobalShaderMap)->GetPixelShader();
	}
}

void SetNonPSParametersForQuadOverdraw(
	FRHICommandList& RHICmdList, 
	const FMaterialRenderProxy* MaterialRenderProxy, 
	const FMaterial* MaterialResource, 
	const FSceneView& View,
	const FVertexFactory* VertexFactory,
	bool bHasHullAndDomainShader
	)
{
	VertexFactory->Set(RHICmdList);

	FVertexFactoryType* VertexFactoryType = VertexFactory->GetType();
	
	if (!MaterialResource->HasVertexPositionOffsetConnected() && MaterialResource->GetTessellationMode() == MTM_NoTessellation)
	{
		MaterialRenderProxy = UMaterial::GetDefaultMaterial(MD_Surface)->GetRenderProxy(false);
		MaterialResource = UMaterial::GetDefaultMaterial(MD_Surface)->GetRenderProxy(false)->GetMaterial(View.GetFeatureLevel());
	}

	MaterialResource->GetShader<FQuadOverdrawVS>(VertexFactoryType)->SetParameters(RHICmdList, MaterialRenderProxy, *MaterialResource, View);

	if (bHasHullAndDomainShader)
	{
		MaterialResource->GetShader<FQuadOverdrawHS>(VertexFactoryType)->SetParameters(RHICmdList, MaterialRenderProxy, View);
		MaterialResource->GetShader<FQuadOverdrawDS>(VertexFactoryType)->SetParameters(RHICmdList, MaterialRenderProxy, View);
	}
}

void SetMeshForQuadOverdraw(
	FRHICommandList& RHICmdList, 
	const FMaterial* MaterialResource, 
	const FSceneView& View,
	const FVertexFactory* VertexFactory,
	bool bHasHullAndDomainShader,
	const FPrimitiveSceneProxy* Proxy,
	const FMeshBatchElement& BatchElement, 
	float DitheredLODTransitionValue
	)
{
	FVertexFactoryType* VertexFactoryType = VertexFactory->GetType();
	
	if (!MaterialResource->HasVertexPositionOffsetConnected() && MaterialResource->GetTessellationMode() == MTM_NoTessellation)
	{
		MaterialResource = UMaterial::GetDefaultMaterial(MD_Surface)->GetRenderProxy(false)->GetMaterial(View.GetFeatureLevel());
	}

	MaterialResource->GetShader<FQuadOverdrawVS>(VertexFactoryType)->SetMesh(RHICmdList, VertexFactory, View, Proxy, BatchElement, DitheredLODTransitionValue);

	if (bHasHullAndDomainShader)
	{
		MaterialResource->GetShader<FQuadOverdrawHS>(VertexFactoryType)->SetMesh(RHICmdList, VertexFactory, View, Proxy, BatchElement, DitheredLODTransitionValue);
		MaterialResource->GetShader<FQuadOverdrawDS>(VertexFactoryType)->SetMesh(RHICmdList, VertexFactory, View, Proxy, BatchElement, DitheredLODTransitionValue);
	}
}



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
		QuadOverdrawTexture.Bind(Initializer.ParameterMap,TEXT("QuadOverdrawTexture"));
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

		SetTextureParameter(Context.RHICmdList, ShaderRHI, MiniFontTexture, GEngine->MiniFontTexture ? GEngine->MiniFontTexture->Resource->TextureRHI : GSystemTextures.WhiteDummy->GetRenderTargetItem().TargetableTexture);

		// Whether acccess or not the QuadOverdraw buffer.
		EQuadOverdrawMode QuadOverdrawMode = Context.View.Family->GetQuadOverdrawMode();

		if (QuadOverdrawTexture.IsBound())
		{
			const FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(Context.RHICmdList);
			if (SceneContext.QuadOverdrawBuffer.IsValid() && SceneContext.QuadOverdrawBuffer->GetRenderTargetItem().ShaderResourceTexture.IsValid())
			{
				Context.RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EGfxToGfx, SceneContext.QuadOverdrawBuffer->GetRenderTargetItem().UAV);
				SetTextureParameter(Context.RHICmdList, ShaderRHI, QuadOverdrawTexture, SceneContext.QuadOverdrawBuffer->GetRenderTargetItem().ShaderResourceTexture);
			}
			else
			{
				SetTextureParameter(Context.RHICmdList, ShaderRHI, QuadOverdrawTexture, FTextureRHIRef());
				QuadOverdrawMode = QOM_None;
			}
		}

		SetShaderValue(Context.RHICmdList, ShaderRHI, ShaderComplexityParams, FVector4(bLegend, QuadOverdrawMode != QOM_None, QuadOverdrawMode == QOM_QuadComplexity, 0));
	}

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return true;
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("READ_QUAD_OVERDRAW"), AllowQuadOverdraw(Platform) ? TEXT("1") : TEXT("0"));
		OutEnvironment.SetDefine(TEXT("NUM_COMPLEXITY_COLORS"), NumShaderComplexityColors);
	}

	bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter << ShaderComplexityColors << MiniFontTexture << ShaderComplexityParams << QuadOverdrawTexture;
		return bShaderHasOutdatedParameters;

	}

private:

	FPostProcessPassParameters PostprocessParameter;
	FShaderParameter ShaderComplexityColors;
	FShaderResourceParameter MiniFontTexture;
	FShaderParameter ShaderComplexityParams;
	FShaderResourceParameter QuadOverdrawTexture;
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

		if (View.Family->GetQuadOverdrawMode() == QOM_QuadComplexity)
		{
			int32 StartX = View.ViewRect.Min.X + 62;
			int32 EndX = View.ViewRect.Max.X - 66;
			int32 NumOffset = (EndX - StartX) / (NumShaderComplexityColors - 1);
			for (int32 PosX = StartX, Number = 0; PosX <= EndX; PosX += NumOffset, ++Number)
			{
				FString Line;
				Line = FString::Printf(TEXT("%d"), Number);
				Canvas.DrawShadowedString(PosX, View.ViewRect.Max.Y - 87, *Line, GetStatsFont(), FLinearColor(0.5f, 0.5f, 0.5f));
			}
		}
		else
		{
			Canvas.DrawShadowedString(View.ViewRect.Min.X + 63, View.ViewRect.Max.Y - 51, TEXT("Very good"), GetStatsFont(), FLinearColor(0.5f, 0.5f, 0.5f));
			Canvas.DrawShadowedString(View.ViewRect.Min.X + 63 + (int32)(View.ViewRect.Width() * 77.0f / 397.0f) - 16, View.ViewRect.Max.Y - 51, TEXT("Good"), GetStatsFont(), FLinearColor(0.5f, 0.5f, 0.5f));
			Canvas.DrawShadowedString(View.ViewRect.Max.X - 124 + 12, View.ViewRect.Max.Y - 51, TEXT("Very bad"), GetStatsFont(), FLinearColor(0.5f, 0.5f, 0.5f));

			Canvas.DrawShadowedString(View.ViewRect.Min.X + 62, View.ViewRect.Max.Y - 87, TEXT("0"), GetStatsFont(), FLinearColor(0.5f, 0.5f, 0.5f));

			FString Line;
			Line = FString::Printf(TEXT("MaxShaderComplexityCount=%d"), (int32)GetMaxShaderComplexityCount(Context.GetFeatureLevel()));
			Canvas.DrawShadowedString(View.ViewRect.Max.X - 260, View.ViewRect.Max.Y - 88, *Line, GetStatsFont(), FLinearColor(0.5f, 0.5f, 0.5f));
		}

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

