// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTCTFFlag.h"
#include "UTCTFFlagBase.generated.h"

UCLASS()
class AUTCTFFlagBase : public AUTGameObjective
{
	GENERATED_UCLASS_BODY()

	// Holds a reference to the flag
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category = GameObject)
	AUTCTFFlag* MyFlag;

	// The mesh that makes up this base.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = GameObject)
	TSubobjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Sound)
		USoundBase* FlagScoreRewardSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Sound)
		USoundBase* FlagTakenSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Sound)
		USoundBase* FlagReturnedSound;

	virtual FName GetFlagState();
	virtual void RecallFlag();

	virtual void ObjectWasPickedUp(AUTCharacter* NewHolder, bool bWasHome) override;
	virtual void ObjectReturnedHome(AUTCharacter* Returner) override;

protected:
	virtual void CreateCarriedObject();
};