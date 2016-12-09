// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	CustomDepthRendering.cpp: CustomDepth rendering implementation.
=============================================================================*/

#include "CustomDepthRendering.h"
#include "SceneUtils.h"
#include "DrawingPolicy.h"
#include "DepthRendering.h"
#include "SceneRendering.h"

/*-----------------------------------------------------------------------------
	FCustomDepthPrimSet
-----------------------------------------------------------------------------*/

bool FCustomDepthPrimSet::DrawPrims(FRHICommandListImmediate& RHICmdList, const FViewInfo& View, FDrawingPolicyRenderState& DrawRenderState, bool bWriteCustomStencilValues)
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

				FDrawingPolicyRenderState DrawRenderStateLocal(&RHICmdList, DrawRenderState);
				if (bWriteCustomStencilValues)
				{
					DrawRenderStateLocal.SetDepthStencilState(RHICmdList, TStaticDepthStencilState<true, CF_DepthNearOrEqual, true, CF_Always, SO_Keep, SO_Keep, SO_Replace>::GetRHI(), PrimitiveSceneProxy->GetCustomDepthStencilValue());
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

						FDepthDrawingPolicyFactory::DrawDynamicMesh(RHICmdList, View, Context, MeshBatch, true, DrawRenderStateLocal, MeshBatchAndRelevance.PrimitiveSceneProxy, MeshBatch.BatchHitProxyId);
					}
				}

				if (ViewRelevance.bStaticRelevance)
				{
					for (int32 StaticMeshIdx = 0; StaticMeshIdx < PrimitiveSceneInfo->StaticMeshes.Num(); StaticMeshIdx++)
					{
						const FStaticMesh& StaticMesh = PrimitiveSceneInfo->StaticMeshes[StaticMeshIdx];

						if (View.StaticMeshVisibilityMap[StaticMesh.Id])
						{
							FDrawingPolicyRenderState DrawRenderStateLocal2(&RHICmdList, DrawRenderStateLocal);
							FMeshDrawingPolicy::OnlyApplyDitheredLODTransitionState(RHICmdList, DrawRenderStateLocal2, View, StaticMesh, false);

							bDirty |= FDepthDrawingPolicyFactory::DrawStaticMesh(
								RHICmdList, 
								View,
								Context,
								StaticMesh,
								StaticMesh.bRequiresPerElementVisibility ? View.StaticMeshBatchVisibility[StaticMesh.Id] : ((1ull << StaticMesh.Elements.Num()) - 1),
								true,
								DrawRenderStateLocal2,
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
