// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTBot.h"
#include "BehaviorTree/BehaviorTreeTypes.h"
#include "UTCustomBot.generated.h"

class UBlackboardComponent;

UCLASS()
class CUSTOMBOT_API AUTCustomBot : public AUTBot
{
	GENERATED_BODY()

protected:
	FBlackboard::FKey BlackboardKey_Enemy;
	FBlackboard::FKey BlackboardKey_EnemyVisible;

	UPROPERTY()
	UBlackboardComponent* BlackboardComponent;

public:
	AUTCustomBot(const FObjectInitializer& ObjectInitializer);

	// AIController begin
	virtual bool InitializeBlackboard(UBlackboardComponent& BlackboardComp, UBlackboardData& BlackboardAsset) override;
	virtual void Possess(APawn* PossessedPawn) override;
	virtual void PawnPendingDestroy(APawn* InPawn) override;
	// AIController end

	// UTBot begin
	virtual void Tick(float DeltaTime) override;
	virtual void SetEnemy(APawn* NewEnemy) override;
	virtual FVector GetEnemyLocation(APawn* TestEnemy, bool bAllowPrediction) override;
	virtual void ApplyWeaponAimAdjust(FVector TargetLoc, FVector& FocalPoint) override;

	virtual void UpdateTrackingError(bool bNewEnemy) override;	
	virtual void ProcessIncomingWarning() override;
	// UTBot end

	UFUNCTION(Category = "CustomBot", BlueprintCallable)
	AUTWeapon* GetBestWeapon() const;
};