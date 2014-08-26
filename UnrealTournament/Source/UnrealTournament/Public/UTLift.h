// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTLift.generated.h"

UCLASS(BlueprintType, Blueprintable)
class AUTLift : public AActor
{
	GENERATED_UCLASS_BODY()

	UFUNCTION()
	virtual void OnOverlapBegin(AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	/** Event when encroach an actor (Overlap that can't be handled gracefully) */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Lift")
	virtual void SetEncroachComponent(class UPrimitiveComponent* NewEncroachComponent);

	/** Event when encroach an actor (Overlap that can't be handled gracefully) */
	UFUNCTION(BlueprintImplementableEvent, BlueprintAuthorityOnly, Category = "Lift")
	virtual void OnEncroachActor(class AActor* EncroachedActor);

	virtual void ReceiveHit(class UPrimitiveComponent* MyComp, class AActor* Other, class UPrimitiveComponent* OtherComp, bool bSelfMoved, FVector HitLocation, FVector HitNormal, FVector NormalImpulse, const FHitResult& Hit) override;

	/** Move the colliding lift mesh component */
	UFUNCTION(BlueprintCallable, Category = "Lift")
	virtual void MoveLiftTo(FVector NewLocation, FRotator NewRotation);

	UPROPERTY()
		float LastEncroachNotifyTime;

	/** Event when a player starts standing on me */
	UFUNCTION(BlueprintImplementableEvent, BlueprintAuthorityOnly, Category = "Lift")
		virtual void AddBasedCharacter(APawn* NewBasedPawn);

private:
	/** Component to test for encroachment */
	UPROPERTY()
	UPrimitiveComponent* EncroachComponent;
};