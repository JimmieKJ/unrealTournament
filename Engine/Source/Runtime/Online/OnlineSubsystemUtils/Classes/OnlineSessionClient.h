// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/**
 * Everything a local player will use to manage online sessions.
 */

#pragma once
#include "OnlineSessionInterface.h"
#include "OnlineSessionClient.generated.h"

UCLASS(config=Game)
class ONLINESUBSYSTEMUTILS_API UOnlineSessionClient : public UOnlineSession
{
	GENERATED_UCLASS_BODY()

protected:

	/** Reference to the online sessions interface */
	IOnlineSessionPtr SessionInt;

	/** Delegate when an invite has been accepted from an external source */
	FOnSessionInviteAcceptedDelegate OnSessionInviteAcceptedDelegate;
	/** Delegate for destroying a session after previously ending it */
	FOnEndSessionCompleteDelegate OnEndForJoinSessionCompleteDelegate;
	/** Delegate for joining a new session after previously destroying it */
	FOnDestroySessionCompleteDelegate OnDestroyForJoinSessionCompleteDelegate;
	/** Delegate for returning to main menu after cleaning up */
	FOnDestroySessionCompleteDelegate OnDestroyForMainMenuCompleteDelegate;
	/** Delegate after joining a session */
	FOnJoinSessionCompleteDelegate OnJoinSessionCompleteDelegate;

	/** Cached invite/search result while in the process of tearing down an existing session */
	FOnlineSessionSearchResult CachedSessionResult;
	/** Is this join from an invite */
	UPROPERTY(Transient)
	bool bIsFromInvite;
	/** Have we started returning to main menu already */
	UPROPERTY(Transient)
	bool bHandlingDisconnect;

	/**
	 * Get the player controller associated with this session (from the owning ULocalPlayer)
	 */
	class APlayerController* GetPlayerController();

	/**
	 * Helper function to retrieve the controller id of the owning controller
	 *
	 * @return controller id of the controller
	 */
	int32 GetControllerId();

	/**
	 * Chance for the session client to handle the disconnect
     *
     * @param World world involved in disconnect (possibly NULL for pending travel)
     * @param NetDriver netdriver involved in disconnect
     *
     * @return true if disconnect was handled, false for general engine handling
	 */
	virtual bool HandleDisconnectInternal(UWorld* World, UNetDriver* NetDriver);

	/**
	 * Transition from ending a session to destroying a session
	 *
	 * @param SessionName session that just ended
	 * @param bWasSuccessful was the end session attempt successful
	 */
	void OnEndForJoinSessionComplete(FName SessionName, bool bWasSuccessful);

	/**
	 * Ends an existing session of a given name
	 *
	 * @param SessionName name of session to end
	 * @param Delegate delegate to call at session end
	 */
	void EndExistingSession(FName SessionName, FOnEndSessionCompleteDelegate& Delegate);

	/**
	 * Transition from destroying a session to joining a new one of the same name
	 *
  	 * @param SessionName name of session recently destroyed
 	 * @param bWasSuccessful was the destroy attempt successful
	 */
	void OnDestroyForJoinSessionComplete(FName SessionName, bool bWasSuccessful);

	/**
	 * Transition from destroying a session to returning to the main menu
	 *
  	 * @param SessionName name of session recently destroyed
 	 * @param bWasSuccessful was the destroy attempt successful
	 */
	void OnDestroyForMainMenuComplete(FName SessionName, bool bWasSuccessful);
	
	/**
	 * Destroys an existing session of a given name
	 *
     * @param SessionName name of session to destroy
	 * @param Delegate delegate to call at session destruction
	 */
	void DestroyExistingSession(FName SessionName, FOnDestroySessionCompleteDelegate& Delegate);

	/**
	 * Delegate fired when the joining process for an online session has completed
	 *
	 * @param SessionName the name of the session this callback is for
	 * @param bWasSuccessful true if the async action completed without error, false if there was an error
	 */
	void OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);

	/**
	 * Join a session of a given name after potentially tearing down an existing one
	 *
	 * @param LocalUserNum local user id
	 * @param SessionName name of session to join
	 * @param SearchResult the session to join
	 */
	void JoinSession(int32 LocalUserNum, FName SessionName, const FOnlineSessionSearchResult& SearchResult);

	/**
	 * Delegate fired when an invite request has been accepted (via external UI)
	 *
	 * @param LocalUserNum local user accepting invite
	 * @param bWasSuccessful true if the async action completed without error, false if there was an error
	 * @param SearchResult search result containing the invite data
	 */
	virtual void OnSessionInviteAccepted(int32 LocalUserNum, bool bWasSuccessful, const class FOnlineSessionSearchResult& SearchResult);

public:

	/**
	 * Register all delegates needed to manage online sessions
	 */
	virtual void RegisterOnlineDelegates(class UWorld* InWorld) override;

	/**
	 * Tear down all delegates used to manage online sessions
	 */
	virtual void ClearOnlineDelegates(class UWorld* InWorld) override;

	/**
	 * Called to tear down any online sessions and return to main menu
	 */
	void HandleDisconnect(UWorld *World, UNetDriver *NetDriver) override;

};



