// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EngineBaseTypes.h"
#include "GameInstance.generated.h"

class ULocalPlayer;
class FUniqueNetId;

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
	
	// Delegate handle that stores delegate for when an invite is accepted by a user
	FDelegateHandle OnSessionUserInviteAcceptedDelegateHandle;
	
public:

	FString PIEMapName;

	// Begin FExec Interface
	virtual bool Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Out = *GLog) override;
	// End FExec Interface

	// Begin UObject Interface
	virtual class UWorld* GetWorld() const override;
	virtual void FinishDestroy() override;
	// End UObject Interface

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
	bool InitializePIE(bool bAnyBlueprintErrors, int32 PIEInstance);

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
	class ULocalPlayer*		CreateLocalPlayer(int32 ControllerId, FString& OutError, bool bSpawnActor);

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
	APlayerController*		GetFirstLocalPlayerController() const;
	ULocalPlayer*			FindLocalPlayerFromControllerId(const int32 ControllerId) const;
	ULocalPlayer*			FindLocalPlayerFromUniqueNetId(TSharedPtr< FUniqueNetId > UniqueNetId) const;
	ULocalPlayer*			FindLocalPlayerFromUniqueNetId(const FUniqueNetId& UniqueNetId) const;
	ULocalPlayer*			GetFirstGamePlayer() const;

	void					CleanupGameViewport();

	TArray<class ULocalPlayer*>::TConstIterator	GetLocalPlayerIterator();
	const TArray<class ULocalPlayer*> &			GetLocalPlayers();

	/** Called when demo playback fails for any reason */
	virtual void			HandleDemoPlaybackFailure( EDemoPlayFailure::Type FailureType, const FString& ErrorString = TEXT("") ) { }

	static void				AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);

    /** Delegate that is called when a user has accepted an invite. */
	void HandleSessionUserInviteAccepted(const bool bWasSuccess, const int32 ControllerId, TSharedPtr< FUniqueNetId > UserId, const FOnlineSessionSearchResult & InviteResult);
	
	/** Overridable implementation of HandleSessionUserInviteAccepted, which does nothing but call this function */
	virtual void OnSessionUserInviteAccepted(const bool bWasSuccess, const int32 ControllerId, TSharedPtr< FUniqueNetId > UserId, const FOnlineSessionSearchResult & InviteResult);

	inline FTimerManager& GetTimerManager() const { return *TimerManager; }

	/** Start recording a replay with the given custom name and friendly name. */
	virtual void StartRecordingReplay(const FString& Name, const FString& FriendlyName);

	/** Stop recording a replay if one is currently in progress */
	virtual void StopRecordingReplay();

	/** Start playing back a previously recorded replay. */
	virtual void PlayReplay(const FString& Name);

private:
	FTimerManager* TimerManager;
};