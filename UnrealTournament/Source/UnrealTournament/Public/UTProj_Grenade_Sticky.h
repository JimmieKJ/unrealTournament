// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTProjectile.h"
#include "UTProj_Grenade_Sticky.generated.h"


UCLASS()
class UNREALTOURNAMENT_API AUTProj_Grenade_Sticky : public AUTProjectile
{
	GENERATED_UCLASS_BODY()
			
public:
	UFUNCTION(BlueprintCallable, Category = Projectile)
	uint8 GetInstigatorTeamNum();

	UPROPERTY(BlueprintReadOnly, Category = Projectile)
	class AUTWeap_GrenadeLauncher* GrenadeLauncherOwner;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Projectile)
	float LifeTime;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Projectile)
	float MinimumLifeTime;

	UPROPERTY(BlueprintReadOnly, Category=Projectile)
	bool bArmed;

	FTimerHandle FLifeTimeHandle;
	FTimerHandle FArmedHandle;

	virtual void BeginPlay() override;
	virtual void ShutDown() override;
	virtual void Destroyed() override;
	virtual	void Explode_Implementation(const FVector& HitLocation, const FVector& HitNormal, UPrimitiveComponent* HitComp) override;

	UFUNCTION()
	void ExplodeDueToTimeout();

	UFUNCTION(BlueprintCallable, Category = Projectile)
	void ArmGrenade();

protected:

	UPROPERTY()
	AUTProjectile* SavedFakeProjectile;

};
