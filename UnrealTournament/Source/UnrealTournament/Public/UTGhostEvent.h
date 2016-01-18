// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Object.h"
#include "UTGhostEvent.generated.h"

/**
 * Baseclass for all ghost events
 */
UCLASS(Blueprintable)
class UNREALTOURNAMENT_API UUTGhostEvent : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Ghost)
	float Time;
	
	UFUNCTION(BlueprintNativeEvent)
	void ApplyEvent(class AUTCharacter* UTC);
};



UCLASS(Blueprintable)
class UNREALTOURNAMENT_API UUTGhostEvent_Move : public UUTGhostEvent
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Ghost)
	FRepUTMovement RepMovement;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Ghost)
	uint8 CompressedFlags;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Ghost)
	uint32 bIsCrouched : 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Ghost)
	uint32 bApplyWallSlide : 1;

	UFUNCTION(BlueprintNativeEvent)
	void ApplyEvent(class AUTCharacter* UTC);
};



UCLASS(Blueprintable)
class UNREALTOURNAMENT_API UUTGhostEvent_MovementEvent : public UUTGhostEvent
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Ghost)
	FMovementEventInfo MovementEvent;

	UFUNCTION(BlueprintNativeEvent)
	void ApplyEvent(class AUTCharacter* UTC);
};



UCLASS(Blueprintable)
class UNREALTOURNAMENT_API UUTGhostEvent_Input : public UUTGhostEvent
{
	GENERATED_BODY()

public:
	/** 1 bit for each firemode. 0 up 1 pressed*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Ghost)
	uint8 FireFlags;

	UFUNCTION(BlueprintNativeEvent)
	void ApplyEvent(class AUTCharacter* UTC);
};



UCLASS(Blueprintable)
class UNREALTOURNAMENT_API UUTGhostEvent_Weapon : public UUTGhostEvent
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Ghost)
	TSubclassOf<AUTWeapon> WeaponClass;

	UFUNCTION(BlueprintNativeEvent)
	void ApplyEvent(class AUTCharacter* UTC);
};


UCLASS(Blueprintable)
class UNREALTOURNAMENT_API UUTGhostEvent_JumpBoots: public UUTGhostEvent
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Ghost)
	TSubclassOf<class AUTReplicatedEmitter> SuperJumpEffect;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Ghost)
	USoundBase* SuperJumpSound;

	UFUNCTION(BlueprintNativeEvent)
	void ApplyEvent(class AUTCharacter* UTC);
};