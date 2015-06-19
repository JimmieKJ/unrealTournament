// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PostProcessMaterial.cpp: Post processing Material implementation.
=============================================================================*/

#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "SceneFilterRendering.h"
#include "PostProcessMaterial.h"
#include "PostProcessing.h"
#include "PostProcessEyeAdaptation.h"
#include "../../../Engine/Public/TileRendering.h"
#include "SceneUtils.h"

class FPostProcessMaterialVS : public FMaterialShader
{
	DECLARE_SHADER_TYPE(FPostProcessMaterialVS, Material);
public:

	/**
	  * Only compile these shaders for post processing domain materials
	  */
	static bool ShouldCache(EShaderPlatform Platform, const FMaterial* Material)
	{
		return (Material->GetMaterialDomain() == MD_PostProcess) && IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const class FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		FMaterialShader::ModifyCompilationEnvironment(Platform, OutEnvironment);

		OutEnvironment.SetDefine(TEXT("POST_PROCESS_MATERIAL"), 1);
	}


	FPostProcessMaterialVS( )	{ }
	FPostProcessMaterialVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FMaterialShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
	}

	void SetParameters(FRHICommandList& RHICmdList, const FRenderingCompositePassContext& Context )
	{
		const FVertexShaderRHIParamRef ShaderRHI = GetVertexShader();
		FMaterialShader::SetParameters(RHICmdList, ShaderRHI, Context.View);
		PostprocessParameter.SetVS(ShaderRHI, Context, TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI());
	}

	// Begin FShader interface
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FMaterialShader::Serialize(Ar);
		Ar << PostprocessParameter;
		return bShaderHasOutdatedParameters;
	}
	//  End FShader interface 
private:
	FPostProcessPassParameters PostprocessParameter;
};

IMPLEMENT_MATERIAL_SHADER_TYPE(,FPostProcessMaterialVS,TEXT("PostProcessMaterialShaders"),TEXT("MainVS"),SF_Vertex);

/**
 * A pixel shader for rendering a post process material
 */
class FPostProcessMaterialPS : public FMaterialShader
{
	DECLARE_SHADER_TYPE(FPostProcessMaterialPS,Material);
public:

	/**
	  * Only compile these shaders for post processing domain materials
	  */
	static bool ShouldCache(EShaderPlatform Platform, const FMaterial* Material)
	{
		return (Material->GetMaterialDomain() == MD_PostProcess) && IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const class FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		FMaterialShader::ModifyCompilationEnvironment(Platform, OutEnvironment);

		OutEnvironment.SetDefine(TEXT("POST_PROCESS_MATERIAL"), 1);
	}

	FPostProcessMaterialPS() {}
	FPostProcessMaterialPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FMaterialShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
	}

	void SetParameters(FRHICommandList& RHICmdList, const FRenderingCompositePassContext& Context, const FMaterialRenderProxy* Material )
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		FMaterialShader::SetParameters(RHICmdList, ShaderRHI, Material, *Material->GetMaterial(Context.View.GetFeatureLevel()), Context.View, true, ESceneRenderTargetsMode::SetTextures);
		PostprocessParameter.SetPS(ShaderRHI, Context, TStaticSamplerState<SF_Point,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI());
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FMaterialShader::Serialize(Ar);
		Ar << PostprocessParameter;
		return bShaderHasOutdatedParameters;
	}

private:
	FPostProcessPassParameters PostprocessParameter;
};

IMPLEMENT_MATERIAL_SHADER_TYPE(,FPostProcessMaterialPS,TEXT("PostProcessMaterialShaders"),TEXT("MainPS"),SF_Pixel);

FRCPassPostProcessMaterial::FRCPassPostProcessMaterial(UMaterialInterface* InMaterialInterface, ERHIFeatureLevel::Type InFeatureLevel, EPixelFormat OutputFormatIN)
: MaterialInterface(InMaterialInterface), OutputFormat(OutputFormatIN)
{
	FMaterialRenderProxy* Proxy = MaterialInterface->GetRenderProxy(false);
	check(Proxy);

	const FMaterial* Material = Proxy->GetMaterialNoFallback(InFeatureLevel);
	
	if (!Material || Material->GetMaterialDomain() != MD_PostProcess)
	{
		MaterialInterface = UMaterial::GetDefaultMaterial(MD_PostProcess);
	}
}

void FRCPassPostProcessMaterial::Process(FRenderingCompositePassContext& Context)
{
	FMaterialRenderProxy* Proxy = MaterialInterface->GetRenderProxy(false);

	check(Proxy);

	const FMaterial* Material = Proxy->GetMaterial(Context.View.GetFeatureLevel());
	
	check(Material);

	SCOPED_DRAW_EVENTF(Context.RHICmdList, PostProcessMaterial, TEXT("PostProcessMaterial Material=%s"), *Material->GetFriendlyName());

	const FPooledRenderTargetDesc* InputDesc = GetInputDesc(ePId_Input0);

	if(!InputDesc)
	{
		// input is not hooked up correctly
		return;
	}

	const FSceneView& View = Context.View;
	const FSceneViewFamily& ViewFamily = *(View.Family);

	// hacky cast
	FRenderingCompositePassContext RenderingCompositePassContext(Context.RHICmdList, (FViewInfo&)View);
	RenderingCompositePassContext.Pass = this;

	FIntRect SrcRect = View.ViewRect;
	FIntRect DestRect = View.ViewRect;
	FIntPoint SrcSize = InputDesc->Extent;

	const FSceneRenderTargetItem& DestRenderTarget = PassOutputs[0].RequestSurface(Context);

	// Set the view family's render target/viewport.
	SetRenderTarget(Context.RHICmdList, DestRenderTarget.TargetableTexture, FTextureRHIParamRef());

	if( ViewFamily.RenderTarget->GetRenderTargetTexture() != DestRenderTarget.TargetableTexture )
	{
		Context.RHICmdList.Clear(true, FLinearColor::Black, false, 1.0f, false, 0, View.ViewRect);
	}

	Context.SetViewportAndCallRHI(View.ViewRect);

	// set the state
	Context.RHICmdList.SetBlendState(TStaticBlendState<>::GetRHI());
	Context.RHICmdList.SetRasterizerState(TStaticRasterizerState<>::GetRHI());
	Context.RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());

	float ScaleX = 1.0f / InputDesc->Extent.X;
	float ScaleY = 1.0f / InputDesc->Extent.Y;

	const FMaterialShaderMap* MaterialShaderMap = Material->GetRenderingThreadShaderMap();
	FPostProcessMaterialPS* PixelShader = MaterialShaderMap->GetShader<FPostProcessMaterialPS>();
	FPostProcessMaterialVS* VertexShader = MaterialShaderMap->GetShader<FPostProcessMaterialVS>();

	Context.RHICmdList.SetLocalBoundShaderState(Context.RHICmdList.BuildLocalBoundShaderState(GetVertexDeclarationFVector4(), VertexShader->GetVertexShader(), FHullShaderRHIRef(), FDomainShaderRHIRef(), PixelShader->GetPixelShader(), FGeometryShaderRHIRef()));

	VertexShader->SetParameters(Context.RHICmdList, Context);
	PixelShader->SetParameters(Context.RHICmdList, Context, MaterialInterface->GetRenderProxy(false));

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

	Context.RHICmdList.CopyToResolveTarget(DestRenderTarget.TargetableTexture, DestRenderTarget.ShaderResourceTexture, false, FResolveParams());

	if(Material->NeedsGBuffer())
	{
		GSceneRenderTargets.AdjustGBufferRefCount(-1);
	}
}

FPooledRenderTargetDesc FRCPassPostProcessMaterial::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	FPooledRenderTargetDesc Ret = PassInputs[0].GetOutput()->RenderTargetDesc;

	if (OutputFormat != PF_Unknown)
	{
		Ret.Format = OutputFormat;
	}
	Ret.Reset();
	Ret.DebugName = TEXT("PostProcessMaterial");

	return Ret;
}
