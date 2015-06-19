// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	StaticMeshDrawList.inl: Static mesh draw list implementation.
=============================================================================*/

#ifndef __STATICMESHDRAWLIST_INL__
#define __STATICMESHDRAWLIST_INL__

#include "RHICommandList.h"
#include "EngineStats.h"

// Expensive
#define PER_MESH_DRAW_STATS 0

template<typename DrawingPolicyType>
void TStaticMeshDrawList<DrawingPolicyType>::FElementHandle::Remove()
{
	// Make a copy of this handle's variables on the stack, since the call to Elements.RemoveSwap deletes the handle.
	TStaticMeshDrawList* const LocalDrawList = StaticMeshDrawList;
	FDrawingPolicyLink* const LocalDrawingPolicyLink = &LocalDrawList->DrawingPolicySet[SetId];
	const int32 LocalElementIndex = ElementIndex;

	checkSlow(LocalDrawingPolicyLink->SetId == SetId);

	// Unlink the mesh from this draw list.
	LocalDrawingPolicyLink->Elements[ElementIndex].Mesh->UnlinkDrawList(this);
	LocalDrawingPolicyLink->Elements[ElementIndex].Mesh = NULL;

	checkSlow(LocalDrawingPolicyLink->Elements.Num() == LocalDrawingPolicyLink->CompactElements.Num());

	// Remove this element from the drawing policy's element list.
	const uint32 LastDrawingPolicySize = LocalDrawingPolicyLink->GetSizeBytes();

	LocalDrawingPolicyLink->Elements.RemoveAtSwap(LocalElementIndex);
	LocalDrawingPolicyLink->CompactElements.RemoveAtSwap(LocalElementIndex);
	
	const uint32 CurrentDrawingPolicySize = LocalDrawingPolicyLink->GetSizeBytes();
	const uint32 DrawingPolicySizeDiff = LastDrawingPolicySize - CurrentDrawingPolicySize;

	LocalDrawList->TotalBytesUsed -= DrawingPolicySizeDiff;


	if (LocalElementIndex < LocalDrawingPolicyLink->Elements.Num())
	{
		// Fixup the element that was moved into the hole created by the removed element.
		LocalDrawingPolicyLink->Elements[LocalElementIndex].Handle->ElementIndex = LocalElementIndex;
	}

	// If this was the last element for the drawing policy, remove the drawing policy from the draw list.
	if(!LocalDrawingPolicyLink->Elements.Num())
	{
		LocalDrawList->TotalBytesUsed -= LocalDrawingPolicyLink->GetSizeBytes();

		LocalDrawList->OrderedDrawingPolicies.RemoveSingle(LocalDrawingPolicyLink->SetId);
		LocalDrawList->DrawingPolicySet.Remove(LocalDrawingPolicyLink->SetId);
	}
}

template<typename DrawingPolicyType>
void TStaticMeshDrawList<DrawingPolicyType>::DrawElement(
	FRHICommandList& RHICmdList,
	const FViewInfo& View,
	const typename DrawingPolicyType::ContextDataType PolicyContext,
	const FElement& Element,
	uint64 BatchElementMask,
	FDrawingPolicyLink* DrawingPolicyLink,
	bool& bDrawnShared
	)
{
#if PER_MESH_DRAW_STATS
	FScopeCycleCounter Context(Element.Mesh->PrimitiveSceneInfo->Proxy->GetStatId());
#endif // #if PER_MESH_DRAW_STATS

	if (!bDrawnShared)
	{
		if (ensure(IsValidRef(DrawingPolicyLink->BoundShaderState)))
		{
			RHICmdList.SetBoundShaderState(DrawingPolicyLink->BoundShaderState);
		}
		else
		{
			RHICmdList.BuildAndSetLocalBoundShaderState(DrawingPolicyLink->DrawingPolicy.GetBoundShaderStateInput(View.GetFeatureLevel()));
		}
		DrawingPolicyLink->DrawingPolicy.SetSharedState(RHICmdList, &View, PolicyContext);
		bDrawnShared = true;
	}
	
	uint32 BackFaceEnd = DrawingPolicyLink->DrawingPolicy.NeedsBackfacePass() ? 2 : 1;

	int32 BatchElementIndex = 0;
	do
	{
		if(BatchElementMask & 1)
		{
			for (uint32 BackFace = 0; BackFace < BackFaceEnd; ++BackFace)
			{
				INC_DWORD_STAT(STAT_StaticDrawListMeshDrawCalls);

				DrawingPolicyLink->DrawingPolicy.SetMeshRenderState(
					RHICmdList, 
					View,
					Element.Mesh->PrimitiveSceneInfo->Proxy,
					*Element.Mesh,
					BatchElementIndex,
					!!BackFace,
					Element.PolicyData,
					PolicyContext
					);
				DrawingPolicyLink->DrawingPolicy.DrawMesh(RHICmdList, *Element.Mesh,BatchElementIndex);
			}
		}

		BatchElementMask >>= 1;
		BatchElementIndex++;
	} while(BatchElementMask);
}

template<typename DrawingPolicyType>
void TStaticMeshDrawList<DrawingPolicyType>::AddMesh(
	FStaticMesh* Mesh,
	const ElementPolicyDataType& PolicyData,
	const DrawingPolicyType& InDrawingPolicy,
	ERHIFeatureLevel::Type InFeatureLevel
	)
{
	// Check for an existing drawing policy matching the mesh's drawing policy.
	FDrawingPolicyLink* DrawingPolicyLink = DrawingPolicySet.Find(InDrawingPolicy);
	if(!DrawingPolicyLink)
	{
		// If no existing drawing policy matches the mesh, create a new one.
		const FSetElementId DrawingPolicyLinkId = DrawingPolicySet.Add(FDrawingPolicyLink(this,InDrawingPolicy, InFeatureLevel));

		DrawingPolicyLink = &DrawingPolicySet[DrawingPolicyLinkId];
		DrawingPolicyLink->SetId = DrawingPolicyLinkId;

		TotalBytesUsed += DrawingPolicyLink->GetSizeBytes();

		// Insert the drawing policy into the ordered drawing policy list.
		int32 MinIndex = 0;
		int32 MaxIndex = OrderedDrawingPolicies.Num() - 1;
		while(MinIndex < MaxIndex)
		{
			int32 PivotIndex = (MaxIndex + MinIndex) / 2;
			int32 CompareResult = CompareDrawingPolicy(DrawingPolicySet[OrderedDrawingPolicies[PivotIndex]].DrawingPolicy,DrawingPolicyLink->DrawingPolicy);
			if(CompareResult < 0)
			{
				MinIndex = PivotIndex + 1;
			}
			else if(CompareResult > 0)
			{
				MaxIndex = PivotIndex;
			}
			else
			{
				MinIndex = MaxIndex = PivotIndex;
			}
		};
		check(MinIndex >= MaxIndex);
		OrderedDrawingPolicies.Insert(DrawingPolicyLinkId,MinIndex);
	}

	const int32 ElementIndex = DrawingPolicyLink->Elements.Num();
	const SIZE_T PreviousElementsSize = DrawingPolicyLink->Elements.GetAllocatedSize();
	const SIZE_T PreviousCompactElementsSize = DrawingPolicyLink->CompactElements.GetAllocatedSize();
	FElement* Element = new(DrawingPolicyLink->Elements) FElement(Mesh, PolicyData, this, DrawingPolicyLink->SetId, ElementIndex);
	new(DrawingPolicyLink->CompactElements) FElementCompact(Mesh->Id);
	TotalBytesUsed += DrawingPolicyLink->Elements.GetAllocatedSize() - PreviousElementsSize + DrawingPolicyLink->CompactElements.GetAllocatedSize() - PreviousCompactElementsSize;
	Mesh->LinkDrawList(Element->Handle);
}

template<typename DrawingPolicyType>
TStaticMeshDrawList<DrawingPolicyType>::TStaticMeshDrawList()
{
	if(IsInRenderingThread())
	{
		InitResource();
	}
	else
	{
		BeginInitResource(this);
	}
}

template<typename DrawingPolicyType>
TStaticMeshDrawList<DrawingPolicyType>::~TStaticMeshDrawList()
{
	if(IsInRenderingThread())
	{
		ReleaseResource();
	}
	else
	{
		BeginReleaseResource(this);
	}

#if STATS
	for (typename TArray<FSetElementId>::TConstIterator PolicyIt(OrderedDrawingPolicies); PolicyIt; ++PolicyIt)
	{
		const FDrawingPolicyLink* DrawingPolicyLink = &DrawingPolicySet[*PolicyIt];
		TotalBytesUsed -= DrawingPolicyLink->GetSizeBytes();
	}
#endif
}

template<typename DrawingPolicyType>
void TStaticMeshDrawList<DrawingPolicyType>::ReleaseRHI()
{
	for(typename TDrawingPolicySet::TIterator DrawingPolicyIt(DrawingPolicySet);DrawingPolicyIt;++DrawingPolicyIt)
	{
		DrawingPolicyIt->ReleaseBoundShaderState();
	}
}

template<typename DrawingPolicyType>
bool TStaticMeshDrawList<DrawingPolicyType>::DrawVisible(
	const FViewInfo& View,
	const typename DrawingPolicyType::ContextDataType PolicyContext,
	const TBitArray<SceneRenderingBitArrayAllocator>& StaticMeshVisibilityMap
	)
{
	bool bDirty = false;
	for(typename TArray<FSetElementId>::TConstIterator PolicyIt(OrderedDrawingPolicies); PolicyIt; ++PolicyIt)
	{
		FDrawingPolicyLink* DrawingPolicyLink = &DrawingPolicySet(*PolicyIt);
		bool bDrawnShared = false;
		FPlatformMisc::Prefetch(DrawingPolicyLink->CompactElements.GetTypedData());
		const int32 NumElements = DrawingPolicyLink->Elements.Num();
		FPlatformMisc::Prefetch(&DrawingPolicyLink->CompactElements.GetTypedData()->MeshId);
		const FElementCompact* CompactElementPtr = DrawingPolicyLink->CompactElements.GetTypedData();
		for(int32 ElementIndex = 0; ElementIndex < NumElements; ElementIndex++, CompactElementPtr++)
		{
			if(StaticMeshVisibilityMap.AccessCorrespondingBit(FRelativeBitReference(CompactElementPtr->MeshId)))
			{
				const FElement& Element = DrawingPolicyLink->Elements[ElementIndex];
				INC_DWORD_STAT_BY(STAT_StaticMeshTriangles,Element.Mesh->GetNumPrimitives());
				// Avoid the virtual call looking up batch visibility if there is only one element.
				uint32 BatchElementMask = Element.Mesh->Elements.Num() == 1 ? 1 : Element.Mesh->VertexFactory->GetStaticBatchElementVisibility(View,Element.Mesh);
				DrawElement(View, PolicyContext, Element, BatchElementMask, DrawingPolicyLink, bDrawnShared);
				bDirty = true;
			}
		}
	}
	return bDirty;
}

template<typename DrawingPolicyType>
bool TStaticMeshDrawList<DrawingPolicyType>::DrawVisibleInner(
	FRHICommandList& RHICmdList,
	const FViewInfo& View,
	const typename DrawingPolicyType::ContextDataType PolicyContext,
	const TBitArray<SceneRenderingBitArrayAllocator>& StaticMeshVisibilityMap,
	const TArray<uint64, SceneRenderingAllocator>& BatchVisibilityArray,
	int32 FirstPolicy, int32 LastPolicy
	)
{
	bool bDirty = false;
	for (int32 Index = FirstPolicy; Index <= LastPolicy; Index++)
	{
		FDrawingPolicyLink* DrawingPolicyLink = &DrawingPolicySet[OrderedDrawingPolicies[Index]];
		bool bDrawnShared = false;
		FPlatformMisc::Prefetch(DrawingPolicyLink->CompactElements.GetData());
		const int32 NumElements = DrawingPolicyLink->Elements.Num();
		FPlatformMisc::Prefetch(&DrawingPolicyLink->CompactElements.GetData()->MeshId);
		const FElementCompact* CompactElementPtr = DrawingPolicyLink->CompactElements.GetData();
		for (int32 ElementIndex = 0; ElementIndex < NumElements; ElementIndex++, CompactElementPtr++)
		{
			if (StaticMeshVisibilityMap.AccessCorrespondingBit(FRelativeBitReference(CompactElementPtr->MeshId)))
			{
				const FElement& Element = DrawingPolicyLink->Elements[ElementIndex];
				INC_DWORD_STAT_BY(STAT_StaticMeshTriangles, Element.Mesh->GetNumPrimitives());
				// Avoid the cache miss looking up batch visibility if there is only one element.
				uint64 BatchElementMask = Element.Mesh->Elements.Num() == 1 ? 1 : BatchVisibilityArray[Element.Mesh->Id];
				DrawElement(RHICmdList, View, PolicyContext, Element, BatchElementMask, DrawingPolicyLink, bDrawnShared);
				bDirty = true;
			}
		}
	}
	return bDirty;
}

template<typename DrawingPolicyType>
bool TStaticMeshDrawList<DrawingPolicyType>::DrawVisible(
	FRHICommandList& RHICmdList,
	const FViewInfo& View,
	const typename DrawingPolicyType::ContextDataType PolicyContext,
	const TBitArray<SceneRenderingBitArrayAllocator>& StaticMeshVisibilityMap,
	const TArray<uint64,SceneRenderingAllocator>& BatchVisibilityArray
	)
{
	return DrawVisibleInner(RHICmdList, View, PolicyContext, StaticMeshVisibilityMap, BatchVisibilityArray, 0, OrderedDrawingPolicies.Num() - 1);
}


template<typename DrawingPolicyType>
class FDrawVisibleAnyThreadTask
{
	TStaticMeshDrawList<DrawingPolicyType>& Caller;
	FRHICommandList& RHICmdList;
	const FViewInfo& View;
	const typename DrawingPolicyType::ContextDataType PolicyContext;
	const TBitArray<SceneRenderingBitArrayAllocator>& StaticMeshVisibilityMap;
	const TArray<uint64, SceneRenderingAllocator>& BatchVisibilityArray;

	const int32 FirstPolicy;
	const int32 LastPolicy;

	bool& OutDirty;

public:

	FDrawVisibleAnyThreadTask(
		TStaticMeshDrawList<DrawingPolicyType>& InCaller,
		FRHICommandList& InRHICmdList,
		const FViewInfo& InView,
		const typename DrawingPolicyType::ContextDataType& InPolicyContext,
		const TBitArray<SceneRenderingBitArrayAllocator>& InStaticMeshVisibilityMap,
		const TArray<uint64, SceneRenderingAllocator>& InBatchVisibilityArray,
		int32 InFirstPolicy,
		int32 InLastPolicy,
		bool& InOutDirty
		)
		: Caller(InCaller)
		, RHICmdList(InRHICmdList)
		, View(InView)
		, PolicyContext(InPolicyContext)
		, StaticMeshVisibilityMap(InStaticMeshVisibilityMap)
		, BatchVisibilityArray(InBatchVisibilityArray)
		, FirstPolicy(InFirstPolicy)
		, LastPolicy(InLastPolicy)
		, OutDirty(InOutDirty)
	{
	}

	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FDrawVisibleAnyThreadTask, STATGROUP_TaskGraphTasks);
	}

	ENamedThreads::Type GetDesiredThread()
	{
		return ENamedThreads::AnyThread;
	}

	static ESubsequentsMode::Type GetSubsequentsMode() { return ESubsequentsMode::TrackSubsequents; }

	void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
	{
		if (this->Caller.DrawVisibleInner(this->RHICmdList, this->View, this->PolicyContext, this->StaticMeshVisibilityMap, this->BatchVisibilityArray, this->FirstPolicy, this->LastPolicy))
		{
			this->OutDirty = true;
		}
		this->RHICmdList.HandleRTThreadTaskCompletion(MyCompletionGraphEvent);
	}
};

template<typename DrawingPolicyType>
void TStaticMeshDrawList<DrawingPolicyType>::DrawVisibleParallel(
	const typename DrawingPolicyType::ContextDataType PolicyContext,
	const TBitArray<SceneRenderingBitArrayAllocator>& StaticMeshVisibilityMap,
	const TArray<uint64, SceneRenderingAllocator>& BatchVisibilityArray,
	FParallelCommandListSet& ParallelCommandListSet
	)
{

	int32 EffectiveThreads = FMath::Min<int32>(OrderedDrawingPolicies.Num(), ParallelCommandListSet.Width);

	int32 Start = 0;
	if (EffectiveThreads)
	{

		int32 NumPer = OrderedDrawingPolicies.Num() / EffectiveThreads;
		int32 Extra = OrderedDrawingPolicies.Num() - NumPer * EffectiveThreads;


		for (int32 ThreadIndex = 0; ThreadIndex < EffectiveThreads; ThreadIndex++)
		{
			int32 Last = Start + (NumPer - 1) + (ThreadIndex < Extra);
			check(Last >= Start);

			{
				FRHICommandList* CmdList = ParallelCommandListSet.NewParallelCommandList();

				FGraphEventRef AnyThreadCompletionEvent = TGraphTask<FDrawVisibleAnyThreadTask<DrawingPolicyType> >::CreateTask(nullptr, ENamedThreads::RenderThread)
					.ConstructAndDispatchWhenReady(*this, *CmdList, ParallelCommandListSet.View, PolicyContext, StaticMeshVisibilityMap, BatchVisibilityArray, Start, Last, ParallelCommandListSet.OutDirty);

				ParallelCommandListSet.AddParallelCommandList(CmdList, AnyThreadCompletionEvent);
			}

			Start = Last + 1;
		}
	}
	check(Start == OrderedDrawingPolicies.Num());
}

template<typename DrawingPolicyType>
int32 TStaticMeshDrawList<DrawingPolicyType>::DrawVisibleFrontToBack(
	FRHICommandList& RHICmdList,
	const FViewInfo& View,
	const typename DrawingPolicyType::ContextDataType PolicyContext,
	const TBitArray<SceneRenderingBitArrayAllocator>& StaticMeshVisibilityMap,
	const TArray<uint64,SceneRenderingAllocator>& BatchVisibilityArray,
	int32 MaxToDraw
	)
{
	int32 NumDraws = 0;
	TArray<FDrawListSortKey,SceneRenderingAllocator> SortKeys;
	const FVector ViewLocation = View.ViewLocation;
	SortKeys.Reserve(64);

	for(typename TArray<FSetElementId>::TConstIterator PolicyIt(OrderedDrawingPolicies); PolicyIt; ++PolicyIt)
	{
		FDrawingPolicyLink* DrawingPolicyLink = &DrawingPolicySet[*PolicyIt];
		FVector DrawingPolicyCenter = DrawingPolicyLink->CachedBoundingSphere.Center;
		FPlatformMisc::Prefetch(DrawingPolicyLink->CompactElements.GetData());
		const int32 NumElements = DrawingPolicyLink->Elements.Num();
		FPlatformMisc::Prefetch(&DrawingPolicyLink->CompactElements.GetData()->MeshId);
		const FElementCompact* CompactElementPtr = DrawingPolicyLink->CompactElements.GetData();
		for(int32 ElementIndex = 0; ElementIndex < NumElements; ElementIndex++, CompactElementPtr++)
		{
			if(StaticMeshVisibilityMap.AccessCorrespondingBit(FRelativeBitReference(CompactElementPtr->MeshId)))
			{
				const FElement& Element = DrawingPolicyLink->Elements[ElementIndex];
				const FBoxSphereBounds& Bounds = Element.Bounds;
				float DistanceSq = (Bounds.Origin - ViewLocation).SizeSquared();
				float DrawingPolicyDistanceSq = (DrawingPolicyCenter - ViewLocation).SizeSquared();
				SortKeys.Add(GetSortKey(Element.bBackground,Bounds.SphereRadius,DrawingPolicyDistanceSq,PolicyIt->AsInteger(),DistanceSq,ElementIndex));
			}
		}
	}

	SortKeys.Sort();

	int32 LastDrawingPolicyIndex = INDEX_NONE;
	FDrawingPolicyLink* DrawingPolicyLink = NULL;
	bool bDrawnShared = false;
	for (int32 SortedIndex = 0, NumSorted = FMath::Min(SortKeys.Num(),MaxToDraw); SortedIndex < NumSorted; ++SortedIndex)
	{
		int32 DrawingPolicyIndex = SortKeys[SortedIndex].Fields.DrawingPolicyIndex;
		int32 ElementIndex = SortKeys[SortedIndex].Fields.MeshElementIndex;
		if (DrawingPolicyIndex != LastDrawingPolicyIndex)
		{
			DrawingPolicyLink = &DrawingPolicySet[FSetElementId::FromInteger(DrawingPolicyIndex)];
			LastDrawingPolicyIndex = DrawingPolicyIndex;
			bDrawnShared = false;
		}

		const FElement& Element = DrawingPolicyLink->Elements[ElementIndex];
		INC_DWORD_STAT_BY(STAT_StaticMeshTriangles,Element.Mesh->GetNumPrimitives());
		// Avoid the cache miss looking up batch visibility if there is only one element.
		uint64 BatchElementMask = Element.Mesh->Elements.Num() == 1 ? 1 : BatchVisibilityArray[Element.Mesh->Id];
		DrawElement(RHICmdList, View, PolicyContext, Element, BatchElementMask, DrawingPolicyLink, bDrawnShared);
		NumDraws++;
	}

	return NumDraws;
}

template<typename DrawingPolicyType>
FVector TStaticMeshDrawList<DrawingPolicyType>::SortViewPosition = FVector(0);

template<typename DrawingPolicyType>
typename TStaticMeshDrawList<DrawingPolicyType>::TDrawingPolicySet* TStaticMeshDrawList<DrawingPolicyType>::SortDrawingPolicySet;

template<typename DrawingPolicyType>
int32 TStaticMeshDrawList<DrawingPolicyType>::Compare(FSetElementId A, FSetElementId B)
{
	const FSphere& BoundsA = (*SortDrawingPolicySet)[A].CachedBoundingSphere;
	const FSphere& BoundsB = (*SortDrawingPolicySet)[B].CachedBoundingSphere;

	// Assume state buckets with large bounds are background geometry
	if (BoundsA.W >= HALF_WORLD_MAX / 2 && BoundsB.W < HALF_WORLD_MAX / 2)
	{
		return 1;
	}
	else if (BoundsB.W >= HALF_WORLD_MAX / 2 && BoundsA.W < HALF_WORLD_MAX / 2)
	{
		return -1;
	}
	else
	{
		const float DistanceASquared = (BoundsA.Center - SortViewPosition).SizeSquared();
		const float DistanceBSquared = (BoundsB.Center - SortViewPosition).SizeSquared();
		// Sort front to back
		return DistanceASquared > DistanceBSquared ? 1 : -1;
	}
}

template<typename DrawingPolicyType>
void TStaticMeshDrawList<DrawingPolicyType>::SortFrontToBack(FVector ViewPosition)
{
	// Cache policy link bounds
	for (typename TDrawingPolicySet::TIterator DrawingPolicyIt(DrawingPolicySet); DrawingPolicyIt; ++DrawingPolicyIt)
	{
		FBoxSphereBounds AccumulatedBounds(ForceInit);

		FDrawingPolicyLink& DrawingPolicyLink = *DrawingPolicyIt;
		if (DrawingPolicyLink.Elements.Num())
		{
			AccumulatedBounds = DrawingPolicyLink.Elements[0].Bounds;
			for (int32 ElementIndex = 1; ElementIndex < DrawingPolicyLink.Elements.Num(); ElementIndex++)
		    {
		        FElement& Element = DrawingPolicyLink.Elements[ElementIndex];
		        AccumulatedBounds = AccumulatedBounds + Element.Bounds;
		    }
		}
		DrawingPolicyIt->CachedBoundingSphere = AccumulatedBounds.GetSphere();
	}

	SortViewPosition = ViewPosition;
	SortDrawingPolicySet = &DrawingPolicySet;

	OrderedDrawingPolicies.Sort( TCompareStaticMeshDrawList<DrawingPolicyType>() );
}

template<typename DrawingPolicyType>
void TStaticMeshDrawList<DrawingPolicyType>::GetUsedPrimitivesBasedOnMaterials(ERHIFeatureLevel::Type FeatureLevel, const TArray<const FMaterial*>& Materials, TArray<FPrimitiveSceneInfo*>& PrimitivesToUpdate)
{
	for (typename TDrawingPolicySet::TIterator DrawingPolicyIt(DrawingPolicySet); DrawingPolicyIt; ++DrawingPolicyIt)
	{
		FDrawingPolicyLink& DrawingPolicyLink = *DrawingPolicyIt;

		for (int32 ElementIndex = 0; ElementIndex < DrawingPolicyLink.Elements.Num(); ElementIndex++)
		{
			FElement& Element = DrawingPolicyLink.Elements[ElementIndex];

			// Compare to the referenced material, not the material used for rendering
			// In the case of async shader compiling, MaterialRenderProxy->GetMaterial() is going to be the default material until the async compiling is complete
			const FMaterialRenderProxy* Proxy = Element.Mesh->MaterialRenderProxy;

			if (Proxy)
			{
				FMaterial* MaterialResource = Proxy->GetMaterialNoFallback(FeatureLevel);

				if (Materials.Contains(MaterialResource))
				{
					PrimitivesToUpdate.AddUnique(Element.Mesh->PrimitiveSceneInfo);
				}
			}
		}
	}
}

template<typename DrawingPolicyType>
int32 TStaticMeshDrawList<DrawingPolicyType>::NumMeshes() const
{
	int32 TotalMeshes=0;
	for(typename TArray<FSetElementId>::TConstIterator PolicyIt(OrderedDrawingPolicies); PolicyIt; ++PolicyIt)
	{
		const FDrawingPolicyLink* DrawingPolicyLink = &DrawingPolicySet(*PolicyIt);
		TotalMeshes += DrawingPolicyLink->Elements.Num();
	}
	return TotalMeshes;
}

template<typename DrawingPolicyType>
FDrawListStats TStaticMeshDrawList<DrawingPolicyType>::GetStats() const
{
	FDrawListStats Stats = {0};
	TArray<int32> MeshCounts;
	for(typename TArray<FSetElementId>::TConstIterator PolicyIt(OrderedDrawingPolicies); PolicyIt; ++PolicyIt)
	{
		const FDrawingPolicyLink* DrawingPolicyLink = &DrawingPolicySet[*PolicyIt];
		int32 NumMeshes = DrawingPolicyLink->Elements.Num();
		Stats.NumDrawingPolicies++;
		Stats.NumMeshes += NumMeshes;
		MeshCounts.Add(NumMeshes);
	}
	if (MeshCounts.Num())
	{
		MeshCounts.Sort();
		Stats.MedianMeshesPerDrawingPolicy = MeshCounts[MeshCounts.Num() / 2];
		Stats.MaxMeshesPerDrawingPolicy = MeshCounts.Last();
		while (Stats.NumSingleMeshDrawingPolicies < MeshCounts.Num()
			&& MeshCounts[Stats.NumSingleMeshDrawingPolicies] == 1)
		{
			Stats.NumSingleMeshDrawingPolicies++;
		}
	}
	return Stats;
}

template<typename DrawingPolicyType>
void TStaticMeshDrawList<DrawingPolicyType>::ApplyWorldOffset(FVector InOffset)
{
	for (auto It = DrawingPolicySet.CreateIterator(); It; ++It)
	{
		FDrawingPolicyLink& DrawingPolicyLink = (*It);
		for (int32 ElementIndex = 0; ElementIndex < DrawingPolicyLink.Elements.Num(); ElementIndex++)
		{
			FElement& Element = DrawingPolicyLink.Elements[ElementIndex];
			Element.Bounds.Origin+= InOffset;
		}

		DrawingPolicyLink.CachedBoundingSphere.Center+= InOffset;
	}
}

#endif
