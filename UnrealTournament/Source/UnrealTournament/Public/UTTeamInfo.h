// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTTeamInfo.generated.h"

UCLASS()
class AUTTeamInfo : public AInfo, public IUTTeamInterface
{
	GENERATED_UCLASS_BODY()

	/** team ID, set by UTTeamGameMode */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = ReceivedTeamIndex, Category = Team)
	uint8 TeamIndex;
	/** team color (e.g. for HUD, character material, etc) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = Team)
	FLinearColor TeamColor;

	/** team score */
	UPROPERTY(BlueprintReadWrite, Replicated, Category = Team)
	int32 Score;

	/** internal flag used to prevent the wrong TeamInfo from getting hooked up on clients during seamless travel, because it is possible for two sets to be on the client temporarily */
	UPROPERTY(Replicated)
	bool bFromPreviousLevel;

	UFUNCTION(BlueprintCallable, Category = Team)
	virtual void AddToTeam(AController* C);
	UFUNCTION(BlueprintCallable, Category = Team)
	virtual void RemoveFromTeam(AController* C);

	/** returns current number of players on the team; server only */
	inline uint32 GetSize()
	{
		checkSlow(Role == ROLE_Authority);
		return TeamMembers.Num();
	}

	virtual uint8 GetTeamNum() const override
	{
		return TeamIndex;
	}

	UFUNCTION()
	virtual void ReceivedTeamIndex();

protected:
	/** list of players on this team currently (server only) */
	UPROPERTY(BlueprintReadOnly, Category = Team)
	TArray<AController*> TeamMembers;
};