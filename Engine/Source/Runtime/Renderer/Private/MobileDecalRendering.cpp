// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MobileDecalRendering.cpp: Decals for mobile renderer
=============================================================================*/

#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "SceneUtils.h"
#include "DecalRenderingShared.h"

void FMobileSceneRenderer::RenderDecals(FRHICommandListImmediate& RHICmdList)
{
	if (Scene->Decals.Num() == 0 || !IsMobileHDR())
	{
		return;
	}

	SCOPE_CYCLE_COUNTER(STAT_DecalsDrawTime);

	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);
	SceneContext.BeginRenderingSceneColor(RHICmdList, ESimpleRenderTargetMode::EExistingColorAndDepth, FExclusiveDepthStencil::DepthRead_StencilRead);
	
	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		const FViewInfo& View = Views[ViewIndex];
		
		// Build a list of decals that need to be rendered for this view
		FTransientDecalRenderDataList SortedDecals;
		FDecalRendering::BuildVisibleDecalList(*Scene, View, DRS_Mobile, SortedDecals);

		if (SortedDecals.Num())
		{
			SCOPED_DRAW_EVENT(RHICmdList, DeferredDecals);
			INC_DWORD_STAT_BY(STAT_Decals, SortedDecals.Num());
		
			RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0, View.ViewRect.Max.X, View.ViewRect.Max.Y, 1);
			RHICmdList.SetStreamSource(0, GetUnitCubeVertexBuffer(), sizeof(FVector4), 0);

			TOptional<EDecalBlendMode> LastDecalBlendMode;
			TOptional<bool> LastDecalDepthState;
			bool bEncodedHDR = IsMobileHDR32bpp() && !IsMobileHDRMosaic();
			if (bEncodedHDR)
			{
				RHICmdList.SetBlendState(TStaticBlendState<>::GetRHI());
			}
			for (int32 DecalIndex = 0, DecalCount = SortedDecals.Num(); DecalIndex < DecalCount; DecalIndex++)
			{
				const FTransientDecalRenderData& DecalData = SortedDecals[DecalIndex];
				const FDeferredDecalProxy& DecalProxy = *DecalData.DecalProxy;
				const FMatrix ComponentToWorldMatrix = DecalProxy.ComponentTrans.ToMatrixWithScale();
				const FMatrix FrustumComponentToClip = FDecalRendering::ComputeComponentToClipMatrix(View, ComponentToWorldMatrix);
						
				const float ConservativeRadius = DecalData.ConservativeRadius;
				const bool bInsideDecal = ((FVector)View.ViewMatrices.ViewOrigin - ComponentToWorldMatrix.GetOrigin()).SizeSquared() < FMath::Square(ConservativeRadius * 1.05f + View.NearClippingDistance * 2.0f);

				if (!LastDecalDepthState.IsSet() || LastDecalDepthState.GetValue() != bInsideDecal)
				{
					LastDecalDepthState = bInsideDecal;
					if (bInsideDecal)
					{
						RHICmdList.SetRasterizerState(View.bReverseCulling ? TStaticRasterizerState<FM_Solid, CM_CCW>::GetRHI() : TStaticRasterizerState<FM_Solid, CM_CW>::GetRHI());
						RHICmdList.SetDepthStencilState(TStaticDepthStencilState<
							false,CF_Always,
							true,CF_Equal,SO_Keep,SO_Keep,SO_Keep,
							false,CF_Always,SO_Keep,SO_Keep,SO_Keep,
							GET_STENCIL_BIT_MASK(RECEIVE_DECAL, 1),0x00>::GetRHI(), 0);
					}
					else
					{
						RHICmdList.SetRasterizerState(View.bReverseCulling ? TStaticRasterizerState<FM_Solid, CM_CW>::GetRHI() : TStaticRasterizerState<FM_Solid, CM_CCW>::GetRHI());
						RHICmdList.SetDepthStencilState(TStaticDepthStencilState<
							false,CF_DepthNearOrEqual,
							true,CF_Equal,SO_Keep,SO_Keep,SO_Keep,
							false,CF_Always,SO_Keep,SO_Keep,SO_Keep,
							GET_STENCIL_BIT_MASK(RECEIVE_DECAL, 1),0x00>::GetRHI(), 0);
					}
				}

				if (!bEncodedHDR && (!LastDecalBlendMode.IsSet() || LastDecalBlendMode.GetValue() != DecalData.DecalBlendMode))
				{
					LastDecalBlendMode = DecalData.DecalBlendMode;
					switch(DecalData.DecalBlendMode)
					{
					case DBM_Translucent:
						RHICmdList.SetBlendState(TStaticBlendState<CW_RGB, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha>::GetRHI());
						break;
					case DBM_Stain:
						// Modulate
						RHICmdList.SetBlendState(TStaticBlendState<CW_RGB, BO_Add, BF_DestColor, BF_InverseSourceAlpha>::GetRHI());
						break;
					case DBM_Emissive:
						// Additive
						RHICmdList.SetBlendState(TStaticBlendState<CW_RGB, BO_Add, BF_SourceAlpha, BF_One>::GetRHI());
						break;
					default:
						check(0);
					};
				}

				// Set shader params
				FDecalRendering::SetShader(RHICmdList, View, DecalData, FrustumComponentToClip);
			
				RHICmdList.DrawIndexedPrimitive(GetUnitCubeIndexBuffer(), PT_TriangleList, 0, 0, 8, 0, ARRAY_COUNT(GCubeIndices) / 3, 1);
			}
		}
	}
}
