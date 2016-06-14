// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTCTFCapturePoint.generated.h"

DECLARE_DYNAMIC_DELEGATE(FOnCaptureCompletedDelegate);

UCLASS(HideCategories = GameObject)
class UNREALTOURNAMENT_API AUTCTFCapturePoint : public AUTGameObjective
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = Objective)
	UCapsuleComponent* Capsule;

	UPROPERTY(BlueprintReadOnly, Category = Objective)
	int TeamMatesInCapsule;

	UPROPERTY(BlueprintReadOnly, Category = Objective)
	int EnemiesInCapsule;

	UPROPERTY(BlueprintReadWrite, replicated, Category = Objective)
	float CapturePercent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Objective)
	bool bIsActive;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Objective)
	float TimeToFullCapture;

	UPROPERTY(BlueprintReadOnly, Category = Objective)
	bool bIsPausedByEnemy;

	UPROPERTY(BlueprintReadOnly, Category = Objective)
	bool bIsAdvancing;

	UPROPERTY(BlueprintReadOnly, Category = Objective)
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
	virtual void OnOverlapBegin(AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	virtual void OnOverlapEnd(AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	UFUNCTION()
	virtual void AdvanceCapturePercent(float DeltaTime);

	UFUNCTION()
	virtual void DecreaseCapturePercent(float DeltaTime);

	UFUNCTION(BlueprintNativeEvent, Category = CapturePoint)
	void OnCaptureComplete();

	UFUNCTION(BlueprintCallable, Category = CapturePoint)
	const TArray<AUTCharacter*>& GetCharactersInCapturePoint();

	UPROPERTY()
	FOnCaptureCompletedDelegate OnCaptureCompletedDelegate;

	virtual void BeginPlay() override;

protected:
	UPROPERTY()
	TArray<AUTCharacter*> CharactersInCapturePoint;

};