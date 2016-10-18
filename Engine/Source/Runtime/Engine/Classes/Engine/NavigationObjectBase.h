// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "AI/Navigation/NavAgentInterface.h"
#include "NavigationObjectBase.generated.h"

UCLASS(hidecategories=(Lighting, LightColor, Force), ClassGroup=Navigation, NotBlueprintable, abstract)
class ENGINE_API ANavigationObjectBase : public AActor, public INavAgentInterface
{
	GENERATED_UCLASS_BODY()

private_subobject:
	DEPRECATED_FORGAME(4.6, "CapsuleComponent should not be accessed directly, please use GetCapsuleComponent() function instead. CapsuleComponent will soon be private and your code will not compile.")
	UPROPERTY()
	class UCapsuleComponent* CapsuleComponent;

	/** Normal editor sprite. */
	DEPRECATED_FORGAME(4.6, "GoodSprite should not be accessed directly, please use GetGoodSprite() function instead. GoodSprite will soon be private and your code will not compile.")
	UPROPERTY()
	class UBillboardComponent* GoodSprite;

	/** Used to draw bad collision intersection in editor. */
	DEPRECATED_FORGAME(4.6, "BadSprite should not be accessed directly, please use GetBadSprite() function instead. BadSprite will soon be private and your code will not compile.")
	UPROPERTY()
	class UBillboardComponent* BadSprite;
public:

	/** True if this nav point was spawned to be a PIE player start. */
	UPROPERTY()
	uint32 bIsPIEPlayerStart:1;


	//~ Begin AActor Interface
	virtual void GetSimpleCollisionCylinder(float& CollisionRadius, float& CollisionHalfHeight) const override;

#if	WITH_EDITOR
	virtual void PostEditMove(bool bFinished) override;
	virtual void PostEditUndo() override;
#endif	// WITH_EDITOR
	//~ End AActor Interface

	/** @todo document */
	virtual bool ShouldBeBased();
	/** @todo document */
	virtual void FindBase();

	/** 
	 * Check to make sure the navigation is at a valid point. 
	 * Detect when path building is going to move a pathnode around
	 * This may be undesirable for LDs (ie b/c cover links define slots by offsets)
	 */
	virtual void Validate();

	/** Return Physics Volume for this Actor. **/
	class APhysicsVolume* GetNavPhysicsVolume();

	// INavAgentInterface start
	virtual FVector GetNavAgentLocation() const override { return GetActorLocation(); }
	virtual void GetMoveGoalReachTest(const AActor* MovingActor, const FVector& MoveOffset, FVector& GoalOffset, float& GoalRadius, float& GoalHalfHeight) const override;
	// INavAgentInterface end

public:
	/** Returns CapsuleComponent subobject **/
	class UCapsuleComponent* GetCapsuleComponent() const;
	/** Returns GoodSprite subobject **/
	class UBillboardComponent* GetGoodSprite() const;
	/** Returns BadSprite subobject **/
	class UBillboardComponent* GetBadSprite() const;
};



