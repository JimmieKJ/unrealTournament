// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "Paper2DPrivatePCH.h"

//////////////////////////////////////////////////////////////////////////
// APaperTerrainActor

APaperTerrainActor::APaperTerrainActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DummyRoot = ObjectInitializer.CreateDefaultSubobject<USceneComponent>(this, TEXT("RootComponent"));
	SplineComponent = ObjectInitializer.CreateDefaultSubobject<UPaperTerrainSplineComponent>(this, TEXT("SplineComponent"));
 	RenderComponent = ObjectInitializer.CreateDefaultSubobject<UPaperTerrainComponent>(this, TEXT("RenderComponent"));
 
	SplineComponent->AttachParent = DummyRoot;
	RenderComponent->AttachParent = DummyRoot;
	RenderComponent->AssociatedSpline = SplineComponent;
	RootComponent = DummyRoot;
}

#if WITH_EDITOR
bool APaperTerrainActor::GetReferencedContentObjects(TArray<UObject*>& Objects) const
{
	if (RenderComponent->TerrainMaterial != nullptr)
	{
		Objects.Add(RenderComponent->TerrainMaterial);
	}
	return true;
}
#endif
