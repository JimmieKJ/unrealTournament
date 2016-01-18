// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "UTGhostComponent.generated.h"

//TODOTIM: Make all these into objects that they can be derriveed from in BP
// Modders should be able to create their own Ghost events and push them into the recording

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FGhostPlayFinishedDelegate);

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class UNREALTOURNAMENT_API UUTGhostComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UUTGhostComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent( float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction ) override;

	/** Character movement component belongs to */
	UPROPERTY(BlueprintReadOnly)
	AUTCharacter* UTOwner;


	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = Ghost)
	void GhostStartRecording();
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = Ghost)
	void GhostStopRecording();

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = Ghost)
	void GhostStartPlaying();
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = Ghost)
	void GhostStopPlaying();
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = Ghost)
	void GhostMoveToStart();


	UPROPERTY(BlueprintReadOnly, Category = Ghost)
	bool bGhostRecording;
	UPROPERTY(BlueprintReadOnly, Category = Ghost)
	bool bGhostPlaying;

	UPROPERTY(BlueprintAssignable)
	FGhostPlayFinishedDelegate OnGhostPlayFinished;

	int32 GhostEventIndex;

	/** The asset to save the ghost data to. If this is null, the recording will be temporary for the lifetime of the character*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Ghost)
	class UUTGhostData* GhostData;

	virtual void GhostMove();
	virtual void GhostStartFire(uint8 FireModeNum);
	virtual void GhostStopFire(uint8 FireModeNum);
	virtual void GhostSwitchWeapon(AUTWeapon* NewWeapon);
	virtual void GhostMovementEvent(const FMovementEventInfo& MovementEvent);
	virtual void GhostJumpBoots(TSubclassOf<class AUTReplicatedEmitter> SuperJumpEffect, USoundBase* SuperJumpSound);

	UFUNCTION(BlueprintCallable, Category = Ghost)
	class UUTGhostEvent* CreateAndAddEvent(TSubclassOf<class UUTGhostEvent> EventClass);

	/** 1 bit for each firemode. 0 up 1 pressed*/
	uint8 GhostFireFlags;

	/**The maximum amount of time to save move events*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Ghost)
	float MaxMoveDelta;

	float NextMoveTime;
	uint8 OldCompressedFlags;

protected:

	float GhostStartTime;


	/**Save the old role since a Ghost always runs on ROLE_SimulatedProxy*/
	TEnumAsByte<enum ENetRole> OldRole;
	
};
