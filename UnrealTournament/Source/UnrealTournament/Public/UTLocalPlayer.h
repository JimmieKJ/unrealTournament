// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "SlateBasics.h"
#include "Slate/SlateGameResources.h"
#include "Online.h"
#include "OnlineSubsystemTypes.h"
#include "../Private/Slate/SUWToast.h"
#include "../Private/Slate/SUWDialog.h"
#include "UTProfileSettings.h"
#include "OnlinePresenceInterface.h"
#include "UTLocalPlayer.generated.h"

class SUWServerBrowser;
class SUWFriendsPopup;

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

	virtual void MessageBox(FText MessageTitle, FText MessageText);

#if !UE_SERVER
	virtual TSharedPtr<class SUWDialog> ShowMessage(FText MessageTitle, FText MessageText, uint16 Buttons, const FDialogResultDelegate& Callback = FDialogResultDelegate(), FVector2D DialogSize = FVector2D(0.0,0.0f));

	/** utilities for opening and closing dialogs */
	virtual void OpenDialog(TSharedRef<class SUWDialog> Dialog, int32 ZOrder = 255);
	virtual void CloseDialog(TSharedRef<class SUWDialog> Dialog);
	TSharedPtr<class SUWServerBrowser> GetServerBrowser();
	TSharedPtr<class SUWStatsViewer> GetStatsViewer();
	TSharedPtr<class SUWCreditsPanel> GetCreditsPanel();

	TSharedPtr<class SUWindowsDesktop> GetCurrentMenu()
	{
		return DesktopSlateWidget;
	}
#endif

	// Holds all of the chat this client has recieved.
	TArray<TSharedPtr<FStoredChatMessage>> ChatArchive;
	virtual void SaveChat(FName Type, FString Message, FLinearColor Color);

	UPROPERTY(Config)
	FString TutorialLaunchParams;

protected:

#if !UE_SERVER
	TSharedPtr<class SUWindowsDesktop> DesktopSlateWidget;
	
	// Holds a persistent reference to the server browser.
	TSharedPtr<class SUWServerBrowser> ServerBrowserWidget;

	TSharedPtr<class SUWStatsViewer> StatsViewerWidget;
	TSharedPtr<class SUWCreditsPanel> CreditsPanelWidget;

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

	/** returns path for player's cosmetic item
	 * profile settings takes priority over .ini
	 */
	virtual FString GetHatPath() const;
	virtual void SetHatPath(const FString& NewHatPath);
	virtual FString GetEyewearPath() const;
	virtual void SetEyewearPath(const FString& NewEyewearPath);
	/** returns path for player's character (visual only data) */
	virtual FString GetCharacterPath() const;
	virtual void SetCharacterPath(const FString& NewCharacterPath);

	/** accessors for default URL options */
	virtual FString GetDefaultURLOption(const TCHAR* Key) const;
	virtual void SetDefaultURLOption(const FString& Key, const FString& Value);

// ONLINE ------

	// Called after creation on non-default objects to setup the online Subsystem
	virtual void InitializeOnlineSubsystem();

	// Returns true if this player is logged in to the UT Online Services
	virtual bool IsLoggedIn();

	virtual FString GetOnlinePlayerNickname();

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
	IOnlineSessionPtr OnlineSessionInterface;
	IOnlinePresencePtr OnlinePresenceInterface;

	// Our delegate references....
	FDelegateHandle OnLoginCompleteDelegate;		
	FDelegateHandle OnLoginStatusChangedDelegate;
	FDelegateHandle OnLogoutCompleteDelegate;

	FDelegateHandle OnReadUserFileCompleteDelegate;
	FDelegateHandle OnWriteUserFileCompleteDelegate;
	FDelegateHandle OnDeleteUserFileCompleteDelegate;

	FDelegateHandle OnJoinSessionCompleteDelegate;
	FDelegateHandle OnEndSessionCompleteDelegate;
	
public:
	virtual void LoadProfileSettings();
	virtual void SaveProfileSettings();
	virtual void ClearProfileSettings();

	virtual UUTProfileSettings* GetProfileSettings() { return CurrentProfileSettings; };

	virtual void SetNickname(FString NewName);

	FName TeamStyleRef(FName InName);

protected:

	// Holds the current profile settings.  
	UPROPERTY()
	UUTProfileSettings* CurrentProfileSettings;

	virtual FString GetProfileFilename();	
	virtual void ClearProfileWarnResults(TSharedPtr<SCompoundWidget> Widget, uint16 ButtonID);
	virtual void OnReadUserFileComplete(bool bWasSuccessful, const FUniqueNetId& InUserId, const FString& FileName);
	virtual void OnWriteUserFileComplete(bool bWasSuccessful, const FUniqueNetId& InUserId, const FString& FileName);
	virtual void OnDeleteUserFileComplete(bool bWasSuccessful, const FUniqueNetId& InUserId, const FString& FileName);

#if !UE_SERVER
	TSharedPtr<class SUWDialog> HUDSettings;
#endif

public:
	FString LastLobbyServerGUID;

	virtual void ShowHUDSettings();
	virtual void HideHUDSettings();

	// NOTE: These functions are for getting the user's ELO rating from the cloud.  This
	// is temp code and will be changed so don't rely on it staying as is.
private:
	
	int32 TDM_ELO;	// The Player's current TDM ELO rank
	int32 DUEL_ELO;	// The Player's current Duel ELO rank
	int32 MatchesPlayed;	// The # of matches this player has played.

	void ReadELOFromCloud();
	void UpdateBaseELOFromCloudData();
public:

	// Returns the filename for stats.
	static FString GetStatsFilename() { return TEXT("stats.json"); }
	
	// Returns the base ELO Rank with any type of processing we need.
	int GetBaseELORank();

	// Connect to a server via the session id
	virtual void JoinSession(FOnlineSessionSearchResult SearchResult, bool bSpectate);
	virtual void LeaveSession();

	// Updates this user's online presence
	void UpdatePresence(FString NewPresenceString, bool bAllowInvites, bool bAllowJoinInProgress, bool bAllowJoinViaPresence, bool bAllowJoinViaPresenceFriendsOnly);

protected:
	virtual void OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);
	virtual void OnEndSessionComplete(FName SessionName, bool bWasSuccessful);
	virtual void OnPresenceUpdated(const FUniqueNetId& UserId, const bool bWasSuccessful);
	virtual void OnPresenceRecieved(const FUniqueNetId& UserId, const TSharedRef<FOnlineUserPresence>& Presence);

#if !UE_SERVER

	 TSharedPtr<SOverlay> ContentLoadingMessage;

public:
	virtual void ShowContentLoadingMessage();
	virtual void HideContentLoadingMessage();

	virtual TSharedPtr<SWidget> GetFriendsPopup();
protected:
	TSharedPtr<SUWFriendsPopup> FriendsMenu;

#endif
	// If the player is not logged in, then this string will hold the last attempted presence update
	FString LastPresenceUpdate;
	bool bLastAllowInvites;


public:
	virtual uint32 GetCountryFlag();
	virtual void SetCountryFlag(uint32 NewFlag, bool bSave=false);
	
};




