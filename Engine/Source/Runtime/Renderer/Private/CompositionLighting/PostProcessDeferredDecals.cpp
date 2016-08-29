// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

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
#include "DecalRenderingShared.h"




static TAutoConsoleVariable<int32> CVarGenerateDecalRTWriteMaskTexture(
	TEXT("r.Decal.GenerateRTWriteMaskTexture"),
	1,
	TEXT("Turn on or off generation of the RT write mask texture for decals\n"),
	ECVF_Default);




class FRTWriteMaskDecodeCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FRTWriteMaskDecodeCS, Global);

public:
	static const uint32 ThreadGroupSizeX = 8;
	static const uint32 ThreadGroupSizeY = 8;

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEX"), ThreadGroupSizeX);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEY"), ThreadGroupSizeY);

		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
	}

	FRTWriteMaskDecodeCS() {}

public:
	FShaderParameter OutCombinedRTWriteMask;		// UAV
	FShaderResourceParameter RTWriteMaskInput0;				// SRV 
	FShaderResourceParameter RTWriteMaskInput1;				// SRV 
	FShaderResourceParameter RTWriteMaskInput2;				// SRV 
	FShaderParameter UtilizeMask;				// SRV 

	FRTWriteMaskDecodeCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		RTWriteMaskDimensions.Bind(Initializer.ParameterMap, TEXT("RTWriteMaskDimensions"));
		OutCombinedRTWriteMask.Bind(Initializer.ParameterMap, TEXT("OutCombinedRTWriteMask"));
		RTWriteMaskInput0.Bind(Initializer.ParameterMap, TEXT("RTWriteMaskInput0"));
		RTWriteMaskInput1.Bind(Initializer.ParameterMap, TEXT("RTWriteMaskInput1"));
		RTWriteMaskInput2.Bind(Initializer.ParameterMap, TEXT("RTWriteMaskInput2"));
		UtilizeMask.Bind(Initializer.ParameterMap, TEXT("UtilizeMask"));
	}

	void SetCS(FRHICommandList& RHICmdList, const FRenderingCompositePassContext& Context, const FSceneView& View, FIntPoint WriteMaskDimensions)
	{
		const FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();

		FGlobalShader::SetParameters(RHICmdList, ShaderRHI, Context.View);
		//PostprocessParameter.SetCS(ShaderRHI, Context, Context.RHICmdList, TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI());

		FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

		SetShaderValue(Context.RHICmdList, ShaderRHI, RTWriteMaskDimensions, WriteMaskDimensions);
		SetSRVParameter(Context.RHICmdList, ShaderRHI, RTWriteMaskInput0, SceneContext.DBufferA->GetRenderTargetItem().RTWriteMaskBufferRHI_SRV);
		SetSRVParameter(Context.RHICmdList, ShaderRHI, RTWriteMaskInput1, SceneContext.DBufferB->GetRenderTargetItem().RTWriteMaskBufferRHI_SRV);
		SetSRVParameter(Context.RHICmdList, ShaderRHI, RTWriteMaskInput2, SceneContext.DBufferC->GetRenderTargetItem().RTWriteMaskBufferRHI_SRV);
		int32 UseMask = CVarGenerateDecalRTWriteMaskTexture.GetValueOnRenderThread();
		SetShaderValue(Context.RHICmdList, ShaderRHI, UtilizeMask, UseMask);
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << RTWriteMaskDimensions;
		Ar << OutCombinedRTWriteMask;
		Ar << RTWriteMaskInput0;
		Ar << RTWriteMaskInput1;
		Ar << RTWriteMaskInput2;
		Ar << UtilizeMask;
		return bShaderHasOutdatedParameters;
	}

private:
	FShaderParameter			RTWriteMaskDimensions;
};

IMPLEMENT_SHADER_TYPE(, FRTWriteMaskDecodeCS, TEXT("RTWriteMaskDecode"), TEXT("RTWriteMaskCombineMain"), SF_Compute);

static TAutoConsoleVariable<float> CVarStencilSizeThreshold(
	TEXT("r.Decal.StencilSizeThreshold"),
	0.1f,
	TEXT("Control a per decal stencil pass that allows to large (screen space) decals faster. It adds more overhead per decals so this\n")
	TEXT("  <0: optimization is disabled\n")
	TEXT("   0: optimization is enabled no matter how small (screen space) the decal is\n")
	TEXT("0..1: optimization is enabled, value defines the minimum size (screen space) to trigger the optimization (default 0.1)")
	);

enum EDecalDepthInputState
{
	DDS_Undefined,
	DDS_Always,
	DDS_DepthTest,
	DDS_DepthAlways_StencilEqual1,
	DDS_DepthAlways_StencilEqual1_IgnoreMask,
	DDS_DepthAlways_StencilEqual0,
	DDS_DepthTest_StencilEqual1,
	DDS_DepthTest_StencilEqual1_IgnoreMask,
	DDS_DepthTest_StencilEqual0,
};

struct FDecalDepthState
{
	EDecalDepthInputState DepthTest;
	bool bDepthOutput;

	FDecalDepthState()
		: DepthTest(DDS_Undefined)
		, bDepthOutput(false)
	{
	}

	bool operator !=(const FDecalDepthState &rhs) const
	{
		return DepthTest != rhs.DepthTest || bDepthOutput != rhs.bDepthOutput;
	}
};

enum EDecalRasterizerState
{
	DRS_Undefined,
	DRS_CCW,
	DRS_CW,
};

// @param RenderState 0:before BasePass, 1:before lighting, (later we could add "after lighting" and multiply)
void SetDecalBlendState(FRHICommandList& RHICmdList, const ERHIFeatureLevel::Type SMFeatureLevel, EDecalRenderStage InDecalRenderStage, EDecalBlendMode DecalBlendMode, bool bHasNormal)
{
	if(InDecalRenderStage == DRS_BeforeBasePass)
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
	else if(InDecalRenderStage == DRS_AfterBasePass)
	{
		ensure(DecalBlendMode == DBM_Volumetric_DistanceFunction);

		RHICmdList.SetBlendState( TStaticBlendState<>::GetRHI() );
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
						CW_RGB, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha,		BO_Add, BF_Zero, BF_One,	// Metallic, Specular, Roughness
						CW_RGB, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha,		BO_Add, BF_Zero, BF_One		// BaseColor
					>::GetRHI() );
				}
				else
				{
					RHICmdList.SetBlendState( TStaticBlendState<
						CW_RGB, BO_Add, BF_SourceAlpha, BF_One,						BO_Add, BF_Zero, BF_One,	// Emissive
						CW_RGB, BO_Add, BF_Zero,		BF_One,						BO_Add, BF_Zero, BF_One,	// Normal
						CW_RGB, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha,		BO_Add, BF_Zero, BF_One,	// Metallic, Specular, Roughness
						CW_RGB, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha,		BO_Add, BF_Zero, BF_One		// BaseColor
					>::GetRHI() );
				}
			}
			else if(SMFeatureLevel == ERHIFeatureLevel::SM4)
			{
				RHICmdList.SetBlendState( TStaticBlendState<
					CW_RGB, BO_Add, BF_SourceAlpha, BF_One,							BO_Add, BF_Zero, BF_One,	// Emissive
					CW_RGB, BO_Add, BF_SourceAlpha, BF_One,							BO_Add, BF_Zero, BF_One,	// Normal
					CW_RGB, BO_Add, BF_SourceAlpha, BF_One,							BO_Add, BF_Zero, BF_One,	// Metallic, Specular, Roughness
					CW_RGB, BO_Add, BF_SourceAlpha,	BF_One,							BO_Add, BF_Zero, BF_One		// BaseColor
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
						CW_RGB, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha,		BO_Add, BF_Zero, BF_One,	// Metallic, Specular, Roughness
						CW_RGB, BO_Add, BF_DestColor,	BF_InverseSourceAlpha,		BO_Add, BF_Zero, BF_One		// BaseColor
					>::GetRHI() );
				}
				else
				{
					RHICmdList.SetBlendState( TStaticBlendState<
						CW_RGB, BO_Add, BF_SourceAlpha, BF_One,						BO_Add, BF_Zero, BF_One,	// Emissive
						CW_RGB, BO_Add, BF_Zero,		BF_One,						BO_Add, BF_Zero, BF_One,	// Normal
						CW_RGB, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha,		BO_Add, BF_Zero, BF_One,	// Metallic, Specular, Roughness
						CW_RGB, BO_Add, BF_DestColor,	BF_InverseSourceAlpha,		BO_Add, BF_Zero, BF_One		// BaseColor
					>::GetRHI() );
				}
			}
			else if(SMFeatureLevel == ERHIFeatureLevel::SM4)
			{
				RHICmdList.SetBlendState( TStaticBlendState<
					CW_RGB, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha,			BO_Add, BF_Zero, BF_One,	// Emissive
					CW_RGB, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha,			BO_Add, BF_Zero, BF_One,	// Normal
					CW_RGB, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha,			BO_Add, BF_Zero, BF_One,	// Metallic, Specular, Roughness
					CW_RGB, BO_Add, BF_SourceAlpha,	BF_InverseSourceAlpha,			BO_Add, BF_Zero, BF_One		// BaseColor
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

bool RenderPreStencil(FRenderingCompositePassContext& Context, const FMatrix& ComponentToWorldMatrix, const FMatrix& FrustumComponentToClip)
{
	const FViewInfo& View = Context.View;

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
	
	FDecalRendering::SetVertexShaderOnly(Context.RHICmdList, View, FrustumComponentToClip);

	// Set states, the state cache helps us avoiding redundant sets
	Context.RHICmdList.SetRasterizerState(TStaticRasterizerState<FM_Solid, CM_None>::GetRHI());

	// all the same to have DX10 working
	Context.RHICmdList.SetBlendState(TStaticBlendState<
		CW_NONE, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha,		BO_Add, BF_Zero, BF_One,	// Emissive
		CW_NONE, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha,		BO_Add, BF_Zero, BF_One,	// Normal
		CW_NONE, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha,		BO_Add, BF_Zero, BF_One,	// Metallic, Specular, Roughness
		CW_NONE, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha,		BO_Add, BF_Zero, BF_One		// BaseColor
	>::GetRHI() );

	// Carmack's reverse the sandbox stencil bit on the bounds
	Context.RHICmdList.SetDepthStencilState(TStaticDepthStencilState<
		false,CF_LessEqual,
		true,CF_Always,SO_Keep,SO_Keep,SO_Invert,
		true,CF_Always,SO_Keep,SO_Keep,SO_Invert,
		STENCIL_SANDBOX_MASK,STENCIL_SANDBOX_MASK
	>::GetRHI(), 0);

	// Render decal mask
	Context.RHICmdList.DrawIndexedPrimitive(GetUnitCubeIndexBuffer(), PT_TriangleList, 0, 0, 8, 0, ARRAY_COUNT(GCubeIndices) / 3, 1);

	return true;
}

bool IsDBufferEnabled()
{
	static auto* CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.DBuffer"));

	return CVar->GetValueOnRenderThread() > 0;
}


static EDecalRasterizerState ComputeDecalRasterizerState(bool bInsideDecal, bool bIsInverted, const FViewInfo& View)
{
	bool bClockwise = bInsideDecal;

	if(View.bReverseCulling)
	{
		bClockwise = !bClockwise;
	}

	if (bIsInverted) 
	{
		bClockwise = !bClockwise;
	}
	return bClockwise ? DRS_CW : DRS_CCW;
}

FRCPassPostProcessDeferredDecals::FRCPassPostProcessDeferredDecals(EDecalRenderStage InDecalRenderStage)
	: CurrentStage(InDecalRenderStage)
{
}

static FDecalDepthState ComputeDecalDepthState(EDecalRenderStage LocalDecalStage, bool bInsideDecal, bool bThisDecalUsesStencil)
{
	FDecalDepthState Ret;

	Ret.bDepthOutput = (LocalDecalStage == DRS_AfterBasePass);
				
	if(Ret.bDepthOutput)
	{
		// can be made one enum
		Ret.DepthTest = DDS_DepthTest;
		return Ret;
	}

	const bool bGBufferDecal = LocalDecalStage == DRS_BeforeLighting;

	if(bInsideDecal)
	{
		if(bThisDecalUsesStencil)
		{
			Ret.DepthTest = bGBufferDecal ? DDS_DepthAlways_StencilEqual1 : DDS_DepthAlways_StencilEqual1_IgnoreMask;
		}
		else
		{
			Ret.DepthTest = bGBufferDecal ? DDS_DepthAlways_StencilEqual0 : DDS_Always;
		}
	}
	else
	{
		if(bThisDecalUsesStencil)
		{
			Ret.DepthTest = bGBufferDecal ? DDS_DepthTest_StencilEqual1 : DDS_DepthTest_StencilEqual1_IgnoreMask;
		}
		else
		{
			Ret.DepthTest = bGBufferDecal ? DDS_DepthTest_StencilEqual0 : DDS_DepthTest;
		}
	}

	return Ret;
}

static void SetDecalDepthState(FDecalDepthState DecalDepthState, FRHICommandListImmediate& RHICmdList)
{
	switch(DecalDepthState.DepthTest)
	{
		case DDS_DepthAlways_StencilEqual1:
			check(!DecalDepthState.bDepthOutput);			// todo
			RHICmdList.SetDepthStencilState(TStaticDepthStencilState<
				false,CF_Always,
				true,CF_Equal,SO_Zero,SO_Zero,SO_Zero,
				true,CF_Equal,SO_Zero,SO_Zero,SO_Zero,
				STENCIL_SANDBOX_MASK | GET_STENCIL_BIT_MASK(RECEIVE_DECAL, 1),STENCIL_SANDBOX_MASK>::GetRHI(),
				STENCIL_SANDBOX_MASK | GET_STENCIL_BIT_MASK(RECEIVE_DECAL, 1));
			break;
			
		case DDS_DepthAlways_StencilEqual1_IgnoreMask:
			check(!DecalDepthState.bDepthOutput);			// todo
			RHICmdList.SetDepthStencilState(TStaticDepthStencilState<
				false,CF_Always,
				true,CF_Equal,SO_Zero,SO_Zero,SO_Zero,
				true,CF_Equal,SO_Zero,SO_Zero,SO_Zero,
				STENCIL_SANDBOX_MASK,STENCIL_SANDBOX_MASK>::GetRHI(),
				STENCIL_SANDBOX_MASK);
			break;

		case DDS_DepthAlways_StencilEqual0:
			check(!DecalDepthState.bDepthOutput);			// todo
			RHICmdList.SetDepthStencilState(TStaticDepthStencilState<
				false,CF_Always,
				true,CF_Equal,SO_Keep,SO_Keep,SO_Keep,
				false,CF_Always,SO_Keep,SO_Keep,SO_Keep,
				STENCIL_SANDBOX_MASK | GET_STENCIL_BIT_MASK(RECEIVE_DECAL, 1),0x00>::GetRHI(),
				GET_STENCIL_BIT_MASK(RECEIVE_DECAL, 1));
			break;

		case DDS_Always:
			check(!DecalDepthState.bDepthOutput);			// todo
			RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false,CF_Always>::GetRHI());
			break;

		case DDS_DepthTest_StencilEqual1:
			check(!DecalDepthState.bDepthOutput);			// todo
			RHICmdList.SetDepthStencilState(TStaticDepthStencilState<
				false,CF_DepthNearOrEqual,
				true,CF_Equal,SO_Zero,SO_Zero,SO_Zero,
				true,CF_Equal,SO_Zero,SO_Zero,SO_Zero,
				STENCIL_SANDBOX_MASK | GET_STENCIL_BIT_MASK(RECEIVE_DECAL, 1),STENCIL_SANDBOX_MASK>::GetRHI(),
				STENCIL_SANDBOX_MASK | GET_STENCIL_BIT_MASK(RECEIVE_DECAL, 1));
			break;
			
		case DDS_DepthTest_StencilEqual1_IgnoreMask:
			check(!DecalDepthState.bDepthOutput);			// todo
			RHICmdList.SetDepthStencilState(TStaticDepthStencilState<
				false,CF_DepthNearOrEqual,
				true,CF_Equal,SO_Zero,SO_Zero,SO_Zero,
				true,CF_Equal,SO_Zero,SO_Zero,SO_Zero,
				STENCIL_SANDBOX_MASK,STENCIL_SANDBOX_MASK>::GetRHI(),
				STENCIL_SANDBOX_MASK);
			break;

		case DDS_DepthTest_StencilEqual0:
			check(!DecalDepthState.bDepthOutput);			// todo
			RHICmdList.SetDepthStencilState(TStaticDepthStencilState<
				false,CF_DepthNearOrEqual,
				true,CF_Equal,SO_Keep,SO_Keep,SO_Keep,
				false,CF_Always,SO_Keep,SO_Keep,SO_Keep,
				STENCIL_SANDBOX_MASK | GET_STENCIL_BIT_MASK(RECEIVE_DECAL, 1),0x00>::GetRHI(),
				GET_STENCIL_BIT_MASK(RECEIVE_DECAL, 1));
			break;

		case DDS_DepthTest:
			if(DecalDepthState.bDepthOutput)
			{
				RHICmdList.SetDepthStencilState(TStaticDepthStencilState<true, CF_DepthNearOrEqual>::GetRHI(), 0);
			}
			else
			{
				RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false, CF_DepthNearOrEqual>::GetRHI(), 0);
			}
			break;

		default:
			check(0);
	}
}

static void SetDecalRasterizerState(EDecalRasterizerState DecalRasterizerState, FRHICommandList& RHICmdList)
{
	switch (DecalRasterizerState)
	{
		case DRS_CW: RHICmdList.SetRasterizerState(TStaticRasterizerState<FM_Solid, CM_CW>::GetRHI()); break;
		case DRS_CCW: RHICmdList.SetRasterizerState(TStaticRasterizerState<FM_Solid, CM_CCW>::GetRHI()); break;
		default: check(0);
	}
}

static inline bool IsStencilOptimizationAvailable(EDecalRenderStage RenderStage)
{
	return RenderStage == DRS_BeforeLighting || RenderStage == DRS_BeforeBasePass;
}

const TCHAR* GetStageName(EDecalRenderStage Stage)
{
	// could be implemented with enum reflections as well

	switch(Stage)
	{
		case DRS_BeforeBasePass: return TEXT("DRS_BeforeBasePass");
		case DRS_AfterBasePass: return TEXT("DRS_AfterBasePass");
		case DRS_BeforeLighting: return TEXT("DRS_BeforeLighting");
		case DRS_Mobile: return TEXT("DRS_Mobile");
	}
	return TEXT("<UNKNOWN>");
}


void FRCPassPostProcessDeferredDecals::DecodeRTWriteMask(FRenderingCompositePassContext& Context)
{
	// @todo: get these values from the RHI?
	const uint32 MaskTileSizeX = 8;
	const uint32 MaskTileSizeY = 8;

	check(GSupportsRenderTargetWriteMask);

	FRHICommandListImmediate& RHICmdList = Context.RHICmdList;
	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);
	FTextureRHIRef DBufferTex = SceneContext.DBufferA->GetRenderTargetItem().TargetableTexture;

	FIntPoint RTWriteMaskDims(
		FMath::DivideAndRoundUp(DBufferTex->GetTexture2D()->GetSizeX(), MaskTileSizeX),
		FMath::DivideAndRoundUp(DBufferTex->GetTexture2D()->GetSizeY(), MaskTileSizeY));

	// allocate the DBufferMask from the render target pool.
	FPooledRenderTargetDesc MaskDesc(FPooledRenderTargetDesc::Create2DDesc(RTWriteMaskDims,
		PF_R8_UINT,
		FClearValueBinding::White,
		TexCreate_None,
		TexCreate_UAV | TexCreate_RenderTargetable,
		false));

	GRenderTargetPool.FindFreeElement(Context.RHICmdList, MaskDesc, SceneContext.DBufferMask, TEXT("DBufferMask"));

	FIntRect ViewRect(0, 0, DBufferTex->GetTexture2D()->GetSizeX(), DBufferTex->GetTexture2D()->GetSizeY());

	TShaderMapRef< FRTWriteMaskDecodeCS > ComputeShader(Context.GetShaderMap());

	SetRenderTarget(Context.RHICmdList, FTextureRHIRef(), FTextureRHIRef());
	Context.SetViewportAndCallRHI(ViewRect);
	Context.RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());

	// set destination
	Context.RHICmdList.SetUAVParameter(ComputeShader->GetComputeShader(), ComputeShader->OutCombinedRTWriteMask.GetBaseIndex(), SceneContext.DBufferMask->GetRenderTargetItem().UAV);
	ComputeShader->SetCS(Context.RHICmdList, Context, Context.View, RTWriteMaskDims);

	RHICmdList.TransitionResource(EResourceTransitionAccess::EWritable, EResourceTransitionPipeline::EGfxToCompute, SceneContext.DBufferMask->GetRenderTargetItem().UAV);
	{
		SCOPED_DRAW_EVENTF(Context.RHICmdList, DeferredDecals, TEXT("Combine DBuffer RTWriteMasks"));

		FIntPoint ThreadGroupCountValue(
			FMath::DivideAndRoundUp((uint32)RTWriteMaskDims.X, FRTWriteMaskDecodeCS::ThreadGroupSizeX),
			FMath::DivideAndRoundUp((uint32)RTWriteMaskDims.Y, FRTWriteMaskDecodeCS::ThreadGroupSizeY));

		DispatchComputeShader(Context.RHICmdList, *ComputeShader, ThreadGroupCountValue.X, ThreadGroupCountValue.Y, 1);
	}

	//	void FD3D11DynamicRHI::RHIGraphicsWaitOnAsyncComputeJob( uint32 FenceIndex )
	Context.RHICmdList.FlushComputeShaderCache();

	RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToGfx, SceneContext.DBufferMask->GetRenderTargetItem().UAV);

	RHICmdList.TransitionResource(EResourceTransitionAccess::EMetaData, SceneContext.DBufferA->GetRenderTargetItem().TargetableTexture);
	RHICmdList.TransitionResource(EResourceTransitionAccess::EMetaData, SceneContext.DBufferB->GetRenderTargetItem().TargetableTexture);
	RHICmdList.TransitionResource(EResourceTransitionAccess::EMetaData, SceneContext.DBufferC->GetRenderTargetItem().TargetableTexture);

	// un-set destination
	Context.RHICmdList.SetUAVParameter(ComputeShader->GetComputeShader(), ComputeShader->OutCombinedRTWriteMask.GetBaseIndex(), NULL);
}


void FRCPassPostProcessDeferredDecals::Process(FRenderingCompositePassContext& Context)
{
	FRHICommandListImmediate& RHICmdList = Context.RHICmdList;
	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

	const bool bShaderComplexity = Context.View.Family->EngineShowFlags.ShaderComplexity;
	const bool bDBuffer = IsDBufferEnabled();
	const bool bStencilSizeThreshold = CVarStencilSizeThreshold.GetValueOnRenderThread() >= 0;

	SCOPED_DRAW_EVENTF(RHICmdList, DeferredDecals, TEXT("DeferredDecals %s"), GetStageName(CurrentStage));

	if (CurrentStage == DRS_BeforeBasePass)
	{
		// before BasePass, only if DBuffer is enabled

		check(bDBuffer);

		FPooledRenderTargetDesc GBufferADesc;
		SceneContext.GetGBufferADesc(GBufferADesc);

		// DBuffer: Decal buffer
		FPooledRenderTargetDesc Desc(FPooledRenderTargetDesc::Create2DDesc(GBufferADesc.Extent,
			PF_B8G8R8A8,
			FClearValueBinding::None,
			TexCreate_None,
			TexCreate_ShaderResource | TexCreate_RenderTargetable,
			false,
			1, 
			true, 
			true));

		if (!SceneContext.DBufferA)
		{
			Desc.ClearValue = FClearValueBinding::Black;
			GRenderTargetPool.FindFreeElement(RHICmdList, Desc, SceneContext.DBufferA, TEXT("DBufferA"));
		}

		if (!SceneContext.DBufferB)
		{
			Desc.ClearValue = FClearValueBinding(FLinearColor(128.0f / 255.0f, 128.0f / 255.0f, 128.0f / 255.0f, 1));
			GRenderTargetPool.FindFreeElement(RHICmdList, Desc, SceneContext.DBufferB, TEXT("DBufferB"));
		}

		Desc.Format = PF_R8G8;

		if (!SceneContext.DBufferC)
		{
			Desc.ClearValue = FClearValueBinding(FLinearColor(0, 1, 0, 1));
			GRenderTargetPool.FindFreeElement(RHICmdList, Desc, SceneContext.DBufferC, TEXT("DBufferC"));
		}

		// we assume views are non overlapping, then we need to clear only once in the beginning, otherwise we would need to set scissor rects
		// and don't get FastClear any more.
		bool bFirstView = Context.View.Family->Views[0] == &Context.View;

		if (bFirstView)
		{
			SCOPED_DRAW_EVENT(RHICmdList, DBufferClear);

			FRHIRenderTargetView RenderTargets[3];
			RenderTargets[0] = FRHIRenderTargetView(SceneContext.DBufferA->GetRenderTargetItem().TargetableTexture, 0, -1, ERenderTargetLoadAction::EClear, ERenderTargetStoreAction::EStore);
			RenderTargets[1] = FRHIRenderTargetView(SceneContext.DBufferB->GetRenderTargetItem().TargetableTexture, 0, -1, ERenderTargetLoadAction::EClear, ERenderTargetStoreAction::EStore);
			RenderTargets[2] = FRHIRenderTargetView(SceneContext.DBufferC->GetRenderTargetItem().TargetableTexture, 0, -1, ERenderTargetLoadAction::EClear, ERenderTargetStoreAction::EStore);

			FRHIDepthRenderTargetView DepthView(SceneContext.GetSceneDepthTexture(), ERenderTargetLoadAction::ELoad, ERenderTargetStoreAction::ENoAction, ERenderTargetLoadAction::ELoad, ERenderTargetStoreAction::ENoAction, FExclusiveDepthStencil(FExclusiveDepthStencil::DepthRead_StencilWrite));

			FRHISetRenderTargetsInfo Info(3, RenderTargets, DepthView);
			RHICmdList.SetRenderTargetsAndClear(Info);
		}
	}

	// this cast is safe as only the dedicated server implements this differently and this pass should not be executed on the dedicated server
	const FViewInfo& View = Context.View;
	const FSceneViewFamily& ViewFamily = *(View.Family);

	bool bHasValidDBufferMask = false;

	if(ViewFamily.EngineShowFlags.Decals)
	{
		if(CurrentStage == DRS_BeforeBasePass || CurrentStage == DRS_BeforeLighting)
		{
			RenderMeshDecals(Context, CurrentStage);
		}

		FScene& Scene = *(FScene*)ViewFamily.Scene;

		//don't early return.  Resolves must be run for fast clears to work.
		if (Scene.Decals.Num())
		{
			FDecalRenderTargetManager RenderTargetManager(RHICmdList, Context.GetShaderPlatform(), CurrentStage);

			// Build a list of decals that need to be rendered for this view
			FTransientDecalRenderDataList SortedDecals;
			FDecalRendering::BuildVisibleDecalList(Scene, View, CurrentStage, SortedDecals);

			if (SortedDecals.Num() > 0)
			{
				SCOPED_DRAW_EVENTF(RHICmdList, DeferredDecalsInner, TEXT("DeferredDecalsInner %d/%d"), SortedDecals.Num(), Scene.Decals.Num());

				// optimization to have less state changes
				EDecalRasterizerState LastDecalRasterizerState = DRS_Undefined;
				FDecalDepthState LastDecalDepthState;
				int32 LastDecalBlendMode = -1;
				int32 LastDecalHasNormal = -1; // Decal state can change based on its normal property.(SM5)
			
				FDecalRenderingCommon::ERenderTargetMode LastRenderTargetMode = FDecalRenderingCommon::RTM_Unknown;
				const ERHIFeatureLevel::Type SMFeatureLevel = Context.GetFeatureLevel();

				SCOPED_DRAW_EVENT(RHICmdList, Decals);
				INC_DWORD_STAT_BY(STAT_Decals, SortedDecals.Num());

				for (int32 DecalIndex = 0, DecalCount = SortedDecals.Num(); DecalIndex < DecalCount; DecalIndex++)
				{
					const FTransientDecalRenderData& DecalData = SortedDecals[DecalIndex];
					const FDeferredDecalProxy& DecalProxy = *DecalData.DecalProxy;
					const FMatrix ComponentToWorldMatrix = DecalProxy.ComponentTrans.ToMatrixWithScale();
					const FMatrix FrustumComponentToClip = FDecalRendering::ComputeComponentToClipMatrix(View, ComponentToWorldMatrix);

					EDecalBlendMode DecalBlendMode = DecalData.DecalBlendMode;
					EDecalRenderStage LocalDecalStage = FDecalRenderingCommon::ComputeRenderStage(View.GetShaderPlatform(), DecalBlendMode);
					bool bStencilThisDecal = IsStencilOptimizationAvailable(LocalDecalStage);

					FDecalRenderingCommon::ERenderTargetMode CurrentRenderTargetMode = FDecalRenderingCommon::ComputeRenderTargetMode(View.GetShaderPlatform(), DecalBlendMode, DecalData.bHasNormal);

					if (bShaderComplexity)
					{
						CurrentRenderTargetMode = FDecalRenderingCommon::RTM_SceneColor;
						// we want additive blending for the ShaderComplexity mode
						DecalBlendMode = DBM_Emissive;
					}

					// Here we assume that GBuffer can only be WorldNormal since it is the only GBufferTarget handled correctly.
					if (RenderTargetManager.bGufferADirty && DecalData.MaterialResource->NeedsGBuffer())
					{ 
						RHICmdList.CopyToResolveTarget(SceneContext.GBufferA->GetRenderTargetItem().TargetableTexture, SceneContext.GBufferA->GetRenderTargetItem().TargetableTexture, true, FResolveParams());
						RenderTargetManager.TargetsToResolve[FDecalRenderTargetManager::GBufferAIndex] =  nullptr;
						RenderTargetManager.bGufferADirty = false;
					}

					// fewer rendertarget switches if possible
					if (CurrentRenderTargetMode != LastRenderTargetMode)
					{
						LastRenderTargetMode = CurrentRenderTargetMode;

						RenderTargetManager.SetRenderTargetMode(CurrentRenderTargetMode, DecalData.bHasNormal);
						Context.SetViewportAndCallRHI(Context.View.ViewRect);
					}

					bool bThisDecalUsesStencil = false;

					if (bStencilThisDecal && bStencilSizeThreshold)
					{
						// note this is after a SetStreamSource (in if CurrentRenderTargetMode != LastRenderTargetMode) call as it needs to get the VB input
						bThisDecalUsesStencil = RenderPreStencil(Context, ComponentToWorldMatrix, FrustumComponentToClip);

						LastDecalRasterizerState = DRS_Undefined;
						LastDecalDepthState = FDecalDepthState();
						LastDecalBlendMode = -1;
					}

					const bool bBlendStateChange = DecalBlendMode != LastDecalBlendMode;// Has decal mode changed.
					const bool bDecalNormalChanged = GSupportsSeparateRenderTargetBlendState && // has normal changed for SM5 stain/translucent decals?
						(DecalBlendMode == DBM_Translucent || DecalBlendMode == DBM_Stain) &&
						(int32)DecalData.bHasNormal != LastDecalHasNormal;

					// fewer blend state changes if possible
					if (bBlendStateChange || bDecalNormalChanged)
					{
						LastDecalBlendMode = DecalBlendMode;
						LastDecalHasNormal = (int32)DecalData.bHasNormal;

						SetDecalBlendState(RHICmdList, SMFeatureLevel, CurrentStage, (EDecalBlendMode)LastDecalBlendMode, DecalData.bHasNormal);
					}

					// todo
					const float ConservativeRadius = DecalData.ConservativeRadius;
					//			const int32 IsInsideDecal = ((FVector)View.ViewMatrices.ViewOrigin - ComponentToWorldMatrix.GetOrigin()).SizeSquared() < FMath::Square(ConservativeRadius * 1.05f + View.NearClippingDistance * 2.0f) + ( bThisDecalUsesStencil ) ? 2 : 0;
					const bool bInsideDecal = ((FVector)View.ViewMatrices.ViewOrigin - ComponentToWorldMatrix.GetOrigin()).SizeSquared() < FMath::Square(ConservativeRadius * 1.05f + View.NearClippingDistance * 2.0f);
					//			const bool bInsideDecal =  !(IsInsideDecal & 1);

					// update rasterizer state if needed
					{
						bool bReverseHanded = false;
						{
							// Account for the reversal of handedness caused by negative scale on the decal
							const auto& Scale3d = DecalProxy.ComponentTrans.GetScale3D();
							bReverseHanded =  Scale3d[0] * Scale3d[1] * Scale3d[2] < 0.f;
						}
						EDecalRasterizerState DecalRasterizerState = ComputeDecalRasterizerState(bInsideDecal, bReverseHanded, View);

						if (LastDecalRasterizerState != DecalRasterizerState)
						{
							LastDecalRasterizerState = DecalRasterizerState;
							SetDecalRasterizerState(DecalRasterizerState, RHICmdList);
						}
					}

					// update DepthStencil state if needed
					{
						FDecalDepthState DecalDepthState = ComputeDecalDepthState(LocalDecalStage, bInsideDecal, bThisDecalUsesStencil);

						if (LastDecalDepthState != DecalDepthState)
						{
							LastDecalDepthState = DecalDepthState;
							SetDecalDepthState(DecalDepthState, RHICmdList);
						}
					}

					FDecalRendering::SetShader(RHICmdList, View, DecalData, FrustumComponentToClip);

					RHICmdList.DrawIndexedPrimitive(GetUnitCubeIndexBuffer(), PT_TriangleList, 0, 0, 8, 0, ARRAY_COUNT(GCubeIndices) / 3, 1);
					RenderTargetManager.bGufferADirty |= (RenderTargetManager.TargetsToResolve[FDecalRenderTargetManager::GBufferAIndex] != nullptr);
				}

				// we don't modify stencil but if out input was having stencil for us (after base pass - we need to clear)
				// Clear stencil to 0, which is the assumed default by other passes
				RHICmdList.Clear(false, FLinearColor::White, false, (float)ERHIZBuffer::FarPlane, true, 0, FIntRect());
			}

			if (CurrentStage == DRS_BeforeBasePass)
			{
				// combine DBuffer RTWriteMasks; will end up in one texture we can load from in the base pass PS and decide whether to do the actual work or not
				RenderTargetManager.FlushMetaData();

				if (GSupportsRenderTargetWriteMask)
				{
					DecodeRTWriteMask(Context);
					bHasValidDBufferMask = true;
				}
			}

			RenderTargetManager.ResolveTargets();
		}

		if (CurrentStage == DRS_BeforeBasePass)
		{
			// before BasePass
			GRenderTargetPool.VisualizeTexture.SetCheckPoint(RHICmdList, SceneContext.DBufferA);
			GRenderTargetPool.VisualizeTexture.SetCheckPoint(RHICmdList, SceneContext.DBufferB);
			GRenderTargetPool.VisualizeTexture.SetCheckPoint(RHICmdList, SceneContext.DBufferC);
		}
	}

	if (CurrentStage == DRS_BeforeBasePass && !bHasValidDBufferMask)
	{
		// Return the DBufferMask to the render target pool.
		// FDeferredPixelShaderParameters will fall back to setting a white dummy mask texture.
		// This allows us to ignore the DBufferMask on frames without decals, without having to explicitly clear the texture.
		SceneContext.DBufferMask = nullptr;
	}
}

FPooledRenderTargetDesc FRCPassPostProcessDeferredDecals::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	// This pass creates it's own output so the compositing graph output isn't needed.
	FPooledRenderTargetDesc Ret;
	
	Ret.DebugName = TEXT("DeferredDecals");

	return Ret;
}
