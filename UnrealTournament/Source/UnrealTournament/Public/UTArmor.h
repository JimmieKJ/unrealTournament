// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "UTArmor.generated.h"

UCLASS(Blueprintable, Abstract, notplaceable, meta = (ChildCanTick))
class UNREALTOURNAMENT_API AUTArmor : public AUTInventory
{
	GENERATED_UCLASS_BODY()

	/** armor amount remaining */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Armor)
	int32 ArmorAmount;

	/** percentage of incoming damage redirected to armor */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Armor, Meta = (UIMin = 0.0, UIMax = 1.0))
	float AbsorptionPct;

	/** whether to also absorb momentum by the same percentage that damage is absorbed */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Armor)
	bool bAbsorbMomentum;

	/** whether to destroy armor when its remaining amount hits zero (would want to set false if it can be regenerated) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Armor)
	bool bDestroyWhenConsumed;

	/** character overlay applied while this armor is equipped */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Effects)
	FOverlayEffect OverlayEffect;
	UPROPERTY(VisibleDefaultsOnly, Category = Effects)
	UMaterialInterface* OverlayMaterial;

	/** Hold a descriptive tag that describes what type of armor this is. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Armor)
	FName ArmorType;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Powerup)
	FName StatsName;

	/** Effect to spawn on armor hit. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Effects)
	UParticleSystem* ArmorImpactEffect;

	virtual void AddOverlayMaterials_Implementation(AUTGameState* GS) const override
	{
		if (OverlayEffect.IsValid())
		{
			GS->AddOverlayEffect(OverlayEffect);
		}
		else
		{
			GS->AddOverlayEffect(FOverlayEffect(OverlayMaterial));
		}
	}

	virtual bool AllowPickupBy(AUTCharacter* Other) const override;

	/** Handles any C++ generated effects, calls blueprint PlayArmorEffects */
	bool HandleArmorEffects(AUTCharacter* HitPawn) const override;

	virtual void GivenTo(AUTCharacter* NewOwner, bool bAutoActivate) override;
	virtual void Removed() override;

	virtual bool HandleGivenTo_Implementation(AUTCharacter* Character) override;

	virtual float BotDesireability_Implementation(APawn* Asker, AActor* Pickup, float PathDistance) const override;
	virtual float DetourWeight_Implementation(APawn* Asker, AActor* Pickup, float PathDistance) const override
	{
		// don't detour too far out of way for low impact armor
		return (BasePickupDesireability >= 1.0f || PathDistance < 2000.0f) ? BotDesireability(Asker, Pickup, PathDistance) : 0.0f;
	}
};