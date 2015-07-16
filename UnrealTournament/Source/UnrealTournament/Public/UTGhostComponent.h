// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "UTGhostComponent.generated.h"

//TODOTIM: Make all these into objects that they can be derriveed from in BP
// Modders should be able to create their own Ghost events and push them into the recording

USTRUCT(BlueprintType)
struct FGhostBase
{
	GENERATED_USTRUCT_BODY()

	FGhostBase() : Time(0.0f) {}
	FGhostBase(float InTime) : Time(InTime) {}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Ghost)
	float Time;
};

USTRUCT(BlueprintType)
struct FGhostMove : public FGhostBase
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Ghost)
	FRepUTMovement RepMovement;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Ghost)
	uint8 CompressedFlags;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Ghost)
	uint32 bIsCrouched : 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Ghost)
	uint32 bApplyWallSlide : 1;
};

USTRUCT(BlueprintType)
struct FGhostEvent : public FGhostBase
{
	GENERATED_USTRUCT_BODY()

	FGhostEvent() : FGhostBase() {}
	FGhostEvent(float InTime, const FMovementEventInfo& InMovementEvent) : FGhostBase(InTime), MovementEvent(InMovementEvent) {}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Ghost)
	FMovementEventInfo MovementEvent;
};

USTRUCT(BlueprintType)
struct FGhostInput : public FGhostBase
{
	GENERATED_USTRUCT_BODY()

	FGhostInput() : FGhostBase(0.0f), FireFlags(0) {}
	FGhostInput(float InTime, uint8 InFireFlags) : FGhostBase(InTime), FireFlags(InFireFlags) {}

	/** 1 bit for each firemode. 0 up 1 pressed*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Ghost)
	uint8 FireFlags;
};

USTRUCT(BlueprintType)
struct FGhostWeapon : public FGhostBase
{
	GENERATED_USTRUCT_BODY()

	FGhostWeapon() : FGhostBase(0.0f), WeaponClass(nullptr) {}
	FGhostWeapon(float InTime, TSubclassOf<AUTWeapon> InWeaponClass) : FGhostBase(InTime), WeaponClass(InWeaponClass) {}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Ghost)
	TSubclassOf<AUTWeapon> WeaponClass;
};

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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Ghost)
	TArray<FGhostMove> GhostMoves;
	int32 GhostMoveIndex;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Ghost)
	TArray<FGhostEvent> GhostEvents;
	int32 GhostEventIndex;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Ghost)
	TArray<FGhostInput> GhostInputs;
	int32 GhostInputIndex;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Ghost)
	TArray<FGhostWeapon> GhostWeapons;
	int32 GhostWeaponIndex;


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
