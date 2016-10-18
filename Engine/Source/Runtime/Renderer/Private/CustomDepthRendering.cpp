// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	CustomDepthRendering.cpp: CustomDepth rendering implementation.
=============================================================================*/

#include "RendererPrivate.h"
#include "Engine.h"
#include "ScenePrivate.h"
#include "ScreenRendering.h"
#include "PostProcessing.h"
#include "RenderingCompositionGraph.h"
#include "SceneFilterRendering.h"
#include "SceneUtils.h"

/*-----------------------------------------------------------------------------
	FCustomDepthPrimSet
-----------------------------------------------------------------------------*/

bool FCustomDepthPrimSet::DrawPrims(FRHICommandListImmediate& RHICmdList, const FViewInfo& View, bool bWriteCustomStencilValues)
{
	bool bDirty=false;

	if(Prims.Num())
	{
		SCOPED_DRAW_EVENT(RHICmdList, CustomDepth);

		for (int32 PrimIdx = 0; PrimIdx < Prims.Num(); PrimIdx++)
		{
			FPrimitiveSceneProxy* PrimitiveSceneProxy = Prims[PrimIdx];
			const FPrimitiveSceneInfo* PrimitiveSceneInfo = PrimitiveSceneProxy->GetPrimitiveSceneInfo();

			if (View.PrimitiveVisibilityMap[PrimitiveSceneInfo->GetIndex()])
			{
				const FPrimitiveViewRelevance& ViewRelevance = View.PrimitiveViewRelevanceMap[PrimitiveSceneInfo->GetIndex()];

				FDepthDrawingPolicyFactory::ContextType Context(DDM_AllOpaque, false);

				if (bWriteCustomStencilValues)
				{
					const uint32 CustomDepthStencilValue = PrimitiveSceneProxy->GetCustomDepthStencilValue();
					RHICmdList.SetDepthStencilState(TStaticDepthStencilState<true, CF_DepthNearOrEqual, true, CF_Always, SO_Keep, SO_Keep, SO_Replace>::GetRHI(), CustomDepthStencilValue);
				}

				// Note: As for custom depth rendering the order doesn't matter we actually could iterate View.DynamicMeshElements without this indirection	
				{
					// range in View.DynamicMeshElements[]
					FInt32Range range = View.GetDynamicMeshElementRange(PrimitiveSceneInfo->GetIndex());

					for (int32 MeshBatchIndex = range.GetLowerBoundValue(); MeshBatchIndex < range.GetUpperBoundValue(); MeshBatchIndex++)
					{
						const FMeshBatchAndRelevance& MeshBatchAndRelevance = View.DynamicMeshElements[MeshBatchIndex];

						checkSlow(MeshBatchAndRelevance.PrimitiveSceneProxy == PrimitiveSceneInfo->Proxy);

						const FMeshBatch& MeshBatch = *MeshBatchAndRelevance.Mesh;
						FDepthDrawingPolicyFactory::DrawDynamicMesh(RHICmdList, View, Context, MeshBatch, false, true, MeshBatchAndRelevance.PrimitiveSceneProxy, MeshBatch.BatchHitProxyId);
					}
				}

				if (ViewRelevance.bStaticRelevance)
				{
					for (int32 StaticMeshIdx = 0; StaticMeshIdx < PrimitiveSceneInfo->StaticMeshes.Num(); StaticMeshIdx++)
					{
						const FStaticMesh& StaticMesh = PrimitiveSceneInfo->StaticMeshes[StaticMeshIdx];

						if (View.StaticMeshVisibilityMap[StaticMesh.Id])
						{
							const FMeshDrawingRenderState DrawRenderState(View.GetDitheredLODTransitionState(StaticMesh));
							bDirty |= FDepthDrawingPolicyFactory::DrawStaticMesh(
								RHICmdList, 
								View,
								Context,
								StaticMesh,
								StaticMesh.bRequiresPerElementVisibility ? View.StaticMeshBatchVisibility[StaticMesh.Id] : ((1ull << StaticMesh.Elements.Num()) - 1),
								true,
								DrawRenderState,
								PrimitiveSceneProxy,
								StaticMesh.BatchHitProxyId
								);
						}
					}
				}
			}
		}
	}

	return bDirty;
}
