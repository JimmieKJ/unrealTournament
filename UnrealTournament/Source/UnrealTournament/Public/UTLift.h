// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTPathBuilderInterface.h"
#include "UTMovementBaseInterface.h"
#include "UTLift.generated.h"

UCLASS(BlueprintType, Blueprintable, Abstract)
class UNREALTOURNAMENT_API AUTLift : public AActor, public INavRelevantInterface, public IUTPathBuilderInterface, public IUTMovementBaseInterface
{
	GENERATED_UCLASS_BODY()

	UFUNCTION()
	virtual void OnOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	inline class UPrimitiveComponent* GetEncroachComponent() const
	{
		return EncroachComponent;
	}

	/** Event when encroach an actor (Overlap that can't be handled gracefully) */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Lift")
	virtual void SetEncroachComponent(class UPrimitiveComponent* NewEncroachComponent);

	/** Event when encroach an actor (Overlap that can't be handled gracefully) */
	UFUNCTION(BlueprintImplementableEvent, BlueprintAuthorityOnly, Category = "Lift")
	void OnEncroachActor(class AActor* EncroachedActor);
	
	virtual void NotifyHit(class UPrimitiveComponent* MyComp, AActor* Other, class UPrimitiveComponent* OtherComp, bool bSelfMoved, FVector HitLocation, FVector HitNormal, FVector NormalImpulse, const FHitResult& Hit) override;
	
	// UTMovementBaseInterface
	virtual void AddBasedCharacter_Implementation(class AUTCharacter* BasedCharacter) {};
	virtual	void RemoveBasedCharacter_Implementation(class AUTCharacter* BasedCharacter) {};

	/** List of characters based on this lift - may include dead or destroyed characters. */
	UPROPERTY()
	TArray<AUTCharacter*> BasedCharacters;

	/** Added BasedCharacter to the BasedCharacters array. */
	UFUNCTION(BlueprintCallable, Category = "Lift")
	virtual void UpdateCurrentlyBasedCharacters(class AUTCharacter* BasedCharacter);

	/** Returns true if this lift has based characters (that are alive). */
	UFUNCTION(BlueprintCallable, Category = "Lift")
	virtual bool HasBasedCharacters();

	/** Move the colliding lift mesh component */
	UFUNCTION(BlueprintCallable, Category = "Lift")
	virtual void MoveLiftTo(FVector NewLocation, FRotator NewRotation);

	UPROPERTY()
	float LastEncroachNotifyTime;

	UPROPERTY(BlueprintReadOnly, Category = "Lift")
	FVector LiftVelocity;

	/** scaling for the navmesh geometry that is exported on the lift's end location
	 * it is better for AI if the navmesh in the lift's non-resting position does not directly connect to the normal navmesh area
	 */
	UPROPERTY(EditAnywhere, Category = "AI")
	float NavmeshScale;

	virtual FVector GetVelocity() const override;

	virtual void Tick(float DeltaTime) override;

	/** return all the locations the lift stops at */
	UFUNCTION(BlueprintImplementableEvent, Category = "Lift", meta = (CallInEditor = "true"))
	TArray<FVector> GetStops() const;

	virtual void GetNavigationData(struct FNavigationRelevantData& Data) const;
	virtual FBox GetNavigationBounds() const;

	virtual bool IsPOI() const
	{
		return false;
	}

	virtual void AddSpecialPaths(class UUTPathNode* MyNode, class AUTRecastNavMesh* NavData);
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override
	{
		Super::PostEditChangeProperty(PropertyChangedEvent);

		if (PropertyChangedEvent.Property == NULL || PropertyChangedEvent.Property->GetFName() == FName(TEXT("NavmeshScale")))
		{
			UNavigationSystem::UpdateNavOctreeBounds(this);
			UNavigationSystem::UpdateActorAndComponentsInNavOctree(*this);
		}
	}
#endif

protected:
	/** Component to test for encroachment */
	UPROPERTY(BlueprintReadOnly, Category = "Lift")
	UPrimitiveComponent* EncroachComponent;

	/** Used during lift move to identify if movement was incomplete */
	UPROPERTY()
	bool bMoveWasBlocked;

	/** Where lift started for current move. */
	UPROPERTY()
	FVector LiftStartLocation;

	/** Where lift wants to end for current move. */
	UPROPERTY()
	FVector LiftEndLocation;

	/**  Where lift was at end of last tick */
	UPROPERTY()
	FVector TickLocation;
};
