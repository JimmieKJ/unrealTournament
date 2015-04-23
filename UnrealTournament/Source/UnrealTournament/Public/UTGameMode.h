// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "TAttributeProperty.h"
#include "UTServerBeaconLobbyClient.h"
#include "UTBotConfig.h"
#include "UTGameMode.generated.h"

/** Defines the current state of the game. */

namespace MatchState
{
	extern const FName CountdownToBegin;				// We are entering this map, actors are not yet ticking
	extern const FName MatchEnteringOvertime;			// The game is entering overtime
	extern const FName MatchIsInOvertime;				// The game is in overtime
}

/** list of bots user asked to put into the game */
USTRUCT()
struct FSelectedBot
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FString BotName;
	/** team to place them on - note that 255 is valid even for team games (placed on any team) */
	UPROPERTY()
	uint8 Team;

	FSelectedBot()
		: Team(255)
	{}
	FSelectedBot(const FString& InName, uint8 InTeam)
		: BotName(InName), Team(InTeam)
	{}
};

UCLASS(Config = Game, Abstract)
class UNREALTOURNAMENT_API AUTGameMode : public AUTBaseGameMode
{
	GENERATED_UCLASS_BODY()

public:
	/** Cached reference to our game state for quick access. */
	UPROPERTY()
	AUTGameState* UTGameState;		

	/** base difficulty of bots */
	UPROPERTY(globalconfig)
	float GameDifficulty;		

	/** How long to wait after the end of a match before the transition to the new level start */
	UPROPERTY(globalconfig)
	float EndTimeDelay;			

	/* How long after the end of the match before we display the scoreboard */
	UPROPERTY(globalconfig)
	float EndScoreboardDelay;			

	UPROPERTY(EditDefaultsOnly)
	uint32 bAllowOvertime:1;

	/** If TRUE, force dead players to respawn immediately */
	UPROPERTY(config)
	bool bForceRespawn;

	/** If true, only those who are tied going in to overtime will be allowed to player - Otherwise everyone will be allowed to fight on until there is a winner */
	UPROPERTY()
	bool bOnlyTheStrongSurvive;

	UPROPERTY()
	bool bHasRespawnChoices;

	UPROPERTY(EditDefaultsOnly)
	bool bHideInUI;

	/** maximum amount of time (in seconds) to wait for players to be ready before giving up and starting the game anyway; <= 0 means wait forever until everyone readies up */
	UPROPERTY(globalconfig)
	int32 MaxReadyWaitTime;

	/** Score needed to win the match.  Can be overridden with GOALSCORE=x on the url */
	UPROPERTY(config)
	int32 GoalScore;    

	/** How long should the match be.  Can be overridden with TIMELIMIT=x on the url */
	UPROPERTY(config)
	int32 TimeLimit;    

	/** Will be TRUE if the game has ended */
	UPROPERTY()
	uint32 bGameEnded:1;    

	/** Will be TRUE if this is a team game */
	UPROPERTY()
	uint32 bTeamGame:1;

	/** TRUE if we have started the count down to the match starting */
	UPROPERTY()
	bool bStartedCountDown;

	UPROPERTY()
	bool bFirstBloodOccurred;

	/** if set, this setting overrides the number of players that are needed to start a hub instance
	 * (defaults to UTLobbyGameMode's MinPlayersToStart)
	 */
	UPROPERTY(config)
	int32 HubMinPlayers;

	/** Minimum number of players that must have joined match before it will start. */
	UPROPERTY()
	int32 MinPlayersToStart;

	/** After this wait, add bots to min players level */
	UPROPERTY()
	float MaxWaitForPlayers;

	/** World time when match was first ready to start. */
	UPROPERTY()
	float StartPlayTime;

	virtual void StartPlay() override;

	/** add bots until NumPlayers + NumBots is this number */
	UPROPERTY(config)
	int32 BotFillCount;

	// How long a player must wait before respawning.  Set to 0 for no delay.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Rules)
	float RespawnWaitTime;

	// How long a player can wait before being forced respawned.  Set to 0 for no delay.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Rules)
	float ForceRespawnTime;

	/** # of seconds before the match begins */
	UPROPERTY()
	int32 CountDown;

	/** Holds the last place any player started from */
	UPROPERTY()
	class AActor* LastStartSpot;    // last place any player started from

	/** Timestamp of when this game ended */
	UPROPERTY()
	float EndTime;

	/** whether weapon stay is active */
	UPROPERTY()
	bool bWeaponStayActive;

	/** Which actor in the game should all other actors focus on after the game is over */
	UPROPERTY()
	class AActor* EndGameFocus;

	UPROPERTY(EditDefaultsOnly, Category = Game)
	TSubclassOf<class UUTLocalMessage> DeathMessageClass;

	UPROPERTY(EditDefaultsOnly, Category = Game)
	TSubclassOf<class UUTLocalMessage> GameMessageClass;

	UPROPERTY(EditDefaultsOnly, Category = Game)
	TSubclassOf<class UUTLocalMessage> VictoryMessageClass;

	/** Name of the Scoreboard */
	UPROPERTY(Config)
	FStringClassReference ScoreboardClassName;

	/** Remove all items from character inventory list, before giving him game mode's default inventory. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
	bool bClearPlayerInventory;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
	TArray<TSubclassOf<AUTInventory> > DefaultInventory;

	/** If true, characters taking damage lose health. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Game")
	bool bDamageHurtsHealth;

	/** If true, firing weapons costs ammo. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Game")
	bool bAmmoIsLimited;

	/** Toggle invulnerability */
	UFUNCTION(Exec, BlueprintCallable, Category = AI)
	virtual void Demigod();

	/** mutators required for the game, added at startup just before command line mutators */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Game)
	TArray< TSubclassOf<class AUTMutator> > BuiltInMutators;
	
	/** mutators that will load from the config*/
	UPROPERTY(Config)
	TArray<FString> ConfigMutators;

	/** last starting map selected in the UI */
	UPROPERTY(Config)
	FString UILastStartingMap;

	UPROPERTY(Config)
	TArray<FString> MapRotation;

	UPROPERTY()
	FString RconNextMapName;

	/** How long should the server wait when there is no one on it before auto-restarting */
	UPROPERTY(Config)
	int32 AutoRestartTime;

	/** How long has the server been empty */
	int32 EmptyServerTime;

	/** HUD class used for the caster's multiview */
	UPROPERTY(EditAnywhere, NoClear, BlueprintReadWrite, Category = Classes)
	TSubclassOf<class AHUD> CastingGuideHUDClass;

	/** first mutator; mutators are a linked list */
	UPROPERTY(BlueprintReadOnly, Category = Mutator)
	class AUTMutator* BaseMutator;

	virtual void PostInitProperties()
	{
		Super::PostInitProperties();
		if (DisplayName.IsEmpty() || (GetClass() != AUTGameMode::StaticClass() && DisplayName.EqualTo(GetClass()->GetSuperClass()->GetDefaultObject<AUTGameMode>()->DisplayName)))
		{
			DisplayName = FText::FromName(GetClass()->GetFName());
		}
	}

	UPROPERTY(Config)
	TArray<FSelectedBot> SelectedBots;
	/** bot configuration (skill ratings, etc) to use */
	UPROPERTY(Transient)
	UUTBotConfig* BotConfig;

	/** type of SquadAI that contains game specific AI logic for this gametype */
	UPROPERTY(EditDefaultsOnly, Category = AI)
	TSubclassOf<class AUTSquadAI> SquadType;
	/** maximum number of players per squad (except human-led squad if human forces bots to follow) */
	UPROPERTY(EditDefaultsOnly, Category = AI)
	int32 MaxSquadSize;

	/** cached list of mutator assets from the asset registry and native classes, used to allow shorthand names for mutators instead of full paths all the time */
	TArray<FAssetData> MutatorAssets;

	/** whether to record a demo (starts when the countdown starts) */
	UPROPERTY(GlobalConfig)
	bool bRecordDemo;
	/** filename for demos... should use one of the replacement strings or it'll overwrite every game */
	UPROPERTY(GlobalConfig)
	FString DemoFilename;

	/** assign squad to player - note that humans can have a squad for bots to follow their lead
	 * this method should always result in a valid squad being assigned
	 */
	virtual void AssignDefaultSquadFor(AController* C);

	virtual void EntitlementQueryComplete(bool bWasSuccessful, const class FUniqueNetId& UniqueId, const FString& Namespace, const FString& ErrorMessage);

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	UFUNCTION(BlueprintImplementableEvent)
	void PostInitGame(const FString& Options);
	/** add a mutator by string path name */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = Mutator)
	virtual void AddMutator(const FString& MutatorPath);
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = Mutator)
	virtual void AddMutatorClass(TSubclassOf<AUTMutator> MutClass);
	virtual void InitGameState() override;
	virtual APlayerController* Login(class UPlayer* NewPlayer, const FString& Portal, const FString& Options, const TSharedPtr<class FUniqueNetId>& UniqueId, FString& ErrorMessage) override;
	virtual void Reset();
	virtual void RestartGame();
	virtual void BeginGame();
	virtual bool IsEnemy(class AController* First, class AController* Second);
	virtual void Killed(class AController* Killer, class AController* KilledPlayer, class APawn* KilledPawn, TSubclassOf<UDamageType> DamageType);
	virtual void NotifyKilled(AController* Killer, AController* Killed, APawn* KilledPawn, TSubclassOf<UDamageType> DamageType);
	virtual void ScorePickup(AUTPickup* Pickup, AUTPlayerState* PickedUpBy, AUTPlayerState* LastPickedUpBy);
	virtual void ScoreDamage(int32 DamageAmount, AController* Victim, AController* Attacker);
	virtual void ScoreKill(AController* Killer, AController* Other, APawn* KilledPawn, TSubclassOf<UDamageType> DamageType);
	virtual void ScoreObject(AUTCarriedObject* GameObject, AUTCharacter* HolderPawn, AUTPlayerState* Holder, FName Reason);
	virtual bool CheckScore(AUTPlayerState* Scorer);
	virtual void FindAndMarkHighScorer();
	virtual void SetEndGameFocus(AUTPlayerState* Winner);
	virtual void EndGame(AUTPlayerState* Winner, FName Reason);
	virtual void StartMatch();
	virtual void EndMatch();
	virtual void BroadcastDeathMessage(AController* Killer, AController* Other, TSubclassOf<UDamageType> DamageType);
	virtual void PlayEndOfMatchMessage();
	UFUNCTION(BlueprintCallable, Category = UTGame)
	virtual void DiscardInventory(APawn* Other, AController* Killer = NULL);

	virtual void OverridePlayerState(APlayerController* PC, APlayerState* OldPlayerState) override;
	virtual void GenericPlayerInitialization(AController* C) override;
	virtual void PostLogin( APlayerController* NewPlayer );
	virtual void Logout(AController* Exiting) override;
	virtual void RestartPlayer(AController* aPlayer);
	UFUNCTION(BlueprintCallable, Category = UTGame)
	virtual void SetPlayerDefaults(APawn* PlayerPawn) override;
	virtual FString InitNewPlayer(class APlayerController* NewPlayerController, const TSharedPtr<FUniqueNetId>& UniqueId, const FString& Options, const FString& Portal = TEXT("")) override;

	virtual void GiveDefaultInventory(APawn* PlayerPawn);

	virtual void ChangeName(AController* Other, const FString& S, bool bNameChange);

	virtual void StartNewPlayer(APlayerController* NewPlayer);
	virtual bool ShouldSpawnAtStartSpot(AController* Player);
	virtual class AActor* FindPlayerStart( AController* Player, const FString& IncomingName = TEXT("") );
	virtual AActor* ChoosePlayerStart( AController* Player );
	virtual float RatePlayerStart(APlayerStart* P, AController* Player);

	virtual bool ReadyToStartMatch();

	virtual bool HasMatchStarted() const;
	virtual bool IsMatchInProgress() const;
	virtual bool HasMatchEnded() const;
	virtual void SetMatchState(FName NewState);
	virtual void CallMatchStateChangeNotify();

	virtual void HandleCountdownToBegin();
	virtual void CheckCountDown();

	virtual void HandleMatchIsWaitingToStart() override;
	virtual void HandleMatchHasStarted();
	virtual void AnnounceMatchStart();
	virtual void HandleMatchHasEnded() override;
	virtual void HandleEnteringOvertime();
	virtual void HandleMatchInOvertime();

	virtual void ShowFinalScoreboard();
	virtual void TravelToNextMap();

	virtual void RecreateLobbyBeacon();
	virtual void DefaultTimer();
	virtual void CheckGameTime();
	virtual AUTPlayerState* IsThereAWinner(uint32& bTied);
	virtual bool PlayerCanRestart( APlayerController* Player );

	// Allows gametypes to build their rules settings for the mid game menu.
	virtual FText BuildServerRules(AUTGameState* GameState);

	/**
	 *	Builds a \t separated list of rules that will be sent out to clients when they request server info via the UTServerBeaconClient.  
	 **/
	virtual void BuildServerResponseRules(FString& OutRules);

protected:
	/** adds a bot to the game */
	virtual class AUTBot* AddBot(uint8 TeamNum = 255);
	virtual class AUTBot* AddNamedBot(const FString& BotName, uint8 TeamNum = 255);
	/** check for adding/removing bots to satisfy BotFillCount */
	virtual void CheckBotCount();
	/** returns whether we should allow removing the given bot to satisfy the desired player/bot count settings
	 * generally used to defer destruction of bots that currently are important to the current game state, like flag carriers
	 */
	virtual bool AllowRemovingBot(AUTBot* B);
public:
	/** adds a bot to the game, ignoring game settings */
	UFUNCTION(Exec, BlueprintCallable, Category = AI)
	virtual class AUTBot* ForceAddBot(uint8 TeamNum = 255);
	UFUNCTION(Exec, BlueprintCallable, Category = AI)
	virtual class AUTBot* ForceAddNamedBot(const FString& BotName, uint8 TeamNum = 255);
	/** sets bot count, ignoring startup settings */
	UFUNCTION(Exec, BlueprintCallable, Category = AI)
	virtual void SetBotCount(uint8 NewCount);
	/** adds num bots to current total */
	UFUNCTION(Exec, BlueprintCallable, Category = AI)
	virtual void AddBots(uint8 Num);
	/** Remove all bots */
	UFUNCTION(Exec, BlueprintCallable, Category = AI)
	virtual void KillBots();

	/** NOTE: return value is a workaround for blueprint bugs involving ref parameters and is not used */
	UFUNCTION(BlueprintNativeEvent)
	bool ModifyDamage(UPARAM(ref) int32& Damage, UPARAM(ref) FVector& Momentum, APawn* Injured, AController* InstigatedBy, const FHitResult& HitInfo, AActor* DamageCauser, TSubclassOf<UDamageType> DamageType);

	/** used to modify, remove, and replace Actors. Return false to destroy the passed in Actor. Default implementation queries mutators.
	 * note that certain critical Actors such as PlayerControllers can't be destroyed, but we'll still call this code path to allow mutators
	 * to change properties on them
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintAuthorityOnly)
	bool CheckRelevance(AActor* Other);

	/** changes world gravity to the specified value */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = World)
	void SetWorldGravity(float NewGravity);

	/** set or change a player's team
	 * NewTeam is a request, not a guarantee (game mode may force balanced teams, for example)
	 */
	UFUNCTION(BlueprintCallable, Category = TeamGame)
	virtual bool ChangeTeam(AController* Player, uint8 NewTeam = 255, bool bBroadcast = true);
	/** pick the best team to place this player to keep the teams as balanced as possible
	 * passed in team number is used as tiebreaker if the teams would be just as balanced either way
	 */

	virtual TSubclassOf<class AGameSession> GetGameSessionClass() const;
	
	virtual void PreInitializeComponents() override;

	virtual void GameObjectiveInitialized(AUTGameObjective* Obj);

	virtual void GetSeamlessTravelActorList(bool bToEntry, TArray<AActor*>& ActorList) override;

#if !UE_SERVER
	/** called on the default object of this class by the UI to create widgets to manipulate this game type's settings
	 * you can use TAttributeProperty<> to easily implement get/set delegates that map directly to the config property address
	 * add any such to the ConfigProps array so the menu maintains the shared pointer
	 */
	virtual void CreateConfigWidgets(TSharedPtr<class SVerticalBox> MenuSpace, bool bCreateReadOnly, TArray< TSharedPtr<TAttributePropertyBase> >& ConfigProps);
	virtual FString GetHUBPregameFormatString();
#endif

	/**
	 *	returns the default game options for a given lobby.  TODO: make this config so that the server can configure what they feel the default lobby should be
	 **/
	virtual FString GetDefaultLobbyOptions() const;

	/** returns whether the given map name is appropriate for this game type
	 * this is just for UI and doesn't prevent the map from loading via e.g. the console
	 */
	virtual bool SupportsMap(const FString& MapName) const
	{
		// TEMP HACK: sublevel that shouldn't be shown
		if (MapName.EndsWith(TEXT("DM-Circuit_FX")))
		{
			return false;
		}

		return MapPrefix.Len() == 0 || MapName.StartsWith(MapPrefix + TEXT("-"));
	}

	virtual void ProcessServerTravel(const FString& URL, bool bAbsolute = false);

	UFUNCTION(BlueprintCallable, Category = Messaging)
	virtual void BlueprintBroadcastLocalized( AActor* Sender, TSubclassOf<ULocalMessage> Message, int32 Switch = 0, APlayerState* RelatedPlayerState_1 = NULL, APlayerState* RelatedPlayerState_2 = NULL, UObject* OptionalObject = NULL );

	UFUNCTION(BlueprintCallable, Category = Messaging)
	virtual void BlueprintSendLocalized( AActor* Sender, AUTPlayerController* Receiver, TSubclassOf<ULocalMessage> Message, int32 Switch = 0, APlayerState* RelatedPlayerState_1 = NULL, APlayerState* RelatedPlayerState_2 = NULL, UObject* OptionalObject = NULL );

	/** called on the default object of the game class being played to precache announcer sounds
	 * needed because announcers are dynamically loaded for convenience of user announcer packs, so we need to load up the audio we think we'll use at game time
	 */
	virtual void PrecacheAnnouncements(class UUTAnnouncer* Announcer) const;

	/** OverridePickupQuery()
	* when pawn wants to pickup something, mutators are given a chance to modify it. If this function
	* returns true, bAllowPickup will determine if the object can be picked up.
	* Note that overriding bAllowPickup to false from this function without disabling the item in some way will have detrimental effects on bots,
	* since the pickup's AI interface will assume the default behavior and keep telling the bot to pick the item up.
	* @param Other the Pawn that wants the item
	* @param Pickup the Actor containing the item (this may be a UTPickup or it may be a UTDroppedPickup)
	* @param bAllowPickup (out) whether or not the Pickup actor should give its item to Other
	* @return whether or not to override the default behavior with the value of bAllowPickup
	*/
	UFUNCTION(BlueprintNativeEvent, BlueprintAuthorityOnly)
	bool OverridePickupQuery(APawn* Other, TSubclassOf<AUTInventory> ItemClass, AActor* Pickup, bool& bAllowPickup);
protected:


	/** map prefix for valid maps (not including the dash); you can create more broad handling by overriding SupportsMap() */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Game)
	FString MapPrefix;

	/** checks whether the mutator is allowed in this gametype and doesn't conflict with any existing mutators */
	virtual bool AllowMutator(TSubclassOf<AUTMutator> MutClass);

	// Updates the MCP with the current game state.  Happens once per minute.
	virtual void UpdateOnlineServer();

	virtual void SendEndOfGameStats(FName Reason);
	virtual void UpdateSkillRating();

private:
	// hacked into ReceiveBeginPlay() so we can do mutator replacement of Actors and such
	void BeginPlayMutatorHack(FFrame& Stack, RESULT_DECL);

public:
	/**
	 *	Tells an associated lobby that this game is ready for joins.
	 **/
	void NotifyLobbyGameIsReady();

	// How long before a lobby instance times out waiting for players to join and the match to begin.  This is to keep lobby instance servers from sitting around forever.
	UPROPERTY(Config)
	float LobbyInitialTimeoutTime;

	UPROPERTY(Config)
	bool bDisableCloudStats;

	// This is the map that will start as "selected" by default in the lobby
	UPROPERTY(Config)
	FString DefaultLobbyMap;

	// The options that are available for a given game mode
	UPROPERTY(Config)
	FString DefaultLobbyOptions;

	// These options will be forced on the game when played on an instance server using this game mode
	UPROPERTY(Config)
	FString ForcedInstanceGameOptions;

	// The maximum number of players allowed in this lobby.
	UPROPERTY(Config)
	int32 MaxLobbyPlayers;

	UPROPERTY(Config)
	bool bLobbyAllowJoinInProgress;

	bool bDedicatedInstance;

protected:
	// A Beacon for communicating back to the lobby
	UPROPERTY(transient)
	AUTServerBeaconLobbyClient* LobbyBeacon;

	float LastLobbyUpdateTime;
	virtual void ForceLobbyUpdate();

	uint32 HostLobbyListenPort;

	// Update the Lobby with the current stats of the game
	virtual void UpdateLobbyMatchStats(FString Update);

	// Updates the lobby with the current player list
	virtual void UpdateLobbyPlayerList();

	// Updates the badge for the Lobby.  This is called from UpdateLobbyMatchStats() and should be gametype specific
	virtual void UpdateLobbyBadge(FString BadgeText);

	virtual void SendEveryoneBackToLobby();
	
	// When players leave/join or during the end of game state
	virtual void UpdatePlayersPresence();

public:
#if !UE_SERVER
	TSharedRef<SWidget> NewPlayerInfoLine(FString LeftStr, FString RightStr);
	virtual void BuildPlayerInfo(TSharedPtr<SVerticalBox> Panel, AUTPlayerState* PlayerState);
#endif

	virtual void InstanceNextMap(const FString& NextMap);

};

