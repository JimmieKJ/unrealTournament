// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTProj_BioShot.h"

#include "UTProj_BioLauncherShot.generated.h"

UCLASS()
class AUTProj_BioLauncherShot : public AUTProj_BioShot
{
	GENERATED_BODY()
public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	int32 WebHealth;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float MinWebStringDistance;
	/** maximum web length when at full glob strength */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float MaxWebReach;
	/** amount to pull back web traces along hit normal so they don't get stuck */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float WebTraceOriginOffset;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	UStaticMesh* WebStringMesh;

	/** used to spawn the web perpendicular to the shooter */
	UPROPERTY(BlueprintReadWrite)
	FVector WebReferenceAxis;
	UPROPERTY(BlueprintReadWrite, Replicated)
	TArray<FVector> WebHitLocations;
	UPROPERTY(BlueprintReadWrite, ReplicatedUsing = SpawnWebEffects)
	bool bSpawnedWeb;
	UPROPERTY(BlueprintReadWrite)
	TArray<UMaterialInstanceDynamic*> WebMIDs;
	UPROPERTY(BlueprintReadWrite, ReplicatedUsing = PlayWebHitFlash)
	uint8 WebHitFlashCount;
	UPROPERTY(BlueprintReadOnly)
	float HitFlashStartTime;

	AUTProj_BioLauncherShot(const FObjectInitializer& OI);

	virtual void BeginPlay() override;
	virtual void SetGlobStrength(float NewStrength) override;
	virtual void Landed(UPrimitiveComponent* HitComp, const FVector& HitLocation) override;
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, class AActor* DamageCauser) override;
	virtual void PreReplication(IRepChangedPropertyTracker& ChangedPropertyTracker) override;
	virtual void Tick(float DeltaTime) override;

	virtual void SpawnWeb(FVector HitNormal);
	UFUNCTION()
	virtual void SpawnWebEffects();
	UFUNCTION()
	virtual void PlayWebHitFlash();
	UFUNCTION()
	virtual void OnWebOverlapBegin(AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	virtual void Track(AUTCharacter* NewTrackedPawn) override
	{}
};