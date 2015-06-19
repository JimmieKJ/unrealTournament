// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "Paper2DPrivatePCH.h"
#include "PaperTerrainActor.h"
#include "PaperTerrainMaterial.h"
#include "PaperTerrainComponent.h"
#include "PaperTerrainSplineComponent.h"

//////////////////////////////////////////////////////////////////////////
// APaperTerrainActor

APaperTerrainActor::APaperTerrainActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DummyRoot = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));
	SplineComponent = CreateDefaultSubobject<UPaperTerrainSplineComponent>(TEXT("SplineComponent"));
 	RenderComponent = CreateDefaultSubobject<UPaperTerrainComponent>(TEXT("RenderComponent"));
 
	SplineComponent->AttachParent = DummyRoot;
	RenderComponent->AttachParent = DummyRoot;
	RenderComponent->AssociatedSpline = SplineComponent;
	RootComponent = DummyRoot;
}

#if WITH_EDITOR
bool APaperTerrainActor::GetReferencedContentObjects(TArray<UObject*>& Objects) const
{
	Super::GetReferencedContentObjects(Objects);

	if (RenderComponent->TerrainMaterial != nullptr)
	{
		Objects.Add(RenderComponent->TerrainMaterial);
	}
	return true;
}
#endif
