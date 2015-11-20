// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	StaticMeshDrawList.inl: Static mesh draw list implementation.
=============================================================================*/

#pragma once

#include "RHICommandList.h"
#include "EngineStats.h"
#include "ParallelFor.h"

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
				float DitherValue = View.GetDitheredLODTransitionValue(*Element.Mesh);
				DrawingPolicyLink->DrawingPolicy.SetMeshRenderState(
					RHICmdList, 
					View,
					Element.Mesh->PrimitiveSceneInfo->Proxy,
					*Element.Mesh,
					BatchElementIndex,
					!!BackFace,
					DitherValue,
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
	: FrameNumberForVisibleCount(MAX_uint32)
	, ViewStateUniqueId(0)
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
	STAT(int32 StatInc = 0;)
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
				STAT(StatInc +=  Element.Mesh->GetNumPrimitives();)
				// Avoid the virtual call looking up batch visibility if there is only one element.
				uint32 BatchElementMask = Element.Mesh->Elements.Num() == 1 ? 1 : Element.Mesh->VertexFactory->GetStaticBatchElementVisibility(View,Element.Mesh);
				DrawElement(View, PolicyContext, Element, BatchElementMask, DrawingPolicyLink, bDrawnShared);
				bDirty = true;
			}
		}
	}
	INC_DWORD_STAT_BY(STAT_StaticMeshTriangles, StatInc);
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
	STAT(int32 StatInc = 0;)
	for (int32 Index = FirstPolicy; Index <= LastPolicy; Index++)
	{
		FDrawingPolicyLink* DrawingPolicyLink = &DrawingPolicySet[OrderedDrawingPolicies[Index]];
		bool bDrawnShared = false;
		FPlatformMisc::Prefetch(DrawingPolicyLink->CompactElements.GetData());
		const int32 NumElements = DrawingPolicyLink->Elements.Num();
		FPlatformMisc::Prefetch(&DrawingPolicyLink->CompactElements.GetData()->MeshId);
		const FElementCompact* CompactElementPtr = DrawingPolicyLink->CompactElements.GetData();
		uint32 Count = 0;
		for (int32 ElementIndex = 0; ElementIndex < NumElements; ElementIndex++, CompactElementPtr++)
		{
			if (StaticMeshVisibilityMap.AccessCorrespondingBit(FRelativeBitReference(CompactElementPtr->MeshId)))
			{
				const FElement& Element = DrawingPolicyLink->Elements[ElementIndex];
				STAT(StatInc +=  Element.Mesh->GetNumPrimitives();)
				int32 SubCount = Element.Mesh->Elements.Num();
				Count += SubCount;
				// Avoid the cache miss looking up batch visibility if there is only one element.
				uint64 BatchElementMask = SubCount == 1 ? 1 : BatchVisibilityArray[Element.Mesh->Id];
				DrawElement(RHICmdList, View, PolicyContext, Element, BatchElementMask, DrawingPolicyLink, bDrawnShared);
			}
		}
		bDirty = bDirty || !!Count;
		DrawingPolicyLink->VisibleCount = Count;
	}
	INC_DWORD_STAT_BY(STAT_StaticMeshTriangles, StatInc);
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

	const TArray<uint16, SceneRenderingAllocator>* PerDrawingPolicyCounts;

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
		const TArray<uint16, SceneRenderingAllocator>* InPerDrawingPolicyCounts
		)
		: Caller(InCaller)
		, RHICmdList(InRHICmdList)
		, View(InView)
		, PolicyContext(InPolicyContext)
		, StaticMeshVisibilityMap(InStaticMeshVisibilityMap)
		, BatchVisibilityArray(InBatchVisibilityArray)
		, FirstPolicy(InFirstPolicy)
		, LastPolicy(InLastPolicy)
		, PerDrawingPolicyCounts(InPerDrawingPolicyCounts)
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
		if (this->PerDrawingPolicyCounts)
		{
			int32 Start = this->FirstPolicy;
			// we have per policy draw counts, lets not look at the zeros
			while (Start <= this->LastPolicy)
			{
				while (Start <= this->LastPolicy && !(*PerDrawingPolicyCounts)[Start])
				{
					Start++;
				}
				if (Start <= this->LastPolicy)
				{
					int32 BatchEnd = Start;
					while (BatchEnd < this->LastPolicy && (*PerDrawingPolicyCounts)[BatchEnd + 1])
					{
						BatchEnd++;
					}
					this->Caller.DrawVisibleInner(this->RHICmdList, this->View, this->PolicyContext, this->StaticMeshVisibilityMap, this->BatchVisibilityArray, Start, BatchEnd);
					Start = BatchEnd + 1;
				}
			}
		}
		else
		{
			this->Caller.DrawVisibleInner(this->RHICmdList, this->View, this->PolicyContext, this->StaticMeshVisibilityMap, this->BatchVisibilityArray, this->FirstPolicy, this->LastPolicy);
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
	int32 NumPolicies = OrderedDrawingPolicies.Num();
	int32 EffectiveThreads = FMath::Min<int32>(NumPolicies, ParallelCommandListSet.Width);
	if (EffectiveThreads)
	{

		if (ParallelCommandListSet.bBalanceCommands)
		{
			TArray<uint16, SceneRenderingAllocator>& PerDrawingPolicyCounts = *new (FMemStack::Get()) TArray<uint16, SceneRenderingAllocator>();
			PerDrawingPolicyCounts.AddZeroed(NumPolicies);

			bool bNeedScan = true;
			bool bCountsAreAccurate = false;
			uint32 ViewKey = ParallelCommandListSet.View.GetViewKey();
			uint32 ViewFrame = ParallelCommandListSet.View.GetOcclusionFrameCounter();
			if (ParallelCommandListSet.bBalanceCommandsWithLastFrame)
			{
				// this means this will never work in split screen or the editor (multiple views) I am ok with that. It is fixable with more elaborate bookkeeping and additional memory.
				if (ViewKey && ViewStateUniqueId == ViewKey && FrameNumberForVisibleCount + 1 == ViewFrame)
				{
					QUICK_SCOPE_CYCLE_COUNTER(STAT_FDeferredShadingSceneRenderer_DrawVisibleParallel_LastFrameScan);
					// could use a ParallelFor here, but would rather leave that perf for the game thread.
					for (int32 Index = 0; Index < NumPolicies; Index++)
					{
						FDrawingPolicyLink* DrawingPolicyLink = &DrawingPolicySet[OrderedDrawingPolicies[Index]];
						PerDrawingPolicyCounts[Index] = (uint16)FMath::Min<uint32>(DrawingPolicyLink->VisibleCount, MAX_uint16);
					}
					bNeedScan = false;
				}
			}

			if (bNeedScan)
			{
				QUICK_SCOPE_CYCLE_COUNTER(STAT_FDeferredShadingSceneRenderer_DrawVisibleParallel_FullVisibilityScan);
				ParallelForWithPreWork(NumPolicies, 
					[this, &PerDrawingPolicyCounts, &StaticMeshVisibilityMap, &BatchVisibilityArray](int32 Index)
					{
						int32 Count = 0;

						FDrawingPolicyLink* DrawingPolicyLink = &DrawingPolicySet[OrderedDrawingPolicies[Index]];
						const FElementCompact* CompactElementPtr = DrawingPolicyLink->CompactElements.GetData();
						FPlatformMisc::Prefetch(CompactElementPtr);
						const int32 NumElements = DrawingPolicyLink->CompactElements.Num();
						FPlatformMisc::Prefetch(&DrawingPolicyLink->CompactElements.GetData()->MeshId);
						for (int32 ElementIndex = 0; ElementIndex < NumElements; ElementIndex++, CompactElementPtr++)
						{
							if (StaticMeshVisibilityMap.AccessCorrespondingBit(FRelativeBitReference(CompactElementPtr->MeshId)))
							{
								const FElement& Element = DrawingPolicyLink->Elements[ElementIndex];
								Count += Element.Mesh->Elements.Num();
							}
						}
						if (Count)
						{
							// this is unlikely to overflow, but I don't think it would matter much if it did
							PerDrawingPolicyCounts[Index] = (uint16)FMath::Min<int32>(Count, MAX_uint16);
						}
						DrawingPolicyLink->VisibleCount = Count;
					},
					[]()
					{
						QUICK_SCOPE_CYCLE_COUNTER(STAT_FDeferredShadingSceneRenderer_DrawVisibleParallel_ServiceLocalQueue);
						FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::RenderThread_Local);
					}
				);
				bCountsAreAccurate = true;
			}
			FrameNumberForVisibleCount = ViewFrame;
			ViewStateUniqueId = ViewKey;

			int32 Total = 0;
			for (uint16 Count : PerDrawingPolicyCounts)
			{
				Total += Count;
			}
			if (bCountsAreAccurate && Total == 0)
			{
				return;
			}
			UE_CLOG(ParallelCommandListSet.bSpewBalance, LogTemp, Display, TEXT("Total Draws %d"), Total);

			EffectiveThreads = FMath::Min<int32>(EffectiveThreads, FMath::DivideAndRoundUp(FMath::Max<int32>(1, Total), ParallelCommandListSet.MinDrawsPerCommandList));
			check(EffectiveThreads > 0 && EffectiveThreads <= ParallelCommandListSet.Width);

			int32 DrawsPerCmdList = FMath::DivideAndRoundUp(Total, EffectiveThreads);
			int32 DrawsPerCmdListMergeLimit = FMath::DivideAndRoundUp(DrawsPerCmdList, 3); // if the last list would be small, we just merge it into the previous one

			int32 Start = 0;

			int32 PreviousBatchStart = -1;
			int32 PreviousBatchEnd = -2;
			int32 PreviousBatchDraws = 0;

			while (Start < NumPolicies)
			{
				// skip zeros
				while (bCountsAreAccurate && Start < NumPolicies && !PerDrawingPolicyCounts[Start])
				{
					Start++;
				}
				if (Start < NumPolicies)
				{
					int32 BatchCount = PerDrawingPolicyCounts[Start];
					int32 BatchEnd = Start;
					int32 LastNonZeroPolicy = Start;
					while (BatchEnd < NumPolicies - 1 && BatchCount < DrawsPerCmdList)
					{
						BatchEnd++;
						if (!bCountsAreAccurate || PerDrawingPolicyCounts[BatchEnd])
						{
							BatchCount += PerDrawingPolicyCounts[BatchEnd];
							LastNonZeroPolicy = BatchEnd;
						}
					}
					if (BatchCount)
					{
						if (PreviousBatchStart <= PreviousBatchEnd)
						{
							FRHICommandList* CmdList = ParallelCommandListSet.NewParallelCommandList();
							if (BatchCount < DrawsPerCmdListMergeLimit)
							{
								// this is the last batch and it is small, let merge it
								UE_CLOG(ParallelCommandListSet.bSpewBalance, LogTemp, Display, TEXT("    Index %d  BatchCount %d    (last merge)"), ParallelCommandListSet.NumParallelCommandLists(), PreviousBatchDraws + BatchCount);
								FGraphEventRef AnyThreadCompletionEvent = TGraphTask<FDrawVisibleAnyThreadTask<DrawingPolicyType> >::CreateTask(ParallelCommandListSet.GetPrereqs(), ENamedThreads::RenderThread)
									.ConstructAndDispatchWhenReady(*this, *CmdList, ParallelCommandListSet.View, PolicyContext, StaticMeshVisibilityMap, BatchVisibilityArray, PreviousBatchStart, LastNonZeroPolicy, bCountsAreAccurate ? &PerDrawingPolicyCounts : nullptr);
								ParallelCommandListSet.AddParallelCommandList(CmdList, AnyThreadCompletionEvent, PreviousBatchDraws + BatchCount);
								PreviousBatchStart = -1;
								PreviousBatchEnd = -2;
								PreviousBatchDraws = 0;
							}
							else
							{
								// this is a decent sized batch, emit the previous batch and save this one for possible merging
								UE_CLOG(ParallelCommandListSet.bSpewBalance, LogTemp, Display, TEXT("    Index %d  BatchCount %d    "), ParallelCommandListSet.NumParallelCommandLists(), PreviousBatchDraws);
								FGraphEventRef AnyThreadCompletionEvent = TGraphTask<FDrawVisibleAnyThreadTask<DrawingPolicyType> >::CreateTask(ParallelCommandListSet.GetPrereqs(), ENamedThreads::RenderThread)
									.ConstructAndDispatchWhenReady(*this, *CmdList, ParallelCommandListSet.View, PolicyContext, StaticMeshVisibilityMap, BatchVisibilityArray, PreviousBatchStart, PreviousBatchEnd, bCountsAreAccurate ? &PerDrawingPolicyCounts : nullptr);
								ParallelCommandListSet.AddParallelCommandList(CmdList, AnyThreadCompletionEvent, PreviousBatchDraws);
								PreviousBatchStart = Start;
								PreviousBatchEnd = LastNonZeroPolicy;
								PreviousBatchDraws = BatchCount;
							}
						}
						else
						{
							// no batch yet, save this one
							PreviousBatchStart = Start;
							PreviousBatchEnd = LastNonZeroPolicy;
							PreviousBatchDraws = BatchCount;
						}
					}
					Start = BatchEnd + 1;
				}
			}
			// we didn't merge the last batch, emit now
			if (PreviousBatchStart <= PreviousBatchEnd)
			{
				UE_CLOG(ParallelCommandListSet.bSpewBalance, LogTemp, Display, TEXT("    Index %d  BatchCount %d    (last)"), ParallelCommandListSet.NumParallelCommandLists(), PreviousBatchDraws);
				FRHICommandList* CmdList = ParallelCommandListSet.NewParallelCommandList();
				FGraphEventRef AnyThreadCompletionEvent = TGraphTask<FDrawVisibleAnyThreadTask<DrawingPolicyType> >::CreateTask(ParallelCommandListSet.GetPrereqs(), ENamedThreads::RenderThread)
					.ConstructAndDispatchWhenReady(*this, *CmdList, ParallelCommandListSet.View, PolicyContext, StaticMeshVisibilityMap, BatchVisibilityArray, PreviousBatchStart, PreviousBatchEnd, bCountsAreAccurate ? &PerDrawingPolicyCounts : nullptr);
				ParallelCommandListSet.AddParallelCommandList(CmdList, AnyThreadCompletionEvent, PreviousBatchDraws);
			}
		}
		else
		{
			int32 NumPer = OrderedDrawingPolicies.Num() / EffectiveThreads;
			int32 Extra = OrderedDrawingPolicies.Num() - NumPer * EffectiveThreads;
				int32 Start = 0;
			for (int32 ThreadIndex = 0; ThreadIndex < EffectiveThreads; ThreadIndex++)
			{
				int32 Last = Start + (NumPer - 1) + (ThreadIndex < Extra);
				check(Last >= Start);

				{
					FRHICommandList* CmdList = ParallelCommandListSet.NewParallelCommandList();

						FGraphEventRef AnyThreadCompletionEvent = TGraphTask<FDrawVisibleAnyThreadTask<DrawingPolicyType> >::CreateTask(ParallelCommandListSet.GetPrereqs(), ENamedThreads::RenderThread)
							.ConstructAndDispatchWhenReady(*this, *CmdList, ParallelCommandListSet.View, PolicyContext, StaticMeshVisibilityMap, BatchVisibilityArray, Start, Last, nullptr);

					ParallelCommandListSet.AddParallelCommandList(CmdList, AnyThreadCompletionEvent);
				}

				Start = Last + 1;
			}
			check(Start == OrderedDrawingPolicies.Num());
		}
	}
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
	STAT(int32 StatInc = 0;)
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
		STAT(StatInc +=  Element.Mesh->GetNumPrimitives();)
		// Avoid the cache miss looking up batch visibility if there is only one element.
		uint64 BatchElementMask = Element.Mesh->Elements.Num() == 1 ? 1 : BatchVisibilityArray[Element.Mesh->Id];
		DrawElement(RHICmdList, View, PolicyContext, Element, BatchElementMask, DrawingPolicyLink, bDrawnShared);
		NumDraws++;
	}
	INC_DWORD_STAT_BY(STAT_StaticMeshTriangles, StatInc);

	return NumDraws;
}

template<typename DrawingPolicyType>
int32 TStaticMeshDrawList<DrawingPolicyType>::Compare(FSetElementId A, FSetElementId B, const TDrawingPolicySet * const InSortDrawingPolicySet, const FVector InSortViewPosition)
{
	const FSphere& BoundsA = (*InSortDrawingPolicySet)[A].CachedBoundingSphere;
	const FSphere& BoundsB = (*InSortDrawingPolicySet)[B].CachedBoundingSphere;

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
		const float DistanceASquared = (BoundsA.Center - InSortViewPosition).SizeSquared();
		const float DistanceBSquared = (BoundsB.Center - InSortViewPosition).SizeSquared();
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

		const int32 NumElements = DrawingPolicyLink.Elements.Num();
		if (NumElements)
		{
			AccumulatedBounds = DrawingPolicyLink.Elements[0].Bounds;

		    for (int32 ElementIndex = 1; ElementIndex < NumElements; ElementIndex++)
		    {
			    AccumulatedBounds = AccumulatedBounds + DrawingPolicyLink.Elements[ElementIndex].Bounds;
		    }
		}

		DrawingPolicyIt->CachedBoundingSphere = AccumulatedBounds.GetSphere();
	}

	OrderedDrawingPolicies.Sort(TCompareStaticMeshDrawList<DrawingPolicyType>(&DrawingPolicySet, ViewPosition));
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
				check(!Proxy->IsDeleted());
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

