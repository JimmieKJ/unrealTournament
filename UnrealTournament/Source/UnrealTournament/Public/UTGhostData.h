// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Object.h"
#include "UTGhostData.generated.h"

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

/**
 * 
 */
UCLASS(CustomConstructor, Blueprintable)
class UNREALTOURNAMENT_API UUTGhostData : public UObject
{
	GENERATED_UCLASS_BODY()

	UUTGhostData(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	{}

	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Ghost)
	TArray<FGhostMove> GhostMoves;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Ghost)
	TArray<FGhostEvent> GhostEvents;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Ghost)
	TArray<FGhostInput> GhostInputs;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Ghost)
	TArray<FGhostWeapon> GhostWeapons;
};
