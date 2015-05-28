// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AIModulePrivate.h"
#include "Kismet/GameplayStatics.h"
#include "DisplayDebugHelpers.h"
#include "BrainComponent.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BehaviorTreeManager.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Navigation/PathFollowingComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PawnMovementComponent.h"
#include "Engine/Canvas.h"
#include "GameFramework/PhysicsVolume.h"
#include "AIController.h"
#include "Perception/AIPerceptionComponent.h"

// mz@todo these need to be removed, legacy code
#define CLOSEPROXIMITY					500.f
#define NEARSIGHTTHRESHOLD				2000.f
#define MEDSIGHTTHRESHOLD				3162.f
#define FARSIGHTTHRESHOLD				8000.f
#define CLOSEPROXIMITYSQUARED			(CLOSEPROXIMITY*CLOSEPROXIMITY)
#define NEARSIGHTTHRESHOLDSQUARED		(NEARSIGHTTHRESHOLD*NEARSIGHTTHRESHOLD)
#define MEDSIGHTTHRESHOLDSQUARED		(MEDSIGHTTHRESHOLD*MEDSIGHTTHRESHOLD)
#define FARSIGHTTHRESHOLDSQUARED		(FARSIGHTTHRESHOLD*FARSIGHTTHRESHOLD)

//----------------------------------------------------------------------//
// AAIController
//----------------------------------------------------------------------//
bool AAIController::bAIIgnorePlayers = false;

DECLARE_CYCLE_STAT(TEXT("MoveTo"), STAT_MoveTo, STATGROUP_AI);

DEFINE_LOG_CATEGORY(LogAINavigation);

AAIController::AAIController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PathFollowingComponent = CreateDefaultSubobject<UPathFollowingComponent>(TEXT("PathFollowingComponent"));
	PathFollowingComponent->OnMoveFinished.AddUObject(this, &AAIController::OnMoveCompleted);

	ActionsComp = CreateDefaultSubobject<UPawnActionsComponent>("ActionsComp");

	bSkipExtraLOSChecks = true;
	bWantsPlayerState = false;
}

void AAIController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	UpdateControlRotation(DeltaTime);
}

void AAIController::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	if (bWantsPlayerState && !IsPendingKill() && (GetNetMode() != NM_Client))
	{
		InitPlayerState();
	}

#if ENABLE_VISUAL_LOG
	TArray<UActorComponent*> ComponentSet;
	GetComponents(ComponentSet);
	for (auto Component : ComponentSet)
	{
		REDIRECT_OBJECT_TO_VLOG(Component, this);
	}
#endif // ENABLE_VISUAL_LOG
}

void AAIController::PostRegisterAllComponents()
{
	Super::PostRegisterAllComponents();

	// cache PerceptionComponent if not already set
	// note that it's possible for an AI to not have a perception component at all
	if (PerceptionComponent == NULL || PerceptionComponent->IsPendingKill() == true)
	{
		PerceptionComponent = FindComponentByClass<UAIPerceptionComponent>();
	}
}

void AAIController::Reset()
{
	Super::Reset();

	if (PathFollowingComponent)
	{
		PathFollowingComponent->AbortMove(TEXT("controller reset"));
	}
}

void AAIController::DisplayDebug(UCanvas* Canvas, const FDebugDisplayInfo& DebugDisplay, float& YL, float& YPos)
{
	Super::DisplayDebug(Canvas, DebugDisplay, YL, YPos);

	static FName NAME_AI = FName(TEXT("AI"));
	if (DebugDisplay.IsDisplayOn(NAME_AI))
	{
		if (PathFollowingComponent)
		{
			PathFollowingComponent->DisplayDebug(Canvas, DebugDisplay, YL, YPos);
		}

		AActor* FocusActor = GetFocusActor();
		if (FocusActor)
		{
			YL = Canvas->DrawText(GEngine->GetSmallFont(), FString::Printf(TEXT("      Focus %s"), *FocusActor->GetName()), 4.0f, YPos);
			YPos += YL;
		}
	}
}

#if ENABLE_VISUAL_LOG

void AAIController::GrabDebugSnapshot(FVisualLogEntry* Snapshot) const
{
	FVisualLogStatusCategory MyCategory;
	MyCategory.Category = TEXT("AI Controller");
	MyCategory.Add(TEXT("Pawn"), GetNameSafe(GetPawn()));
	AActor* FocusActor = GetFocusActor();
	MyCategory.Add(TEXT("Focus"), GetDebugName(FocusActor));

	if (FocusActor == nullptr)
	{
		MyCategory.Add(TEXT("Focus Location"), TEXT_AI_LOCATION(GetFocalPoint()));
	}
	Snapshot->Status.Add(MyCategory);

	if (GetPawn())
	{
		Snapshot->Location = GetPawn()->GetActorLocation();
	}

	if (PathFollowingComponent)
	{
		PathFollowingComponent->DescribeSelfToVisLog(Snapshot);
	}

	if (BrainComponent != NULL)
	{
		BrainComponent->DescribeSelfToVisLog(Snapshot);
	}

	if (PerceptionComponent != NULL)
	{
		PerceptionComponent->DescribeSelfToVisLog(Snapshot);
	}
}
#endif // ENABLE_VISUAL_LOG

void AAIController::SetFocalPoint(FVector NewFocus, EAIFocusPriority::Type InPriority)
{
	if (InPriority >= FocusInformation.Priorities.Num())
	{
		FocusInformation.Priorities.SetNum(InPriority + 1);
	}
	FFocusKnowledge::FFocusItem& FocusItem = FocusInformation.Priorities[InPriority];

	FocusItem.Actor = nullptr;
	FocusItem.Position = NewFocus;
}

FVector AAIController::GetFocalPointForPriority(EAIFocusPriority::Type InPriority) const
{
	FVector Result = FAISystem::InvalidLocation;

	if (InPriority < FocusInformation.Priorities.Num())
	{
		const FFocusKnowledge::FFocusItem& FocusItem = FocusInformation.Priorities[InPriority];

		AActor* FocusActor = FocusItem.Actor.Get();
		if (FocusActor)
		{
			Result = GetFocalPointOnActor(FocusActor);
		}
		else
		{
			Result = FocusItem.Position;
		}
	}

	return Result;
}

FVector AAIController::GetFocalPoint() const
{
	FVector Result = FAISystem::InvalidLocation;

	// find focus with highest priority
	for (int32 Index = FocusInformation.Priorities.Num() - 1; Index >= 0; --Index)
	{
		const FFocusKnowledge::FFocusItem& FocusItem = FocusInformation.Priorities[Index];
		AActor* FocusActor = FocusItem.Actor.Get();
		if (FocusActor)
		{
			Result = GetFocalPointOnActor(FocusActor);
			break;
		}
		else if (FAISystem::IsValidLocation(FocusItem.Position))
		{
			Result = FocusItem.Position;
			break;
		}
	}

	return Result;
}

AActor* AAIController::GetFocusActor() const
{
	AActor* FocusActor = nullptr;
	for (int32 Index = FocusInformation.Priorities.Num() - 1; Index >= 0; --Index)
	{
		const FFocusKnowledge::FFocusItem& FocusItem = FocusInformation.Priorities[Index];
		FocusActor = FocusItem.Actor.Get();
		if (FocusActor)
		{
			break;
		}
		else if (FAISystem::IsValidLocation(FocusItem.Position))
		{
			break;
		}
	}

	return FocusActor;
}

FVector AAIController::GetFocalPointOnActor(const AActor *Actor) const
{
	return Actor != nullptr ? Actor->GetActorLocation() : FAISystem::InvalidLocation;
}

void AAIController::K2_SetFocus(AActor* NewFocus)
{
	SetFocus(NewFocus, EAIFocusPriority::Gameplay);
}

void AAIController::K2_SetFocalPoint(FVector NewFocus)
{
	SetFocalPoint(NewFocus, EAIFocusPriority::Gameplay);
}

void AAIController::K2_ClearFocus()
{
	ClearFocus(EAIFocusPriority::Gameplay);
}

void AAIController::SetFocus(AActor* NewFocus, EAIFocusPriority::Type InPriority)
{
	if (NewFocus)
	{
		if (InPriority >= FocusInformation.Priorities.Num())
		{
			FocusInformation.Priorities.SetNum(InPriority + 1);
		}
		FocusInformation.Priorities[InPriority].Actor = NewFocus;
	}
	else
	{
		ClearFocus(InPriority);
	}
}

void AAIController::ClearFocus(EAIFocusPriority::Type InPriority)
{
	if (InPriority < FocusInformation.Priorities.Num())
	{
		FocusInformation.Priorities[InPriority].Actor = nullptr;
		FocusInformation.Priorities[InPriority].Position = FAISystem::InvalidLocation;
	}
}

void AAIController::SetPerceptionComponent(UAIPerceptionComponent& InPerceptionComponent)
{
	if (PerceptionComponent != nullptr)
	{
		UE_VLOG(this, LogAIPerception, Warning, TEXT("Setting perception component while AIController already has one!"));
	}
	PerceptionComponent = &InPerceptionComponent;
}

bool AAIController::LineOfSightTo(const AActor* Other, FVector ViewPoint, bool bAlternateChecks) const
{
	if (Other == nullptr)
	{
		return false;
	}

	if (ViewPoint.IsZero())
	{
		FRotator ViewRotation;
		GetActorEyesViewPoint(ViewPoint, ViewRotation);

		// if we still don't have a view point we simply fail
		if (ViewPoint.IsZero())
		{
			return false;
		}
	}

	static FName NAME_LineOfSight = FName(TEXT("LineOfSight"));
	FVector TargetLocation = Other->GetTargetLocation(GetPawn());

	FCollisionQueryParams CollisionParams(NAME_LineOfSight, true, this->GetPawn());
	CollisionParams.AddIgnoredActor(Other);

	bool bHit = GetWorld()->LineTraceTestByChannel(ViewPoint, TargetLocation, ECC_Visibility, CollisionParams);
	if (!bHit)
	{
		return true;
	}

	// if other isn't using a cylinder for collision and isn't a Pawn (which already requires an accurate cylinder for AI)
	// then don't go any further as it likely will not be tracing to the correct location
	const APawn * OtherPawn = Cast<const APawn>(Other);
	if (!OtherPawn && Cast<UCapsuleComponent>(Other->GetRootComponent()) == NULL)
	{
		return false;
	}

	const FVector OtherActorLocation = Other->GetActorLocation();
	const float DistSq = (OtherActorLocation - ViewPoint).SizeSquared();
	if (DistSq > FARSIGHTTHRESHOLDSQUARED)
	{
		return false;
	}

	if (!OtherPawn && (DistSq > NEARSIGHTTHRESHOLDSQUARED))
	{
		return false;
	}

	float OtherRadius, OtherHeight;
	Other->GetSimpleCollisionCylinder(OtherRadius, OtherHeight);

	if (!bAlternateChecks || !bLOSflag)
	{
		//try viewpoint to head
		bHit = GetWorld()->LineTraceTestByChannel(ViewPoint, OtherActorLocation + FVector(0.f, 0.f, OtherHeight), ECC_Visibility, CollisionParams);
		if (!bHit)
		{
			return true;
		}
	}

	if (!bSkipExtraLOSChecks && (!bAlternateChecks || bLOSflag))
	{
		// only check sides if width of other is significant compared to distance
		if (OtherRadius * OtherRadius / (OtherActorLocation - ViewPoint).SizeSquared() < 0.0001f)
		{
			return false;
		}
		//try checking sides - look at dist to four side points, and cull furthest and closest
		FVector Points[4];
		Points[0] = OtherActorLocation - FVector(OtherRadius, -1 * OtherRadius, 0);
		Points[1] = OtherActorLocation + FVector(OtherRadius, OtherRadius, 0);
		Points[2] = OtherActorLocation - FVector(OtherRadius, OtherRadius, 0);
		Points[3] = OtherActorLocation + FVector(OtherRadius, -1 * OtherRadius, 0);
		int32 IndexMin = 0;
		int32 IndexMax = 0;
		float CurrentMax = (Points[0] - ViewPoint).SizeSquared();
		float CurrentMin = CurrentMax;
		for (int32 PointIndex = 1; PointIndex<4; PointIndex++)
		{
			const float NextSize = (Points[PointIndex] - ViewPoint).SizeSquared();
			if (NextSize > CurrentMin)
			{
				CurrentMin = NextSize;
				IndexMax = PointIndex;
			}
			else if (NextSize < CurrentMax)
			{
				CurrentMax = NextSize;
				IndexMin = PointIndex;
			}
		}

		for (int32 PointIndex = 0; PointIndex<4; PointIndex++)
		{
			if ((PointIndex != IndexMin) && (PointIndex != IndexMax))
			{
				bHit = GetWorld()->LineTraceTestByChannel(ViewPoint, Points[PointIndex], ECC_Visibility, CollisionParams);
				if (!bHit)
				{
					return true;
				}
			}
		}
	}
	return false;
}

void AAIController::ActorsPerceptionUpdated(const TArray<AActor*>& UpdatedActors)
{

}

DEFINE_LOG_CATEGORY_STATIC(LogTestAI, All, All);

void AAIController::UpdateControlRotation(float DeltaTime, bool bUpdatePawn)
{
	// Look toward focus
	FVector FocalPoint = GetFocalPoint();
	APawn* const Pawn = GetPawn();

	if (Pawn)
	{
		FVector Direction = FAISystem::IsValidLocation(FocalPoint) ? (FocalPoint - Pawn->GetPawnViewLocation()) : Pawn->GetActorForwardVector();
		FRotator NewControlRotation = Direction.Rotation();

		// Don't pitch view unless looking at another pawn
		if (Cast<APawn>(GetFocusActor()) == nullptr)
		{
			NewControlRotation.Pitch = 0.f;
		}
		NewControlRotation.Yaw = FRotator::ClampAxis(NewControlRotation.Yaw);

		if (GetControlRotation().Equals(NewControlRotation, 1e-3f) == false)
		{
			SetControlRotation(NewControlRotation);

			if (bUpdatePawn)
			{
				Pawn->FaceRotation(NewControlRotation, DeltaTime);
			}
		}
	}
}


void AAIController::Possess(APawn* InPawn)
{
	Super::Possess(InPawn);

	if (GetPawn() == nullptr || InPawn == nullptr)
	{
		return;
	}

	// no point in doing navigation setup if pawn has no movement component
	const UPawnMovementComponent* MovementComp = InPawn->GetMovementComponent();
	if (MovementComp != NULL)
	{
		UpdateNavigationComponents();
	}

	if (PathFollowingComponent)
	{
		PathFollowingComponent->Initialize();
	}

	if (bWantsPlayerState)
	{
		ChangeState(NAME_Playing);
	}

	OnPossess(InPawn);
}

void AAIController::UnPossess()
{
	Super::UnPossess();

	if (PathFollowingComponent)
	{
		PathFollowingComponent->Cleanup();
	}

	if (BrainComponent)
	{
		BrainComponent->Cleanup();
	}
}

void AAIController::InitNavigationControl(UPathFollowingComponent*& PathFollowingComp)
{
	PathFollowingComp = PathFollowingComponent;
}

EPathFollowingRequestResult::Type AAIController::MoveToActor(AActor* Goal, float AcceptanceRadius, bool bStopOnOverlap, bool bUsePathfinding, bool bCanStrafe, TSubclassOf<UNavigationQueryFilter> FilterClass, bool bAllowPartialPaths)
{
	FAIMoveRequest MoveReq(Goal);
	MoveReq.SetUsePathfinding(bUsePathfinding);
	MoveReq.SetAllowPartialPath(bAllowPartialPaths);
	MoveReq.SetNavigationFilter(FilterClass);
	MoveReq.SetAcceptanceRadius(AcceptanceRadius);
	MoveReq.SetStopOnOverlap(bStopOnOverlap);
	MoveReq.SetCanStrafe(bCanStrafe);

	return MoveTo(MoveReq);
}

EPathFollowingRequestResult::Type AAIController::MoveToLocation(const FVector& Dest, float AcceptanceRadius, bool bStopOnOverlap, bool bUsePathfinding, bool bProjectDestinationToNavigation, bool bCanStrafe, TSubclassOf<UNavigationQueryFilter> FilterClass, bool bAllowPartialPaths)
{
	FAIMoveRequest MoveReq(Dest);
	MoveReq.SetUsePathfinding(bUsePathfinding);
	MoveReq.SetAllowPartialPath(bAllowPartialPaths);
	MoveReq.SetProjectGoalLocation(bProjectDestinationToNavigation);
	MoveReq.SetNavigationFilter(FilterClass);
	MoveReq.SetAcceptanceRadius(AcceptanceRadius);
	MoveReq.SetStopOnOverlap(bStopOnOverlap);
	MoveReq.SetCanStrafe(bCanStrafe);

	return MoveTo(MoveReq);
}

EPathFollowingRequestResult::Type AAIController::MoveTo(const FAIMoveRequest& MoveRequest)
{
	SCOPE_CYCLE_COUNTER(STAT_MoveTo);
	UE_VLOG(this, LogAINavigation, Log, TEXT("MoveTo: %s"), *MoveRequest.ToString());

	EPathFollowingRequestResult::Type Result = EPathFollowingRequestResult::Failed;
	bool bCanRequestMove = true;
	bool bAlreadyAtGoal = false;

	if (!MoveRequest.HasGoalActor())
	{
		if (MoveRequest.GetGoalLocation().ContainsNaN() || FAISystem::IsValidLocation(MoveRequest.GetGoalLocation()) == false)
		{
			UE_VLOG(this, LogAINavigation, Error, TEXT("AAIController::MoveTo: Destination is not valid! Goal(%s)"), TEXT_AI_LOCATION(MoveRequest.GetGoalLocation()));
			bCanRequestMove = false;
		}

		// fail if projection to navigation is required but it failed
		if (bCanRequestMove && MoveRequest.IsProjectingGoal())
		{
			UNavigationSystem* NavSys = UNavigationSystem::GetCurrent(GetWorld());
			const FNavAgentProperties& AgentProps = GetNavAgentPropertiesRef();
			FNavLocation ProjectedLocation;

			if (NavSys && !NavSys->ProjectPointToNavigation(MoveRequest.GetGoalLocation(), ProjectedLocation, AgentProps.GetExtent(), &AgentProps))
			{
				UE_VLOG_LOCATION(this, LogAINavigation, Error, MoveRequest.GetGoalLocation(), 30.f, FLinearColor::Red, TEXT("AAIController::MoveTo failed to project destination location to navmesh"));
				bCanRequestMove = false;
			}

			MoveRequest.UpdateGoalLocation(ProjectedLocation.Location);
		}

		bAlreadyAtGoal = bCanRequestMove && PathFollowingComponent &&
			PathFollowingComponent->HasReached(MoveRequest.GetGoalLocation(), MoveRequest.GetAcceptanceRadius(), !MoveRequest.CanStopOnOverlap());
	}
	else
	{
		bAlreadyAtGoal = bCanRequestMove && PathFollowingComponent &&
			PathFollowingComponent->HasReached(*MoveRequest.GetGoalActor(), MoveRequest.GetAcceptanceRadius(), !MoveRequest.CanStopOnOverlap());
	}

	if (bAlreadyAtGoal)
	{
		UE_VLOG(this, LogAINavigation, Log, TEXT("MoveToActor: already at goal!"));

		if (PathFollowingComponent)
		{
			// make sure previous move request gets aborted
			PathFollowingComponent->AbortMove(TEXT("Aborting move due to new move request finishing with AlreadyAtGoal"), FAIRequestID::AnyRequest);

			PathFollowingComponent->SetLastMoveAtGoal(true);
		}

		OnMoveCompleted(FAIRequestID::CurrentRequest, EPathFollowingResult::Success);
		Result = EPathFollowingRequestResult::AlreadyAtGoal;
	}
	else if (bCanRequestMove)
	{
		FPathFindingQuery Query;
		const bool bValidQuery = PreparePathfinding(MoveRequest, Query);
		const FAIRequestID RequestID = bValidQuery ? RequestPathAndMove(MoveRequest, Query) : FAIRequestID::InvalidRequest;

		if (RequestID.IsValid())
		{
			bAllowStrafe = MoveRequest.CanStrafe();
			Result = EPathFollowingRequestResult::RequestSuccessful;
		}
	}

	if (Result == EPathFollowingRequestResult::Failed)
	{
		if (PathFollowingComponent)
		{
			PathFollowingComponent->SetLastMoveAtGoal(false);
		}

		OnMoveCompleted(FAIRequestID::InvalidRequest, EPathFollowingResult::Invalid);
	}

	return Result;
}

FAIRequestID AAIController::RequestMove(const FAIMoveRequest& MoveRequest, FNavPathSharedPtr Path)
{
	uint32 RequestID = FAIRequestID::InvalidRequest;
	if (PathFollowingComponent)
	{
		RequestID = PathFollowingComponent->RequestMove(Path, MoveRequest.GetGoalActor(), MoveRequest.GetAcceptanceRadius(), MoveRequest.CanStopOnOverlap(), MoveRequest.GetUserData());
	}

	return RequestID;
}

// deprecated
FAIRequestID AAIController::RequestMove(FNavPathSharedPtr Path, AActor* Goal, float AcceptanceRadius, bool bStopOnOverlap, FCustomMoveSharedPtr CustomData)
{
	FAIMoveRequest MoveReq(Goal);
	MoveReq.SetAcceptanceRadius(AcceptanceRadius);
	MoveReq.SetStopOnOverlap(bStopOnOverlap);
	MoveReq.SetUserData(CustomData);

	return RequestMove(MoveReq, Path);
}

bool AAIController::PauseMove(FAIRequestID RequestToPause)
{
	if (PathFollowingComponent != NULL && RequestToPause.IsEquivalent(PathFollowingComponent->GetCurrentRequestId()))
	{
		PathFollowingComponent->PauseMove(RequestToPause);
		return true;
	}
	return false;
}

bool AAIController::ResumeMove(FAIRequestID RequestToResume)
{
	if (PathFollowingComponent != NULL && RequestToResume.IsEquivalent(PathFollowingComponent->GetCurrentRequestId()))
	{
		PathFollowingComponent->ResumeMove(RequestToResume);
		return true;
	}
	return false;
}

void AAIController::StopMovement()
{
	UE_VLOG(this, LogAINavigation, Log, TEXT("AAIController::StopMovement: %s STOP MOVEMENT"), *GetNameSafe(GetPawn()));
	PathFollowingComponent->AbortMove(TEXT("StopMovement"));
}

bool AAIController::PreparePathfinding(const FAIMoveRequest& MoveRequest, FPathFindingQuery& Query)
{
	UNavigationSystem* NavSys = UNavigationSystem::GetCurrent(GetWorld());
	if (NavSys)
	{
		ANavigationData* NavData = MoveRequest.IsUsingPathfinding() ?
			NavSys->GetNavDataForProps(GetNavAgentPropertiesRef()) :
			NavSys->GetAbstractNavData();

		if (NavData)
		{
			FVector GoalLocation = MoveRequest.GetGoalLocation();
			if (MoveRequest.HasGoalActor())
			{
				const INavAgentInterface* NavGoal = Cast<const INavAgentInterface>(MoveRequest.GetGoalActor());
				if (NavGoal)
				{
					const FVector Offset = NavGoal->GetMoveGoalOffset(this);
					GoalLocation = FQuatRotationTranslationMatrix(MoveRequest.GetGoalActor()->GetActorQuat(), NavGoal->GetNavAgentLocation()).TransformPosition(Offset);
				}
				else
				{
					GoalLocation = MoveRequest.GetGoalActor()->GetActorLocation();
				}
			}

			Query = FPathFindingQuery(this, *NavData, GetNavAgentLocation(), GoalLocation, UNavigationQueryFilter::GetQueryFilter(*NavData, MoveRequest.GetNavigationFilter()));
			Query.SetAllowPartialPaths(MoveRequest.IsUsingPartialPaths());

			if (PathFollowingComponent)
			{
				PathFollowingComponent->OnPathfindingQuery(Query);
			}

			return true;
		}
		else
		{
			UE_VLOG(this, LogAINavigation, Warning, TEXT("Unable to find NavigationData instance while calling AAIController::PreparePathfinding"));
		}
	}

	return false;
}

// deprecated
bool AAIController::PreparePathfinding(FPathFindingQuery& Query, const FVector& Dest, AActor* Goal, bool bUsePathfinding, TSubclassOf<class UNavigationQueryFilter> FilterClass)
{
	if (Goal)
	{
		FAIMoveRequest MoveReq(Goal);
		MoveReq.SetUsePathfinding(bUsePathfinding);
		MoveReq.SetNavigationFilter(FilterClass);

		return PreparePathfinding(MoveReq, Query);
	}

	FAIMoveRequest MoveReq(Dest);
	MoveReq.SetUsePathfinding(bUsePathfinding);
	MoveReq.SetNavigationFilter(FilterClass);

	return PreparePathfinding(MoveReq, Query);
}

FAIRequestID AAIController::RequestPathAndMove(const FAIMoveRequest& MoveRequest, FPathFindingQuery& Query)
{
	FAIRequestID RequestID;

	UNavigationSystem* NavSys = UNavigationSystem::GetCurrent(GetWorld());
	if (NavSys)
	{
		FPathFindingResult PathResult = NavSys->FindPathSync(Query);
		if (PathResult.Result != ENavigationQueryResult::Error)
		{
			if (PathResult.IsSuccessful() && PathResult.Path.IsValid())
			{
				if (MoveRequest.HasGoalActor())
				{
					PathResult.Path->SetGoalActorObservation(*MoveRequest.GetGoalActor(), 100.0f);
				}

				PathResult.Path->EnableRecalculationOnInvalidation(true);

				RequestID = RequestMove(MoveRequest, PathResult.Path);
			}
		}
		else
		{
			UE_VLOG(this, LogAINavigation, Error, TEXT("Trying to find path to %s resulted in Error"), *GetNameSafe(MoveRequest.GetGoalActor()));
		}
	}

	return RequestID;
}

// deprecated
FAIRequestID AAIController::RequestPathAndMove(FPathFindingQuery& Query, AActor* Goal, float AcceptanceRadius, bool bStopOnOverlap, FCustomMoveSharedPtr CustomData)
{
	FAIMoveRequest MoveReq(Goal);
	MoveReq.SetAcceptanceRadius(AcceptanceRadius);
	MoveReq.SetStopOnOverlap(bStopOnOverlap);
	MoveReq.SetUserData(CustomData);

	return RequestPathAndMove(MoveReq, Query);
}

EPathFollowingStatus::Type AAIController::GetMoveStatus() const
{
	return (PathFollowingComponent) ? PathFollowingComponent->GetStatus() : EPathFollowingStatus::Idle;
}

bool AAIController::HasPartialPath() const
{
	return (PathFollowingComponent != NULL) && (PathFollowingComponent->HasPartialPath());
}

bool AAIController::IsFollowingAPath() const
{
	return (PathFollowingComponent != nullptr) && PathFollowingComponent->HasValidPath();
}

FVector AAIController::GetImmediateMoveDestination() const
{
	return (PathFollowingComponent) ? PathFollowingComponent->GetCurrentTargetLocation() : FVector::ZeroVector;
}

void AAIController::SetMoveBlockDetection(bool bEnable)
{
	if (PathFollowingComponent)
	{
		PathFollowingComponent->SetBlockDetectionState(bEnable);
	}
}

void AAIController::OnMoveCompleted(FAIRequestID RequestID, EPathFollowingResult::Type Result)
{
	ReceiveMoveCompleted.Broadcast(RequestID, Result);
}

bool AAIController::RunBehaviorTree(UBehaviorTree* BTAsset)
{
	// @todo: find BrainComponent and see if it's BehaviorTreeComponent
	// Also check if BTAsset requires BlackBoardComponent, and if so 
	// check if BB type is accepted by BTAsset.
	// Spawn BehaviorTreeComponent if none present. 
	// Spawn BlackBoardComponent if none present, but fail if one is present but is not of compatible class
	if (BTAsset == NULL)
	{
		UE_VLOG(this, LogBehaviorTree, Warning, TEXT("RunBehaviorTree: Unable to run NULL behavior tree"));
		return false;
	}

	bool bSuccess = true;
	bool bShouldInitializeBlackboard = false;

	// see if need a blackboard component at all
	UBlackboardComponent* BlackboardComp = NULL;
	if (BTAsset->BlackboardAsset)
	{
		bSuccess = UseBlackboard(BTAsset->BlackboardAsset, BlackboardComp);
	}

	if (bSuccess)
	{
		UBehaviorTreeComponent* BTComp = Cast<UBehaviorTreeComponent>(BrainComponent);
		if (BTComp == NULL)
		{
			UE_VLOG(this, LogBehaviorTree, Log, TEXT("RunBehaviorTree: spawning BehaviorTreeComponent.."));

			BTComp = NewObject<UBehaviorTreeComponent>(this, TEXT("BTComponent"));
			BTComp->RegisterComponent();
		}
		
		// make sure BrainComponent points at the newly created BT component
		BrainComponent = BTComp;

		check(BTComp != NULL);
		BTComp->StartTree(*BTAsset, EBTExecutionMode::Looped);
	}

	return bSuccess;
}

bool AAIController::InitializeBlackboard(UBlackboardComponent& BlackboardComp, UBlackboardData& BlackboardAsset)
{
	check(BlackboardComp.GetOwner() == this);
	if (BlackboardComp.InitializeBlackboard(BlackboardAsset))
	{
		OnUsingBlackBoard(&BlackboardComp, &BlackboardAsset);
		return true;
	}
	return false;
}

bool AAIController::UseBlackboard(UBlackboardData* BlackboardAsset, UBlackboardComponent*& BlackboardComponent)
{
	if (BlackboardAsset == NULL)
	{
		UE_VLOG(this, LogBehaviorTree, Log, TEXT("UseBlackboard: trying to use NULL Blackboard asset. Ignoring"));
		return false;
	}

	bool bSuccess = true;
	UBlackboardComponent* BlackboardComp = FindComponentByClass<UBlackboardComponent>();

	if (BlackboardComp == NULL)
	{
		BlackboardComp = NewObject<UBlackboardComponent>(this, TEXT("BlackboardComponent"));
		if (BlackboardComp != NULL)
		{
			InitializeBlackboard(*BlackboardComp, *BlackboardAsset);
			BlackboardComp->RegisterComponent();
		}

	}
	else if (BlackboardComp->GetBlackboardAsset() == NULL)
	{
		InitializeBlackboard(*BlackboardComp, *BlackboardAsset);
	}
	else if (BlackboardComp->GetBlackboardAsset() != BlackboardAsset)
	{
		UE_VLOG(this, LogBehaviorTree, Log, TEXT("UseBlackboard: requested blackboard %s while already has %s instantiated. Forcing new BB.")
			, *GetNameSafe(BlackboardAsset), *GetNameSafe(BlackboardComp->GetBlackboardAsset()));
		InitializeBlackboard(*BlackboardComp, *BlackboardAsset);
	}

	BlackboardComponent = BlackboardComp;

	return bSuccess;
}

bool AAIController::SuggestTossVelocity(FVector& OutTossVelocity, FVector Start, FVector End, float TossSpeed, bool bPreferHighArc, float CollisionRadius, bool bOnlyTraceUp)
{
	// pawn's physics volume gets 2nd priority
	APhysicsVolume const* const PhysicsVolume = GetPawn() ? GetPawn()->GetPawnPhysicsVolume() : NULL;
	float const GravityOverride = PhysicsVolume ? PhysicsVolume->GetGravityZ() : 0.f;
	ESuggestProjVelocityTraceOption::Type const TraceOption = bOnlyTraceUp ? ESuggestProjVelocityTraceOption::OnlyTraceWhileAsceding : ESuggestProjVelocityTraceOption::TraceFullPath;

	return UGameplayStatics::SuggestProjectileVelocity(this, OutTossVelocity, Start, End, TossSpeed, bPreferHighArc, CollisionRadius, GravityOverride, TraceOption);
}
bool AAIController::PerformAction(UPawnAction& Action, EAIRequestPriority::Type Priority, UObject* const Instigator /*= NULL*/)
{
	return ActionsComp != NULL && ActionsComp->PushAction(Action, Priority, Instigator);
}

FString AAIController::GetDebugIcon() const
{
	if (BrainComponent == NULL || BrainComponent->IsRunning() == false)
	{
		return TEXT("/Engine/EngineResources/AICON-Red.AICON-Red");
	}

	return TEXT("/Engine/EngineResources/AICON-Green.AICON-Green");
}


/** Returns PathFollowingComponent subobject **/
UPathFollowingComponent* AAIController::GetPathFollowingComponent() const { return PathFollowingComponent; }
/** Returns ActionsComp subobject **/
UPawnActionsComponent* AAIController::GetActionsComp() const { return ActionsComp; }

UAIPerceptionComponent* AAIController::GetAIPerceptionComponent()
{
	return PerceptionComponent;
}

const UAIPerceptionComponent* AAIController::GetAIPerceptionComponent() const 
{
	return PerceptionComponent;
}