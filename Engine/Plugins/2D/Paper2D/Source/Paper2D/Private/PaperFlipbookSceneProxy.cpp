// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "Paper2DPrivatePCH.h"
#include "PaperFlipbookSceneProxy.h"
#include "PaperFlipbookComponent.h"

//////////////////////////////////////////////////////////////////////////
// FPaperFlipbookSceneProxy

FPaperFlipbookSceneProxy::FPaperFlipbookSceneProxy(const UPaperFlipbookComponent* InComponent)
	: FPaperRenderSceneProxy(InComponent)
{
	//@TODO: PAPER2D: WireframeColor = RenderComp->GetWireframeColor();

	Material = InComponent->GetMaterial(0);
	MaterialRelevance = InComponent->GetMaterialRelevance(GetScene().GetFeatureLevel());
}
