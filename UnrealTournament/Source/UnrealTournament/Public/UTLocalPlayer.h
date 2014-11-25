// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "SlateBasics.h"
#include "Slate/SlateGameResources.h"
#include "Online.h"
#include "OnlineSubsystemTypes.h"
#include "../Private/Slate/SUWToast.h"
#include "../Private/Slate/SUWDialog.h"
#include "UTProfileSettings.h"
#include "UTLocalPlayer.generated.h"

class SUWServerBrowser;

DECLARE_DELEGATE_ThreeParams(FPlayerOnlineStatusChangedDelegate, class UUTLocalPlayer*, ELoginStatus::Type, const FUniqueNetId&);

class FStoredChatMessage
{
public:
	// Where did this chat come from
	UPROPERTY()
	FName Type;

	// What was the message
	UPROPERTY()
	FString Message;

	// What color should we display this chat in
	UPROPERTY()
	FLinearColor Color;

	FStoredChatMessage(FName inType, FString inMessage, FLinearColor inColor)
		: Type(inType), Message(inMessage), Color(inColor)
	{}

	static TSharedRef<FStoredChatMessage> Make(FName inType, FString inMessage, FLinearColor inColor)
	{
		return MakeShareable( new FStoredChatMessage( inType, inMessage, inColor ) );
	}

};


UCLASS(config=Engine)
class UUTLocalPlayer : public ULocalPlayer
{
	GENERATED_UCLASS_BODY()

public:
	virtual ~UUTLocalPlayer();

	virtual bool IsMenuGame();

	virtual FString GetNickname() const;
	virtual FText GetAccountSummary() const;
	virtual FText GetAccountDisplayName() const;

	virtual void PlayerAdded(class UGameViewportClient* InViewportClient, int32 InControllerID);

	virtual void ShowMenu();
	virtual void HideMenu();
	virtual void ShowToast(FText ToastText);	// NOTE: Need to add a type/etc so that they can be skinned better.

#if !UE_SERVER
	virtual TSharedPtr<class SUWDialog> ShowMessage(FText MessageTitle, FText MessageText, uint16 Buttons, const FDialogResultDelegate& Callback = FDialogResultDelegate(), FVector2D DialogSize = FVector2D(0.0,0.0f));

	/** utilities for opening and closing dialogs */
	virtual void OpenDialog(TSharedRef<class SUWDialog> Dialog);
	virtual void CloseDialog(TSharedRef<class SUWDialog> Dialog);
	TSharedPtr<class SUWServerBrowser> GetServerBrowser();

	TSharedPtr<class SUWindowsDesktop> GetCurrentMenu()
	{
		return DesktopSlateWidget;
	}
#endif

	// Holds all of the chat this client has recieved.
	TArray<TSharedPtr<FStoredChatMessage>> ChatArchive;
	virtual void SaveChat(FName Type, FString Message, FLinearColor Color);

protected:

#if !UE_SERVER
	TSharedPtr<class SUWindowsDesktop> DesktopSlateWidget;
	
	// Holds a persistent reference to the server browser.
	TSharedPtr<class SUWServerBrowser> ServerBrowserWidget;

	/** stores a reference to open dialogs so they don't get destroyed */
	TArray< TSharedPtr<class SUWDialog> > OpenDialogs;
	TArray<TSharedPtr<class SUWToast>> ToastList;

	virtual void AddToastToViewport(TSharedPtr<SUWToast> ToastToDisplay);
	void WelcomeDialogResult(TSharedPtr<SCompoundWidget> Widget, uint16 ButtonID);
#endif


public:
	FProcHandle DedicatedServerProcessHandle;

	// Last text entered in Connect To IP
	UPROPERTY(config)
	FString LastConnectToIP;

	UPROPERTY(config)
	uint32 bNoMidGameMenu:1;

// ONLINE ------

	// Called after creation on non-default objects to setup the online Subsystem
	virtual void InitializeOnlineSubsystem();

	// Returns true if this player is logged in to the UT Online Services
	virtual bool IsLoggedIn();

	/**
	 *	Login to the Epic online serivces. 
	 *
	 *  EpicID = the ID to login with.  Right now it's the's players UT forum id
	 *  Auth = is the auth-token or password to login with
	 *  bIsRememberToken = If true, Auth will be considered a remember me token
	 *  bSilentlyFail = If true, failure to login will not trigger a request for auth
	 *
	 **/
	virtual void LoginOnline(FString EpicID, FString Auth, bool bIsRememberToken=false, bool bSilentlyFail=false);

	// Log this local player out
	virtual void Logout();
	
	/**
	 *	Gives a call back to an object looking to know when a player's status changed.
	 **/
	virtual void AddPlayerLoginStatusChangedDelegate(FPlayerOnlineStatusChangedDelegate NewDelegate);

	/**
	 *	Removes the  call back to an object looking to know when a player's status changed.
	 **/
	virtual void ClearPlayerLoginStatusChangedDelegate(FPlayerOnlineStatusChangedDelegate Delegate);


#if !UE_SERVER
	virtual void ToastCompleted();
#endif

protected:

	bool bInitialSignInAttempt;

	// Holds the local copy of the player nickname.
	UPROPERTY(config)
	FString PlayerNickname;

	// What is the Epic ID associated with this player.
	UPROPERTY(config)
	FString LastEpicIDLogin;
	
	// The RememberMe Token for this player. 
	UPROPERTY(config)
	FString LastEpicRememberMeToken;

	// Called to insure the OSS is cleaned up properly
	virtual void CleanUpOnlineSubSystyem();

	// Out Delegates
	virtual void OnLoginComplete(int32 LocalUserNum, bool bWasSuccessful, const FUniqueNetId& UniqueID, const FString& ErrorMessage);
	virtual void OnLoginStatusChanged(int32 LocalUserNum, ELoginStatus::Type PreviousLoginStatus, ELoginStatus::Type LoginStatus, const FUniqueNetId& UniqueID);
	virtual void OnLogoutComplete(int32 LocalUserNum, bool bWasSuccessful);

	// Array of delegates to call.
	TArray<FPlayerOnlineStatusChangedDelegate> PlayerLoginStatusChangedListeners;

#if !UE_SERVER
	virtual void AuthDialogClosed(TSharedPtr<SCompoundWidget> Widget, uint16 ButtonID);
#endif

	// Call this function to Attempt to load the Online Profile Settings for this user.
	virtual void GetAuth(bool bLastFailed=false);

private:

	// Holds the Username of the pending user.  It's set in LoginOnline and cleared when there is a successful connection
	FString PendingLoginUserName;

	// If true, a login failure will not prompt for a password.  This is for the initial auto-login sequence
	bool bSilentLoginFail;

	IOnlineSubsystem* OnlineSubsystem;
	IOnlineIdentityPtr OnlineIdentityInterface;
	IOnlineUserCloudPtr OnlineUserCloudInterface;

	// Our delegate references....
	FOnLoginCompleteDelegate OnLoginCompleteDelegate;		
	FOnLoginStatusChangedDelegate OnLoginStatusChangedDelegate;
	FOnLogoutCompleteDelegate OnLogoutCompleteDelegate;

	FOnReadUserFileCompleteDelegate OnReadUserFileCompleteDelegate;
	FOnWriteUserFileCompleteDelegate OnWriteUserFileCompleteDelegate;
	
public:
	virtual void LoadProfileSettings();
	virtual void SaveProfileSettings();
	virtual UUTProfileSettings* GetProfileSettings() { return CurrentProfileSettings; };

	virtual void SetNickname(FString NewName);

	FName TeamStyleRef(FName InName);

protected:

	// Holds the current profile settings.  
	UPROPERTY()
	UUTProfileSettings* CurrentProfileSettings;

	virtual FString GetProfileFilename();	

	virtual void OnReadUserFileComplete(bool bWasSuccessful, const FUniqueNetId& InUserId, const FString& FileName);
	virtual void OnWriteUserFileComplete(bool bWasSuccessful, const FUniqueNetId& InUserId, const FString& FileName);

public:
	FString LastLobbyServerGUID;

};




