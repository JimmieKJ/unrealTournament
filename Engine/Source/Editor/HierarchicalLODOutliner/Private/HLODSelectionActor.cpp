// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "HLODSelectionActor.h"
#include "Components/DrawSphereComponent.h"

AHLODSelectionActor::AHLODSelectionActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCanBeDamaged = false;
	bListedInSceneOutliner = false;

#if WITH_EDITORONLY_DATA	
	DrawSphereComponent = CreateEditorOnlyDefaultSubobject<UDrawSphereComponent>(TEXT("VisualizeComponent0"));
	if (DrawSphereComponent)
	{
		DrawSphereComponent->SetSphereRadius(0.0f);
		RootComponent = DrawSphereComponent;
	}
#endif // WITH_EDITORONLY_DATA
}
