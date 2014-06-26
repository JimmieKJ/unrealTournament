// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTTeamPlayerStart.generated.h"

UCLASS(CustomConstructor)
class AUTTeamPlayerStart : public APlayerStart, public IUTTeamInterface
{
	GENERATED_UCLASS_BODY()

	AUTTeamPlayerStart(const FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
	{}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PlayerStart)
	uint8 TeamNum;

	virtual uint8 GetTeamNum() const
	{
		return TeamNum;
	}
};