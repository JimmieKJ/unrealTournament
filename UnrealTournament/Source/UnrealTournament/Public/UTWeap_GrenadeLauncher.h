// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTWeapon.h"
#include "UTWeap_RocketLauncher.h"
#include "UTProj_Grenade_Sticky.h"
#include "UTWeap_GrenadeLauncher.generated.h"

UCLASS(abstract)
class UNREALTOURNAMENT_API AUTWeap_GrenadeLauncher : public AUTWeapon
{
	GENERATED_BODY()

private:
	
	UPROPERTY()
	float LastGrenadeFireTime;

	UPROPERTY()
	TArray<AUTProj_Grenade_Sticky*> ActiveGrenades;

	UFUNCTION()
	void OnRep_HasStickyGrenades();

	UPROPERTY(replicatedUsing=OnRep_HasStickyGrenades)
	bool bHasStickyGrenades;
	
	UPROPERTY()
	bool bFiringStickyGrenade;

public:
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = GrenadeLauncher)
	float DetonationAfterFireDelay;

	AUTWeap_GrenadeLauncher();

	virtual bool BeginFiringSequence(uint8 FireModeNum, bool bClientFired) override;
	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void ConsumeAmmo(uint8 FireModeNum) override;

	void RegisterStickyGrenade(AUTProj_Grenade_Sticky* InGrenade);
	void UnregisterStickyGrenade(AUTProj_Grenade_Sticky* InGrenade);

	void DetonateStickyGrenades();
		
	UFUNCTION(BlueprintImplementableEvent, Category=GrenadeLauncher)
	void PlayDetonationEffects();

	UFUNCTION(BlueprintImplementableEvent, Category=GrenadeLauncher)
	void ShowDetonatorUI();

	UFUNCTION(BlueprintImplementableEvent, Category = GrenadeLauncher)
	void HideDetonatorUI();

	UPROPERTY(replicated, BlueprintReadOnly, Category = GrenadeLauncher)
	int32 ActiveStickyGrenadeCount;
	
	void BringUp(float OverflowTime) override;
	bool PutDown() override;
	void Destroyed() override;
	void DetachFromOwner_Implementation() override;

	void DropFrom(const FVector& StartLocation, const FVector& TossVelocity) override;
	void OnRep_Ammo() override;
	bool CanSwitchTo() override;
};