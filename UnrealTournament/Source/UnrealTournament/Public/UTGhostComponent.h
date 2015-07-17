// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

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

	int32 GhostMoveIndex;
	int32 GhostEventIndex;
	int32 GhostInputIndex;
	int32 GhostWeaponIndex;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Ghost)
	TSubclassOf<class UUTGhostData> GhostDataClass;

	class UUTGhostData* GhostData;


	virtual void GhostMove();
	virtual void GhostStartFire(uint8 FireModeNum);
	virtual void GhostStopFire(uint8 FireModeNum);
	virtual void GhostSwitchWeapon(AUTWeapon* NewWeapon);
	virtual void GhostMovementEvent(const FMovementEventInfo& MovementEvent);

protected:

	float GhostStartTime;


	/** 1 bit for each firemode. 0 up 1 pressed*/
	uint8 GhostFireFlags;

	/**Save the old role since a Ghost always runs on ROLE_SimulatedProxy*/
	TEnumAsByte<enum ENetRole> OldRole;
	
};
