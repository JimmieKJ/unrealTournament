// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "Paper2DPrivatePCH.h"
#include "PaperTileMapActor.h"
#include "PaperTileMapComponent.h"

//////////////////////////////////////////////////////////////////////////
// APaperTileMapActor

APaperTileMapActor::APaperTileMapActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	RenderComponent = CreateDefaultSubobject<UPaperTileMapComponent>(TEXT("RenderComponent"));

	RootComponent = RenderComponent;
}

#if WITH_EDITOR
bool APaperTileMapActor::GetReferencedContentObjects(TArray<UObject*>& Objects) const
{
	Super::GetReferencedContentObjects(Objects);

	if (const UObject* Asset = RenderComponent->AdditionalStatObject())
	{
		Objects.Add(const_cast<UObject*>(Asset));
	}
	return true;
}
#endif
