// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTReplicatedLoadoutInfo.h"
#include "UTGameState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTeamSideSwapDelegate, uint8, Offset);

class AUTGameMode;
class AUTReplicatedMapVoteInfo;

struct FLoadoutInfo;

UCLASS(Config = Game)
class UNREALTOURNAMENT_API AUTGameState : public AGameState
{
	GENERATED_UCLASS_BODY()

	/** server settings */
	UPROPERTY(Replicated, Config, EditAnywhere, BlueprintReadWrite, Category = ServerInfo, replicatedUsing = OnRep_ServerName)
	FString ServerName;
	
	// The message of the day
	UPROPERTY(Replicated, Config, EditAnywhere, BlueprintReadWrite, Category = ServerInfo, replicatedUsing = OnRep_ServerMOTD)
	FString ServerMOTD;

	UFUNCTION()
	virtual void OnRep_ServerName();

	UFUNCTION()
	virtual void OnRep_ServerMOTD();

	// A quick string field for the scoreboard and other browsers that contains description of the server
	UPROPERTY(Replicated, Config, EditAnywhere, BlueprintReadWrite, Category = ServerInfo)
	FString ServerDescription;

	/** teams, if the game type has them */
	UPROPERTY(BlueprintReadOnly, Category = GameState)
	TArray<AUTTeamInfo*> Teams;

	/** If TRUE, then we weapon pick ups to stay on their base */
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = GameState)
	uint32 bWeaponStay:1;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = GameState)
	uint32 bTeamGame : 1;

	/** True if plaeyrs are allowed to switch teams (if team game). */
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = GameState)
	uint32 bAllowTeamSwitches : 1;

	/** If true, we will stop the game clock */
	UPROPERTY(Replicated, BlueprintReadWrite, Category = GameState)
	uint32 bStopGameClock : 1;

	/** If a single player's (or team's) score hits this limited, the game is over */
	UPROPERTY(Replicated, EditDefaultsOnly, BlueprintReadOnly, Category = GameState)
	int32 GoalScore;

	/** The maximum amount of time the game will be */
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = GameState)
	int32 TimeLimit;

	/** amount of time after a player spawns where they are immune to damage from enemies */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = GameState)
	float SpawnProtectionTime;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = GameState)
	TSubclassOf<UUTLocalMessage> MultiKillMessageClass;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = GameState)
	TSubclassOf<UUTLocalMessage> SpreeMessageClass;

	/** amount of time between kills to qualify as a multikill */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = GameState)
	float MultiKillDelay;

	// Used to sync the time on clients to the server. -- See DefaultTimer()
	UPROPERTY(Replicated)
	int32 RemainingMinute;

	// Tell clients if more players are needed before match starts
	UPROPERTY(Replicated)
	int32 PlayersNeeded;

	UPROPERTY(Replicated)
	uint32 bOnlyTheStrongSurvive:1;

	UPROPERTY(Replicated)
	uint32 bViewKillerOnDeath:1;

	/** How much time is remaining in this match. */
	UPROPERTY(Replicated, ReplicatedUsing = OnRep_RemainingTime, BlueprintReadOnly, Category = GameState)
	int32 RemainingTime;

	/** local world time that game ended (i.e. relative to World->TimeSeconds) */
	UPROPERTY(BlueprintReadOnly, Category = GameState)
	float MatchEndTime;

	UFUNCTION()
	virtual void OnRep_RemainingTime();

	/** Returns time in seconds that should be displayed on game clock. */
	virtual float GetClockTime();

	// How long must a player wait before respawning
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = GameState)
	float RespawnWaitTime;

	// How long a player can wait before being forced respawned.  Set to 0 for no delay.
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = GameState)
	float ForceRespawnTime;

	/** offset to level placed team IDs for the purposes of swapping/rotating sides
	 * i.e. if this value is 1 and there are 4 teams, team 0 objects become owned by team 1, team 1 objects become owned by team 2... team 3 objects become owned by team 0
	 */
	UPROPERTY(ReplicatedUsing = OnTeamSideSwap, BlueprintReadOnly, Category = GameState)
	uint8 TeamSwapSidesOffset;
	/** previous value, so we know how much we're changing by */
	UPROPERTY()
	uint8 PrevTeamSwapSidesOffset;

	/** changes team sides; generally offset should be 1 unless it's a 3+ team game and you want to rotate more than one spot */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = GameState)
	virtual void ChangeTeamSides(uint8 Offset = 1);

	UFUNCTION()
	virtual void OnTeamSideSwap();

	UPROPERTY(BlueprintAssignable)
	FTeamSideSwapDelegate TeamSideSwapDelegate;

	UPROPERTY(Replicated, BlueprintReadOnly, ReplicatedUsing = OnWinnerReceived, Category = GameState)
	AUTPlayerState* WinnerPlayerState;

	/** Holds the team of the winning team */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = GameState)
	AUTTeamInfo* WinningTeam;

	UFUNCTION()
	virtual void OnWinnerReceived();

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = GameState)
	virtual void SetTimeLimit(int32 NewTimeLimit);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = GameState)
	virtual void SetGoalScore(int32 NewGoalScore);

	UFUNCTION()
	virtual void SetWinner(AUTPlayerState* NewWinner);

	/** Called once per second (or so depending on TimeDilation) after RemainingTime() has been replicated */
	virtual void DefaultTimer();

	/** Determines if a player is on the same team */
	UFUNCTION(BlueprintCallable, Category = GameState)
	virtual bool OnSameTeam(const AActor* Actor1, const AActor* Actor2);

	/** Determines if 2 PlayerStates are in score order */
	virtual bool InOrder( class AUTPlayerState* P1, class AUTPlayerState* P2 );

	/** Sorts the Player State Array */
	virtual void SortPRIArray();

	/** Returns true if the match state is InProgress or later */
	UFUNCTION(BlueprintCallable, Category = GameState)
	virtual bool HasMatchStarted() const;

	UFUNCTION(BlueprintCallable, Category = GameState)
	virtual bool IsMatchInProgress() const;

	UFUNCTION(BlueprintCallable, Category = GameState)
	virtual bool IsMatchAtHalftime() const;

	UFUNCTION(BlueprintCallable, Category = GameState)
	virtual bool IsMatchInSuddenDeath() const;

	UFUNCTION(BlueprintCallable, Category = GameState)
	virtual bool IsMatchInOvertime() const;

	UFUNCTION(BlueprintCallable, Category = GameState)
	virtual bool IsMatchInCountdown() const;

	UFUNCTION(BlueprintCallable, Category = GameState)
	virtual bool IsMatchIntermission() const;

	virtual void BeginPlay() override;

	/** Return largest SpectatingId value in current PlayerArray. */
	virtual int32 GetMaxSpectatingId();

	/** Return largest SpectatingIdTeam value in current PlayerArray. */
	virtual int32 GetMaxTeamSpectatingId(int32 TeamNum);

	/** add an overlay to the OverlayMaterials list */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = Effects)
	virtual void AddOverlayMaterial(UMaterialInterface* NewOverlay);
	/** find an overlay in the OverlayMaterials list, return its index */
	int32 FindOverlayMaterial(UMaterialInterface* TestOverlay)
	{
		for (int32 i = 0; i < ARRAY_COUNT(OverlayMaterials); i++)
		{
			if (OverlayMaterials[i] == TestOverlay)
			{
				return i;
			}
		}
		return INDEX_NONE;
	}
	/** get overlay material from index */
	UMaterialInterface* GetOverlayMaterial(int32 Index)
	{
		return (Index >= 0 && Index < ARRAY_COUNT(OverlayMaterials)) ? OverlayMaterials[Index] : NULL;
	}
	/** returns first active overlay material given the passed in flags */
	UMaterialInterface* GetFirstOverlay(uint16 Flags)
	{
		// early out
		if (Flags == 0)
		{
			return NULL;
		}
		else
		{
			for (int32 i = 0; i < ARRAY_COUNT(OverlayMaterials); i++)
			{
				if (Flags & (1 << i))
				{
					return OverlayMaterials[i];
				}
			}
			return NULL;
		}
	}

	/**
	 *	This is called from the UTPlayerCameraManage to allow the game to force an override to the current player camera to make it easier for
	 *  Presentation to be controlled by the server.
	 **/
	
	virtual FName OverrideCameraStyle(APlayerController* PCOwner, FName CurrentCameraStyle);

	// Returns the rules for this server.
	virtual FText ServerRules();

	/** used on clients to know when all TeamInfos have been received */
	UPROPERTY(Replicated)
	uint8 NumTeams;

	virtual void PreReplication(IRepChangedPropertyTracker& ChangedPropertyTracker) override;

	virtual void ReceivedGameModeClass() override;

	virtual FText GetGameStatusText();

	virtual void OnRep_MatchState() override;

	virtual void AddPlayerState(class APlayerState* PlayerState) override;

	/** rearrange any players' SpectatingID so that the list of values is continuous starting from 1
	 * generally should not be called during gameplay as reshuffling this list unnecessarily defeats the point
	 */
	virtual void CompactSpectatingIDs();

	UPROPERTY()
		FName SecondaryAttackerStat;

protected:
	/** overlay materials, mapped to bits in UTCharacter's OverlayFlags/WeaponOverlayFlags and used to efficiently handle character/weapon overlays
	 * only replicated at startup so set any used materials via BeginPlay()
	 */
	UPROPERTY(Replicated)
	UMaterialInterface* OverlayMaterials[16];

	virtual void HandleMatchHasEnded() override
	{
		MatchEndTime = GetWorld()->TimeSeconds;
	}

public:
	// Will be true if this is an instanced server from a lobby.
	UPROPERTY(Replicated)
	bool bIsInstanceServer;

	// the GUID of the hub the player should return when they leave.  
	UPROPERTY(Replicated)
	FGuid HubGuid;

	// Holds a list of weapons available for loadouts
	UPROPERTY(Replicated)
	TArray<AUTReplicatedLoadoutInfo*> AvailableLoadout;

	// Adds a weapon to the list of possible loadout weapons.
	virtual void AddLoadoutItem(const FLoadoutInfo& Loadout);

	// Adjusts the cost of a weapon available for loadouts
	virtual void AdjustLoadoutCost(TSubclassOf<AUTInventory> ItemClass, float NewCost);

	/** Game specific rating of a player as a desireable camera focus for spectators. */
	virtual float ScoreCameraView(AUTPlayerState* InPS, AUTCharacter *Character)
	{
		return 0.f;
	};

protected:
	// These IDs are banned for the remainder of the match
	TArray<TSharedPtr<class FUniqueNetId>> TempBans;

public:
	// Returns true if this player has been temp banned from this server/instance
	bool IsTempBanned(const TSharedPtr<class FUniqueNetId>& UniqueId);

	// Registers a vote for temp banning a player.  If the player goes above the threashhold, they will be banned for the remainder of the match
	void VoteForTempBan(AUTPlayerState* BadGuy, AUTPlayerState* Voter);

	UPROPERTY(Config)
	float KickThreshold;

	/** Returns which team side InActor is closest to.   255 = no team. */
	virtual uint8 NearestTeamSide(AActor* InActor)
	{
		return 255;
	};


	// Used to get a list of game modes and maps that can be choosen from the menu.  Typically, this just pulls all of 
	// available local content however, in hubs it will be from data replicated from the server.

	virtual void GetAvailableGameData(TArray<UClass*>& GameModes, TArray<UClass*>& MutatorList);
	virtual void GetAvailableMaps(const TArray<FString>& AllowedMapPrefixes, TArray<TSharedPtr<FMapListItem>>& MapList);

	UPROPERTY()
		TArray<FName> GameScoreStats;

	UPROPERTY()
		TArray<FName> TeamStats;

	UPROPERTY()
		TArray<FName> WeaponStats;

	UPROPERTY()
		TArray<FName> RewardStats;

	UPROPERTY()
		TArray<FName> MovementStats;

	UPROPERTY(Replicated)
	TArray<AUTReplicatedMapVoteInfo*> MapVoteList;

	virtual void CreateMapVoteInfo(const FString& MapPackage,const FString& MapTitle, const FString& MapScreenshotReference);
	void SortVotes();

	// The # of seconds left for voting for a map.
	UPROPERTY(Replicated)
	int32 VoteTimer;

	/** Returns a list of important pickups for this gametype
	*	Used to gather pickups for the spectator slideout
	*	For now, do gamytype specific team sorting here
	*   NOTE: return value is a workaround for blueprint bugs involving ref parameters and is not used
	*/
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = GameState)
	bool GetImportantPickups(UPARAM(ref) TArray<class AUTPickup*>& PickupList);

	/**
	 *	The server will replicate it's session id.  Clients, upon recieving it, will check to make sure they are in that session
	 *  and if not, add themselves.  If at all possible, clients should add themselves to the session before joining a server
	 *  however there are times where that isn't possible (join via IP) and this will act as a catch all.
	 **/
	UPROPERTY(Replicated, ReplicatedUsing = OnRep_ServerSessionId)
	FString ServerSessionId;

protected:
	UFUNCTION()
	virtual void OnRep_ServerSessionId();


	/** map of additional stats to hold match total stats*/
	TMap< FName, float > StatsData;

public:
	UPROPERTY()
	float LastScoreStatsUpdateTime;

	/** Accessors for StatsData. */
	float GetStatsValue(FName StatsName);
	void SetStatsValue(FName StatsName, float NewValue);
	void ModifyStatsValue(FName StatsName, float Change);

};



