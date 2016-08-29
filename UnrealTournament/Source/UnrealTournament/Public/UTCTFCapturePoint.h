// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTCTFCapturePoint.generated.h"

DECLARE_DYNAMIC_DELEGATE(FOnCaptureCompletedDelegate);

UCLASS(HideCategories = GameObject)
class UNREALTOURNAMENT_API AUTCTFCapturePoint : public AUTGameObjective, public IUTResetInterface
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = Objective)
	UCapsuleComponent* Capsule;

	UPROPERTY(BlueprintReadOnly, replicated, Category = Objective)
	int DefendersInCapsule;

	UPROPERTY(BlueprintReadOnly, replicated, Category = Objective)
	int AttackersInCapsule;

	UPROPERTY(BlueprintReadWrite, replicated, Category = Objective)
	float CapturePercent;

	/** If the control point can currently be capped. */
	UPROPERTY(EditAnywhere, replicated, BlueprintReadWrite, Category = Objective)
	bool bIsActive;
	
	/** If this is true the control point never switches sides. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Objective)
	bool bIsOneSidedCapturePoint;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Objective)
	float TimeToFullCapture;

	UPROPERTY(BlueprintReadOnly, replicated, Category = Objective)
	bool bIsPaused;

	UPROPERTY(BlueprintReadOnly, replicated, Category = Objective)
	bool bIsCapturing;

	UPROPERTY(BlueprintReadOnly, replicated, Category = Objective)
	bool bIsDraining;

	/** What % should drain each second no one is on the point */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Objective)
	float DrainRate;

	/** Once the capture point progress reaches one of these thresholds (between 0-1) it will not drain previously. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Objective)
	TArray<float> DrainLockSegments;

	/** An array that stores how fast it should capture for each person on the point. IE: Index 2 would be how fast should 2 people capture the point. If it is 1.5 then it will capture at 1.5x the speed.
		If the 0th element is set to something >0, the capture point will advance even with no team mates on it!*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Objective)
	TArray<float> CaptureBoostPerCharacter;

	UFUNCTION()
	virtual void Tick(float DeltaTime) override;

	UFUNCTION()
	void CalculateOccupyingCharacterCounts();

	UFUNCTION()
	virtual void OnOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	virtual void OnOverlapEnd(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	UFUNCTION()
	virtual void AdvanceCapturePercent(float DeltaTime);

	UFUNCTION()
	virtual void DecreaseCapturePercent(float DeltaTime);

	/** Determines if the Defending team should switch, and if so handles the logic to set a new TeamNum to the correct defending team. */
	UFUNCTION()
	virtual void HandleDefendingTeamSwitch();

	UFUNCTION(BlueprintNativeEvent, Category = CapturePoint)
	void OnCaptureComplete();

	UFUNCTION(BlueprintCallable, Category = CapturePoint)
	const TArray<AUTCharacter*>& GetCharactersInCapturePoint();

	UPROPERTY()
	FOnCaptureCompletedDelegate OnCaptureCompletedDelegate;

	virtual void BeginPlay() override;

	virtual void Reset_Implementation() override;

protected:
	UPROPERTY()
	TArray<AUTCharacter*> CharactersInCapturePoint;

	UPROPERTY()
	bool bHasRunOnCaptureComplete;
};