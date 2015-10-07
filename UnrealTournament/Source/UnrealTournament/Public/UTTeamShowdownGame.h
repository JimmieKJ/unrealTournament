// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTShowdownGame.h"

#include "UTTeamShowdownGame.generated.h"

UCLASS()
class UNREALTOURNAMENT_API AUTTeamShowdownGame : public AUTShowdownGame
{
	GENERATED_BODY()
public:
	AUTTeamShowdownGame(const FObjectInitializer& OI);

	virtual bool ChangeTeam(AController* Player, uint8 NewTeam, bool bBroadcast) override
	{
		return AUTTeamDMGameMode::ChangeTeam(Player, NewTeam, bBroadcast);
	}
	virtual bool ShouldBalanceTeams(bool bInitialTeam) const override
	{
		return AUTTeamDMGameMode::ShouldBalanceTeams(bInitialTeam);
	}

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void ScoreKill_Implementation(AController* Killer, AController* Other, APawn* KilledPawn, TSubclassOf<UDamageType> DamageType) override;
	virtual void ScoreExpiredRoundTime() override;
	virtual bool CheckRelevance_Implementation(AActor* Other) override;
};