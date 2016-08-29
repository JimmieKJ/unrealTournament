// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTIntermissionBeginInterface.h"
#include "UTResetInterface.h"

#include "UTRepulsorBubble.generated.h"

UENUM()
enum class RepulsorLastHitType: uint8
{
	None,
	Projectile,
	Hitscan,
	Character
};


UCLASS(Blueprintable, Abstract)
class UNREALTOURNAMENT_API AUTRepulsorBubble : public AActor , public IUTResetInterface , public IUTIntermissionBeginInterface
{

	GENERATED_UCLASS_BODY()

public:

	/** Called when our local player has a new view target */
	UFUNCTION(BlueprintCallable, Category = Repulsor)
	virtual void OnViewTargetChange(AUTPlayerController* NewViewTarget);

	UPROPERTY(EditAnywhere, Replicated, BlueprintReadOnly, Category = Team)
	uint8 TeamNum;

	/** Used to prevent chain bouncing someone in rapid succesion*/
	UPROPERTY(BlueprintReadOnly, Category = Repulsor)
	TArray<AUTCharacter*> RecentlyBouncedCharacters;

	UPROPERTY(EditDefaultsOnly, Category = Repulsor)
	float RecentlyBouncedResetTime;

	FTimerHandle RecentlyBouncedClearTimerHandle;

	UPROPERTY(EditDefaultsOnly, Category = Repulsor)
	bool bShouldIgnoreTeamProjectiles;

	UPROPERTY(EditDefaultsOnly, Category = Repulsor)
	bool bShouldIgnoreTeamCharacters;

	UPROPERTY(EditDefaultsOnly, Category = Repulsor)
	bool bShouldReflectProjectiles;

	UPROPERTY(EditDefaultsOnly, Category = Repulsor)
	float MaxRepulsorSize;

	UPROPERTY(EditDefaultsOnly, Category = Repulsor)
	float MinRepulsorSize;

	UPROPERTY(EditDefaultsOnly, Category = Repulsor)
	FVector KnockbackVector;

	UPROPERTY(EditDefaultsOnly, Category = Repulsor)
	float KnockbackStrength;
	
	/** Amount of damage the repulsor can absorb/reflect before being destroyed */
	UPROPERTY(BlueprintReadWrite, Replicated, Category = Repulsor, ReplicatedUsing = OnRep_Health)
	float Health;
	
	UPROPERTY(EditDefaultsOnly, Category = Repulsor)
	float MaxHealth;

	UPROPERTY(EditDefaultsOnly, Category = Repulsor)
	float HealthLostToRepulsePlayer;

	UPROPERTY(EditDefaultsOnly, Category = Repulsor)
	TSubclassOf<AUTInventory> ParentInventoryItemClass;

	UPROPERTY(EditDefaultsOnly, Category = Repulsor)
	UMaterial* BubbleMaterial;

	UPROPERTY(BlueprintReadOnly, Category = Repulsor)
	UMaterialInstanceDynamic* MID_Bubble;

	UPROPERTY(EditDefaultsOnly, Category = Repulsor)
	UStaticMeshComponent* BubbleMesh;

	UPROPERTY(EditDefaultsOnly, Category = Repulsor)
	FLinearColor RedTeamColor;

	UPROPERTY(EditDefaultsOnly, Category = Repulsor)
	FLinearColor BlueTeamColor;

	/** Sphere collision component */
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = Projectile)
	USphereComponent* CollisionComp;

	UFUNCTION()
	virtual void BeginPlay() override;

	UFUNCTION()
	virtual void OnOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	/** called when repulsor bubble hits something */
	UFUNCTION(BlueprintCallable, Category = Bubble)
	void ProcessHit(AActor* OtherActor, UPrimitiveComponent* OtherComp, const FVector& HitLocation, const FVector& HitNormal);

	virtual float TakeDamage(float Damage, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;
	virtual float TakeDamage(float Damage, AActor* DamageCauser);

	UFUNCTION()
	bool ShouldInteractWithActor(AActor* OtherActor);

	virtual void Destroyed() override;

	UFUNCTION(BlueprintNativeEvent)
	void OnCharacterBounce();

	UFUNCTION(BlueprintNativeEvent)
	void OnProjectileHit();

	UFUNCTION(BlueprintNativeEvent)
	void OnHitScanBlocked();

	virtual void Reset_Implementation() override;
	virtual void IntermissionBegin_Implementation() override;
	
	UFUNCTION()
	virtual void OnRep_Health();

protected:

	UFUNCTION(BlueprintCallable, Category = Bubble)
	void ProcessHitPlayer(AUTCharacter* OtherPlayer, UPrimitiveComponent* OtherComp, const FVector& HitLocation, const FVector& HitNormal);

	UFUNCTION(BlueprintCallable, Category = Bubble)
	void ProcessHitProjectile(AUTProjectile* OtherProj, UPrimitiveComponent* OtherComp, const FVector& HitLocation, const FVector& HitNormal);

	virtual void PostInitializeComponents() override;

	void ClearRecentlyBouncedPlayers();

	UPROPERTY(Replicated)
	RepulsorLastHitType LastHitByType;
};