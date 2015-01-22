// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AITypes.h"
#include "GameFramework/Pawn.h"
#include "AI/Navigation/NavigationTypes.h"
#include "AI/Navigation/NavigationSystem.h"
#include "Navigation/PathFollowingComponent.h"
#include "GameFramework/Controller.h"
#include "Actions/PawnActionsComponent.h"
#include "Perception/AIPerceptionListenerInterface.h"
#include "BehaviorTree/BehaviorTreeTypes.h"
#include "AIController.generated.h"

class APawn;
class UPathFollowingComponent;
class UBrainComponent;
class UAIPerceptionComponent;
class UPawnAction;
class UPawnActionsComponent;
class UNavigationQueryFilter;
class UBehaviorTree;
class UBlackboardData;
class UCanvas;
#if ENABLE_VISUAL_LOG
struct FVisualLogEntry;
#endif // ENABLE_VISUAL_LOG
struct FPathFindingQuery;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FAIMoveCompletedSignature, FAIRequestID, RequestID, EPathFollowingResult::Type, Result);

// the reason for this being namespace instead of a regular enum is
// so that it can be expanded in game-specific code
// @todo this is a bit messy, needs to be refactored
namespace EAIFocusPriority
{
	typedef uint8 Type;

	const Type Default = 0;
	const Type Move = 1;
	const Type Gameplay = 2;

	const Type LastFocusPriority = Gameplay;
}

struct FFocusKnowledge
{
	struct FFocusItem
	{
		TWeakObjectPtr<AActor> Actor;
		FVector Position;

		FFocusItem()
		{
			Actor = nullptr;
			Position = FAISystem::InvalidLocation;
		}
	};
	
	TArray<FFocusItem> Priorities;
};

//=============================================================================
/**
 * AIController is the base class of controllers for AI-controlled Pawns.
 * 
 * Controllers are non-physical actors that can be attached to a pawn to control its actions.
 * AIControllers manage the artificial intelligence for the pawns they control.
 * In networked games, they only exist on the server.
 *
 * @see https://docs.unrealengine.com/latest/INT/Gameplay/Framework/Controller/
 */

UCLASS(ClassGroup = AI, BlueprintType, Blueprintable)
class AIMODULE_API AAIController : public AController, public IAIPerceptionListenerInterface
{
	GENERATED_BODY()

protected:
	FFocusKnowledge	FocusInformation;

public:
	/** used for alternating LineOfSight traces */
	UPROPERTY()
	mutable uint32 bLOSflag:1;

	/** Skip extra line of sight traces to extremities of target being checked. */
	UPROPERTY()
	uint32 bSkipExtraLOSChecks:1;

	/** Is strafing allowed during movement? */
	UPROPERTY()
	uint32 bAllowStrafe:1;	

	/** Specifies if this AI wants its own PlayerState. */
	UPROPERTY()
	uint32 bWantsPlayerState:1;

private_subobject:

	/** Component used for moving along a path. */
	DEPRECATED_FORGAME(4.6, "PathFollowingComponent should not be accessed directly, please use GetPathFollowingComponent() function instead. PathFollowingComponent will soon be private and your code will not compile.")
	UPROPERTY()
	UPathFollowingComponent* PathFollowingComponent;

public:

	/** Component responsible for behaviors. */
	UPROPERTY(BlueprintReadWrite, Category = AI)
	UBrainComponent* BrainComponent;

	UPROPERTY(BlueprintReadOnly, Category = AI)
	UAIPerceptionComponent* PerceptionComponent;

private_subobject:
	DEPRECATED_FORGAME(4.6, "ActionsComp should not be accessed directly, please use GetActionsComp() function instead. ActionsComp will soon be private and your code will not compile.")
	UPROPERTY(BlueprintReadOnly, Category = AI, meta = (AllowPrivateAccess = "true"))
	UPawnActionsComponent* ActionsComp;

public:

	AAIController(const FObjectInitializer& ObjectInitializer);

	/** Event called when PossessedPawn is possesed by this controller. */
	UFUNCTION(BlueprintImplementableEvent)
	virtual void OnPossess(APawn* PossessedPawn);

	/** Makes AI go toward specified Goal actor (destination will be continuously updated)
	 *  @param AcceptanceRadius - finish move if pawn gets close enough
	 *  @param bStopOnOverlap - add pawn's radius to AcceptanceRadius
	 *  @param bUsePathfinding - use navigation data to calculate path (otherwise it will go in straight line)
	 *  @param bCanStrafe - set focus related flag: bAllowStrafe
	 *	@note AcceptanceRadius has default value or -1 due to Header Parser not being able to recognize UPathFollowingComponent::DefaultAcceptanceRadius
	 */
	UFUNCTION(BlueprintCallable, Category="AI|Navigation")
	EPathFollowingRequestResult::Type MoveToActor(AActor* Goal, float AcceptanceRadius = -1, bool bStopOnOverlap = true, bool bUsePathfinding = true, bool bCanStrafe = true, TSubclassOf<UNavigationQueryFilter> FilterClass = NULL);
	// @todo: above should be: 
	// EPathFollowingRequestResult::Type MoveToActor(AActor* Goal, float AcceptanceRadius = /*UPathFollowingComponent::DefaultAcceptanceRadius==*/-1, bool bStopOnOverlap = true, bool bUsePathfinding = true, bool bCanStrafe = true);
	// but parser doesn't like this (it's a bug, when fixed this will be changed)

	/** Makes AI go toward specified Dest location
	 *  @param AcceptanceRadius - finish move if pawn gets close enough
	 *  @param bStopOnOverlap - add pawn's radius to AcceptanceRadius
	 *  @param bUsePathfinding - use navigation data to calculate path (otherwise it will go in straight line)
	 *  @param bCanStrafe - set focus related flag: bAllowStrafe
	 *	@note AcceptanceRadius has default value or -1 due to Header Parser not being able to recognize UPathFollowingComponent::DefaultAcceptanceRadius
	 */
	UFUNCTION(BlueprintCallable, Category="AI|Navigation")
	EPathFollowingRequestResult::Type MoveToLocation(const FVector& Dest, float AcceptanceRadius = -1, bool bStopOnOverlap = true, bool bUsePathfinding = true, bool bProjectDestinationToNavigation = false, bool bCanStrafe = true, TSubclassOf<UNavigationQueryFilter> FilterClass = NULL);
	// @todo: above should be: 
	// EPathFollowingRequestResult::Type MoveToLocation(const FVector& Dest, float AcceptanceRadius = /*UPathFollowingComponent::DefaultAcceptanceRadius==*/-1, bool bStopOnOverlap = true, bool bUsePathfinding = true, bool bCanStrafe = true);
	// but parser doesn't like this (it's a bug, when fixed this will be changed)

	/** Prepares a query for pathfinding
	 *  @param Query - query struct to fill
	 *  @param Dest - destination location
	 *  @param Goal - goal actor (if applies)
	 *  @param bUsePathfinding - calculate paths vs move in straight line
	 *  @param QueryFilter - optional filter for path finding
	 *  @return true if query was filled in successfully
	 */
	virtual bool PreparePathfinding(FPathFindingQuery& Query, const FVector& Dest, AActor* Goal, bool bUsePathfinding = true, TSubclassOf<class UNavigationQueryFilter> FilterClass = NULL);

	/** Executes pathfinding query and starts move request
	 *  @param Query - query struct holding pathfinding data
	 *  @param Goal - goal actor if following actor
	 *  @param AcceptanceRadius - finish move if pawn gets close enough
	 *  @param CustomData - game specific data, that will be passed to pawn's movement component
	 *  @return RequestID, or 0 when failed
	 */
	virtual FAIRequestID RequestPathAndMove(FPathFindingQuery& Query, AActor* Goal, float AcceptanceRadius, bool bStopOnOverlap, FCustomMoveSharedPtr CustomData);

	/** Handle move requests
	 *  @param Path - path to follow (can use incomplete)
	 *  @param Goal - goal actor if following actor
	 *  @param AcceptanceRadius - finish move if pawn gets close enough
	 *  @param CustomData - game specific data, that will be passed to pawn's movement component
	 *  @return RequestID, or 0 when failed
	 */
	virtual FAIRequestID RequestMove(FNavPathSharedPtr Path, AActor* Goal = NULL, float AcceptanceRadius = UPathFollowingComponent::DefaultAcceptanceRadius, bool bStopOnOverlap = true, FCustomMoveSharedPtr CustomData = NULL);

	/** if AI is currently moving due to request given by RequestToPause, then the move will be paused */
	bool PauseMove(FAIRequestID RequestToPause);

	/** resumes last AI-performed, paused request provided it's ID was equivalent to RequestToResume */
	bool ResumeMove(FAIRequestID RequestToResume);

	/** Aborts the move the controller is currently performing */
	virtual void StopMovement() override;

	/** Called on completing current movement request */
	virtual void OnMoveCompleted(FAIRequestID RequestID, EPathFollowingResult::Type Result);

	/** Returns the Move Request ID for the current move */
	FORCEINLINE FAIRequestID GetCurrentMoveRequestID() const { return GetPathFollowingComponent() ? GetPathFollowingComponent()->GetCurrentRequestId() : FAIRequestID::InvalidRequest; }

	/** Blueprint notification that we've completed the current movement request */
	UPROPERTY(BlueprintAssignable, meta=(DisplayName="MoveCompleted"))
	FAIMoveCompletedSignature ReceiveMoveCompleted;

	/** Returns status of path following */
	UFUNCTION(BlueprintCallable, Category="AI|Navigation")
	EPathFollowingStatus::Type GetMoveStatus() const;

	/** Returns true if the current PathFollowingComponent's path is partial (does not reach desired destination). */
	UFUNCTION(BlueprintCallable, Category="AI|Navigation")
	bool HasPartialPath() const;
	
	/** Returns position of current path segment's end. */
	UFUNCTION(BlueprintCallable, Category="AI|Navigation")
	FVector GetImmediateMoveDestination() const;

	/** Updates state of movement block detection. */
	UFUNCTION(BlueprintCallable, Category="AI|Navigation")
	void SetMoveBlockDetection(bool bEnable);

	/** Prepares path finding and path following components. */
	virtual void InitNavigationControl(UPathFollowingComponent*& PathFollowingComp) override;

	/** Starts executing behavior tree. */
	UFUNCTION(BlueprintCallable, Category="AI")
	virtual bool RunBehaviorTree(UBehaviorTree* BTAsset);

	/** makes AI use specified BB asset */
	UFUNCTION(BlueprintCallable, Category = "AI")
	bool UseBlackboard(UBlackboardData* BlackboardAsset);

	/** does this AIController allow given UBlackboardComponent sync data with it */
	virtual bool ShouldSyncBlackboardWith(const UBlackboardComponent& OtherBlackboardComponent) const { return true; }
		

protected:
	UFUNCTION(BlueprintImplementableEvent)
	virtual void OnUsingBlackBoard(UBlackboardComponent* BlackboardComp, UBlackboardData* BlackboardAsset);

	virtual bool InitializeBlackboard(UBlackboardComponent& BlackboardComp, UBlackboardData& BlackboardAsset);

public:
	/** Retrieve the final position that controller should be looking at. */
	UFUNCTION(BlueprintCallable, Category="AI")
	FVector GetFocalPoint() const;

	FVector GetFocalPointForPriority(EAIFocusPriority::Type InPriority) const;

	/** Retrieve the focal point this controller should focus to on given actor. */
	UFUNCTION(BlueprintCallable, Category="AI")
	virtual FVector GetFocalPointOnActor(const AActor *Actor) const;

	/** Set the position that controller should be looking at. */
	UFUNCTION(BlueprintCallable, Category="AI", meta=(FriendlyName="SetFocalPoint"))
	void K2_SetFocalPoint(FVector FP);

	/** Set Focus for actor, will set FocalPoint as a result. */
	UFUNCTION(BlueprintCallable, Category="AI", meta=(FriendlyName="SetFocus"))
	void K2_SetFocus(AActor* NewFocus);

	/** Get the focused actor. */
	UFUNCTION(BlueprintCallable, Category="AI")
	AActor* GetFocusActor() const;

	FORCEINLINE AActor* GetFocusActorForPriority(EAIFocusPriority::Type InPriority) const {  return FocusInformation.Priorities.IsValidIndex(InPriority) ? FocusInformation.Priorities[InPriority].Actor.Get() : nullptr; }

	/** Clears Focus, will also clear FocalPoint as a result */
	UFUNCTION(BlueprintCallable, Category="AI", meta=(FriendlyName="ClearFocus"))
	void K2_ClearFocus();


	/** 
	 * Computes a launch velocity vector to toss a projectile and hit the given destination.
	 * Performance note: Potentially expensive. Nonzero CollisionRadius and bOnlyTraceUp=false are the more expensive options.
	 * 
	 * @param OutTossVelocity - out param stuffed with the computed velocity to use
	 * @param Start - desired start point of arc
	 * @param End - desired end point of arc
	 * @param TossSpeed - Initial speed of the theoretical projectile. Assumed to only change due to gravity for the entire lifetime of the projectile
	 * @param CollisionSize (optional) - is the size of bounding box of the tossed actor (defaults to (0,0,0)
	 * @param bOnlyTraceUp  (optional) - when true collision checks verifying the arc will only be done along the upward portion of the arc
	 * @return - true if a valid arc was computed, false if no valid solution could be found
	 */
	bool SuggestTossVelocity(FVector& OutTossVelocity, FVector Start, FVector End, float TossSpeed, bool bPreferHighArc, float CollisionRadius=0, bool bOnlyTraceUp=false);

	// Begin AActor Interface
	virtual void Tick(float DeltaTime) override;
	virtual void PostInitializeComponents() override;
	virtual void PostRegisterAllComponents() override;
	// End AActor Interface

	// Begin AController Interface
	virtual void Possess(APawn* InPawn) override;
	virtual void UnPossess() override;
	virtual void DisplayDebug(UCanvas* Canvas, const FDebugDisplayInfo& DebugDisplay, float& YL, float& YPos) override;

#if ENABLE_VISUAL_LOG
	virtual void GrabDebugSnapshot(FVisualLogEntry* Snapshot) const override;
#endif

	virtual void Reset() override;
	virtual void GetPlayerViewPoint(FVector& out_Location, FRotator& out_Rotation) const override;

	/**
	 * Checks line to center and top of other actor
	 * @param Other is the actor whose visibility is being checked.
	 * @param ViewPoint is eye position visibility is being checked from.  If vect(0,0,0) passed in, uses current viewtarget's eye position.
	 * @param bAlternateChecks used only in AIController implementation
	 * @return true if controller's pawn can see Other actor.
	 */
	virtual bool LineOfSightTo(const AActor* Other, FVector ViewPoint = FVector(ForceInit), bool bAlternateChecks = false) const override;
	// End AController Interface

	/** Notifies AIController of changes in given actors' perception */
	virtual void ActorsPerceptionUpdated(const TArray<AActor*>& UpdatedActors);

	/** Update direction AI is looking based on FocalPoint */
	virtual void UpdateControlRotation(float DeltaTime, bool bUpdatePawn = true);

	/** Set FocalPoint for given priority as absolute position or offset from base. */
	virtual void SetFocalPoint(FVector NewFocus, EAIFocusPriority::Type InPriority=EAIFocusPriority::Gameplay);

	/* Set Focus actor for given priority, will set FocalPoint as a result. */
	virtual void SetFocus(AActor* NewFocus, EAIFocusPriority::Type InPriority = EAIFocusPriority::Gameplay);

	/** Clears Focus for given priority, will also clear FocalPoint as a result
	 *	@param InPriority focus priority to clear. If you don't know what to use you probably mean EAIFocusPriority::Gameplay*/
	virtual void ClearFocus(EAIFocusPriority::Type InPriority);

	//----------------------------------------------------------------------//
	// IAIPerceptionListenerInterface
	//----------------------------------------------------------------------//
	virtual UAIPerceptionComponent* GetPerceptionComponent() override { return PerceptionComponent; }

	//----------------------------------------------------------------------//
	// Actions
	//----------------------------------------------------------------------//
	bool PerformAction(UPawnAction& Action, EAIRequestPriority::Type Priority, UObject* const Instigator = NULL);

	//----------------------------------------------------------------------//
	// debug/dev-time 
	//----------------------------------------------------------------------//
	virtual FString GetDebugIcon() const;
	
	// Cheat/debugging functions
	static void ToggleAIIgnorePlayers() { bAIIgnorePlayers = !bAIIgnorePlayers; }
	static bool AreAIIgnoringPlayers() { return bAIIgnorePlayers; }

	/** If true, AI controllers will ignore players. */
	static bool bAIIgnorePlayers;

public:
	/** Returns NavComponent subobject **/
	UNavigationComponent* GetNavComponent() const;
	/** Returns PathFollowingComponent subobject **/
	UPathFollowingComponent* GetPathFollowingComponent() const;
	/** Returns ActionsComp subobject **/
	UPawnActionsComponent* GetActionsComp() const;
};

namespace FAISystem
{
	FORCEINLINE bool IsValidControllerAndHasValidPawn(const AController* Controller)
	{
		return Controller != nullptr && Controller->IsPendingKillPending() == false
			&& Controller->GetPawn() != nullptr && Controller->GetPawn()->IsPendingKillPending() == false;
	}
}
