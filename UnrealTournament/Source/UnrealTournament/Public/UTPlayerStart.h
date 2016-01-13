// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTPlayerStart.generated.h"

UCLASS()
class UNREALTOURNAMENT_API AUTPlayerStart : public APlayerStart
{
	GENERATED_BODY()

public:
	/** if set ignore this PlayerStart for Showdown/Team Showdown */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bIgnoreInShowdown;

};