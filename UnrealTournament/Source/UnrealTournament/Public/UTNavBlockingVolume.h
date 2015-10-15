// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "AI/Navigation/NavigationSystem.h"
#include "AI/NavigationOctree.h"

#include "UTNavBlockingVolume.generated.h"

UCLASS(CustomConstructor)
class UNREALTOURNAMENT_API AUTNavBlockingVolume : public AVolume, public INavRelevantInterface // can't sublcass ABlockingVolume either...
{
	GENERATED_UCLASS_BODY()

	/** whether this volume should also be blocking for purposes of special move reachability tests (jump, swim, translocator, etc)
	 * if false, only blocking for purposes of navmesh generation
	 */
	UPROPERTY(EditAnywhere)
	bool bBlockSpecialMoveTests;

	AUTNavBlockingVolume(const FObjectInitializer& ObjectInitializer)
	//: Super(ObjectInitializer.SetDefaultSubobjectClass<UUTNavBlockingBrushComponent>("BrushComponent0"))
	: Super(ObjectInitializer)
	{
		GetBrushComponent()->bCanEverAffectNavigation = true;
		GetBrushComponent()->SetCollisionProfileName(FName(TEXT("InvisibleWall")));
		GetBrushComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision); // should match default for bBlockSpecialMoveTests
		// NOTE: this relies on no nav building during gameplay
		GetBrushComponent()->AlwaysLoadOnClient = false;
		GetBrushComponent()->AlwaysLoadOnServer = false;
		bNotForClientOrServer = true;
	}

#if WITH_EDITOR
	virtual void PostEditChangeChainProperty(struct FPropertyChangedChainEvent& PropertyChangedEvent) override
	{
		Super::PostEditChangeChainProperty(PropertyChangedEvent);

		GetBrushComponent()->SetCollisionEnabled(bBlockSpecialMoveTests ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
	}
#endif

	virtual void PostLoad() override
	{
		Super::PostLoad();

		if (GetBrushComponent() != NULL) // may be null in game, various versions have not fully respected bNotForClientOrServer on the actor
		{
			GetBrushComponent()->SetCollisionEnabled(bBlockSpecialMoveTests ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
		}
	}

	virtual void GetNavigationData(struct FNavigationRelevantData& Data) const override
	{
		// force BrushComponent to be exported for navmesh if we have no collision (because bBlockSpecialMoveTests is false)
		if (!bBlockSpecialMoveTests)
		{
			UNavigationSystem* NavSys = GetWorld()->GetNavigationSystem();
			if (NavSys != NULL && NavSys->GetNavOctree() != NULL && NavSys->GetNavOctree()->ComponentExportDelegate.IsBound())
			{
				GetBrushComponent()->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
				NavSys->GetNavOctree()->ComponentExportDelegate.Execute(GetBrushComponent(), Data);
				GetBrushComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			}
		}
	}

	virtual FBox GetNavigationBounds() const override
	{
		return GetBrushComponent()->Bounds.GetBox();
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