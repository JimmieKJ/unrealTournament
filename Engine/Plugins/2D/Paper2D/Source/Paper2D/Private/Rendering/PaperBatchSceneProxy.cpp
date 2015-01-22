// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "Paper2DPrivatePCH.h"
#include "PaperBatchSceneProxy.h"
#include "PaperRenderSceneProxy.h"

//////////////////////////////////////////////////////////////////////////
// FPaperBatchSceneProxy

FPaperBatchSceneProxy::FPaperBatchSceneProxy(const UPrimitiveComponent* InComponent)
	: FPrimitiveSceneProxy(InComponent)
{
}

uint32 FPaperBatchSceneProxy::GetMemoryFootprint() const
{
	return sizeof(*this) + GetAllocatedSize();
}

FPrimitiveViewRelevance FPaperBatchSceneProxy::GetViewRelevance(const FSceneView* View)
{
	checkSlow(IsInParallelRenderingThread());

	FPrimitiveViewRelevance Result;
	Result.bDrawRelevance = IsShown(View) && View->Family->EngineShowFlags.Paper2DSprites;
	Result.bRenderCustomDepth = ShouldRenderCustomDepth();
	Result.bRenderInMainPass = ShouldRenderInMainPass();

	Result.bMaskedRelevance = true;
	//Result.bNormalTranslucencyRelevance = true;
	Result.bDynamicRelevance = true;
	Result.bOpaqueRelevance = true;

	return Result;
}

void FPaperBatchSceneProxy::RegisterManagedProxy(FPaperRenderSceneProxy* Proxy)
{
	check(IsInRenderingThread());
	ManagedProxies.Add(Proxy);
}

void FPaperBatchSceneProxy::UnregisterManagedProxy(FPaperRenderSceneProxy* Proxy)
{
	check(IsInRenderingThread());
	ManagedProxies.RemoveSwap(Proxy);
}

bool FPaperBatchSceneProxy::CanBeOccluded() const
{
	return false;
}
