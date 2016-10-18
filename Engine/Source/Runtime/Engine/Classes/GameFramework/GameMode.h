// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "TimerManager.h"
#include "GameFramework/Info.h"
#include "PlayerStart.h"
#include "GameMode.generated.h"

class FDebugDisplayInfo;
class FUniqueNetId;
struct FUniqueNetIdRepl;

//~=============================================================================
//  GameMode defines the rules and mechanics of the game.  It is only 
//  instanced on the server and will never exist on the client.
//~=============================================================================

/** Possible state of the current match, where a match is all the gameplay that happens on a single map */
namespace MatchState
{
	extern ENGINE_API const FName EnteringMap;			// We are entering this map, actors are not yet ticking
	extern ENGINE_API const FName WaitingToStart;		// Actors are ticking, but the match has not yet started
	extern ENGINE_API const FName InProgress;			// Normal gameplay is occurring. Specific games will have their own state machine inside this state
	extern ENGINE_API const FName WaitingPostMatch;		// Match has ended so we aren't accepting new players, but actors are still ticking
	extern ENGINE_API const FName LeavingMap;			// We are transitioning out of the map to another location
	extern ENGINE_API const FName Aborted;				// Match has failed due to network issues or other problems, cannot continue

	// If a game needs to add additional states, you may need to override HasMatchStarted and HasMatchEnded to deal with the new states
	// Do not add any states before WaitingToStart or after WaitingPostMatch
}

/** Default delegate that provides an implementation for those that don't have special needs other than a toggle */
DECLARE_DELEGATE_RetVal( bool, FCanUnpause );

/** Configurable shortened aliases for GameMode classes.  For convenience when typing urls, for instance. */
USTRUCT()
struct FGameClassShortName
{
	GENERATED_USTRUCT_BODY()

	/** Abbreviation that can be used as an alias for the class name */
	UPROPERTY()
	FString ShortName;

	/** The class name to use when the alias is specified in a URL */
	UPROPERTY()
	FString GameClassName;
};

/**
 * The GameMode defines the game being played. It governs the game rules, scoring, what actors
 * are allowed to exist in this game type, and who may enter the game.
 *
 * It is only instanced on the server and will never exist on the client. 
 *
 * A GameMode actor is instantiated when the level is initialized for gameplay in
 * C++ UGameEngine::LoadMap().  
 * 
 * The class of this GameMode actor is determined by (in order) either the URL ?game=xxx, 
 * the GameMode Override value set in the World Settings, or the DefaultGameMode entry set 
 * in the game's Project Settings.
 *
 * @see https://docs.unrealengine.com/latest/INT/Gameplay/Framework/GameMode/index.html
 */
UCLASS(config=Game, notplaceable, BlueprintType, Blueprintable, Transient, hideCategories=(Info, Rendering, MovementReplication, Replication, Actor), meta=(ShortTooltip="Game Mode defines the game being played, its rules, scoring, and other facets of the game type."))
class ENGINE_API AGameMode : public AInfo
{
	GENERATED_UCLASS_BODY()

	// Code to deal with the match state machine

	/** Returns the current match state, this is an accessor to protect the state machine flow */
	UFUNCTION(BlueprintCallable, Category="Game")
	FName GetMatchState() const { return MatchState; }

	/** Returns true if the match state is InProgress or later */
	UFUNCTION(BlueprintCallable, Category="Game")
	virtual bool HasMatchStarted() const;

	/** Returns true if the match state is InProgress or other gameplay state */
	UFUNCTION(BlueprintCallable, Category="Game")
	virtual bool IsMatchInProgress() const;

	/** Returns true if the match state is WaitingPostMatch or later */
	UFUNCTION(BlueprintCallable, Category="Game")
	virtual bool HasMatchEnded() const;

	/** Transitions to WaitingToStart and calls BeginPlay on actors. */
	UFUNCTION(BlueprintCallable, Category="Game")
	virtual void StartPlay();

	/**
	 * Called after a seamless level transition has been completed on the *new* GameMode.
	 * Used to reinitialize players already in the game as they won't have *Login() called on them
	 */
	virtual void PostSeamlessTravel();

	/** Transition from WaitingToStart to InProgress. You can call this manually, will also get called if ReadyToStartMatch returns true */
	UFUNCTION(BlueprintCallable, Category="Game")
	virtual void StartMatch();

	/** Transition from InProgress to WaitingPostMatch. You can call this manually, will also get called if ReadyToEndMatch returns true */
	UFUNCTION(BlueprintCallable, Category="Game")
	virtual void EndMatch();

	/** Transition to LeavingMap state. Start the transition out of the current map. Called at start of seamless travel, or right before map change for hard travel. */
	virtual void StartToLeaveMap();

	/** Restart the game, by default travel to the current map */
	UFUNCTION(BlueprintCallable, Category="Game")
	virtual void RestartGame();

	/** Return to main menu, and disconnect any players */
	UFUNCTION(BlueprintCallable, Category="Game")
	virtual void ReturnToMainMenuHost();

	/** Report that a match has failed due to unrecoverable error */
	UFUNCTION(BlueprintCallable, Category="Game")
	virtual void AbortMatch();

protected:

	/** What match state we are currently in */
	UPROPERTY(Transient)
	FName MatchState;

	/** Updates the match state and calls the appropriate transition functions */
	virtual void SetMatchState(FName NewState);

	/** Overridable virtual function to dispatch the appropriate transition functions before GameState and Blueprints get SetMatchState calls. */
	virtual void OnMatchStateSet();

	/** Implementable event to respond to match state changes */
	UFUNCTION(BlueprintImplementableEvent, Category="Game", meta=(DisplayName="OnSetMatchState"))
	void K2_OnSetMatchState(FName NewState);

	// Games should override these functions to deal with their game specific logic

	/** Called when the state transitions to WaitingToStart */
	virtual void HandleMatchIsWaitingToStart();

	/** @return True if ready to Start Match. Games should override this */
	UFUNCTION(BlueprintNativeEvent, Category="Game")
	bool ReadyToStartMatch();

	/** Called when the state transitions to InProgress */
	virtual void HandleMatchHasStarted();

	/** @return true if ready to End Match. Games should override this */
	UFUNCTION(BlueprintNativeEvent, Category="Game")
	bool ReadyToEndMatch();

	/** Called when the map transitions to WaitingPostMatch */
	virtual void HandleMatchHasEnded();

	/** Called when the match transitions to LeavingMap */
	virtual void HandleLeavingMap();

	/** Called when the match transitions to Aborted */
	virtual void HandleMatchAborted();

public:

	/**
	 * perform map travels using SeamlessTravel() which loads in the background and doesn't disconnect clients
	 * @see World::SeamlessTravel()
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GameMode")
	uint32 bUseSeamlessTravel : 1;

	/** Whether the game is pauseable. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GameMode")
	uint32 bPauseable:1;

	/** Whether players should immediately spawn when logging in, or stay as spectators until they manually spawn */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="GameMode")
	uint32 bStartPlayersAsSpectators : 1;

	/** Whether the game should immediately start when the first player logs in. Affects the default behavior of ReadyToStartMatch */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="GameMode")
	uint32 bDelayedStart : 1;

	/** Save options string and parse it when needed */
	UPROPERTY(BlueprintReadOnly, Category="GameMode")
	FString OptionsString;

	/** The default pawn class used by players. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Classes)
	TSubclassOf<class APawn> DefaultPawnClass;

	/** HUD class this game uses. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Classes, meta=(DisplayName="HUD Class"))
	TSubclassOf<class AHUD> HUDClass;    

	/** Current number of spectators. */
	UPROPERTY(BlueprintReadOnly, Category=GameMode)
	int32 NumSpectators;    

	/** Current number of human players. */
	UPROPERTY(BlueprintReadOnly, Category=GameMode)
	int32 NumPlayers;    

	/** number of non-human players (AI controlled but participating as a player). */
	UPROPERTY(BlueprintReadOnly, Category=GameMode)
	int32 NumBots;    

	/** Minimum time before player can respawn after dying. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=GameMode, meta=(DisplayName="Minimum Respawn Delay"))
	float MinRespawnDelay;

	/** Game Session handles login approval, arbitration, online game interface */
	UPROPERTY()
	class AGameSession* GameSession;

	/** Number of players that are still traveling from a previous map */
	UPROPERTY(BlueprintReadOnly, Category=GameMode)
	int32 NumTravellingPlayers;

	/** Used to assign unique PlayerIDs to each PlayerState. */
	int32 CurrentID;    

	/** The default player name assigned to players that join with no name specified. */
	UPROPERTY(EditAnywhere, Category=GameMode)
	FText DefaultPlayerName;

	/** Contains strings describing localized game agnostic messages. */
	UPROPERTY()
	TSubclassOf<class ULocalMessage> EngineMessageClass;

	/** The class of PlayerController to spawn for players logging in. */
	UPROPERTY(EditAnywhere, noclear, BlueprintReadOnly, Category=Classes)
	TSubclassOf<class APlayerController> PlayerControllerClass;

	/** The pawn class used by the PlayerController for players when spectating. */
	UPROPERTY(EditAnywhere, noclear, BlueprintReadOnly, Category=Classes)
	TSubclassOf<class ASpectatorPawn> SpectatorClass;

	/** The PlayerController class used when spectating a network replay. */
	UPROPERTY(EditAnywhere, noclear, BlueprintReadOnly, Category = Classes)
	TSubclassOf<class APlayerController> ReplaySpectatorPlayerControllerClass;

	/** A PlayerState of this class will be associated with every player to replicate relevant player information to all clients. */
	UPROPERTY(EditAnywhere, noclear, BlueprintReadOnly, Category=Classes)
	TSubclassOf<class APlayerState> PlayerStateClass;

	/** Class of GameState associated with this GameMode. */
	UPROPERTY(EditAnywhere, noclear, BlueprintReadOnly, Category=Classes)
	TSubclassOf<class AGameState> GameStateClass;

	/** GameState is used to replicate game state relevant properties to all clients. */
	UPROPERTY(Transient)
	class AGameState* GameState;

	/** Helper template to returns the current GameState casted to the desired type. */
	template< class T >
	T* GetGameState() const
	{
		return Cast<T>(GameState);
	}

	/** PlayerStates of players who have disconnected from the server (saved in case they reconnect) */
	UPROPERTY()
	TArray<class APlayerState*> InactivePlayerArray;    

	/** The list of delegates to check before unpausing a game */
	TArray<FCanUnpause> Pausers;

protected:

	/** Handy alternate short names for GameMode classes (e.g. "DM" could be an alias for "MyProject.MyGameModeMP_DM". */
	UPROPERTY(config)
	TArray<struct FGameClassShortName> GameModeClassAliases;

	/** Time a playerstate will stick around in an inactive state after a player logout */
	UPROPERTY(EditAnywhere, Category=GameMode)
	float InactivePlayerStateLifeSpan;

	/** If true, dedicated servers will record replays when HandleMatchHasStarted/HandleMatchHasStopped is called */
	UPROPERTY(config)
	bool bHandleDedicatedServerReplays;

public:

	/** Alters the synthetic bandwidth limit for a running game. */
	UFUNCTION(exec)
	virtual void SetBandwidthLimit(float AsyncIOBandwidthLimit);

	//~ Begin AActor Interface
	virtual void PreInitializeComponents() override;
	virtual void DisplayDebug(class UCanvas* Canvas, const FDebugDisplayInfo& DebugDisplay, float& YL, float& YPos) override;
	virtual void Reset() override;
	virtual void Tick(float DeltaSeconds) override;
	//~ End AActor Interface

	/** 
	 * Overridable function to determine whether an Actor should have Reset called when the game has Reset called on it.
	 * Default implementation returns true
	 * @param ActorToReset The actor to make a determination for
	 * @return true if ActorToReset should have Reset() called on it while restarting the game, 
	 *		   false if the GameMode will manually reset it or if the actor does not need to be reset

	 */
	UFUNCTION(BlueprintNativeEvent, Category="Game")
	bool ShouldReset(AActor* ActorToReset);

	/** 
	  * Overridable function called when resetting level.
	  * Default implementation calls Reset() on all actors except GameMode and Controllers
	  */
	UFUNCTION(BlueprintCallable, Category="Game")
	virtual void ResetLevel();

	/**
	 *	Check to see if we should start in cinematic mode (e.g. matinee movie capture).
	 * 	@param	OutHidePlayer		Whether to hide the player
	 *	@param	OutHideHud			Whether to hide the HUD		
	 *	@param	OutDisableMovement	Whether to disable movement
	 * 	@param	OutDisableTurning	Whether to disable turning
	 *	@return	bool			true if we should turn on cinematic mode, 
	 *							false if if we should not.
	 */
	bool ShouldStartInCinematicMode(APlayerController* Player, bool& OutHidePlayer, bool& OutHideHud, bool& OutDisableMovement, bool& OutDisableTurning);

	/**
	 * Initialize the game.
	 * The GameMode's InitGame() event is called before any other functions (including PreInitializeComponents() )
	 * and is used by the GameMode to initialize parameters and spawn its helper classes.
	 * @warning: this is called before actors' PreInitializeComponents.
	 */
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage);

	/**
	 * Initialize the GameState actor with default settings
	 * called during PreInitializeComponents() of the GameMode after a GameState has been spawned
	 * as well as during Reset()
	 */
	virtual void InitGameState();

	/** Get local address */
	virtual FString GetNetworkNumber();
	
	/** Will remove the controller from the correct count then add them to the spectator count. **/
	void PlayerSwitchedToSpectatorOnly(APlayerController* PC);

	/** Removes the passed in player controller from the correct count for player/spectator/tranistioning **/
	void RemovePlayerControllerFromPlayerCount(APlayerController* PC);

	/** Total number of players */
	UFUNCTION(BlueprintCallable, Category="Game")
	virtual int32 GetNumPlayers();

	/**
	 * Adds the delegate to the list if the player Controller has the right to pause
	 * the game. The delegate is called to see if it is ok to unpause the game, e.g.
	 * the reason the game was paused has been cleared.
	 * @param PC the player Controller to check for admin privs
	 * @param CanUnpauseDelegate the delegate to query when checking for unpause
	 */
	virtual bool SetPause(APlayerController* PC, FCanUnpause CanUnpauseDelegate = FCanUnpause());

	/**
	 * Checks the list of delegates to determine if the pausing can be cleared. If
	 * the delegate says it's ok to unpause, that delegate is removed from the list
	 * and the rest are checked. The game is considered unpaused when the list is
	 * empty.
	 */
	virtual void ClearPause();

	/**
	 * Forcibly removes an object's CanUnpause delegates from the list of pausers.  If any of the object's CanUnpause delegate
	 * handlers were in the list, triggers a call to ClearPause().
	 *
	 * Called when the player controller is being destroyed to prevent the game from being stuck in a paused state when a PC that
	 * paused the game is destroyed before the game is unpaused.
	 */
	void ForceClearUnpauseDelegates( AActor* PauseActor );

	//~=========================================================================
	// URL Parsing
	/** Grab the next option from a string. */
	DEPRECATED(4.9, "Use UGameplayStatics::GrabOption instead")
	static bool GrabOption( FString& Options, FString& ResultString );

	/** Break up a key=value pair into its key and value. */
	DEPRECATED(4.9, "Use UGameplayStatics::GetKeyValue instead")
	static void GetKeyValue( const FString& Pair, FString& Key, FString& Value );

	/* Find an option in the options string and return it. */
	DEPRECATED(4.9, "Use UGameplayStatics::ParseOption instead")
	static FString ParseOption( FString Options, const FString& InKey );

	/** HasOption - return true if the option is specified on the command line. */
	DEPRECATED(4.9, "Use UGameplayStatics::HasOption instead")
	static bool HasOption( FString Options, const FString& InKey );

	/** Search array of options for ParseString and return the index or CurrentValue if not found*/
	DEPRECATED(4.9, "Use UGameplayStatics::GetIntOption instead")
	static int32 GetIntOption( const FString& Options, const FString& ParseString, int32 CurrentValue);
	//~=========================================================================

	/** 
	 * @return the full path to the optimal GameMode class to use for the specified map and options
	 * this is used for preloading cooked packages, etc. and therefore doesn't need to include any fallbacks
	 * as GetGameModeClass() will be called later to actually find/load the desired class
	 */
	virtual FString GetDefaultGameClassPath(const FString& MapName, const FString& Options, const FString& Portal) const;

	/** 
	 * @return the class of GameMode to spawn for the game on the specified map and the specified options
	 * this function should include any fallbacks in case the desired class can't be found
	 */
	virtual TSubclassOf<AGameMode> GetGameModeClass(const FString& MapName, const FString& Options, const FString& Portal) const;

	DEPRECATED(4.5, "AGameMode::SetGameMode renamed AGameMode::GetGameModeClass")
	TSubclassOf<AGameMode> SetGameMode(const FString& MapName, const FString& Options, const FString& Portal);


	/** @return GameSession class to use for this game  */
	virtual TSubclassOf<class AGameSession> GetGameSessionClass() const;

	/** Called when a connection closes before getting to PostLogin() */
	virtual void NotifyPendingConnectionLost();

	/** @return true if we want to travel_absolute */
	virtual bool GetTravelType();

	/** Optional handling of ServerTravel for network games. */
	virtual void ProcessServerTravel(const FString& URL, bool bAbsolute = false);

	/**
	 * Notifies all clients to travel to the specified URL.
	 *
	 * @param	URL				a string containing the mapname (or IP address) to travel to, along with option key/value pairs
	 * @param	NextMapGuid		the GUID of the server's version of the next map
	 * @param	bSeamless		indicates whether the travel should use seamless travel or not.
	 * @param	bAbsolute		indicates which type of travel the server will perform (i.e. TRAVEL_Relative or TRAVEL_Absolute)
	 */
	virtual APlayerController* ProcessClientTravel( FString& URL, FGuid NextMapGuid, bool bSeamless, bool bAbsolute );

	/** 
	 * Accept or reject a player attempting to join the server.  Fails login if you set the ErrorMessage to a non-empty string.
	 * PreLogin is called before Login.  Significant game time may pass before Login is called, especially if content is downloaded.
	 *
	 * @param	Options					The URL options (e.g. name/spectator) the player has passed
	 * @param	Address					The network address of the player
	 * @param	UniqueId				The unique id the player has passed to the server
	 * @param	ErrorMessage			When set to a non-empty value, the player will be rejected using the error message set
	 */
	virtual void PreLogin(const FString& Options, const FString& Address, const TSharedPtr<const FUniqueNetId>& UniqueId, FString& ErrorMessage);
	virtual void PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage);

	/** 
	 * Called to login new players by creating a player controller, overridable by the game
	 *
	 * Sets up basic properties of the player (name, unique id, registers with backend, etc) and should not be used to do
	 * more complicated game logic.  The player controller is not fully initialized within this function as far as networking is concerned.
	 * Save "game logic" for PostLogin which is called shortly afterward.
	 *
	 * @param NewPlayer pointer to the UPlayer object that represents this player (either local or remote)
	 * @param RemoteRole the remote role this controller has
	 * @param Portal desired portal location specified by the client
	 * @param Options game options passed in by the client at login
	 * @param UniqueId platform specific unique identifier for the logging in player
	 * @param ErrorMessage [out] error message, if any, why this login will be failing
	 *
	 * If login is successful, returns a new PlayerController to associate with this player. Login fails if ErrorMessage string is set. 
	 *
	 * @return a new player controller for the logged in player, NULL if login failed for any reason
	 */
	virtual APlayerController* Login(class UPlayer* NewPlayer, ENetRole InRemoteRole, const FString& Portal, const FString& Options, const TSharedPtr<const FUniqueNetId>& UniqueId, FString& ErrorMessage);
	virtual APlayerController* Login(class UPlayer* NewPlayer, ENetRole InRemoteRole, const FString& Portal, const FString& Options, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage);

	/** Called after a successful login.  This is the first place it is safe to call replicated functions on the PlayerAController. */
	virtual void PostLogin( APlayerController* NewPlayer );

	/** Notification that a player has successfully logged in, and has been given a player controller */
	UFUNCTION(BlueprintImplementableEvent, Category="Game", meta=(DisplayName="OnPostLogin"))
	void K2_PostLogin( APlayerController* NewPlayer );

	/** 
	 * Spawns a PlayerController at the specified location; split out from Login()/HandleSeamlessTravelPlayer() for easier overriding 
	 *
	 * @param RemoteRole the role this controller will play remotely
	 * @param SpawnLocation location in the world to spawn
	 * @param SpawnRotation rotation to set relative to the world
	 *
	 * @return PlayerController for the player, NULL if there is any reason this player shouldn't exist or due to some error
	 */
	virtual APlayerController* SpawnPlayerController(ENetRole InRemoteRole, FVector const& SpawnLocation, FRotator const& SpawnRotation);

	/** @Returns true if NewPlayerController may only join the server as a spectator. */
	UFUNCTION(BlueprintNativeEvent, Category="Game")
	bool MustSpectate(APlayerController* NewPlayerController) const;

	/** returns default pawn class for given controller */
	UFUNCTION(BlueprintNativeEvent, Category="Game")
	UClass* GetDefaultPawnClassForController(AController* InController);

	/** Called when StartSpot is selected for spawning NewPlayer to allow optional initialization. */
	UFUNCTION(BlueprintNativeEvent, Category="Game")
	void InitStartSpot(class AActor* StartSpot, AController* NewPlayer);

	/**
	 * @param	NewPlayer - Controller for whom this pawn is spawned
	 * @param	StartSpot - PlayerStart at which to spawn pawn
	 * @return	a pawn of the default pawn class
	 */
	UFUNCTION(BlueprintNativeEvent, Category="Game")
	APawn* SpawnDefaultPawnFor(AController* NewPlayer, class AActor* StartSpot);

	/** replicates the current level streaming status to the given PlayerController */
	virtual void ReplicateStreamingStatus(APlayerController* PC);

	/**
	 * handles all player initialization that is shared between the travel methods
	 * (i.e. called from both PostLogin() and HandleSeamlessTravelPlayer())
	 */
	virtual void GenericPlayerInitialization(AController* C);

	/** start match, or let player enter, immediately */
	virtual void StartNewPlayer(APlayerController* NewPlayer);

	/** Called when a Controller with a PlayerState leaves the match. */
	virtual void Logout( AController* Exiting );

	/** Implementable event when a Controller with a PlayerState leaves the match. */
	UFUNCTION(BlueprintImplementableEvent, Category="Game", meta=(DisplayName="OnLogout"))
	void K2_OnLogout(AController* ExitingController);

	/**
	 * Make sure pawn properties are back to default
	 * Also a good place to modify them on spawn
	 */
	virtual void SetPlayerDefaults(APawn* PlayerPawn);

	/** Return whether Viewer is allowed to spectate from the point of view of ViewTarget. */
	UFUNCTION(BlueprintNativeEvent, Category="Game")
	bool CanSpectate( APlayerController* Viewer, APlayerState* ViewTarget );

	/** 
	 * Sets the name for a controller 
	 * @param Controller	The controller of the player to change the name of
	 * @param NewName		The name to set the player to
	 * @param bNameChange	Whether the name is changing or if this is the first time it has been set
	 */
	UFUNCTION(BlueprintCallable, Category="Game")
	virtual void ChangeName( AController* Controller, const FString& NewName, bool bNameChange );

	/** 
	 * Overridable event for GameMode blueprint to respond to a change name call
	 * @param Controller	The controller of the player to change the name of
	 * @param NewName		The name to set the player to
	 * @param bNameChange	Whether the name is changing or if this is the first time it has been set
	 */
	UFUNCTION(BlueprintImplementableEvent,Category="Game",meta=(DisplayName="OnChangeName"))
	void K2_OnChangeName( AController* Other, const FString& NewName, bool bNameChange );

	/* Send a player to a URL.*/
	virtual void SendPlayer( APlayerController* aPlayer, const FString& URL );

	/** Broadcast a string to all players. */
	virtual void Broadcast( AActor* Sender, const FString& Msg, FName Type = NAME_None );

	/**
	 * Broadcast a localized message to all players.
	 * Most message deal with 0 to 2 related PlayerStates.
	 * The LocalMessage class defines how the PlayerState's and optional actor are used.
	 */
	virtual void BroadcastLocalized( AActor* Sender, TSubclassOf<ULocalMessage> Message, int32 Switch = 0, APlayerState* RelatedPlayerState_1 = NULL, APlayerState* RelatedPlayerState_2 = NULL, UObject* OptionalObject = NULL );

	/** @return true if the given Controller StartSpot property should be used as the spawn location for its Pawn */
	virtual bool ShouldSpawnAtStartSpot(AController* Player);

	/** 
	 * Return the 'best' player start for this player to start from.
	 * @param Player The AController for whom we are choosing a Player Start
	 * @param IncomingName Specifies the tag of a Player Start to use
	 * @returns Actor chosen as player start (usually a PlayerStart)
	 */
	UFUNCTION(BlueprintNativeEvent, Category="Game")
	class AActor* FindPlayerStart( AController* Player, const FString& IncomingName = TEXT("") );

	/** Calls code to select the best player start for this player to start from. */
	UFUNCTION(BlueprintPure, Category=Game, meta=(DisplayName="FindPlayerStart"))
	AActor* K2_FindPlayerStart(AController* Player);

	/** 
	* Return the 'best' player start for this player to start from.  
	* Default implementation just returns the first PlayerStart found.
	* @param Player is the controller for whom we are choosing a playerstart
	* @returns AActor chosen as player start (usually a PlayerStart)
	 */
	UFUNCTION(BlueprintNativeEvent, Category="Game")
	class AActor* ChoosePlayerStart( AController* Player );

	/** @return true if it's valid to call RestartPlayer. Will call Player->CanRestartPlayer */
	UFUNCTION(BlueprintNativeEvent, Category="Game")
	bool PlayerCanRestart( APlayerController* Player );

	/**
	 * Used to notify the game type that it is ok to update a player's gameplay
	 * specific muting information now. The playercontroller needs to notify
	 * the server when it is possible to do so or the unique net id will be
	 * incorrect and the muting not work.
	 *
	 * @param aPlayer the playercontroller that is ready for updates
	 */
	virtual void UpdateGameplayMuteList( APlayerController* aPlayer );

	/** @return true if player is allowed to access the cheats */
	virtual bool AllowCheats(APlayerController* P);

	/** @return	true if the player is allowed to pause the game. */
	virtual bool AllowPausing( APlayerController* PC = NULL );

	/**
	 * Called from CommitMapChange before unloading previous level. Used for asynchronous level streaming
	 * @param PreviousMapName - Name of the previous persistent level
	 * @param NextMapName - Name of the persistent level being streamed to
	 */
	virtual void PreCommitMapChange(const FString& PreviousMapName, const FString& NextMapName);

	/** Called from CommitMapChange after unloading previous level and loading new level+sublevels. Used for asynchronous level streaming */
	virtual void PostCommitMapChange();

	/** Add PlayerState to the inactive list, remove from the active list */
	virtual void AddInactivePlayer(APlayerState* PlayerState, APlayerController* PC);

	/** Attempt to find and associate an inactive PlayerState with entering PC.  
	  * @Returns true if a PlayerState was found and associated with PC. */
	virtual bool FindInactivePlayer(APlayerController* PC);

	/** Override PC's PlayerState with the values in OldPlayerState. */
	virtual void OverridePlayerState(APlayerController* PC, APlayerState* OldPlayerState);

	/** 
	 * called on server during seamless level transitions to get the list of Actors that should be moved into the new level
	 * PlayerControllers, Role < ROLE_Authority Actors, and any non-Actors that are inside an Actor that is in the list
	 * (i.e. Object.Outer == Actor in the list)
	 * are all automatically moved regardless of whether they're included here
	 * only dynamic actors in the PersistentLevel may be moved (this includes all actors spawned during gameplay)
	 * this is called for both parts of the transition because actors might change while in the middle (e.g. players might join or leave the game)
	 * @see also PlayerController::GetSeamlessTravelActorList() (the function that's called on clients)
	 * @param bToTransition true if we are going from old level to transition map, false if we are going from transition map to new level
	 * @param ActorList (out) list of actors to maintain
	 */
	virtual void GetSeamlessTravelActorList(bool bToTransition, TArray<AActor*>& ActorList);

	/** Allow the game to specify a place for clients to download MapName */
	DEPRECATED(4.11, "Override GameWelcomePlayer() instead")
	virtual FString GetRedirectURL(const FString& MapName) const;

	/** allows game to send network messages to provide more information to the client joining the game via NMT_GameSpecific
	 * (for example required DLC)
	 * the out string RedirectURL is built in and send automatically if only a simple URL is needed
	 */
	virtual void GameWelcomePlayer(UNetConnection* Connection, FString& RedirectURL);

	/** 
	 * used to swap a viewport/connection's PlayerControllers when seamless traveling and the new GameMode's
	 * controller class is different than the previous
	 * includes network handling
	 * @param OldPC - the old PC that should be discarded
	 * @param NewPC - the new PC that should be used for the player
	 */
	virtual void SwapPlayerControllers(APlayerController* OldPC, APlayerController* NewPC);

	UFUNCTION(BlueprintImplementableEvent, Category="Game", meta=(DisplayName="OnSwapPlayerControllers"))
	void K2_OnSwapPlayerControllers(APlayerController* OldPC, APlayerController* NewPC);

	/** 
	 * Handles reinitializing players that remained through a seamless level transition
	 * called from C++ for players that finished loading after the server
	 * @param C the Controller to handle
	 */
	virtual void HandleSeamlessTravelPlayer(AController*& C);

	/** SetViewTarget of player control on server change */
	virtual void SetSeamlessTravelViewTarget(APlayerController* PC);

	/** Called when this PC is in cinematic mode, and its matinee is canceled by the user. */
	virtual void MatineeCancelled();

	/** Does end of game handling for the online layer */
	virtual void RestartPlayer(class AController* NewPlayer);

	UFUNCTION(BlueprintImplementableEvent, Category="Game", meta=(DisplayName="OnRestartPlayer"))
	void K2_OnRestartPlayer(AController* NewPlayer);

	/** Given a string, return a fully-qualified GameMode class name */
	static FString StaticGetFullGameClassName(FString const& Str);

	/** Returns true if replays will start/stop during gameplay starting/stopping */
	virtual bool IsHandlingReplays();

protected:

	/**
	 * Customize incoming player based on URL options
	 *
	 * @param NewPlayerController player logging in
	 * @param UniqueId unique id for this player
	 * @param Options URL options that came at login
	 *
	 */
	virtual FString InitNewPlayer(class APlayerController* NewPlayerController, const TSharedPtr<const FUniqueNetId>& UniqueId, const FString& Options, const FString& Portal = TEXT(""));
	virtual FString InitNewPlayer(class APlayerController* NewPlayerController, const FUniqueNetIdRepl& UniqueId, const FString& Options, const FString& Portal = TEXT(""));

private:
	// Hidden functions that don't make sense to use on this class.
	HIDE_ACTOR_TRANSFORM_FUNCTIONS();
};

/** GameMode events, particularly for use by plugins */
class ENGINE_API FGameModeEvents
{
public:
	// Post login event, triggered when a player joins the game as well as after non-seamless ServerTravel
	DECLARE_EVENT_TwoParams(AGameMode, FGameModePostLoginEvent, AGameMode* /*GameMode*/, APlayerController* /*NewPlayer*/);

	/**
	* Logout event, triggered when a player leaves the game as well as during non-seamless ServerTravel
	*
	* Note that this is called before performing any cleanup of the specified AController
	*/
	DECLARE_EVENT_TwoParams(AGameMode, FGameModeLogoutEvent, AGameMode* /*GameMode*/, AController* /*Exiting*/);
	

	static FGameModePostLoginEvent GameModePostLoginEvent;
	static FGameModeLogoutEvent GameModeLogoutEvent;
};

