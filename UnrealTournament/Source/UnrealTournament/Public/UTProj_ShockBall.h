// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTProjectile.h"
#include "UTProj_ShockBall.generated.h"

UCLASS(Abstract)
class AUTProj_ShockBall : public AUTProjectile
{
	GENERATED_UCLASS_BODY()

	virtual void InitFakeProjectile(AUTPlayerController* OwningPlayer) override;
	virtual void NotifyClientSideHit(AUTPlayerController* InstigatedBy, FVector HitLocation, AActor* DamageCauser) override;

	/** combo parameters */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ShockCombo)
	FRadialDamageParams ComboDamageParams;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ShockCombo)
	float ComboMomentum;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ShockCombo)
	TSubclassOf<UUTDamageType> ComboDamageType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ShockCombo)
	TSubclassOf<UUTDamageType> ComboTriggerType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ShockCombo)
	int32 ComboAmmoCost;

	/** Effects */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Projectile)
	TSubclassOf<AUTImpactEffect> ComboExplosionEffects;

	UPROPERTY(Replicated, ReplicatedUsing = OnRep_ComboExplosion)
	bool bComboExplosion;
	UFUNCTION()
	void OnRep_ComboExplosion();

	/** hook to do some additional shock combo stuff; called on both client and server */
	UFUNCTION(BlueprintNativeEvent, Category = Damage)
	void OnComboExplode();

	/** Overridden to do the combo */
	virtual void ReceiveAnyDamage(float Damage, const class UDamageType* DamageType, class AController* InstigatedBy, class AActor* DamageCauser) override;

	virtual void PerformCombo(class AController* InstigatedBy, class AActor* DamageCauser);

	virtual void Explode_Implementation(const FVector& HitLocation, const FVector& HitNormal, UPrimitiveComponent* HitComp = NULL);

	virtual float GetMaxDamageRadius_Implementation() const override
	{
		return FMath::Max<float>(DamageParams.OuterRadius, ComboDamageParams.OuterRadius);
	}
};
