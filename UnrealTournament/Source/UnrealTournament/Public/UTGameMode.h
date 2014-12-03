// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "../Private/Slate/Panels/SULobbyGameSettingsPanel.h"
#include "TAttributeProperty.h"
#include "UTServerBeaconLobbyClient.h"
#include "UTGameMode.generated.h"

/** Defines the current state of the game. */

namespace MatchState
{
	extern const FName CountdownToBegin;				// We are entering this map, actors are not yet ticking
	extern const FName MatchEnteringOvertime;			// The game is entering overtime
	extern const FName MatchIsInOvertime;				// The game is in overtime
}

USTRUCT()
struct FRedirectReference
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FString MapName;

	UPROPERTY()
	FString MapURL;
};

UCLASS(Config = Game, Abstract)
class UNREALTOURNAMENT_API AUTGameMode : public AUTBaseGameMode
{
	GENERATED_UCLASS_BODY()

public:
	/** Cached reference to our game state for quick access. */
	UPROPERTY()
	AUTGameState* UTGameState;		

	/** Currently not used, but will be needed later*/
	UPROPERTY(globalconfig)
	float GameDifficulty;		

	/** How long to wait after the end of a match before the transition to the new level start */
	UPROPERTY(globalconfig)
	float EndTimeDelay;			

	/* How long after the end of the match before we display the scoreboard */
	UPROPERTY(globalconfig)
	float EndScoreboardDelay;			

	UPROPERTY(config)
	uint32 bAllowOvertime:1;

	/** If TRUE, force dead players to respawn immediately */
	UPROPERTY(globalconfig)
	bool bForceRespawn;

	/** If true, players will have to all be ready before the match will begin */
	UPROPERTY()
	bool bPlayersMustBeReady;

	/** If true, only those who are tied going in to overtime will be allowed to player - Otherwise everyone will be allowed to fight on until there is a winner */
	UPROPERTY(Config)
	bool bOnlyTheStrongSurvive;

	/** human readable localized name for the game mode */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Game)
	FText DisplayName;

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

	UPROPERTY()
	int32 MinPlayersToStart;

	/** add bots until NumPlayers + NumBots is this number */
	UPROPERTY(GlobalConfig)
	int32 BotFillCount;

	// How long a player must wait before respawning.  Set to 0 for no delay.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Rules)
	float RespawnWaitTime;

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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
	TArray<TSubclassOf<AUTInventory> > DefaultInventory;

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

	/** list of bot characters that may be added */
	UPROPERTY(GlobalConfig)
	TArray<struct FBotCharacter> BotCharacters;

	/** type of SquadAI that contains game specific AI logic for this gametype */
	UPROPERTY(EditDefaultsOnly, Category = AI)
	TSubclassOf<class AUTSquadAI> SquadType;
	/** maximum number of players per squad (except human-led squad if human forces bots to follow) */
	UPROPERTY(EditDefaultsOnly, Category = AI)
	int32 MaxSquadSize;

	/** cached list of mutator assets from the asset registry and native classes, used to allow shorthand names for mutators instead of full paths all the time */
	TArray<FAssetData> MutatorAssets;

	UPROPERTY(Config)
	TArray<FRedirectReference> RedirectReferences;

	/** assign squad to player - note that humans can have a squad for bots to follow their lead
	 * this method should always result in a valid squad being assigned
	 */
	virtual void AssignDefaultSquadFor(AController* C);

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	UFUNCTION(BlueprintImplementableEvent)
	void PostInitGame(const FString& Options);
	/** add a mutator by string path name */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = Mutator)
	virtual void AddMutator(const FString& MutatorPath);
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = Mutator)
	virtual void AddMutatorClass(TSubclassOf<AUTMutator> MutClass);
	virtual void InitGameState();
	virtual APlayerController* Login(class UPlayer* NewPlayer, const FString& Portal, const FString& Options, const TSharedPtr<class FUniqueNetId>& UniqueId, FString& ErrorMessage) override;
	virtual void Reset();
	virtual void RestartGame();
	virtual void BeginGame();
	virtual bool IsEnemy(class AController* First, class AController* Second);
	virtual void Killed(class AController* Killer, class AController* KilledPlayer, class APawn* KilledPawn, TSubclassOf<UDamageType> DamageType);
	virtual void NotifyKilled(AController* Killer, AController* Killed, APawn* KilledPawn, TSubclassOf<UDamageType> DamageType);
	virtual void ScorePickup(AUTPickup* Pickup, AUTPlayerState* PickedUpBy, AUTPlayerState* LastPickedUpBy);
	virtual void ScoreDamage(int32 DamageAmount, AController* Victim, AController* Attacker);
	virtual void ScoreKill(AController* Killer, AController* Other, TSubclassOf<UDamageType> DamageType);
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

	virtual void HandleCountdownToBegin();
	virtual void CheckCountDown();

	virtual void HandleMatchHasStarted();
	virtual void HandleEnteringOvertime();
	virtual void HandleMatchInOvertime();

	virtual void ShowFinalScoreboard();
	virtual void TravelToNextMap();

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
public:
	/** adds a bot to the game, ignoring game settings */
	UFUNCTION(Exec, BlueprintCallable, Category = AI)
	virtual class AUTBot* ForceAddBot(uint8 TeamNum = 255);

	UFUNCTION(BlueprintNativeEvent)
	void ModifyDamage(int32& Damage, FVector& Momentum, APawn* Injured, AController* InstigatedBy, const FHitResult& HitInfo, AActor* DamageCauser, TSubclassOf<UDamageType> DamageType);

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

	virtual FString GetRedirectURL(const FString& MapName) const override;

#if !UE_SERVER
	/** called on the default object of this class by the UI to create widgets to manipulate this game type's settings
	 * you can use TAttributeProperty<> to easily implement get/set delegates that map directly to the config property address
	 * add any such to the ConfigProps array so the menu maintains the shared pointer
	 */
	virtual void CreateConfigWidgets(TSharedPtr<class SVerticalBox> MenuSpace, TArray< TSharedPtr<TAttributePropertyBase> >& ConfigProps);

	/**
	 *	returns the widget that defines the lobby.
	 **/
	virtual TSharedRef<SWidget> CreateLobbyPanel(bool inIsHost, TWeakObjectPtr<class UUTLocalPlayer> inPlayerOwner, TWeakObjectPtr<AUTLobbyMatchInfo>) const;
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
		return MapPrefix.Len() == 0 || MapName.StartsWith(MapPrefix + TEXT("-"));
	}

	virtual void ProcessServerTravel(const FString& URL, bool bAbsolute = false);

	UFUNCTION(BlueprintCallable, Category = Messaging)
	virtual void BlueprintBroadcastLocalized( AActor* Sender, TSubclassOf<ULocalMessage> Message, int32 Switch = 0, APlayerState* RelatedPlayerState_1 = NULL, APlayerState* RelatedPlayerState_2 = NULL, UObject* OptionalObject = NULL );

	UFUNCTION(BlueprintCallable, Category = Messaging)
	virtual void BlueprintSendLocalized( AActor* Sender, AUTPlayerController* Receiver, TSubclassOf<ULocalMessage> Message, int32 Switch = 0, APlayerState* RelatedPlayerState_1 = NULL, APlayerState* RelatedPlayerState_2 = NULL, UObject* OptionalObject = NULL );

	// Returns true if the game wants to restrict all player spawns
	virtual bool RestrictPlayerSpawns();

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

	/**
	 * Converts a string to a bool.  If the string is empty, it will return the default.
	 **/
	inline bool EvalBoolOptions(FString InOpt, bool Default)
	{
		if (!InOpt.IsEmpty())
		{
			if (FCString::Stricmp(*InOpt,TEXT("True") )==0 
				||	FCString::Stricmp(*InOpt,*GTrue.ToString())==0
				||	FCString::Stricmp(*InOpt,*GYes.ToString())==0)
			{
				return true;
			}
			else if(FCString::Stricmp(*InOpt,TEXT("False"))==0
				||	FCString::Stricmp(*InOpt,*GFalse.ToString())==0
				||	FCString::Stricmp(*InOpt,TEXT("No"))==0
				||	FCString::Stricmp(*InOpt,*GNo.ToString())==0)
			{
				return false;
			}
			else
			{
				return FCString::Atoi(*InOpt) != 0;
			}
		}
		else
		{
			return Default;
		}
	}

	// Updates the MCP with the current game state.  Happens once per minute.
	virtual void UpdateOnlineServer();

	virtual void SendEndOfGameStats(FName Reason);

private:
	// hacked into ReceiveBeginPlay() so we can do mutator replacement of Actors and such
	void BeginPlayMutatorHack(FFrame& Stack, RESULT_DECL);

public:
	/**
	 *	Tells an assoicated lobby that this game is ready for joins.
	 **/
	void NotifyLobbyGameIsReady();

protected:
	// A Beacon for communicating back to the lobby
	AUTServerBeaconLobbyClient* LobbyBeacon;

};





