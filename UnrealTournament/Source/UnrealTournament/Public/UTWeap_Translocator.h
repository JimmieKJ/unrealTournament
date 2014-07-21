

#pragma once

#include "UTWeapon.h"
#include "UTWeap_Translocator.generated.h"

class AUTProj_TransDisk;



/**
 * 
 */
UCLASS()
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


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Telefrag)
	float TelefragDamage;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Telefrag)
	TSubclassOf<UDamageType> TelefragDamageType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Telefrag)
	TSubclassOf<UDamageType> TransFailDamageType;	


	/**Dont drop Weapon when killed. Kill the disk*/
	virtual void DropFrom(const FVector& StartLocation, const FVector& TossVelocity) override;
};
