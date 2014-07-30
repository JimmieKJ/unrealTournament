// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTPlayerState.h"
#include "UTCarriedObjectMessage.h"
#include "UTTeamInterface.h"

#include "UTCarriedObject.generated.h"

UCLASS()
class AUTCarriedObject : public AActor, public IUTTeamInterface
{
	GENERATED_UCLASS_BODY()

	// The Current state of this object.  See ObjectiveState in UTATypes.h
	UPROPERTY(Replicated, BlueprintReadOnly, ReplicatedUsing = OnObjectStateChanged, Category = GameObject)
	FName ObjectState;

	// Holds the UTPlayerState of the person currently holding this object.  

	UPROPERTY(Replicated, BlueprintReadOnly, ReplicatedUsing = OnHolderChanged, Category = GameObject)
	AUTPlayerState* Holder;

	// This is an internal array that holds a list of people who have held this object
	// since it was last on a base.  It's only valid on the server.
	UPROPERTY(BlueprintReadOnly, Category = GameObject)
	TArray<AUTPlayerState*> PreviousHolders;

	// Server Side - Holds a reference to the pawn that is holding this object
	UPROPERTY(BlueprintReadOnly, Category = GameObject)
	AUTCharacter* HoldingPawn;

	// Holds the home base for this object.
	UPROPERTY(BlueprintReadOnly, Category = GameObject)
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

	// If true, when a player on the team matching this object's team picks it up, it will be sent home instead of being picked up.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = GameObject)
	TSubclassOf<UUTCarriedObjectMessage> MessageClass;

	// DONT KNOW IF WE NEED THESE YET

	UPROPERTY(BlueprintReadOnly, Category = GameObject)
	uint32 bLastSecondSave : 1;

	void Init(AUTGameObjective* NewBase);

	// Returns the team number of the team that owns this object
	UFUNCTION()
	virtual uint8 GetTeamNum() const;

	/**
	 *	Changes the current state of the carried object.  NOTE: this should only be called on the server
	 **/
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

	/**
	 *	Sends this object back to it's base
	 **/
	UFUNCTION()
	virtual void SendHome();

	/**
	 *	Uses this carried object
	 **/
	UFUNCTION()
	virtual void Use();

	/**
	 *  Call this to tell the object to score.  
	 **/
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
	TSubobjectPtr<UCapsuleComponent> Collision;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = GameObject)
	TSubobjectPtr<class UProjectileMovementComponent> MovementComponent;

	virtual void OnConstruction(const FTransform& Transform) override;

	void FellOutOfWorld(const UDamageType& dmgType);

	// workaround for bug in AActor implementation
	virtual void OnRep_AttachmentReplication() override;
	// HACK: workaround for engine bug with transform replication when attaching/detaching things
	virtual void OnRep_ReplicatedMovement() override;
	virtual void GatherCurrentMovement() override;

protected:

	// Server Side - Holds a reference to the pawn that is holding this object
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = GameObject)
	uint8 DesiredTeamNum;

	// Server Side - Holds a reference to the pawn that is holding this object
	UPROPERTY(BlueprintReadOnly, Category = GameObject)
	AUTCharacter* LastHoldingPawn;

	// The timestamp of when this object was last taken.
	UPROPERTY(BlueprintReadOnly, Category = GameObject)
	float TakenTime;

	// How long before this object is automatically returned to it's base
	UPROPERTY(BlueprintReadWrite, Category = GameObject)
	float AutoReturnTime;	

	// Sound to play when this object is picked up
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = GameObject)
	USoundCue* PickupSound;	

	// Sound to play when this object is dropped
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = GameObject)
	USoundCue* DroppedSound;	

	// Sound to play when this object is sent home
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = GameObject)
	USoundCue* HomeSound;	

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

	/**
	 *	This function will be called when a pick up is denied.
	 **/
	virtual void PickupDenied(AUTCharacter* Character);

	/**
	 *	Called from both Drop and SendHome - cleans up the current holder.
	 **/
	UFUNCTION()
	virtual void NoLongerHeld();

	/**
	 *	Move the flag to it's home base
	 **/
	UFUNCTION()
	virtual void MoveToHome();

	virtual void SendGameMessage(uint32 Switch, APlayerState* PS1, APlayerState* PS2, UObject* OptionalObject = NULL);

	virtual void TossObject(AUTCharacter* ObjectHolder);
};