// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SequenceRecorderActorFilter.generated.h"

USTRUCT()
struct FSequenceRecorderActorFilter
{
	GENERATED_BODY()

	/** Actor classes to accept for recording */
	UPROPERTY(EditAnywhere, Category="Actor Filter")
	TArray<TSubclassOf<AActor>> ActorClassesToRecord;
};