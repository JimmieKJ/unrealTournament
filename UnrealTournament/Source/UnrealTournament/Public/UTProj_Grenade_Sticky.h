// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTProjectile.h"
#include "UTProj_Grenade_Sticky.generated.h"


UCLASS()
class UNREALTOURNAMENT_API AUTProj_Grenade_Sticky : public AUTProjectile
{
	GENERATED_UCLASS_BODY()
			
public:
	UPROPERTY(BlueprintReadOnly, Category = Projectile)
	class AUTWeap_GrenadeLauncher* GrenadeLauncherOwner;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Projectile)
	float LifeTime;

	FTimerHandle FLifeTimeHandle;

	virtual void BeginPlay() override;
	virtual void Destroyed() override;

	UFUNCTION()
	void ExplodeDueToTimeout();

};
