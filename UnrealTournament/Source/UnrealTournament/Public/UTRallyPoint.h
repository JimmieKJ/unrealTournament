// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTRallyPoint.generated.h"

UCLASS(Blueprintable)
class UNREALTOURNAMENT_API AUTRallyPoint : public AUTGameObjective, public IUTResetInterface
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = Objective)
		UCapsuleComponent* Capsule;

	UPROPERTY(BlueprintReadOnly)
		class AUTGameVolume* MyGameVolume;

	UPROPERTY(BlueprintReadOnly)
		class AUTCharacter* NearbyFC;

	/** how long FC has to be here for rally to start */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Objective)
		float RallyReadyDelay;

	/** When rallypoint was powered up */
	UPROPERTY(BlueprintReadOnly, Category = Objective)
		float RallyStartTime;

	FTimerHandle EndRallyHandle;

	/** Minimum powered up time */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Objective)
		float MinimumRallyTime;

	UPROPERTY(BlueprintReadOnly, Category = Objective)
		float RallyReadyCountdown;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = Objective)
		int32 ReplicatedCountdown;

	UPROPERTY(ReplicatedUsing = OnAvailableEffectChanged, BlueprintReadOnly)
		bool bShowAvailableEffect;

	UPROPERTY()
		bool bHaveGameState;

	UPROPERTY(Replicated, BlueprintReadOnly)
		bool bIsEnabled;

	UPROPERTY(ReplicatedUsing = OnRallyChargingChanged, BlueprintReadOnly)
		FName RallyPointState;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Objective)
		UParticleSystem* AvailableEffect;

	UPROPERTY()
		UParticleSystemComponent* AvailableEffectPSC;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Objective)
		UParticleSystem* RallyChargingEffect;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Objective)
		UParticleSystem* RallyBrokenEffect;

	UPROPERTY()
		UParticleSystemComponent* RallyEffectPSC;

	UPROPERTY()
		UDecalComponent* AvailableDecal;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = AmbientSoundUpdated, Category = "Audio")
		USoundBase* AmbientSound;

	UPROPERTY(BlueprintReadWrite, Category = "Audio")
		float AmbientSoundPitch;

	UPROPERTY(BlueprintReadOnly, Category = "Audio")
		UAudioComponent* AmbientSoundComp;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Objective)
		USoundBase* PoweringUpSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Objective)
		USoundBase* ReadyToRallySound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Objective)
		USoundBase* FCTouchedSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Objective)
		USoundBase* EnabledSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Objective)
		USoundBase* RallyEndedSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Objective)
		USoundBase* RallyBrokenSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = PickupDisplay)
		UMaterialInterface* GlowDecalMaterial;

	UPROPERTY(BlueprintReadOnly, Category = PickupDisplay)
		UMaterialInstanceDynamic* GlowDecalMaterialInstance;

	UFUNCTION()
		void OnOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
		void OnOverlapEnd(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void PostRenderFor(APlayerController *PC, UCanvas *Canvas, FVector CameraPosition, FVector CameraDir) override;

	virtual void Reset_Implementation() override;

	virtual void FlagCarrierInVolume(class AUTCharacter* NewFC);

	virtual void StartRallyCharging();

	virtual void EndRallyCharging();

	virtual void RallyChargingComplete();

	virtual void SetRallyPointState(FName NewState);

	virtual FVector GetRallyLocation(class AUTCharacter* TestChar);

	// increment to give different rally spots to each arriving player
	UPROPERTY()
		int32 RallyOffset;

	UFUNCTION()
	void OnAvailableEffectChanged();

	UFUNCTION()
		void OnRallyChargingChanged();

	virtual void ChangeAmbientSoundPitch(USoundBase* InAmbientSound, float NewPitch);

	UFUNCTION()
		virtual void AmbientSoundPitchUpdated();

	virtual void SetAmbientSound(USoundBase* NewAmbientSound, bool bClear);

	UFUNCTION()
		void AmbientSoundUpdated();

	/** Arrow component to indicate forward direction of start */
#if WITH_EDITORONLY_DATA
	private_subobject :
						  UPROPERTY()
						  class UArrowComponent* ArrowComponent;
public:
	/** Returns ArrowComponent subobject **/
	class UArrowComponent* GetArrowComponent() const;
#endif
};
