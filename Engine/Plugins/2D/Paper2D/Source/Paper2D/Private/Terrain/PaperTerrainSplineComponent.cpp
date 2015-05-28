// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "Paper2DPrivatePCH.h"
#include "PaperTerrainSplineComponent.h"

//////////////////////////////////////////////////////////////////////////
// UPaperTerrainSplineComponent

UPaperTerrainSplineComponent::UPaperTerrainSplineComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

#if WITH_EDITOR
void UPaperTerrainSplineComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	OnSplineEdited.ExecuteIfBound();
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif
