// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTTeamDMGameMode.generated.h"

UCLASS()
class UNREALTOURNAMENT_API AUTTeamDMGameMode : public AUTTeamGameMode
{
	GENERATED_UCLASS_BODY()

	/** whether to reduce team score for team kills */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = TeamDM)
	bool bScoreTeamKills;
	/** whether to reduce team score for suicides */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = TeamDM)
	bool bScoreSuicides;

	virtual void ScoreKill_Implementation(AController* Killer, AController* Other, APawn* KilledPawn, TSubclassOf<UDamageType> DamageType) override;
	virtual void ScoreTeamKill_Implementation(AController* Killer, AController* Other, APawn* KilledPawn, TSubclassOf<UDamageType> DamageType) override;
	virtual AUTPlayerState* IsThereAWinner_Implementation(bool& bTied) override;
	virtual void UpdateSkillRating() override;
	virtual uint8 GetNumMatchesFor(AUTPlayerState* PS, bool bRankedSession) const override;
	virtual int32 GetEloFor(AUTPlayerState* PS, bool bRankedSession) const override;
	virtual void SetEloFor(AUTPlayerState* PS, bool bRankedSession, int32 NewEloValue, bool bIncrementMatchCount) override;
};
