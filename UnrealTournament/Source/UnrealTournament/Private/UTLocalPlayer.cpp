// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTLocalPlayer.h"
#include "UTCharacter.h"
#include "Online.h"
#include "OnlineSubsystemTypes.h"
#include "UTMenuGameMode.h"
#include "UTProfileSettings.h"
#include "UTGameViewportClient.h"
#include "Slate/SUWindowsDesktop.h"
#include "Slate/SUWindowsMainMenu.h"
#include "Slate/Panels/SUWServerBrowser.h"
#include "Slate/Panels/SUWReplayBrowser.h"
#include "Slate/Panels/SUWStatsViewer.h"
#include "Slate/Panels/SUWCreditsPanel.h"
#include "Slate/SUWMessageBox.h"
#include "Slate/SUWindowsStyle.h"
#include "Slate/SUWDialog.h"
#include "Slate/SUWToast.h"
#include "Slate/SUWInputBox.h"
#include "Slate/SUWLoginDialog.h"
#include "Slate/SUWPlayerSettingsDialog.h"
#include "Slate/SUWPlayerInfoDialog.h"
#include "Slate/SUWHUDSettingsDialog.h"
#include "Slate/SUTQuickMatch.h"
#include "Slate/SUWFriendsPopup.h"
#include "Slate/SUWRedirectDialog.h"
#include "Slate/SUTLoadoutMenu.h"
#include "Slate/SUTBuyMenu.h"
#include "Slate/SUWMapVoteDialog.h"
#include "Slate/SUTReplayWindow.h"
#include "Slate/SUTReplayMenu.h"
#include "UTAnalytics.h"
#include "FriendsAndChat.h"
#include "Runtime/Analytics/Analytics/Public/Analytics.h"
#include "Runtime/Analytics/Analytics/Public/Interfaces/IAnalyticsProvider.h"
#include "Base64.h"
#include "UTGameEngine.h"
#include "Engine/DemoNetDriver.h"
#include "UTConsole.h"


UUTLocalPlayer::UUTLocalPlayer(const class FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bInitialSignInAttempt = true;
	LastProfileCloudWriteTime = 0;
	ProfileCloudWriteCooldownTime = 15;
	bShowSocialNotification = false;
	ServerPingBlockSize = 30;
	bSuppressToastsInGame = false;
	DownloadStatusText = FText::GetEmpty();
}

UUTLocalPlayer::~UUTLocalPlayer()
{
	// Terminate the dedicated server if we started one
	if (DedicatedServerProcessHandle.IsValid() && FPlatformProcess::IsProcRunning(DedicatedServerProcessHandle))
	{
		FPlatformProcess::TerminateProc(DedicatedServerProcessHandle);
	}
}

void UUTLocalPlayer::InitializeOnlineSubsystem()
{
	OnlineSubsystem = IOnlineSubsystem::Get();
	if (OnlineSubsystem) 
	{
		OnlineIdentityInterface = OnlineSubsystem->GetIdentityInterface();
		OnlineUserCloudInterface = OnlineSubsystem->GetUserCloudInterface();
		OnlineSessionInterface = OnlineSubsystem->GetSessionInterface();
		OnlinePresenceInterface = OnlineSubsystem->GetPresenceInterface();
		OnlineFriendsInterface = OnlineSubsystem->GetFriendsInterface();
	}

	if (OnlineIdentityInterface.IsValid())
	{
		OnLoginCompleteDelegate = OnlineIdentityInterface->AddOnLoginCompleteDelegate_Handle(GetControllerId(), FOnLoginCompleteDelegate::CreateUObject(this, &UUTLocalPlayer::OnLoginComplete));
		OnLoginStatusChangedDelegate = OnlineIdentityInterface->AddOnLoginStatusChangedDelegate_Handle(GetControllerId(), FOnLoginStatusChangedDelegate::CreateUObject(this, &UUTLocalPlayer::OnLoginStatusChanged));
		OnLogoutCompleteDelegate = OnlineIdentityInterface->AddOnLogoutCompleteDelegate_Handle(GetControllerId(), FOnLogoutCompleteDelegate::CreateUObject(this, &UUTLocalPlayer::OnLogoutComplete));
	}

	if (OnlineUserCloudInterface.IsValid())
	{
		OnReadUserFileCompleteDelegate = OnlineUserCloudInterface->AddOnReadUserFileCompleteDelegate_Handle(FOnReadUserFileCompleteDelegate::CreateUObject(this, &UUTLocalPlayer::OnReadUserFileComplete));
		OnWriteUserFileCompleteDelegate = OnlineUserCloudInterface->AddOnWriteUserFileCompleteDelegate_Handle(FOnWriteUserFileCompleteDelegate::CreateUObject(this, &UUTLocalPlayer::OnWriteUserFileComplete));
		OnDeleteUserFileCompleteDelegate = OnlineUserCloudInterface->AddOnDeleteUserFileCompleteDelegate_Handle(FOnDeleteUserFileCompleteDelegate::CreateUObject(this, &UUTLocalPlayer::OnDeleteUserFileComplete));
		OnEnumerateUserFilesCompleteDelegate = OnlineUserCloudInterface->AddOnEnumerateUserFilesCompleteDelegate_Handle(FOnEnumerateUserFilesCompleteDelegate::CreateUObject(this, &UUTLocalPlayer::OnEnumerateUserFilesComplete));
	}

	if (OnlineSessionInterface.IsValid())
	{
		OnJoinSessionCompleteDelegate = OnlineSessionInterface->AddOnJoinSessionCompleteDelegate_Handle(FOnJoinSessionCompleteDelegate::CreateUObject(this, &UUTLocalPlayer::OnJoinSessionComplete));
	}
	
}

void UUTLocalPlayer::CleanUpOnlineSubSystyem()
{
	if (OnlineSubsystem)
	{
		if (OnlineIdentityInterface.IsValid())
		{
			OnlineIdentityInterface->ClearOnLoginCompleteDelegate_Handle(GetControllerId(), OnLoginCompleteDelegate);
			OnlineIdentityInterface->ClearOnLoginStatusChangedDelegate_Handle(GetControllerId(), OnLoginStatusChangedDelegate);
			OnlineIdentityInterface->ClearOnLogoutCompleteDelegate_Handle(GetControllerId(), OnLogoutCompleteDelegate);
		}

		if (OnlineUserCloudInterface.IsValid())
		{
			OnlineUserCloudInterface->ClearOnReadUserFileCompleteDelegate_Handle(OnReadUserFileCompleteDelegate);
			OnlineUserCloudInterface->ClearOnWriteUserFileCompleteDelegate_Handle(OnWriteUserFileCompleteDelegate);
			OnlineUserCloudInterface->ClearOnDeleteUserFileCompleteDelegate_Handle(OnDeleteUserFileCompleteDelegate);
			OnlineUserCloudInterface->ClearOnEnumerateUserFilesCompleteDelegate_Handle(OnEnumerateUserFilesCompleteDelegate);
		}

		if (OnlineSessionInterface.IsValid())
		{
			OnlineSessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(OnJoinSessionCompleteDelegate);
		}
	}
}

bool UUTLocalPlayer::Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar)
{
	// disallow certain commands in shipping builds
#if UE_BUILD_SHIPPING
	if (FParse::Command(&Cmd, TEXT("SHOW")))
	{
		return true;
	}
#endif
	return Super::Exec(InWorld, Cmd, Ar);
}

bool UUTLocalPlayer::IsAFriend(FUniqueNetIdRepl PlayerId)
{
	return (PlayerId.IsValid() && OnlineFriendsInterface.IsValid() && OnlineFriendsInterface->IsFriend(0, *PlayerId, EFriendsLists::ToString(EFriendsLists::InGamePlayers)));
}

FString UUTLocalPlayer::GetNickname() const
{
	return PlayerNickname;
}

FText UUTLocalPlayer::GetAccountDisplayName() const
{
	if (OnlineIdentityInterface.IsValid() && PlayerController && PlayerController->PlayerState)
	{

		TSharedPtr<FUniqueNetId> UserId = OnlineIdentityInterface->GetUniquePlayerId(GetControllerId());
		if (UserId.IsValid())
		{
			TSharedPtr<FUserOnlineAccount> UserAccount = OnlineIdentityInterface->GetUserAccount(*UserId);
			if (UserAccount.IsValid())
			{
				return FText::FromString(UserAccount->GetDisplayName());
			}
		}
	}

	return FText::GetEmpty();
}

FText UUTLocalPlayer::GetAccountSummary() const
{
	if (OnlineIdentityInterface.IsValid() && PlayerController && PlayerController->PlayerState)
	{

		TSharedPtr<FUniqueNetId> UserId = OnlineIdentityInterface->GetUniquePlayerId(GetControllerId());
		if (UserId.IsValid())
		{
			TSharedPtr<FUserOnlineAccount> UserAccount = OnlineIdentityInterface->GetUserAccount(*UserId);
			if (UserAccount.IsValid())
			{
				return FText::Format(NSLOCTEXT("UTLocalPlayer","AccountSummaryFormat","{0} # of Friends: {1}  # Online: {2}"), FText::FromString(UserAccount->GetDisplayName()), FText::AsNumber(0),FText::AsNumber(0));
			}
		}
	}

	return FText::GetEmpty();
}



void UUTLocalPlayer::PlayerAdded(class UGameViewportClient* InViewportClient, int32 InControllerID)
{
#if !UE_SERVER
	SUWindowsStyle::Initialize();
#endif

	Super::PlayerAdded(InViewportClient, InControllerID);

	if (FUTAnalytics::IsAvailable())
	{
		FString OSMajor;
		FString OSMinor;

		FPlatformMisc::GetOSVersions(OSMajor, OSMinor);

		TArray<FAnalyticsEventAttribute> ParamArray;
		ParamArray.Add(FAnalyticsEventAttribute(TEXT("OSMajor"), OSMajor));
		ParamArray.Add(FAnalyticsEventAttribute(TEXT("OSMinor"), OSMinor));
		ParamArray.Add(FAnalyticsEventAttribute(TEXT("CPUVendor"), FPlatformMisc::GetCPUVendor()));
		ParamArray.Add(FAnalyticsEventAttribute(TEXT("CPUBrand"), FPlatformMisc::GetCPUBrand()));
		FUTAnalytics::GetProvider().RecordEvent( TEXT("SystemInfo"), ParamArray );
	}

	if (!InViewportClient->GetWorld()->IsPlayInEditor())
	{
		if (!HasAnyFlags(RF_ClassDefaultObject))
		{
			// Initialize the Online Subsystem for this player
			InitializeOnlineSubsystem();

			if (OnlineIdentityInterface.IsValid())
			{
				// Attempt to Auto-Login to MCP
				if (!OnlineIdentityInterface->AutoLogin(GetControllerId()))
				{
					bInitialSignInAttempt = false;
				}
			}
		}
	}
}

bool UUTLocalPlayer::IsMenuGame()
{
	if (bNoMidGameMenu) return true;

	if (GetWorld()->GetNetMode() == NM_Standalone)
	{
		AUTMenuGameMode* GM = Cast<AUTMenuGameMode>(GetWorld()->GetAuthGameMode());
		return GM != NULL;
	}

	return false;
}


void UUTLocalPlayer::ShowMenu()
{
#if !UE_SERVER
	// Create the slate widget if it doesn't exist
	if (!DesktopSlateWidget.IsValid())
	{
		if ( IsMenuGame() )
		{
			SAssignNew(DesktopSlateWidget, SUWindowsMainMenu).PlayerOwner(this);
		}
		else if (IsReplay())
		{
			SAssignNew(DesktopSlateWidget, SUTReplayMenu).PlayerOwner(this);
		}
		else
		{

			AGameState* GameState = GetWorld()->GetGameState<AGameState>();
			if (GameState != nullptr && GameState->GameModeClass != nullptr)
			{
				AUTBaseGameMode* UTGameMode = GameState->GameModeClass->GetDefaultObject<AUTBaseGameMode>();
				if (UTGameMode != nullptr)
				{
					DesktopSlateWidget = UTGameMode->GetGameMenu(this);
				}
			}

		}
		if (DesktopSlateWidget.IsValid())
		{
			GEngine->GameViewport->AddViewportWidgetContent( SNew(SWeakWidget).PossiblyNullContent(DesktopSlateWidget.ToSharedRef()));
		}
	}

	// Make it visible.
	if (DesktopSlateWidget.IsValid())
	{
		// Widget is already valid, just make it visible.
		DesktopSlateWidget->SetVisibility(EVisibility::Visible);
		DesktopSlateWidget->OnMenuOpened();

		if (PlayerController)
		{
			PlayerController->bShowMouseCursor = true;
			PlayerController->SetInputMode(FInputModeUIOnly());

			if (!IsMenuGame())
			{
				PlayerController->SetPause(true);
			}
		}
	}
#endif
}
void UUTLocalPlayer::HideMenu()
{
#if !UE_SERVER

	if (ContentLoadingMessage.IsValid())
	{
		UE_LOG(UT,Log,TEXT("Can't close menus during loading"));
		return; // Don't allow someone to close the menu while we are loading....
	}

	if (DesktopSlateWidget.IsValid())
	{
		GEngine->GameViewport->RemoveViewportWidgetContent(DesktopSlateWidget.ToSharedRef());
		DesktopSlateWidget->OnMenuClosed();
		DesktopSlateWidget.Reset();
		if (PlayerController)
		{
			PlayerController->bShowMouseCursor = false;
			PlayerController->SetPause(false);

			//Replay window will set its own input mode
			if (!IsReplay())
			{
				PlayerController->SetInputMode(FInputModeGameOnly());
			}
		}

		if (IsReplay())
		{
			OpenReplayWindow();

			//Temp workaround for the console breaking input
			UUTConsole* Console = (ViewportClient != nullptr) ? Cast<UUTConsole>(ViewportClient->ViewportConsole) : nullptr;
			if (Console != nullptr)
			{
				Console->ClearReopenMenus();
			}
		}
		else
		{
			FSlateApplication::Get().SetUserFocusToGameViewport(0, EFocusCause::SetDirectly);
		}

	}
	else
	{
		UE_LOG(UT,Log,TEXT("Call to HideMenu() when without a menu being opened."));
	}
	CloseConnectingDialog();
#endif
}

void UUTLocalPlayer::MessageBox(FText MessageTitle, FText MessageText)
{
#if !UE_SERVER
	ShowMessage(MessageTitle, MessageText, UTDIALOG_BUTTON_OK, NULL);
#endif
}

#if !UE_SERVER
TSharedPtr<class SUWDialog>  UUTLocalPlayer::ShowMessage(FText MessageTitle, FText MessageText, uint16 Buttons, const FDialogResultDelegate& Callback, FVector2D DialogSize)
{
	TSharedPtr<class SUWDialog> NewDialog;
	if (DialogSize.IsNearlyZero())
	{
		SAssignNew(NewDialog, SUWMessageBox)
			.PlayerOwner(this)
			.DialogTitle(MessageTitle)
			.MessageText(MessageText)
			.ButtonMask(Buttons)
			.OnDialogResult(Callback);
	}
	else
	{
		SAssignNew(NewDialog, SUWMessageBox)
			.PlayerOwner(this)
			.bDialogSizeIsRelative(true)
			.DialogSize(DialogSize)
			.DialogTitle(MessageTitle)
			.MessageText(MessageText)
			.ButtonMask(Buttons)
			.OnDialogResult(Callback);
	}

	OpenDialog( NewDialog.ToSharedRef() );
	return NewDialog;
}

TSharedPtr<class SUWDialog> UUTLocalPlayer::ShowSupressableConfirmation(FText MessageTitle, FText MessageText, FVector2D DialogSize, bool &InOutShouldSuppress, const FDialogResultDelegate& Callback)
{
	auto OnGetSuppressibleState = [&InOutShouldSuppress]() { return InOutShouldSuppress ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; };

	auto OnSetSuppressibleState = [&InOutShouldSuppress](ECheckBoxState CheckBoxState) 
	{ 
		InOutShouldSuppress = CheckBoxState == ECheckBoxState::Checked;
	};

	TSharedPtr<class SUWDialog> NewDialog;
	if (DialogSize.IsNearlyZero())
	{
		SAssignNew(NewDialog, SUWMessageBox)
			.PlayerOwner(this)
			.DialogTitle(MessageTitle)
			.MessageText(MessageText)
			.ButtonMask(UTDIALOG_BUTTON_OK)
			.OnDialogResult(Callback)
			.IsSuppressible(true)
			.SuppressibleCheckBoxState_Lambda( OnGetSuppressibleState )
			.OnSuppressibleCheckStateChanged_Lambda( OnSetSuppressibleState );
	}
	else
	{
		SAssignNew(NewDialog, SUWMessageBox)
			.PlayerOwner(this)
			.bDialogSizeIsRelative(true)
			.DialogSize(DialogSize)
			.DialogTitle(MessageTitle)
			.MessageText(MessageText)
			.ButtonMask(UTDIALOG_BUTTON_OK)
			.IsSuppressible(true)
			.SuppressibleCheckBoxState_Lambda(OnGetSuppressibleState)
			.OnSuppressibleCheckStateChanged_Lambda(OnSetSuppressibleState)
			.OnDialogResult(Callback);
	}

	OpenDialog(NewDialog.ToSharedRef());
	return NewDialog;
}

void UUTLocalPlayer::OpenDialog(TSharedRef<SUWDialog> Dialog, int32 ZOrder)
{
	GEngine->GameViewport->AddViewportWidgetContent(Dialog, ZOrder);
	Dialog->OnDialogOpened();
}

void UUTLocalPlayer::CloseDialog(TSharedRef<SUWDialog> Dialog)
{
	Dialog->OnDialogClosed();
	GEngine->GameViewport->RemoveViewportWidgetContent(Dialog);
}

TSharedPtr<class SUWServerBrowser> UUTLocalPlayer::GetServerBrowser()
{
	if (!ServerBrowserWidget.IsValid())
	{
		SAssignNew(ServerBrowserWidget, SUWServerBrowser, this);
	}

	return ServerBrowserWidget;
}

TSharedPtr<class SUWReplayBrowser> UUTLocalPlayer::GetReplayBrowser()
{
	if (!ReplayBrowserWidget.IsValid())
	{
		SAssignNew(ReplayBrowserWidget, SUWReplayBrowser, this);
	}

	return ReplayBrowserWidget;
}

TSharedPtr<class SUWStatsViewer> UUTLocalPlayer::GetStatsViewer()
{
	if (!StatsViewerWidget.IsValid())
	{
		SAssignNew(StatsViewerWidget, SUWStatsViewer, this);
	}

	return StatsViewerWidget;
}

TSharedPtr<class SUWCreditsPanel> UUTLocalPlayer::GetCreditsPanel()
{
	if (!CreditsPanelWidget.IsValid())
	{
		SAssignNew(CreditsPanelWidget, SUWCreditsPanel, this);
	}

	return CreditsPanelWidget;
}

#endif

void UUTLocalPlayer::ShowHUDSettings()
{
#if !UE_SERVER
	if (!HUDSettings.IsValid())
	{
		SAssignNew(HUDSettings, SUWHUDSettingsDialog)
			.PlayerOwner(this);

		OpenDialog( HUDSettings.ToSharedRef() );

		if (PlayerController)
		{
			PlayerController->bShowMouseCursor = true;
			PlayerController->SetInputMode( FInputModeUIOnly() );
			if (!IsMenuGame())
			{
				PlayerController->SetPause(true);
			}
		}
	}
#endif
}

void UUTLocalPlayer::HideHUDSettings()
{
#if !UE_SERVER

	if (HUDSettings.IsValid())
	{
		CloseDialog(HUDSettings.ToSharedRef());
		HUDSettings.Reset();

		if (PlayerController)
		{
			PlayerController->bShowMouseCursor = false;
			PlayerController->SetInputMode(FInputModeGameOnly());
			PlayerController->SetPause(false);
		}

		FSlateApplication::Get().SetUserFocusToGameViewport(0, EFocusCause::SetDirectly);
	}
#endif
}

bool UUTLocalPlayer::IsLoggedIn() 
{ 
	return OnlineIdentityInterface.IsValid() && OnlineIdentityInterface->GetLoginStatus(GetControllerId());
}


void UUTLocalPlayer::LoginOnline(FString EpicID, FString Auth, bool bIsRememberToken, bool bSilentlyFail)
{
	if ( !OnlineIdentityInterface.IsValid() ) return;

	if (IsLoggedIn() )
	{
		// Allow users to switch accounts
		PendingLoginUserName = LastEpicIDLogin;
		GetAuth();
	}
	else
	{

		FString Override;
		if ( FParse::Value(FCommandLine::Get(),TEXT("-id="),Override))
		{
			EpicID = Override;
		}

		if ( FParse::Value(FCommandLine::Get(),TEXT("-pass="),Override))
		{
			Auth=Override;
			bIsRememberToken=false;
		}

		if (EpicID == TEXT(""))
		{
			EpicID = LastEpicIDLogin;
		}

		// Save this for later.
		PendingLoginUserName = EpicID;
		bSilentLoginFail = bSilentlyFail;

		if (EpicID == TEXT("") || Auth == TEXT(""))
		{
			GetAuth();
			return;
		}

		FOnlineAccountCredentials AccountCreds(TEXT("epic"), EpicID, Auth);
		if (bIsRememberToken)
		{
			AccountCreds.Type = TEXT("refresh");
		}

		// Begin the Login Process...
		if (!OnlineIdentityInterface->Login(GetControllerId(), AccountCreds))
		{
#if !UE_SERVER
			// We should never fail here unless something has gone horribly wrong
			if (bSilentLoginFail)
			{
				UE_LOG(UT, Warning, TEXT("Could not connect to the online subsystem. Please check your connection and try again."));
			}
			else
			{
				ShowMessage(NSLOCTEXT("MCPMessages", "OnlineError", "Online Error"), NSLOCTEXT("MCPMessages", "UnknownLoginFailuire", "Could not connect to the online subsystem.  Please check your connection and try again."), UTDIALOG_BUTTON_OK, NULL);
			}
			return;
#endif
		}
	}
}

void UUTLocalPlayer::Logout()
{
	if (IsLoggedIn() && OnlineIdentityInterface.IsValid())
	{
		// Begin the Login Process....
		if (!OnlineIdentityInterface->Logout(GetControllerId()))
		{
#if !UE_SERVER
			// We should never fail here unless something has gone horribly wrong
			ShowMessage(NSLOCTEXT("MCPMessages","OnlineError","Online Error"), NSLOCTEXT("MCPMessages","UnknownLogoutFailuire","Could not log out from the online subsystem.  Please check your connection and try again."), UTDIALOG_BUTTON_OK, NULL);
			return;
#endif
		}
	}
}



FString UUTLocalPlayer::GetOnlinePlayerNickname()
{
	return IsLoggedIn() ? OnlineIdentityInterface->GetPlayerNickname(0) : TEXT("None");
}

void UUTLocalPlayer::OnLoginComplete(int32 LocalUserNum, bool bWasSuccessful, const FUniqueNetId& UniqueID, const FString& ErrorMessage)
{
	if (bWasSuccessful)
	{
		// Save the creds for the next auto-login

		TSharedPtr<FUserOnlineAccount> Account = OnlineIdentityInterface->GetUserAccount(UniqueID);
		if (Account.IsValid())
		{
			FString RememberMeToken;
			FString Token;
			Account->GetAuthAttribute(TEXT("refresh_token"), RememberMeToken);

			if ( Account->GetAuthAttribute(TEXT("ut:developer"), Token) )			CommunityRole = EUnrealRoles::Developer;
			else if ( Account->GetAuthAttribute(TEXT("ut:contributor"), Token) )	CommunityRole = EUnrealRoles::Contributor;
			else if ( Account->GetAuthAttribute(TEXT("ut:concepter"), Token) )		CommunityRole = EUnrealRoles::Concepter;
			else if ( Account->GetAuthAttribute(TEXT("ut:prototyper"), Token) )		CommunityRole = EUnrealRoles::Prototyper;
			else if ( Account->GetAuthAttribute(TEXT("ut:marketplace"), Token) )	CommunityRole = EUnrealRoles::Marketplace;
			else if ( Account->GetAuthAttribute(TEXT("ut:ambassador"), Token) )		CommunityRole = EUnrealRoles::Ambassador;
			else 
			{
				CommunityRole = EUnrealRoles::Gamer;
			}
			
			LastEpicIDLogin = PendingLoginUserName;
			LastEpicRememberMeToken = RememberMeToken;
			SaveConfig();
		}

		PendingLoginUserName = TEXT("");

		LoadProfileSettings();
		FText WelcomeToast = FText::Format(NSLOCTEXT("MCP","MCPWelcomeBack","Welcome back {0}"), FText::FromString(*GetOnlinePlayerNickname()));
		ShowToast(WelcomeToast);

		// Init the Friends And Chat system
		IFriendsAndChatModule::Get().GetFriendsAndChatManager()->Login();
		IFriendsAndChatModule::Get().GetFriendsAndChatManager()->SetAnalyticsProvider(FUTAnalytics::GetProviderPtr());

		if (!IFriendsAndChatModule::Get().GetFriendsAndChatManager()->OnFriendsJoinGame().IsBoundToObject(this))
		{
			IFriendsAndChatModule::Get().GetFriendsAndChatManager()->OnFriendsJoinGame().AddUObject(this, &UUTLocalPlayer::HandleFriendsJoinGame);
		}
		if (!IFriendsAndChatModule::Get().GetFriendsAndChatManager()->AllowFriendsJoinGame().IsBoundToObject(this))
		{
			IFriendsAndChatModule::Get().GetFriendsAndChatManager()->AllowFriendsJoinGame().BindUObject(this, &UUTLocalPlayer::AllowFriendsJoinGame);
		}
		if (!IFriendsAndChatModule::Get().GetFriendsAndChatManager()->OnFriendsNotification().IsBoundToObject(this))
		{
			IFriendsAndChatModule::Get().GetFriendsAndChatManager()->OnFriendsNotification().AddUObject(this, &UUTLocalPlayer::HandleFriendsNotificationAvail);
		}
		if (!IFriendsAndChatModule::Get().GetFriendsAndChatManager()->OnFriendsActionNotification().IsBoundToObject(this))
		{
			IFriendsAndChatModule::Get().GetFriendsAndChatManager()->OnFriendsActionNotification().AddUObject(this, &UUTLocalPlayer::HandleFriendsActionNotification);
		}

		// on successful auto login, attempt to join an accepted friend game invite
		if (bInitialSignInAttempt)
		{
			FString SessionId;
			FString FriendId;
			if (FParse::Value(FCommandLine::Get(), TEXT("invitesession="), SessionId) && !SessionId.IsEmpty() &&
				FParse::Value(FCommandLine::Get(), TEXT("invitefrom="), FriendId) && !FriendId.IsEmpty())
			{
				JoinFriendSession(FUniqueNetIdString(FriendId), FUniqueNetIdString(SessionId));
			}
		}
	}

	// We have enough credentials to auto-login.  So try it, but silently fail if we cant.
	else if (bInitialSignInAttempt)
	{
		if (LastEpicIDLogin != TEXT("") && LastEpicRememberMeToken != TEXT(""))
		{
			bInitialSignInAttempt = false;
			LoginOnline(LastEpicIDLogin, LastEpicRememberMeToken, true, true);
		}
	}

	// Otherwise if this is the first attempt, then silently fair
	else if (!bSilentLoginFail)
	{
		GetAuth(ErrorMessage);
	}
}

void UUTLocalPlayer::GetAuth(FString ErrorMessage)
{
#if !UE_SERVER
	if (GetWorld()->IsPlayInEditor())
	{
		return;
	}

	if (LoginDialog.IsValid())
	{
		return;
	}

	bool bError = ErrorMessage != TEXT("");

	SAssignNew(LoginDialog, SUWLoginDialog)
		.OnDialogResult(FDialogResultDelegate::CreateUObject(this, &UUTLocalPlayer::AuthDialogClosed))
		.UserIDText(PendingLoginUserName)
		.ErrorText(bError ? FText::FromString(ErrorMessage) : FText::GetEmpty())
		.PlayerOwner(this);

	GEngine->GameViewport->AddViewportWidgetContent(LoginDialog.ToSharedRef(), 160);
	LoginDialog->SetInitialFocus();

#endif
}

void UUTLocalPlayer::OnLoginStatusChanged(int32 LocalUserNum, ELoginStatus::Type PreviousLoginStatus, ELoginStatus::Type LoginStatus, const FUniqueNetId& UniqueID)
{
	UE_LOG(UT,Verbose,TEXT("***[LoginStatusChanged]*** - User %i - %i"), LocalUserNum, int32(LoginStatus));

	// If we have logged out, or started using the local profile, then clear the online profile.
	if (LoginStatus == ELoginStatus::NotLoggedIn || LoginStatus == ELoginStatus::UsingLocalProfile)
	{
		CurrentProfileSettings = NULL;
		FUTAnalytics::LoginStatusChanged(FString());

		if (bPendingLoginCreds)
		{
			bPendingLoginCreds = false;
			LoginOnline(PendingLoginName, PendingLoginPassword);
			PendingLoginPassword = TEXT("");
		}


		// If we are connected to a server, then exit back to the main menu.
		if (GetWorld()->GetNetMode() == NM_Client)
		{
			ReturnToMainMenu();		
		}

	}
	else if (LoginStatus == ELoginStatus::LoggedIn)
	{
		ReadELOFromCloud();
		UpdatePresence(LastPresenceUpdate, bLastAllowInvites,bLastAllowInvites,bLastAllowInvites,false);
		ReadCloudFileListing();
		// query entitlements for UI
		IOnlineEntitlementsPtr EntitlementsInterface = OnlineSubsystem->GetEntitlementsInterface();
		if (EntitlementsInterface.IsValid())
		{
			EntitlementsInterface->QueryEntitlements(UniqueID, TEXT("ut"));
		}
		FUTAnalytics::LoginStatusChanged(UniqueID.ToString());

		// If we hage a pending session, then join it.
		if (bPendingSession)
		{
			bPendingSession = false;
			OnlineSessionInterface->JoinSession(0, GameSessionName, PendingSession);
		}
	}


	PlayerOnlineStatusChanged.Broadcast(this, LoginStatus, UniqueID);
}

void UUTLocalPlayer::ReadCloudFileListing()
{
	if (OnlineUserCloudInterface.IsValid() && OnlineIdentityInterface.IsValid())
	{
		TSharedPtr<FUniqueNetId> UserId = OnlineIdentityInterface->GetUniquePlayerId(GetControllerId());
		if (UserId.IsValid())
		{
			OnlineUserCloudInterface->EnumerateUserFiles(*UserId.Get());
		}
	}
}

void UUTLocalPlayer::OnEnumerateUserFilesComplete(bool bWasSuccessful, const FUniqueNetId& InUserId)
{
	UE_LOG(UT, Verbose, TEXT("OnEnumerateUserFilesComplete %d"), bWasSuccessful ? 1 : 0);
	UUTGameEngine* UTEngine = Cast<UUTGameEngine>(GEngine);
	if (UTEngine)
	{
		UTEngine->CloudContentChecksums.Empty();

		if (OnlineUserCloudInterface.IsValid() && OnlineIdentityInterface.IsValid())
		{
			TArray<FCloudFileHeader> UserFiles;
			OnlineUserCloudInterface->GetUserFileList(InUserId, UserFiles);
			for (int32 i = 0; i < UserFiles.Num(); i++)
			{
				TArray<uint8> DecodedHash;
				FBase64::Decode(UserFiles[i].Hash, DecodedHash);
				FString Hash = BytesToHex(DecodedHash.GetData(), DecodedHash.Num());
				UE_LOG(UT, Verbose, TEXT("%s %s"), *UserFiles[i].FileName, *Hash);
				UTEngine->CloudContentChecksums.Add(FPaths::GetBaseFilename(UserFiles[i].FileName), Hash);
			}
		}		
	}
}

void UUTLocalPlayer::OnLogoutComplete(int32 LocalUserNum, bool bWasSuccessful)
{
	UE_LOG(UT,Verbose,TEXT("***[Logout Complete]*** - User %i"), LocalUserNum);
	// TO-DO: Add a Toast system for displaying stuff like this

	GetWorld()->GetTimerManager().ClearTimer(ProfileWriteTimerHandle);
}

#if !UE_SERVER

void UUTLocalPlayer::CloseAuth()
{
	GEngine->GameViewport->RemoveViewportWidgetContent(LoginDialog.ToSharedRef());
	LoginDialog.Reset();
}

void UUTLocalPlayer::AuthDialogClosed(TSharedPtr<SCompoundWidget> Widget, uint16 ButtonID)
{
	
	if (ButtonID != UTDIALOG_BUTTON_CANCEL)
	{
		if (LoginDialog.IsValid())
		{
			// Look to see if we are already logged in.
			if ( IsLoggedIn() )
			{
				bPendingLoginCreds = true;
				PendingLoginName = LoginDialog->GetEpicID();
				PendingLoginPassword = LoginDialog->GetPassword();

				CloseAuth();


				// If we are in an active session, warn that this will cause you to go back to the main menu.
				TSharedPtr<FUniqueNetId> UserId = OnlineIdentityInterface->GetUniquePlayerId(0);
				if (UserId.IsValid() && OnlineSessionInterface->IsPlayerInSession(GameSessionName, *UserId))
				{
					ShowMessage(NSLOCTEXT("UTLocalPlayer", "SwitchLoginsTitle", "Change Users..."), NSLOCTEXT("UTLocalPlayer", "SwitchLoginsMsg", "Switching users will cause you to return to the main menu and leave any game you are currently in.  Are you sure you wish to do this?"), UTDIALOG_BUTTON_YES + UTDIALOG_BUTTON_NO, FDialogResultDelegate::CreateUObject(this, &UUTLocalPlayer::OnSwitchUserResult),FVector2D(0.25,0.25));					
				}
				else
				{
					Logout();
				}
				return;
			}

			else
			{
				FString UserName = LoginDialog->GetEpicID();
				FString Password = LoginDialog->GetPassword();
				CloseAuth();
				LoginOnline(UserName, Password,false);
			}
		}
	}
	else
	{
		if (LoginDialog.IsValid())
		{
			CloseAuth();
		}
		PendingLoginUserName = TEXT("");
	}
}

void UUTLocalPlayer::OnSwitchUserResult(TSharedPtr<SCompoundWidget> Widget, uint16 ButtonID)
{
	if (ButtonID == UTDIALOG_BUTTON_YES)
	{
		// If we are in an active session, then we have to force a return to the main menu.  If we are not in an active session (ie: setting at the main menu)
		// we can just logout/login..
		TSharedPtr<FUniqueNetId> UserId = OnlineIdentityInterface->GetUniquePlayerId(0);
		if (UserId.IsValid() && OnlineSessionInterface->IsPlayerInSession(GameSessionName, *UserId))
		{
			// kill the current menu....
			HideMenu();
			ReturnToMainMenu();	
		}
		else
		{
			Logout();
		}
	}
	else
	{
		bPendingLoginCreds = false;
		PendingLoginPassword = TEXT("");
	}
}

#endif

FDelegateHandle UUTLocalPlayer::RegisterPlayerOnlineStatusChangedDelegate(const FPlayerOnlineStatusChanged::FDelegate& NewDelegate)
{
	return PlayerOnlineStatusChanged.Add(NewDelegate);
}

void UUTLocalPlayer::RemovePlayerOnlineStatusChangedDelegate(FDelegateHandle DelegateHandle)
{
	PlayerOnlineStatusChanged.Remove(DelegateHandle);
}


void UUTLocalPlayer::ShowToast(FText ToastText)
{
#if !UE_SERVER

	if (GetWorld()->GetNetMode() == ENetMode::NM_Client && bSuppressToastsInGame) return;

	// Build the Toast to Show...

	TSharedPtr<SUWToast> Toast;
	SAssignNew(Toast, SUWToast)
		.PlayerOwner(this)
		.ToastText(ToastText);

	if (Toast.IsValid())
	{
		ToastList.Add(Toast);

		// Auto show if it's the first toast..
		if (ToastList.Num() == 1)
		{
			AddToastToViewport(ToastList[0]);
		}
	}
#endif
}

#if !UE_SERVER
void UUTLocalPlayer::AddToastToViewport(TSharedPtr<SUWToast> ToastToDisplay)
{
	GEngine->GameViewport->AddViewportWidgetContent( SNew(SWeakWidget).PossiblyNullContent(ToastToDisplay.ToSharedRef()),10000);
}

void UUTLocalPlayer::ToastCompleted()
{
	GEngine->GameViewport->RemoveViewportWidgetContent(ToastList[0].ToSharedRef());
	ToastList.RemoveAt(0,1);

	if (ToastList.Num() > 0)
	{
		AddToastToViewport(ToastList[0]);
	}
}

#endif

FString UUTLocalPlayer::GetProfileFilename()
{
	if (IsLoggedIn())
	{
		return TEXT("user_profile_1");
	}

	return TEXT("local_user_profile");
}

/*
 *	If the player is currently logged in, trigger a load of their profile settings from the MCP.  
 */
void UUTLocalPlayer::LoadProfileSettings()
{
	if (GetWorld()->IsPlayInEditor())
	{
		return;
	}

	if (IsLoggedIn())
	{
		TSharedPtr<FUniqueNetId> UserID = OnlineIdentityInterface->GetUniquePlayerId(GetControllerId());
		if (OnlineUserCloudInterface.IsValid() && UserID.IsValid())
		{
			OnlineUserCloudInterface->ReadUserFile(*UserID, GetProfileFilename());
		}
	}
}

void UUTLocalPlayer::ClearProfileSettings()
{
#if !UE_SERVER
	if (IsLoggedIn())
	{
		ShowMessage(NSLOCTEXT("UUTLocalPlayer","ClearCloudWarnTitle","!!! WARNING !!!"), NSLOCTEXT("UUTLocalPlayer","ClearCloudWarnMessage","You are about to clear all of your settings in the cloud as well as clear your active game and input ini files locally.  The game will then exit and wait for a restart!\n\nAre you sure you want to do this??"), UTDIALOG_BUTTON_YES + UTDIALOG_BUTTON_NO, FDialogResultDelegate::CreateUObject(this, &UUTLocalPlayer::ClearProfileWarnResults));
	}
#endif
}

void UUTLocalPlayer::ClearProfileWarnResults(TSharedPtr<SCompoundWidget> Widget, uint16 ButtonID)
{
	if (IsLoggedIn() && ButtonID == UTDIALOG_BUTTON_YES)
	{
		TSharedPtr<FUniqueNetId> UserID = OnlineIdentityInterface->GetUniquePlayerId(GetControllerId());
		if (OnlineUserCloudInterface.IsValid() && UserID.IsValid())
		{
			OnlineUserCloudInterface->DeleteUserFile(*UserID, GetProfileFilename(), true, true);
		}
	}
}

void UUTLocalPlayer::OnDeleteUserFileComplete(bool bWasSuccessful, const FUniqueNetId& InUserId, const FString& FileName)
{
#if !UE_SERVER
	// We successfully cleared the cloud, rewrite everything
	if (bWasSuccessful && FileName == GetProfileFilename())
	{
		FString PlaformName = FPlatformProperties::PlatformName();
		FString Filename = FString::Printf(TEXT("%s%s/Input.ini"), *FPaths::GeneratedConfigDir(), *PlaformName);
		if (FPlatformFileManager::Get().GetPlatformFile().DeleteFile(*Filename))
		{
			UE_LOG(UT,Log,TEXT("Failed to delete Input.ini"));
		}

		Filename = FString::Printf(TEXT("%s%s/Game.ini"), *FPaths::GeneratedConfigDir(), *PlaformName);
		if (FPlatformFileManager::Get().GetPlatformFile().DeleteFile(*Filename))
		{
			UE_LOG(UT,Log,TEXT("Failed to delete Game.ini"));
		}

		Filename = FString::Printf(TEXT("%s%s/GameUserSettings.ini"), *FPaths::GeneratedConfigDir(), *PlaformName);
		if (FPlatformFileManager::Get().GetPlatformFile().DeleteFile(*Filename))
		{
			UE_LOG(UT,Log,TEXT("Failed to delete GameUserSettings.ini"));
		}


		FPlatformMisc::RequestExit( 0 );
	}
#endif
}


void UUTLocalPlayer::OnReadUserFileComplete(bool bWasSuccessful, const FUniqueNetId& InUserId, const FString& FileName)
{
	if (FileName == GetProfileFilename())
	{
		// We were attempting to read the profile.. see if it was successful.	

		if (bWasSuccessful && OnlineUserCloudInterface.IsValid())	
		{
			// Create the current profile.
			if (CurrentProfileSettings == NULL)
			{
				CurrentProfileSettings = NewObject<UUTProfileSettings>(GetTransientPackage(),UUTProfileSettings::StaticClass());
			}

			TArray<uint8> FileContents;
			OnlineUserCloudInterface->GetFileContents(InUserId, FileName, FileContents);
			
			// Serialize the object
			FMemoryReader MemoryReader(FileContents, true);
			FObjectAndNameAsStringProxyArchive Ar(MemoryReader, false);
			CurrentProfileSettings->Serialize(Ar);
			CurrentProfileSettings->VersionFixup();

			FString CmdLineSwitch = TEXT("");
			bool bClearProfile = FParse::Param(FCommandLine::Get(), TEXT("ClearProfile"));

			// Check to make sure the profile settings are valid and that we aren't forcing them
			// to be cleared.  If all is OK, then apply these settings.
			if (CurrentProfileSettings->SettingsRevisionNum >= VALID_PROFILESETTINGS_VERSION && !bClearProfile)
			{
				CurrentProfileSettings->ApplyAllSettings(this);
				return;
			}
			else
			{
				CurrentProfileSettings->ClearWeaponPriorities();
			}
		}
		else if (CurrentProfileSettings == NULL) // Create a new profile settings object
		{
			CurrentProfileSettings = NewObject<UUTProfileSettings>(GetTransientPackage(),UUTProfileSettings::StaticClass());

			// Set some profile defaults, should be a function call if this gets any larger
			CurrentProfileSettings->TauntPath = GetDefaultURLOption(TEXT("Taunt"));
			CurrentProfileSettings->Taunt2Path = GetDefaultURLOption(TEXT("Taunt2"));
		}

		PlayerNickname = GetAccountDisplayName().ToString();
		SaveConfig();
		SaveProfileSettings();

#if !UE_SERVER
		FText WelcomeMessage = FText::Format(NSLOCTEXT("UTLocalPlayer","Welcome","This is your first time logging in so we have set your player name to '{0}'.  Would you like to change it now?"), GetAccountDisplayName());
		ShowMessage(NSLOCTEXT("UTLocalPlayer", "WelcomeTitle", "Welcome to Unreal Tournament"), WelcomeMessage, UTDIALOG_BUTTON_YES + UTDIALOG_BUTTON_NO, FDialogResultDelegate::CreateUObject(this, &UUTLocalPlayer::WelcomeDialogResult),FVector2D(0.35,0.25));
		// We couldn't load our profile or it was invalid or we choose to clear it so save it out.
#endif

	}
	else if (FileName == GetStatsFilename())
	{
		if (bWasSuccessful)
		{
			UpdateBaseELOFromCloudData();
		}
	}
}

#if !UE_SERVER
void UUTLocalPlayer::WelcomeDialogResult(TSharedPtr<SCompoundWidget> Widget, uint16 ButtonID)
{
	if (ButtonID == UTDIALOG_BUTTON_YES)
	{
		OpenDialog(SNew(SUWPlayerSettingsDialog).PlayerOwner(this));			
	}
}

#endif

void UUTLocalPlayer::SaveProfileSettings()
{
	if ( CurrentProfileSettings != NULL && IsLoggedIn() )
	{
		CurrentProfileSettings->GatherAllSettings(this);
		CurrentProfileSettings->SettingsRevisionNum = CURRENT_PROFILESETTINGS_VERSION;

		CurrentProfileSettings->bNeedProfileWriteOnLevelChange = false;

		// Build a blob of the profile contents
		TArray<uint8> FileContents;
		FMemoryWriter MemoryWriter(FileContents, true);
		FObjectAndNameAsStringProxyArchive Ar(MemoryWriter, false);
		CurrentProfileSettings->Serialize(Ar);

		if (FApp::GetCurrentTime() - LastProfileCloudWriteTime < ProfileCloudWriteCooldownTime)
		{
			GetWorld()->GetTimerManager().SetTimer(ProfileWriteTimerHandle, this, &UUTLocalPlayer::SaveProfileSettings, ProfileCloudWriteCooldownTime - (FApp::GetCurrentTime() - LastProfileCloudWriteTime), false);
		}
		else
		{
			// Save the blob to the cloud
			TSharedPtr<FUniqueNetId> UserID = OnlineIdentityInterface->GetUniquePlayerId(GetControllerId());
			if (OnlineUserCloudInterface.IsValid() && UserID.IsValid())
			{
				OnlineUserCloudInterface->WriteUserFile(*UserID, GetProfileFilename(), FileContents);
			}
		}
	}
}

void UUTLocalPlayer::OnWriteUserFileComplete(bool bWasSuccessful, const FUniqueNetId& InUserId, const FString& FileName)
{
	// Make sure this was our filename
	if (FileName == GetProfileFilename())
	{
		LastProfileCloudWriteTime = FApp::GetCurrentTime();

		if (bWasSuccessful)
		{
			FText Saved = NSLOCTEXT("MCP", "ProfileSaved", "Profile Saved");
			ShowToast(Saved);
		}
		else
		{
	#if !UE_SERVER
			// Should give a warning here if it fails.
			ShowMessage(NSLOCTEXT("MCPMessages", "ProfileSaveErrorTitle", "An error has occured"), NSLOCTEXT("MCPMessages", "ProfileSaveErrorText", "UT could not save your profile with the MCP.  Your settings may be lost."), UTDIALOG_BUTTON_OK, NULL);
	#endif
		}
	}
}

void UUTLocalPlayer::SetNickname(FString NewName)
{
	PlayerNickname = NewName;
	SaveConfig();

	
	if (PlayerController) 
	{
		PlayerController->ServerChangeName(NewName);
	}
}

void UUTLocalPlayer::SaveChat(FName Type, FString Message, FLinearColor Color)
{
	ChatArchive.Add( FStoredChatMessage::Make(Type, Message, Color));
}

FName UUTLocalPlayer::TeamStyleRef(FName InName)
{
	if (PlayerController)
	{
		AUTPlayerController* PC = Cast<AUTPlayerController>(PlayerController);
		if (PC && PC->GetTeamNum() == 0)
		{
			return FName( *(TEXT("Red.") + InName.ToString()));
		}
	}

	return FName( *(TEXT("Blue.") + InName.ToString()));
}

void UUTLocalPlayer::ReadELOFromCloud()
{
	TSharedPtr<FUniqueNetId> UserId = OnlineIdentityInterface->GetUniquePlayerId(GetControllerId());
	if (OnlineUserCloudInterface.IsValid() && UserId.IsValid())
	{
		OnlineUserCloudInterface->ReadUserFile(*UserId, GetStatsFilename());
	}
}

void UUTLocalPlayer::UpdateBaseELOFromCloudData()
{
	TArray<uint8> FileContents;
	TSharedPtr<FUniqueNetId> UserId = OnlineIdentityInterface->GetUniquePlayerId(GetControllerId());
	if (UserId.IsValid() && OnlineUserCloudInterface.IsValid() && OnlineUserCloudInterface->GetFileContents(*UserId, GetStatsFilename(), FileContents))
	{
		if (FileContents.Num() <= 0)
		{
			UE_LOG(LogGameStats, Warning, TEXT("Stats json content is empty"));
			return;
		}

		if (FileContents.GetData()[FileContents.Num() - 1] != 0)
		{
			UE_LOG(LogGameStats, Warning, TEXT("Failed to get proper stats json"));
			return;
		}

		FString JsonString = ANSI_TO_TCHAR((char*)FileContents.GetData());

		TSharedPtr<FJsonObject> StatsJson;
		TSharedRef<TJsonReader<>> JsonReader = TJsonReaderFactory<>::Create(JsonString);
		if (FJsonSerializer::Deserialize(JsonReader, StatsJson) && StatsJson.IsValid())
		{
			FString JsonStatsID;
			if (StatsJson->TryGetStringField(TEXT("StatsID"), JsonStatsID) && JsonStatsID == UserId->ToString())
			{
				StatsJson->TryGetNumberField(TEXT("SkillRating"), DUEL_ELO);
				StatsJson->TryGetNumberField(TEXT("TDMSkillRating"), TDM_ELO);
				StatsJson->TryGetNumberField(TEXT("DMSkillRating"), FFA_ELO);
				StatsJson->TryGetNumberField(TEXT("CTFSkillRating"), CTF_ELO);
				StatsJson->TryGetNumberField(TEXT("MatchesPlayed"), MatchesPlayed);
				StatsJson->TryGetNumberField(TEXT("SkillRatingSamples"), DuelMatchesPlayed);
				StatsJson->TryGetNumberField(TEXT("TDMSkillRatingSamples"), TDMMatchesPlayed);
				StatsJson->TryGetNumberField(TEXT("DMSkillRatingSamples"), FFAMatchesPlayed);
				StatsJson->TryGetNumberField(TEXT("CTFSkillRatingSamples"), CTFMatchesPlayed);
			}
		}
	}
}

int32 UUTLocalPlayer::GetBaseELORank()
{
	float TotalRating = 0.f;
	float RatingCount = 0.f;
	float CurrentRating = 1.f;
	int32 BestRating = 0;
	int32 MatchCount = DuelMatchesPlayed + TDMMatchesPlayed + FFAMatchesPlayed + CTFMatchesPlayed;

	if (DUEL_ELO > 0)
	{
		TotalRating += DUEL_ELO;
		RatingCount += 1.f;
		CurrentRating = TotalRating / RatingCount;
		BestRating = (DuelMatchesPlayed > 40) ? FMath::Max(BestRating, DUEL_ELO) : BestRating;
	}

	if (TDM_ELO > 0)
	{
		TotalRating += TDM_ELO;
		RatingCount += 1.f;
		CurrentRating = TotalRating / RatingCount;
		BestRating = (TDMMatchesPlayed > 40) ? FMath::Max(BestRating, TDM_ELO) : BestRating;
	}

	// FFA Elo is the least accurate, weighted lower @TODO FIXMESTEVE show badge based on current game type
	// max rating of 2400 based on FFA 
	if ((FFA_ELO > CurrentRating) && ((CurrentRating < 2400) || (DuelMatchesPlayed + TDMMatchesPlayed + CTFMatchesPlayed < 40)))
	{
		TotalRating += 0.5f * FMath::Min(FFA_ELO, 2400);
		RatingCount += 0.5f;
		CurrentRating = TotalRating / RatingCount;
	}

	if ((CTF_ELO > CurrentRating) || (DuelMatchesPlayed + TDMMatchesPlayed < 40))
	{
		TotalRating += CTF_ELO;
		RatingCount += 1.f;
		CurrentRating = TotalRating / RatingCount;
		BestRating = (CTFMatchesPlayed > 40) ? FMath::Max(BestRating, CTF_ELO) : BestRating;
	}

	// Limit displayed Elo to 400 + 50 * number of matches played
	return FMath::Min(FMath::Max(float(BestRating), CurrentRating), 400.f + 50.f * MatchCount);
}

void UUTLocalPlayer::GetBadgeFromELO(int32 EloRating, int32& BadgeLevel, int32& SubLevel)
{
	// Bronze levels up to 1750, start at 400, go up every 150
	if (EloRating  < 1750)
	{
		BadgeLevel = 0;
		SubLevel = FMath::Clamp((float(EloRating) - 250.f) / 150.f, 0.f, 8.f);
	}
	else if (EloRating < 3550)
	{
		BadgeLevel = 1;
		SubLevel = FMath::Clamp((float(EloRating) - 1750.f) / 200.f, 0.f, 8.f);
	}
	else
	{
		BadgeLevel = 2;
		SubLevel = FMath::Clamp((float(EloRating) - 3550.f) / 400.f, 0.f, 8.f);
	}
}

int32 UUTLocalPlayer::GetHatVariant() const
{
	return (CurrentProfileSettings != NULL) ? CurrentProfileSettings->HatVariant : FCString::Atoi(*GetDefaultURLOption(TEXT("HatVar")));
}

void UUTLocalPlayer::SetHatVariant(int32 NewVariant)
{
	if (CurrentProfileSettings != NULL)
	{
		CurrentProfileSettings->HatVariant = NewVariant;
	}
	SetDefaultURLOption(TEXT("HatVar"), FString::FromInt(NewVariant));
	if (PlayerController != NULL)
	{
		AUTPlayerState* PS = Cast<AUTPlayerState>(PlayerController->PlayerState);
		if (PS != NULL)
		{
			PS->ServerReceiveHatVariant(NewVariant);
		}
	}
}

int32 UUTLocalPlayer::GetEyewearVariant() const
{
	return (CurrentProfileSettings != NULL) ? CurrentProfileSettings->EyewearVariant : FCString::Atoi(*GetDefaultURLOption(TEXT("EyewearVar")));

}
void UUTLocalPlayer::SetEyewearVariant(int32 NewVariant)
{
	if (CurrentProfileSettings != NULL)
	{
		CurrentProfileSettings->EyewearVariant = NewVariant;
	}
	SetDefaultURLOption(TEXT("EyewearVar"), FString::FromInt(NewVariant));
	if (PlayerController != NULL)
	{
		AUTPlayerState* PS = Cast<AUTPlayerState>(PlayerController->PlayerState);
		if (PS != NULL)
		{
			PS->ServerReceiveEyewearVariant(NewVariant);
		}
	}
}

FString UUTLocalPlayer::GetHatPath() const
{
	return (CurrentProfileSettings != NULL) ? CurrentProfileSettings->HatPath : GetDefaultURLOption(TEXT("Hat"));
}
void UUTLocalPlayer::SetHatPath(const FString& NewHatPath)
{
	if (CurrentProfileSettings != NULL)
	{
		CurrentProfileSettings->HatPath = NewHatPath;
	}
	SetDefaultURLOption(TEXT("Hat"), NewHatPath);
	if (PlayerController != NULL)
	{
		AUTPlayerState* PS = Cast<AUTPlayerState>(PlayerController->PlayerState);
		if (PS != NULL)
		{
			PS->ServerReceiveHatClass(NewHatPath);
		}
	}
}
FString UUTLocalPlayer::GetEyewearPath() const
{
	return (CurrentProfileSettings != NULL) ? CurrentProfileSettings->EyewearPath : GetDefaultURLOption(TEXT("Eyewear"));
}
void UUTLocalPlayer::SetEyewearPath(const FString& NewEyewearPath)
{
	if (CurrentProfileSettings != NULL)
	{
		CurrentProfileSettings->EyewearPath = NewEyewearPath;
	}
	SetDefaultURLOption(TEXT("Eyewear"), NewEyewearPath);
	if (PlayerController != NULL)
	{
		AUTPlayerState* PS = Cast<AUTPlayerState>(PlayerController->PlayerState);
		if (PS != NULL)
		{
			PS->ServerReceiveEyewearClass(NewEyewearPath);
		}
	}
}
FString UUTLocalPlayer::GetCharacterPath() const
{
	return (CurrentProfileSettings != NULL) ? CurrentProfileSettings->CharacterPath : GetDefaultURLOption(TEXT("Character"));
}
void UUTLocalPlayer::SetCharacterPath(const FString& NewCharacterPath)
{
	if (CurrentProfileSettings != NULL)
	{
		CurrentProfileSettings->CharacterPath = NewCharacterPath;
	}
	SetDefaultURLOption(TEXT("Character"), NewCharacterPath);
	if (PlayerController != NULL)
	{
		AUTPlayerState* PS = Cast<AUTPlayerState>(PlayerController->PlayerState);
		if (PS != NULL)
		{
			PS->ServerSetCharacter(NewCharacterPath);
		}
	}
}
FString UUTLocalPlayer::GetTauntPath() const
{
	return (CurrentProfileSettings != NULL) ? CurrentProfileSettings->TauntPath : GetDefaultURLOption(TEXT("Taunt"));
}
void UUTLocalPlayer::SetTauntPath(const FString& NewTauntPath)
{
	if (CurrentProfileSettings != NULL)
	{
		CurrentProfileSettings->TauntPath = NewTauntPath;
	}
	SetDefaultURLOption(TEXT("Taunt"), NewTauntPath);
	if (PlayerController != NULL)
	{
		AUTPlayerState* PS = Cast<AUTPlayerState>(PlayerController->PlayerState);
		if (PS != NULL)
		{
			PS->ServerReceiveTauntClass(NewTauntPath);
		}
	}
}


FString UUTLocalPlayer::GetTaunt2Path() const
{
	return (CurrentProfileSettings != NULL) ? CurrentProfileSettings->Taunt2Path : GetDefaultURLOption(TEXT("Taunt2"));
}
void UUTLocalPlayer::SetTaunt2Path(const FString& NewTauntPath)
{
	if (CurrentProfileSettings != NULL)
	{
		CurrentProfileSettings->Taunt2Path = NewTauntPath;
	}
	SetDefaultURLOption(TEXT("Taunt2"), NewTauntPath);
	if (PlayerController != NULL)
	{
		AUTPlayerState* PS = Cast<AUTPlayerState>(PlayerController->PlayerState);
		if (PS != NULL)
		{
			PS->ServerReceiveTaunt2Class(NewTauntPath);
		}
	}
}

FString UUTLocalPlayer::GetDefaultURLOption(const TCHAR* Key) const
{
	FURL DefaultURL;
	DefaultURL.LoadURLConfig(TEXT("DefaultPlayer"), GGameIni);
	FString Op = DefaultURL.GetOption(Key, TEXT(""));
	FString Result;
	Op.Split(TEXT("="), NULL, &Result);
	return Result;
}
void UUTLocalPlayer::SetDefaultURLOption(const FString& Key, const FString& Value)
{
	FURL DefaultURL;
	DefaultURL.LoadURLConfig(TEXT("DefaultPlayer"), GGameIni);
	DefaultURL.AddOption(*FString::Printf(TEXT("%s=%s"), *Key, *Value));
	DefaultURL.SaveURLConfig(TEXT("DefaultPlayer"), *Key, GGameIni);
}

#if !UE_SERVER
void UUTLocalPlayer::ShowContentLoadingMessage()
{
	if (!ContentLoadingMessage.IsValid())
	{
		SAssignNew(ContentLoadingMessage, SOverlay)
		+SOverlay::Slot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.FillWidth(1.0)
			[
				SNew(SVerticalBox)
				+SVerticalBox::Slot()
				.HAlign(HAlign_Center)
				.AutoHeight()
				[
					SNew(SBox)
					.WidthOverride(700)
					.HeightOverride(64)
					[
						SNew(SOverlay)
						+SOverlay::Slot()
						[
							SNew(SVerticalBox)
							+SVerticalBox::Slot()
							.VAlign(VAlign_Fill)
							.HAlign(HAlign_Fill)
							[
								SNew(SImage)
								.Image(SUWindowsStyle::Get().GetBrush("UWindows.Standard.Dialog.Background"))
							]
						]
						+SOverlay::Slot()
						[
							SNew(SVerticalBox)
							+SVerticalBox::Slot()
							.VAlign(VAlign_Center)
							.HAlign(HAlign_Center)
							[
								SNew(STextBlock)
								.Text(NSLOCTEXT("MenuMessages","InitMenu","Initializing Menus"))
								.TextStyle(SUWindowsStyle::Get(), "UT.TopMenu.Button.TextStyle")
							]
						]
					]
				]
			]
		];
	}

	if (ContentLoadingMessage.IsValid())
	{
		GEngine->GameViewport->AddViewportWidgetContent(ContentLoadingMessage.ToSharedRef(), 255);
	}

}

void UUTLocalPlayer::HideContentLoadingMessage()
{
	if (ContentLoadingMessage.IsValid())
	{
		GEngine->GameViewport->RemoveViewportWidgetContent(ContentLoadingMessage.ToSharedRef());			
		ContentLoadingMessage.Reset();
	}
}

TSharedPtr<SUWFriendsPopup> UUTLocalPlayer::GetFriendsPopup()
{
	if (!FriendsMenu.IsValid())
	{
		SAssignNew(FriendsMenu, SUWFriendsPopup)
			.PlayerOwner(this);
	}

	return FriendsMenu;
}


#endif

void UUTLocalPlayer::ReturnToMainMenu()
{
	HideMenu();

	// Under certain situations (when we fail to load a replay immediately after starting watching it), 
	//	the replay menu will show up at the last second, and nothing will close it.
	// This is to make absolutely sure the replay menu doesn't persist into the main menu
	CloseReplayWindow();

	Exec(GetWorld(), TEXT("disconnect"), *GLog);
}

bool UUTLocalPlayer::JoinSession(const FOnlineSessionSearchResult& SearchResult, bool bSpectate, FName QuickMatchType, bool bFindMatch, int32 DesiredTeam)
{
	UE_LOG(UT,Log, TEXT("##########################"));
	UE_LOG(UT,Log, TEXT("Joining a New Session"));
	UE_LOG(UT,Log, TEXT("##########################"));

	QuickMatchJoinType = QuickMatchType;

	bWantsToConnectAsSpectator = bSpectate;
	bWantsToFindMatch = bFindMatch;

	ConnectDesiredTeam = DesiredTeam;

	FUniqueNetIdRepl UniqueId = OnlineIdentityInterface->GetUniquePlayerId(0);
	if (!UniqueId.IsValid())
	{
		return false;
	}
	else
	{
		if (OnlineSessionInterface->IsPlayerInSession(GameSessionName, *UniqueId))
		{
			UE_LOG(UT, Log, TEXT("--- Alreadyt in a Session -- Deferring while I clean it up"));
			bPendingSession = true;
			PendingSession = SearchResult;
			LeaveSession();
		}
		else
		{
			OnlineSessionInterface->JoinSession(0, GameSessionName, SearchResult);
		}

		return true;
	}
}

void UUTLocalPlayer::OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
	bPendingSession = false;

	UE_LOG(UT,Log, TEXT("----------- [OnJoinSessionComplete %i"), (Result == EOnJoinSessionCompleteResult::Success));

	// If we are trying to be crammed in to an existing session, we can just exit.
	if (bAttemptingForceJoin)
	{
		bAttemptingForceJoin = false;
		return;
	}

	ChatArchive.Empty();

	// If we successed, nothing else needs to be done.
	if (Result == EOnJoinSessionCompleteResult::Success)
	{
		FString ConnectionString;
		if ( OnlineSessionInterface->GetResolvedConnectString(SessionName, ConnectionString) )
		{
			if (QuickMatchJoinType != NAME_None)
			{
				if (QuickMatchJoinType == QuickMatchTypes::Deathmatch)
				{
					ConnectionString += TEXT("?QuickStart=DM");
				}
				else if (QuickMatchJoinType == QuickMatchTypes::CaptureTheFlag)
				{
					ConnectionString += TEXT("?QuickStart=CTF");
				}
			}
			QuickMatchJoinType = NAME_None;

			if (PendingFriendInviteFriendId != TEXT(""))
			{
				ConnectionString += FString::Printf(TEXT("?Friend=%s"), *PendingFriendInviteFriendId);
				PendingFriendInviteFriendId = TEXT("");
			}

			if (bWantsToFindMatch)
			{
				ConnectionString += TEXT("?RTM=1");
			}

			ConnectionString += FString::Printf(TEXT("?SpectatorOnly=%i"), bWantsToConnectAsSpectator ? 1 : 0);

			if (ConnectDesiredTeam >= 0)
			{
				ConnectionString += FString::Printf(TEXT("?Team=%i"), ConnectDesiredTeam);
			}

			FWorldContext &Context = GEngine->GetWorldContextFromWorldChecked(GetWorld());
			Context.LastURL.RemoveOption(TEXT("QuickStart"));
			
			PlayerController->ClientTravel(ConnectionString, ETravelType::TRAVEL_Partial,false);

			bWantsToFindMatch = false;
			bWantsToConnectAsSpectator = false;
			return;

		}
	}

	// Any failures, return to the main menu.
	bWantsToConnectAsSpectator = false;
	bWantsToFindMatch = false;
	QuickMatchJoinType = NAME_None;

	if (Result == EOnJoinSessionCompleteResult::AlreadyInSession)
	{
		MessageBox(NSLOCTEXT("MCPMessages", "OnlineError", "Online Error"), NSLOCTEXT("MCPMessages", "AlreadyInSession", "You are already in a session and can't join another."));
	}


	if (Result == EOnJoinSessionCompleteResult::SessionIsFull)
	{
		MessageBox(NSLOCTEXT("MCPMessages", "OnlineError", "Online Error"), NSLOCTEXT("MCPMessages", "SessionFull", "The session you are attempting to join is full."));
	}

	// Force back to the main menu.
	ReturnToMainMenu();
}

void UUTLocalPlayer::LeaveSession()
{
	TSharedPtr<FUniqueNetId> UserId = OnlineIdentityInterface->GetUniquePlayerId(0);
	if (UserId.IsValid() && OnlineSessionInterface->IsPlayerInSession(GameSessionName, *UserId))
	{
		OnEndSessionCompleteDelegate = OnlineSessionInterface->AddOnEndSessionCompleteDelegate_Handle(FOnEndSessionCompleteDelegate::CreateUObject(this, &UUTLocalPlayer::OnEndSessionComplete));
		OnlineSessionInterface->EndSession(GameSessionName);
	}
	else
	{
		if (bPendingLoginCreds)
		{
			Logout();
		}
	}

}

void UUTLocalPlayer::OnEndSessionComplete(FName SessionName, bool bWasSuccessful)
{
	OnlineSessionInterface->ClearOnEndSessionCompleteDelegate_Handle(OnEndSessionCompleteDelegate);
	OnDestroySessionCompleteDelegate = OnlineSessionInterface->AddOnDestroySessionCompleteDelegate_Handle(FOnDestroySessionCompleteDelegate::CreateUObject(this, &UUTLocalPlayer::OnDestroySessionComplete));
	OnlineSessionInterface->DestroySession(GameSessionName);
}

void UUTLocalPlayer::OnDestroySessionComplete(FName SessionName, bool bWasSuccessful)
{
	UE_LOG(UT,Log, TEXT("----------- [OnDestroySessionComplete %i"), bPendingSession);
	
	OnlineSessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(OnDestroySessionCompleteDelegate);

	if (bPendingLoginCreds)
	{
		Logout();
	}
	else if (bPendingSession)
	{
		bPendingSession = false;
		OnlineSessionInterface->JoinSession(0,GameSessionName,PendingSession);
	}

}

void UUTLocalPlayer::UpdatePresence(FString NewPresenceString, bool bAllowInvites, bool bAllowJoinInProgress, bool bAllowJoinViaPresence, bool bAllowJoinViaPresenceFriendsOnly)
{
	if (OnlineIdentityInterface.IsValid() && OnlineSessionInterface.IsValid() && OnlinePresenceInterface.IsValid())
	{
		TSharedPtr<FUniqueNetId> UserId = OnlineIdentityInterface->GetUniquePlayerId(GetControllerId());
		if (UserId.IsValid())
		{
			FOnlineSessionSettings* GameSettings = OnlineSessionInterface->GetSessionSettings(TEXT("Game"));
			if (GameSettings != NULL)
			{
				GameSettings->bAllowInvites = true;
				GameSettings->bAllowJoinInProgress = true;
				GameSettings->bAllowJoinViaPresence = true;
				GameSettings->bAllowJoinViaPresenceFriendsOnly = false;
				OnlineSessionInterface->UpdateSession(TEXT("Game"), *GameSettings, false);
			}

			TSharedPtr<FOnlineUserPresence> CurrentPresence;
			OnlinePresenceInterface->GetCachedPresence(*UserId, CurrentPresence);
			if (CurrentPresence.IsValid())
			{
				CurrentPresence->Status.StatusStr = NewPresenceString;
				OnlinePresenceInterface->SetPresence(*UserId, CurrentPresence->Status, IOnlinePresence::FOnPresenceTaskCompleteDelegate::CreateUObject(this, &UUTLocalPlayer::OnPresenceUpdated));
			}
			else
			{
				FOnlineUserPresenceStatus NewStatus;
				NewStatus.State = EOnlinePresenceState::Online;
				NewStatus.StatusStr = NewPresenceString;
				OnlinePresenceInterface->SetPresence(*UserId, NewStatus, IOnlinePresence::FOnPresenceTaskCompleteDelegate::CreateUObject(this, &UUTLocalPlayer::OnPresenceUpdated));
			}
		}
		else
		{
			LastPresenceUpdate = NewPresenceString;
			bLastAllowInvites = bAllowInvites;
		}
	}
}

bool UUTLocalPlayer::IsPlayerShowingSocialNotification() const
{
	return bShowSocialNotification;
}

void UUTLocalPlayer::OnPresenceUpdated(const FUniqueNetId& UserId, const bool bWasSuccessful)
{
	UE_LOG(UT,Verbose,TEXT("OnPresenceUpdated %s"), (bWasSuccessful ? TEXT("Successful") : TEXT("Failed")));
}

void UUTLocalPlayer::OnPresenceReceived(const FUniqueNetId& UserId, const TSharedRef<FOnlineUserPresence>& Presence)
{
	UE_LOG(UT,Verbose,TEXT("Presence Received %s %i %i"), *UserId.ToString(), Presence->bIsJoinable);
}

void UUTLocalPlayer::HandleFriendsJoinGame(const FUniqueNetId& FriendId, const FUniqueNetId& SessionId)
{
	JoinFriendSession(FriendId, SessionId);
}

bool UUTLocalPlayer::AllowFriendsJoinGame()
{
	// determine when to disable "join game" option in friends/chat UI
	return true;
}

void UUTLocalPlayer::HandleFriendsNotificationAvail(bool bAvailable)
{
	bShowSocialNotification = bAvailable;
}

void UUTLocalPlayer::HandleFriendsActionNotification(TSharedRef<FFriendsAndChatMessage> FriendsAndChatMessage)
{
	if (FriendsAndChatMessage->GetMessageType() == EFriendsRequestType::GameInvite ||
		FriendsAndChatMessage->GetMessageType() == EFriendsRequestType::FriendAccepted || 
		FriendsAndChatMessage->GetMessageType() == EFriendsRequestType::FriendInvite)
	{
		ShowToast(FText::FromString(FriendsAndChatMessage->GetMessage()));
	}
}

void UUTLocalPlayer::JoinFriendSession(const FUniqueNetId& FriendId, const FUniqueNetId& SessionId)
{
	UE_LOG(UT, Log, TEXT("##########################"));
	UE_LOG(UT, Log, TEXT("Joining a Friend Session"));
	UE_LOG(UT, Log, TEXT("##########################"));

	//@todo samz - use FindSessionById instead of FindFriendSession with a pending SessionId
	PendingFriendInviteSessionId = SessionId.ToString();
	PendingFriendInviteFriendId = FriendId.ToString();
	OnFindFriendSessionCompleteDelegate = OnlineSessionInterface->AddOnFindFriendSessionCompleteDelegate_Handle(0, FOnFindFriendSessionCompleteDelegate::CreateUObject(this, &UUTLocalPlayer::OnFindFriendSessionComplete));
	OnlineSessionInterface->FindFriendSession(0, FriendId);
}

void UUTLocalPlayer::OnFindFriendSessionComplete(int32 LocalUserNum, bool bWasSuccessful, const FOnlineSessionSearchResult& SearchResult)
{
	OnlineSessionInterface->ClearOnFindFriendSessionCompleteDelegate_Handle(LocalUserNum, OnFindFriendSessionCompleteDelegate);
	if (bWasSuccessful)
	{
		if (SearchResult.Session.SessionInfo.IsValid())
		{
			JoinSession(SearchResult, false);
		}
		else
		{
			PendingFriendInviteFriendId = TEXT("");
			MessageBox(NSLOCTEXT("MCPMessages", "OnlineError", "Online Error"), NSLOCTEXT("MCPMessages", "InvalidFriendSession", "Friend no longer in session."));
		}
	}
	else
	{
		PendingFriendInviteFriendId = TEXT("");
		MessageBox(NSLOCTEXT("MCPMessages", "OnlineError", "Online Error"), NSLOCTEXT("MCPMessages", "NoFriendSession", "Couldn't find friend session to join."));
	}
	PendingFriendInviteSessionId = FString();
}

uint32 UUTLocalPlayer::GetCountryFlag()
{
	if (CurrentProfileSettings)
	{
		return CurrentProfileSettings->CountryFlag;
	}
	if (PlayerController)
	{
		AUTPlayerState* PS = Cast<AUTPlayerState>(PlayerController->PlayerState);
		if (PS)
		{
			return PS->CountryFlag;
		}
	}
	return 0;
}

void UUTLocalPlayer::SetCountryFlag(uint32 NewFlag, bool bSave)
{
	if (CurrentProfileSettings)
	{
		CurrentProfileSettings->CountryFlag = NewFlag;
		if (bSave)
		{
			SaveProfileSettings();
		}
	}

	AUTPlayerController* PC = Cast<AUTPlayerController>(PlayerController);
	if (PC != NULL)
	{
		PC->ServerReceiveCountryFlag(NewFlag);
	}
}

#if !UE_SERVER

void UUTLocalPlayer::StartQuickMatch(FName QuickMatchType)
{
	if (IsLoggedIn() && OnlineSessionInterface.IsValid())
	{
		if (QuickMatchDialog.IsValid())
		{
			return;
		}

		if ( ServerBrowserWidget.IsValid() && ServerBrowserWidget->IsRefreshing())
		{
			MessageBox(NSLOCTEXT("Generic","RequestInProgressTitle","Busy"), NSLOCTEXT("Generic","RequestInProgressText","A server list request is already in progress.  Please wait for it to finish before attempting to quickmatch."));
			return;
		}

		if (OnlineSessionInterface.IsValid())
		{
			OnlineSessionInterface->CancelFindSessions();				
		}

		SAssignNew(QuickMatchDialog, SUTQuickMatch)
			.PlayerOwner(this)
			.QuickMatchType(QuickMatchType);

		if (QuickMatchDialog.IsValid())
		{
			GEngine->GameViewport->AddViewportWidgetContent(QuickMatchDialog.ToSharedRef(), 150);
			QuickMatchDialog->TellSlateIWantKeyboardFocus();
		}
	}
	else
	{
		MessageBox(NSLOCTEXT("Generic","LoginNeededTitle","Login Needed"), NSLOCTEXT("Generic","LoginNeededMessage","You need to login before you can do that."));
	}
}
void UUTLocalPlayer::CloseQuickMatch()
{
	if (QuickMatchDialog.IsValid())
	{
		GEngine->GameViewport->RemoveViewportWidgetContent(QuickMatchDialog.ToSharedRef());
		QuickMatchDialog.Reset();
	}
}

#endif

void UUTLocalPlayer::ShowConnectingDialog()
{
#if !UE_SERVER
	if (!ConnectingDialog.IsValid())
	{
		FDialogResultDelegate Callback;
		Callback.BindUObject(this, &UUTLocalPlayer::ConnectingDialogCancel);

		TSharedPtr<SUWDialog> NewDialog; // important to make sure the ref count stays until OpenDialog()
		SAssignNew(NewDialog, SUWMessageBox)
			.PlayerOwner(this)
			.DialogTitle(NSLOCTEXT("UT", "ConnectingTitle", "Connecting..."))
			.MessageText(NSLOCTEXT("UT", "ConnectingText", "Connecting to server, please wait..."))
			.ButtonMask(UTDIALOG_BUTTON_CANCEL)
			.OnDialogResult(Callback);

		ConnectingDialog = NewDialog;
		OpenDialog(NewDialog.ToSharedRef());
	}
#endif
}
void UUTLocalPlayer::CloseConnectingDialog()
{
#if !UE_SERVER
	if (ConnectingDialog.IsValid())
	{
		CloseDialog(ConnectingDialog.Pin().ToSharedRef());
	}
#endif
}
void UUTLocalPlayer::ConnectingDialogCancel(TSharedPtr<SCompoundWidget> Dialog, uint16 ButtonID)
{
#if !UE_SERVER
	GEngine->Exec(GetWorld(), TEXT("Cancel"));
#endif
}

bool UUTLocalPlayer::IsInSession()
{ 
	TSharedPtr<FUniqueNetId> UserId = OnlineIdentityInterface->GetUniquePlayerId(0);
	return (UserId.IsValid() && OnlineSessionInterface.IsValid() && OnlineSessionInterface->IsPlayerInSession(GameSessionName,*UserId));
}

void UUTLocalPlayer::ShowPlayerInfo(TWeakObjectPtr<AUTPlayerState> Target)
{
#if !UE_SERVER
	OpenDialog(SNew(SUWPlayerInfoDialog).PlayerOwner(this).TargetPlayerState(Target));
#endif
}

void UUTLocalPlayer::RequestFriendship(TSharedPtr<FUniqueNetId> FriendID)
{
	if (OnlineFriendsInterface.IsValid() && FriendID.IsValid())
	{
		OnlineFriendsInterface->SendInvite(0, *FriendID.Get(), EFriendsLists::ToString(EFriendsLists::Default));
	}
}

void UUTLocalPlayer::UpdateRedirect(const FString& FileURL, int32 NumBytes, float Progress, int32 NumFilesLeft)
{
	DownloadStatusText = FText::Format(NSLOCTEXT("UTLocalPlayer","DownloadStatusFormat","Downloading {0} Files: {1} ({2} / {3}) ...."), FText::AsNumber(NumFilesLeft), FText::FromString(FileURL), FText::AsNumber(NumBytes), FText::AsPercent(Progress));
	UE_LOG(UT,Verbose,TEXT("Redirect: %s %i [%f%%]"), *FileURL, NumBytes, Progress);
}

void UUTLocalPlayer::AccquireContent(TArray<FString>& ContentList)
{
	UUTGameViewportClient* UTGameViewport = Cast<UUTGameViewportClient>(ViewportClient);
	if (UTGameViewport)
	{
		for (int32 i = 0; i < ContentList.Num(); i++)
		{
			if (ContentList[i] != TEXT(""))
			{
				UTGameViewport->DownloadRedirect(ContentList[i]);
			}
		}
	}
}

FText UUTLocalPlayer::GetDownloadStatusText()
{
	return IsDownloadInProgress() ? DownloadStatusText : FText::GetEmpty();
}

bool UUTLocalPlayer::IsDownloadInProgress()
{
	UUTGameViewportClient* UTGameViewport = Cast<UUTGameViewportClient>(ViewportClient);
	return UTGameViewport ? UTGameViewport->IsDownloadInProgress() : false;
}

void UUTLocalPlayer::CancelDownload()
{
	UUTGameViewportClient* UTGameViewport = Cast<UUTGameViewportClient>(ViewportClient);
	if (UTGameViewport && UTGameViewport->IsDownloadInProgress())
	{
		UTGameViewport->CancelAllRedirectDownloads();		
	}
}

void UUTLocalPlayer::HandleNetworkFailureMessage(enum ENetworkFailure::Type FailureType, const FString& ErrorString)
{
	AUTBasePlayerController* BasePlayerController = Cast<AUTBasePlayerController>(PlayerController);
	if (BasePlayerController)
	{
		BasePlayerController->HandleNetworkFailureMessage(FailureType, ErrorString);
	}
}
void UUTLocalPlayer::OpenLoadout(bool bBuyMenu)
{
#if !UE_SERVER
	// Create the slate widget if it doesn't exist
	if (!LoadoutMenu.IsValid())
	{
		if (bBuyMenu)
		{
			SAssignNew(LoadoutMenu, SUTBuyMenu).PlayerOwner(this);
		}
		else
		{
			SAssignNew(LoadoutMenu, SUTLoadoutMenu).PlayerOwner(this);
		}

		if (LoadoutMenu.IsValid())
		{
			GEngine->GameViewport->AddViewportWidgetContent( SNew(SWeakWidget).PossiblyNullContent(LoadoutMenu.ToSharedRef()),60);
		}

		// Make it visible.
		if (LoadoutMenu.IsValid())
		{
			// Widget is already valid, just make it visible.
			LoadoutMenu->SetVisibility(EVisibility::Visible);
			LoadoutMenu->OnMenuOpened();

			if (PlayerController)
			{
				PlayerController->bShowMouseCursor = true;
				PlayerController->SetInputMode( FInputModeUIOnly() );
			}
		}
	}
#endif
}
void UUTLocalPlayer::CloseLoadout()
{
#if !UE_SERVER
	if (LoadoutMenu.IsValid())
	{
		GEngine->GameViewport->RemoveViewportWidgetContent(LoadoutMenu.ToSharedRef());
		LoadoutMenu->OnMenuClosed();
		LoadoutMenu.Reset();
		if (PlayerController)
		{
			PlayerController->bShowMouseCursor = false;
			PlayerController->SetInputMode(FInputModeGameOnly());
			PlayerController->SetPause(false);
		}

		FSlateApplication::Get().SetUserFocusToGameViewport(0, EFocusCause::SetDirectly);

	}
#endif
}

void UUTLocalPlayer::OpenMapVote(AUTGameState* GameState)
{
#if !UE_SERVER
	SAssignNew(MapVoteMenu,SUWMapVoteDialog).PlayerOwner(this).GameState(GameState);
	OpenDialog( MapVoteMenu.ToSharedRef(), 200 );
#endif
}
void UUTLocalPlayer::CloseMapVote()
{
#if !UE_SERVER
	if (MapVoteMenu.IsValid())
	{
		CloseDialog(MapVoteMenu.ToSharedRef());		
	}
#endif
}

void UUTLocalPlayer::OpenReplayWindow()
{
#if !UE_SERVER
	UDemoNetDriver* DemoDriver = GetWorld()->DemoNetDriver;
	if (DemoDriver)
	{
		if (!ReplayWindow.IsValid())
		{
			SAssignNew(ReplayWindow, SUTReplayWindow)
				.PlayerOwner(this)
				.DemoNetDriver(DemoDriver);

			if (ReplayWindow.IsValid())
			{
				GEngine->GameViewport->AddViewportWidgetContent(ReplayWindow.ToSharedRef(), -1);
			}

			if (ReplayWindow.IsValid())
			{
				ReplayWindow->SetVisibility(EVisibility::Visible);

				if (PlayerController)
				{
					PlayerController->bShowMouseCursor = true;
					PlayerController->SetInputMode(FInputModeGameAndUI().SetWidgetToFocus(ReplayWindow));
				}
			}
		}
	}
#endif
}

void UUTLocalPlayer::CloseReplayWindow()
{
#if !UE_SERVER
	if (ReplayWindow.IsValid())
	{
		GEngine->GameViewport->RemoveViewportWidgetContent(ReplayWindow.ToSharedRef());
		ReplayWindow.Reset();

		if (PlayerController)
		{
			PlayerController->bShowMouseCursor = false;
			PlayerController->SetInputMode(FInputModeGameAndUI());
		}
	}
#endif
}

void UUTLocalPlayer::ToggleReplayWindow()
{
#if !UE_SERVER
	if (IsReplay())
	{
		if (!ReplayWindow.IsValid())
		{
			OpenReplayWindow();
		}
		else
		{
			CloseReplayWindow();
		}
	}
#endif
}

bool UUTLocalPlayer::IsReplay()
{
	return (GetWorld()->DemoNetDriver != nullptr);
}

void UUTLocalPlayer::VerifyGameSession(const FString& ServerSessionId)
{
	if (OnlineSessionInterface.IsValid())
	{
		// Get our current Session Id.
		FNamedOnlineSession* Session = OnlineSessionInterface->GetNamedSession(FName(TEXT("Game")));
		if (Session == NULL || !Session->SessionInfo.IsValid() || Session->SessionInfo->GetSessionId().ToString() != ServerSessionId)
		{
			TSharedPtr<FUniqueNetId> UserId = OnlineIdentityInterface->GetUniquePlayerId(GetControllerId());
			if (UserId.IsValid())
			{
				TSharedPtr<FUniqueNetId> ServerId = MakeShareable(new FUniqueNetIdString(ServerSessionId));		
				TSharedPtr<FUniqueNetId> EmptyId = MakeShareable(new FUniqueNetIdString(""));				
				FOnSingleSessionResultCompleteDelegate CompletionDelegate;
				CompletionDelegate.BindUObject(this, &UUTLocalPlayer::OnFindSessionByIdComplete);
				OnlineSessionInterface->FindSessionById(*UserId, *ServerId, *EmptyId, CompletionDelegate);
			}
		}
	}
}

void UUTLocalPlayer::OnFindSessionByIdComplete(int32 LocalUserNum, bool bWasSucessful, const FOnlineSessionSearchResult& SearchResult)
{
	if (bWasSucessful)
	{
		bAttemptingForceJoin = true;
		OnlineSessionInterface->JoinSession(0, GameSessionName, SearchResult);
	}
}