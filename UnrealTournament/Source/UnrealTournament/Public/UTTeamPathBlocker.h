// base class for objects that only allow certain team(s) through
// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTPathBuilderInterface.h"

#include "UTTeamPathBlocker.generated.h"

UCLASS(Abstract, Blueprintable)
class UNREALTOURNAMENT_API AUTTeamPathBlocker : public AActor, public IUTPathBuilderInterface, public INavRelevantInterface
{
	GENERATED_BODY()
public:
	/** whether players of the passed in team are allowed through, used by the AI pathing code */
	UFUNCTION(BlueprintNativeEvent)
	bool IsAllowedThrough(uint8 TeamNum);

	/** return component that actually blocks players from the wrong team
	 * if unknown or it's not a single component, returns NULL
	 * this is used to shape the affected area for pathing
	 */
	UFUNCTION(BlueprintNativeEvent, meta = (CallInEditor = "true"))
	USceneComponent* GetBlockingComponent() const;

	virtual void GetSimpleCollisionCylinder(float& CollisionRadius, float& CollisionHalfHeight) const
	{
		USceneComponent* Blocker = GetBlockingComponent();
		if (Blocker != nullptr && Blocker->IsRegistered() && Blocker->IsCollisionEnabled())
		{
			Blocker->CalcBoundingCylinder(CollisionRadius, CollisionHalfHeight);
		}
		else
		{
			Super::GetSimpleCollisionCylinder(CollisionRadius, CollisionHalfHeight);
		}
	}

	virtual void AddSpecialPaths(class UUTPathNode* MyNode, class AUTRecastNavMesh* NavData) override;

	virtual bool IsDestinationOnly() const
	{
		return true;
	}

	virtual void GetNavigationData(FNavigationRelevantData& Data) const override;
	virtual FBox GetNavigationBounds() const override
	{
		FVector Extent = GetSimpleCollisionCylinderExtent();
		return FBox(GetActorLocation() - Extent, GetActorLocation() + Extent);
	}
};