// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTBasePlayerController.generated.h"

UCLASS()
class UNREALTOURNAMENT_API AUTBasePlayerController : public APlayerController , public IUTTeamInterface
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	AUTPlayerState* UTPlayerState;

	virtual void SetupInputComponent() override;

	virtual void Destroyed() override;

	/**	Will popup the in-game menu	 **/
	UFUNCTION(exec)
	virtual void ShowMenu();

	UFUNCTION(exec)
	virtual void HideMenu();

	void InitPlayerState();

	UFUNCTION()
	virtual void Talk();

	UFUNCTION()
	virtual void TeamTalk();

	UFUNCTION(exec)
	virtual void Say(FString Message);

	UFUNCTION(exec)
	virtual void TeamSay(FString Message);

	UFUNCTION(reliable, server, WithValidation)
	virtual void ServerSay(const FString& Message, bool bTeamMessage);

	UFUNCTION(reliable, client)
	virtual void ClientSay(class AUTPlayerState* Speaker, const FString& Message, FName Destination);

	virtual uint8 GetTeamNum() const;
	// not applicable
	virtual void SetTeamForSideSwap_Implementation(uint8 NewTeamNum) override
	{}

#if !UE_SERVER
	virtual void ShowMessage(FText MessageTitle, FText MessageText, uint16 Buttons, const FDialogResultDelegate& Callback = FDialogResultDelegate());
#endif

	// A quick function so I don't have to keep adding one when I want to test something.  @REMOVEME: Before the final version
	UFUNCTION(exec)
	virtual void DebugTest(FString TestCommand);

	UFUNCTION(Server, Reliable, WithValidation)
	virtual void ServerDebugTest(const FString& TestCommand);


public:
	/**
	 *	User a GUID to find a server via the MCP and connect to it.
	 **/
	virtual void ConnectToServerViaGUID(FString ServerGUID, bool bSpectate=false, bool bFindLastMatch=false);

	UFUNCTION(Client, Reliable)
	virtual void ClientReturnToLobby();

	UFUNCTION()
	virtual void ClientReturnedToMenus();

	// Allows the game to cause the client to set it's presence.
	UFUNCTION(Client, reliable)
	virtual void ClientSetPresence(const FString& NewPresenceString, bool bAllowInvites, bool bAllowJoinInProgress, bool bAllowJoinViaPresence, bool bAllowJoinViaPresenceFriendsOnly, bool bIsInstance);

	UFUNCTION(client, reliable)
	virtual void ClientGenericInitialization();

	UFUNCTION(server, reliable, WithValidation)
	virtual void ServerRecieveAverageRank(int32 NewAverageRank);

	UFUNCTION(client, reliable)
	virtual void ClientRequireContentItemListBegin(const FString& CloudId);

	UFUNCTION(client, reliable)
	virtual void ClientRequireContentItem(const FString& PakFile, const FString& MD5);

	UFUNCTION(client, reliable)
	virtual void ClientRequireContentItemListComplete();

protected:
	FOnFindSessionsCompleteDelegate OnFindGUIDSessionCompleteDelegate;
	FDelegateHandle OnFindGUIDSessionCompleteDelegateHandle;
	
	FOnCancelFindSessionsCompleteDelegate OnCancelGUIDFindSessionCompleteDelegate;
	FDelegateHandle OnCancelGUIDFindSessionCompleteDelegateHandle;

	TSharedPtr<class FUTOnlineGameSearchBase> GUIDSessionSearchSettings;

	FString GUIDJoin_CurrentGUID;
	bool GUIDJoinWantsToSpectate;
	int GUIDJoinAttemptCount;
	bool GUIDJoinWantsToFindMatch;

	void AttemptGUIDJoin();
	void OnFindSessionsComplete(bool bWasSuccessful);
	void OnCancelGUIDFindSessionComplete(bool bWasSuccessful);

public:
	UFUNCTION(Exec)
	virtual void RconAuth(FString Password);

	UFUNCTION(Server, Reliable, WithValidation)
	virtual void ServerRconAuth(const FString& Password);

	UFUNCTION(Exec)
	virtual void RconExec(FString Command);

	UFUNCTION(Server, Reliable, WithValidation)
	virtual void ServerRconExec(const FString& Command);

	UFUNCTION(Exec)
	virtual void RconKick(FString NameOrUIDStr, bool bBan = false, FString Reason = TEXT(""));

	UFUNCTION(Server, Reliable, WithValidation)
	virtual void ServerRconKick(const FString& NameOrUIDStr, bool bBan, const FString& Reason);
};