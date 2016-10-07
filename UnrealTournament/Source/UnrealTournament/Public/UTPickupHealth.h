// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTPickupHealth.generated.h"

UCLASS(Blueprintable, Abstract)
class UNREALTOURNAMENT_API AUTPickupHealth : public AUTPickup
{
	GENERATED_UCLASS_BODY()

	/** amount of health to restore */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Pickup)
	int32 HealAmount;
	/** if true, heal amount goes to SuperHealthMax instead of HealthMax */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Pickup)
	bool bSuperHeal;

	/** return the upper limit this pickup can increase the character's health to */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = Pickup)
	int32 GetHealMax(AUTCharacter* P);

	/**The stat for how many times this was pickup up*/
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Pickup)
	FName StatsNameCount;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadWrite, Category = Pickup)
		UMeshComponent* Mesh;

	/** copy of mesh displayed when inventory is not available */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadWrite, Category = Pickup)
		UMeshComponent* GhostMesh;

	/** material to be set on GhostMesh */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = PickupDisplay)
		UMaterialInterface* GhostMeshMaterial;

	virtual bool AllowPickupBy_Implementation(APawn* TouchedBy, bool bDefaultAllowPickup) override;
	virtual void GiveTo_Implementation(APawn* Target) override;
	virtual void BeginPlay() override;
	virtual void SetPickupHidden(bool bNowHidden) override;

	virtual float BotDesireability_Implementation(APawn* Asker, AController* RequestOwner, float PathDistance) override;
	virtual float DetourWeight_Implementation(APawn* Asker, float PathDistance) override;
	virtual bool IsSuperDesireable_Implementation(AController* RequestOwner, float CalculatedDesire) override
	{
		return BaseDesireability >= 1.0f;
	}
};