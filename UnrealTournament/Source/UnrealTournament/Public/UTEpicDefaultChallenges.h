// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTATypes.h"
#include "UTEpicDefaultChallenges.generated.h"

UCLASS(Config=Challenges, CustomConstructor)
class UNREALTOURNAMENT_API UUTEpicDefaultChallenges : public UObject
{
	GENERATED_UCLASS_BODY()

	UUTEpicDefaultChallenges(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	{
	}

public:
	// Holds the complete list of rules allowed in a Hub.  
	UPROPERTY(Config)
	TArray<FUTChallengeInfo> Challenges;
};




