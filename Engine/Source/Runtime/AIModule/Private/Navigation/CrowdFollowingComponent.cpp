// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AIModulePrivate.h"
#include "Navigation/CrowdFollowingComponent.h"
#include "Navigation/CrowdManager.h"
#include "AI/Navigation/NavLinkCustomInterface.h"
#include "AI/Navigation/AbstractNavData.h"
#include "GameFramework/CharacterMovementComponent.h"


DEFINE_LOG_CATEGORY(LogCrowdFollowing);

UCrowdFollowingComponent::UCrowdFollowingComponent(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bAffectFallingVelocity = false;
	bRotateToVelocity = true;
	bEnableCrowdSimulation = true;
	bSuspendCrowdSimulation = false;

	bEnableAnticipateTurns = false;
	bEnableObstacleAvoidance = true;
	bEnableSeparation = false;
	bEnableOptimizeVisibility = true;
	bEnableOptimizeTopology = true;
	bEnablePathOffset = false;
	bEnableSlowdownAtGoal = true;

	SeparationWeight = 2.0f;
	CollisionQueryRange = 400.0f;		// approx: radius * 12.0f
	PathOptimizationRange = 1000.0f;	// approx: radius * 30.0f
	AvoidanceQuality = ECrowdAvoidanceQuality::Low;
	AvoidanceRangeMultiplier = 1.0f;

	AvoidanceGroup.SetFlagsDirectly(1);
	GroupsToAvoid.SetFlagsDirectly(MAX_uint32);
	GroupsToIgnore.SetFlagsDirectly(0);
}

FVector UCrowdFollowingComponent::GetCrowdAgentLocation() const
{
	return MovementComp ? MovementComp->GetActorFeetLocation() : FVector::ZeroVector;
}

FVector UCrowdFollowingComponent::GetCrowdAgentVelocity() const
{
	FVector Velocity(MovementComp ? MovementComp->Velocity : FVector::ZeroVector);
	Velocity *= (Status == EPathFollowingStatus::Moving) ? 1.0f : 0.25f;
	return Velocity;
}

void UCrowdFollowingComponent::GetCrowdAgentCollisions(float& CylinderRadius, float& CylinderHalfHeight) const
{
	if (MovementComp && MovementComp->UpdatedComponent)
	{
		MovementComp->UpdatedComponent->CalcBoundingCylinder(CylinderRadius, CylinderHalfHeight);
	}
}

float UCrowdFollowingComponent::GetCrowdAgentMaxSpeed() const
{
	return MovementComp ? MovementComp->GetMaxSpeed() : 0.0f;
}

void UCrowdFollowingComponent::SetCrowdAnticipateTurns(bool bEnable, bool bUpdateAgent)
{
	if (bEnableAnticipateTurns != bEnable)
	{
		bEnableAnticipateTurns = bEnable;
		
		if (bUpdateAgent)
		{
			UpdateCrowdAgentParams();
		}
	}
}

void UCrowdFollowingComponent::SetCrowdObstacleAvoidance(bool bEnable, bool bUpdateAgent)
{
	if (bEnableObstacleAvoidance != bEnable)
	{
		bEnableObstacleAvoidance = bEnable;

		if (bUpdateAgent)
		{
			UpdateCrowdAgentParams();
		}
	}
}

void UCrowdFollowingComponent::SetCrowdSeparation(bool bEnable, bool bUpdateAgent)
{
	if (bEnableSeparation != bEnable)
	{
		bEnableSeparation = bEnable;

		if (bUpdateAgent)
		{
			UpdateCrowdAgentParams();
		}
	}
}

void UCrowdFollowingComponent::SetCrowdOptimizeVisibility(bool bEnable, bool bUpdateAgent)
{
	if (bEnableOptimizeVisibility != bEnable)
	{
		bEnableOptimizeVisibility = bEnable;

		if (bUpdateAgent)
		{
			UpdateCrowdAgentParams();
		}
	}
}

void UCrowdFollowingComponent::SetCrowdOptimizeTopology(bool bEnable, bool bUpdateAgent)
{
	if (bEnableOptimizeTopology != bEnable)
	{
		bEnableOptimizeTopology = bEnable;

		if (bUpdateAgent)
		{
			UpdateCrowdAgentParams();
		}
	}
}

void UCrowdFollowingComponent::SetCrowdSlowdownAtGoal(bool bEnable, bool bUpdateAgent)
{
	if (bEnableSlowdownAtGoal != bEnable)
	{
		bEnableSlowdownAtGoal = bEnable;

		if (bUpdateAgent)
		{
			UpdateCrowdAgentParams();
		}
	}
}

void UCrowdFollowingComponent::SetCrowdSeparationWeight(float Weight, bool bUpdateAgent)
{
	if (SeparationWeight != Weight)
	{
		SeparationWeight = Weight;

		if (bUpdateAgent)
		{
			UpdateCrowdAgentParams();
		}
	}
}

void UCrowdFollowingComponent::SetCrowdCollisionQueryRange(float Range, bool bUpdateAgent)
{
	if (CollisionQueryRange != Range)
	{
		CollisionQueryRange = Range;

		if (bUpdateAgent)
		{
			UpdateCrowdAgentParams();
		}
	}
}

void UCrowdFollowingComponent::SetCrowdPathOptimizationRange(float Range, bool bUpdateAgent)
{
	if (PathOptimizationRange != Range)
	{
		PathOptimizationRange = Range;

		if (bUpdateAgent)
		{
			UpdateCrowdAgentParams();
		}
	}
}

void UCrowdFollowingComponent::SetCrowdAvoidanceQuality(ECrowdAvoidanceQuality::Type Quality, bool bUpdateAgent)
{
	if (AvoidanceQuality != Quality)
	{
		AvoidanceQuality = Quality;

		if (bUpdateAgent)
		{
			UpdateCrowdAgentParams();
		}
	}
}

void UCrowdFollowingComponent::SuspendCrowdSteering(bool bSuspend)
{
	if (bSuspendCrowdSimulation != bSuspend)
	{
		bSuspendCrowdSimulation = bSuspend;
		UpdateCrowdAgentParams();
	}
}

void UCrowdFollowingComponent::UpdateCrowdAgentParams() const
{
	UCrowdManager* CrowdManager = UCrowdManager::GetCurrent(GetWorld());
	if (CrowdManager)
	{
		const ICrowdAgentInterface* IAgent = Cast<ICrowdAgentInterface>(this);
		CrowdManager->UpdateAgentParams(IAgent);
	}
}

void UCrowdFollowingComponent::UpdateCachedDirections(const FVector& NewVelocity, const FVector& NextPathCorner, bool bTraversingLink)
{
	// MoveSegmentDirection = direction on string pulled path
	const FVector AgentLoc = GetCrowdAgentLocation();
	const FVector ToCorner = NextPathCorner - AgentLoc;
	if (ToCorner.SizeSquared() > FMath::Square(10.0f))
	{
		MoveSegmentDirection = ToCorner.GetSafeNormal();
	}

	// CrowdAgentMoveDirection either direction on path or aligned with current velocity
	if (!bTraversingLink)
	{
		CrowdAgentMoveDirection = bRotateToVelocity && (NewVelocity.SizeSquared() > KINDA_SMALL_NUMBER) ? NewVelocity.GetSafeNormal() : MoveSegmentDirection;
	}
}

void UCrowdFollowingComponent::ApplyCrowdAgentVelocity(const FVector& NewVelocity, const FVector& DestPathCorner, bool bTraversingLink)
{
	if (bEnableCrowdSimulation && Status == EPathFollowingStatus::Moving)
	{
		if (bAffectFallingVelocity || CharacterMovement == NULL || CharacterMovement->MovementMode != MOVE_Falling)
		{
			MovementComp->RequestDirectMove(NewVelocity, false);

			UpdateCachedDirections(NewVelocity, DestPathCorner, bTraversingLink);
		}
	}
}

void UCrowdFollowingComponent::ApplyCrowdAgentPosition(const FVector& NewPosition)
{
	// base implementation does nothing
}

void UCrowdFollowingComponent::SetCrowdSimulation(bool bEnable)
{
	if (bEnableCrowdSimulation == bEnable)
	{
		return;
	}

	if (GetStatus() != EPathFollowingStatus::Idle)
	{
		UE_VLOG(GetOwner(), LogCrowdFollowing, Warning, TEXT("SetCrowdSimulation failed, agent is not in Idle state!"));
		return;
	}

	UCrowdManager* Manager = UCrowdManager::GetCurrent(GetWorld());
	if (Manager == NULL && bEnable)
	{
		UE_VLOG(GetOwner(), LogCrowdFollowing, Log, TEXT("Crowd manager can't be found, disabling simulation"));
		bEnable = false;
	}

	UE_VLOG(GetOwner(), LogCrowdFollowing, Log, TEXT("SetCrowdSimulation: %s"), bEnable ? TEXT("enabled") : TEXT("disabled"));
	bEnableCrowdSimulation = bEnable;
}

void UCrowdFollowingComponent::Initialize()
{
	Super::Initialize();

	UCrowdManager* CrowdManager = UCrowdManager::GetCurrent(GetWorld());
	if (CrowdManager)
	{
		ICrowdAgentInterface* IAgent = Cast<ICrowdAgentInterface>(this);
		CrowdManager->RegisterAgent(IAgent);
	}
	else
	{
		bEnableCrowdSimulation = false;
	}
}

void UCrowdFollowingComponent::Cleanup()
{
	Super::Cleanup();

	UCrowdManager* CrowdManager = UCrowdManager::GetCurrent(GetWorld());
	if (CrowdManager)
	{
		const ICrowdAgentInterface* IAgent = Cast<ICrowdAgentInterface>(this);
		CrowdManager->UnregisterAgent(IAgent);
	}
}

void UCrowdFollowingComponent::AbortMove(const FString& Reason, FAIRequestID RequestID, bool bResetVelocity, bool bSilent, uint8 MessageFlags)
{
	if (bEnableCrowdSimulation && (Status != EPathFollowingStatus::Idle) && RequestID.IsEquivalent(GetCurrentRequestId()))
	{
		UCrowdManager* CrowdManager = UCrowdManager::GetCurrent(GetWorld());
		if (CrowdManager)
		{
			CrowdManager->ClearAgentMoveTarget(this);
		}
	}

	Super::AbortMove(Reason, RequestID, bResetVelocity, bSilent, MessageFlags);
}

void UCrowdFollowingComponent::PauseMove(FAIRequestID RequestID, bool bResetVelocity)
{
	if (bEnableCrowdSimulation && (Status != EPathFollowingStatus::Paused) && RequestID.IsEquivalent(GetCurrentRequestId()))
	{
		UCrowdManager* CrowdManager = UCrowdManager::GetCurrent(GetWorld());
		if (CrowdManager)
		{
			CrowdManager->PauseAgent(this);
		}
	}

	Super::PauseMove(RequestID, bResetVelocity);
}

void UCrowdFollowingComponent::ResumeMove(FAIRequestID RequestID)
{
	if (bEnableCrowdSimulation && (Status == EPathFollowingStatus::Paused) && RequestID.IsEquivalent(GetCurrentRequestId()))
	{
		UCrowdManager* CrowdManager = UCrowdManager::GetCurrent(GetWorld());
		if (CrowdManager)
		{
			const bool bHasMoved = HasMovedDuringPause();
			CrowdManager->ResumeAgent(this, bHasMoved);
		}

		// reset cached direction, will be set again after velocity update
		// but before it happens do not change actor's focus point (rotation)
		CrowdAgentMoveDirection = FVector::ZeroVector;
	}

	Super::ResumeMove(RequestID);
}

void UCrowdFollowingComponent::Reset()
{
	Super::Reset();
	PathStartIndex = 0;
	LastPathPolyIndex = 0;

	bFinalPathPart = false;
	bCheckMovementAngle = false;
	bUpdateDirectMoveVelocity = false;
}

bool UCrowdFollowingComponent::UpdateMovementComponent(bool bForce)
{
	bool bRet = Super::UpdateMovementComponent(bForce);
	CharacterMovement = Cast<UCharacterMovementComponent>(MovementComp);

	return bRet;
}

void UCrowdFollowingComponent::OnLanded()
{
	UCrowdManager* CrowdManager = UCrowdManager::GetCurrent(GetWorld());
	if (bEnableCrowdSimulation && CrowdManager)
	{
		const ICrowdAgentInterface* IAgent = Cast<ICrowdAgentInterface>(this);
		CrowdManager->UpdateAgentState(IAgent);
	}
}

void UCrowdFollowingComponent::FinishUsingCustomLink(INavLinkCustomInterface* CustomNavLink)
{
	const bool bPrevCustomLink = CurrentCustomLinkOb.IsValid();
	Super::FinishUsingCustomLink(CustomNavLink);

	if (bEnableCrowdSimulation)
	{
		const bool bCurrentCustomLink = CurrentCustomLinkOb.IsValid();
		UCrowdManager* CrowdManager = UCrowdManager::GetCurrent(GetWorld());
		if (bPrevCustomLink && !bCurrentCustomLink && CrowdManager)
		{
			const ICrowdAgentInterface* IAgent = Cast<ICrowdAgentInterface>(this);
			CrowdManager->OnAgentFinishedCustomLink(IAgent);
		}
	}
}

void UCrowdFollowingComponent::OnPathFinished(EPathFollowingResult::Type Result)
{
	UCrowdManager* CrowdManager = UCrowdManager::GetCurrent(GetWorld());
	if (bEnableCrowdSimulation && CrowdManager)
	{
		CrowdManager->ClearAgentMoveTarget(this);
	}

	Super::OnPathFinished(Result);
}

void UCrowdFollowingComponent::OnPathUpdated()
{
	PathStartIndex = 0;
	LastPathPolyIndex = 0;
}

void UCrowdFollowingComponent::OnPathfindingQuery(FPathFindingQuery& Query)
{
	// disable path post processing (string pulling), crowd simulation needs to handle 
	// large paths by splitting into smaller parts and optimization gets in the way

	if (bEnableCrowdSimulation)
	{
		Query.NavDataFlags |= ERecastPathFlags::SkipStringPulling;
	}
}

bool UCrowdFollowingComponent::ShouldCheckPathOnResume() const
{
	if (bEnableCrowdSimulation)
	{
		// never call SetMoveSegment on resuming
		return false;
	}

	return HasMovedDuringPause();
}

bool UCrowdFollowingComponent::HasMovedDuringPause() const
{
	return Super::ShouldCheckPathOnResume();
}

bool UCrowdFollowingComponent::IsOnPath() const
{
	if (bEnableCrowdSimulation)
	{
		// agent can move off path for steering/avoidance purposes
		// just pretend it's always on path to avoid problems when movement is being resumed
		return true;
	}

	return Super::IsOnPath();
}

int32 UCrowdFollowingComponent::DetermineStartingPathPoint(const FNavigationPath* ConsideredPath) const
{
	int32 StartIdx = 0;

	if (bEnableCrowdSimulation)
	{
		StartIdx = PathStartIndex;

		// no path = called from SwitchToNextPathPart
		if (ConsideredPath == NULL && Path.IsValid())
		{
			StartIdx = LastPathPolyIndex;
		}
	}
	else
	{
		StartIdx = Super::DetermineStartingPathPoint(ConsideredPath);
	}

	return StartIdx;
}

void LogPathPartHelper(AActor* LogOwner, FNavMeshPath* NavMeshPath, int32 StartIdx, int32 EndIdx)
{
#if ENABLE_VISUAL_LOG && WITH_RECAST
	ARecastNavMesh* NavMesh = Cast<ARecastNavMesh>(NavMeshPath->GetNavigationDataUsed());
	if (NavMesh == NULL ||
		!NavMeshPath->PathCorridor.IsValidIndex(StartIdx) ||
		!NavMeshPath->PathCorridor.IsValidIndex(EndIdx))
	{
		return;
	}

	NavMesh->BeginBatchQuery();
	
	FVector SegmentStart = NavMeshPath->GetPathPoints()[0].Location;
	FVector SegmentEnd(FVector::ZeroVector);

	if (StartIdx > 0)
	{
		NavMesh->GetPolyCenter(NavMeshPath->PathCorridor[StartIdx], SegmentStart);
	}

	for (int32 Idx = StartIdx + 1; Idx < EndIdx; Idx++)
	{
		NavMesh->GetPolyCenter(NavMeshPath->PathCorridor[Idx], SegmentEnd);
		UE_VLOG_SEGMENT_THICK(LogOwner, LogCrowdFollowing, Log, SegmentStart, SegmentEnd, FColor::Yellow, 2, TEXT_EMPTY);

		SegmentStart = SegmentEnd;
	}

	if (EndIdx == (NavMeshPath->PathCorridor.Num() - 1))
	{
		SegmentEnd = NavMeshPath->GetPathPoints()[1].Location;
	}
	else
	{
		NavMesh->GetPolyCenter(NavMeshPath->PathCorridor[EndIdx], SegmentEnd);
	}

	UE_VLOG_SEGMENT_THICK(LogOwner, LogCrowdFollowing, Log, SegmentStart, SegmentEnd, FColor::Yellow, 2, TEXT_EMPTY);
	NavMesh->FinishBatchQuery();
#endif // ENABLE_VISUAL_LOG && WITH_RECAST
}

void UCrowdFollowingComponent::SetMoveSegment(int32 SegmentStartIndex)
{
	if (!bEnableCrowdSimulation)
	{
		Super::SetMoveSegment(SegmentStartIndex);
		return;
	}

	PathStartIndex = SegmentStartIndex;
	LastPathPolyIndex = PathStartIndex;
	if (Path.IsValid() == false || Path->IsValid() == false || GetOwner() == NULL)
	{
		return;
	}
	
	FVector CurrentTargetPt = Path->GetPathPoints()[1].Location;

	FNavMeshPath* NavMeshPath = Path->CastPath<FNavMeshPath>();
	FAbstractNavigationPath* DirectPath = Path->CastPath<FAbstractNavigationPath>();
	if (NavMeshPath)
	{
#if WITH_RECAST
		if (NavMeshPath->PathCorridor.IsValidIndex(PathStartIndex) == false)
		{
			// this should never matter, but just in case
			UE_VLOG(GetOwner(), LogCrowdFollowing, Error, TEXT("SegmentStartIndex in call to UCrowdFollowingComponent::SetMoveSegment is out of path corridor array's bounds (index: %d, array size %d)")
				, PathStartIndex, NavMeshPath->PathCorridor.Num());
			PathStartIndex = FMath::Clamp<int32>(PathStartIndex, 0, NavMeshPath->PathCorridor.Num() - 1);
		}

		// cut paths into parts to avoid problems with crowds getting into local minimum
		// due to using only first 10 steps of A*

		// do NOT use PathPoints here, crowd simulation disables path post processing
		// which means, that PathPoints contains only start and end position 
		// full path is available through PathCorridor array (poly refs)

		ARecastNavMesh* RecastNavData = Cast<ARecastNavMesh>(MyNavData);

		const int32 PathPartSize = 15;
		const int32 LastPolyIdx = NavMeshPath->PathCorridor.Num() - 1;
		int32 PathPartEndIdx = FMath::Min(PathStartIndex + PathPartSize, LastPolyIdx);

		FVector PtA, PtB;
		const bool bStartIsNavLink = RecastNavData->GetLinkEndPoints(NavMeshPath->PathCorridor[PathStartIndex], PtA, PtB);
		const bool bEndIsNavLink = RecastNavData->GetLinkEndPoints(NavMeshPath->PathCorridor[PathPartEndIdx], PtA, PtB);
		if (bStartIsNavLink)
		{
			PathStartIndex = FMath::Max(0, PathStartIndex - 1);
		}
		if (bEndIsNavLink)
		{
			PathPartEndIdx = FMath::Max(0, PathPartEndIdx - 1);
		}

		bFinalPathPart = (PathPartEndIdx == LastPolyIdx);
		if (!bFinalPathPart)
		{
			RecastNavData->GetPolyCenter(NavMeshPath->PathCorridor[PathPartEndIdx], CurrentTargetPt);
		}
		else if (NavMeshPath->IsPartial())
		{
			RecastNavData->GetClosestPointOnPoly(NavMeshPath->PathCorridor[PathPartEndIdx], Path->GetPathPoints()[1].Location, CurrentTargetPt);
		}

		// not safe to read those directions yet, you have to wait until crowd manager gives you next corner of string pulled path
		CrowdAgentMoveDirection = FVector::ZeroVector;
		MoveSegmentDirection = FVector::ZeroVector;

		CurrentDestination.Set(Path->GetBaseActor(), CurrentTargetPt);

		LogPathPartHelper(GetOwner(), NavMeshPath, PathStartIndex, PathPartEndIdx);
		UE_VLOG_SEGMENT(GetOwner(), LogCrowdFollowing, Log, MovementComp->GetActorFeetLocation(), CurrentTargetPt, FColor::Red, TEXT("path part"));
		UE_VLOG(GetOwner(), LogCrowdFollowing, Log, TEXT("SetMoveSegment, from:%d segments:%d%s"),
			PathStartIndex, (PathPartEndIdx - PathStartIndex)+1, bFinalPathPart ? TEXT(" (final)") : TEXT(""));

		UCrowdManager* CrowdManager = UCrowdManager::GetCurrent(GetWorld());
		if (CrowdManager)
		{
			CrowdManager->SetAgentMovePath(this, NavMeshPath, PathStartIndex, PathPartEndIdx);
		}
#endif
	}
	else if (DirectPath)
	{
		// direct paths are not using any steering or avoidance
		// pathfinding is replaced with simple velocity request 

		const FVector AgentLoc = MovementComp->GetActorFeetLocation();

		bFinalPathPart = true;
		bCheckMovementAngle = true;
		bUpdateDirectMoveVelocity = true;
		CurrentDestination.Set(Path->GetBaseActor(), CurrentTargetPt);
		CrowdAgentMoveDirection = (CurrentTargetPt - AgentLoc).GetSafeNormal();
		MoveSegmentDirection = CrowdAgentMoveDirection;

		UE_VLOG(GetOwner(), LogCrowdFollowing, Log, TEXT("SetMoveSegment, direct move"));
		UE_VLOG_SEGMENT(GetOwner(), LogCrowdFollowing, Log, AgentLoc, CurrentTargetPt, FColor::Red, TEXT("path"));

		UCrowdManager* CrowdManager = UCrowdManager::GetCurrent(GetWorld());
		if (CrowdManager)
		{
			CrowdManager->SetAgentMoveDirection(this, CrowdAgentMoveDirection);
		}
	}
	else
	{
		UE_VLOG(GetOwner(), LogCrowdFollowing, Error, TEXT("SetMoveSegment, unknown path type!"));
	}
}

void UCrowdFollowingComponent::UpdatePathSegment()
{
	if (!bEnableCrowdSimulation)
	{
		Super::UpdatePathSegment();
		return;
	}

	if (!Path.IsValid() || MovementComp == NULL)
	{
		AbortMove(TEXT("no path"), FAIRequestID::CurrentRequest, true, false, EPathFollowingMessage::NoPath);
		return;
	}

	if (!Path->IsValid())
	{
		if (!Path->IsWaitingForRepath())
		{
			AbortMove(TEXT("no path"), FAIRequestID::CurrentRequest, true, false, EPathFollowingMessage::NoPath);
		}
		return;
	}

	// if agent has control over its movement, check finish conditions
	const bool bCanReachTarget = MovementComp->CanStopPathFollowing();
	if (bCanReachTarget && Status == EPathFollowingStatus::Moving)
	{
		const FVector CurrentLocation = MovementComp->GetActorFeetLocation();
		const FVector GoalLocation = GetCurrentTargetLocation();

		if (bCollidedWithGoal)
		{
			// check if collided with goal actor
			OnSegmentFinished();
			OnPathFinished(EPathFollowingResult::Success);
		}
		else if (bFinalPathPart)
		{
			const FVector ToTarget = (GoalLocation - MovementComp->GetActorFeetLocation());
			const bool bDirectPath = Path->CastPath<FAbstractNavigationPath>() != NULL;
			const float SegmentDot = FVector::DotProduct(ToTarget, bDirectPath ? MovementComp->Velocity : CrowdAgentMoveDirection);
			const bool bMovedTooFar = bCheckMovementAngle && (SegmentDot < 0.0);

			// can't use HasReachedDestination here, because it will use last path point
			// which is not set correctly for partial paths without string pulling
			if (bMovedTooFar || HasReachedInternal(GoalLocation, 0.0f, 0.0f, CurrentLocation, AcceptanceRadius, bStopOnOverlap ? MinAgentRadiusPct : 0.0f))
			{
				UE_VLOG(GetOwner(), LogCrowdFollowing, Log, TEXT("Last path segment finished due to \'%s\'"), bMovedTooFar ? TEXT("Missing Last Point") : TEXT("Reaching Destination"));
				OnPathFinished(EPathFollowingResult::Success);
			}
		}
		else
		{
			// override radius multiplier and switch to next path part when closer than 4x agent radius
			const float NextPartMultiplier = 4.0f;
			const bool bHasReached = HasReachedInternal(GoalLocation, 0.0f, 0.0f, CurrentLocation, 0.0f, NextPartMultiplier);

			if (bHasReached)
			{
				SwitchToNextPathPart();
			}
		}
	}

	// gather location samples to detect if moving agent is blocked
	if (bCanReachTarget && Status == EPathFollowingStatus::Moving)
	{
		const bool bHasNewSample = UpdateBlockDetection();
		if (bHasNewSample && IsBlocked())
		{
			OnPathFinished(EPathFollowingResult::Blocked);
		}
	}
}

void UCrowdFollowingComponent::FollowPathSegment(float DeltaTime)
{
	if (!bEnableCrowdSimulation)
	{
		Super::FollowPathSegment(DeltaTime);
		return;
	}

	if (bUpdateDirectMoveVelocity)
	{
		const FVector CurrentTargetPt = DestinationActor.IsValid() ? DestinationActor->GetActorLocation() : GetCurrentTargetLocation();
		const FVector AgentLoc = GetCrowdAgentLocation();
		const FVector NewDirection = (CurrentTargetPt - AgentLoc).GetSafeNormal();

		const bool bDirectionChanged = !NewDirection.Equals(CrowdAgentMoveDirection);
		if (bDirectionChanged)
		{
			CurrentDestination.Set(Path->GetBaseActor(), CurrentTargetPt);
			CrowdAgentMoveDirection = NewDirection;
			MoveSegmentDirection = NewDirection;

			UCrowdManager* Manager = UCrowdManager::GetCurrent(GetWorld());
			Manager->SetAgentMoveDirection(this, NewDirection);

			UE_VLOG(GetOwner(), LogCrowdFollowing, Log, TEXT("Updated direct move direction for crowd agent."));
		}
	}

	UpdateMoveFocus();
}

FVector UCrowdFollowingComponent::GetMoveFocus(bool bAllowStrafe) const
{
	// can't really use CurrentDestination here, as it's pointing at end of path part
	// fallback to looking at point in front of agent

	if (!bAllowStrafe && MovementComp && bEnableCrowdSimulation)
	{
		const FVector AgentLoc = MovementComp->GetActorLocation();

		// if we're not moving, falling, or don't have a crowd agent move direction, set our focus to ahead of the rotation of our owner to keep the same rotation,
		// otherwise use the Crowd Agent Move Direction to move in the direction we're supposed to be going
		const FVector ForwardDir = MovementComp->GetOwner() && ((Status != EPathFollowingStatus::Moving) || (CharacterMovement && (CharacterMovement->MovementMode == MOVE_Falling)) || CrowdAgentMoveDirection.IsNearlyZero()) ?
			MovementComp->GetOwner()->GetActorForwardVector() :
			CrowdAgentMoveDirection;

		return AgentLoc + ForwardDir * 100.0f;
	}

	return Super::GetMoveFocus(bAllowStrafe);
}

void UCrowdFollowingComponent::OnNavNodeChanged(NavNodeRef NewPolyRef, NavNodeRef PrevPolyRef, int32 CorridorSize)
{
	if (bEnableCrowdSimulation && Status != EPathFollowingStatus::Idle)
	{
		// update last visited path poly
		FNavMeshPath* NavPath = Path.IsValid() ? Path->CastPath<FNavMeshPath>() : NULL;
		if (NavPath)
		{
			for (int32 Idx = LastPathPolyIndex; Idx < NavPath->PathCorridor.Num(); Idx++)
			{
				if (NavPath->PathCorridor[Idx] == NewPolyRef)
				{
					LastPathPolyIndex = Idx;
					break;
				}
			}
		}

		UE_VLOG(GetOwner(), LogCrowdFollowing, Verbose, TEXT("OnNavNodeChanged, CorridorSize:%d, LastVisitedIndex:%d"), CorridorSize, LastPathPolyIndex);

		const bool bSwitchPart = ShouldSwitchPathPart(CorridorSize);
		if (bSwitchPart && !bFinalPathPart)
		{
			SwitchToNextPathPart();
		}
	}
}

bool UCrowdFollowingComponent::ShouldSwitchPathPart(int32 CorridorSize) const
{
	return CorridorSize <= 2;
}

void UCrowdFollowingComponent::SwitchToNextPathPart()
{
	const int32 NewPartStart = DetermineStartingPathPoint(NULL);
	SetMoveSegment(NewPartStart);
}

void UCrowdFollowingComponent::GetDebugStringTokens(TArray<FString>& Tokens, TArray<EPathFollowingDebugTokens::Type>& Flags) const
{
	if (!bEnableCrowdSimulation)
	{
		Super::GetDebugStringTokens(Tokens, Flags);
		return;
	}

	Tokens.Add(GetStatusDesc());
	Flags.Add(EPathFollowingDebugTokens::Description);

	UCrowdManager* Manager = UCrowdManager::GetCurrent(GetWorld());
	if (Manager && !Manager->IsAgentValid(this))
	{
		Tokens.Add(TEXT("simulation"));
		Flags.Add(EPathFollowingDebugTokens::ParamName);
		Tokens.Add(TEXT("NOT ACTIVE"));
		Flags.Add(EPathFollowingDebugTokens::FailedValue);
	}

	if (Status != EPathFollowingStatus::Moving)
	{
		return;
	}

	FString& StatusDesc = Tokens[0];
	if (Path.IsValid())
	{
		FNavMeshPath* NavMeshPath = Path->CastPath<FNavMeshPath>();
		if (NavMeshPath)
		{
			StatusDesc += FString::Printf(TEXT(" (path:%d, visited:%d)"), PathStartIndex, LastPathPolyIndex);
		}
		else if (Path->CastPath<FAbstractNavigationPath>())
		{
			StatusDesc += TEXT(" (direct)");
		}
		else
		{
			StatusDesc += TEXT(" (unknown path)");
		}
	}

	// get cylinder of moving agent
	float AgentRadius = 0.0f;
	float AgentHalfHeight = 0.0f;
	AActor* MovingAgent = MovementComp->GetOwner();
	MovingAgent->GetSimpleCollisionCylinder(AgentRadius, AgentHalfHeight);

	if (bFinalPathPart)
	{
		float CurrentDot = 0.0f, CurrentDistance = 0.0f, CurrentHeight = 0.0f;
		uint8 bFailedDot = 0, bFailedDistance = 0, bFailedHeight = 0;
		DebugReachTest(CurrentDot, CurrentDistance, CurrentHeight, bFailedHeight, bFailedDistance, bFailedHeight);

		Tokens.Add(TEXT("dist2D"));
		Flags.Add(EPathFollowingDebugTokens::ParamName);
		Tokens.Add(FString::Printf(TEXT("%.0f"), CurrentDistance));
		Flags.Add(bFailedDistance ? EPathFollowingDebugTokens::FailedValue : EPathFollowingDebugTokens::PassedValue);

		Tokens.Add(TEXT("distZ"));
		Flags.Add(EPathFollowingDebugTokens::ParamName);
		Tokens.Add(FString::Printf(TEXT("%.0f"), CurrentHeight));
		Flags.Add(bFailedHeight ? EPathFollowingDebugTokens::FailedValue : EPathFollowingDebugTokens::PassedValue);
	}
	else
	{
		const FVector CurrentLocation = MovementComp->GetActorFeetLocation();

		// make sure we're not too close to end of path part (poly count can always fail when AI goes off path)
		const float DistSq = (GetCurrentTargetLocation() - CurrentLocation).SizeSquared();
		const float PathSwitchThresSq = FMath::Square(AgentRadius * 5.0f);

		Tokens.Add(TEXT("distance"));
		Flags.Add(EPathFollowingDebugTokens::ParamName);
		Tokens.Add(FString::Printf(TEXT("%.0f"), FMath::Sqrt(DistSq)));
		Flags.Add((DistSq < PathSwitchThresSq) ? EPathFollowingDebugTokens::PassedValue : EPathFollowingDebugTokens::FailedValue);
	}
}

#if ENABLE_VISUAL_LOG

void UCrowdFollowingComponent::DescribeSelfToVisLog(FVisualLogEntry* Snapshot) const
{
	if (!bEnableCrowdSimulation)
	{
		Super::DescribeSelfToVisLog(Snapshot);
		return;
	}

	FVisualLogStatusCategory Category;
	Category.Category = TEXT("Path following");

	if (DestinationActor.IsValid())
	{
		Category.Add(TEXT("Goal"), GetNameSafe(DestinationActor.Get()));
	}

	FString StatusDesc = GetStatusDesc();

	FNavMeshPath* NavMeshPath = Path.IsValid() ? Path->CastPath<FNavMeshPath>() : NULL;
	FAbstractNavigationPath* DirectPath = Path.IsValid() ? Path->CastPath<FAbstractNavigationPath>() : NULL;

	if (Status == EPathFollowingStatus::Moving)
	{
		StatusDesc += FString::Printf(TEXT(" [path:%d, visited:%d]"), PathStartIndex, LastPathPolyIndex);
	}

	Category.Add(TEXT("Status"), StatusDesc);
	Category.Add(TEXT("Path"), !Path.IsValid() ? TEXT("none") : NavMeshPath ? TEXT("navmesh") : DirectPath ? TEXT("direct") : TEXT("unknown"));

	UObject* CustomLinkOb = GetCurrentCustomLinkOb();
	if (CustomLinkOb)
	{
		Category.Add(TEXT("SmartLink"), CustomLinkOb->GetName());
	}

	UCrowdManager* Manager = UCrowdManager::GetCurrent(GetWorld());
	if (Manager && !Manager->IsAgentValid(this))
	{
		Category.Add(TEXT("Simulation"), TEXT("unable to register!"));
	}

	Snapshot->Status.Add(Category);
}

#endif // ENABLE_VISUAL_LOG
