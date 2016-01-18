// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "AITask.h"
#include "Navigation/PathFollowingComponent.h"
#include "AITask_MoveTo.generated.h"

class APawn;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMoveTaskCompletedSignature, TEnumAsByte<EPathFollowingResult::Type>, Result);

UCLASS()
class AIMODULE_API UAITask_MoveTo : public UAITask
{
	GENERATED_BODY()
protected:
	UPROPERTY(BlueprintAssignable)
	FGenericGameplayTaskDelegate OnRequestFailed;

	UPROPERTY(BlueprintAssignable)
	FMoveTaskCompletedSignature OnMoveFinished;

	UPROPERTY(BlueprintReadOnly, meta = (ExposeOnSpawn = true, DisplayName="Goal Location"), Category = "AITask")
	FVector MoveGoalLocation;

	/** gets set to GoalLocation if GoalActor != nullptr, otherwise is FAISystem::InvalidLocation */
	FVector RealGoalLocation;
	
	UPROPERTY(BlueprintReadOnly, meta = (ExposeOnSpawn = true, DisplayName="Goal Actor"), Category = "AITask")
	AActor* MoveGoalActor;

	UPROPERTY(BlueprintReadOnly, meta = (ExposeOnSpawn = true, DisplayName = "Acceptance Radius"), Category = "AITask", AdvancedDisplay)
	float MoveAcceptanceRadius; 
	
	UPROPERTY(BlueprintReadOnly, meta = (ExposeOnSpawn = true), Category = "AITask", AdvancedDisplay)
	bool bShouldStopOnOverlap;

	UPROPERTY(BlueprintReadOnly, meta = (ExposeOnSpawn = true), Category = "AITask", AdvancedDisplay)
	bool bShouldAcceptPartialPath;

	UPROPERTY(BlueprintReadOnly, meta = (ExposeOnSpawn = true), Category = "AITask", AdvancedDisplay)
	bool bShouldUsePathfinding;

	FDelegateHandle PathFollowingDelegateHandle;
	FAIRequestID MoveRequestID;

	virtual void HandleMoveFinished(FAIRequestID RequestID, EPathFollowingResult::Type Result);
	virtual void Activate() override;
	virtual void OnDestroy(bool bOwnerFinished) override;

	virtual void Pause() override;
	virtual void Resume() override;

	virtual void PostInitProperties() override;
	
public:
	UAITask_MoveTo(const FObjectInitializer& ObjectInitializer);

	/**
	 *	@param 
	 */
	UFUNCTION(BlueprintCallable, Category = "AI|Tasks", meta = (AdvancedDisplay = "TaskOwner,AcceptanceRadius,StopOnOverlap,AcceptPartialPath,bUsePathfinding", DefaultToSelf = "TaskOwner", BlueprintInternalUseOnly = "TRUE", DisplayName = "Move To Location or Actor"))
	static UAITask_MoveTo* AIMoveTo(AAIController* Controller, FVector GoalLocation, AActor* GoalActor = nullptr, float AcceptanceRadius = -1.f, EAIOptionFlag::Type StopOnOverlap = EAIOptionFlag::Default, EAIOptionFlag::Type AcceptPartialPath = EAIOptionFlag::Default, bool bUsePathfinding = true, bool bLockAILogic = true);

	void SetUp(AAIController* Controller, FVector GoalLocation, AActor* GoalActor = nullptr, float AcceptanceRadius = -1.f, bool bUsePathfinding = true, EAIOptionFlag::Type StopOnOverlap = EAIOptionFlag::Default, EAIOptionFlag::Type AcceptPartialPath = EAIOptionFlag::Default);

	void PerformMove();
};