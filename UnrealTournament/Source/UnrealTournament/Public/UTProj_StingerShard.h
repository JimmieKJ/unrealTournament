// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTProjectile.h"
#include "UTMovementBaseInterface.h"
#include "UTProj_StingerShard.generated.h"

UCLASS()
class AUTProj_StingerShard : public AUTProjectile, public IUTMovementBaseInterface
{
	GENERATED_UCLASS_BODY()

	/**Overridden to do the stick*/
	virtual void ProcessHit_Implementation(AActor* OtherActor, UPrimitiveComponent* OtherComp, const FVector& HitLocation, const FVector& HitNormal) override;

	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, class AActor* DamageCauser) override;

	virtual void Destroyed() override;

	// UTMovementBaseInterface
	virtual void AddBasedCharacter_Implementation(class AUTCharacter* BasedCharacter) {};
	virtual void RemoveBasedCharacter_Implementation(class AUTCharacter* BasedCharacter) override;

	/** Called when UTCharacter jumps off me. */
	virtual void RemoveBasedCharacterNative(class AUTCharacter* UTChar);

	/** Damage taken by player jumping off impacted shard. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Shard)
	int32 JumpOffDamage;

	/** Normal of wall this shard impacted on. */
	UPROPERTY()
		FVector ImpactNormal;

	/** Visible static mesh - will collide when shard sticks. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Effects)
		UStaticMeshComponent* ShardMesh;
};