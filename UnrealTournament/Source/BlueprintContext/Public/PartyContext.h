// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "BlueprintContextTypes.h"
#include "BlueprintContextBase.h"
#include "Online/UTPartyGameState.h"
#include "Online/UTPartyMemberState.h"

class IGameAndPartyService;
class IChatNotificationService;
class FFriendsAndChatMessage;

// NOTE: Defining this enum prior to the generated header since the generated file contains
//		static delegates that depend on it being defined

/** Describes the player's current party transition */
UENUM(BlueprintType)
enum class EUTPartyTransition : uint8
{
	// No transition
	Idle,
	// Joining a new party
	Joining,
	// Leaving a party
	Leaving,
	Max UMETA(Hidden)
};

#include "PartyContext.generated.h"

/**
 * The PartyContext object is an Epic Ecosystem representation for your XMPP party.
 *
 * this should not be confused with your team, which is a gameplay concept of people you are playing with
 */
UCLASS()
class BLUEPRINTCONTEXT_API UPartyContext : public UBlueprintContextBase
{
	GENERATED_BODY()

	UPartyContext();

	virtual void Initialize() override;
	virtual void PreDestructContext(UWorld* World) override;

	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPartyChanged);
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPartyStateChanged, EPartyMemberState, NewState);
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPartyTypeChanged, EPartyType, PartyType);
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPartyTransitionStartedDelegate, EUTPartyTransition, PartyTransition);
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPartyTransitionCompleteDelegate, EUTPartyTransition, PartyTransition);
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnClientPartyStateChanged, EUTPartyState, PartyState);
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPartyDataChanged, const FPartyState&, PartyData);
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLeaderFriendsOnlyChanged, bool, bLeaderFriendsOnly);
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLeaderInvitesOnlyChanged, bool, bLeaderInviteOnly);

protected:
	virtual void PostInitWorld(UWorld* World) override;
	virtual void OnWorldCleanup(UWorld* World) override;

	void HandlePlayerLoggedIn();
	void HandlePlayerLoggedOut();
	
public:
	UFUNCTION( BlueprintCallable, Category = PartyContext )
	int32 NumPartyMembers() const;

	UFUNCTION( BlueprintCallable, Category = PartyContext )
	int32 MaxPartyMembers() const;

	/** called when a player joins / leaves your party */
	UPROPERTY( BlueprintAssignable, Category = PartyContext )
	FOnPartyChanged OnPartyChanged;

	/** Called when you join / leave a party */
	UPROPERTY( BlueprintAssignable, Category = PartyContext )
	FOnPartyStateChanged OnPartyStateChanged;
	
	/** Delegate fired when a party type changes */
	UPROPERTY(BlueprintAssignable, Category = PartyContext)
	FOnPartyTypeChanged OnPartyTypeChanged;
	
	/** Called when a player starts joining or leaving a party */
	UPROPERTY(BlueprintAssignable, Category=PartyContext)
	FOnPartyTransitionStartedDelegate OnPartyTransitionStarted;

	/** Called when a player has completed joining or leaving a party */
	UPROPERTY(BlueprintAssignable, Category=PartyContext)
	FOnPartyTransitionCompleteDelegate OnPartyTransitionCompleted;
	
	/** Called when the clients party state changes (Only called on the client (aka non-leader) currently) */
	UPROPERTY(BlueprintAssignable, Category = PartyContext)
	FOnClientPartyStateChanged OnClientPartyStateChanged;
	
	/** Delegate fired when a join via presence permissions change */
	UPROPERTY(BlueprintAssignable, Category = PartyContext)
	FOnLeaderFriendsOnlyChanged OnLeaderFriendsOnlyChanged;

	/**  Delegate fired when a invite permissions change */
	UPROPERTY(BlueprintAssignable, Category = PartyContext)
	FOnLeaderInvitesOnlyChanged OnLeaderInvitesOnlyChanged;

	/** Called when party data got updated */
	UPROPERTY( BlueprintAssignable, Category = PartyContext )
	FOnPartyDataChanged OnPartyDataChanged;

protected:
	/**
	 * Notification that the local players have joined a party,
	 * register with party state delegates
	 *
	 * @param PartyState party that has been joined
	 */
	void OnPartyJoined( UPartyGameState* PartyState );

	/**
	 * Notification that the local players have left a party,
	 * unregister with party state delegates and cancels matchmaking
	 *
	 * @param PartyState party that has been left
	 * @param Reason reason why the players left
	 */
	void OnPartyLeft( UPartyGameState* PartyState, EMemberExitedReason Reason );

	/**
	 * Notification that the party has returned to the frontend and needs to be reset
	 * may be the previous party or a new party if the user was removed on network error, etc
	 *
	 * @param PersistentParty party that is being reset in the frontend
	 */
	void OnPartyResetForFrontend( UPartyGameState* PersistentParty );

	/**
	 * Notification that a new party member has joined a party
	 *
	 * @param PartyState state of the party being joined
	 * @param MemberId UniqueId of the new party member
	 */
	void OnPartyMemberJoined( UPartyGameState* PartyState, const FUniqueNetIdRepl& MemberId );

	/**
	 * Notification that a party member has been promoted
	 * Cancel matchmaking under these circumstances
	 *
	 * @param PartyState party that has been affected
	 * @param MemberId party member that has been promoted
	 */
	void OnPartyMemberPromoted( UPartyGameState* PartyState, const FUniqueNetIdRepl& MemberId );

	/**
	 * Notification that a party member has left
	 *
	 * @param PartyState party that has been affected
	 * @param MemberId party member that left
	 * @param ExitReason reason why the player left
	 */
	void OnPartyMemberLeft( UPartyGameState* PartyState, const FUniqueNetIdRepl& MemberId, EMemberExitedReason ExitReason );

	/**
	 * Notification that a party member has begun to leave
	 * Allows UI to handle both before and after the leaving has occurred
	 *
	 * @param PartyState party that has been affected
	 * @param MemberId party member that left
	 * @param ExitReason reason why the player left
	 */
	void OnPartyMemberLeaving( UPartyGameState* PartyState, const FUniqueNetIdRepl& MemberId, EMemberExitedReason ExitReason );

	/**
	 * Notification from the friends and chat manager that the JOIN GAME button has been pressed
	 *
	 * @param SenderId id of the user to be joined (or sent the invite)
	 * @param PartyJoinInfo relevant information for joining a given party
	 * @param bIsFromInvite did this call originate from an invite or user join action
	 */
	void OnFriendsJoinParty( const FUniqueNetId& SenderId, const TSharedRef<IOnlinePartyJoinInfo>& PartyJoinInfo, bool bIsFromInvite );

	/**
	* Notification from the friends and chat manager that a game invite request wants to be sent
	*
	* @param SenderId id of the user sending the invite
	* @param ReceiverId id of the user to invite
	*/
	void OnSendGameInvitationRequested( const FUniqueNetId& SenderId, const FUniqueNetId& ReceiverId );

	/**
	 * Notification that the join party async operation has completed
	 *
	 * @param LocalUserId id of the user initiating the request
	 * @param Result outcome of the join attempt
	 * @param DeniedResultCode game specific failure reason (only valid in error cases)
	 */
	void OnJoinPartyComplete( const FUniqueNetId& LocalUserId, EJoinPartyCompletionResult Result, int32 DeniedResultCode );

	/**
	 * Notification that a party join initiated in the UT Party has completed
	 * @param LocalUserId id of the user initiating the request
	 * @param Result outcome of the join attempt
	 * @param DeniedResultCode game specific failure reason (only valid in error cases)
	 */
	void OnUTJoinPartyComplete( const FUniqueNetId& LocalUserId, EJoinPartyCompletionResult Result, int32 DeniedResultCode );

	/**
	 * Internal join party handler
	 *
	 * @param Result outcome of the join attempt
	 * @param DeniedResultCode game specific failure reason (only valid in error cases)
	 */
	void JoinPartyCompleteInternal( EJoinPartyCompletionResult Result, int32 DeniedResultCode );
	
private:
	
	TWeakObjectPtr<UPartyGameState> GetActivePartyState() const;

	//Handlers for when party data got updated
	void HandlePartyDataChanged(const FPartyState& PartyData);

	/** Handler for when the state of the party changes */
	void HandleClientPartyStateChanged(EUTPartyState PartyState);

	//Handles for when client party type is changed
	void HandlePartyTypeChanged(EPartyType PartyType);

	/** Handler for when the state of a member of the local player's party changes */
	void HandlePartyMemberStateChanged(const FUniqueNetIdRepl& UniqueId, const UPartyMemberState* PartyMember);

	//Handles for when join via presence permissions change
	void HandleLeaderFriendsOnlyChanged(bool bLeaderFriendsOnly);

	//Handles for when party invite permissions change
	void HandleLeaderInvitesOnlyChanged(bool bLeaderInviteOnly);

	/**
	 * Bind all online related delegates to the party context
	 *
	 * @param World relevant world for binding
	 */
	void BindOnlineDelegates( UWorld* World );

	/**
	 * Unbind all online related delegates to the party context
	 *
	 * @param World relevant world for unbinding
	 */
	void UnbindOnlineDelegates( UWorld* World );

	UUTPartyMemberState* GetMyPersistentPartyEntry() const;

	void OnPresenceReceivedInternal(const FUniqueNetId& UserId, const TSharedRef<FOnlineUserPresence>& NewPresence);
	void OnFriendsListSendPartyInviteComplete(const FUniqueNetId& LocalUserId, const FOnlinePartyId& PartyId, const FUniqueNetId& RecipientId, ESendPartyInvitationCompletionResult Result);
	void HandleFriendsActionNotification(TSharedRef<FFriendsAndChatMessage> FriendsNotification);
	void OnPartyInvitesChangedInternal(const FUniqueNetId& LocalUserId);
	
	// The player's current party transition
	EUTPartyTransition CurrentTransitionState;

	IOnlineUserPtr UserInfoInterface;
	IOnlinePartyPtr PartyInterface;
	TWeakObjectPtr<UParty> Party;

	TWeakPtr<IGameAndPartyService> GameAndPartyService;
	TWeakPtr<IChatNotificationService> NotificationService;

	TArray<TWeakObjectPtr<UPartyGameState>> PartyStack;

	FDelegateHandle PartyInviteResponseReceivedDelegateHandle;
	FDelegateHandle ParyInvitesUpdatedHandle;
	FDelegateHandle PresenceReceivedDelegateHandle;

	// Called when an invitee has made a decision about a sent invite
	void HandlePartyInviteResponseReceived(const FUniqueNetId& LocalUserId, const FOnlinePartyId& PartyId, const FUniqueNetId& SenderId, const EInvitationResponse Response);

	FOnPartyInviteResponseReceivedDelegate OnPartyInviteResponseReceivedDelegate;

	bool bIsLocalPlayerPartyLeader;
};