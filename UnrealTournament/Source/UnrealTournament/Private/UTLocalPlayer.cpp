// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTLocalPlayer.h"
#include "UTCharacter.h"
#include "Online.h"
#include "OnlineSubsystemTypes.h"
#include "UTMenuGameMode.h"
#include "UTProfileSettings.h"
#include "Slate/SUWindowsDesktop.h"
#include "Slate/SUWindowsMainMenu.h"
#include "Slate/SUWindowsMidGame.h"
#include "Slate/Panels/SUWServerBrowser.h"
#include "Slate/Panels/SUWStatsViewer.h"
#include "Slate/Panels/SUWCreditsPanel.h"
#include "Slate/SUWMessageBox.h"
#include "Slate/SUWindowsStyle.h"
#include "Slate/SUWDialog.h"
#include "Slate/SUWToast.h"
#include "Slate/SUWInputBox.h"
#include "Slate/SUWLoginDialog.h"
#include "Slate/SUWPlayerSettingsDialog.h"
#include "Slate/SUWHUDSettingsDialog.h"
#include "Slate/SUWFriendsPopup.h"
#include "UTAnalytics.h"
#include "FriendsAndChat.h"
#include "Runtime/Analytics/Analytics/Public/Analytics.h"
#include "Runtime/Analytics/Analytics/Public/Interfaces/IAnalyticsProvider.h"


UUTLocalPlayer::UUTLocalPlayer(const class FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bInitialSignInAttempt = true;
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
	}

	if (OnlineIdentityInterface.IsValid())
	{
		OnLoginCompleteDelegate = FOnLoginCompleteDelegate::CreateUObject(this, &UUTLocalPlayer::OnLoginComplete);
		OnlineIdentityInterface->AddOnLoginCompleteDelegate(GetControllerId(), OnLoginCompleteDelegate);

		OnLoginStatusChangedDelegate = FOnLoginStatusChangedDelegate::CreateUObject(this, &UUTLocalPlayer::OnLoginStatusChanged);
		OnlineIdentityInterface->AddOnLoginStatusChangedDelegate(GetControllerId(), OnLoginStatusChangedDelegate);

		OnLogoutCompleteDelegate = FOnLogoutCompleteDelegate::CreateUObject(this, &UUTLocalPlayer::OnLogoutComplete);
		OnlineIdentityInterface->AddOnLogoutCompleteDelegate(GetControllerId(), OnLogoutCompleteDelegate);
	}

	if (OnlineUserCloudInterface.IsValid())
	{

	    OnReadUserFileCompleteDelegate = FOnReadUserFileCompleteDelegate::CreateUObject(this, &UUTLocalPlayer::OnReadUserFileComplete);
		OnlineUserCloudInterface->AddOnReadUserFileCompleteDelegate(OnReadUserFileCompleteDelegate);
		
	    OnWriteUserFileCompleteDelegate = FOnWriteUserFileCompleteDelegate::CreateUObject(this, &UUTLocalPlayer::OnWriteUserFileComplete);
		OnlineUserCloudInterface->AddOnWriteUserFileCompleteDelegate(OnWriteUserFileCompleteDelegate);

		OnDeleteUserFileCompleteDelegate = FOnDeleteUserFileCompleteDelegate::CreateUObject(this, &UUTLocalPlayer::OnDeleteUserFileComplete);
		OnlineUserCloudInterface->AddOnDeleteUserFileCompleteDelegate(OnDeleteUserFileCompleteDelegate);

	}

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
			if (PlayerController->MyHUD)
			{
				PlayerController->MyHUD->bShowHUD = false;
			}

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
	if (DesktopSlateWidget.IsValid())
	{
		GEngine->GameViewport->RemoveViewportWidgetContent(DesktopSlateWidget.ToSharedRef());
		DesktopSlateWidget->OnMenuClosed();
		DesktopSlateWidget.Reset();
		if (PlayerController)
		{
			PlayerController->bShowMouseCursor = false;
			if (PlayerController->MyHUD)
			{
				PlayerController->MyHUD->bShowHUD = true;
			}
			PlayerController->SetInputMode(FInputModeGameOnly());
			PlayerController->SetPause(false);
		}

		FSlateApplication::Get().SetUserFocusToGameViewport(0, EFocusCause::SetDirectly);

	}
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
		SAssignNew(ServerBrowserWidget, SUWServerBrowser)
			.PlayerOwner(this);
	}

	return ServerBrowserWidget;
}

TSharedPtr<class SUWStatsViewer> UUTLocalPlayer::GetStatsViewer()
{
	if (!StatsViewerWidget.IsValid())
	{
		SAssignNew(StatsViewerWidget, SUWStatsViewer)
			.PlayerOwner(this);
	}

	return StatsViewerWidget;
}

TSharedPtr<class SUWCreditsPanel> UUTLocalPlayer::GetCreditsPanel()
{
	if (!CreditsPanelWidget.IsValid())
	{
		SAssignNew(CreditsPanelWidget, SUWCreditsPanel)
			.PlayerOwner(this);
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
	if (!IsLoggedIn() && OnlineIdentityInterface.IsValid())
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


void UUTLocalPlayer::CleanUpOnlineSubSystyem()
{
	if (OnlineSubsystem)
	{
		if (OnlineIdentityInterface.IsValid())
		{
			OnlineIdentityInterface->ClearOnLoginCompleteDelegate(GetControllerId(), OnLoginCompleteDelegate);
			OnlineIdentityInterface->ClearOnLoginStatusChangedDelegate(GetControllerId(), OnLoginStatusChangedDelegate);
			OnlineIdentityInterface->ClearOnLogoutCompleteDelegate(GetControllerId(), OnLogoutCompleteDelegate);
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
			Account->GetAuthAttribute(TEXT("refresh_token"), RememberMeToken);
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
		GetAuth(true);
	}
}

void UUTLocalPlayer::GetAuth(bool bLastFailed)
{
#if !UE_SERVER
	if (GetWorld()->IsPlayInEditor())
	{
		return;
	}

		FVector2D Size = FVector2D(510,300);

		OpenDialog(	SNew(SUWLoginDialog)
					.OnDialogResult(FDialogResultDelegate::CreateUObject(this, &UUTLocalPlayer::AuthDialogClosed))
					.DialogSize(Size)
					.UserIDText(PendingLoginUserName)
					.ErrorText( bLastFailed ? NSLOCTEXT("MCPMessages","LoginFailure","(Bad Username or Password)") : FText::GetEmpty())
					.PlayerOwner(this));
#endif
}

void UUTLocalPlayer::OnLoginStatusChanged(int32 LocalUserNum, ELoginStatus::Type PreviousLoginStatus, ELoginStatus::Type LoginStatus, const FUniqueNetId& UniqueID)
{
	UE_LOG(UT,Verbose,TEXT("***[LoginStatusChanged]*** - User %i - %i"), LocalUserNum, int32(LoginStatus));

	// If we have logged out, or started using the local profile, then clear the online profile.
	if (LoginStatus == ELoginStatus::NotLoggedIn || LoginStatus == ELoginStatus::UsingLocalProfile)
	{
		CurrentProfileSettings = NULL;
	}
	else if (LoginStatus == ELoginStatus::LoggedIn)
	{
		ReadELOFromCloud();
	}

	for (int32 i=0; i< PlayerLoginStatusChangedListeners.Num(); i++)
	{
		PlayerLoginStatusChangedListeners[i].ExecuteIfBound(this, LoginStatus, UniqueID);
	}
}

void UUTLocalPlayer::OnLogoutComplete(int32 LocalUserNum, bool bWasSuccessful)
{
	UE_LOG(UT,Log,TEXT("***[Logout Complete]*** - User %i"), LocalUserNum);
	// TO-DO: Add a Toast system for displaying stuff like this

}

#if !UE_SERVER

void UUTLocalPlayer::AuthDialogClosed(TSharedPtr<SCompoundWidget> Widget, uint16 ButtonID)
{
	if (ButtonID != UTDIALOG_BUTTON_CANCEL)
	{
		TSharedPtr<SUWLoginDialog> Login = StaticCastSharedPtr<SUWLoginDialog>(Widget);
		if (Login.IsValid())
		{
			LoginOnline(Login->GetEpicID(), Login->GetPassword(),false);
		}
	}
	else
	{
		PendingLoginUserName = TEXT("");
	}
}

#endif

void UUTLocalPlayer::AddPlayerLoginStatusChangedDelegate(FPlayerOnlineStatusChangedDelegate NewDelegate)
{
	int TargetIndex = -1;
	for (int i=0;i<PlayerLoginStatusChangedListeners.Num();i++)
	{
		if (PlayerLoginStatusChangedListeners[i] == NewDelegate) return;	// Already in the list
		if (!PlayerLoginStatusChangedListeners[i].IsBound())
		{
			TargetIndex = i;
		}
	}

	if (TargetIndex >=0 && TargetIndex < PlayerLoginStatusChangedListeners.Num() )
	{
		PlayerLoginStatusChangedListeners[TargetIndex] = NewDelegate;
	}
	else
	{
		PlayerLoginStatusChangedListeners.Add(NewDelegate);
	}
}

void UUTLocalPlayer::ClearPlayerLoginStatusChangedDelegate(FPlayerOnlineStatusChangedDelegate Delegate)
{
	PlayerLoginStatusChangedListeners.Remove(Delegate);
}


void UUTLocalPlayer::ShowToast(FText ToastText)
{
#if !UE_SERVER

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
		FString AccountUserName = OnlineIdentityInterface->GetPlayerNickname(GetControllerId());
		FString ProfileFilename = FString::Printf(TEXT("%s.user.profile"), *AccountUserName);
		return ProfileFilename;
	}

	return TEXT("local.user.profile");
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
		OnlineUserCloudInterface->ReadUserFile(*UserID, GetProfileFilename() );
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
		OnlineUserCloudInterface->DeleteUserFile(*UserID, GetProfileFilename(), true, true);
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

		if (bWasSuccessful)	
		{
			// Create the current profile.
			if (CurrentProfileSettings == NULL)
			{
				CurrentProfileSettings = ConstructObject<UUTProfileSettings>(UUTProfileSettings::StaticClass(), GetTransientPackage());
			}

			TArray<uint8> FileContents;
			OnlineUserCloudInterface->GetFileContents(InUserId, FileName, FileContents);
			
			// Serialize the object
			FMemoryReader MemoryReader(FileContents, true);
			FObjectAndNameAsStringProxyArchive Ar(MemoryReader, false);
			CurrentProfileSettings->Serialize(Ar);

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
			CurrentProfileSettings = ConstructObject<UUTProfileSettings>(UUTProfileSettings::StaticClass(), GetTransientPackage());
		}

		PlayerNickname = GetAccountDisplayName().ToString();
		SaveConfig();
		SaveProfileSettings();

#if !UE_SERVER
		FText WelcomeMessage = FText::Format(NSLOCTEXT("UTLocalPlayer","Welcome","This is your first time logging in so we have set your player name to '{0}'.  Would you like to change it now?"), GetAccountDisplayName());
		ShowMessage(NSLOCTEXT("UTLocalPlayer", "WelcomeTitle", "Welcome to Unreal Tournament"), WelcomeMessage, UTDIALOG_BUTTON_YES + UTDIALOG_BUTTON_NO, FDialogResultDelegate::CreateUObject(this, &UUTLocalPlayer::WelcomeDialogResult),FVector2D(0.25,0.25));
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
		OpenDialog(SNew(SUWPlayerSettingsDialog).PlayerOwner(this).DialogTitle(NSLOCTEXT("SUWindowsDesktop","PlayerSettings","Player Settings")));			
	}
}
#endif

void UUTLocalPlayer::SaveProfileSettings()
{
	if ( CurrentProfileSettings != NULL && IsLoggedIn() )
	{
		CurrentProfileSettings->GatherAllSettings(this);
		CurrentProfileSettings->SettingsRevisionNum = CURRENT_PROFILESETTINGS_VERSION;

		// Build a blob of the profile contents
		TArray<uint8> FileContents;
		FMemoryWriter MemoryWriter(FileContents, true);
		FObjectAndNameAsStringProxyArchive Ar(MemoryWriter, false);
		CurrentProfileSettings->Serialize(Ar);

		// Save the blob to the cloud
		TSharedPtr<FUniqueNetId> UserID = OnlineIdentityInterface->GetUniquePlayerId(GetControllerId());
		OnlineUserCloudInterface->WriteUserFile(*UserID, GetProfileFilename(), FileContents);
	}
}

void UUTLocalPlayer::OnWriteUserFileComplete(bool bWasSuccessful, const FUniqueNetId& InUserId, const FString& FileName)
{
	if (bWasSuccessful)
	{
		FText Saved = NSLOCTEXT("MCP", "ProfileSaved", "Profile Saved");
		ShowToast(Saved);
	}
	else
	{
#if !UE_SERVER
		// Should give a warning here if it fails.
		ShowMessage(NSLOCTEXT("MCPMessages", "ProfileSaveErrorTitle", "An Error has occured"), NSLOCTEXT("MCPMessages", "ProfileSaveErrorText", "UT could not save your profile with the MCP.  Your settings may be lost."), UTDIALOG_BUTTON_OK, NULL);
#endif
	}
}

void UUTLocalPlayer::SetNickname(FString NewName)
{
	PlayerNickname = NewName;
	SaveConfig();
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
	OnlineUserCloudInterface->ReadUserFile(*UserId, GetStatsFilename());
}

void UUTLocalPlayer::UpdateBaseELOFromCloudData()
{
	TArray<uint8> FileContents;
	TSharedPtr<FUniqueNetId> UserId = OnlineIdentityInterface->GetUniquePlayerId(GetControllerId());
	if (OnlineUserCloudInterface->GetFileContents(*UserId, GetStatsFilename(), FileContents))
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
				StatsJson->TryGetNumberField(TEXT("MatchesPlayed"), MatchesPlayed);
			}
		}
	}
}

int UUTLocalPlayer::GetBaseELORank()
{

	// we can do whatever we want here.  

	int32 Cnt = 0;
	int32 Total = 0;
	if (DUEL_ELO > 0) { Cnt++; Total += DUEL_ELO; }
	if (TDM_ELO > 0) { Cnt++; Total += TDM_ELO; }

	if (Cnt > 0)
	{
		return int32( float(Total) / float(Cnt) );
	}
							
	return 1500;
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
					.WidthOverride(400)
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
								.Text(NSLOCTEXT("Loading","LoadingContent","Loading Content..."))
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

TSharedPtr<SWidget> UUTLocalPlayer::GetFriendsPopup()
{
	if (!FriendsMenu.IsValid())
	{
		SAssignNew(FriendsMenu, SUWFriendsPopup)
			.PlayerOwner(this);
	}

	return FriendsMenu;
}


#endif
