// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PostProcessMorpheus.cpp: Post processing for Sony Morpheus HMD device.
=============================================================================*/

#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "SceneFilterRendering.h"
#include "PostProcessMorpheus.h"
#include "PostProcessing.h"
#include "PostProcessHistogram.h"
#include "PostProcessEyeAdaptation.h"
#include "IHeadMountedDisplay.h"
#include "SceneUtils.h"

DEFINE_LOG_CATEGORY_STATIC(LogMorpheusHMDPostProcess, All, All);

#if MORPHEUS_ENGINE_DISTORTION

/** Encapsulates the post processing HMD distortion and correction pixel shader. */
class FPostProcessMorpheusPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessMorpheusPS, Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		// we must use a run time check for this because the builds the build machines create will have Morpheus defined,
		// but a user will not necessarily have the Morpheus files
		bool bEnableMorpheus = false;
		if (GConfig->GetBool(TEXT("/Script/MorpheusEditor.MorpheusRuntimeSettings"), TEXT("bEnableMorpheus"), bEnableMorpheus, GEngineIni))
		{
			return bEnableMorpheus;
		}
		return false;
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("NEW_MORPHEUS_DISTORTION"), TEXT("1"));
	}

	/** Default constructor. */
	FPostProcessMorpheusPS()
	{
	}

public:
	FPostProcessPassParameters PostprocessParameter;
	FDeferredPixelShaderParameters DeferredParameters;
	
	// Distortion parameter values
	FShaderParameter TextureScale;
	FShaderParameter TextureOffset;
	FShaderParameter TextureUVOffset;

	FShaderResourceParameter DistortionTextureParam; 
	FShaderResourceParameter DistortionTextureSampler; 

	/** Initialization constructor. */
	FPostProcessMorpheusPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
		DeferredParameters.Bind(Initializer.ParameterMap);

		TextureScale.Bind(Initializer.ParameterMap, TEXT("TextureScale"));
		//check(TextureScaleLeft.IsBound());		

		TextureOffset.Bind(Initializer.ParameterMap, TEXT("TextureOffset"));
		//check(TextureOffsetRight.IsBound());

		TextureUVOffset.Bind(Initializer.ParameterMap, TEXT("TextureUVOffset"));
		//check(TextureUVOffset.IsBound());
			
		DistortionTextureParam.Bind(Initializer.ParameterMap, TEXT("DistortionTextureArray"));
		//check(DistortionTextureLeftParam.IsBound());		

		DistortionTextureSampler.Bind(Initializer.ParameterMap, TEXT("DistortionTextureSampler"));
		//check(DistortionTextureSampler.IsBound());		
	}


	void SetPS(const FRenderingCompositePassContext& Context, FIntRect SrcRect, FIntPoint SrcBufferSize, EStereoscopicPass StereoPass, FMatrix& QuadTexTransform)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();
		
		FGlobalShader::SetParameters(Context.RHICmdList, ShaderRHI, Context.View);

		PostprocessParameter.SetPS(ShaderRHI, Context, TStaticSamplerState<SF_Bilinear, AM_Border, AM_Border, AM_Border>::GetRHI());
		DeferredParameters.Set(Context.RHICmdList, ShaderRHI, Context.View);

		{
			check(GEngine->HMDDevice.IsValid());
			TSharedPtr< class IHeadMountedDisplay, ESPMode::ThreadSafe > HMDDevice = GEngine->HMDDevice;

			check (StereoPass != eSSP_FULL);
			if (StereoPass == eSSP_LEFT_EYE)
			{
				FTexture* TextureLeft = HMDDevice->GetDistortionTextureLeft();
				if (TextureLeft)
				{
					SetTextureParameter(Context.RHICmdList, ShaderRHI, DistortionTextureParam, DistortionTextureSampler, TextureLeft->SamplerStateRHI, TextureLeft->TextureRHI);
				}
				SetShaderValue(Context.RHICmdList, ShaderRHI, TextureScale, HMDDevice->GetTextureScaleLeft());
				SetShaderValue(Context.RHICmdList, ShaderRHI, TextureOffset, HMDDevice->GetTextureOffsetLeft());
				SetShaderValue(Context.RHICmdList, ShaderRHI, TextureUVOffset, 0.0f);
			}
			else
			{
				FTexture* TextureRight = HMDDevice->GetDistortionTextureRight();
				if (TextureRight)
				{
					SetTextureParameter(Context.RHICmdList, ShaderRHI, DistortionTextureParam, DistortionTextureSampler, TextureRight->SamplerStateRHI, TextureRight->TextureRHI);
				}
				SetShaderValue(Context.RHICmdList, ShaderRHI, TextureScale, HMDDevice->GetTextureScaleRight());
				SetShaderValue(Context.RHICmdList, ShaderRHI, TextureOffset, HMDDevice->GetTextureOffsetRight());
				SetShaderValue(Context.RHICmdList, ShaderRHI, TextureUVOffset, -0.5f);
			}				
				  
			QuadTexTransform = FMatrix::Identity;            
		}
	}
	
	// FShader interface.
	virtual bool Serialize(FArchive& Ar)
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter << DeferredParameters << TextureScale << TextureOffset << TextureUVOffset << DistortionTextureParam << DistortionTextureSampler;
		return bShaderHasOutdatedParameters;
	}
};

/** Encapsulates the post processing vertex shader. */
class FPostProcessMorpheusVS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessMorpheusVS,Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		// we must use a run time check for this because the builds the build machines create will have Morpheus defined,
		// but a user will not necessarily have the Morpheus files
		bool bEnableMorpheus = false;
		if (GConfig->GetBool(TEXT("/Script/MorpheusEditor.MorpheusRuntimeSettings"), TEXT("bEnableMorpheus"), bEnableMorpheus, GEngineIni))
		{
			return bEnableMorpheus;
		}
		return false;
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("NEW_MORPHEUS_DISTORTION"), TEXT("1"));
	}

	/** Default constructor. */
	FPostProcessMorpheusVS() {}

	/** to have a similar interface as all other shaders */
	void SetParameters(const FRenderingCompositePassContext& Context)
	{
		FGlobalShader::SetParameters(Context.RHICmdList, GetVertexShader(), Context.View);
	}

	void SetParameters(FRHICommandList& RHICmdList, const FSceneView& View)
	{
		FGlobalShader::SetParameters(RHICmdList, GetVertexShader(), View);
	}

public:

	/** Initialization constructor. */
	FPostProcessMorpheusVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
	}
};

IMPLEMENT_SHADER_TYPE(, FPostProcessMorpheusVS, TEXT("MorpheusInclude"), TEXT("MainVS"), SF_Vertex);
IMPLEMENT_SHADER_TYPE(, FPostProcessMorpheusPS,TEXT("MorpheusInclude"),TEXT("MainPS"),SF_Pixel);

void FRCPassPostProcessMorpheus::Process(FRenderingCompositePassContext& Context)
{
	SCOPED_DRAW_EVENT(Context.RHICmdList, PostProcessMorpheus);
	const FPooledRenderTargetDesc* InputDesc = GetInputDesc(ePId_Input0);

	if(!InputDesc)
	{
		// input is not hooked up correctly
		return;
	}

	const FSceneView& View = Context.View;
	const FSceneViewFamily& ViewFamily = *(View.Family);
	
	FIntRect SrcRect = View.ViewRect;

	//we should be the last node in the graph, so use the 'unscaled' view rect.  aka the one not affected by screenpercentage as we should be 
	//targetting the final final up/downsampled backbuffer.
	FIntRect DestRect = View.UnscaledViewRect; //View.ViewRect; // View.UnscaledViewRect;
	FIntPoint SrcSize = InputDesc->Extent;

	const FSceneRenderTargetItem& DestRenderTarget = PassOutputs[0].RequestSurface(Context);

	// Set the view family's render target/viewport.
	SetRenderTarget(Context.RHICmdList, DestRenderTarget.TargetableTexture, FTextureRHIRef());

	Context.SetViewportAndCallRHI(DestRect);

	// set the state
	Context.RHICmdList.SetBlendState(TStaticBlendState<>::GetRHI());
	Context.RHICmdList.SetRasterizerState(TStaticRasterizerState<>::GetRHI());
	Context.RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());

#if MORPHEUS_ENGINE_DISTORTION
	TShaderMapRef<FPostProcessMorpheusVS> VertexShader(Context.GetShaderMap());
	TShaderMapRef<FPostProcessMorpheusPS> PixelShader(Context.GetShaderMap());

	static FGlobalBoundShaderState BoundShaderState;
	

	SetGlobalBoundShaderState(Context.RHICmdList, Context.GetFeatureLevel(), BoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader);

	FMatrix QuadTexTransform;
	FMatrix QuadPosTransform = FMatrix::Identity;

	PixelShader->SetPS(Context, SrcRect, SrcSize, View.StereoPass, QuadTexTransform);

	// Draw a quad mapping scene color to the view's render target
	DrawTransformedRectangle(
		Context.RHICmdList,
		0, 0,
		DestRect.Width(), DestRect.Height(),
		QuadPosTransform,
		SrcRect.Min.X, SrcRect.Min.Y,
		SrcRect.Width(), SrcRect.Height(),
		QuadTexTransform,
		DestRect.Size(),
		SrcSize
		);
#elif PLATFORM_PS4
	checkf(false, TEXT("PS4 uses SDK distortion."));
#else
	checkf(false, TEXT("Unsupported path.  Morpheus should be disabled."));
#endif

	Context.RHICmdList.CopyToResolveTarget(DestRenderTarget.TargetableTexture, DestRenderTarget.ShaderResourceTexture, false, FResolveParams());
}

FPooledRenderTargetDesc FRCPassPostProcessMorpheus::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	FPooledRenderTargetDesc Ret = GetInput(ePId_Input0)->GetOutput()->RenderTargetDesc;

	Ret.NumSamples = 1;	// no MSAA
	Ret.Reset();
	Ret.DebugName = TEXT("Morpheus");

	return Ret;
}

#endif // MORPHEUS_ENGINE_DISTORTION