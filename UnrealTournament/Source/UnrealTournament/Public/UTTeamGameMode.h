// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTTeamGameMode.generated.h"

UCLASS(Abstract)
class UNREALTOURNAMENT_API AUTTeamGameMode : public AUTGameMode
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(BlueprintReadOnly, Category = TeamGame)
	TArray<class AUTTeamInfo*> Teams;
	/** colors assigned to the teams */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = TeamGame)
	TArray<FLinearColor> TeamColors;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = TeamGame)
	TArray<FText> TeamNames;

	/** number of teams to create - set either in defaults or via InitGame() */
	UPROPERTY(EditDefaultsOnly, Category = TeamGame)
	uint8 NumTeams;
	/** class of TeamInfo to spawn */
	UPROPERTY(EditDefaultsOnly, Category = TeamGame)
	TSubclassOf<AUTTeamInfo> TeamClass;
	/** whether the URL can override the number of teams */
	UPROPERTY(EditDefaultsOnly, Category = TeamGame)
	bool bAllowURLTeamCountOverride;
	/** whether we should attempt to keep teams balanced */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = TeamGame)
	bool bBalanceTeams;
	/** whether players should be spawned only on UTTeamPlayerStarts with the appropriate team number */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = TeamGame)
	bool bUseTeamStarts;

	/** percentage of damage applied for friendly fire */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = TeamGame)
	float TeamDamagePct;
	/** percentage of momentum applied for friendly fire */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = TeamGame)
	float TeamMomentumPct;

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void InitGameState() override;
	virtual APlayerController* Login(class UPlayer* NewPlayer, const FString& Portal, const FString& Options, const TSharedPtr<class FUniqueNetId>& UniqueId, FString& ErrorMessage) override;
	virtual void ModifyDamage_Implementation(int32& Damage, FVector& Momentum, APawn* Injured, AController* InstigatedBy, const FHitResult& HitInfo, AActor* DamageCauser) override;
	virtual float RatePlayerStart(APlayerStart* P, AController* Player) override;
	virtual bool CheckScore(AUTPlayerState* Scorer) override;

	virtual bool ChangeTeam(AController* Player, uint8 NewTeam = 255, bool bBroadcast = true);

	/** pick the best team to place this player to keep the teams as balanced as possible
	 * passed in team number is used as tiebreaker if the teams would be just as balanced either way
	 */
	virtual uint8 PickBalancedTeam(AUTPlayerState* PS, uint8 RequestedTeam);

	virtual void CreateConfigWidgets(TSharedPtr<class SVerticalBox> MenuSpace, TArray< TSharedPtr<TAttributePropertyBase> >& ConfigProps) override;

	/**
	 * Find the best player on a given team
	 */
	virtual AUTPlayerState* FindBestPlayerOnTeam(int TeamNumToTest);
	
};