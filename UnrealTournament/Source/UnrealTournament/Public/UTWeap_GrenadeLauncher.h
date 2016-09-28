// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTWeapon.h"
#include "UTWeap_RocketLauncher.h"
#include "UTProj_Grenade_Sticky.h"
#include "UTWeap_GrenadeLauncher.generated.h"

UCLASS(abstract)
class UNREALTOURNAMENT_API AUTWeap_GrenadeLauncher : public AUTWeap_RocketLauncher
{
	GENERATED_BODY()

private:

	UPROPERTY()
	TArray<AUTProj_Grenade_Sticky*> ActiveGrenades;

	UPROPERTY(replicated)
	bool bHasStickyGrenades;

	UPROPERTY(replicated)
	int32 ActiveStickyGrenadeCount;

public:
	AUTWeap_GrenadeLauncher();

	virtual bool BeginFiringSequence(uint8 FireModeNum, bool bClientFired) override;
	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;

	void RegisterStickyGrenade(AUTProj_Grenade_Sticky* InGrenade);
	void UnregisterStickyGrenade(AUTProj_Grenade_Sticky* InGrenade);

	void DetonateStickyGrenades();

	// Can be removed when we don't parent from rocket launcher anymore
	bool CanLockTarget(AActor *Target)
	{
		return false;
	}
};