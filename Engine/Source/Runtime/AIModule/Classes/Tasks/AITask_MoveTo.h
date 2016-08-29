// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
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

public:
	UAITask_MoveTo(const FObjectInitializer& ObjectInitializer);

	/** tries to start move request and handles retry timer */
	void ConditionalPerformMove();

	/** prepare move task for activation */
	void SetUp(AAIController* Controller, const FAIMoveRequest& InMoveRequest);

	EPathFollowingResult::Type GetMoveResult() const { return MoveResult; }
	bool WasMoveSuccessful() const { return MoveResult == EPathFollowingResult::Success; }

	UFUNCTION(BlueprintCallable, Category = "AI|Tasks", meta = (AdvancedDisplay = "AcceptanceRadius,StopOnOverlap,AcceptPartialPath,bUsePathfinding", DefaultToSelf = "Controller", BlueprintInternalUseOnly = "TRUE", DisplayName = "Move To Location or Actor"))
	static UAITask_MoveTo* AIMoveTo(AAIController* Controller, FVector GoalLocation, AActor* GoalActor = nullptr, float AcceptanceRadius = -1.f, EAIOptionFlag::Type StopOnOverlap = EAIOptionFlag::Default, EAIOptionFlag::Type AcceptPartialPath = EAIOptionFlag::Default, bool bUsePathfinding = true, bool bLockAILogic = true);

	DEPRECATED(4.12, "This function is now depreacted, please use version with FAIMoveRequest parameter")
	void SetUp(AAIController* Controller, FVector GoalLocation, AActor* GoalActor = nullptr, float AcceptanceRadius = -1.f, bool bUsePathfinding = true, EAIOptionFlag::Type StopOnOverlap = EAIOptionFlag::Default, EAIOptionFlag::Type AcceptPartialPath = EAIOptionFlag::Default);

protected:
	UPROPERTY(BlueprintAssignable)
	FGenericGameplayTaskDelegate OnRequestFailed;

	UPROPERTY(BlueprintAssignable)
	FMoveTaskCompletedSignature OnMoveFinished;

	/** parameters of move request */
	UPROPERTY()
	FAIMoveRequest MoveRequest;

	/** handle of path following's OnMoveFinished delegate */
	FDelegateHandle PathFinishDelegateHandle;

	/** handle of path's update event delegate */
	FDelegateHandle PathUpdateDelegateHandle;

	/** handle of active ConditionalPerformMove timer  */
	FTimerHandle MoveRetryTimerHandle;

	/** handle of active ConditionalUpdatePath timer */
	FTimerHandle PathRetryTimerHandle;

	/** request ID of path following's request */
	FAIRequestID MoveRequestID;

	/** currently followed path */
	FNavPathSharedPtr Path;

	TEnumAsByte<EPathFollowingResult::Type> MoveResult;

	virtual void Activate() override;
	virtual void OnDestroy(bool bOwnerFinished) override;

	virtual void Pause() override;
	virtual void Resume() override;

	/** finish task */
	void FinishMoveTask(EPathFollowingResult::Type InResult);

	/** stores path and starts observing its events */
	void SetObservedPath(FNavPathSharedPtr InPath);

	/** remove all delegates */
	virtual void ResetObservers();

	/** remove all timers */
	virtual void ResetTimers();

	/** tries to update invalidated path and handles retry timer */
	void ConditionalUpdatePath();

	/** start move request */
	virtual void PerformMove();

	/** event from followed path */
	virtual void OnPathEvent(FNavigationPath* InPath, ENavPathEvent::Type Event);

	/** event from path following */
	virtual void OnRequestFinished(FAIRequestID RequestID, const FPathFollowingResult& Result);
};
