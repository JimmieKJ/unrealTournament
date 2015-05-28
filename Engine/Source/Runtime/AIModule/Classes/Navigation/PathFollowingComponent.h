// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "AITypes.h"
#include "AI/Navigation/NavigationTypes.h"
#include "Components/ActorComponent.h"
#include "AIResourceInterface.h"
#include "PathFollowingComponent.generated.h"

AIMODULE_API DECLARE_LOG_CATEGORY_EXTERN(LogPathFollowing, Warning, All);

class UNavMovementComponent;
class UCanvas;
class AActor;
class INavLinkCustomInterface;
class INavAgentInterface;
class UNavigationComponent;

UENUM(BlueprintType)
namespace EPathFollowingStatus
{
	enum Type
	{
		/** No requests */
		Idle,

		/** Request with incomplete path, will start after UpdateMove() */
		Waiting,

		/** Request paused, will continue after ResumeMove() */
		Paused,

		/** Following path */
		Moving,
	};
}

UENUM(BlueprintType)
namespace EPathFollowingResult
{
	enum Type
	{
		/** Reached destination */
		Success,

		/** Movement was blocked */
		Blocked,

		/** Agent is not on path */
		OffPath,

		/** Aborted and stopped (failure) */
		Aborted,

		/** Aborted and replaced with new request */
		Skipped,

		/** Request was invalid */
		Invalid,
	};
}

// left for now, will be removed soon! please use EPathFollowingStatus instead
UENUM(BlueprintType)
namespace EPathFollowingAction
{
	enum Type
	{
		Error,
		NoMove,
		DirectMove,
		PartialPath,
		PathToGoal,
	};
}

UENUM(BlueprintType)
namespace EPathFollowingRequestResult
{
	enum Type
	{
		Failed,
		AlreadyAtGoal,
		RequestSuccessful
	};
}

namespace EPathFollowingDebugTokens
{
	enum Type
	{
		Description,
		ParamName,
		FailedValue,
		PassedValue,
	};
}

namespace EPathFollowingMessage
{
	enum Type
	{
		/** Aborted because no path was found */
		NoPath,

		/** Aborted because another request came in */
		OtherRequest,
	};
}

UCLASS(config=Engine)
class AIMODULE_API UPathFollowingComponent : public UActorComponent, public IAIResourceInterface
{
	GENERATED_UCLASS_BODY()

	DECLARE_DELEGATE_TwoParams(FPostProcessMoveSignature, UPathFollowingComponent* /*comp*/, FVector& /*velocity*/);
	DECLARE_DELEGATE_OneParam(FRequestCompletedSignature, EPathFollowingResult::Type /*Result*/);
	DECLARE_MULTICAST_DELEGATE_TwoParams(FMoveCompletedSignature, FAIRequestID /*RequestID*/, EPathFollowingResult::Type /*Result*/);

	/** delegate for modifying path following velocity */
	FPostProcessMoveSignature PostProcessMove;

	/** delegate for move completion notify */
	FMoveCompletedSignature OnMoveFinished;

	// Begin UActorComponent Interface
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;
	// End UActorComponent Interface

	/** initialize component to use */
	virtual void Initialize();

	/** cleanup component before destroying */
	virtual void Cleanup();

	/** updates cached pointers to relevant owner's components */
	virtual void UpdateCachedComponents();

	/** start movement along path
	 *  @returns request ID or 0 when failed */
	virtual FAIRequestID RequestMove(FNavPathSharedPtr Path, FRequestCompletedSignature OnComplete, const AActor* DestinationActor = NULL, float AcceptanceRadius = UPathFollowingComponent::DefaultAcceptanceRadius, bool bStopOnOverlap = true, FCustomMoveSharedPtr GameData = NULL);

	/** start movement along path
	 *  @returns request ID or 0 when failed */
	FORCEINLINE FAIRequestID RequestMove(FNavPathSharedPtr InPath, const AActor* InDestinationActor = NULL, float InAcceptanceRadius = UPathFollowingComponent::DefaultAcceptanceRadius, bool InStopOnOverlap = true, FCustomMoveSharedPtr InGameData = NULL)
	{
		return RequestMove(InPath, UnboundRequestDelegate, InDestinationActor, InAcceptanceRadius, InStopOnOverlap, InGameData);
	}

	/** update path for specified request
	 *  @param RequestID - request to update */
	virtual bool UpdateMove(FNavPathSharedPtr Path, FAIRequestID RequestID = FAIRequestID::CurrentRequest);

	/** aborts following path
	 *  @param RequestID - request to abort, 0 = current
	 *  @param bResetVelocity - try to stop movement component
	 *  @param bSilent - finish with Skipped result instead of Aborted */
	virtual void AbortMove(const FString& Reason, FAIRequestID RequestID = FAIRequestID::CurrentRequest, bool bResetVelocity = true, bool bSilent = false, uint8 MessageFlags = 0);

	/** pause path following
	*  @param RequestID - request to pause, FAIRequestID::CurrentRequest means pause current request, regardless of its ID */
	virtual void PauseMove(FAIRequestID RequestID = FAIRequestID::CurrentRequest, bool bResetVelocity = true);

	/** resume path following
	*  @param RequestID - request to resume, FAIRequestID::CurrentRequest means restor current request, regardless of its ID*/
	virtual void ResumeMove(FAIRequestID RequestID = FAIRequestID::CurrentRequest);

	/** notify about finished movement */
	virtual void OnPathFinished(EPathFollowingResult::Type Result);

	/** notify about finishing move along current path segment */
	virtual void OnSegmentFinished();

	/** notify about changing current path */
	virtual void OnPathUpdated();

	/** set associated movement component */
	virtual void SetMovementComponent(UNavMovementComponent* MoveComp);

	/** get current focal point of movement */
	virtual FVector GetMoveFocus(bool bAllowStrafe) const;

	/** simple test for stationary agent (used as early finish condition), check if reached given point
	 *  @param TestPoint - point to test
	 *  @param AcceptanceRadius - allowed 2D distance
	 *  @param bExactSpot - false: increase AcceptanceRadius with agent's radius
	 */
	bool HasReached(const FVector& TestPoint, float AcceptanceRadius = UPathFollowingComponent::DefaultAcceptanceRadius, bool bExactSpot = false) const;

	/** simple test for stationary agent (used as early finish condition), check if reached given goal
	 *  @param TestGoal - actor to test
	 *  @param AcceptanceRadius - allowed 2D distance
	 *  @param bExactSpot - false: increase AcceptanceRadius with agent's radius
	 */
	bool HasReached(const AActor& TestGoal, float AcceptanceRadius = UPathFollowingComponent::DefaultAcceptanceRadius, bool bExactSpot = false) const;

	/** update state of block detection */
	void SetBlockDetectionState(bool bEnable);

	/** @returns state of block detection */
	bool IsBlockDetectionActive() const { return bUseBlockDetection; }

	/** set block detection params */
	void SetBlockDetection(float DistanceThreshold, float Interval, int32 NumSamples);

	/** @returns state of movement stopping on finish */
	FORCEINLINE bool IsStopMovementOnFinishActive() const { return bStopMovementOnFinish; }
	
	/** set whether movement is stopped on finish of move. */
	FORCEINLINE void SetStopMovementOnFinish(bool bEnable) { bStopMovementOnFinish = bEnable; }

	/** set threshold for precise reach tests in intermediate goals (minimal test radius)  */
	void SetPreciseReachThreshold(float AgentRadiusMultiplier, float AgentHalfHeightMultiplier);

	/** set status of last requested move */
	void SetLastMoveAtGoal(bool bFinishedAtGoal);

	/** @returns estimated cost of unprocessed path segments
	 *	@NOTE 0 means, that component is following final path segment or doesn't move */
	float GetRemainingPathCost() const;
	
	/** Returns current location on navigation data */
	FNavLocation GetCurrentNavLocation() const;

	FORCEINLINE EPathFollowingStatus::Type GetStatus() const { return Status; }
	FORCEINLINE float GetAcceptanceRadius() const { return AcceptanceRadius; }
	FORCEINLINE float GetDefaultAcceptanceRadius() const { return MyDefaultAcceptanceRadius; }
	FORCEINLINE void SetAcceptanceRadius(float InAcceptanceRadius) { AcceptanceRadius = InAcceptanceRadius; }
	FORCEINLINE AActor* GetMoveGoal() const { return DestinationActor.Get(); }
	FORCEINLINE bool HasPartialPath() const { return Path.IsValid() && Path->IsPartial(); }
	FORCEINLINE bool DidMoveReachGoal() const { return bLastMoveReachedGoal && (Status == EPathFollowingStatus::Idle); }

	FORCEINLINE FAIRequestID GetCurrentRequestId() const { return CurrentRequestId; }
	FORCEINLINE uint32 GetCurrentPathIndex() const { return MoveSegmentStartIndex; }
	FORCEINLINE uint32 GetNextPathIndex() const { return MoveSegmentEndIndex; }
	FORCEINLINE UObject* GetCurrentCustomLinkOb() const { return CurrentCustomLinkOb.Get(); }
	FORCEINLINE FVector GetCurrentTargetLocation() const { return *CurrentDestination; }
	FORCEINLINE FBasedPosition GetCurrentTargetLocationBased() const { return CurrentDestination; }
	FVector GetCurrentDirection() const;

	/** will be deprecated soon, please use AIController.GetMoveStatus instead! */
	UFUNCTION(BlueprintCallable, Category="AI|Components|PathFollowing")
	EPathFollowingAction::Type GetPathActionType() const;

	/** will be deprecated soon, please use AIController.GetImmediateMoveDestination instead! */
	UFUNCTION(BlueprintCallable, Category="AI|Components|PathFollowing")
	FVector GetPathDestination() const;

	FORCEINLINE const FNavPathSharedPtr GetPath() const { return Path; }
	FORCEINLINE bool HasValidPath() const { return Path.IsValid() && Path->IsValid(); }
	bool HasDirectPath() const;

	/** readable name of current status */
	FString GetStatusDesc() const;
	/** readable name of result enum */
	FString GetResultDesc(EPathFollowingResult::Type Result) const;

	void SetDestinationActor(const AActor* InDestinationActor);

	/** returns index of the currently followed element of path. Depending on the actual 
	 *	path it may represent different things, like a path point or navigation corridor index */
	virtual int32 GetCurrentPathElement() const { return MoveSegmentEndIndex; }

	virtual void GetDebugStringTokens(TArray<FString>& Tokens, TArray<EPathFollowingDebugTokens::Type>& Flags) const;
	virtual FString GetDebugString() const;

	virtual void DisplayDebug(UCanvas* Canvas, const FDebugDisplayInfo& DebugDisplay, float& YL, float& YPos) const;
#if ENABLE_VISUAL_LOG
	virtual void DescribeSelfToVisLog(struct FVisualLogEntry* Snapshot) const;
#endif // ENABLE_VISUAL_LOG

	/** called when moving agent collides with another actor */
	UFUNCTION()
	virtual void OnActorBump(AActor* SelfActor, AActor* OtherActor, FVector NormalImpulse, const FHitResult& Hit);

	/** Called when movement is blocked by a collision with another actor.  */
	virtual void OnMoveBlockedBy(const FHitResult& BlockingImpact) {}

	/** Called when falling movement ends. */
	virtual void OnLanded() {}

	/** Check if path following can be activated */
	virtual bool IsPathFollowingAllowed() const;

	/** call when moving agent finishes using custom nav link, returns control back to path following */
	virtual void FinishUsingCustomLink(INavLinkCustomInterface* CustomNavLink);

	/** called when owner is preparing new pathfinding request */
	virtual void OnPathfindingQuery(FPathFindingQuery& Query) {}

	// IAIResourceInterface begin
	virtual void LockResource(EAIRequestPriority::Type LockSource) override;
	virtual void ClearResourceLock(EAIRequestPriority::Type LockSource) override;
	virtual void ForceUnlockResource() override;
	virtual bool IsResourceLocked() const override;
	// IAIResourceInterface end

	void OnPathEvent(FNavigationPath* InvalidatedPath, ENavPathEvent::Type Event);

	/** helper function for sending a path for visual log */
	static void LogPathHelper(const AActor* LogOwner, FNavPathSharedPtr LogPath, const AActor* LogGoalActor);
	static void LogPathHelper(const AActor* LogOwner, FNavigationPath* LogPath, const AActor* LogGoalActor);

protected:

	/** associated movement component */
	UPROPERTY(transient)
	UNavMovementComponent* MovementComp;

	/** currently traversed custom nav link */
	FWeakObjectPtr CurrentCustomLinkOb;

	/** navigation data for agent described in movement component */
	UPROPERTY(transient)
	ANavigationData* MyNavData;

	/** current status */
	EPathFollowingStatus::Type Status;

	/** requested path */
	FNavPathSharedPtr Path;

	/** value based on navigation agent's properties that's used for AcceptanceRadius when DefaultAcceptanceRadius is requested */
	float MyDefaultAcceptanceRadius;

	/** min distance to destination to consider request successful */
	float AcceptanceRadius;

	/** min distance to end of current path segment to consider segment finished */
	float CurrentAcceptanceRadius;

	/** part of agent radius used as min acceptance radius */
	float MinAgentRadiusPct;

	/** part of agent height used as min acceptable height difference */
	float MinAgentHalfHeightPct;

	/** game specific data */
	FCustomMoveSharedPtr GameData;

	/** current request observer */
	FRequestCompletedSignature OnRequestFinished;

	/** destination actor. Use SetDestinationActor to set this */
	TWeakObjectPtr<AActor> DestinationActor;

	/** cached DestinationActor cast to INavAgentInterface. Use SetDestinationActor to set this */
	const INavAgentInterface* DestinationAgent;

	/** destination for current path segment */
	FBasedPosition CurrentDestination;

	/** relative offset from goal actor's location to end of path */
	FVector MoveOffset;

	/** agent location when movement was paused */
	FVector LocationWhenPaused;

	/** timestamp of path update when movement was paused */
	float PathTimeWhenPaused;

	/** set when paths simplification using visibility tests are needed  (disabled by default because of performance) */
	UPROPERTY(config)
	uint32 bUseVisibilityTestsSimplification : 1;

	/** increase acceptance radius with agent's radius */
	uint32 bStopOnOverlap : 1;

	/** if set, movement block detection will be used */
	uint32 bUseBlockDetection : 1;

	/** set when agent collides with goal actor */
	uint32 bCollidedWithGoal : 1;

	/** set when last move request was finished at goal */
	uint32 bLastMoveReachedGoal : 1;

	/** if set, movement will be stopped on finishing path */
	uint32 bStopMovementOnFinish : 1;

	/** detect blocked movement when distance between center of location samples and furthest one (centroid radius) is below threshold */
	float BlockDetectionDistance;

	/** interval for collecting location samples */
	float BlockDetectionInterval;

	/** number of samples required for block detection */
	int32 BlockDetectionSampleCount;

	/** timestamp of last location sample */
	float LastSampleTime;

	/** index of next location sample in array */
	int32 NextSampleIdx;

	/** location samples for stuck detection */
	TArray<FBasedPosition> LocationSamples;

	/** index of path point being current move beginning */
	int32 MoveSegmentStartIndex;

	/** index of path point being current move target */
	int32 MoveSegmentEndIndex;

	/** reference of node at segment start */
	NavNodeRef MoveSegmentStartRef;

	/** reference of node at segment end */
	NavNodeRef MoveSegmentEndRef;

	/** direction of current move segment */
	FVector MoveSegmentDirection;

	/** reset path following data */
	virtual void Reset();

	/** should verify if agent if still on path ater movement has been resumed? */
	virtual bool ShouldCheckPathOnResume() const;

	/** sets variables related to current move segment */
	virtual void SetMoveSegment(int32 SegmentStartIndex);
	
	/** follow current path segment */
	virtual void FollowPathSegment(float DeltaTime);

	/** check state of path following, update move segment if needed */
	virtual void UpdatePathSegment();

	/** next path segment if custom nav link, try passing control to it */
	virtual void StartUsingCustomLink(INavLinkCustomInterface* CustomNavLink, const FVector& DestPoint);

	/** update blocked movement detection, @returns true if new sample was added */
	virtual bool UpdateBlockDetection();

	/** check if move is completed */
	bool HasReachedDestination(const FVector& CurrentLocation) const;

	/** check if segment is completed */
	bool HasReachedCurrentTarget(const FVector& CurrentLocation) const;

	/** check if moving agent has reached goal defined by cylinder */
	DEPRECATED(4.8, "Please use override with AgentRadiusMultiplier instead of this.")
	bool HasReachedInternal(const FVector& GoalLocation, float GoalRadius, float GoalHalfHeight, const FVector& AgentLocation, float RadiusThreshold, bool bSuccessOnRadiusOverlap) const;
	bool HasReachedInternal(const FVector& GoalLocation, float GoalRadius, float GoalHalfHeight, const FVector& AgentLocation, float RadiusThreshold, float AgentRadiusMultiplier) const;

	/** check if agent is on path */
	virtual bool IsOnPath() const;

	/** check if movement is blocked */
	bool IsBlocked() const;

	/** switch to next segment on path */
	FORCEINLINE void SetNextMoveSegment() { SetMoveSegment(GetNextPathIndex()); }

	FORCEINLINE static uint32 GetNextRequestId() { return NextRequestId++; }
	FORCEINLINE void StoreRequestId() { CurrentRequestId = UPathFollowingComponent::GetNextRequestId(); }

	/** Checks if this PathFollowingComponent is already on path, and
	*	if so determines index of next path point
	*	@return what PathFollowingComponent thinks should be next path point. INDEX_NONE if given path is invalid
	*	@note this function does not set MoveSegmentEndIndex */
	virtual int32 DetermineStartingPathPoint(const FNavigationPath* ConsideredPath) const;

	/** @return index of path point, that should be target of current move segment */
	virtual int32 DetermineCurrentTargetPathPoint(int32 StartIndex);

	/** Visibility tests to skip some path points if possible
	@param NextSegmentStartIndex Selected next segment to follow
	@return  better path segment to follow, to use as MoveSegmentEndIndex */
	int32 OptimizeSegmentVisibility(int32 StartIndex);

	/** check if movement component is valid or tries to grab one from owner 
	 *	@param bForce results in looking for owner's movement component even if pointer to one is already cached */
	virtual bool UpdateMovementComponent(bool bForce = false);

	/** clears Block Detection stored data effectively resetting the mechanism */
	void ResetBlockDetectionData();

	/** force creating new location sample for block detection */
	void ForceBlockDetectionUpdate();

	/** set move focus in AI owner */
	void UpdateMoveFocus();

	/** debug point reach test values */
	void DebugReachTest(float& CurrentDot, float& CurrentDistance, float& CurrentHeight, uint8& bDotFailed, uint8& bDistanceFailed, uint8& bHeightFailed) const;
	
private:

	/** used for debugging purposes to be able to identify which logged information
	 *	results from which request, if there was multiple ones during one frame */
	static uint32 NextRequestId;
	FAIRequestID CurrentRequestId;

	/** Current location on navigation data.  Lazy-updated, so read this via GetCurrentNavLocation(). 
	 *	Since it makes conceptual sense for GetCurrentNavLocation() to be const but we may 
	 *	need to update the cached value, CurrentNavLocation is mutable. */
	mutable FNavLocation CurrentNavLocation;

	/** used to keep track of which subsystem requested this AI resource be locked */
	FAIResourceLock ResourceLock;

	/** empty delegate for RequestMove */
	static FRequestCompletedSignature UnboundRequestDelegate;

public:
	/** special float constant to symbolize "use default value". This does not contain 
	 *	value to be used, it's used to detect the fact that it's requested, and 
	 *	appropriate value from querier/doer will be pulled */
	static const float DefaultAcceptanceRadius;
};
