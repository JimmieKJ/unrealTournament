// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTCTFFlag.h"
#include "UTCTFFlagBase.generated.h"

UCLASS(HideCategories = GameObject)
class UNREALTOURNAMENT_API AUTCTFFlagBase : public AUTGameObjective
{
	GENERATED_UCLASS_BODY()

	// Holds a reference to the flag
	UPROPERTY(BlueprintReadOnly, Replicated, Category = Flag)
	AUTCTFFlag* MyFlag;

	// capsule for collision for detecting flag caps
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = Objective)
	UCapsuleComponent* Capsule;

	// The mesh that makes up this base.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Objective)
	UStaticMeshComponent* Mesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Sound)
	USoundBase* FlagScoreRewardSound;

	/** Own flag taken, play unattenuated sound for all players on team. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Sound)
	USoundBase* FlagTakenSound;

	/** Enemy flag taken, play attenuated sound. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Sound)
	USoundBase* EnemyFlagTakenSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Sound)
	USoundBase* FlagReturnedSound;

	/** array of flag classes by team */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Flag)
	TArray< TSubclassOf<AUTCTFFlag> > TeamFlagTypes;

	/** Adjustment to number of lives available to team with this base in round based CTF. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Objective)
		int32 RoundLivesAdjustment;

	virtual FName GetFlagState();
	virtual void RecallFlag();

	virtual void ObjectWasPickedUp(AUTCharacter* NewHolder, bool bWasHome) override;
	virtual void ObjectReturnedHome(AUTCharacter* Returner) override;

	UFUNCTION()
	virtual void OnOverlapBegin(AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

protected:
	virtual void CreateCarriedObject();
};