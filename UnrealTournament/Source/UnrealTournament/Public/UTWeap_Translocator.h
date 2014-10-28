

#pragma once

#include "UTWeapon.h"
#include "UTWeap_Translocator.generated.h"

class AUTProj_TransDisk;



/**
 * 
 */
UCLASS(abstract)
class AUTWeap_Translocator : public AUTWeapon
{
	GENERATED_UCLASS_BODY()

	virtual void FireShot();


	UPROPERTY(BlueprintReadOnly, Replicated, ReplicatedUsing = OnRep_TransDisk, Category = Translocator)
	AUTProj_TransDisk* TransDisk;

	UFUNCTION()
	virtual void OnRep_TransDisk();

	bool bHaveDisk;

	virtual void ClearDisk();


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Translocator)
	USoundBase* ThrowSound;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Translocator)
	USoundBase* RecallSound;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Translocator)
	USoundBase* TeleSound;

	/** recharge rate for ammo charges */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Translocator)
	float AmmoRechargeRate;

	/** alternate (usually shorter) refire delay on the disk recall */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Translocator)
	float RecallFireInterval;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Telefrag)
	float TelefragDamage;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Telefrag)
	TSubclassOf<UDamageType> TelefragDamageType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Telefrag)
	TSubclassOf<UDamageType> TransFailDamageType;	

	virtual void ConsumeAmmo(uint8 FireModeNum) override;
	virtual bool HasAmmo(uint8 FireModeNum) override
	{
		// neither mode can fire when there's no ammo regardless of cost
		return (Ammo > 0 && Super::HasAmmo(FireModeNum));
	}
	virtual bool HasAnyAmmo() override
	{
		// return true since even if currently zero we'll shortly recharge more
		return true;
	}
	virtual void OnRep_Ammo() override;

	UFUNCTION()
	virtual void RechargeTimer();

	/**Dont drop Weapon when killed. Kill the disk*/
	virtual void DropFrom(const FVector& StartLocation, const FVector& TossVelocity) override;

	virtual void DrawWeaponInfo_Implementation(UUTHUDWidget* WeaponHudWidget, float RenderDelta) override;

	virtual void Tick(float DeltaTime) override;
	virtual void GivenTo(AUTCharacter* NewOwner, bool bAutoActivate) override;
	virtual float GetAISelectRating_Implementation() override;
	virtual bool DoAssistedJump() override;
};
