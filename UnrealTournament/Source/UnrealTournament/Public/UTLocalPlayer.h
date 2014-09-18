// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Slate.h"
#include "Slate/SlateGameResources.h"
#include "../Private/Slate/SUWDialog.h"
#include "Online.h"
#include "OnlineSubsystemTypes.h"
//#include "UTProfileSettings.h"
#include "UTLocalPlayer.generated.h"

DECLARE_DELEGATE_ThreeParams(FPlayerOnlineStatusChangedDelegate, class UUTLocalPlayer*, ELoginStatus::Type, const FUniqueNetId&);

UCLASS()
class UUTLocalPlayer : public ULocalPlayer
{
	GENERATED_UCLASS_BODY()

public:
	virtual FString GetNickname() const;
	virtual void PlayerAdded(class UGameViewportClient* InViewportClient, int32 InControllerID);

	virtual void ShowMenu();
	virtual void HideMenu();

#if !UE_SERVER
	virtual TSharedPtr<class SUWDialog> ShowMessage(FText MessageTitle, FText MessageText, uint16 Buttons, const FDialogResultDelegate& Callback = FDialogResultDelegate());

	/** utilities for opening and closing dialogs */
	virtual void OpenDialog(TSharedRef<class SUWDialog> Dialog);
	virtual void CloseDialog(TSharedRef<class SUWDialog> Dialog);
#endif

	virtual bool IsMenuGame();

#if !UE_SERVER
	TSharedPtr<class SUWServerBrowser> GetServerBrowser();
#endif

protected:

#if !UE_SERVER
	TSharedPtr<class SUWindowsDesktop> DesktopSlateWidget;
	
	// Holds a persistent reference to the server browser.
	TSharedPtr<class SUWServerBrowser> ServerBrowserWidget;

	/** stores a reference to open dialogs so they don't get destroyed */
	TArray< TSharedPtr<class SUWDialog> > OpenDialogs;
#endif

	// ONLINE ------

	// What is the Epic ID associated with this profile.
	UPROPERTY(config)
	FString LastEpicIDLogin;
	
	// The RememberMe Token for this profile. 
	UPROPERTY(config)
	FString LastEpicRememberMeToken;

public:
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

	// Auto-Login if the proper creds are available
	virtual void PostInitProperties() override;

	/**
	 *	Gives a call back to an object looking to know when a player's status changed.
	 **/
	virtual void AddPlayerLoginStatusChangedDelegate(FPlayerOnlineStatusChangedDelegate NewDelegate);

protected:
	// Called to insure the OSS is cleaned up properly
	virtual void CleanUpOnlineSubSystyem();

	// Out Delegates
	virtual void OnLoginComplete(int32 LocalUserNum, bool bWasSuccessful, const FUniqueNetId& UniqueID, const FString& ErrorMessage);
	virtual void OnLoginStatusChanged(int32 LocalUserNum, ELoginStatus::Type LoginStatus, const FUniqueNetId& UniqueID);
	virtual void OnLogoutComplete(int32 LocalUserNum, bool bWasSuccessful);

	// Array of delegates to call.
	TArray<FPlayerOnlineStatusChangedDelegate> PlayerLoginStatusChangedListeners;

#if !UE_SERVER
	virtual void AuthDialogClosed(TSharedPtr<SCompoundWidget> Widget, uint16 ButtonID);
#endif

	// Call this function to Attempt to load the Online Profile Settings for this user.
/*
	virtual void LoadOnlineProfileSettings(UUTProfileSettings* ProfileSettingsToFill);
	virtual void SaveOnlineProfileSettings(UUTProfileSettings* ProfileSettingsToSave);
*/
	virtual void GetAuth(bool bLastFailed=false);

private:

	// Holds the Username of the pending user.  It's set in LoginOnline and cleared when there is a successful connection
	FString PendingLoginUserName;

	// If true, a login failure will not prompt for a password.  This is for the initial auto-login sequence
	bool bSilentLoginFail;

	IOnlineSubsystem* OnlineSubsystem;
	IOnlineIdentityPtr OnlineIdentityInterface;

	// Our delegate references....
	FOnLoginCompleteDelegate OnLoginCompleteDelegate;		
	FOnLoginStatusChangedDelegate OnLoginStatusChangedDelegate;
	FOnLogoutCompleteDelegate OnLogoutCompleteDelegate;
};




