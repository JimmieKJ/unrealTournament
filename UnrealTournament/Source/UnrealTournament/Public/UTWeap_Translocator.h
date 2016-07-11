// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTWeapon.h"
#include "UTWeaponRedirector.h"
#include "UTWeap_Translocator.generated.h"

class AUTProj_TransDisk;
UCLASS(abstract)
class UNREALTOURNAMENT_API AUTWeap_Translocator : public AUTWeapon
{
	GENERATED_UCLASS_BODY()

	virtual void FireShot();

	UPROPERTY(BlueprintReadOnly, Replicated, ReplicatedUsing = OnRep_TransDisk, Category = Translocator)
	AUTProj_TransDisk* TransDisk;

	/** Fake disk spawned on clients, */
	UPROPERTY(BlueprintReadOnly, Category = Translocator)
		AUTProj_TransDisk* FakeTransDisk;

	UFUNCTION()
	virtual void OnRep_TransDisk();

	UPROPERTY()
	bool bHaveDisk;

	virtual void RecallDisk();

	virtual void ClearDisk();

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Translocator)
	USoundBase* ThrowSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Translocator)
	USoundBase* RecallSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Translocator)
	USoundBase* TeleSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Translocator)
	USoundBase* DisruptedSound;

	/** alternate (usually shorter) refire delay on the disk recall */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Translocator)
	float RecallFireInterval;

	/** alternate shorter refire delay for transloc when haven't recently translocated */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Translocator)
		float FirstFireInterval;

	/** alternate shorter refire delay for transloc when haven't recently translocated */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Translocator)
		float LastTranslocTime;

	/** Minium time since last translocated to get FirstFireInterval. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Translocator)
		float MinFastTranslocInterval;

	virtual FRotator GetAdjustedAim_Implementation(FVector StartFireLoc) override;

	/** Adjust pitch of projectile shot by this amount of degrees (scaled down as player aims up). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Translocator)
		float ShotPitchUp;

	/** Adjust pitch of projectile shot by this amount of degrees (scaled down as player aims up). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Translocator)
		float DiskGravity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Telefrag)
	float TelefragDamage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Telefrag)
	TSubclassOf<UDamageType> TelefragDamageType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Telefrag)
	TSubclassOf<UDamageType> TransFailDamageType;	

	/** optional class spawned at source location after translocating that continues to receive damage (and possible teleport weapons fire with the player) for a short duration */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<AUTWeaponRedirector> AfterImageType;
	/** effect played at translocation destination */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<class AUTReplicatedEmitter> DestinationEffect;

	/** Messages about translocator functionality, disruption, etc. */
	UPROPERTY(EditDefaultsOnly, Category = Translocator)
	TSubclassOf<class UUTLocalMessage> TranslocatorMessageClass;
	
	virtual void ConsumeAmmo(uint8 FireModeNum) override;
	virtual bool HasAmmo(uint8 FireModeNum) override
	{
		return true;
	}

	virtual bool HasAnyAmmo() override
	{
		// return true since even if currently zero we'll shortly recharge more
		return true;
	}

	virtual bool NeedsAmmoDisplay_Implementation() const override
	{
		return false;
	}

	virtual bool ShouldDropOnDeath() override;
	virtual void DropFrom(const FVector& StartLocation, const FVector& TossVelocity) override;

	virtual void Tick(float DeltaTime) override;
	virtual void GivenTo(AUTCharacter* NewOwner, bool bAutoActivate) override;
	virtual float GetAISelectRating_Implementation() override;
	virtual bool DoAssistedJump() override;
	virtual bool CanAttack_Implementation(AActor* Target, const FVector& TargetLoc, bool bDirectOnly, bool bPreferCurrentMode, uint8& BestFireMode, FVector& OptimalTargetLoc) override;
	virtual bool HUDShouldRender_Implementation(UUTHUDWidget* TargetWidget) override;
	virtual void UpdateHUDText() override;
	virtual void PostInitProperties() override;
	virtual void GuessPlayerTarget(const FVector& StartFireLoc, const FVector& FireDir) override;
};
