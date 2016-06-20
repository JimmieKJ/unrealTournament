// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTPlayerStart.h"

#include "UTTeamPlayerStart.generated.h"

UCLASS()
class UNREALTOURNAMENT_API AUTTeamPlayerStart : public AUTPlayerStart, public IUTTeamInterface
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PlayerStart)
	uint8 TeamNum;

	virtual uint8 GetTeamNum() const
	{
		return TeamNum;
	}

	virtual void SetTeamForSideSwap_Implementation(uint8 NewTeamNum)
	{
		if (Role == ROLE_Authority)
		{
			TeamNum = NewTeamNum;
		}
	}
};