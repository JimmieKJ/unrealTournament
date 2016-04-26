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


static EDecalRasterizerState ComputeDecalRasterizerState(bool bInsideDecal, const FViewInfo& View)
{
	bool bClockwise = bInsideDecal;

	if(View.bReverseCulling)
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

static inline bool IsWrittingToGBufferA(FDecalRendering::ERenderTargetMode RenderTargetMode)
{
	return RenderTargetMode == FDecalRendering::RTM_SceneColorAndGBufferWithNormal ||  RenderTargetMode == FDecalRendering::RTM_SceneColorAndGBufferDepthWriteWithNormal || RenderTargetMode == FDecalRendering::RTM_GBufferNormal;
}

const TCHAR* GetStageName(EDecalRenderStage Stage)
{
	// could be implemented with enum reflections as well

	switch(Stage)
	{
		case DRS_BeforeBasePass: return TEXT("DRS_BeforeBasePass");
		case DRS_AfterBasePass: return TEXT("DRS_AfterBasePass");
		case DRS_BeforeLighting: return TEXT("DRS_BeforeLighting");
		case DRS_ForwardShading: return TEXT("DRS_ForwardShading");
	}
	return TEXT("<UNKNOWN>");
}

void FRCPassPostProcessDeferredDecals::Process(FRenderingCompositePassContext& Context)
{
	FRHICommandListImmediate& RHICmdList = Context.RHICmdList;
	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

	const bool bShaderComplexity = Context.View.Family->EngineShowFlags.ShaderComplexity;
	const bool bDBuffer = IsDBufferEnabled();
	const bool bStencilSizeThreshold = CVarStencilSizeThreshold.GetValueOnRenderThread() >= 0;

	SCOPED_DRAW_EVENTF(Context.RHICmdList, DeferredDecals, TEXT("DeferredDecals %s"), GetStageName(CurrentStage));

	enum EDecalResolveBufferIndex
	{
		SceneColorIndex,
		GBufferAIndex,
		GBufferBIndex,
		GBufferCIndex,
		GBufferEIndex,
		DBufferAIndex,
		DBufferBIndex,
		DBufferCIndex,
		ResolveBufferMax,
	};

	bool TargetsToTransitionWritable[ResolveBufferMax] = { true, true, true, true, true, true, true };
	FTextureRHIParamRef TargetsToResolve[ResolveBufferMax] = { nullptr };

	if(CurrentStage == DRS_BeforeBasePass)
	{
		// before BasePass, only if DBuffer is enabled

		check(bDBuffer);

		// DBuffer: Decal buffer
		FPooledRenderTargetDesc Desc(FPooledRenderTargetDesc::Create2DDesc(SceneContext.GBufferA->GetDesc().Extent, 
			PF_B8G8R8A8,
			FClearValueBinding::None,
			TexCreate_None, 
			TexCreate_ShaderResource | TexCreate_RenderTargetable,
			false));

		if(!SceneContext.DBufferA)
		{
			Desc.ClearValue = FClearValueBinding::Black;
			GRenderTargetPool.FindFreeElement(Context.RHICmdList, Desc, SceneContext.DBufferA, TEXT("DBufferA"));
		}

		if(!SceneContext.DBufferB)
		{
			Desc.ClearValue = FClearValueBinding(FLinearColor(128.0f / 255.0f, 128.0f / 255.0f, 128.0f / 255.0f, 1));
			GRenderTargetPool.FindFreeElement(Context.RHICmdList, Desc, SceneContext.DBufferB, TEXT("DBufferB"));
		}

		Desc.Format = PF_R8G8;

		if(!SceneContext.DBufferC)
		{
			Desc.ClearValue = FClearValueBinding(FLinearColor(0, 1, 0, 1));
			GRenderTargetPool.FindFreeElement(Context.RHICmdList, Desc, SceneContext.DBufferC, TEXT("DBufferC"));
		}

		// we assume views are non overlapping, then we need to clear only once in the beginning, otherwise we would need to set scissor rects
		// and don't get FastClear any more.
		bool bFirstView = Context.View.Family->Views[0] == &Context.View;

		if(bFirstView)
		{
			SCOPED_DRAW_EVENT(RHICmdList, DBufferClear);

			
			FRHIRenderTargetView RenderTargets[3];
			RenderTargets[0] = FRHIRenderTargetView(SceneContext.DBufferA->GetRenderTargetItem().TargetableTexture, 0, -1, ERenderTargetLoadAction::EClear, ERenderTargetStoreAction::EStore);
			RenderTargets[1] = FRHIRenderTargetView(SceneContext.DBufferB->GetRenderTargetItem().TargetableTexture, 0, -1, ERenderTargetLoadAction::EClear, ERenderTargetStoreAction::EStore);
			RenderTargets[2] = FRHIRenderTargetView(SceneContext.DBufferC->GetRenderTargetItem().TargetableTexture, 0, -1, ERenderTargetLoadAction::EClear, ERenderTargetStoreAction::EStore);

			FRHIDepthRenderTargetView DepthView(SceneContext.GetSceneDepthSurface(), ERenderTargetLoadAction::ELoad, ERenderTargetStoreAction::ENoAction, ERenderTargetLoadAction::ELoad, ERenderTargetStoreAction::ENoAction, FExclusiveDepthStencil(FExclusiveDepthStencil::DepthRead_StencilWrite));

			FRHISetRenderTargetsInfo Info(3, RenderTargets, DepthView);
			RHICmdList.SetRenderTargetsAndClear(Info);

			TargetsToResolve[DBufferAIndex] = SceneContext.DBufferA->GetRenderTargetItem().TargetableTexture;
			TargetsToResolve[DBufferBIndex] = SceneContext.DBufferB->GetRenderTargetItem().TargetableTexture;
			TargetsToResolve[DBufferCIndex] = SceneContext.DBufferC->GetRenderTargetItem().TargetableTexture;
		}
	}

	// this cast is safe as only the dedicated server implements this differently and this pass should not be executed on the dedicated server
	const FViewInfo& View = Context.View;
	const FSceneViewFamily& ViewFamily = *(View.Family);
	FScene& Scene = *(FScene*)ViewFamily.Scene;

	//don't early return.  Resolves must be run for fast clears to work.
	bool bRenderDecal = Scene.Decals.Num() && ViewFamily.EngineShowFlags.Decals;
	if (bRenderDecal)
	{
		// Build a list of decals that need to be rendered for this view
		FTransientDecalRenderDataList SortedDecals;
		FDecalRendering::BuildVisibleDecalList(Scene, View, CurrentStage, SortedDecals);

		if (SortedDecals.Num() > 0)
		{
			SCOPED_DRAW_EVENTF(Context.RHICmdList, DeferredDecalsInner, TEXT("DeferredDecalsInner %d/%d"), SortedDecals.Num(), Scene.Decals.Num());

			FIntRect SrcRect = View.ViewRect;
			FIntRect DestRect = View.ViewRect;

			// optimization to have less state changes
			EDecalRasterizerState LastDecalRasterizerState = DRS_Undefined;
			FDecalDepthState LastDecalDepthState;
			int32 LastDecalBlendMode = -1;
			int32 LastDecalHasNormal = -1; // Decal state can change based on its normal property.(SM5)
			bool bGufferADirty = CurrentStage == DRS_AfterBasePass; // Normal buffer is already dirty at this point and needs resolve before being read from (irrelevant for DBuffer).

			FDecalRendering::ERenderTargetMode LastRenderTargetMode = FDecalRendering::RTM_Unknown;
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
				EDecalRenderStage LocalDecalStage = FDecalRendering::ComputeRenderStage(View.GetShaderPlatform(), DecalBlendMode);
				bool bStencilThisDecal = IsStencilOptimizationAvailable(LocalDecalStage);

				FDecalRendering::ERenderTargetMode CurrentRenderTargetMode = FDecalRendering::ComputeRenderTargetMode(View.GetShaderPlatform(), DecalBlendMode, DecalData.bHasNormal);

				if (bShaderComplexity)
				{
					CurrentRenderTargetMode = FDecalRendering::RTM_SceneColor;
					// we want additive blending for the ShaderComplexity mode
					DecalBlendMode = DBM_Emissive;
				}

				// Here we assume that GBuffer can only be WorldNormal since it is the only GBufferTarget handled correctly.
				if (bGufferADirty && DecalData.MaterialResource->NeedsGBuffer())
				{ 
					RHICmdList.CopyToResolveTarget(SceneContext.GBufferA->GetRenderTargetItem().TargetableTexture, SceneContext.GBufferA->GetRenderTargetItem().TargetableTexture, true, FResolveParams());
					TargetsToResolve[GBufferAIndex] =  nullptr;
					bGufferADirty = false;
				}

				// fewer rendertarget switches if possible
				if (CurrentRenderTargetMode != LastRenderTargetMode)
				{
					LastRenderTargetMode = CurrentRenderTargetMode;

					// If GBuffrA was resolved for read, and we want to write to it again.
					if (!bGufferADirty && IsWrittingToGBufferA(CurrentRenderTargetMode)) 
					{
						// This is required to be compliant with RHISetRenderTargets resource transition code : const bool bAccessValid = !bReadable || LastFrameWritten != CurrentFrame;
						// If the normal buffer was resolved as a texture before, then bReadable && LastFrameWritten == CurrentFrame, and an error msg will be triggered. 
						// Which is not needed here since no more read will be done at this point (at least not before any other CopyToResolvedTarget).
						RHICmdList.TransitionResource(EResourceTransitionAccess::EWritable, SceneContext.GBufferA->GetRenderTargetItem().TargetableTexture);
					}

					switch (CurrentRenderTargetMode)
					{
						case FDecalRendering::RTM_SceneColorAndGBufferWithNormal:
						case FDecalRendering::RTM_SceneColorAndGBufferNoNormal:
							TargetsToResolve[SceneColorIndex] = SceneContext.GetSceneColor()->GetRenderTargetItem().TargetableTexture;
							TargetsToResolve[GBufferAIndex] = DecalData.bHasNormal ? SceneContext.GBufferA->GetRenderTargetItem().TargetableTexture : (PLATFORM_MAC ? SceneContext.GBufferB->GetRenderTargetItem().TargetableTexture : nullptr); // @todo Workaround a Mac NV/Intel graphics driver bug that requires we pointlessly bind into RT1 even though we don't write to it, otherwise the writes to RT2 and RT3 go haywire. This isn't really possible to fix lower down the stack.
							TargetsToResolve[GBufferBIndex] = SceneContext.GBufferB->GetRenderTargetItem().TargetableTexture;
							TargetsToResolve[GBufferCIndex] = SceneContext.GBufferC->GetRenderTargetItem().TargetableTexture;
							SetRenderTargets(RHICmdList, 4, TargetsToResolve, SceneContext.GetSceneDepthSurface(), ESimpleRenderTargetMode::EExistingColorAndDepth, FExclusiveDepthStencil::DepthRead_StencilWrite, TargetsToTransitionWritable[CurrentRenderTargetMode]);							
							break;

						case FDecalRendering::RTM_SceneColorAndGBufferDepthWriteWithNormal:
						case FDecalRendering::RTM_SceneColorAndGBufferDepthWriteNoNormal:
							TargetsToResolve[SceneColorIndex] = SceneContext.GetSceneColor()->GetRenderTargetItem().TargetableTexture;
							TargetsToResolve[GBufferAIndex] = DecalData.bHasNormal ? SceneContext.GBufferA->GetRenderTargetItem().TargetableTexture : (PLATFORM_MAC ? SceneContext.GBufferB->GetRenderTargetItem().TargetableTexture : nullptr); // @todo Workaround a Mac NV/Intel graphics driver bug that requires we pointlessly bind into RT1 even though we don't write to it, otherwise the writes to RT2 and RT3 go haywire. This isn't really possible to fix lower down the stack.
							TargetsToResolve[GBufferBIndex] = SceneContext.GBufferB->GetRenderTargetItem().TargetableTexture;
							TargetsToResolve[GBufferCIndex] = SceneContext.GBufferC->GetRenderTargetItem().TargetableTexture;
							TargetsToResolve[GBufferEIndex] = SceneContext.GBufferE->GetRenderTargetItem().TargetableTexture;
							SetRenderTargets(RHICmdList, 5, TargetsToResolve, SceneContext.GetSceneDepthSurface(), ESimpleRenderTargetMode::EExistingColorAndDepth, FExclusiveDepthStencil::DepthWrite_StencilWrite, TargetsToTransitionWritable[CurrentRenderTargetMode]);
							break;

						case FDecalRendering::RTM_GBufferNormal:
							TargetsToResolve[GBufferAIndex] = SceneContext.GBufferA->GetRenderTargetItem().TargetableTexture;
							SetRenderTarget(RHICmdList, TargetsToResolve[GBufferAIndex], SceneContext.GetSceneDepthSurface(), ESimpleRenderTargetMode::EExistingColorAndDepth, FExclusiveDepthStencil::DepthRead_StencilWrite, TargetsToTransitionWritable[CurrentRenderTargetMode]);
							break;

						case FDecalRendering::RTM_SceneColor:
							TargetsToResolve[SceneColorIndex] = SceneContext.GetSceneColor()->GetRenderTargetItem().TargetableTexture;
							SetRenderTarget(RHICmdList, TargetsToResolve[SceneColorIndex], SceneContext.GetSceneDepthSurface(), ESimpleRenderTargetMode::EExistingColorAndDepth, FExclusiveDepthStencil::DepthRead_StencilWrite, TargetsToTransitionWritable[CurrentRenderTargetMode]);
							break;

						case FDecalRendering::RTM_DBuffer:
							TargetsToResolve[DBufferAIndex] = SceneContext.DBufferA->GetRenderTargetItem().TargetableTexture;
							TargetsToResolve[DBufferBIndex] = SceneContext.DBufferB->GetRenderTargetItem().TargetableTexture;
							TargetsToResolve[DBufferCIndex] = SceneContext.DBufferC->GetRenderTargetItem().TargetableTexture;
							SetRenderTargets(RHICmdList, 3, &TargetsToResolve[DBufferAIndex], SceneContext.GetSceneDepthSurface(), ESimpleRenderTargetMode::EExistingColorAndDepth, FExclusiveDepthStencil::DepthRead_StencilWrite, TargetsToTransitionWritable[CurrentRenderTargetMode]);
							break;

						default:
							check(0);
							break;
					}
					TargetsToTransitionWritable[CurrentRenderTargetMode] = false;
					Context.SetViewportAndCallRHI(DestRect);

					// we need to reset the stream source after any call to SetRenderTarget (at least for Metal, which doesn't queue up VB assignments)
					RHICmdList.SetStreamSource(0, GetUnitCubeVertexBuffer(), sizeof(FVector4), 0);
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
					EDecalRasterizerState DecalRasterizerState = ComputeDecalRasterizerState(bInsideDecal, View);

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

				bGufferADirty |= (TargetsToResolve[GBufferAIndex] != nullptr);
			}

			// If GBuffer A is dirty, mark it as needing resolve since the content of TargetsToResolve[GBufferAIndex] could have been nullified by modes like RTM_SceneColorAndGBufferNoNormal
			if (bGufferADirty)
			{
				TargetsToResolve[GBufferAIndex] = SceneContext.GBufferA->GetRenderTargetItem().TargetableTexture;
			}

			// we don't modify stencil but if out input was having stencil for us (after base pass - we need to clear)
			// Clear stencil to 0, which is the assumed default by other passes
			RHICmdList.Clear(false, FLinearColor::White, false, (float)ERHIZBuffer::FarPlane, true, 0, FIntRect());

			if (CurrentStage == DRS_BeforeBasePass)
			{
				// before BasePass
				GRenderTargetPool.VisualizeTexture.SetCheckPoint(RHICmdList, SceneContext.DBufferA);
				GRenderTargetPool.VisualizeTexture.SetCheckPoint(RHICmdList, SceneContext.DBufferB);
				GRenderTargetPool.VisualizeTexture.SetCheckPoint(RHICmdList, SceneContext.DBufferC);
			}
		}
	}

	// resolve the targets we wrote to.
	FResolveParams ResolveParams;
	for (int32 i = 0; i < ResolveBufferMax; ++i)
	{
		if (TargetsToResolve[i])
		{
			RHICmdList.CopyToResolveTarget(TargetsToResolve[i], TargetsToResolve[i], true, ResolveParams);
		}
	}		
}

FPooledRenderTargetDesc FRCPassPostProcessDeferredDecals::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	// This pass creates it's own output so the compositing graph output isn't needed.
	FPooledRenderTargetDesc Ret;
	
	Ret.DebugName = TEXT("DeferredDecals");

	return Ret;
}
