#pragma once

#include "UTProjectile.h"
#include "UTMovementBaseInterface.h"
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
class UNREALTOURNAMENT_API AUTProj_TransDisk : public AUTProjectile, public IUTMovementBaseInterface
{
	GENERATED_UCLASS_BODY()

	virtual void ProcessHit_Implementation(AActor* OtherActor, UPrimitiveComponent* OtherComp, const FVector& HitLocation, const FVector& HitNormal);

	virtual void OnStop(const FHitResult& Hit);

	UFUNCTION()
	virtual void OnBlockingHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	/** Transdisk replaces fake projectile instead of using it */
	virtual void BeginFakeProjectileSynch(AUTProjectile* InFakeProjectile) override;

	virtual void InitFakeProjectile(AUTPlayerController* OwningPlayer) override;

	// UTMovementBaseInterface
	virtual	void RemoveBasedCharacter_Implementation(class AUTCharacter* BasedCharacter) {};
	virtual void AddBasedCharacter_Implementation(class AUTCharacter* BasedCharacter) override;

	// IUTMovementBaseInterface
	virtual void AddBasedCharacterNative(class AUTCharacter* BasedCharacter);

	/** hook to spawn effects when the trans disk lands */
	UFUNCTION(BlueprintNativeEvent, Category=TransDisk)
	void OnLanded();

	/** hook to spawn effects when the trans disk is destroyed */
	UFUNCTION(BlueprintNativeEvent, Category=TransDisk)
	void OnDisrupted();
	
	UFUNCTION(BlueprintNativeEvent, Category=TransDisk)
	void ModifyTakeDamage(UPARAM(ref) float &DamageAmount, struct FDamageEvent const &DamageEvent, class AController* EventInstigator, AActor* DamageCauser);

	virtual void Recall();
	virtual void ShutDown();
	virtual void FellOutOfWorld(const UDamageType& dmgType) override;

	UPROPERTY(BlueprintReadOnly, Replicated, Category=TransDisk)
	AUTWeap_Translocator* MyTranslocator;

	/** controller who disrupted the disk (and thus should gain kill credit) */
	UPROPERTY(BlueprintReadOnly, Category = Translocator)
	AController* DisruptedController;

	UPROPERTY(BlueprintReadOnly, Replicated, ReplicatedUsing = OnRep_TransState, Category = TransDisk)
	TEnumAsByte<ETLState> TransState;

	UFUNCTION()
	virtual void OnRep_TransState();

	virtual UParticleSystemComponent* SpawnOffsetEffect(UParticleSystem *Effect, const FVector& Offset);

	/**The effect played when the Disk lands*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=TransDisk)
	TArray<UParticleSystem*> LandedEffect;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = TransDisk)
	UParticleSystemComponent* LandedBeaconComponent;

	/**The effect played when the Disk is disrupted*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = TransDisk)
		UParticleSystem* DisruptedEffect;

	/** Max speed while underwater */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = TransDisk)
	float MaxSpeedUnderWater;

	/** How long after disrupted to destroy this disk */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = TransDisk)
	float DisruptDestroyTime;

	/** Health remaining before disk gets disrupted/ */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = TransDisk)
	int32 RemainingHealth;

	UPROPERTY(BlueprintReadWrite, Category = TransDisk)
		bool bCanShieldBounce;

	FVector ComputeBounceResult(const FHitResult& Hit, float TimeSlice, const FVector& MoveDelta);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=TransDisk)
	UStaticMeshComponent* DiskMesh;

	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, class AActor* DamageCauser);

	virtual void Tick(float DeltaTime) override;

	/** used for bot translocations to check that translocating to our current position allows reaching DesiredDest and is a safe target (e.g. not going to fall to death, etc) */
	virtual bool IsAcceptableTranslocationTo(const FVector& DesiredDest);

	virtual void OnBounce(const struct FHitResult& ImpactResult, const FVector& ImpactVelocity) override;
	virtual bool CanMatchFake(AUTProjectile* InFakeProjectile, const FVector& VelDir) const override;

protected:
	/** utility to trigger bot translocation and reset translocation related data */
	virtual void BotTranslocate();
};
