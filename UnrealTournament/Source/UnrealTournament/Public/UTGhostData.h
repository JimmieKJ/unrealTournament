// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Object.h"
#include "UTGhostData.generated.h"


/**
 * 
 */
UCLASS(CustomConstructor, BlueprintType)
class UNREALTOURNAMENT_API UUTGhostData : public UObject
{
	GENERATED_UCLASS_BODY()

	UUTGhostData(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	{}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Ghost)
	FTransform StartTransform;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Ghost)
	TArray<class UUTGhostEvent*> Events;
};
