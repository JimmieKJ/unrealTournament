// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTLocalPlayer.h"
#include "Online.h"
#include "OnlineSubsystemTypes.h"
#include "UTMenuGameMode.h"
#include "Slate/SUWindowsDesktop.h"
#include "Slate/SUWindowsMainMenu.h"
#include "Slate/SUWindowsMidGame.h"
#include "Slate/SUWServerBrowser.h"
#include "Slate/SUWMessageBox.h"
#include "Slate/SUWindowsStyle.h"
#include "Slate/SUWDialog.h"
#include "Slate/SUWInputBox.h"
#include "Slate/SUWLoginDialog.h"
#include "UTAnalytics.h"
#include "Runtime/Analytics/Analytics/Public/Analytics.h"
#include "Runtime/Analytics/Analytics/Public/Interfaces/IAnalyticsProvider.h"


UUTLocalPlayer::UUTLocalPlayer(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
}

void UUTLocalPlayer::InitializeOnlineSubsystem()
{
	OnlineSubsystem = IOnlineSubsystem::Get();
	if (OnlineSubsystem) OnlineIdentityInterface = OnlineSubsystem->GetIdentityInterface();

	if (OnlineIdentityInterface.IsValid())
	{
		OnLoginCompleteDelegate = FOnLoginCompleteDelegate::CreateUObject(this, &UUTLocalPlayer::OnLoginComplete);
		OnlineIdentityInterface->AddOnLoginCompleteDelegate(ControllerId, OnLoginCompleteDelegate);

		OnLoginStatusChangedDelegate = FOnLoginStatusChangedDelegate::CreateUObject(this, &UUTLocalPlayer::OnLoginStatusChanged);
		OnlineIdentityInterface->AddOnLoginStatusChangedDelegate(ControllerId, OnLoginStatusChangedDelegate);

		OnLogoutCompleteDelegate = FOnLogoutCompleteDelegate::CreateUObject(this, &UUTLocalPlayer::OnLogoutComplete);
		OnlineIdentityInterface->AddOnLogoutCompleteDelegate(ControllerId, OnLogoutCompleteDelegate);
	}
}

FString UUTLocalPlayer::GetNickname() const
{
	UUTGameUserSettings* Settings;
	Settings = Cast<UUTGameUserSettings>(GEngine->GetGameUserSettings());
	if (Settings) 
	{
		return Settings->GetPlayerName();
	}

	return TEXT("");
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
}

bool UUTLocalPlayer::IsMenuGame()
{
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
		if (true) // IsMenuGame())
		{
			SAssignNew(DesktopSlateWidget, SUWindowsMainMenu).PlayerOwner(this);
		}
		else
		{
			SAssignNew(DesktopSlateWidget, SUWindowsMidGame).PlayerOwner(this);
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
	}
#endif
}

#if !UE_SERVER
TSharedPtr<class SUWDialog>  UUTLocalPlayer::ShowMessage(FText MessageTitle, FText MessageText, uint16 Buttons, const FDialogResultDelegate& Callback)
{
	TSharedPtr<class SUWDialog> NewDialog;
	SAssignNew(NewDialog, SUWMessageBox)
		.PlayerOwner(this)
		.DialogTitle(MessageTitle)
		.MessageText(MessageText)
		.ButtonMask(Buttons)
		.OnDialogResult(Callback);


	OpenDialog( NewDialog.ToSharedRef() );
	return NewDialog;
}

void UUTLocalPlayer::OpenDialog(TSharedRef<SUWDialog> Dialog)
{
	GEngine->GameViewport->AddViewportWidgetContent(Dialog);
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
#endif

bool UUTLocalPlayer::IsLoggedIn() 
{ 
	return OnlineIdentityInterface.IsValid() && OnlineIdentityInterface->GetLoginStatus(ControllerId);
}


void UUTLocalPlayer::LoginOnline(FString EpicID, FString Auth, bool bIsRememberToken, bool bSilentlyFail)
{
	if (!IsLoggedIn() && OnlineIdentityInterface.IsValid())
	{
		if (EpicID == TEXT("")) EpicID = LastEpicIDLogin;

		// Save this for later.
		PendingLoginUserName = EpicID;
		bSilentLoginFail = bSilentlyFail;

		if (EpicID == TEXT("") || Auth == TEXT(""))
		{
			GetAuth();
			return;
		}

		FOnlineAccountCredentials AccountCreds(TEXT("epic"), EpicID, Auth);
		if (bIsRememberToken) AccountCreds.Type = TEXT("refresh");

		// Begin the Login Process....
		if (!OnlineIdentityInterface->Login(ControllerId, AccountCreds) )
		{
			// We should never fail here unless something has gone horribly wrong
			ShowMessage(NSLOCTEXT("MCPMessages","OnlineError","Online Error"), NSLOCTEXT("MCPMessages","UnknownLoginFailuire","Could not connect to the online subsystem.  Please check your connection and try again."), UTDIALOG_BUTTON_OK, NULL);
			return;
		}
	}
}

void UUTLocalPlayer::Logout()
{
	if (IsLoggedIn() && OnlineIdentityInterface.IsValid())
	{
		// Begin the Login Process....
		if (!OnlineIdentityInterface->Logout(ControllerId))
		{
			// We should never fail here unless something has gone horribly wrong
			ShowMessage(NSLOCTEXT("MCPMessages","OnlineError","Online Error"), NSLOCTEXT("MCPMessages","UnknownLogoutFailuire","Could not log out from the online subsystem.  Please check your connection and try again."), UTDIALOG_BUTTON_OK, NULL);
			return;
		}
	}
}
/*
void UUTLocalPlayer::LoadOnlineProfileSettings(UUTProfileSettings* ProfileSettingsToFill)
{
}

void UUTLocalPlayer::SaveOnlineProfileSettings(UUTProfileSettings* ProfileSettingsToSave)
{
}
*/

void UUTLocalPlayer::CleanUpOnlineSubSystyem()
{
	if (OnlineSubsystem)
	{
		if (OnlineIdentityInterface.IsValid())
		{
			OnlineIdentityInterface->ClearOnLoginCompleteDelegate(ControllerId, OnLoginCompleteDelegate);
			OnlineIdentityInterface->ClearOnLoginStatusChangedDelegate(ControllerId, OnLoginStatusChangedDelegate);
			OnlineIdentityInterface->ClearOnLogoutCompleteDelegate(ControllerId, OnLogoutCompleteDelegate);
		}
	}
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

		// We are online, pull the profile information/etc.
	}
	else if (!bSilentLoginFail)
	{
		GetAuth(true);
	}
}

void UUTLocalPlayer::GetAuth(bool bLastFailed)
{
#if !UE_SERVER

		FVector2D Size = bLastFailed ? FVector2D(510,270) : FVector2D(510,250);

		OpenDialog(	SNew(SUWLoginDialog)
					.OnDialogResult(FDialogResultDelegate::CreateUObject(this, &UUTLocalPlayer::AuthDialogClosed))
					.DialogSize(Size)
					.UserIDText(PendingLoginUserName)
					.ErrorText( bLastFailed ? NSLOCTEXT("MCPMessages","LoginFailure","(Bad Username or Password)") : FText::GetEmpty())
					.PlayerOwner(this));
#endif
}

void UUTLocalPlayer::OnLoginStatusChanged(int32 LocalUserNum, ELoginStatus::Type LoginStatus, const FUniqueNetId& UniqueID)
{
	UE_LOG(UT,Log,TEXT("***[LoginStatusChanged]*** - User %i - %i"), LocalUserNum, int32(LoginStatus));

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

void UUTLocalPlayer::PostInitProperties()
{
	Super::PostInitProperties();

	UUTLocalPlayer* Obj = GetClass()->GetDefaultObject<UUTLocalPlayer>();
	if (Obj != this)
	{
		// Initialize the Online Subsystem for this player
		InitializeOnlineSubsystem();

		FString LoginName = LastEpicIDLogin;
		FString Token = LastEpicRememberMeToken;

		FString CmdLineLoginName;
		FParse::Value(FCommandLine::Get(), TEXT("EpicID="), CmdLineLoginName);
		
		FString CmdLineToken = TEXT("");
		FParse::Value(FCommandLine::Get(), TEXT("EpicToken="), CmdLineToken);

		if (CmdLineLoginName != TEXT("")) LoginName = CmdLineLoginName;
		if (CmdLineToken != TEXT("")) Token = CmdLineToken;

		// We don't want to start the auto-logon process from the default object.
		if (!LoginName.IsEmpty() && LoginName != TEXT("") && !Token.IsEmpty() && Token != TEXT(""))
		{
			// We have enough credentials to auto-login.  So try it, but silently fail if we cant.
			LoginOnline(LoginName, Token, true, true);
		}
	}

}

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