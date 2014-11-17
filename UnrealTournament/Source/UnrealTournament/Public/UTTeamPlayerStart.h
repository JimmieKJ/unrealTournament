// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTTeamPlayerStart.generated.h"

UCLASS(CustomConstructor)
class AUTTeamPlayerStart : public APlayerStart, public IUTTeamInterface
{
	GENERATED_UCLASS_BODY()

	AUTTeamPlayerStart(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	{}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PlayerStart)
	uint8 TeamNum;

	virtual uint8 GetTeamNum() const
	{
		return TeamNum;
	}

	virtual void SetTeamForSideSwap_Implementation(uint8 NewTeamNum)
	{
		TeamNum = NewTeamNum;
	}
};