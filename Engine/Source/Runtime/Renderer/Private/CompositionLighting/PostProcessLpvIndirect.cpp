//-----------------------------------------------------------------------------
// File:		PostProcessLpvIndirect.cpp
//
// Summary:		Light propagation volume postprocessing
//
// Created:		11/03/2013
//
// Author:		mailto:benwood@microsoft.com
//
//				Copyright (C) Microsoft. All rights reserved.
//-----------------------------------------------------------------------------

#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "PostProcess/SceneFilterRendering.h"
#include "PostProcess/PostProcessing.h"
#include "PostProcessLpvIndirect.h"
#include "LightPropagationVolume.h"
#include "UniformBuffer.h"
#include "SceneUtils.h"

IMPLEMENT_UNIFORM_BUFFER_STRUCT(FLpvReadUniformBufferParameters,TEXT("LpvRead"));
typedef TUniformBufferRef<FLpvReadUniformBufferParameters> FLpvReadUniformBufferRef;

/** Encapsulates the post processing ambient pixel shader. */
class FPostProcessLpvIndirectPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessLpvIndirectPS, Global);

	//@todo-rco: Remove this when reenabling for OpenGL
	static bool ShouldCache( EShaderPlatform Platform )		{ return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && !IsOpenGLPlatform(Platform); }

	/** Default constructor. */
	FPostProcessLpvIndirectPS() {}

public:
	FPostProcessPassParameters PostprocessParameter;

#if LPV_VOLUME_TEXTURE
	FShaderResourceParameter LpvBufferSRVParameters[7];
	FShaderResourceParameter LpvVolumeTextureSampler;
#else
	FShaderResourceParameter LpvBufferSRV;
#endif 

	FDeferredPixelShaderParameters DeferredParameters;
	FShaderResourceParameter PreIntegratedGF;
	FShaderResourceParameter PreIntegratedGFSampler;

	/** Initialization constructor. */
	FPostProcessLpvIndirectPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
		DeferredParameters.Bind(Initializer.ParameterMap);
#if LPV_VOLUME_TEXTURE
		for ( int i=0; i<7; i++ )
		{
			LpvBufferSRVParameters[i].Bind( Initializer.ParameterMap, LpvVolumeTextureSRVNames[i] );
		}
		LpvVolumeTextureSampler.Bind(Initializer.ParameterMap, TEXT("gLpv3DTextureSampler"));
#else
		LpvBufferSRV.Bind( Initializer.ParameterMap, TEXT("gLpvBuffer") );
#endif

		PreIntegratedGF.Bind(Initializer.ParameterMap, TEXT("PreIntegratedGF"));
		PreIntegratedGFSampler.Bind(Initializer.ParameterMap, TEXT("PreIntegratedGFSampler"));
	}

	void SetParameters(	
#if LPV_VOLUME_TEXTURE
		FTextureRHIParamRef* LpvBufferSRVsIn, 
#else
		FShaderResourceViewRHIParamRef LpvBufferSRVIn, 
#endif 

		FLpvReadUniformBufferRef LpvUniformBuffer, 
		const FRenderingCompositePassContext& Context )
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		SetUniformBufferParameter(Context.RHICmdList, ShaderRHI, GetUniformBufferParameter<FLpvReadUniformBufferParameters>(), LpvUniformBuffer);

#if LPV_VOLUME_TEXTURE
		for ( int i=0; i<7; i++ )
		{
			if ( LpvBufferSRVParameters[i].IsBound() )
			{
				Context.RHICmdList.SetShaderTexture(ShaderRHI, LpvBufferSRVParameters[i].GetBaseIndex(), LpvBufferSRVsIn[i]);
				SetTextureParameter(Context.RHICmdList, ShaderRHI, LpvBufferSRVParameters[i], LpvVolumeTextureSampler, TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI(), LpvBufferSRVsIn[i]);
			}
		}
#else
		if ( LpvBufferSRV.IsBound() )
		{
			Context.RHICmdList.SetShaderResourceViewParameter( ShaderRHI, LpvBufferSRV.GetBaseIndex(), LpvBufferSRVIn );
		}
#endif
		FGlobalShader::SetParameters(Context.RHICmdList, ShaderRHI, Context.View);
		PostprocessParameter.SetPS(ShaderRHI, Context, TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI());
		DeferredParameters.Set(Context.RHICmdList, ShaderRHI, Context.View);
		SetTextureParameter(Context.RHICmdList, ShaderRHI, PreIntegratedGF, PreIntegratedGFSampler, TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI(), GSystemTextures.PreintegratedGF->GetRenderTargetItem().ShaderResourceTexture);
	}
	
	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter;

#if LPV_VOLUME_TEXTURE
		for ( int i=0; i<7; i++ )
		{
			Ar << LpvBufferSRVParameters[i];
		}
		Ar << LpvVolumeTextureSampler;
#else
		Ar << LpvBufferSRV;
#endif
		Ar << DeferredParameters;
		Ar << PreIntegratedGF;
		Ar << PreIntegratedGFSampler;

		return bShaderHasOutdatedParameters;
	}
};

IMPLEMENT_SHADER_TYPE(,FPostProcessLpvIndirectPS,TEXT("PostProcessLpvIndirect"),TEXT("MainPS"),SF_Pixel);


void FRCPassPostProcessLpvIndirect::Process(FRenderingCompositePassContext& Context)
{
	SCOPED_DRAW_EVENT(Context.RHICmdList, PostProcessLpvIndirect);

	{
		FRenderingCompositeOutput* OutputOfMyInput = GetInput(ePId_Input0)->GetOutput();
		PassOutputs[0].PooledRenderTarget = OutputOfMyInput->PooledRenderTarget;
		OutputOfMyInput->RenderTargetDesc.DebugName = PassOutputs[0].RenderTargetDesc.DebugName;
		PassOutputs[0].RenderTargetDesc = OutputOfMyInput->RenderTargetDesc;

		check(PassOutputs[0].RenderTargetDesc.Extent.X);
		check(PassOutputs[0].RenderTargetDesc.Extent.Y);
	}

	const FPostProcessSettings& PostprocessSettings = Context.View.FinalPostProcessSettings;
	const FSceneView& View = Context.View;
	const FSceneViewFamily& ViewFamily = *(View.Family);

	FIntRect SrcRect = View.ViewRect;
	// todo: view size should scale with input texture size so we can do SSAO in half resolution as well
	FIntRect DestRect = View.ViewRect;
	FIntPoint DestSize = DestRect.Size();

	uint32 NumReflectionCaptures = ViewFamily.Scene->GetRenderScene()->ReflectionSceneData.RegisteredReflectionCaptures.Num();

	const FSceneRenderTargetItem& DestColorRenderTarget = GSceneRenderTargets.GetSceneColor()->GetRenderTargetItem();

	// Set the view family's render target/viewport.
	FTextureRHIParamRef RenderTargets[] =
	{
		DestColorRenderTarget.TargetableTexture,
	};

	// Set the view family's render target/viewport.
	SetRenderTargets(Context.RHICmdList, 1, RenderTargets, GSceneRenderTargets.GetSceneDepthSurface(), ESimpleRenderTargetMode::EExistingColorAndDepth, FExclusiveDepthStencil::DepthRead_StencilWrite);

	Context.SetViewportAndCallRHI(View.ViewRect);

	// set the state
	if ( ViewFamily.EngineShowFlags.LpvLightingOnly )
	{
		Context.RHICmdList.SetBlendState(TStaticBlendState<>::GetRHI());
	}
	else
	{
		// additive blending
		Context.RHICmdList.SetBlendState(TStaticBlendState<CW_RGB, BO_Add, BF_One, BF_One>::GetRHI());
	}
	Context.RHICmdList.SetRasterizerState(TStaticRasterizerState<>::GetRHI());
	Context.RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());

	TShaderMapRef<FPostProcessVS> VertexShader(Context.GetShaderMap());

	FSceneViewState* ViewState = (FSceneViewState*)View.State;
	FLightPropagationVolume* Lpv = NULL;
	bool bUseLpv = false;
	if ( ViewState )
	{
		Lpv = ViewState->GetLightPropagationVolume();

		bUseLpv = Lpv && PostprocessSettings.LPVIntensity > 0.0f;
	}

	if ( !bUseLpv )
	{
		return;
	}

	TShaderMapRef<FPostProcessLpvIndirectPS> PixelShader(Context.GetShaderMap());

	static FGlobalBoundShaderState BoundShaderState;
	

	// call it once after setting up the shader data to avoid the warnings in the function
	SetGlobalBoundShaderState(Context.RHICmdList, Context.GetFeatureLevel(), BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);

	FLpvReadUniformBufferParameters	LpvReadUniformBufferParams;
	FLpvReadUniformBufferRef LpvReadUniformBuffer;

	LpvReadUniformBufferParams = Lpv->GetReadUniformBufferParams();
	LpvReadUniformBuffer = FLpvReadUniformBufferRef::CreateUniformBufferImmediate( LpvReadUniformBufferParams, UniformBuffer_SingleDraw ); 

#if LPV_VOLUME_TEXTURE
	FTextureRHIParamRef LpvBufferSrvs[7];
	for ( int i = 0; i < 7; i++ ) 
	{
		LpvBufferSrvs[i] = Lpv->GetLpvBufferSrv(i);
	}
	PixelShader->SetParameters(LpvBufferSrvs, LpvReadUniformBuffer, Context);
#else
	FShaderResourceViewRHIParamRef LpvBufferSrv = Lpv->GetLpvBufferSrv();
	PixelShader->SetParameters(LpvBufferSrv, LpvReadUniformBuffer, Context);
#endif

	// Draw a quad mapping scene color to the view's render target
	DrawRectangle( 
		Context.RHICmdList,
		0, 0,
		View.ViewRect.Width(), View.ViewRect.Height(),
		View.ViewRect.Min.X, View.ViewRect.Min.Y, 
		View.ViewRect.Width(), View.ViewRect.Height(),
		View.ViewRect.Size(),
		GSceneRenderTargets.GetBufferSizeXY(),
		*VertexShader);

	Context.RHICmdList.CopyToResolveTarget(DestColorRenderTarget.TargetableTexture, DestColorRenderTarget.ShaderResourceTexture, false, FResolveParams());
}

FPooledRenderTargetDesc FRCPassPostProcessLpvIndirect::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	// we assume this pass is additively blended with the scene color so this data is not needed
	FPooledRenderTargetDesc Ret;

	Ret.DebugName = TEXT("LpvIndirect");

	return Ret;
}
