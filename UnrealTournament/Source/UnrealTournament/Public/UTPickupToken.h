// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTPickupToken.generated.h"

UCLASS(Blueprintable, Abstract)
class UNREALTOURNAMENT_API AUTPickupToken : public AActor
{
	GENERATED_UCLASS_BODY()

	virtual void PostLoad() override;

#if WITH_EDITOR
	virtual void PostActorCreated() override;
	virtual void PostDuplicate(bool bDuplicateForPIE) override;
	virtual void CheckForErrors() override;
#endif

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = Pickup)
	FName TokenUniqueID;

	UFUNCTION(BlueprintCallable, Category = Pickup)
	bool HasBeenPickedUpBefore();

	UFUNCTION(BlueprintCallable, Category = Pickup)
	void PickedUp();

	UFUNCTION(BlueprintCallable, Category = Pickup)
	void Revoke();
};