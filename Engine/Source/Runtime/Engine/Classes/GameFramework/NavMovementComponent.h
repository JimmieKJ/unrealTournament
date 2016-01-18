// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/**
 * Movement component that is compatible with the navigation system's PathFollowingComponent
 */

#pragma once

#include "AI/Navigation/NavigationSystem.h"
#include "AI/Navigation/NavAgentInterface.h"
#include "AI/Navigation/NavigationTypes.h"
#include "GameFramework/MovementComponent.h"
#include "Components/PrimitiveComponent.h"
#include "NavMovementComponent.generated.h"

class AActor;
class UCapsuleComponent;

/**
 * NavMovementComponent defines base functionality for MovementComponents that move any 'agent' that may be involved in AI pathfinding.
 */
UCLASS(abstract)
class ENGINE_API UNavMovementComponent : public UMovementComponent
{
	GENERATED_UCLASS_BODY()

	/** Properties that define how the component can move. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Movement Capabilities", meta=(DisplayName="Movement Capabilities", Keywords="Nav Agent"))
	FNavAgentProperties NavAgentProps;

protected:
	/** If set to true NavAgentProps' radius and height will be updated with Owner's collision capsule size */
	UPROPERTY(EditAnywhere, Category=MovementComponent)
	uint32 bUpdateNavAgentWithOwnersCollision:1;

private:
	/** If set, StopActiveMovement call will abort current path following request */
	uint32 bStopMovementAbortPaths:1;

public:
	/** Expresses runtime state of character's movement. Put all temporal changes to movement properties here */
	UPROPERTY()
	FMovementProperties MovementState;

	/** associated path following component */
	TWeakObjectPtr<class UPathFollowingComponent> PathFollowingComp;

	/** Stops applying further movement (usually zeros acceleration). */
	UFUNCTION(BlueprintCallable, Category="Pawn|Components|PawnMovement")
	virtual void StopActiveMovement();

	/** Stops movement immediately (reset velocity) but keeps following current path */
	UFUNCTION(BlueprintCallable, Category="Components|Movement")
	void StopMovementKeepPathing();

	// Overridden to also call StopActiveMovement().
	virtual void StopMovementImmediately() override;

	void SetUpdateNavAgentWithOwnersCollisions(bool bUpdateWithOwner);
	FORCEINLINE bool ShouldUpdateNavAgentWithOwnersCollision() const { return bUpdateNavAgentWithOwnersCollision != 0; }
	
	void UpdateNavAgent(const AActor& InOwner);
	void UpdateNavAgent(const UCapsuleComponent& CapsuleComponent);

	/** @returns location of controlled actor - meaning center of collision bounding box */
	FORCEINLINE FVector GetActorLocation() const { return UpdatedComponent ? UpdatedComponent->GetComponentLocation() : FVector(FLT_MAX); }
	/** @returns location of controlled actor's "feet" meaning center of bottom of collision bounding box */
	FORCEINLINE FVector GetActorFeetLocation() const { return UpdatedComponent ? (UpdatedComponent->GetComponentLocation() - FVector(0,0,UpdatedComponent->Bounds.BoxExtent.Z)) : FNavigationSystem::InvalidLocation; }
	/** @returns based location of controlled actor */
	virtual FBasedPosition GetActorFeetLocationBased() const;
	/** @returns navigation location of controlled actor */
	FORCEINLINE FVector GetActorNavLocation() const { INavAgentInterface* MyOwner = Cast<INavAgentInterface>(GetOwner()); return MyOwner ? MyOwner->GetNavAgentLocation() : FNavigationSystem::InvalidLocation; }

	/** request direct movement */
	virtual void RequestDirectMove(const FVector& MoveVelocity, bool bForceMaxSpeed);

	/** check if current move target can be reached right now if positions are matching
	 *  (e.g. performing scripted move and can't stop) */
	virtual bool CanStopPathFollowing() const;

	/** @returns the NavAgentProps(const) */
	FORCEINLINE const FNavAgentProperties& GetNavAgentPropertiesRef() const { return NavAgentProps; }
	/** @returns the NavAgentProps */
	FORCEINLINE FNavAgentProperties& GetNavAgentPropertiesRef() { return NavAgentProps; }

	/** Resets runtime movement state to character's movement capabilities */
	void ResetMoveState() { MovementState = NavAgentProps; }

	/** @return true if path following can start */
	virtual bool CanStartPathFollowing() const { return true; }

	/** @return true if component can crouch */
	FORCEINLINE bool CanEverCrouch() const { return NavAgentProps.bCanCrouch; }

	/** @return true if component can jump */
	FORCEINLINE bool CanEverJump() const { return NavAgentProps.bCanJump; }

	/** @return true if component can move along the ground (walk, drive, etc) */
	FORCEINLINE bool CanEverMoveOnGround() const { return NavAgentProps.bCanWalk; }

	/** @return true if component can swim */
	FORCEINLINE bool CanEverSwim() const { return NavAgentProps.bCanSwim; }

	/** @return true if component can fly */
	FORCEINLINE bool CanEverFly() const { return NavAgentProps.bCanFly; }

	/** @return true if component is allowed to jump */
	FORCEINLINE bool IsJumpAllowed() const { return CanEverJump() && MovementState.bCanJump; }

	/** @param bAllowed true if component is allowed to jump */
	FORCEINLINE void SetJumpAllowed(bool bAllowed) { MovementState.bCanJump = bAllowed; }


	/** @return true if currently crouching */ 
	UFUNCTION(BlueprintCallable, Category="AI|Components|NavMovement")
	virtual bool IsCrouching() const;

	/** @return true if currently falling (not flying, in a non-fluid volume, and not on the ground) */ 
	UFUNCTION(BlueprintCallable, Category="AI|Components|NavMovement")
	virtual bool IsFalling() const;

	/** @return true if currently moving on the ground (e.g. walking or driving) */ 
	UFUNCTION(BlueprintCallable, Category="AI|Components|NavMovement")
	virtual bool IsMovingOnGround() const;
	
	/** @return true if currently swimming (moving through a fluid volume) */
	UFUNCTION(BlueprintCallable, Category="AI|Components|NavMovement")
	virtual bool IsSwimming() const;

	/** @return true if currently flying (moving through a non-fluid volume without resting on the ground) */
	UFUNCTION(BlueprintCallable, Category="AI|Components|NavMovement")
	virtual bool IsFlying() const;

	//----------------------------------------------------------------------//
	// DEPRECATED
	//----------------------------------------------------------------------//
public:
	DEPRECATED(4.7, "This function is deprecated. Please use GetNavAgentPropertiesRef instead.")
	const FNavAgentProperties* GetNavAgentProperties() const;
	DEPRECATED(4.7, "This function is deprecated. Please use GetNavAgentPropertiesRef instead.")
	FNavAgentProperties* GetNavAgentProperties();

	DEPRECATED(4.8, "This function is deprecated. Please use UpdateNavAgent version that's accepring a reference instead.")
	void UpdateNavAgent(AActor* InOwner);
	DEPRECATED(4.8, "This function is deprecated. Please use UpdateNavAgent version that's accepring a reference instead.")
	void UpdateNavAgent(UCapsuleComponent* CapsuleComponent);
};


inline bool UNavMovementComponent::IsCrouching() const
{
	return false;
}

inline bool UNavMovementComponent::IsFalling() const
{
	return false;
}

inline bool UNavMovementComponent::IsMovingOnGround() const
{
	return false;
}

inline bool UNavMovementComponent::IsSwimming() const
{
	return false;
}

inline bool UNavMovementComponent::IsFlying() const
{
	return false;
}

inline void UNavMovementComponent::StopMovementKeepPathing()
{
	bStopMovementAbortPaths = false;
	StopMovementImmediately();
	bStopMovementAbortPaths = true;
}

inline void UNavMovementComponent::StopMovementImmediately()
{
	Super::StopMovementImmediately();
	StopActiveMovement();
}
