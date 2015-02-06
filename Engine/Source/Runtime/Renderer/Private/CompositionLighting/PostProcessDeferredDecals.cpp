// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PostProcessDeferredDecals.cpp: Deferred Decals implementation.
=============================================================================*/

#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "SceneFilterRendering.h"
#include "PostProcessing.h"
#include "PostProcessDeferredDecals.h"
#include "ScreenRendering.h"
#include "SceneUtils.h"

static TAutoConsoleVariable<float> CVarStencilSizeThreshold(
	TEXT("r.Decal.StencilSizeThreshold"),
	0.1f,
	TEXT("Control a per decal stencil pass that allows to large (screen space) decals faster. It adds more overhead per decals so this\n")
	TEXT("  <0: optimization is disabled\n")
	TEXT("   0: optimization is enabled no matter how small (screen space) the decal is\n")
	TEXT("0..1: optimization is enabled, value defines the minimum size (screen space) to trigger the optimization (default 0.1)")
	);

enum ERenderTargetMode
{
	RTM_Unknown = -1,
	RTM_SceneColorAndGBuffer,
	RTM_DBuffer,
	RTM_GBufferNormal,
	RTM_SceneColor,
};

// @return DECAL_RENDERTARGET_COUNT for the shader
uint32 ComputeRenderTargetCount(ERenderTargetMode RenderTargetMode)
{
	switch(RenderTargetMode)
	{
		case RTM_SceneColorAndGBuffer:	return 5;
		case RTM_DBuffer:				return 3;
		case RTM_GBufferNormal:			return 1;
		case RTM_SceneColor:			return 1;
	}

	return 0;
}

EDecalBlendMode ComputeFinalDecalBlendMode(EDecalBlendMode DecalBlendMode, bool bUseNormal)
{
	if(!bUseNormal)
	{
		if(DecalBlendMode == DBM_DBuffer_ColorNormalRoughness)
		{
			DecalBlendMode = DBM_DBuffer_ColorRoughness;
		}
		else if(DecalBlendMode == DBM_DBuffer_NormalRoughness)
		{
			DecalBlendMode = DBM_DBuffer_Roughness;
		}
	}

	return DecalBlendMode;
}

ERenderTargetMode ComputeRenderTargetMode(EDecalBlendMode DecalBlendMode)
{
	switch(DecalBlendMode)
	{
		case DBM_Translucent:
		case DBM_Stain:
			return RTM_SceneColorAndGBuffer;

		case DBM_Normal:
			return RTM_GBufferNormal;

		case DBM_Emissive:
			return RTM_SceneColor;

		case DBM_DBuffer_ColorNormalRoughness:
		case DBM_DBuffer_Color:
		case DBM_DBuffer_ColorNormal:
		case DBM_DBuffer_ColorRoughness:
		case DBM_DBuffer_Normal:
		case DBM_DBuffer_NormalRoughness:
		case DBM_DBuffer_Roughness:
			// can be optimized using less MRT when possible
			return RTM_DBuffer;
	}

	// add the missing decal blend mode to the switch
	check(0);
	return RTM_Unknown;
}

// @return 0:before BasePass, 1:after base pass, before lighting, (later we could add "after lighting" and multiply)
uint32 ComputeRenderStage(EDecalBlendMode DecalBlendMode)
{
	switch(DecalBlendMode)
	{
	case DBM_DBuffer_ColorNormalRoughness:
	case DBM_DBuffer_Color:
	case DBM_DBuffer_ColorNormal:
	case DBM_DBuffer_ColorRoughness:
	case DBM_DBuffer_Normal:
	case DBM_DBuffer_NormalRoughness:
	case DBM_DBuffer_Roughness:
		return 0;

	case DBM_Translucent:
	case DBM_Stain:
	case DBM_Normal:
	case DBM_Emissive:
		return 1;

	default:
		check(0);
	}

	return -1;
}

struct FTransientDecalRenderData
{
	EDecalBlendMode DecalBlendMode;
	const FMaterialRenderProxy* MaterialProxy;
	const FMaterial* MaterialResource;
	const FDeferredDecalProxy* DecalProxy;
	bool bHasNormal;

	FTransientDecalRenderData(const FScene& InScene, FDeferredDecalProxy* InDecalProxy)
		: DecalProxy(InDecalProxy)
	{
		MaterialProxy = InDecalProxy->DecalMaterial->GetRenderProxy(InDecalProxy->bOwnerSelected);
		MaterialResource = MaterialProxy->GetMaterial(InScene.GetFeatureLevel());
		bHasNormal = MaterialResource->HasNormalConnected();
		DecalBlendMode = ComputeFinalDecalBlendMode((EDecalBlendMode)MaterialResource->GetDecalBlendMode(), bHasNormal);
		check(MaterialProxy && MaterialResource);
	}
};
// @param RenderState 0:before BasePass, 1:before lighting, (later we could add "after lighting" and multiply)
void SetDecalBlendState(FRHICommandList& RHICmdList, const ERHIFeatureLevel::Type SMFeatureLevel, uint32 RenderStage, EDecalBlendMode DecalBlendMode, bool bHasNormal)
{
	if(RenderStage == 0)
	{
		// before base pass (for DBuffer decals)

		if(SMFeatureLevel == ERHIFeatureLevel::SM4)
		{
			// DX10 doesn't support masking/using different blend modes per MRT.
			// We set the opacity in the shader to 0 so we can use the same frame buffer blend.

			RHICmdList.SetBlendState( TStaticBlendState< 
				CW_RGBA, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha,			BO_Add,BF_Zero,BF_InverseSourceAlpha,
				CW_RGBA, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha,			BO_Add,BF_Zero,BF_InverseSourceAlpha,
				CW_RGBA, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha,			BO_Add,BF_Zero,BF_InverseSourceAlpha >::GetRHI() );
			return;
		}
		else
		{
			// see DX10 comment above
			// As we set the opacity in the shader we don't need to set different frame buffer blend modes but we like to hint to the driver that we
			// don't need to output there. We also could replace this with many SetRenderTarget calls but it might be slower (needs to be tested).

			switch(DecalBlendMode)
			{
			case DBM_DBuffer_ColorNormalRoughness:
				RHICmdList.SetBlendState( TStaticBlendState< 
					CW_RGBA, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha,			BO_Add,BF_Zero,BF_InverseSourceAlpha,
					CW_RGBA, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha,			BO_Add,BF_Zero,BF_InverseSourceAlpha,
					CW_RGBA, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha,			BO_Add,BF_Zero,BF_InverseSourceAlpha >::GetRHI() );		
				break;

			case DBM_DBuffer_Color:
				// we can optimize using less MRT later
				RHICmdList.SetBlendState( TStaticBlendState< 
					CW_RGBA, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha,			BO_Add,BF_Zero,BF_InverseSourceAlpha,
					CW_RGBA, BO_Add, BF_Zero, BF_One,								BO_Add,BF_Zero,BF_One,
					CW_RGBA, BO_Add, BF_Zero, BF_One,								BO_Add,BF_Zero,BF_One>::GetRHI() );		
				break;

			case DBM_DBuffer_ColorNormal:
				// we can optimize using less MRT later
				RHICmdList.SetBlendState( TStaticBlendState< 
					CW_RGBA, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha,			BO_Add,BF_Zero,BF_InverseSourceAlpha,
					CW_RGBA, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha,			BO_Add,BF_Zero,BF_InverseSourceAlpha,
					CW_RGBA, BO_Add, BF_Zero, BF_One,								BO_Add,BF_Zero,BF_One >::GetRHI() );		
				break;

			case DBM_DBuffer_ColorRoughness:
				// we can optimize using less MRT later
				RHICmdList.SetBlendState( TStaticBlendState< 
					CW_RGBA, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha,			BO_Add,BF_Zero,BF_InverseSourceAlpha,
					CW_RGBA, BO_Add, BF_Zero, BF_One,								BO_Add,BF_Zero,BF_One,
					CW_RGBA, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha,			BO_Add,BF_Zero,BF_InverseSourceAlpha >::GetRHI() );		
				break;

			case DBM_DBuffer_Normal:
				// we can optimize using less MRT later
				RHICmdList.SetBlendState( TStaticBlendState< 
					CW_RGBA, BO_Add, BF_Zero, BF_One,								BO_Add,BF_Zero,BF_One,
					CW_RGBA, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha,			BO_Add,BF_Zero,BF_InverseSourceAlpha,
					CW_RGBA, BO_Add, BF_Zero, BF_One,								BO_Add,BF_Zero,BF_One>::GetRHI() );		
				break;

			case DBM_DBuffer_NormalRoughness:
				// we can optimize using less MRT later
				RHICmdList.SetBlendState( TStaticBlendState< 
					CW_RGBA, BO_Add, BF_Zero, BF_One,								BO_Add,BF_Zero,BF_One,
					CW_RGBA, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha,			BO_Add,BF_Zero,BF_InverseSourceAlpha,
					CW_RGBA, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha,			BO_Add,BF_Zero,BF_InverseSourceAlpha >::GetRHI() );		
				break;

			case DBM_DBuffer_Roughness:
				// we can optimize using less MRT later
				RHICmdList.SetBlendState( TStaticBlendState< 
					CW_RGBA, BO_Add, BF_Zero, BF_One,								BO_Add,BF_Zero,BF_One,
					CW_RGBA, BO_Add, BF_Zero, BF_One,								BO_Add,BF_Zero,BF_One,
					CW_RGBA, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha,			BO_Add,BF_Zero,BF_InverseSourceAlpha >::GetRHI() );		
				break;

			default:
				// the decal type should not be rendered in this pass - internal error
				check(0);	
				break;
			}
		}

		return;
	}
	else
	{
		// before lighting (for non DBuffer decals)

		switch(DecalBlendMode)
		{
		case DBM_Translucent:
			// @todo: Feature Level 10 does not support separate blends modes for each render target. This could result in the
			// translucent and stain blend modes looking incorrect when running in this mode.
			if(GSupportsSeparateRenderTargetBlendState)
			{
				if(bHasNormal)
				{
					RHICmdList.SetBlendState( TStaticBlendState<
						CW_RGB, BO_Add, BF_SourceAlpha, BF_One,						BO_Add, BF_Zero, BF_One,	// Emissive
						CW_RGB, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha,		BO_Add, BF_Zero, BF_One,	// Normal
						CW_RGB, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha,		BO_Add, BF_Zero, BF_One,	// Metallic, Specular
						CW_RGB, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha,		BO_Add, BF_Zero, BF_One,	// BaseColor
						CW_RED, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha,		BO_Add, BF_Zero, BF_One		// Roughness in r
					>::GetRHI() );
				}
				else
				{
					RHICmdList.SetBlendState( TStaticBlendState<
						CW_RGB, BO_Add, BF_SourceAlpha, BF_One,						BO_Add, BF_Zero, BF_One,	// Emissive
						CW_RGB, BO_Add, BF_Zero,		BF_One,						BO_Add, BF_Zero, BF_One,	// Normal
						CW_RGB, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha,		BO_Add, BF_Zero, BF_One,	// Metallic, Specular
						CW_RGB, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha,		BO_Add, BF_Zero, BF_One,	// BaseColor
						CW_RED, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha,		BO_Add, BF_Zero, BF_One		// Roughness in r
					>::GetRHI() );
				}
			}
			else if(SMFeatureLevel == ERHIFeatureLevel::SM4)
			{
				RHICmdList.SetBlendState( TStaticBlendState<
					CW_RGB, BO_Add, BF_SourceAlpha, BF_One,							BO_Add, BF_Zero, BF_One,	// Emissive
					CW_RGB, BO_Add, BF_SourceAlpha, BF_One,							BO_Add, BF_Zero, BF_One,	// Normal
					CW_RGB, BO_Add, BF_SourceAlpha, BF_One,							BO_Add, BF_Zero, BF_One,	// Metallic, Specular
					CW_RGB, BO_Add, BF_SourceAlpha,	BF_One,							BO_Add, BF_Zero, BF_One,	// BaseColor
					CW_RED, BO_Add, BF_SourceAlpha, BF_One,							BO_Add, BF_Zero, BF_One		// Roughness in r
				>::GetRHI() );
			}
			break;

		case DBM_Stain:
			if(GSupportsSeparateRenderTargetBlendState)
			{
				if(bHasNormal)
				{
					RHICmdList.SetBlendState( TStaticBlendState<
						CW_RGB, BO_Add, BF_SourceAlpha, BF_One,						BO_Add, BF_Zero, BF_One,	// Emissive
						CW_RGB, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha,		BO_Add, BF_Zero, BF_One,	// Normal
						CW_RGB, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha,		BO_Add, BF_Zero, BF_One,	// Metallic, Specular
						CW_RGB, BO_Add, BF_DestColor,	BF_InverseSourceAlpha,		BO_Add, BF_Zero, BF_One,	// BaseColor
						CW_RED, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha,		BO_Add, BF_Zero, BF_One		// Roughness in r
					>::GetRHI() );
				}
				else
				{
					RHICmdList.SetBlendState( TStaticBlendState<
						CW_RGB, BO_Add, BF_SourceAlpha, BF_One,						BO_Add, BF_Zero, BF_One,	// Emissive
						CW_RGB, BO_Add, BF_Zero,		BF_One,						BO_Add, BF_Zero, BF_One,	// Normal
						CW_RGB, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha,		BO_Add, BF_Zero, BF_One,	// Metallic, Specular
						CW_RGB, BO_Add, BF_DestColor,	BF_InverseSourceAlpha,		BO_Add, BF_Zero, BF_One,	// BaseColor
						CW_RED, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha,		BO_Add, BF_Zero, BF_One		// Roughness in r
					>::GetRHI() );
				}
			}
			else if(SMFeatureLevel == ERHIFeatureLevel::SM4)
			{
				RHICmdList.SetBlendState( TStaticBlendState<
					CW_RGB, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha,			BO_Add, BF_Zero, BF_One,	// Emissive
					CW_RGB, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha,			BO_Add, BF_Zero, BF_One,	// Normal
					CW_RGB, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha,			BO_Add, BF_Zero, BF_One,	// Metallic, Specular
					CW_RGB, BO_Add, BF_SourceAlpha,	BF_InverseSourceAlpha,			BO_Add, BF_Zero, BF_One,	// BaseColor
					CW_RED, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha,			BO_Add, BF_Zero, BF_One		// Roughness in r
				>::GetRHI() );
			}
			break;

		case DBM_Normal:
			RHICmdList.SetBlendState( TStaticBlendState< CW_RGB, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha >::GetRHI() );
			break;

		case DBM_Emissive:
			RHICmdList.SetBlendState( TStaticBlendState< CW_RGB, BO_Add, BF_SourceAlpha, BF_One >::GetRHI() );
			break;

		default:
			// the decal type should not be rendered in this pass - internal error
			check(0);	
			break;
		}
	}
}


const FVector GDefaultDecalSize(1.0f, 1.0f, 1.0f);

/** Pixel shader used to setup the decal receiver mask */
class FStencilDecalMaskPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FStencilDecalMaskPS,Global);
public:

	static bool ShouldCache(EShaderPlatform Platform) 
	{ 
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4); 
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform,OutEnvironment);
	}

	FStencilDecalMaskPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FGlobalShader(Initializer)
	{
		DeferredParameters.Bind(Initializer.ParameterMap);
	}
	FStencilDecalMaskPS() {}

	void SetParameters(FRHICommandList& RHICmdList, const FSceneView& View)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		FGlobalShader::SetParameters(RHICmdList, ShaderRHI, View);
		DeferredParameters.Set(RHICmdList, ShaderRHI, View);
	}

	virtual bool Serialize(FArchive& Ar)
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << DeferredParameters;
		return bShaderHasOutdatedParameters;
	}

private:
	FDeferredPixelShaderParameters DeferredParameters;
};

IMPLEMENT_SHADER_TYPE(,FStencilDecalMaskPS,TEXT("DeferredDecal"),TEXT("StencilDecalMaskMain"),SF_Pixel);

FGlobalBoundShaderState StencilDecalMaskBoundShaderState;

/** Draws a full view quad that sets stencil to 1 anywhere that decals should not be projected. */
void StencilDecalMask(FRHICommandList& RHICmdList, const FViewInfo& View)
{
	SCOPED_DRAW_EVENT(RHICmdList, StencilDecalMask);
	RHICmdList.SetRasterizerState(TStaticRasterizerState<FM_Solid, CM_None>::GetRHI());
	RHICmdList.SetBlendState(TStaticBlendState<CW_NONE>::GetRHI());
	SetRenderTarget(RHICmdList, NULL, GSceneRenderTargets.GetSceneDepthSurface());
	RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0.0f, View.ViewRect.Max.X, View.ViewRect.Max.Y, 1.0f);

	// Write 1 to highest bit of stencil to areas that should not receive decals
	RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always, true, CF_Always, SO_Replace, SO_Replace, SO_Replace>::GetRHI(), 0x80);

	const auto FeatureLevel = View.GetFeatureLevel();
	auto ShaderMap = View.ShaderMap;
	TShaderMapRef<FScreenVS> ScreenVertexShader(ShaderMap);
	TShaderMapRef<FStencilDecalMaskPS> PixelShader(ShaderMap);
	
	SetGlobalBoundShaderState(RHICmdList, FeatureLevel, StencilDecalMaskBoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *ScreenVertexShader, *PixelShader);

	PixelShader->SetParameters(RHICmdList, View);

	DrawRectangle( 
		RHICmdList,
		0, 0, 
		View.ViewRect.Width(), View.ViewRect.Height(),
		View.ViewRect.Min.X, View.ViewRect.Min.Y, 
		View.ViewRect.Width(), View.ViewRect.Height(),
		FIntPoint(View.ViewRect.Width(), View.ViewRect.Height()),
		GSceneRenderTargets.GetBufferSizeXY(),
		*ScreenVertexShader,
		EDRF_UseTriangleOptimization);
}

/**
 * A vertex shader for projecting a deferred decal onto the scene.
 */
class FDeferredDecalVS : public FMaterialShader
{
	DECLARE_SHADER_TYPE(FDeferredDecalVS,Material);
public:

	/**
	  * Makes sure only shaders for materials that are explicitly flagged
	  * as 'UsedAsDeferredDecal' in the Material Editor gets compiled into
	  * the shader cache.
	  */
	static bool ShouldCache(EShaderPlatform Platform, const FMaterial* Material)
	{
		return Material->IsUsedWithDeferredDecal() && IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4);
	}

	FDeferredDecalVS( )	{ }
	FDeferredDecalVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FMaterialShader(Initializer)
	{
		FrustumComponentToClip.Bind(Initializer.ParameterMap, TEXT("FrustumComponentToClip"));
	}

	void SetParameters(FRHICommandList& RHICmdList, const FSceneView& View, const FMatrix& InFrustumComponentToClip)
	{
		FMaterialShader::SetParameters(RHICmdList, GetVertexShader(), View);
		SetShaderValue(RHICmdList, GetVertexShader(), FrustumComponentToClip, InFrustumComponentToClip);
	}

	virtual bool Serialize(FArchive& Ar)
	{
		bool bShaderHasOutdatedParameters = FMaterialShader::Serialize(Ar);
		Ar << FrustumComponentToClip;
		return bShaderHasOutdatedParameters;
	}

private:
	FShaderParameter FrustumComponentToClip;
};

IMPLEMENT_MATERIAL_SHADER_TYPE(,FDeferredDecalVS,TEXT("DeferredDecal"),TEXT("MainVS"),SF_Vertex);

/**
 * A pixel shader for projecting a deferred decal onto the scene.
 */
class FDeferredDecalPS : public FMaterialShader
{
	DECLARE_SHADER_TYPE(FDeferredDecalPS,Material);
public:

	/**
	  * Makes sure only shaders for materials that are explicitly flagged
	  * as 'UsedAsDeferredDecal' in the Material Editor gets compiled into
	  * the shader cache.
	  */
	static bool ShouldCache(EShaderPlatform Platform, const FMaterial* Material)
	{
		if (!Material->IsUsedWithDeferredDecal())
		{
			return false;
		}

		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4);
	}

	static void ModifyCompilationEnvironment( EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment )
	{
		FMaterialShader::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);

		check(Material);
		
		EDecalBlendMode DecalBlendMode = (EDecalBlendMode)Material->GetDecalBlendMode();
		ERenderTargetMode RenderTargetMode = ComputeRenderTargetMode(DecalBlendMode);
		uint32 RenderTargetCount = ComputeRenderTargetCount(RenderTargetMode);

		OutEnvironment.SetDefine(TEXT("DECAL_BLEND_MODE"), (int32)DecalBlendMode);
		OutEnvironment.SetDefine(TEXT("DECAL_PROJECTION"), 1);
		OutEnvironment.SetDefine(TEXT("DECAL_RENDERTARGET_COUNT"), RenderTargetCount);
		OutEnvironment.SetDefine(TEXT("DECAL_RENDERSTAGE"), ComputeRenderStage(DecalBlendMode));
	}

	FDeferredDecalPS() {}
	FDeferredDecalPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FMaterialShader(Initializer)
	{
		ScreenToDecal.Bind(Initializer.ParameterMap,TEXT("ScreenToDecal"));
		DecalToWorld.Bind(Initializer.ParameterMap,TEXT("DecalToWorld"));
	}

	void SetParameters(FRHICommandList& RHICmdList, const FSceneView& View, const FMaterialRenderProxy* MaterialProxy, const FDeferredDecalProxy& DecalProxy)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		FMaterialShader::SetParameters(RHICmdList, ShaderRHI, MaterialProxy, *MaterialProxy->GetMaterial(View.GetFeatureLevel()), View, true, ESceneRenderTargetsMode::SetTextures);

		FTransform ComponentTrans = DecalProxy.ComponentTrans;

		// 1,1,1 requires no scale
		//			ComponentTrans = ComponentTrans.GetScaled(GDefaultDecalSize);

		FMatrix WorldToComponent = ComponentTrans.ToMatrixWithScale().InverseFast();

		// Set the transform from screen space to light space.
		if(ScreenToDecal.IsBound())
		{
			const FMatrix ScreenToDecalValue = 
				FMatrix(
					FPlane(1,0,0,0),
					FPlane(0,1,0,0),
					FPlane(0,0,View.ViewMatrices.ProjMatrix.M[2][2],1),
					FPlane(0,0,View.ViewMatrices.ProjMatrix.M[3][2],0)
				) * View.InvViewProjectionMatrix * WorldToComponent;

			SetShaderValue(RHICmdList, ShaderRHI, ScreenToDecal, ScreenToDecalValue);
		}

		// Set the transform from light space to world space (only for normals)
		if(DecalToWorld.IsBound())
		{
			const FMatrix DecalToWorldValue = ComponentTrans.ToMatrixNoScale();
			
			// 1,1,1 requires no scale
			//			DecalToWorldValue = DecalToWorldValue.GetScaled(GDefaultDecalSize);

			SetShaderValue(RHICmdList, ShaderRHI, DecalToWorld, DecalToWorldValue);
		}
	}

	virtual bool Serialize(FArchive& Ar)
	{
		bool bShaderHasOutdatedParameters = FMaterialShader::Serialize(Ar);
		Ar << ScreenToDecal << DecalToWorld;
		return bShaderHasOutdatedParameters;
	}

private:
	FShaderParameter ScreenToDecal;
	FShaderParameter DecalToWorld;
};

IMPLEMENT_MATERIAL_SHADER_TYPE(,FDeferredDecalPS,TEXT("DeferredDecal"),TEXT("MainPS"),SF_Pixel);

/**
* Static vertex buffer for a unit sized cube. Used to draw the frustum for deferred decals.
*/
class FUnitCubeVertexBuffer : public FVertexBuffer
{
public:
	/**
	* Initialize the RHI for this rendering resource
	*/
	void InitRHI() override
	{
		const int32 NumVerts = 8;
		TResourceArray<FVector4, VERTEXBUFFER_ALIGNMENT> Verts;
		Verts.Init(NumVerts);

		for (uint32 Z = 0; Z < 2; Z++)
		{
			for (uint32 Y = 0; Y < 2; Y++)
			{
				for (uint32 X = 0; X < 2; X++)
				{
					const FVector4 Vertex = FVector4(
					  (X ? -1 : 1),
					  (Y ? -1 : 1),
					  (Z ? -1 : 1),
					  1.0f
					);

					Verts[GetCubeVertexIndex(X, Y, Z)] = Vertex;
				}
			}
		}

		uint32 Size = Verts.GetResourceDataSize();

		// Create vertex buffer. Fill buffer with initial data upon creation
		FRHIResourceCreateInfo CreateInfo(&Verts);
		VertexBufferRHI = RHICreateVertexBuffer(Size, BUF_Static, CreateInfo);
	}
};

/** 
* Unit cube index buffer
*/
class FUnitCubeIndexBuffer : public FIndexBuffer
{
public:
	/**
	* Initialize the RHI for this rendering resource
	*/
	void InitRHI() override
	{
		TResourceArray<uint16, INDEXBUFFER_ALIGNMENT> Indices;
		
		NumIndices = ARRAY_COUNT(GCubeIndices);
		Indices.AddUninitialized(NumIndices);
		FMemory::Memcpy(Indices.GetData(), GCubeIndices, NumIndices * sizeof(uint16));

		const uint32 Size = Indices.GetResourceDataSize();
		const uint32 Stride = sizeof(uint16);

		// Create index buffer. Fill buffer with initial data upon creation
		FRHIResourceCreateInfo CreateInfo(&Indices);
		IndexBufferRHI = RHICreateIndexBuffer(Stride, Size, BUF_Static, CreateInfo);
	}

	int32 GetIndexCount() const
	{
		return NumIndices;
	};

private:
	int32 NumIndices;
};

static TGlobalResource<FUnitCubeVertexBuffer> GUnitCubeVertexBuffer;
static TGlobalResource<FUnitCubeIndexBuffer> GUnitCubeIndexBuffer;

void SetShader(const FRenderingCompositePassContext& Context, const FTransientDecalRenderData& DecalData, FShader* VertexShader)
{
	const FSceneView& View = Context.View;
	
	const FMaterialShaderMap* MaterialShaderMap = DecalData.MaterialResource->GetRenderingThreadShaderMap();
	FDeferredDecalPS* PixelShader = MaterialShaderMap->GetShader<FDeferredDecalPS>();

	Context.RHICmdList.SetLocalBoundShaderState(Context.RHICmdList.BuildLocalBoundShaderState(GetVertexDeclarationFVector3(), VertexShader->GetVertexShader(), FHullShaderRHIRef(), FDomainShaderRHIRef(), PixelShader->GetPixelShader(), FGeometryShaderRHIRef()));

	PixelShader->SetParameters(Context.RHICmdList, View, DecalData.MaterialProxy, *DecalData.DecalProxy);
}

bool RenderPreStencil(FRenderingCompositePassContext& Context, const FMaterialShaderMap* MaterialShaderMap, const FMatrix& ComponentToWorldMatrix, const FMatrix& FrustumComponentToClip)
{
	SCOPED_DRAW_EVENT(Context.RHICmdList, RenderPreStencil);

	const FSceneView& View = Context.View;

	float Distance = (View.ViewMatrices.ViewOrigin - ComponentToWorldMatrix.GetOrigin()).Size();
	float Radius = ComponentToWorldMatrix.GetMaximumAxisScale();

	// if not inside
	if(Distance > Radius)
	{
		float EstimatedDecalSize = Radius / Distance;
		
		float StencilSizeThreshold = CVarStencilSizeThreshold.GetValueOnRenderThread();

		// Check if it's large enough on screen
		if(EstimatedDecalSize < StencilSizeThreshold)
		{
			return false;
		}
	}

	FDeferredDecalVS* VertexShader = MaterialShaderMap->GetShader<FDeferredDecalVS>();
	
	Context.RHICmdList.SetLocalBoundShaderState(Context.RHICmdList.BuildLocalBoundShaderState(GetVertexDeclarationFVector3(), VertexShader->GetVertexShader(), FHullShaderRHIRef(), FDomainShaderRHIRef(), NULL, FGeometryShaderRHIRef()));

	VertexShader->SetParameters(Context.RHICmdList, View, FrustumComponentToClip);

	// Set states, the state cache helps us avoiding redundant sets
	Context.RHICmdList.SetRasterizerState(TStaticRasterizerState<FM_Solid, CM_None>::GetRHI());

	// all the same to have DX10 working
	Context.RHICmdList.SetBlendState(TStaticBlendState<
		CW_NONE, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha,		BO_Add, BF_Zero, BF_One,	// Emissive
		CW_NONE, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha,		BO_Add, BF_Zero, BF_One,	// Normal
		CW_NONE, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha,		BO_Add, BF_Zero, BF_One,	// Metallic, Specular
		CW_NONE, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha,		BO_Add, BF_Zero, BF_One,	// BaseColor
		CW_NONE, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha,		BO_Add, BF_Zero, BF_One		// Roughness in r
	>::GetRHI() );

	// Carmack's reverse on the bounds
	Context.RHICmdList.SetDepthStencilState(TStaticDepthStencilState<
		false,CF_LessEqual,
		true,CF_Equal,SO_Keep,SO_Keep,SO_Increment,
		true,CF_Equal,SO_Keep,SO_Keep,SO_Decrement,
		0x80,0x7f
	>::GetRHI());

	// Render decal mask
	Context.RHICmdList.DrawIndexedPrimitive(GUnitCubeIndexBuffer.IndexBufferRHI, PT_TriangleList, 0, 0, 8, 0, GUnitCubeIndexBuffer.GetIndexCount() / 3, 0);

	return true;
}

bool IsDBufferEnabled()
{
	static auto* CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.DBuffer"));

	return CVar->GetValueOnRenderThread() > 0;
}


FRCPassPostProcessDeferredDecals::FRCPassPostProcessDeferredDecals(uint32 InRenderStage)
	: RenderStage(InRenderStage)
{
}

void FRCPassPostProcessDeferredDecals::Process(FRenderingCompositePassContext& Context)
{
	
	FRHICommandListImmediate& RHICmdList = Context.RHICmdList;

	bool bDBuffer = IsDBufferEnabled();
	float bDecalPreStencil = CVarStencilSizeThreshold.GetValueOnRenderThread() >= 0;

	SCOPED_DRAW_EVENT(RHICmdList, PostProcessDeferredDecals);

	if(RenderStage == 0)
	{
		// before BasePass, only if DBuffer is enabled

		check(bDBuffer);

		// DBuffer: Decal buffer
		FPooledRenderTargetDesc Desc(FPooledRenderTargetDesc::Create2DDesc(GSceneRenderTargets.GBufferA->GetDesc().Extent, 
			PF_B8G8R8A8, 
			TexCreate_None, 
			TexCreate_ShaderResource | TexCreate_RenderTargetable,
			false));

		if(!GSceneRenderTargets.DBufferA)
		{
			GRenderTargetPool.FindFreeElement(Desc, GSceneRenderTargets.DBufferA, TEXT("DBufferA"));
		}

		if(!GSceneRenderTargets.DBufferB)
		{
			GRenderTargetPool.FindFreeElement(Desc, GSceneRenderTargets.DBufferB, TEXT("DBufferB"));
		}

		Desc.Format = PF_R8G8;

		if(!GSceneRenderTargets.DBufferC)
		{
			GRenderTargetPool.FindFreeElement(Desc, GSceneRenderTargets.DBufferC, TEXT("DBufferC"));
		}


		SCOPED_DRAW_EVENT(RHICmdList, DBufferClear);
		{
			// could be optimized
			SetRenderTarget(RHICmdList, GSceneRenderTargets.DBufferA->GetRenderTargetItem().TargetableTexture, FTextureRHIParamRef());
			RHICmdList.Clear(true, FLinearColor(0, 0, 0, 1), false, 0, false, 0, FIntRect());
			SetRenderTarget(RHICmdList, GSceneRenderTargets.DBufferB->GetRenderTargetItem().TargetableTexture, FTextureRHIParamRef());
			// todo: some hardware would like to have 0 or 1 for faster clear, we chose 128/255 to represent 0 (8 bit cannot represent 0.5f)
			RHICmdList.Clear(true, FLinearColor(128.0f / 255.0f, 128.0f / 255.0f, 128.0f / 255.0f, 1), false, 0, false, 0, FIntRect());
			SetRenderTarget(RHICmdList, GSceneRenderTargets.DBufferC->GetRenderTargetItem().TargetableTexture, FTextureRHIParamRef());
			// R:roughness, G:roughness opacity
			RHICmdList.Clear(true, FLinearColor(0, 1, 0, 1), false, 0, false, 0, FIntRect());
		}
	}

	// this cast is safe as only the dedicated server implements this differently and this pass should not be executed on the dedicated server
	const FViewInfo& View = Context.View;
	const FSceneViewFamily& ViewFamily = *(View.Family);
	FScene& Scene = *(FScene*)ViewFamily.Scene;

	if(!Scene.Decals.Num())
	{
		// to avoid the stats showing up
		return;
	}

	TArray<FTransientDecalRenderData, SceneRenderingAllocator> SortedDecals;
	SortedDecals.Empty(Scene.Decals.Num());

	// Build a list of decals that need to be rendered for this view in SortedDecals
	for (TSparseArray<FDeferredDecalProxy*>::TConstIterator It(Scene.Decals); It; ++It)
	{
		FDeferredDecalProxy* DecalProxy = *It;
		bool bIsShown = true;

		// Handle the decal actor having bHidden set when we are in the editor, in G mode
#if WITH_EDITOR
		if (View.Family->EngineShowFlags.Editor)
#endif
		{
			if (!DecalProxy->DrawInGame)
			{
				bIsShown = false;
			}
		}

		const FMatrix ComponentToWorldMatrix = DecalProxy->ComponentTrans.ToMatrixWithScale();

		// can be optimized as we test against a sphere around the box instead of the box itself
		const float ConservativeRadius = FMath::Sqrt(
			ComponentToWorldMatrix.GetScaledAxis( EAxis::X ).SizeSquared() * FMath::Square(GDefaultDecalSize.X) +
			ComponentToWorldMatrix.GetScaledAxis( EAxis::Y ).SizeSquared() * FMath::Square(GDefaultDecalSize.Y) +
			ComponentToWorldMatrix.GetScaledAxis( EAxis::Z ).SizeSquared() * FMath::Square(GDefaultDecalSize.Z));

		// can be optimized as the test is too conservative (sphere instead of OBB)
		if(ConservativeRadius < SMALL_NUMBER || !View.ViewFrustum.IntersectSphere(ComponentToWorldMatrix.GetOrigin(), ConservativeRadius))
		{
			bIsShown = false;
		}

		if (bIsShown)
		{
			FTransientDecalRenderData Data(Scene, DecalProxy);

			uint32 DecalRenderStage = ComputeRenderStage(Data.DecalBlendMode);

			// we could do this test earlier to avoid the decal intersection but getting DecalBlendMode also costs
			if(RenderStage == DecalRenderStage)
			{
				SortedDecals.Add(Data);
			}
		}
	}

	if(SortedDecals.Num() > 0)
	{
		FIntRect SrcRect = View.ViewRect;
		FIntRect DestRect = View.ViewRect;

		// later we also optimize RenderStage == 0 but we would need to output different stencil depending on Stencil Mask
		bool bStencilDecals = (RenderStage == 1);

		// Setup a stencil mask to prevent certain pixels from receiving deferred decals
		if(bStencilDecals)
		{
			StencilDecalMask(RHICmdList, View);
		}

		// Sort by sort order to allow control over composited result
		// Then sort decals by state to reduce render target switches
		// Also sort by component since Sort() is not stable
		struct FCompareFTransientDecalRenderData
		{
			FORCEINLINE bool operator()( const FTransientDecalRenderData& A, const FTransientDecalRenderData& B ) const
			{
				if (B.DecalProxy->SortOrder != A.DecalProxy->SortOrder)
				{ 
					return A.DecalProxy->SortOrder < B.DecalProxy->SortOrder;
				}
				if (B.DecalBlendMode != A.DecalBlendMode)
				{
					return (int32)B.DecalBlendMode < (int32)A.DecalBlendMode;
				}
				if (B.bHasNormal != A.bHasNormal)
				{
					return B.bHasNormal < A.bHasNormal;
				}
				// Batch decals with the same material together
				if (B.MaterialProxy != A.MaterialProxy )
				{
					return B.MaterialProxy < A.MaterialProxy;
				}
				return (PTRINT)B.DecalProxy->Component < (PTRINT)A.DecalProxy->Component;
			}
		};

		// Sort decals by blend mode to reduce render target switches
		SortedDecals.Sort( FCompareFTransientDecalRenderData() );

		// optimization to have less state changes
		int32 LastDecalBlendMode = -1;
		int32 LastDecalHasNormal = -1; // Decal state can change based on its normal property.(SM5)
		ERenderTargetMode LastRenderTargetMode = RTM_Unknown;
		int32 WasInsideDecal = -1;
		const ERHIFeatureLevel::Type SMFeatureLevel = Context.GetFeatureLevel();

		SCOPED_DRAW_EVENT(RHICmdList, Decals);
		INC_DWORD_STAT_BY(STAT_Decals, SortedDecals.Num());
		
		RHICmdList.SetStreamSource(0, GUnitCubeVertexBuffer.VertexBufferRHI, sizeof(FVector4), 0);

		for (int32 DecalIndex = 0; DecalIndex < SortedDecals.Num(); DecalIndex++)
		{
			const FTransientDecalRenderData& DecalData = SortedDecals[DecalIndex];
			const FDeferredDecalProxy& DecalProxy = *DecalData.DecalProxy;
			const FMatrix ComponentToWorldMatrix = DecalProxy.ComponentTrans.ToMatrixWithScale();

			// Set vertex shader params
			const FMaterialShaderMap* MaterialShaderMap = DecalData.MaterialResource->GetRenderingThreadShaderMap();
			
			FScaleMatrix DecalScaleTransform(GDefaultDecalSize);
			FTranslationMatrix PreViewTranslation(View.ViewMatrices.PreViewTranslation);
			FMatrix FrustumComponentToClip = DecalScaleTransform * ComponentToWorldMatrix * PreViewTranslation * View.ViewMatrices.TranslatedViewProjectionMatrix;

			bool bThisDecalUsesStencil = false;

			if(bStencilDecals)
			{
				if(bDecalPreStencil)
				{
					bThisDecalUsesStencil = RenderPreStencil(Context, MaterialShaderMap, ComponentToWorldMatrix, FrustumComponentToClip);
					WasInsideDecal = -1;
					LastDecalBlendMode = -1;
				}
			}

			// can be optimized as we test against a sphere around the box instead of the box itself
			const float ConservativeRadius = FMath::Sqrt(
				ComponentToWorldMatrix.GetScaledAxis( EAxis::X ).SizeSquared() * FMath::Square(GDefaultDecalSize.X) +
				ComponentToWorldMatrix.GetScaledAxis( EAxis::Y ).SizeSquared() * FMath::Square(GDefaultDecalSize.Y) +
				ComponentToWorldMatrix.GetScaledAxis( EAxis::Z ).SizeSquared() * FMath::Square(GDefaultDecalSize.Z));

			EDecalBlendMode DecalBlendMode = DecalData.DecalBlendMode;

			ERenderTargetMode CurrentRenderTargetMode = ComputeRenderTargetMode(DecalBlendMode);

			// fewer rendertarget switches if possible
			if(CurrentRenderTargetMode != LastRenderTargetMode)
			{
				LastRenderTargetMode = CurrentRenderTargetMode;

				switch(CurrentRenderTargetMode)
				{
					case RTM_SceneColorAndGBuffer:
						{
							FTextureRHIParamRef RenderTargets[5];
							RenderTargets[0] = GSceneRenderTargets.GetSceneColor()->GetRenderTargetItem().TargetableTexture;
							RenderTargets[1] = GSceneRenderTargets.GBufferA->GetRenderTargetItem().TargetableTexture;
							RenderTargets[2] = GSceneRenderTargets.GBufferB->GetRenderTargetItem().TargetableTexture;
							RenderTargets[3] = GSceneRenderTargets.GBufferC->GetRenderTargetItem().TargetableTexture;
							RenderTargets[4] = GSceneRenderTargets.GBufferD->GetRenderTargetItem().TargetableTexture;
							SetRenderTargets(RHICmdList, ARRAY_COUNT(RenderTargets), RenderTargets, GSceneRenderTargets.GetSceneDepthSurface(), 0, NULL);
						}
						break;

					case RTM_GBufferNormal:
						SetRenderTarget(RHICmdList, GSceneRenderTargets.GBufferA->GetRenderTargetItem().TargetableTexture, GSceneRenderTargets.GetSceneDepthSurface());
						break;
					
					case RTM_SceneColor:
						SetRenderTarget(RHICmdList, GSceneRenderTargets.GetSceneColor()->GetRenderTargetItem().TargetableTexture, GSceneRenderTargets.GetSceneDepthSurface());
						break;

					case RTM_DBuffer:
						{
							FTextureRHIParamRef RenderTargets[3];
							RenderTargets[0] = GSceneRenderTargets.DBufferA->GetRenderTargetItem().TargetableTexture;
							RenderTargets[1] = GSceneRenderTargets.DBufferB->GetRenderTargetItem().TargetableTexture;
							RenderTargets[2] = GSceneRenderTargets.DBufferC->GetRenderTargetItem().TargetableTexture;
							SetRenderTargets(RHICmdList, ARRAY_COUNT(RenderTargets), RenderTargets, GSceneRenderTargets.GetSceneDepthSurface(), 0, NULL);
						}
						break;

					default:
						check(0);	
						break;
				}
				Context.SetViewportAndCallRHI(DestRect);
			}

			const bool bBlendStateChange = DecalData.DecalBlendMode != LastDecalBlendMode;// Has decal mode changed.
			const bool bDecalNormalChanged = GSupportsSeparateRenderTargetBlendState && // has normal changed for SM5 stain/translucent decals?
							(DecalData.DecalBlendMode == DBM_Translucent || DecalData.DecalBlendMode == DBM_Stain) &&
							(int32)DecalData.bHasNormal != LastDecalHasNormal;

			// fewer blend state changes if possible
			if (bBlendStateChange || bDecalNormalChanged)
			{
				LastDecalBlendMode = DecalData.DecalBlendMode;
				LastDecalHasNormal = (int32)DecalData.bHasNormal;

				SetDecalBlendState(RHICmdList, SMFeatureLevel, RenderStage, (EDecalBlendMode)LastDecalBlendMode, DecalData.bHasNormal);
			}

			{
				FDeferredDecalVS* VertexShader = MaterialShaderMap->GetShader<FDeferredDecalVS>();
				SetShader(Context, DecalData, VertexShader);

				VertexShader->SetParameters(RHICmdList, View, FrustumComponentToClip);

				const int32 IsInsideDecal = ((FVector)View.ViewMatrices.ViewOrigin - ComponentToWorldMatrix.GetOrigin()).SizeSquared() < FMath::Square(ConservativeRadius * 1.05f + View.NearClippingDistance * 2.0f) + ( bThisDecalUsesStencil ) ? 2 : 0;
				if ( WasInsideDecal != IsInsideDecal )
				{
					WasInsideDecal = IsInsideDecal;
					if ( !(IsInsideDecal & 1) )
					{
						// Render backfaces with depth tests disabled since the camera is inside (or close to inside) the light function geometry
						RHICmdList.SetRasterizerState(View.bReverseCulling ? TStaticRasterizerState<FM_Solid, CM_CCW>::GetRHI() : TStaticRasterizerState<FM_Solid, CM_CW>::GetRHI());
						if(bStencilDecals)
						{
							// Enable stencil testing, only write to pixels with stencil of 0
							if ( bThisDecalUsesStencil )
							{
								RHICmdList.SetDepthStencilState(TStaticDepthStencilState<
									false,CF_Always,
									true,CF_Equal,SO_Zero,SO_Zero,SO_Zero,
									true,CF_Equal,SO_Zero,SO_Zero,SO_Zero,
									0xff, 0x7f
								>::GetRHI(), 1);
							}
							else
							{
								RHICmdList.SetDepthStencilState(TStaticDepthStencilState<
									false,CF_Always,
									true,CF_Equal,SO_Keep,SO_Keep,SO_Keep,
									false,CF_Always,SO_Keep,SO_Keep,SO_Keep,
									0x80,0x00>::GetRHI(), 0);
							}
						}
						else
						{
							RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always, true>::GetRHI(), 0);
						}
					}
					else
					{
						// Render frontfaces with depth tests on to get the speedup from HiZ since the camera is outside the light function geometry
						// Note, this is a reversed Z depth surface, using CF_GreaterEqual.
						if(bStencilDecals)
						{
							// Render frontfaces with depth tests on to get the speedup from HiZ since the camera is outside the light function geometry
							// Enable stencil testing, only write to pixels with stencil of 0
							// Note, this is a reversed Z depth surface, using CF_GreaterEqual.
							if ( bThisDecalUsesStencil )
							{
								RHICmdList.SetDepthStencilState(TStaticDepthStencilState<
									false,CF_GreaterEqual,
									true,CF_Equal,SO_Zero,SO_Zero,SO_Zero,
									true,CF_Equal,SO_Zero,SO_Zero,SO_Zero,
									0xff, 0x7f
								>::GetRHI(), 1);
							}
							else
							{
								RHICmdList.SetDepthStencilState(TStaticDepthStencilState<
									false,CF_GreaterEqual,
									true,CF_Equal,SO_Keep,SO_Keep,SO_Keep,
									false,CF_Always,SO_Keep,SO_Keep,SO_Keep,
									0x80,0x00>::GetRHI(), 0);
							}
							RHICmdList.SetRasterizerState(View.bReverseCulling ? TStaticRasterizerState<FM_Solid, CM_CW>::GetRHI() : TStaticRasterizerState<FM_Solid, CM_CCW>::GetRHI());
						}
						else
						{
							RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false, CF_GreaterEqual>::GetRHI(), 0);
						}
						RHICmdList.SetRasterizerState(View.bReverseCulling ? TStaticRasterizerState<FM_Solid, CM_CW>::GetRHI() : TStaticRasterizerState<FM_Solid, CM_CCW>::GetRHI());
					}
				}

				SetShader(Context, DecalData, VertexShader);

				RHICmdList.DrawIndexedPrimitive(GUnitCubeIndexBuffer.IndexBufferRHI, PT_TriangleList, 0, 0, 8, 0, GUnitCubeIndexBuffer.GetIndexCount() / 3, 0);
			}
		}

		// @todo resolve - need to remember the target(s) and resolve them

		// we don't modify stencil but if out input was having stencil for us (after base pass - we need to clear)

		// Clear stencil to 0, which is the assumed default by other passes
		RHICmdList.Clear(false, FLinearColor::White, false, 0, true, 0, FIntRect());
	}

	if(RenderStage == 0)
	{
		// before BasePass
		GRenderTargetPool.VisualizeTexture.SetCheckPoint(RHICmdList, GSceneRenderTargets.DBufferA);
		GRenderTargetPool.VisualizeTexture.SetCheckPoint(RHICmdList, GSceneRenderTargets.DBufferB);
		GRenderTargetPool.VisualizeTexture.SetCheckPoint(RHICmdList, GSceneRenderTargets.DBufferC);
	}
}

FPooledRenderTargetDesc FRCPassPostProcessDeferredDecals::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	// This pass creates it's own output so the compositing graph output isn't needed.
	FPooledRenderTargetDesc Ret;
	
	Ret.DebugName = TEXT("DeferredDecals");

	return Ret;
}
