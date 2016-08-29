// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EngineBaseTypes.h"
#include "GameInstance.generated.h"

class ULocalPlayer;
class FUniqueNetId;
struct FLatentActionManager;

// 
// 	EWelcomeScreen, 	//initial screen.  Used for platforms where we may not have a signed in user yet.
// 	EMessageScreen, 	//message screen.  Used to display a message - EG unable to connect to game.
// 	EMainMenu,		//Main frontend state of the game.  No gameplay, just user/session management and UI.	
// 	EPlaying,		//Game should be playable, or loading into a playable state.
// 	EShutdown,		//Game is shutting down.
// 	EUnknown,		//unknown state. mostly for some initializing game-logic objects.

/** Possible state of the current match, where a match is all the gameplay that happens on a single map */
namespace GameInstanceState
{
	extern ENGINE_API const FName Playing;			// We are playing the game
}

class FOnlineSessionSearchResult;

/**
 * Notification that the client is about to travel to a new URL
 *
 * @param PendingURL the travel URL
 * @param TravelType type of travel that will occur (absolute, relative, etc)
 * @param bIsSeamlessTravel is traveling seamlessly
 */
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnPreClientTravel, const FString& /*PendingURL*/, ETravelType /*TravelType*/, bool /*bIsSeamlessTravel*/);
typedef FOnPreClientTravel::FDelegate FOnPreClientTravelDelegate;

/**
 * GameInstance: high-level manager object for an instance of the running game.
 * Spawned at game creation and not destroyed until game instance is shut down.
 * Running as a standalone game, there will be one of these.
 * Running in PIE (play-in-editor) will generate one of these per PIE instance.
 */

UCLASS(config=Game, transient, BlueprintType, Blueprintable)
class ENGINE_API UGameInstance : public UObject, public FExec
{
	GENERATED_UCLASS_BODY()

protected:
	struct FWorldContext* WorldContext;

	// @todo jcf list of logged-in players?

	virtual bool HandleOpenCommand(const TCHAR* Cmd, FOutputDevice& Ar, UWorld* InWorld);

	UPROPERTY()
	TArray<ULocalPlayer*> LocalPlayers;		// List of locally participating players in this game instance
	
	/** Class to manage online services */
	UPROPERTY()
	class UOnlineSession* OnlineSession;

	/** Listeners to PreClientTravel call */
	FOnPreClientTravel NotifyPreClientTravelDelegates;

public:

	FString PIEMapName;

	//~ Begin FExec Interface
	virtual bool Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Out = *GLog) override;
	//~ End FExec Interface

	//~ Begin UObject Interface
	virtual class UWorld* GetWorld() const override;
	virtual void FinishDestroy() override;
	//~ End UObject Interface

	/** virtual function to allow custom GameInstances an opportunity to set up what it needs */
	virtual void Init();

	/** Opportunity for blueprints to handle the game instance being initialized. */
	UFUNCTION(BlueprintImplementableEvent, meta=(DisplayName = "Init"))
	void ReceiveInit();

	/** virtual function to allow custom GameInstances an opportunity to do cleanup when shutting down */
	virtual void Shutdown();

	/** Opportunity for blueprints to handle the game instance being shutdown. */
	UFUNCTION(BlueprintImplementableEvent, meta=(DisplayName = "Shutdown"))
	void ReceiveShutdown();

	/** Opportunity for blueprints to handle network errors. */
	UFUNCTION(BlueprintImplementableEvent, meta=(DisplayName = "NetworkError"))
	void HandleNetworkError(ENetworkFailure::Type FailureType, bool bIsServer);

	/** Opportunity for blueprints to handle travel errors. */
	UFUNCTION(BlueprintImplementableEvent, meta=(DisplayName = "TravelError"))
	void HandleTravelError(ETravelFailure::Type FailureType);

	/* Called to initialize the game instance for standalone instances of the game */
	void InitializeStandalone();
#if WITH_EDITOR
	/* Called to initialize the game instance for PIE instances of the game */
	virtual bool InitializePIE(bool bAnyBlueprintErrors, int32 PIEInstance, bool bRunAsDedicated);

	virtual bool StartPIEGameInstance(ULocalPlayer* LocalPlayer, bool bInSimulateInEditor, bool bAnyBlueprintErrors, bool bStartInSpectatorMode);

#endif

	class UEngine* GetEngine() const;

	struct FWorldContext* GetWorldContext() const { return WorldContext; };
	class UGameViewportClient* GetGameViewportClient() const;

	/** Starts the GameInstance state machine running */
	virtual void StartGameInstance();
	virtual bool JoinSession(ULocalPlayer* LocalPlayer, int32 SessionIndexInSearchResults) { return false; }
	virtual bool JoinSession(ULocalPlayer* LocalPlayer, const FOnlineSessionSearchResult& SearchResult) { return false; }

	/** Local player access */

	/**
	 * Debug console command to create a player.
	 * @param ControllerId - The controller ID the player should accept input from.
	 */
	UFUNCTION(exec)
	virtual void			DebugCreatePlayer(int32 ControllerId);

	/**
	 * Debug console command to remove the player with a given controller ID.
	 * @param ControllerId - The controller ID to search for.
	 */
	UFUNCTION(exec)
	virtual void			DebugRemovePlayer(int32 ControllerId);

	virtual ULocalPlayer*	CreateInitialPlayer(FString& OutError);

	/**
	 * Adds a new player.
	 * @param ControllerId - The controller ID the player should accept input from.
	 * @param OutError - If no player is returned, OutError will contain a string describing the reason.
	 * @param SpawnActor - True if an actor should be spawned for the new player.
	 * @return The player which was created.
	 */
	ULocalPlayer*			CreateLocalPlayer(int32 ControllerId, FString& OutError, bool bSpawnActor);

	/**
	 * Adds a LocalPlayer to the local and global list of Players.
	 *
	 * @param	NewPlayer	the player to add
	 * @param	ControllerId id of the controller associated with the player
	 */
	int32					AddLocalPlayer(ULocalPlayer * NewPlayer, int32 ControllerId);


	/**
	 * Removes a player.
	 * @param Player - The player to remove.
	 * @return whether the player was successfully removed. Removal is not allowed while connected to a server.
	 */
	bool					RemoveLocalPlayer(ULocalPlayer * ExistingPlayer);

	int32					GetNumLocalPlayers() const;
	ULocalPlayer*			GetLocalPlayerByIndex(const int32 Index) const;
	APlayerController*		GetFirstLocalPlayerController(UWorld* World = nullptr) const;
	ULocalPlayer*			FindLocalPlayerFromControllerId(const int32 ControllerId) const;
	ULocalPlayer*			FindLocalPlayerFromUniqueNetId(TSharedPtr<const FUniqueNetId> UniqueNetId) const;
	ULocalPlayer*			FindLocalPlayerFromUniqueNetId(const FUniqueNetId& UniqueNetId) const;
	ULocalPlayer*			GetFirstGamePlayer() const;

	TArray<ULocalPlayer*>::TConstIterator	GetLocalPlayerIterator() const;
	const TArray<ULocalPlayer*> &			GetLocalPlayers() const;
	/**
	 * Get the primary player controller on this machine (others are splitscreen children)
	 * (must have valid player state and unique id)
	 *
	 * @return the primary controller on this machine
	 */
	APlayerController* GetPrimaryPlayerController() const;

	/**
	 * Get the unique id for the primary player on this machine (others are splitscreen children)
	 *
	 * @return the unique id of the primary player on this machine
	 */
	TSharedPtr<const FUniqueNetId> GetPrimaryPlayerUniqueId() const;

	void CleanupGameViewport();

	/** Called when demo playback fails for any reason */
	virtual void HandleDemoPlaybackFailure( EDemoPlayFailure::Type FailureType, const FString& ErrorString = TEXT("") ) { }

	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);

	inline FTimerManager& GetTimerManager() const { return *TimerManager; }

	inline FLatentActionManager& GetLatentActionManager() const { return *LatentActionManager;  }

	/**
	 * Start recording a replay with the given custom name and friendly name.
	 *
	 * @param InName If not empty, the unique name to use as an identifier for the replay. If empty, a name will be automatically generated by the replay streamer implementation.
	 * @param FriendlyName An optional (may be empty) descriptive name for the replay. Does not have to be unique.
	 * @param AdditionalOptions Additional URL options to append to the URL that will be processed by the replay net driver. Will usually remain empty.
	 */
	virtual void StartRecordingReplay(const FString& InName, const FString& FriendlyName, const TArray<FString>& AdditionalOptions = TArray<FString>());

	/** Stop recording a replay if one is currently in progress */
	virtual void StopRecordingReplay();

	/** Start playing back a previously recorded replay. */
	virtual void PlayReplay(const FString& InName, UWorld* WorldOverride = nullptr, const TArray<FString>& AdditionalOptions = TArray<FString>());

	/**
	 * Adds a join-in-progress user to the set of users associated with the currently recording replay (if any)
	 *
	 * @param UserString a string that uniquely identifies the user, usually his or her FUniqueNetId
	 */
	virtual void AddUserToReplay(const FString& UserString);

	/** handle a game specific net control message (NMT_GameSpecific)
	 * this allows games to insert their own logic into the control channel
	 * the meaning of both data parameters is game-specific
	 */
	virtual void HandleGameNetControlMessage(class UNetConnection* Connection, uint8 MessageByte, const FString& MessageStr)
	{}
	
	/** return true to delay an otherwise ready-to-join PendingNetGame performing LoadMap() and finishing up
	 * useful to wait for content downloads, etc
	 */
	virtual bool DelayPendingNetGameTravel()
	{
		return false;
	}

	FTimerManager* TimerManager;
	FLatentActionManager* LatentActionManager;

	/** @return online session management object associated with this game instance */
	class UOnlineSession* GetOnlineSession() const { return OnlineSession; }

	/** @return OnlineSession class to use for this game instance  */
	virtual TSubclassOf<UOnlineSession> GetOnlineSessionClass();

	/** Returns true if this instance is for a dedicated server world */
	bool IsDedicatedServerInstance() const;

	/** Broadcast a notification that travel is occurring */
	void NotifyPreClientTravel(const FString& PendingURL, ETravelType TravelType, bool bIsSeamlessTravel);
	/** @return delegate fired when client travel occurs */
	FOnPreClientTravel& OnNotifyPreClientTravel() { return NotifyPreClientTravelDelegates; }
};