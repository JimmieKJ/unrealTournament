// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AIModulePrivate.h"
#if WITH_RECAST
#	include "Detour/DetourNavMeshQuery.h"
#endif
#include "AI/Navigation/AbstractNavData.h"
#include "AI/Navigation/RecastNavMesh.h"
#include "AI/Navigation/NavLinkCustomInterface.h"
#include "GameFramework/NavMovementComponent.h"
#include "GameFramework/Character.h"
#include "Engine/Canvas.h"
#include "TimerManager.h"

#include "Navigation/PathFollowingComponent.h"

#define USE_PHYSIC_FOR_VISIBILITY_TESTS 1 // Physic will be used for visibility tests if set or only raycasts on navmesh if not

DEFINE_LOG_CATEGORY(LogPathFollowing);

//----------------------------------------------------------------------//
// Life cycle                                                        
//----------------------------------------------------------------------//
uint32 UPathFollowingComponent::NextRequestId = 1;
const float UPathFollowingComponent::DefaultAcceptanceRadius = -1.f;
UPathFollowingComponent::FRequestCompletedSignature UPathFollowingComponent::UnboundRequestDelegate;

UPathFollowingComponent::UPathFollowingComponent(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = true;
	bAutoActivate = true;

	MinAgentRadiusPct = 1.1f;
	MinAgentHalfHeightPct = 1.05f;
	BlockDetectionDistance = 10.0f;
	BlockDetectionInterval = 0.5f;
	BlockDetectionSampleCount = 10;
	LastSampleTime = 0.0f;
	NextSampleIdx = 0;
	bUseBlockDetection = true;
	bLastMoveReachedGoal = false;
	bUseVisibilityTestsSimplification = false;
	bStopMovementOnFinish = true;

	MoveSegmentStartIndex = 0;
	MoveSegmentEndIndex = 1;
	MoveSegmentStartRef = INVALID_NAVNODEREF;
	MoveSegmentEndRef = INVALID_NAVNODEREF;

	bStopOnOverlap = true;
	Status = EPathFollowingStatus::Idle;
}

void UPathFollowingComponent::LogPathHelper(const AActor* LogOwner, FNavigationPath* InLogPath, const AActor* LogGoalActor)
{
#if ENABLE_VISUAL_LOG
	FVisualLogger& Vlog = FVisualLogger::Get();
	if (Vlog.IsRecording() &&
		InLogPath && InLogPath->IsValid() && InLogPath->GetPathPoints().Num())
	{
		FVisualLogEntry* Entry = Vlog.GetEntryToWrite(LogOwner, LogOwner->GetWorld()->TimeSeconds);
		InLogPath->DescribeSelfToVisLog(Entry);

		const FVector PathEnd = *InLogPath->GetPathPointLocation(InLogPath->GetPathPoints().Num() - 1);
		if (LogGoalActor)
		{
			const FVector GoalLoc = LogGoalActor->GetActorLocation();
			if (FVector::DistSquared(GoalLoc, PathEnd) > 1.0f)
			{
				UE_VLOG_LOCATION(LogOwner, LogPathFollowing, Verbose, GoalLoc, 30, FColor::Green, TEXT("GoalActor"));
				UE_VLOG_SEGMENT(LogOwner, LogPathFollowing, Verbose, GoalLoc, PathEnd, FColor::Green, TEXT_EMPTY);
			}
		}

		UE_VLOG_BOX(LogOwner, LogPathFollowing, Verbose, FBox(PathEnd - FVector(30.0f), PathEnd + FVector(30.0f)), FColor::Green, TEXT("PathEnd"));
	}
#endif // ENABLE_VISUAL_LOG
}

void UPathFollowingComponent::LogPathHelper(const AActor* LogOwner, FNavPathSharedPtr InLogPath, const AActor* LogGoalActor)
{
#if ENABLE_VISUAL_LOG
	if (InLogPath.IsValid())
	{
		LogPathHelper(LogOwner, InLogPath.Get(), LogGoalActor);
	}
#endif // ENABLE_VISUAL_LOG
}

void LogBlockHelper(AActor* LogOwner, UNavMovementComponent* MoveComp, float RadiusPct, float HeightPct, const FVector& SegmentStart, const FVector& SegmentEnd)
{
#if ENABLE_VISUAL_LOG
	if (MoveComp && LogOwner)
	{
		const FVector AgentLocation = MoveComp->GetActorFeetLocation();
		const FVector ToTarget = (SegmentEnd - AgentLocation);
		const float SegmentDot = FVector::DotProduct(ToTarget.GetSafeNormal(), (SegmentEnd - SegmentStart).GetSafeNormal());
		UE_VLOG(LogOwner, LogPathFollowing, Verbose, TEXT("[agent to segment end] dot [segment dir]: %f"), SegmentDot);
		
		float AgentRadius = 0.0f;
		float AgentHalfHeight = 0.0f;
		AActor* MovingAgent = MoveComp->GetOwner();
		MovingAgent->GetSimpleCollisionCylinder(AgentRadius, AgentHalfHeight);

		const float Dist2D = ToTarget.Size2D();
		UE_VLOG(LogOwner, LogPathFollowing, Verbose, TEXT("dist 2d: %f (agent radius: %f [%f])"), Dist2D, AgentRadius, AgentRadius * (1 + RadiusPct));

		const float ZDiff = FMath::Abs(ToTarget.Z);
		UE_VLOG(LogOwner, LogPathFollowing, Verbose, TEXT("Z diff: %f (agent halfZ: %f [%f])"), ZDiff, AgentHalfHeight, AgentHalfHeight * (1 + HeightPct));
	}
#endif // ENABLE_VISUAL_LOG
}

FString GetPathDescHelper(FNavPathSharedPtr Path)
{
	return !Path.IsValid() ? TEXT("missing") :
		!Path->IsValid() ? TEXT("invalid") :
		FString::Printf(TEXT("%s:%d"), Path->IsPartial() ? TEXT("partial") : TEXT("complete"), Path->GetPathPoints().Num());
}

void UPathFollowingComponent::OnPathEvent(FNavigationPath* InvalidatedPath, ENavPathEvent::Type Event)
{
	const static UEnum* NavPathEventEnum = FindObject<UEnum>(ANY_PACKAGE, TEXT("ENavPathEvent"));
	UE_VLOG(GetOwner(), LogPathFollowing, Log, TEXT("OnPathEvent: %s"), *NavPathEventEnum->GetEnumName(Event));

	if (InvalidatedPath == nullptr || Path.Get() != InvalidatedPath)
	{
		return;
	}

	switch (Event)
	{
		case ENavPathEvent::UpdatedDueToGoalMoved:
		case ENavPathEvent::UpdatedDueToNavigationChanged:
		{
			UpdateMove(Path, GetCurrentRequestId());
		}
		break;
	}
}

FAIRequestID UPathFollowingComponent::RequestMove(FNavPathSharedPtr InPath, FRequestCompletedSignature OnComplete,
	const AActor* InDestinationActor, float InAcceptanceRadius, bool bInStopOnOverlap, FCustomMoveSharedPtr InGameData)
{
	UE_VLOG(GetOwner(), LogPathFollowing, Log, TEXT("RequestMove: Path(%s), AcceptRadius(%.1f%s), DestinationActor(%s), GameData(%s)"),
		*GetPathDescHelper(InPath),
		InAcceptanceRadius, bInStopOnOverlap ? TEXT(" + agent") : TEXT(""),
		*GetNameSafe(InDestinationActor),
		!InGameData.IsValid() ? TEXT("missing") : TEXT("valid"));

	LogPathHelper(GetOwner(), InPath, InDestinationActor);

	if (InAcceptanceRadius == UPathFollowingComponent::DefaultAcceptanceRadius)
	{
		InAcceptanceRadius = MyDefaultAcceptanceRadius;
	}

	if (ResourceLock.IsLocked())
	{
		UE_VLOG(GetOwner(), LogPathFollowing, Log, TEXT("Rejecting move request due to resource lock by %s"), *ResourceLock.GetLockPriorityName());
		return FAIRequestID::InvalidRequest;
	}

	if (!InPath.IsValid() || InAcceptanceRadius < 0.0f)
	{
		UE_VLOG(GetOwner(), LogPathFollowing, Log, TEXT("RequestMove: invalid request"));
		return FAIRequestID::InvalidRequest;
	}

	// try to grab movement component
	if (!UpdateMovementComponent())
	{
		UE_VLOG(GetOwner(), LogPathFollowing, Warning, TEXT("RequestMove: missing movement component"));
		return FAIRequestID::InvalidRequest;
	}

	// update ID first, so any observer notified by AbortMove() could detect new request
	const uint32 PrevMoveId = CurrentRequestId;
	
	// abort previous movement
	if (Status == EPathFollowingStatus::Paused && Path.IsValid() && InPath.Get() == Path.Get() && DestinationActor == InDestinationActor)
	{
		ResumeMove();
	}
	else
	{
		if (Status == EPathFollowingStatus::Moving)
		{
			const bool bResetVelocity = false;
			const bool bFinishAsSkipped = true;
			AbortMove(TEXT("new request"), PrevMoveId, bResetVelocity, bFinishAsSkipped, EPathFollowingMessage::OtherRequest);
		}
		
		Reset();

		StoreRequestId();

		// store new data
		Path = InPath;
		Path->AddObserver(FNavigationPath::FPathObserverDelegate::FDelegate::CreateUObject(this, &UPathFollowingComponent::OnPathEvent));
		if (MovementComp && MovementComp->GetOwner())
		{
			Path->SetSourceActor(*(MovementComp->GetOwner()));
		}

		PathTimeWhenPaused = 0.0f;
		OnPathUpdated();

		AcceptanceRadius = InAcceptanceRadius;
		GameData = InGameData;
		OnRequestFinished = OnComplete;	
		bStopOnOverlap = bInStopOnOverlap;
		SetDestinationActor(InDestinationActor);

	#if ENABLE_VISUAL_LOG
		const FVector CurrentLocation = MovementComp ? MovementComp->GetActorFeetLocation() : FVector::ZeroVector;
		const FVector DestLocation = InPath->GetDestinationLocation();
		const FVector ToDest = DestLocation - CurrentLocation;
		UE_VLOG(GetOwner(), LogPathFollowing, Log, TEXT("RequestMove: accepted, ID(%u) dist2D(%.0f) distZ(%.0f)"),
			CurrentRequestId.GetID(), ToDest.Size2D(), FMath::Abs(ToDest.Z));
	#endif // ENABLE_VISUAL_LOG

		// with async pathfinding paths can be incomplete, movement will start after receiving UpdateMove 
		if (Path->IsValid())
		{
			Status = EPathFollowingStatus::Moving;

			// determine with path segment should be followed
			const uint32 CurrentSegment = DetermineStartingPathPoint(InPath.Get());
			SetMoveSegment(CurrentSegment);
		}
		else
		{
			Status = EPathFollowingStatus::Waiting;
		}
	}

	return CurrentRequestId;
}

bool UPathFollowingComponent::UpdateMove(FNavPathSharedPtr InPath, FAIRequestID RequestID)
{
	UE_VLOG(GetOwner(), LogPathFollowing, Log, TEXT("UpdateMove: Path(%s) Status(%s) RequestID(%u)"),
		*GetPathDescHelper(InPath),
		*GetStatusDesc(), RequestID);

	LogPathHelper(GetOwner(), InPath, DestinationActor.Get());

	if (!InPath.IsValid() || !InPath->IsValid() || Status == EPathFollowingStatus::Idle 
		|| RequestID.IsEquivalent(GetCurrentRequestId()) == false)
	{
		UE_VLOG(GetOwner(), LogPathFollowing, Log, TEXT("UpdateMove: invalid request"));
		return false;
	}

	Path = InPath;
	OnPathUpdated();

	if (Status == EPathFollowingStatus::Waiting || Status == EPathFollowingStatus::Moving)
	{
		Status = EPathFollowingStatus::Moving;

		const int32 CurrentSegment = DetermineStartingPathPoint(InPath.Get());
		SetMoveSegment(CurrentSegment);
	}

	return true;
}

void UPathFollowingComponent::AbortMove(const FString& Reason, FAIRequestID RequestID, bool bResetVelocity, bool bSilent, uint8 MessageFlags)
{
	UE_VLOG(GetOwner(), LogPathFollowing, Log, TEXT("AbortMove: Reason(%s) RequestID(%u)"), *Reason, RequestID);

	if ((Status != EPathFollowingStatus::Idle) && RequestID.IsEquivalent(GetCurrentRequestId()))
	{
		const EPathFollowingResult::Type FinishResult = bSilent ? EPathFollowingResult::Skipped : EPathFollowingResult::Aborted;

		INavLinkCustomInterface* CustomNavLink = Cast<INavLinkCustomInterface>(CurrentCustomLinkOb.Get());
		if (CustomNavLink)
		{
			CustomNavLink->OnLinkMoveFinished(this);
		}

		// save data required for observers before reseting temporary variables
		const uint32 AbortedMoveId = RequestID ? RequestID : CurrentRequestId;
		FRequestCompletedSignature SavedReqFinished = OnRequestFinished;

		Reset();
		bLastMoveReachedGoal = false;
		UpdateMoveFocus();

		if (bResetVelocity && MovementComp && MovementComp->CanStopPathFollowing())
		{
			MovementComp->StopMovementKeepPathing();
		}

		// notify observers after state was reset (they can request another move)
		SavedReqFinished.ExecuteIfBound(FinishResult);
		OnMoveFinished.Broadcast(AbortedMoveId, FinishResult);

		FAIMessage Msg(UBrainComponent::AIMessage_MoveFinished, this, AbortedMoveId, FAIMessage::Failure);
		Msg.SetFlag(MessageFlags);

		FAIMessage::Send(Cast<AController>(GetOwner()), Msg);
	}
}

void UPathFollowingComponent::PauseMove(FAIRequestID RequestID, bool bResetVelocity)
{
	UE_VLOG(GetOwner(), LogPathFollowing, Log, TEXT("PauseMove: RequestID(%u)"), RequestID);
	if (Status == EPathFollowingStatus::Paused)
	{
		return;
	}

	if (RequestID.IsEquivalent(GetCurrentRequestId()))
	{
		if (bResetVelocity && MovementComp && MovementComp->CanStopPathFollowing())
		{
			MovementComp->StopMovementKeepPathing();
		}

		LocationWhenPaused = MovementComp ? MovementComp->GetActorFeetLocation() : FVector::ZeroVector;
		PathTimeWhenPaused = Path.IsValid() ? Path->GetTimeStamp() : 0.0f;
		Status = EPathFollowingStatus::Paused;

		UpdateMoveFocus();
	}

	// TODO: pause path updates with goal movement
}

void UPathFollowingComponent::ResumeMove(FAIRequestID RequestID)
{
	if (RequestID.IsEquivalent(CurrentRequestId) && RequestID.IsValid())
	{
		UE_VLOG(GetOwner(), LogPathFollowing, Log, TEXT("ResumeMove: RequestID(%u)"), RequestID);

		const bool bMovedDuringPause = ShouldCheckPathOnResume();
		const bool bIsOnPath = IsOnPath();
		if (bIsOnPath)
		{
			Status = EPathFollowingStatus::Moving;

			const bool bWasPathUpdatedRecently = Path.IsValid() ? (Path->GetTimeStamp() > PathTimeWhenPaused) : false;
			if (bMovedDuringPause || bWasPathUpdatedRecently)
			{
				const int32 CurrentSegment = DetermineStartingPathPoint(Path.Get());
				SetMoveSegment(CurrentSegment);
			}
			else
			{
				UpdateMoveFocus();
			}
		}
		else if (Path.IsValid() && Path->IsValid() && Path->GetNavigationDataUsed() == NULL)
		{
			// this means it's a scripted path, just resume
			Status = EPathFollowingStatus::Moving;
			UpdateMoveFocus();
		}
		else
		{
			OnPathFinished(EPathFollowingResult::OffPath);
		}
	}
	else
	{
		UE_VLOG(GetOwner(), LogPathFollowing, Log, TEXT("ResumeMove: RequestID(%u) is neither \'AnyRequest\' not CurrentRequestId(%u). Ignoring.")
			, RequestID, CurrentRequestId);
	}
}

bool UPathFollowingComponent::ShouldCheckPathOnResume() const
{
	bool bCheckPath = true;
	if (MovementComp != NULL)
	{
		float AgentRadius = 0.0f, AgentHalfHeight = 0.0f;
		MovementComp->GetOwner()->GetSimpleCollisionCylinder(AgentRadius, AgentHalfHeight);

		const FVector CurrentLocation = MovementComp->GetActorFeetLocation();
		const float DeltaMove2DSq = (CurrentLocation - LocationWhenPaused).SizeSquared2D();
		const float DeltaZ = FMath::Abs(CurrentLocation.Z - LocationWhenPaused.Z);
		if (DeltaMove2DSq < FMath::Square(AgentRadius) && DeltaZ < (AgentHalfHeight * 0.5f))
		{
			bCheckPath = false;
		}
	}

	return bCheckPath;
}

void UPathFollowingComponent::OnPathFinished(EPathFollowingResult::Type Result)
{
	UE_VLOG(GetOwner(), LogPathFollowing, Log, TEXT("OnPathFinished: %s"), *GetResultDesc(Result));
	
	// save move status
	bLastMoveReachedGoal = (Result == EPathFollowingResult::Success) && !HasPartialPath();

	// save data required for observers before reseting temporary variables
	const FAIRequestID FinishedMoveId = CurrentRequestId;
	FRequestCompletedSignature SavedReqFinished = OnRequestFinished;

	Reset();
	UpdateMoveFocus();

	if (MovementComp && MovementComp->CanStopPathFollowing() && bStopMovementOnFinish)
	{
		MovementComp->StopMovementKeepPathing();
	}

	// notify observers after state was reset (they can request another move)
	SavedReqFinished.ExecuteIfBound(Result);
	OnMoveFinished.Broadcast(FinishedMoveId, Result);

	FAIMessage::Send(Cast<AController>(GetOwner()), FAIMessage(UBrainComponent::AIMessage_MoveFinished, this, FinishedMoveId, (Result == EPathFollowingResult::Success)));
}

void UPathFollowingComponent::OnSegmentFinished()
{
	UE_VLOG(GetOwner(), LogPathFollowing, Verbose, TEXT("OnSegmentFinished"));
}

void UPathFollowingComponent::OnPathUpdated()
{
}

void UPathFollowingComponent::Initialize()
{
	UpdateCachedComponents();
}

void UPathFollowingComponent::Cleanup()
{
	// empty in base class
}

void UPathFollowingComponent::UpdateCachedComponents()
{
	UpdateMovementComponent(/*bForce=*/true);
}

void UPathFollowingComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (Status == EPathFollowingStatus::Moving)
	{
		// check finish conditions, update current segment if needed
		// may result in changing current status!
		UpdatePathSegment();
	}

	if (Status == EPathFollowingStatus::Moving)
	{
		// follow current path segment
		FollowPathSegment(DeltaTime);
	}
};

void UPathFollowingComponent::SetMovementComponent(UNavMovementComponent* MoveComp)
{
	MovementComp = MoveComp;
	MyNavData = NULL;

	if (MoveComp != NULL)
	{
		const FNavAgentProperties& NavAgentProps = MoveComp->GetNavAgentPropertiesRef();
		MyDefaultAcceptanceRadius = NavAgentProps.AgentRadius;
		MoveComp->PathFollowingComp = this;

		if (GetWorld() && GetWorld()->GetNavigationSystem())
		{	
			MyNavData = GetWorld()->GetNavigationSystem()->GetNavDataForProps(NavAgentProps);
		}
	}
}

FVector UPathFollowingComponent::GetMoveFocus(bool bAllowStrafe) const
{
	FVector MoveFocus = FVector::ZeroVector;
	if (bAllowStrafe && DestinationActor.IsValid())
	{
		MoveFocus = DestinationActor->GetActorLocation();
	}
	else
	{
		const FVector CurrentMoveDirection = GetCurrentDirection();
		MoveFocus = *CurrentDestination + (CurrentMoveDirection * 20.0f);
	}

	return MoveFocus;
}

void UPathFollowingComponent::Reset()
{
	MoveSegmentStartIndex = 0;
	MoveSegmentStartRef = INVALID_NAVNODEREF;
	MoveSegmentEndRef = INVALID_NAVNODEREF;

	LocationSamples.Reset();
	LastSampleTime = 0.0f;
	NextSampleIdx = 0;

	Path.Reset();
	GameData.Reset();
	DestinationActor.Reset();
	OnRequestFinished.Unbind();
	AcceptanceRadius = MyDefaultAcceptanceRadius;
	bStopOnOverlap = true;
	bCollidedWithGoal = false;

	CurrentRequestId = FAIRequestID::InvalidRequest;

	CurrentDestination.Clear();

	Status = EPathFollowingStatus::Idle;
}

int32 UPathFollowingComponent::DetermineStartingPathPoint(const FNavigationPath* ConsideredPath) const
{
	int32 PickedPathPoint = INDEX_NONE;

	if (ConsideredPath && ConsideredPath->IsValid())
	{
		// if we already have some info on where we were on previous path
		// we can find out if there's a segment on new path we're currently at
		if (MoveSegmentStartRef != INVALID_NAVNODEREF &&
			MoveSegmentEndRef != INVALID_NAVNODEREF &&
			ConsideredPath->GetNavigationDataUsed() != NULL)
		{
			// iterate every new path node and see if segment match
			for (int32 PathPoint = 0; PathPoint < ConsideredPath->GetPathPoints().Num() - 1; ++PathPoint)
			{
				if (ConsideredPath->GetPathPoints()[PathPoint].NodeRef == MoveSegmentStartRef &&
					ConsideredPath->GetPathPoints()[PathPoint + 1].NodeRef == MoveSegmentEndRef)
				{
					PickedPathPoint = PathPoint;
					break;
				}
			}
		}

		if (MovementComp && PickedPathPoint == INDEX_NONE)
		{
			if (ConsideredPath->GetPathPoints().Num() > 2)
			{
				// check if is closer to first or second path point (don't assume AI's standing)
				const FVector CurrentLocation = MovementComp->GetActorFeetLocation();
				const FVector PathPt0 = *ConsideredPath->GetPathPointLocation(0);
				const FVector PathPt1 = *ConsideredPath->GetPathPointLocation(1);
				// making this test in 2d to avoid situation where agent's Z location not being in "navmesh plane"
				// would influence the result
				const float SqDistToFirstPoint = (CurrentLocation - PathPt0).SizeSquared2D();
				const float SqDistToSecondPoint = (CurrentLocation - PathPt1).SizeSquared2D();
				PickedPathPoint = (SqDistToFirstPoint < SqDistToSecondPoint) ? 0 : 1;
			}
			else
			{
				// If there are only two point we probably should start from the beginning
				PickedPathPoint = 0;
			}
		}
	}

	return PickedPathPoint;
}

void UPathFollowingComponent::SetDestinationActor(const AActor* InDestinationActor)
{
	DestinationActor = InDestinationActor;
	DestinationAgent = Cast<const INavAgentInterface>(InDestinationActor);
	MoveOffset = DestinationAgent ? DestinationAgent->GetMoveGoalOffset(GetOwner()) : FVector::ZeroVector;
}

void UPathFollowingComponent::SetMoveSegment(int32 SegmentStartIndex)
{
	int32 EndSegmentIndex = SegmentStartIndex + 1;
	if (Path.IsValid() && Path->GetPathPoints().IsValidIndex(SegmentStartIndex) && Path->GetPathPoints().IsValidIndex(EndSegmentIndex))
	{
		EndSegmentIndex = DetermineCurrentTargetPathPoint(SegmentStartIndex);

		MoveSegmentStartIndex = SegmentStartIndex;
		MoveSegmentEndIndex = EndSegmentIndex;
		const FNavPathPoint& PathPt0 = Path->GetPathPoints()[MoveSegmentStartIndex];
		const FNavPathPoint& PathPt1 = Path->GetPathPoints()[MoveSegmentEndIndex];

		MoveSegmentStartRef = PathPt0.NodeRef;
		MoveSegmentEndRef = PathPt1.NodeRef;
		
		CurrentDestination = Path->GetPathPointLocation(MoveSegmentEndIndex);
		const FVector SegmentStart = *Path->GetPathPointLocation(MoveSegmentStartIndex);
		FVector SegmentEnd = *CurrentDestination;

		// make sure we have a non-zero direction if still following a valid path
		if (SegmentStart.Equals(SegmentEnd) && Path->GetPathPoints().IsValidIndex(MoveSegmentEndIndex + 1))
		{
			MoveSegmentEndIndex++;

			CurrentDestination = Path->GetPathPointLocation(MoveSegmentEndIndex);
			SegmentEnd = *CurrentDestination;
		}

		CurrentAcceptanceRadius = (Path->GetPathPoints().Num() == (MoveSegmentEndIndex + 1)) ? AcceptanceRadius : 0.0f;
		MoveSegmentDirection = (SegmentEnd - SegmentStart).GetSafeNormal();

		// handle moving through custom nav links
		if (PathPt0.CustomLinkId)
		{
			UNavigationSystem* NavSys = UNavigationSystem::GetCurrent(GetWorld());
			INavLinkCustomInterface* CustomNavLink = NavSys->GetCustomLink(PathPt0.CustomLinkId);
			StartUsingCustomLink(CustomNavLink, SegmentEnd);
		}

		// update move focus in owning AI
		UpdateMoveFocus();
	}
}

int32 UPathFollowingComponent::DetermineCurrentTargetPathPoint(int32 StartIndex)
{
	if (!bUseVisibilityTestsSimplification ||
		Path->GetPathPoints()[StartIndex].CustomLinkId ||
		Path->GetPathPoints()[StartIndex + 1].CustomLinkId)
	{
		return StartIndex + 1;
	}


#if WITH_RECAST
	FNavMeshNodeFlags Flags0(Path->GetPathPoints()[StartIndex].Flags);
	FNavMeshNodeFlags Flags1(Path->GetPathPoints()[StartIndex + 1].Flags);

	const bool bOffMesh0 = (Flags0.PathFlags & RECAST_STRAIGHTPATH_OFFMESH_CONNECTION) != 0;
	const bool bOffMesh1 = (Flags1.PathFlags & RECAST_STRAIGHTPATH_OFFMESH_CONNECTION) != 0;

	if ((Flags0.Area != Flags1.Area) || (bOffMesh0 && bOffMesh1))
	{
		return StartIndex + 1;
	}
#endif

	return OptimizeSegmentVisibility(StartIndex);
}

int32 UPathFollowingComponent::OptimizeSegmentVisibility(int32 StartIndex)
{
	//SCOPE_CYCLE_COUNTER(STAT_Navigation_PathVisibilityOptimisation);

	const AAIController* MyAI = Cast<AAIController>(GetOwner());
	const ANavigationData* NavData = Path.IsValid() ? Path->GetNavigationDataUsed() : NULL;
	if (NavData == NULL || MyAI == NULL)
	{
		return StartIndex + 1;
	}

	const bool bIsDirect = (Path->CastPath<FAbstractNavigationPath>() != NULL);
	if (bIsDirect)
	{
		// can't optimize anything without real corridor
		return StartIndex + 1;
	}

	const APawn* MyPawn = MyAI->GetPawn();
	const float PawnHalfHeight = (MyAI->GetCharacter() && MyAI->GetCharacter()->GetCapsuleComponent()) ? MyAI->GetCharacter()->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 0.0f;
	const FVector PawnLocation = MyPawn->GetActorLocation();

#if USE_PHYSIC_FOR_VISIBILITY_TESTS
	static const FName NAME_NavAreaTrace = TEXT("NavmeshVisibilityTrace");
	float Radius, HalfHeight;
	MyPawn->GetSimpleCollisionCylinder(Radius, HalfHeight);
	const FCollisionShape CollisionShape = FCollisionShape::MakeCapsule(Radius, HalfHeight);
	FCollisionQueryParams CapsuleParams(NAME_NavAreaTrace, false, MyAI->GetCharacter());
	UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(MyPawn->GetRootComponent());
	if (PrimitiveComponent)
	{
		CapsuleParams.AddIgnoredActors(PrimitiveComponent->MoveIgnoreActors);
	}

#endif
	TSharedPtr<FNavigationQueryFilter> QueryFilter = Path->GetFilter()->GetCopy();
#if WITH_RECAST
	const uint8 StartArea = FNavMeshNodeFlags(Path->GetPathPoints()[StartIndex].Flags).Area;
	TArray<float> CostArray;
	CostArray.Init(DT_UNWALKABLE_POLY_COST, DT_MAX_AREAS);
	CostArray[StartArea] = 0;
	QueryFilter->SetAllAreaCosts(CostArray);
#endif

	const int32 MaxPoints = Path->GetPathPoints().Num();
	int32 Index = StartIndex + 2;
	Path->ShortcutNodeRefs.Reset();

	for (; Index <= MaxPoints; ++Index)
	{
		if (!Path->GetPathPoints().IsValidIndex(Index))
		{
			break;
		}

		const FNavPathPoint& PathPt1 = Path->GetPathPoints()[Index];
		const FVector PathPt1Location = *Path->GetPathPointLocation(Index);
		const FVector AdjustedDestination = PathPt1Location + FVector(0.0f, 0.0f, PawnHalfHeight);

		FVector HitLocation;
#if WITH_RECAST
		ARecastNavMesh::FRaycastResult RaycastResult;
		const bool RaycastHitResult = ARecastNavMesh::NavMeshRaycast(NavData, PawnLocation, AdjustedDestination, HitLocation, QueryFilter, MyAI, RaycastResult);
		if (Path.IsValid())
		{
			Path->ShortcutNodeRefs.Reserve(RaycastResult.CorridorPolysCount);
			Path->ShortcutNodeRefs.SetNumUninitialized(RaycastResult.CorridorPolysCount);

			FPlatformMemory::Memcpy(Path->ShortcutNodeRefs.GetData(), RaycastResult.CorridorPolys, RaycastResult.CorridorPolysCount * sizeof(NavNodeRef));
		}
#else
		const bool RaycastHitResult = NavData->Raycast(PawnLocation, AdjustedDestination, HitLocation, QueryFilter);
#endif
		if (RaycastHitResult)
		{
			break;
		}
#if USE_PHYSIC_FOR_VISIBILITY_TESTS
		else
		{
			FHitResult Result;
			if (GetWorld()->SweepSingleByChannel(Result, PawnLocation, AdjustedDestination, FQuat::Identity, ECC_WorldStatic, CollisionShape, CapsuleParams))
			{
				break;
			}
		}
#endif

#if WITH_RECAST
		/** don't move to next point if we are changing area, we are at off mesh connection or it's a custom nav link*/
		if (FNavMeshNodeFlags(Path->GetPathPoints()[StartIndex].Flags).Area != FNavMeshNodeFlags(PathPt1.Flags).Area ||
			FNavMeshNodeFlags(PathPt1.Flags).PathFlags & RECAST_STRAIGHTPATH_OFFMESH_CONNECTION || 
			PathPt1.CustomLinkId)
		{
			return Index;
		}
#endif
	}

	return Index-1;
}

void UPathFollowingComponent::UpdatePathSegment()
{
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
		const int32 LastSegmentEndIndex = Path->GetPathPoints().Num() - 1;
		const bool bFollowingLastSegment = (MoveSegmentEndIndex >= LastSegmentEndIndex);

		if (bCollidedWithGoal)
		{
			// check if collided with goal actor
			OnSegmentFinished();
			OnPathFinished(EPathFollowingResult::Success);
		}
		else if (HasReachedDestination(CurrentLocation))
		{
			// always check for destination, acceptance radius may cause it to pass before reaching last segment
			OnSegmentFinished();
			OnPathFinished(EPathFollowingResult::Success);
		}
		else if (bFollowingLastSegment)
		{
			// use goal actor for end of last path segment
			// UNLESS it's partial path (can't reach goal)
			if (DestinationActor.IsValid() && Path->IsPartial() == false)
			{
				const FVector AgentLocation = DestinationAgent ? DestinationAgent->GetNavAgentLocation() : DestinationActor->GetActorLocation();
				// note that the condition below requires GoalLocation to be in world space.
				const FVector GoalLocation = FQuatRotationTranslationMatrix(DestinationActor->GetActorQuat(), AgentLocation).TransformPosition(MoveOffset);
				FVector HitLocation;

				if (MyNavData == nullptr //|| MyNavData->DoesNodeContainLocation(Path->GetPathPoints().Last().NodeRef, GoalLocation))
					|| (FVector::DistSquared(GoalLocation, *CurrentDestination) > SMALL_NUMBER &&  MyNavData->Raycast(CurrentLocation, GoalLocation, HitLocation, nullptr) == false))
				{
					CurrentDestination.Set(NULL, GoalLocation);

					UE_VLOG(this, LogPathFollowing, Log, TEXT("Moving directly to move goal rather than following last path segment"));
					UE_VLOG_LOCATION(this, LogPathFollowing, VeryVerbose, GoalLocation, 30, FColor::Green, TEXT("Last-segment-to-actor"));
					UE_VLOG_SEGMENT(this, LogPathFollowing, VeryVerbose, CurrentLocation, GoalLocation, FColor::Green, TEXT_EMPTY);
				}
			}

			UpdateMoveFocus();
		}
		// check if current move segment is finished
		else if (HasReachedCurrentTarget(CurrentLocation))
		{
			OnSegmentFinished();
			SetNextMoveSegment();
		}
	}

	// gather location samples to detect if moving agent is blocked
	if (bCanReachTarget && Status == EPathFollowingStatus::Moving)
	{
		const bool bHasNewSample = UpdateBlockDetection();
		if (bHasNewSample && IsBlocked())
		{
			if (Path->GetPathPoints().IsValidIndex(MoveSegmentEndIndex) && Path->GetPathPoints().IsValidIndex(MoveSegmentStartIndex))
			{
				LogBlockHelper(GetOwner(), MovementComp, MinAgentRadiusPct, MinAgentHalfHeightPct,
					*Path->GetPathPointLocation(MoveSegmentStartIndex),
					*Path->GetPathPointLocation(MoveSegmentEndIndex));
			}
			else
			{
				if ((GetOwner() != NULL) && (MovementComp != NULL))
				{
					UE_VLOG(GetOwner(), LogPathFollowing, Verbose, TEXT("Path blocked, but move segment indices are not valid: start %d, end %d of %d"), MoveSegmentStartIndex, MoveSegmentEndIndex, Path->GetPathPoints().Num());
				}
			}
			OnPathFinished(EPathFollowingResult::Blocked);
		}
	}
}

void UPathFollowingComponent::FollowPathSegment(float DeltaTime)
{
	if (MovementComp == NULL || !Path.IsValid())
	{
		return;
	}

	//const FVector CurrentLocation = MovementComp->IsMovingOnGround() ? MovementComp->GetActorFeetLocation() : MovementComp->GetActorLocation();
	const FVector CurrentLocation = MovementComp->GetActorFeetLocation();
	const FVector CurrentTarget = GetCurrentTargetLocation();
	FVector MoveVelocity = (CurrentTarget - CurrentLocation) / DeltaTime;

	const int32 LastSegmentStartIndex = Path->GetPathPoints().Num() - 2;
	const bool bNotFollowingLastSegment = (MoveSegmentStartIndex < LastSegmentStartIndex);

	PostProcessMove.ExecuteIfBound(this, MoveVelocity);
	MovementComp->RequestDirectMove(MoveVelocity, bNotFollowingLastSegment);
}

bool UPathFollowingComponent::HasReached(const FVector& TestPoint, float InAcceptanceRadius, bool bExactSpot) const
{
	// simple test for stationary agent, used as early finish condition
	const FVector CurrentLocation = MovementComp ? MovementComp->GetActorFeetLocation() : FVector::ZeroVector;
	const float GoalRadius = 0.0f;
	const float GoalHalfHeight = 0.0f;
	if (InAcceptanceRadius == UPathFollowingComponent::DefaultAcceptanceRadius)
	{
		InAcceptanceRadius = MyDefaultAcceptanceRadius;
	}

	return HasReachedInternal(TestPoint, GoalRadius, GoalHalfHeight, CurrentLocation, InAcceptanceRadius, bExactSpot ? 0.0f : MinAgentRadiusPct);
}

bool UPathFollowingComponent::HasReached(const AActor& TestGoal, float InAcceptanceRadius, bool bExactSpot) const
{
	// simple test for stationary agent, used as early finish condition
	float GoalRadius = 0.0f;
	float GoalHalfHeight = 0.0f;
	FVector GoalOffset = FVector::ZeroVector;
	FVector TestPoint = TestGoal.GetActorLocation();
	if (InAcceptanceRadius == UPathFollowingComponent::DefaultAcceptanceRadius)
	{
		InAcceptanceRadius = MyDefaultAcceptanceRadius;
	}

	const INavAgentInterface* NavAgent = Cast<const INavAgentInterface>(&TestGoal);
	if (NavAgent)
	{
		const FVector GoalMoveOffset = NavAgent->GetMoveGoalOffset(GetOwner());
		NavAgent->GetMoveGoalReachTest(GetOwner(), GoalMoveOffset, GoalOffset, GoalRadius, GoalHalfHeight);
		TestPoint = FQuatRotationTranslationMatrix(TestGoal.GetActorQuat(), NavAgent->GetNavAgentLocation()).TransformPosition(GoalOffset);
	}

	const FVector CurrentLocation = MovementComp ? MovementComp->GetActorFeetLocation() : FVector::ZeroVector;
	return HasReachedInternal(TestPoint, GoalRadius, GoalHalfHeight, CurrentLocation, InAcceptanceRadius, bExactSpot ? 0.0f : MinAgentRadiusPct);
}

bool UPathFollowingComponent::HasReachedDestination(const FVector& CurrentLocation) const
{
	// get cylinder at goal location
	FVector GoalLocation = *Path->GetPathPointLocation(Path->GetPathPoints().Num() - 1);
	float GoalRadius = 0.0f;
	float GoalHalfHeight = 0.0f;
	
	// take goal's current location, unless path is partial
	if (DestinationActor.IsValid() && !Path->IsPartial())
	{
		if (DestinationAgent)
		{
			FVector GoalOffset;
			DestinationAgent->GetMoveGoalReachTest(GetOwner(), MoveOffset, GoalOffset, GoalRadius, GoalHalfHeight);

			GoalLocation = FQuatRotationTranslationMatrix(DestinationActor->GetActorQuat(), DestinationAgent->GetNavAgentLocation()).TransformPosition(GoalOffset);
		}
		else
		{
			GoalLocation = DestinationActor->GetActorLocation();
		}
	}

	return HasReachedInternal(GoalLocation, GoalRadius, GoalHalfHeight, CurrentLocation, AcceptanceRadius, bStopOnOverlap ? MinAgentRadiusPct : 0.0f);
}

bool UPathFollowingComponent::HasReachedCurrentTarget(const FVector& CurrentLocation) const
{
	if (MovementComp == NULL)
	{
		return false;
	}

	const FVector CurrentTarget = GetCurrentTargetLocation();
	const FVector CurrentDirection = GetCurrentDirection();

	// check if moved too far
	const FVector ToTarget = (CurrentTarget - MovementComp->GetActorFeetLocation());
	const float SegmentDot = FVector::DotProduct(ToTarget, CurrentDirection);
	if (SegmentDot < 0.0)
	{
		return true;
	}

	// or standing at target position
	// don't use acceptance radius here, it has to be exact for moving near corners (2D test < 5% of agent radius)
	const float GoalRadius = 0.0f;
	const float GoalHalfHeight = 0.0f;

	return HasReachedInternal(CurrentTarget, GoalRadius, GoalHalfHeight, CurrentLocation, CurrentAcceptanceRadius, 0.05f);
}

bool UPathFollowingComponent::HasReachedInternal(const FVector& GoalLocation, float GoalRadius, float GoalHalfHeight, const FVector& AgentLocation, float RadiusThreshold, bool bSuccessOnRadiusOverlap) const
{
	return HasReachedInternal(GoalLocation, GoalRadius, GoalHalfHeight, AgentLocation, RadiusThreshold, bSuccessOnRadiusOverlap ? MinAgentRadiusPct : 0.0f);
}

bool UPathFollowingComponent::HasReachedInternal(const FVector& GoalLocation, float GoalRadius, float GoalHalfHeight, const FVector& AgentLocation, float RadiusThreshold, float AgentRadiusMultiplier) const
{
	if (MovementComp == NULL)
	{
		return false;
	}

	// get cylinder of moving agent
	float AgentRadius = 0.0f;
	float AgentHalfHeight = 0.0f;
	AActor* MovingAgent = MovementComp->GetOwner();
	MovingAgent->GetSimpleCollisionCylinder(AgentRadius, AgentHalfHeight);

	// check if they overlap (with added AcceptanceRadius)
	const FVector ToGoal = GoalLocation - AgentLocation;

	const float Dist2DSq = ToGoal.SizeSquared2D();
	const float UseRadius = FMath::Max(RadiusThreshold, GoalRadius + (AgentRadius * AgentRadiusMultiplier));
	if (Dist2DSq > FMath::Square(UseRadius))
	{
		return false;
	}

	const float ZDiff = FMath::Abs(ToGoal.Z);
	const float UseHeight = GoalHalfHeight + (AgentHalfHeight * MinAgentHalfHeightPct);
	if (ZDiff > UseHeight)
	{
		return false;
	}

	return true;
}

void UPathFollowingComponent::DebugReachTest(float& CurrentDot, float& CurrentDistance, float& CurrentHeight, uint8& bDotFailed, uint8& bDistanceFailed, uint8& bHeightFailed) const
{
	if (!Path.IsValid() || MovementComp == NULL)
	{
		return;
	}

	const int32 LastSegmentEndIndex = Path->GetPathPoints().Num() - 1;
	const bool bFollowingLastSegment = (MoveSegmentEndIndex >= LastSegmentEndIndex);

	float GoalRadius = 0.0f;
	float GoalHalfHeight = 0.0f;
	float RadiusThreshold = 0.0f;
	float AgentRadiusPct = 0.05f;

	FVector AgentLocation = MovementComp->GetActorFeetLocation();
	FVector GoalLocation = GetCurrentTargetLocation();
	RadiusThreshold = CurrentAcceptanceRadius;

	if (bFollowingLastSegment)
	{
		GoalLocation = *Path->GetPathPointLocation(Path->GetPathPoints().Num() - 1);
		AgentRadiusPct = MinAgentRadiusPct;

		// take goal's current location, unless path is partial
		if (DestinationActor.IsValid() && !Path->IsPartial())
		{
			if (DestinationAgent)
			{
				FVector GoalOffset;
				DestinationAgent->GetMoveGoalReachTest(GetOwner(), MoveOffset, GoalOffset, GoalRadius, GoalHalfHeight);

				GoalLocation = FQuatRotationTranslationMatrix(DestinationActor->GetActorQuat(), DestinationAgent->GetNavAgentLocation()).TransformPosition(GoalOffset);
			}
			else
			{
				GoalLocation = DestinationActor->GetActorLocation();
			}
		}
	}

	const FVector ToGoal = (GoalLocation - AgentLocation);
	const FVector CurrentDirection = GetCurrentDirection();
	CurrentDot = FVector::DotProduct(ToGoal.GetSafeNormal(), CurrentDirection);
	bDotFailed = (CurrentDot < 0.0f) ? 1 : 0;

	// get cylinder of moving agent
	float AgentRadius = 0.0f;
	float AgentHalfHeight = 0.0f;
	AActor* MovingAgent = MovementComp->GetOwner();
	MovingAgent->GetSimpleCollisionCylinder(AgentRadius, AgentHalfHeight);

	CurrentDistance = ToGoal.Size2D();
	const float UseRadius = FMath::Max(RadiusThreshold, GoalRadius + (AgentRadius * AgentRadiusPct));
	bDistanceFailed = (CurrentDistance > UseRadius) ? 1 : 0;

	CurrentHeight = FMath::Abs(ToGoal.Z);
	const float UseHeight = GoalHalfHeight + (AgentHalfHeight * MinAgentHalfHeightPct);
	bHeightFailed = (CurrentHeight > UseHeight) ? 1 : 0;
}

void UPathFollowingComponent::StartUsingCustomLink(INavLinkCustomInterface* CustomNavLink, const FVector& DestPoint)
{
	INavLinkCustomInterface* PrevNavLink = Cast<INavLinkCustomInterface>(CurrentCustomLinkOb.Get());
	if (PrevNavLink)
	{
		UE_VLOG(GetOwner(), LogPathFollowing, Log, TEXT("Force finish custom move using navlink: %s"), *GetNameSafe(CurrentCustomLinkOb.Get()));

		PrevNavLink->OnLinkMoveFinished(this);
		CurrentCustomLinkOb.Reset();
	}

	UObject* NewNavLinkOb = Cast<UObject>(CustomNavLink);
	if (NewNavLinkOb)
	{
		CurrentCustomLinkOb = NewNavLinkOb;

		const bool bCustomMove = CustomNavLink->OnLinkMoveStarted(this, DestPoint);
		UE_VLOG(GetOwner(), LogPathFollowing, Log, TEXT("%s navlink: %s"),
			bCustomMove ? TEXT("Custom move using") : TEXT("Notify"), *GetNameSafe(NewNavLinkOb));

		if (!bCustomMove)
		{
			CurrentCustomLinkOb = NULL;
		}
	}
}

void UPathFollowingComponent::FinishUsingCustomLink(INavLinkCustomInterface* CustomNavLink)
{
	UObject* NavLinkOb = Cast<UObject>(CustomNavLink);
	if (CurrentCustomLinkOb == NavLinkOb)
	{
		UE_VLOG(GetOwner(), LogPathFollowing, Log, TEXT("Finish custom move using navlink: %s"), *GetNameSafe(NavLinkOb));

		CustomNavLink->OnLinkMoveFinished(this);
		CurrentCustomLinkOb.Reset();
	}
}

bool UPathFollowingComponent::UpdateMovementComponent(bool bForce)
{
	if (MovementComp == NULL || bForce == true)
	{
		APawn* MyPawn = Cast<APawn>(GetOwner());
		if (MyPawn == NULL)
		{
			AController* MyController = Cast<AController>(GetOwner());
			if (MyController)
			{
				MyPawn = MyController->GetPawn();
			}
		}

		if (MyPawn)
		{
			FScriptDelegate Delegate;
			Delegate.BindUFunction(this, "OnActorBump");
			MyPawn->OnActorHit.AddUnique(Delegate);

			SetMovementComponent(MyPawn->FindComponentByClass<UNavMovementComponent>());
		}
	}

	return (MovementComp != NULL);
}

void UPathFollowingComponent::OnActorBump(AActor* SelfActor, AActor* OtherActor, FVector NormalImpulse, const FHitResult& Hit)
{
	if (Path.IsValid() && DestinationActor.IsValid() && DestinationActor.Get() == OtherActor)
	{
		UE_VLOG(GetOwner(), LogPathFollowing, Verbose, TEXT("Collided with goal actor"));
		bCollidedWithGoal = true;
	}
}

bool UPathFollowingComponent::IsOnPath() const
{
	bool bOnPath = false;
	if (Path.IsValid() && Path->IsValid() && Path->GetNavigationDataUsed() != NULL)
	{
		const bool bHasNavigationCorridor = (Path->CastPath<FNavMeshPath>() != NULL);
		if (bHasNavigationCorridor)
		{
			FNavLocation NavLoc = GetCurrentNavLocation();
			bOnPath = Path->ContainsNode(NavLoc.NodeRef);
		}
		else
		{
			bOnPath = true;
		}
	}

	return bOnPath;
}

bool UPathFollowingComponent::IsBlocked() const
{
	bool bBlocked = false;

	if (LocationSamples.Num() == BlockDetectionSampleCount && BlockDetectionSampleCount > 0)
	{
		FVector Center = FVector::ZeroVector;
		for (int32 SampleIndex = 0; SampleIndex < LocationSamples.Num(); SampleIndex++)
		{
			Center += *LocationSamples[SampleIndex];
		}

		Center /= LocationSamples.Num();
		bBlocked = true;

		for (int32 SampleIndex = 0; SampleIndex < LocationSamples.Num(); SampleIndex++)
		{
			const float TestDistance = FVector::DistSquared(*LocationSamples[SampleIndex], Center);
			if (TestDistance > BlockDetectionDistance)
			{
				bBlocked = false;
				break;
			}
		}
	}

	return bBlocked;
}

bool UPathFollowingComponent::UpdateBlockDetection()
{
	const float GameTime = GetWorld()->GetTimeSeconds();
	if (bUseBlockDetection &&
		MovementComp && 
		GameTime > (LastSampleTime + BlockDetectionInterval) &&
		BlockDetectionSampleCount > 0)
	{
		LastSampleTime = GameTime;

		if (LocationSamples.Num() == NextSampleIdx)
		{
			LocationSamples.AddZeroed(1);
		}

		LocationSamples[NextSampleIdx] = MovementComp->GetActorFeetLocationBased();
		NextSampleIdx = (NextSampleIdx + 1) % BlockDetectionSampleCount;
		return true;
	}

	return false;
}

void UPathFollowingComponent::SetBlockDetection(float DistanceThreshold, float Interval, int32 NumSamples)
{
	BlockDetectionDistance = DistanceThreshold;
	BlockDetectionInterval = Interval;
	BlockDetectionSampleCount = NumSamples;
	ResetBlockDetectionData();
}

void UPathFollowingComponent::SetBlockDetectionState(bool bEnable)
{
	bUseBlockDetection = bEnable;
	ResetBlockDetectionData();
}

void UPathFollowingComponent::ResetBlockDetectionData()
{
	LastSampleTime = 0.0f;
	NextSampleIdx = 0;
	LocationSamples.Reset();
}

void UPathFollowingComponent::ForceBlockDetectionUpdate()
{
	LastSampleTime = 0.0f;
}

void UPathFollowingComponent::UpdateMoveFocus()
{
	AAIController* AIOwner = Cast<AAIController>(GetOwner());
	if (AIOwner != NULL)
	{
		if (Status == EPathFollowingStatus::Moving)
		{
			const FVector MoveFocus = GetMoveFocus(AIOwner->bAllowStrafe);
			AIOwner->SetFocalPoint(MoveFocus, EAIFocusPriority::Move);
		}
		else
		{
			AIOwner->ClearFocus(EAIFocusPriority::Move);
		}
	}
}

void UPathFollowingComponent::SetPreciseReachThreshold(float AgentRadiusMultiplier, float AgentHalfHeightMultiplier)
{
	MinAgentRadiusPct = AgentRadiusMultiplier;
	MinAgentHalfHeightPct = AgentHalfHeightMultiplier;
}

float UPathFollowingComponent::GetRemainingPathCost() const
{
	float Cost = 0.f;

	if (Path.IsValid() && Path->IsValid() && Status == EPathFollowingStatus::Moving)
	{
		const FNavLocation& NavLocation = GetCurrentNavLocation();
		Cost = Path->GetCostFromNode(NavLocation.NodeRef);
	}

	return Cost;
}	

FNavLocation UPathFollowingComponent::GetCurrentNavLocation() const
{
	// get navigation location of moved actor
	if (MovementComp == NULL || GetWorld() == NULL || GetWorld()->GetNavigationSystem() == NULL)
	{
		return FNavLocation();
	}

	const FVector OwnerLoc = MovementComp->GetActorNavLocation();
	const float Tolerance = 1.0f;
	
	// Using !Equals rather than != because exact equivalence isn't required.  (Equals allows a small tolerance.)
	if (!OwnerLoc.Equals(CurrentNavLocation.Location, Tolerance))
	{
		const AActor* OwnerActor = MovementComp->GetOwner();
		const FVector OwnerExtent = OwnerActor ? OwnerActor->GetSimpleCollisionCylinderExtent() : FVector::ZeroVector;

		GetWorld()->GetNavigationSystem()->ProjectPointToNavigation(OwnerLoc, CurrentNavLocation, OwnerExtent, MyNavData);
	}

	return CurrentNavLocation;
}

FVector UPathFollowingComponent::GetCurrentDirection() const
{
	if (CurrentDestination.Base)
	{
		// calculate direction to based destination
		const FVector SegmentStartLocation = *Path->GetPathPointLocation(MoveSegmentStartIndex);
		const FVector SegmentEndLocation = *CurrentDestination;

		return (SegmentEndLocation - SegmentStartLocation).GetSafeNormal();
	}

	// use cached direction of current path segment
	return MoveSegmentDirection;
}

// will be deprecated soon, please don't use it!
EPathFollowingAction::Type UPathFollowingComponent::GetPathActionType() const
{
	switch (Status)
	{
		case EPathFollowingStatus::Idle:
			return EPathFollowingAction::NoMove;

		case EPathFollowingStatus::Waiting:
		case EPathFollowingStatus::Paused:	
		case EPathFollowingStatus::Moving:
			return Path.IsValid() == false ? EPathFollowingAction::Error :
				Path->CastPath<FAbstractNavigationPath>() ? EPathFollowingAction::DirectMove :
				Path->IsPartial() ? EPathFollowingAction::PartialPath : EPathFollowingAction::PathToGoal;

		default:
			break;
	}

	return EPathFollowingAction::Error;
}

// will be deprecated soon, please don't use it!
FVector UPathFollowingComponent::GetPathDestination() const
{
	return Path.IsValid() ? Path->GetDestinationLocation() : FVector::ZeroVector;
}

bool UPathFollowingComponent::HasDirectPath() const
{
	return Path.IsValid() ? (Path->CastPath<FNavMeshPath>() == NULL) : false;
}

FString UPathFollowingComponent::GetStatusDesc() const
{
	const static UEnum* StatusEnum = FindObject<UEnum>(ANY_PACKAGE, TEXT("EPathFollowingStatus"));
	if (StatusEnum)
	{
		return StatusEnum->GetEnumName(Status);
	}

	return TEXT("Unknown");
}

FString UPathFollowingComponent::GetResultDesc(EPathFollowingResult::Type Result) const
{
	const static UEnum* ResultEnum = FindObject<UEnum>(ANY_PACKAGE, TEXT("EPathFollowingResult"));
	if (ResultEnum)
	{
		return ResultEnum->GetEnumName(Result);
	}

	return TEXT("Unknown");
}

void UPathFollowingComponent::DisplayDebug(UCanvas* Canvas, const FDebugDisplayInfo& DebugDisplay, float& YL, float& YPos) const
{
	Canvas->SetDrawColor(FColor::Blue);
	UFont* RenderFont = GEngine->GetSmallFont();
	FString StatusDesc = FString::Printf(TEXT("  Move status: %s"), *GetStatusDesc());
	YL = Canvas->DrawText(RenderFont, StatusDesc, 4.0f, YPos);
	YPos += YL;

	if (Status == EPathFollowingStatus::Moving)
	{
		const int32 NumMoveSegments = (Path.IsValid() && Path->IsValid()) ? Path->GetPathPoints().Num() : -1;
		FString TargetDesc = FString::Printf(TEXT("  Move target [%d/%d]: %s (%s)"),
			MoveSegmentEndIndex, NumMoveSegments, *GetCurrentTargetLocation().ToString(), *GetNameSafe(DestinationActor.Get()));
		
		YL = Canvas->DrawText(RenderFont, TargetDesc, 4.0f, YPos);
		YPos += YL;
	}
}

#if ENABLE_VISUAL_LOG
void UPathFollowingComponent::DescribeSelfToVisLog(FVisualLogEntry* Snapshot) const
{
	FVisualLogStatusCategory Category;
	Category.Category = TEXT("Path following");

	if (DestinationActor.IsValid())
	{
		Category.Add(TEXT("Goal"), GetNameSafe(DestinationActor.Get()));
	}
	
	FString StatusDesc = GetStatusDesc();
	if (Status == EPathFollowingStatus::Moving)
	{
		const int32 NumMoveSegments = (Path.IsValid() && Path->IsValid()) ? Path->GetPathPoints().Num() : -1;
		StatusDesc += FString::Printf(TEXT(" [%d..%d/%d]"), MoveSegmentStartIndex + 1, MoveSegmentEndIndex + 1, NumMoveSegments);
	}

	Category.Add(TEXT("Status"), StatusDesc);
	Category.Add(TEXT("Path"), !Path.IsValid() ? TEXT("none") :
		(Path->CastPath<FNavMeshPath>() != NULL) ? TEXT("navmesh") :
		(Path->CastPath<FAbstractNavigationPath>() != NULL) ? TEXT("direct") :
		TEXT("unknown"));
	
	UObject* CustomNavLinkOb = CurrentCustomLinkOb.Get();
	if (CustomNavLinkOb)
	{
		Category.Add(TEXT("Custom NavLink"), CustomNavLinkOb->GetName());
	}

	Snapshot->Status.Add(Category);
}
#endif // ENABLE_VISUAL_LOG

void UPathFollowingComponent::GetDebugStringTokens(TArray<FString>& Tokens, TArray<EPathFollowingDebugTokens::Type>& Flags) const
{
	Tokens.Add(GetStatusDesc());
	Flags.Add(EPathFollowingDebugTokens::Description);

	if (Status != EPathFollowingStatus::Moving)
	{
		return;
	}

	FString& StatusDesc = Tokens[0];
	if (Path.IsValid())
	{
		const int32 NumMoveSegments = (Path.IsValid() && Path->IsValid()) ? Path->GetPathPoints().Num() : -1;
		const bool bIsDirect = (Path->CastPath<FAbstractNavigationPath>() != NULL);
		const bool bIsCustomLink = CurrentCustomLinkOb.IsValid();

		if (!bIsDirect)
		{
			StatusDesc += FString::Printf(TEXT(" (%d..%d/%d)%s"), MoveSegmentStartIndex + 1, MoveSegmentEndIndex + 1, NumMoveSegments,
				bIsCustomLink ? TEXT(" (custom NavLink)") : TEXT(""));
		}
		else
		{
			StatusDesc += TEXT(" (direct)");
		}
	}
	else
	{
		StatusDesc += TEXT(" (invalid path)");
	}

	// add debug params
	float CurrentDot = 0.0f, CurrentDistance = 0.0f, CurrentHeight = 0.0f;
	uint8 bFailedDot = 0, bFailedDistance = 0, bFailedHeight = 0;
	DebugReachTest(CurrentDot, CurrentDistance, CurrentHeight, bFailedHeight, bFailedDistance, bFailedHeight);

	Tokens.Add(TEXT("dot"));
	Flags.Add(EPathFollowingDebugTokens::ParamName);
	Tokens.Add(FString::Printf(TEXT("%.2f"), CurrentDot));
	Flags.Add(bFailedDot ? EPathFollowingDebugTokens::FailedValue : EPathFollowingDebugTokens::PassedValue);

	Tokens.Add(TEXT("dist2D"));
	Flags.Add(EPathFollowingDebugTokens::ParamName);
	Tokens.Add(FString::Printf(TEXT("%.0f"), CurrentDistance));
	Flags.Add(bFailedDistance ? EPathFollowingDebugTokens::FailedValue : EPathFollowingDebugTokens::PassedValue);

	Tokens.Add(TEXT("distZ"));
	Flags.Add(EPathFollowingDebugTokens::ParamName);
	Tokens.Add(FString::Printf(TEXT("%.0f"), CurrentHeight));
	Flags.Add(bFailedHeight ? EPathFollowingDebugTokens::FailedValue : EPathFollowingDebugTokens::PassedValue);
}

FString UPathFollowingComponent::GetDebugString() const
{
	TArray<FString> Tokens;
	TArray<EPathFollowingDebugTokens::Type> Flags;

	GetDebugStringTokens(Tokens, Flags);

	FString Desc;
	for (int32 TokenIndex = 0; TokenIndex < Tokens.Num(); TokenIndex++)
	{
		Desc += Tokens[TokenIndex];
		Desc += (Flags.IsValidIndex(TokenIndex) && Flags[TokenIndex] == EPathFollowingDebugTokens::ParamName) ? TEXT(": ") : TEXT(", ");
	}

	return Desc;
}

bool UPathFollowingComponent::IsPathFollowingAllowed() const
{
	return MovementComp && MovementComp->CanStartPathFollowing();
}

void UPathFollowingComponent::LockResource(EAIRequestPriority::Type LockSource)
{
	const static UEnum* SourceEnum = FindObject<UEnum>(ANY_PACKAGE, TEXT("EAILockSource"));
	const bool bWasLocked = ResourceLock.IsLocked();

	ResourceLock.SetLock(LockSource);
	if (bWasLocked == false)
	{
		UE_VLOG(GetOwner(), LogPathFollowing, Log, TEXT("Locking Move by source %s"), SourceEnum ? *SourceEnum->GetEnumName(Status) : TEXT("invalid"));
		PauseMove();
	}
}

void UPathFollowingComponent::ClearResourceLock(EAIRequestPriority::Type LockSource)
{
	const bool bWasLocked = ResourceLock.IsLocked();
	ResourceLock.ClearLock(LockSource);

	if (bWasLocked && !ResourceLock.IsLocked())
	{
		UE_VLOG(GetOwner(), LogPathFollowing, Log, TEXT("Unlocking Move"));
		ResumeMove();
	}
}

void UPathFollowingComponent::ForceUnlockResource()
{
	const bool bWasLocked = ResourceLock.IsLocked();
	ResourceLock.ForceClearAllLocks();

	if (bWasLocked)
	{
		UE_VLOG(GetOwner(), LogPathFollowing, Log, TEXT("Unlocking Move - forced"));
		ResumeMove();
	}
}

bool UPathFollowingComponent::IsResourceLocked() const
{
	return ResourceLock.IsLocked();
}

void UPathFollowingComponent::SetLastMoveAtGoal(bool bFinishedAtGoal)
{
	bLastMoveReachedGoal = bFinishedAtGoal;
}
