// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

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

	// The mesh that makes up this base.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Objective)
	UStaticMeshComponent* Mesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Sound)
	USoundBase* FlagScoreRewardSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Sound)
	USoundBase* FlagTakenSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Sound)
	USoundBase* FlagReturnedSound;

	/** array of flag classes by team */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Flag)
	TArray< TSubclassOf<AUTCTFFlag> > TeamFlagTypes;

	virtual FName GetFlagState();
	virtual void RecallFlag();

	virtual void ObjectWasPickedUp(AUTCharacter* NewHolder, bool bWasHome) override;
	virtual void ObjectReturnedHome(AUTCharacter* Returner) override;

protected:
	virtual void CreateCarriedObject();
};