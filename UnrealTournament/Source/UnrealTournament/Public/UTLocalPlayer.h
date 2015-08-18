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
#include "Http.h"
#include "UTProfileItem.h"

#include "UTLocalPlayer.generated.h"

class SUWServerBrowser;
class SUWFriendsPopup;
class SUTQuickMatch;
class SUWLoginDialog;
class SUWRedirectDialog;
class SUWMapVoteDialog;
class SUTReplayWindow;
class FFriendsAndChatMessage;
class AUTPlayerState;
class SUWMatchSummary;
class SUTJoinInstance;
class FServerData;

DECLARE_MULTICAST_DELEGATE_ThreeParams(FPlayerOnlineStatusChanged, class UUTLocalPlayer*, ELoginStatus::Type, const FUniqueNetId&);

// This delegate will be triggered whenever a chat message is updated.

class FStoredChatMessage : public TSharedFromThis<FStoredChatMessage>
{
public:
	// Where did this chat come from
	UPROPERTY()
	FName Type;

	// The Name of the server
	UPROPERTY()
	FString Sender;

	// What was the message
	UPROPERTY()
	FString Message;

	// What color should we display this chat in
	UPROPERTY()
	FLinearColor Color;

	UPROPERTY()
	int32 Timestamp;

	// If this chat message was owned by the player
	UPROPERTY()
	bool bMyChat;

	FStoredChatMessage(FName inType, FString inSender, FString inMessage, FLinearColor inColor, int32 inTimestamp, bool inbMyChat)
		: Type(inType), Sender(inSender), Message(inMessage), Color(inColor), Timestamp(inTimestamp), bMyChat(inbMyChat)
	{}

	static TSharedRef<FStoredChatMessage> Make(FName inType, FString inSender, FString inMessage, FLinearColor inColor, int32 inTimestamp, bool inbMyChat)
	{
		return MakeShareable( new FStoredChatMessage( inType, inSender, inMessage, inColor, inTimestamp, inbMyChat ) );
	}

};

DECLARE_MULTICAST_DELEGATE_TwoParams(FChatArchiveChanged, class UUTLocalPlayer*, TSharedPtr<FStoredChatMessage>);

USTRUCT()
struct FUTFriend
{
	GENERATED_USTRUCT_BODY()
public:
	FUTFriend()
	{}

	FUTFriend(FString inUserId, FString inDisplayName)
		: UserId(inUserId), DisplayName(inDisplayName)
	{}


	UPROPERTY()
	FString UserId;

	UPROPERTY()
	FString DisplayName;
};

UCLASS(config=Engine)
class UNREALTOURNAMENT_API UUTLocalPlayer : public ULocalPlayer
{
	GENERATED_UCLASS_BODY()

public:
	virtual ~UUTLocalPlayer();

	virtual bool IsMenuGame();

	virtual FString GetNickname() const;
	virtual FText GetAccountSummary() const;
	virtual FString GetAccountName() const;
	virtual FText GetAccountDisplayName() const;

	virtual void PlayerAdded(class UGameViewportClient* InViewportClient, int32 InControllerID);

	virtual void ShowMenu();
	virtual void HideMenu();
	virtual void OpenTutorialMenu();
	virtual void ShowToast(FText ToastText);	// NOTE: Need to add a type/etc so that they can be skinned better.

	virtual void MessageBox(FText MessageTitle, FText MessageText);

	virtual bool Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) override;

#if !UE_SERVER
	virtual TSharedPtr<class SUWDialog> ShowMessage(FText MessageTitle, FText MessageText, uint16 Buttons, const FDialogResultDelegate& Callback = FDialogResultDelegate(), FVector2D DialogSize = FVector2D(0.0,0.0f));
	virtual TSharedPtr<class SUWDialog> ShowSupressableConfirmation(FText MessageTitle, FText MessageText, FVector2D DialogSize, bool &InOutShouldSuppress, const FDialogResultDelegate& Callback = FDialogResultDelegate());

	/** utilities for opening and closing dialogs */
	virtual void OpenDialog(TSharedRef<class SUWDialog> Dialog, int32 ZOrder = 255);
	virtual void CloseDialog(TSharedRef<class SUWDialog> Dialog);
	TSharedPtr<class SUWServerBrowser> GetServerBrowser();
	TSharedPtr<class SUWReplayBrowser> GetReplayBrowser();
	TSharedPtr<class SUWStatsViewer> GetStatsViewer();
	TSharedPtr<class SUWCreditsPanel> GetCreditsPanel();

	void StartQuickMatch(FString QuickMatchType);
	void CloseQuickMatch();

	TSharedPtr<class SUWindowsDesktop> GetCurrentMenu()
	{
		return DesktopSlateWidget;
	}

	virtual bool AreMenusOpen();
#endif

	// Holds all of the chat this client has received.
	TArray<TSharedPtr<FStoredChatMessage>> ChatArchive;
	virtual void SaveChat(FName Type, FString Sender, FString Message, FLinearColor Color, bool bMyChat);

	UPROPERTY(Config)
	FString TutorialLaunchParams;

	UPROPERTY(Config)
	bool bFragCenterAutoPlay;

	UPROPERTY(Config)
	bool bFragCenterAutoMute;

	UPROPERTY(config)
	FString YoutubeAccessToken;

	UPROPERTY(config)
	FString YoutubeRefreshToken;

protected:

#if !UE_SERVER
	TSharedPtr<class SUWindowsDesktop> DesktopSlateWidget;
	
	// Holds a persistent reference to the server browser.
	TSharedPtr<class SUWServerBrowser> ServerBrowserWidget;

	TSharedPtr<class SUWReplayBrowser> ReplayBrowserWidget;
	TSharedPtr<class SUWStatsViewer> StatsViewerWidget;
	TSharedPtr<class SUWCreditsPanel> CreditsPanelWidget;

	/** stores a reference to open dialogs so they don't get destroyed */
	TArray< TSharedPtr<class SUWDialog> > OpenDialogs;
	TArray<TSharedPtr<class SUWToast>> ToastList;

	virtual void AddToastToViewport(TSharedPtr<SUWToast> ToastToDisplay);
	void WelcomeDialogResult(TSharedPtr<SCompoundWidget> Widget, uint16 ButtonID);
	void OnSwitchUserResult(TSharedPtr<SCompoundWidget> Widget, uint16 ButtonID);
	TSharedPtr<class SUTQuickMatch> QuickMatchDialog;
	TSharedPtr<class SUWLoginDialog> LoginDialog;

#endif

	bool bWantsToConnectAsSpectator;
	bool bWantsToFindMatch;

	int32 ConnectDesiredTeam;

	int32 CurrentSessionTrustLevel;

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
	virtual int32 GetHatVariant() const;
	virtual void SetHatVariant(int32 NewVariant);
	virtual int32 GetEyewearVariant() const;
	virtual void SetEyewearVariant(int32 NewVariant);
	virtual FString GetTauntPath() const;
	virtual void SetTauntPath(const FString& NewTauntPath);
	virtual FString GetTaunt2Path() const;
	virtual void SetTaunt2Path(const FString& NewTauntPath);
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
	virtual bool IsLoggedIn() const;

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
	virtual FDelegateHandle RegisterPlayerOnlineStatusChangedDelegate(const FPlayerOnlineStatusChanged::FDelegate& NewDelegate);

	/**
	 *	Removes the  call back to an object looking to know when a player's status changed.
	 **/
	virtual void RemovePlayerOnlineStatusChangedDelegate(FDelegateHandle DelegateHandle);


	/**
	 *	Registeres a delegate to get notifications of chat messages
	 **/
	virtual FDelegateHandle RegisterChatArchiveChangedDelegate(const FChatArchiveChanged::FDelegate& NewDelegate);

	/**
	 *	Removes a chat notification delegate.
	 **/
	virtual void RemoveChatArchiveChangedDelegate(FDelegateHandle DelegateHandle);


#if !UE_SERVER
	virtual void ToastCompleted();
	virtual void CloseAuth();
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

	FPlayerOnlineStatusChanged PlayerOnlineStatusChanged;
	FChatArchiveChanged ChatArchiveChanged;

	double LastProfileCloudWriteTime;
	double ProfileCloudWriteCooldownTime;
	FTimerHandle ProfileWriteTimerHandle;

#if !UE_SERVER
	virtual void AuthDialogClosed(TSharedPtr<SCompoundWidget> Widget, uint16 ButtonID);
#endif

public:
	// Call this function to Attempt to load the Online Profile Settings for this user.
	virtual void GetAuth(FString ErrorMessage = TEXT(""));

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
	IOnlineFriendsPtr OnlineFriendsInterface;

	// Our delegate references....
	FDelegateHandle OnLoginCompleteDelegate;		
	FDelegateHandle OnLoginStatusChangedDelegate;
	FDelegateHandle OnLogoutCompleteDelegate;

	FDelegateHandle OnEnumerateUserFilesCompleteDelegate;
	FDelegateHandle OnReadUserFileCompleteDelegate;
	FDelegateHandle OnWriteUserFileCompleteDelegate;
	FDelegateHandle OnDeleteUserFileCompleteDelegate;

	FDelegateHandle OnJoinSessionCompleteDelegate;
	FDelegateHandle OnEndSessionCompleteDelegate;
	FDelegateHandle OnDestroySessionCompleteDelegate;
	FDelegateHandle OnFindFriendSessionCompleteDelegate;
	
public:
	virtual void LoadProfileSettings();
	UFUNCTION()
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
	virtual void OnEnumerateUserFilesComplete(bool bWasSuccessful, const FUniqueNetId& InUserId);
	virtual void OnReadProfileItemsComplete(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded);

#if !UE_SERVER
	TSharedPtr<class SUWDialog> HUDSettings;
#endif

public:
	virtual void ShowHUDSettings();
	virtual void HideHUDSettings();

	// NOTE: These functions are for getting the user's ELO rating from the cloud.  This
	// is temp code and will be changed so don't rely on it staying as is.
private:
	
	int32 TDM_ELO;	// The Player's current TDM ELO rank
	int32 DUEL_ELO;	// The Player's current Duel ELO rank
	int32 FFA_ELO;	// The Player's current FFA ELO rank
	int32 CTF_ELO;	// The Player's current CTF ELO rank
	int32 MatchesPlayed;	// The # of matches this player has played.
	int32 DuelMatchesPlayed;	// The # of matches this player has played.
	int32 TDMMatchesPlayed;	// The # of matches this player has played.
	int32 FFAMatchesPlayed;	// The # of matches this player has played.
	int32 CTFMatchesPlayed;	// The # of matches this player has played.

	void ReadELOFromCloud();
	void UpdateBaseELOFromCloudData();

	void ReadCloudFileListing();
public:

	// Returns the filename for stats.
	static FString GetStatsFilename() { return TEXT("stats.json"); }
	
	// Returns the base ELO Rank with any type of processing we need.
	virtual int32 GetBaseELORank();

	// Returns what badge should represent player's skill level.
	static void GetBadgeFromELO(int32 EloRating, int32& BadgeLevel, int32& SubLevel);

	// Connect to a server via the session id.  Returns TRUE if the join continued, or FALSE if it failed to start
	virtual bool JoinSession(const FOnlineSessionSearchResult& SearchResult, bool bSpectate, FName QuickMatch = NAME_None, bool bFindMatch = false, int32 DesiredTeam = -1, FString MatchId=TEXT(""));
	virtual void LeaveSession();
	virtual void ReturnToMainMenu();

	// Updates this user's online presence
	void UpdatePresence(FString NewPresenceString, bool bAllowInvites, bool bAllowJoinInProgress, bool bAllowJoinViaPresence, bool bAllowJoinViaPresenceFriendsOnly);

	// Does the player have pending social notifications - should the social bang be shown?
	bool IsPlayerShowingSocialNotification() const;

protected:
	virtual void JoinPendingSession();
	virtual void OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);
	virtual void OnEndSessionComplete(FName SessionName, bool bWasSuccessful);
	virtual void OnDestroySessionComplete(FName SessionName, bool bWasSuccessful);

	virtual void OnPresenceUpdated(const FUniqueNetId& UserId, const bool bWasSuccessful);
	virtual void OnPresenceReceived(const FUniqueNetId& UserId, const TSharedRef<FOnlineUserPresence>& Presence);

	bool bPendingSession;
	FOnlineSessionSearchResult PendingSession;

	// friend join functionality
	virtual void JoinFriendSession(const FUniqueNetId& FriendId, const FUniqueNetId& SessionId);
	virtual void OnFindFriendSessionComplete(int32 LocalUserNum, bool bWasSuccessful, const FOnlineSessionSearchResult& SearchResult);
	virtual void HandleFriendsJoinGame(const FUniqueNetId& FriendId, const FUniqueNetId& SessionId);
	virtual bool AllowFriendsJoinGame();
	virtual void HandleFriendsNotificationAvail(bool bAvailable);
	virtual void HandleFriendsActionNotification(TSharedRef<FFriendsAndChatMessage> FriendsAndChatMessage);

	FString PendingFriendInviteSessionId;	
	FString PendingFriendInviteFriendId;
	bool bShowSocialNotification;

#if !UE_SERVER

	 TSharedPtr<SOverlay> ContentLoadingMessage;

public:
	virtual void ShowContentLoadingMessage();
	virtual void HideContentLoadingMessage();

	virtual TSharedPtr<SUWFriendsPopup> GetFriendsPopup();
protected:
	TSharedPtr<SUWFriendsPopup> FriendsMenu;

#endif
	// If the player is not logged in, then this string will hold the last attempted presence update
	FString LastPresenceUpdate;
	bool bLastAllowInvites;

	FName QuickMatchJoinType;


public:
	virtual FName GetCountryFlag();
	virtual void SetCountryFlag(FName NewFlag, bool bSave=false);

	// If the player switches profiles and is in a session, we have to delay the switch until we can leave the current session
	// and exit back to the main menu.  To do this, we store the Pending info here and when the main menu sees that the player has left a session
	// THEN we perform the login.

	virtual FName GetAvatar();
	virtual void SetAvatar(FName NewAvatar, bool bSave=false);


protected:
	bool bPendingLoginCreds;
	FString PendingLoginName;
	FString PendingLoginPassword;

public:
	bool IsAFriend(FUniqueNetIdRepl PlayerId);

	UPROPERTY(Config)
	uint32 bShowBrowserIconOnMainMenu:1;

	bool bSuppressToastsInGame;

protected:
#if !UE_SERVER
	TWeakPtr<SUWDialog> ConnectingDialog;
	TSharedPtr<SUWRedirectDialog> RedirectDialog;

#endif

	// Holds the current status of any ongoing downloads.
	FText DownloadStatusText;

	virtual void ConnectingDialogCancel(TSharedPtr<SCompoundWidget> Dialog, uint16 ButtonID);
public:
	virtual void ShowConnectingDialog();
	virtual void CloseConnectingDialog();

	virtual int32 GetFriendsList(TArray< FUTFriend >& OutFriendsList);
	virtual int32 GetRecentPlayersList(TArray< FUTFriend >& OutRecentPlayersList);

	// returns true if this player is in a session
	virtual bool IsInSession();

	UPROPERTY(config)
	int32 ServerPingBlockSize;

	virtual void ShowPlayerInfo(TWeakObjectPtr<AUTPlayerState> Target);
	virtual void OnTauntPlayed(AUTPlayerState* PS, TSubclassOf<AUTTaunt> TauntToPlay, float EmoteSpeed);
	virtual void OnEmoteSpeedChanged(AUTPlayerState* PS, float EmoteSpeed);

	// Request someone be my friend...
	virtual void RequestFriendship(TSharedPtr<FUniqueNetId> FriendID);

	// Holds a list of maps to play in Single player
	TArray<FString> SinglePlayerMapList;

	virtual void UpdateRedirect(const FString& FileURL, int32 NumBytes, float Progress, int32 NumFilesLeft);
	virtual bool ContentExists(const FPackageRedirectReference& Redirect);
	virtual void AccquireContent(TArray<FPackageRedirectReference>& Redirects);
	virtual void CancelDownload();

	virtual FText GetDownloadStatusText();
	virtual bool IsDownloadInProgress();

	// Forward any network failure messages to the base player controller so that game specific actions can be taken
	virtual void HandleNetworkFailureMessage(enum ENetworkFailure::Type FailureType, const FString& ErrorString);

protected:
#if !UE_SERVER
	TSharedPtr<SUWindowsDesktop> LoadoutMenu;
	TSharedPtr<SUWMapVoteDialog> MapVoteMenu;
	TSharedPtr<SUWMatchSummary> MatchSummaryWindow;
#endif

public:
	virtual void OpenLoadout(bool bBuyMenu = false);
	virtual void CloseLoadout();

	virtual void OpenMapVote(AUTGameState* GameState);
	virtual void CloseMapVote();

	virtual void OpenMatchSummary(AUTGameState* GameState);
	virtual void CloseMatchSummary();

	// What is your role within the unreal community.
	EUnrealRoles::Type CommunityRole;

	//Replay Stuff
#if !UE_SERVER
	TSharedPtr<SUTReplayWindow> ReplayWindow;
#endif
	void OpenReplayWindow();
	void CloseReplayWindow();
	void ToggleReplayWindow();

	virtual bool IsReplay();

#if !UE_SERVER
	void RecordReplay(float RecordTime);
	void RecordingReplayComplete();
	void GetYoutubeConsentForUpload();
	void ShouldVideoCompressDialogResult(TSharedPtr<SCompoundWidget> Widget, uint16 ButtonID);
	void VideoCompressDialogResult(TSharedPtr<SCompoundWidget> Widget, uint16 ButtonID);
	void ShouldVideoUploadDialogResult(TSharedPtr<SCompoundWidget> Widget, uint16 ButtonID);
	void YoutubeConsentResult(TSharedPtr<SCompoundWidget> Widget, uint16 ButtonID);
	void YoutubeUploadResult(TSharedPtr<SCompoundWidget> Widget, uint16 ButtonID);
	void YoutubeUploadCompleteResult(TSharedPtr<SCompoundWidget> Widget, uint16 ButtonID);
	void YoutubeTokenRequestComplete(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded);
	void YoutubeTokenRefreshComplete(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded);
	void UploadVideoToYoutube();
	bool bRecordingReplay;
	FString RecordedReplayFilename;
	FString RecordedReplayTitle;
	TSharedPtr<SUWDialog> YoutubeDialog;
	TSharedPtr<class SUWYoutubeConsent> YoutubeConsentDialog;
#endif

	virtual void VerifyGameSession(const FString& ServerSessionId);

	/** Return whether the progression system considers this player a beginner. **/
	virtual bool IsConsideredABeginnner();

	/** Closes any slate UI elements that are open. **/
	virtual void CloseAllUI();

protected:
	void OnFindSessionByIdComplete(int32 LocalUserNum, bool bWasSucessful, const FOnlineSessionSearchResult& SearchResult);
	
	// Will be true if we are attempting to force the player in to an existing session.
	bool bAttemptingForceJoin;

	/** Profile items this player owns, downloaded from the server */
	UPROPERTY()
	TArray<FProfileItemEntry> ProfileItems;

	/** XP gained in trusted online servers - read from backend */
	UPROPERTY()
	int32 OnlineXP;

	/** Used to avoid reading too often */
	double LastItemReadTime;
	
	// When connecting to a hub, if this is set it will be passed in.
	FString PendingJoinMatchId;

public:
	/** Read profile items from the backend */
	virtual void ReadProfileItems();
	inline const TArray<FProfileItemEntry>& GetProfileItems() const
	{
		return ProfileItems;
	}
	/** returns whether the user owns an item that grants the asset (cosmetic, character, whatever) with the given path */
	bool OwnsItemFor(const FString& Path, int32 VariantId = 0) const;

	float QuickMatchLimitTime;

	inline int32 GetOnlineXP() const
	{
		return OnlineXP;
	}
	inline void AddOnlineXP(int32 NewXP)
	{
		OnlineXP += FMath::Max<float>(0, NewXP);
	}

	bool IsOnTrustedServer() const
	{
		return GetWorld()->GetNetMode() == NM_Client && IsLoggedIn() && CurrentSessionTrustLevel == 0;
	}

	virtual void AttemptJoinInstance(TSharedPtr<FServerData> ServerData, FString InstanceId, bool bSpectate);
	virtual void CloseJoinInstanceDialog();

protected:
#if !UE_SERVER
	TSharedPtr<SUTJoinInstance> JoinInstanceDialog;
#endif


};
