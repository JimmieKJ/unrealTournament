// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

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

				FDepthDrawingPolicyFactory::ContextType Context(DDM_AllOccluders);

				if (bWriteCustomStencilValues)
				{
					const uint32 CustomDepthStencilValue = PrimitiveSceneProxy->GetCustomDepthStencilValue();
					RHICmdList.SetDepthStencilState(TStaticDepthStencilState<true, CF_DepthNearOrEqual, true, CF_Always, SO_Keep, SO_Keep, SO_Replace>::GetRHI(), CustomDepthStencilValue);
				}

				for (int32 MeshBatchIndex = 0; MeshBatchIndex < View.DynamicMeshElements.Num(); MeshBatchIndex++)
				{
					const FMeshBatchAndRelevance& MeshBatchAndRelevance = View.DynamicMeshElements[MeshBatchIndex];

					if (MeshBatchAndRelevance.PrimitiveSceneProxy == PrimitiveSceneProxy)
					{
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
							float DitherValue = View.GetDitheredLODTransitionValue(StaticMesh);
							bDirty |= FDepthDrawingPolicyFactory::DrawStaticMesh(
								RHICmdList, 
								View,
								FDepthDrawingPolicyFactory::ContextType(DDM_AllOccluders),
								StaticMesh,
								StaticMesh.Elements.Num() == 1 ? 1 : View.StaticMeshBatchVisibility[StaticMesh.Id],
								true,
								DitherValue,
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
