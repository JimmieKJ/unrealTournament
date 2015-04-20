// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTPlayerState.h"
#include "UTCarriedObjectMessage.h"
#include "UTTeamInterface.h"
#include "UTProjectileMovementComponent.h"

#include "UTCarriedObject.generated.h"

class AUTCarriedObject;
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnCarriedObjectStateChangedDelegate, class AUTCarriedObject*, Sender, FName, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCarriedObjectHolderChangedDelegate, class AUTCarriedObject*, Sender);

USTRUCT()
struct FAssistTracker
{
	GENERATED_USTRUCT_BODY()

public:

	// The PlayerState of the player who held it
	UPROPERTY()
	AUTPlayerState* Holder;

	// Total amount of time it's been held.
	UPROPERTY()
	float TotalHeldTime;
};


UCLASS()
class UNREALTOURNAMENT_API AUTCarriedObject : public AActor, public IUTTeamInterface
{
	GENERATED_UCLASS_BODY()

	// The Current state of this object.  See ObjectiveState in UTATypes.h
	UPROPERTY(Replicated, BlueprintReadOnly, ReplicatedUsing = OnObjectStateChanged, Category = GameObject)
	FName ObjectState;

	UFUNCTION(BlueprintCallable, Category = GameObject)
		virtual bool IsHome();

	// Holds the UTPlayerState of the person currently holding this object.  
	UPROPERTY(Replicated, BlueprintReadOnly, ReplicatedUsing = OnHolderChanged, Category = GameObject)
	AUTPlayerState* Holder;

	// Holds the UTPlayerState of the last person to hold this object.  
	UPROPERTY(BlueprintReadOnly, Category = GameObject)
	AUTPlayerState* LastHolder;

	UPROPERTY(BlueprintReadOnly, Category = GameObject)
	float PickedUpTime;

	// Holds a array of information about people who have held this object
	UPROPERTY(BlueprintReadOnly, Category = GameObject)
	TArray<FAssistTracker> AssistTracking;

	/** list of players who have 'rescued' the flag carrier by killing an enemy that is targeting the carrier
	 * reset on return/score
	 */
	UPROPERTY(BlueprintReadOnly, Category = GameObject)
	TArray<AController*> HolderRescuers;

	// Server Side - Holds a reference to the pawn that is holding this object
	UPROPERTY(BlueprintReadOnly, Category = GameObject)
	AUTCharacter* HoldingPawn;

	// Holds the home base for this object.
	UPROPERTY(Replicated, BlueprintReadOnly, Category = GameObject)
	class AUTGameObjective* HomeBase;

	// Holds the team that this object belongs to
	UPROPERTY(Replicated, BlueprintReadOnly, Category = GameObject)
	AUTTeamInfo* Team;

	// Where to display this object relative to the home base
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = GameObject)
	FVector HomeBaseOffset;

	// What bone or socket on holder should this object be attached to
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = GameObject)
	FName Holder3PSocketName;

	// Transform to apply when attaching to a Pawn
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = GameObject)
	FVector Holder3PTransform;

	// Rotation to apply when attaching to a Pawn
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = GameObject)
	FRotator Holder3PRotation;

	// if true, then anyone can pick this object up
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = GameObject)
	uint32 bAnyoneCanPickup:1;

	// If true, when a player on the team matching this object's team picks it up, it will be sent home instead of being picked up.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = GameObject)
	uint32 bTeamPickupSendsHome:1;

	// How long before this object is automatically returned to it's base
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = GameObject)
	float AutoReturnTime;

	// If true, when a player on the team matching this object's team picks it up, it will be sent home instead of being picked up.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = GameObject)
	TSubclassOf<UUTCarriedObjectMessage> MessageClass;

	UPROPERTY(BlueprintAssignable)
	FOnCarriedObjectStateChangedDelegate OnCarriedObjectStateChangedDelegate;

	UPROPERTY(BlueprintAssignable)
	FOnCarriedObjectHolderChangedDelegate OnCarriedObjectHolderChangedDelegate;

	/** Last time a game announcement message was sent */
	UPROPERTY()
	float LastGameMessageTime;

	/** sound played when the object is picked up */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Sounds)
	USoundBase* PickupSound;
	/** sound played when the object is dropped */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Sounds)
	USoundBase* DropSound;

	void Init(AUTGameObjective* NewBase);

	// Returns the team number of the team that owns this object
	UFUNCTION()
	virtual uint8 GetTeamNum() const;
	// not applicable
	virtual void SetTeamForSideSwap_Implementation(uint8 NewTeamNum) override
	{}

	/**	Changes the current state of the carried object.  NOTE: this should only be called on the server*/
	UFUNCTION()
	void ChangeState(FName NewCarriedObjectState);

	/**
	 *	Called when a player picks up this object or called
	 *  @NewHolder	The UTCharacter that picked it up
	 **/
	UFUNCTION()
	virtual void SetHolder(AUTCharacter* NewHolder);

	/**
	 *	Drops the object in to the world and allows it to become a pickup.
	 *  @Killer The controller that cause this object to be dropped
	 **/
	UFUNCTION()
	virtual void Drop(AController* Killer = NULL);

	/**	Sends this object back to its base */
	UFUNCTION()
	virtual void SendHome();
	virtual void SendHomeWithNotify();

	/**	Uses this carried object*/
	UFUNCTION()
	virtual void Use();

	/**  Call this to tell the object to score.*/
	UFUNCTION()
	virtual void Score(FName Reason, AUTCharacter* ScoringPawn, AUTPlayerState* ScoringPS);

	UFUNCTION(BlueprintNativeEvent)
	void TryPickup(AUTCharacter* Character);

	virtual void SetTeam(AUTTeamInfo* NewTeam);

	UFUNCTION()
	virtual void AttachTo(USkeletalMeshComponent* AttachToMesh);

	UFUNCTION()
	virtual void DetachFrom(USkeletalMeshComponent* AttachToMesh);

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = GameObject)
	UCapsuleComponent* Collision;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = GameObject)
	class UUTProjectileMovementComponent* MovementComponent;

	virtual void OnConstruction(const FTransform& Transform) override;

	void FellOutOfWorld(const UDamageType& dmgType);

	// workaround for bug in AActor implementation
	virtual void OnRep_AttachmentReplication() override;
	// HACK: workaround for engine bug with transform replication when attaching/detaching things
	virtual void OnRep_ReplicatedMovement() override;
	virtual void GatherCurrentMovement() override;

	virtual float GetHeldTime(AUTPlayerState* TestHolder);

	/**	@Returns the index of a player in the assist array */
	virtual int32 FindAssist(AUTPlayerState* Holder)
	{
		for (int32 i=0; i<AssistTracking.Num(); i++)
		{
			if (AssistTracking[i].Holder == Holder) return i;
		}

		return -1;
	}

	virtual void PostNetReceiveVelocity(const FVector& NewVelocity) override
	{
		MovementComponent->Velocity = NewVelocity;
	}

protected:
	// Server Side - Holds a reference to the pawn that is holding this object
	UPROPERTY(BlueprintReadOnly, Category = GameObject)
	AUTCharacter* LastHoldingPawn;

	// The timestamp of when this object was last taken.
	UPROPERTY(BlueprintReadOnly, Category = GameObject)
	float TakenTime;

	UFUNCTION()
	virtual void OnObjectStateChanged();

	UFUNCTION()
	virtual void OnHolderChanged();

	UFUNCTION()
	virtual void OnOverlapBegin(AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	/**
	 *	By default, only people on the same team as the object can pick it up.  You can quickly override this by setting bTeamPickupSendsHome to true
	 *  or by override this function.  
	 **/
	virtual bool CanBePickedUpBy(AUTCharacter* Character);

	/**	This function will be called when a pick up is denied.*/
	virtual void PickupDenied(AUTCharacter* Character);

	/**	Called from both Drop and SendHome - cleans up the current holder.*/
	UFUNCTION()
	virtual void NoLongerHeld(AController* InstigatedBy = NULL);

	/**	Move the flag to it's home base*/
	UFUNCTION()
	virtual void MoveToHome();

	virtual void SendGameMessage(uint32 Switch, APlayerState* PS1, APlayerState* PS2, UObject* OptionalObject = NULL);

	virtual void TossObject(AUTCharacter* ObjectHolder);
};