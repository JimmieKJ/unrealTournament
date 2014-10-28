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

UCLASS()
class AUTProj_TransDisk : public AUTProjectile
{
	GENERATED_UCLASS_BODY()

	virtual void ProcessHit_Implementation(AActor* OtherActor, UPrimitiveComponent* OtherComp, const FVector& HitLocation, const FVector& HitNormal);

	virtual void OnStop(const FHitResult& Hit);

	/** Transdisk replaces fake projectile instead of using it */
	virtual void BeginFakeProjectileSynch(AUTProjectile* InFakeProjectile) override;

	virtual void InitFakeProjectile(AUTPlayerController* OwningPlayer) override;

	/** hook to spawn effects when the glob lands*/
	UFUNCTION(BlueprintNativeEvent, Category=TransDisk)
	void OnLanded();

	/** hook to spawn effects when the glob lands*/
	UFUNCTION(BlueprintNativeEvent, Category=TransDisk)
	void OnDisrupted();
	
	virtual void ShutDown();

	UPROPERTY(BlueprintReadOnly, Replicated, Category=TransDisk)
	AUTWeap_Translocator* MyTranslocator;

	/** controller who disrupted the disk (and thus should gain kill credit) */
	UPROPERTY(BlueprintReadOnly, Category = Translocator)
	AController* DisruptedController;

	UPROPERTY(BlueprintReadOnly, Replicated, ReplicatedUsing = OnRep_TransState, Category = TransDisk)
	TEnumAsByte<ETLState> TransState;

	UFUNCTION()
	virtual void OnRep_TransState();

	/**The effect played when the Disk lands*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=TransDisk)
	UParticleSystem* LandedEffect;

	FVector ComputeBounceResult(const FHitResult& Hit, float TimeSlice, const FVector& MoveDelta);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=TransDisk)
	UStaticMeshComponent* DiskMesh;

	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, class AActor* DamageCauser);

	virtual void Tick(float DeltaTime) override;
};
