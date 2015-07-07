// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTBasePlayerController.generated.h"

class UUTGameViewportClient;

UENUM(BlueprintType)
namespace EInputMode
{
	enum Type
	{
		EIM_None,
		EIM_GameOnly,
		EIM_GameAndUI,
		EIM_UIOnly,
	};
}

UCLASS()
class UNREALTOURNAMENT_API AUTBasePlayerController : public APlayerController , public IUTTeamInterface
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	AUTPlayerState* UTPlayerState;

	virtual void SetupInputComponent() override;

	virtual void Destroyed() override;

	virtual void InitInputSystem() override;

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
	 *	User a GUID to find a server via the MCP and connect to it.  NOTE.. DesiredTeam = 0, 1, 255 or -1 for don't set the team
	 **/
	virtual void ConnectToServerViaGUID(FString ServerGUID, int32 DesiredTeam, bool bSpectate=false, bool bFindLastMatch=false);

	/**
	 *	Used by the hub system to cancel a pending connect if the player is downloading content.  Used for aborting.
	 **/

	virtual void CancelConnectViaGUID();

	UFUNCTION(Client, Reliable)
	virtual void ClientReturnToLobby();

	UFUNCTION()
	virtual void ClientReturnedToMenus();

	// Allows the game to cause the client to set it's presence.
	UFUNCTION(Client, reliable)
	virtual void ClientSetPresence(const FString& NewPresenceString, bool bAllowInvites, bool bAllowJoinInProgress, bool bAllowJoinViaPresence, bool bAllowJoinViaPresenceFriendsOnly);

	UFUNCTION(client, reliable)
	virtual void ClientGenericInitialization();

	UFUNCTION(server, reliable, WithValidation)
	virtual void ServerReceiveAverageRank(int32 NewAverageRank);

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
	int32 GUIDJoinAttemptCount;
	bool GUIDJoinWantsToFindMatch;
	int32 GUIDJoinDesiredTeam;

	void StartGUIDJoin();
	void AttemptGUIDJoin();
	void OnFindSessionsComplete(bool bWasSuccessful);
	void OnCancelGUIDFindSessionComplete(bool bWasSuccessful);

	FContentDownloadComplete OnDownloadComleteDelgate;
	FDelegateHandle OnDownloadCompleteDelegateHandle;
	virtual void OnDownloadComplete(class UUTGameViewportClient* ViewportClient, ERedirectStatus::Type RedirectStatus, const FString& PackageName);

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

	// Let the game's player controller know there was a network failure message.
	virtual void HandleNetworkFailureMessage(enum ENetworkFailure::Type FailureType, const FString& ErrorString);

	/**Check to see if this PC can chat. Called on Client and server independantly*/
	bool AllowTextMessage(const FString& Msg);

	/**The accumulation of time added per message. Once overflowed the player must wait for this to return to 0*/
	float ChatOverflowTime;
	bool bOverflowed;
	FText SpamText;
	FString LastChatMessage;


	UFUNCTION(Client, Reliable)
	virtual void ClientCloseAllUI();

	virtual void PreClientTravel(const FString& PendingURL, ETravelType TravelType, bool bIsSeamlessTravel);


	/**This is overridden to avoid the Slate focus issues occuring with each widget managing their own input mode.
	Instead of setting this manually, we will update the input mode based on the state of the game in UpdateInputMode()*/
	virtual void SetInputMode(const FInputModeDataBase& InData) override {}

#if !UE_SERVER
	virtual void Tick(float DeltaTime) override;
	virtual void UpdateInputMode();

	UPROPERTY()
	TEnumAsByte<EInputMode::Type> InputMode;
#endif


};