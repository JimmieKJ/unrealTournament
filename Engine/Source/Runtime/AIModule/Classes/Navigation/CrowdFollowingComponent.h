// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "AI/Navigation/NavigationAvoidanceTypes.h"
#include "Navigation/CrowdAgentInterface.h"
#include "Navigation/PathFollowingComponent.h"
#include "CrowdFollowingComponent.generated.h"

class INavLinkCustomInterface;
class UCharacterMovementComponent; 
class UCrowdManager;

namespace ECrowdAvoidanceQuality
{
	enum Type
	{
		Low,
		Medium,
		Good,
		High,
	};
}

UCLASS(BlueprintType)
class AIMODULE_API UCrowdFollowingComponent : public UPathFollowingComponent, public ICrowdAgentInterface
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	FVector CrowdAgentMoveDirection;

	// ICrowdAgentInterface BEGIN
	virtual FVector GetCrowdAgentLocation() const override;
	virtual FVector GetCrowdAgentVelocity() const override;
	virtual void GetCrowdAgentCollisions(float& CylinderRadius, float& CylinderHalfHeight) const override;
	virtual float GetCrowdAgentMaxSpeed() const override;
	// ICrowdAgentInterface END

	// PathFollowingComponent BEGIN
	virtual void Initialize() override;
	virtual void Cleanup() override;
	virtual void AbortMove(const FString& Reason, FAIRequestID RequestID = FAIRequestID::CurrentRequest, bool bResetVelocity = true, bool bSilent = false, uint8 MessageFlags = 0) override;
	virtual void PauseMove(FAIRequestID RequestID = FAIRequestID::CurrentRequest, bool bResetVelocity = true) override;
	virtual void ResumeMove(FAIRequestID RequestID = FAIRequestID::CurrentRequest) override;
	virtual FVector GetMoveFocus(bool bAllowStrafe) const override;
	virtual void OnLanded() override;
	virtual void FinishUsingCustomLink(INavLinkCustomInterface* CustomNavLink) override;
	virtual void OnPathFinished(EPathFollowingResult::Type Result) override;
	virtual void OnPathUpdated() override;
	virtual void OnPathfindingQuery(FPathFindingQuery& Query) override;
	virtual int32 GetCurrentPathElement() const override { return LastPathPolyIndex; }
	// PathFollowingComponent END

	/** update params in crowd manager */
	void UpdateCrowdAgentParams() const;

	/** pass agent velocity to movement component */
	virtual void ApplyCrowdAgentVelocity(const FVector& NewVelocity, const FVector& DestPathCorner, bool bTraversingLink);

	/** pass desired position to movement component (after resolving collisions between crowd agents) */
	virtual void ApplyCrowdAgentPosition(const FVector& NewPosition);

	/** master switch for crowd steering & avoidance */
	UFUNCTION(BlueprintCallable, Category = "Crowd")
	virtual void SuspendCrowdSteering(bool bSuspend);

	/** switch between crowd simulation and parent implementation (following path segments) */
	virtual void SetCrowdSimulation(bool bEnable);

	/** called when agent moved to next nav node (poly) */
	virtual void OnNavNodeChanged(NavNodeRef NewPolyRef, NavNodeRef PrevPolyRef, int32 CorridorSize);

	void SetCrowdAnticipateTurns(bool bEnable, bool bUpdateAgent = true);
	void SetCrowdObstacleAvoidance(bool bEnable, bool bUpdateAgent = true);
	void SetCrowdSeparation(bool bEnable, bool bUpdateAgent = true);
	void SetCrowdOptimizeVisibility(bool bEnable, bool bUpdateAgent = true);
	void SetCrowdOptimizeTopology(bool bEnable, bool bUpdateAgent = true);
	void SetCrowdSlowdownAtGoal(bool bEnable, bool bUpdateAgent = true);
	void SetCrowdSeparationWeight(float Weight, bool bUpdateAgent = true);
	void SetCrowdCollisionQueryRange(float Range, bool bUpdateAgent = true);
	void SetCrowdPathOptimizationRange(float Range, bool bUpdateAgent = true);
	void SetCrowdAvoidanceQuality(ECrowdAvoidanceQuality::Type Quality, bool bUpdateAgent = true);
	void SetCrowdAvoidanceRangeMultiplier(float Multipler, bool bUpdateAgent = true);

	FORCEINLINE bool IsCrowdSimulationEnabled() const { return bEnableCrowdSimulation; }
	FORCEINLINE bool IsCrowdSimulatioSuspended() const { return bSuspendCrowdSimulation; }
	FORCEINLINE bool IsCrowdAnticipateTurnsEnabled() const { return bEnableAnticipateTurns; }
	FORCEINLINE bool IsCrowdObstacleAvoidanceEnabled() const { return bEnableObstacleAvoidance; }
	FORCEINLINE bool IsCrowdSeparationEnabled() const { return bEnableSeparation; }
	FORCEINLINE bool IsCrowdOptimizeVisibilityEnabled() const { return bEnableOptimizeVisibility; /** don't check suspend here! */ }
	FORCEINLINE bool IsCrowdOptimizeTopologyEnabled() const { return bEnableOptimizeTopology; }
	FORCEINLINE bool IsCrowdPathOffsetEnabled() const { return bEnablePathOffset; }
	FORCEINLINE bool IsCrowdSlowdownAtGoalEnabled() const { return bEnableSlowdownAtGoal; }

	FORCEINLINE bool IsCrowdSimulationActive() const { return IsCrowdSimulationEnabled() && !IsCrowdSimulatioSuspended(); }
	/** checks if bEnableAnticipateTurns is set to true, and if crowd simulation is not suspended */
	FORCEINLINE bool IsCrowdAnticipateTurnsActive() const { return IsCrowdAnticipateTurnsEnabled() && !IsCrowdSimulatioSuspended(); }
	/** checks if bEnableObstacleAvoidance is set to true, and if crowd simulation is not suspended */
	FORCEINLINE bool IsCrowdObstacleAvoidanceActive() const { return IsCrowdObstacleAvoidanceEnabled() && !IsCrowdSimulatioSuspended(); }
	/** checks if bEnableSeparation is set to true, and if crowd simulation is not suspended */
	FORCEINLINE bool IsCrowdSeparationActive() const { return IsCrowdSeparationEnabled() && !IsCrowdSimulatioSuspended(); }
	/** checks if bEnableOptimizeTopology is set to true, and if crowd simulation is not suspended */
	FORCEINLINE bool IsCrowdOptimizeTopologyActive() const { return IsCrowdOptimizeTopologyEnabled() && !IsCrowdSimulatioSuspended(); }

	FORCEINLINE float GetCrowdSeparationWeight() const { return SeparationWeight; }
	FORCEINLINE float GetCrowdCollisionQueryRange() const { return CollisionQueryRange; }
	FORCEINLINE float GetCrowdPathOptimizationRange() const { return PathOptimizationRange; }
	FORCEINLINE ECrowdAvoidanceQuality::Type GetCrowdAvoidanceQuality() const { return AvoidanceQuality; }
	FORCEINLINE float GetCrowdAvoidanceRangeMultiplier() const { return AvoidanceRangeMultiplier; }
	FORCEINLINE int32 GetAvoidanceGroup() const { return AvoidanceGroup.Packed; }
	FORCEINLINE int32 GetGroupsToAvoid() const { return GroupsToAvoid.Packed; }
	FORCEINLINE int32 GetGroupsToIgnore() const { return GroupsToIgnore.Packed; }

	virtual void GetDebugStringTokens(TArray<FString>& Tokens, TArray<EPathFollowingDebugTokens::Type>& Flags) const override;
#if ENABLE_VISUAL_LOG
	virtual void DescribeSelfToVisLog(struct FVisualLogEntry* Snapshot) const override;
#endif // ENABLE_VISUAL_LOG

protected:

	UPROPERTY(transient)
	UCharacterMovementComponent* CharacterMovement;

	/** Group mask for this agent */
	UPROPERTY(Category = "Avoidance", EditAnywhere, BlueprintReadOnly, AdvancedDisplay)
	FNavAvoidanceMask AvoidanceGroup;

	/** Will avoid other agents if they are in one of specified groups */
	UPROPERTY(Category = "Avoidance", EditAnywhere, BlueprintReadOnly, AdvancedDisplay)
	FNavAvoidanceMask GroupsToAvoid;

	/** Will NOT avoid other agents if they are in one of specified groups, higher priority than GroupsToAvoid */
	UPROPERTY(Category = "Avoidance", EditAnywhere, BlueprintReadOnly, AdvancedDisplay)
	FNavAvoidanceMask GroupsToIgnore;

	/** if set, velocity will be updated even if agent is falling */
	uint32 bAffectFallingVelocity : 1;

	/** if set, move focus will match velocity direction */
	uint32 bRotateToVelocity : 1;

	/** if set, move velocity will be updated in every tick */
	uint32 bUpdateDirectMoveVelocity : 1;

	/** if set, agent will be simulated by crowd, otherwise it will act only as an obstacle */
	uint32 bEnableCrowdSimulation : 1;

	/** if set, avoidance and steering will be suspended (used for direct move requests) */
	uint32 bSuspendCrowdSimulation : 1;

	uint32 bEnableAnticipateTurns : 1;
	uint32 bEnableObstacleAvoidance : 1;
	uint32 bEnableSeparation : 1;
	uint32 bEnableOptimizeVisibility : 1;
	uint32 bEnableOptimizeTopology : 1;
	uint32 bEnablePathOffset : 1;
	uint32 bEnableSlowdownAtGoal : 1;

	/** if set, agent if moving on final path part, skip further updates (runtime flag) */
	uint32 bFinalPathPart : 1;

	/** if set, movement will be finished when velocity is opposite to path direction (runtime flag) */
	uint32 bCheckMovementAngle : 1;

	float SeparationWeight;
	float CollisionQueryRange;
	float PathOptimizationRange;

	/** multiplier for avoidance samples during detection, doesn't affect actual velocity */
	float AvoidanceRangeMultiplier;

	/** start index of current path part */
	int32 PathStartIndex;

	/** last visited poly on path */
	int32 LastPathPolyIndex;

	TEnumAsByte<ECrowdAvoidanceQuality::Type> AvoidanceQuality;

	// PathFollowingComponent BEGIN
	virtual int32 DetermineStartingPathPoint(const FNavigationPath* ConsideredPath) const override;
	virtual void SetMoveSegment(int32 SegmentStartIndex) override;
	virtual void UpdatePathSegment() override;
	virtual void FollowPathSegment(float DeltaTime) override;
	virtual bool ShouldCheckPathOnResume() const override;
	virtual bool IsOnPath() const override;
	virtual bool UpdateMovementComponent(bool bForce) override;
	virtual void Reset() override;
	// PathFollowingComponent END

	void SwitchToNextPathPart();
	bool ShouldSwitchPathPart(int32 CorridorSize) const;
	bool HasMovedDuringPause() const;
	void UpdateCachedDirections(const FVector& NewVelocity, const FVector& NextPathCorner, bool bTraversingLink);

	friend UCrowdManager;
};
