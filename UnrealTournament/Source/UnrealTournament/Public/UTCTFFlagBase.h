// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTCTFFlag.h"
#include "UTCTFFlagBase.generated.h"

UCLASS(HideCategories = GameObject)
class UNREALTOURNAMENT_API AUTCTFFlagBase : public AUTGameObjective, public IUTResetInterface
{
	GENERATED_UCLASS_BODY()

	// Holds a reference to the flag
	UPROPERTY(BlueprintReadOnly, Replicated, ReplicatedUsing = OnFlagChanged, Category = Flag)
	AUTCTFFlag* MyFlag;

	/**	Called when CarriedObject's state changes and is replicated to the client*/
	UFUNCTION()
	virtual void OnFlagChanged();

	// capsule for collision for detecting flag caps
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = Objective)
	UCapsuleComponent* Capsule;

	// The mesh that makes up this base.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Objective)
	UStaticMeshComponent* Mesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Sound)
	USoundBase* FlagScoreRewardSound;

	/** Own flag taken, play unattenuated sound for all players on team. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Sound)
	USoundBase* FlagTakenSound;

	/** Enemy flag taken, play attenuated sound. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Sound)
	USoundBase* EnemyFlagTakenSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Sound)
	USoundBase* FlagReturnedSound;

	/** array of flag classes by team */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Flag)
	TArray< TSubclassOf<AUTCTFFlag> > TeamFlagTypes;

	/** Adjustment to number of lives available to team with this base in round based CTF. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Objective)
		int32 RoundLivesAdjustment;

	/** effect when this base is being used as a defended objective */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Objective)
		UParticleSystem* BlueDefenseEffect;

	/** effect when this base is being used as a defended objective */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Objective)
		UParticleSystem* RedDefenseEffect;

	UPROPERTY()
		UParticleSystemComponent* DefensePSC;

	UPROPERTY(ReplicatedUsing = OnDefenseEffectChanged, BlueprintReadOnly, Category = Objective)
		uint8 ShowDefenseEffect;

	UFUNCTION()
		void OnDefenseEffectChanged();

	virtual void ClearDefenseEffect();
	virtual void SpawnDefenseEffect();

	virtual FName GetFlagState();
	virtual void RecallFlag();

	virtual void ObjectWasPickedUp(AUTCharacter* NewHolder, bool bWasHome) override;
	virtual void ObjectReturnedHome(AUTCharacter* Returner) override;

	UFUNCTION()
	virtual void OnOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	virtual void PostInitializeComponents() override;
	virtual void PostRenderFor(APlayerController* PC, UCanvas* Canvas, FVector CameraPosition, FVector CameraDir);

	// Returns a status message for this object on the hud.
	virtual FText GetHUDStatusMessage(AUTHUD* HUD);

	UFUNCTION(BlueprintNativeEvent)
	void OnObjectWasPickedUp();

protected:
	virtual void CreateCarriedObject();

public:

	// If true, this flag base will be considered a scoring point.  
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Objective)
	bool bScoreOnlyBase;

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = Game)
	void Reset() override;

};