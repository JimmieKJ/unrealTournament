// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
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
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = TeamGame)
	bool bBalanceTeams;

	/** whether we should announce your team */
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = TeamGame)
	bool bAnnounceTeam;

	/** whether players should be spawned only on UTTeamPlayerStarts with the appropriate team number */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = TeamGame)
	bool bUseTeamStarts;

	/** percentage of damage applied for friendly fire */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = TeamGame)
	float TeamDamagePct;

	/** percentage of momentum applied for friendly fire */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = TeamGame)
	float TeamMomentumPct;

	/** if greater than 0 and any team leads by this many or more, the game ends */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = CTF)
	int32 MercyScore;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = TeamGame)
	bool bHighScorerPerTeamBasis;

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void InitGameState() override;
	virtual APlayerController* Login(class UPlayer* NewPlayer, const FString& Portal, const FString& Options, const TSharedPtr<class FUniqueNetId>& UniqueId, FString& ErrorMessage) override;
	virtual bool ModifyDamage_Implementation(int32& Damage, FVector& Momentum, APawn* Injured, AController* InstigatedBy, const FHitResult& HitInfo, AActor* DamageCauser, TSubclassOf<UDamageType> DamageType) override;
	virtual float RatePlayerStart(APlayerStart* P, AController* Player) override;
	virtual bool CheckScore(AUTPlayerState* Scorer) override;
	virtual void PlayEndOfMatchMessage() override;
	virtual void AnnounceMatchStart() override;
	virtual void FindAndMarkHighScorer() override;
	
	virtual void CheckBotCount() override;
	virtual void DefaultTimer() override;

	/** Process team change request.  May fail based on team sizes and balancing rules. */
	virtual bool ChangeTeam(AController* Player, uint8 NewTeam = 255, bool bBroadcast = true);

	/** Put player on new team if it is valid, return true if successful. */
	virtual bool MovePlayerToTeam(AController* Player, AUTPlayerState* PS, uint8 NewTeam);

	/** pick the best team to place this player to keep the teams as balanced as possible
	 * passed in team number is used as tiebreaker if the teams would be just as balanced either way
	 */
	virtual uint8 PickBalancedTeam(AUTPlayerState* PS, uint8 RequestedTeam);

#if !UE_SERVER
	virtual void CreateConfigWidgets(TSharedPtr<class SVerticalBox> MenuSpace, bool bCreateReadOnly, TArray< TSharedPtr<TAttributePropertyBase> >& ConfigProps) override;
#endif

	/**  Find the best player on a given team */
	virtual AUTPlayerState* FindBestPlayerOnTeam(int TeamNumToTest);
	
	/** Only broadcast "dominating" message once. */
	UPROPERTY()
		bool bHasBroadcastDominating;

	/** Broadcast a message when team scores */
	virtual void BroadcastScoreUpdate(APlayerState* ScoringPlayer, AUTTeamInfo* ScoringTeam);

protected:
	virtual void SendEndOfGameStats(FName Reason);
	virtual void UpdateLobbyBadge(FString BadgeText);

};