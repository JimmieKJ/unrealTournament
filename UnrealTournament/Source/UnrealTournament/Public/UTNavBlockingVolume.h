// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTNavBlockingVolume.generated.h"

UCLASS(CustomConstructor)
class AUTNavBlockingVolume : public AVolume // can't sublcass ABlockingVolume either...
{
	GENERATED_UCLASS_BODY()

	AUTNavBlockingVolume(const FPostConstructInitializeProperties& PCIP)
	//: Super(PCIP.SetDefaultSubobjectClass<UUTNavBlockingBrushComponent>("BrushComponent0"))
	: Super(PCIP)
	{
		BrushComponent->bCanEverAffectNavigation = true;
		BrushComponent->BodyInstance.bEnableCollision_DEPRECATED = true;
		BrushComponent->SetCollisionProfileName(FName(TEXT("InvisibleWall")));
		// NOTE: this relies on no nav building during gameplay
		BrushComponent->AlwaysLoadOnClient = false;
		BrushComponent->AlwaysLoadOnServer = false;
		bNotForClientOrServer = true;
	}

	// it would've been nice to have a component that just does the right thing but unfortunately UBrushComponent can't be subclassed by modules
	virtual void PreInitializeComponents() override
	{
		// should not be in game
		if (GetWorld()->IsGameWorld())
		{
			Destroy();
		}
		else
		{
			Super::PreInitializeComponents();
		}
	}
};