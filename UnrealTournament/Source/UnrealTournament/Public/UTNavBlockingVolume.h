// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTNavBlockingVolume.generated.h"

UCLASS(CustomConstructor)
class UNREALTOURNAMENT_API AUTNavBlockingVolume : public AVolume // can't sublcass ABlockingVolume either...
{
	GENERATED_UCLASS_BODY()

	AUTNavBlockingVolume(const FObjectInitializer& ObjectInitializer)
	//: Super(ObjectInitializer.SetDefaultSubobjectClass<UUTNavBlockingBrushComponent>("BrushComponent0"))
	: Super(ObjectInitializer)
	{
		GetBrushComponent()->bCanEverAffectNavigation = true;
		GetBrushComponent()->SetCollisionProfileName(FName(TEXT("InvisibleWall")));
		// NOTE: this relies on no nav building during gameplay
		GetBrushComponent()->AlwaysLoadOnClient = false;
		GetBrushComponent()->AlwaysLoadOnServer = false;
		bNotForClientOrServer = true;
	}

	// it would've been nice to have a component that just does the right thing but unfortunately UBrushComponent can't be subclassed by modules
	virtual void PreInitializeComponents() override
	{
		// should not be in game
		if (GetWorld()->IsGameWorld())
		{
			GetWorld()->DestroyActor(this, true);
		}
		else
		{
			Super::PreInitializeComponents();
		}
	}
};