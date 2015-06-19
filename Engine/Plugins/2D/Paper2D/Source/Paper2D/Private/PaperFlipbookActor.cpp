// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "Paper2DPrivatePCH.h"
#include "PaperFlipbookActor.h"

//////////////////////////////////////////////////////////////////////////
// APaperFlipbookActor

APaperFlipbookActor::APaperFlipbookActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	RenderComponent = CreateDefaultSubobject<UPaperFlipbookComponent>(TEXT("RenderComponent"));

	RootComponent = RenderComponent;
}

#if WITH_EDITOR
bool APaperFlipbookActor::GetReferencedContentObjects(TArray<UObject*>& Objects) const
{
	Super::GetReferencedContentObjects(Objects);

	if (UPaperFlipbook* FlipbookAsset = RenderComponent->GetFlipbook())
	{
		Objects.Add(FlipbookAsset);
	}
	return true;
}
#endif
