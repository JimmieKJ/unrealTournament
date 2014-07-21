

#pragma once

#include "UTProjectile.h"
#include "UTProj_TransDisk.generated.h"

UENUM()
enum ETLState
{
	TLS_None,
	TLS_InAir,
	TLS_OnGround,
	TLS_Disrupted
};


class AUTWeap_Translocator;
/**
 * 
 */
UCLASS()
class AUTProj_TransDisk : public AUTProjectile
{
	GENERATED_UCLASS_BODY()

	virtual void ProcessHit_Implementation(AActor* OtherActor, UPrimitiveComponent* OtherComp, const FVector& HitLocation, const FVector& HitNormal);

	virtual void OnStop(const FHitResult& Hit);

	/** hook to spawn effects when the glob lands*/
	UFUNCTION(BlueprintNativeEvent, Category = TransDisk)
	void OnLanded();
	/** hook to spawn effects when the glob lands*/
	UFUNCTION(BlueprintNativeEvent, Category = TransDisk)
	void OnDisrupted();
	
	virtual void ShutDown();

	UPROPERTY(BlueprintReadOnly, Replicated, Category = Translocator)
	AUTWeap_Translocator* MyTranslocator;

	UPROPERTY(BlueprintReadOnly, Replicated, ReplicatedUsing = OnRep_TransState, Category = TransDisk)
	TEnumAsByte<ETLState> TransState;
	UFUNCTION()
	virtual void OnRep_TransState();

	/**The effect played when the Disk lands*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = TransDisk)
	UParticleSystem* LandedEffect;

	/**Coppied from UProjectileMovementComponent*/
	FVector ComputeBounceResult(const FHitResult& Hit, float TimeSlice, const FVector& MoveDelta);


	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, class AActor* DamageCauser);
};
