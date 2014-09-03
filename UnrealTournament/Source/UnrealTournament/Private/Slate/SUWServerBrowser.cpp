// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "../Public/UnrealTournament.h"
#include "../Public/UTLocalPlayer.h"
#include "Slate.h"
#include "Slate/SlateGameResources.h"
#include "SUWindowsDesktop.h"
#include "SUWindowsStyle.h"
#include "SUWServerBrowser.h"
#include "Online.h"
#include "UTOnlineGameSearchBase.h"
#include "UTOnlineGameSettingsBase.h"
#include "OnlineSubsystemTypes.h"
#include "SUWMessageBox.h"
#include "SUWDialog.h"
#include "SUWInputBox.h"

void SUWServerBrowser::Construct(const FArguments& InArgs)
{
	PlayerOwner = InArgs._PlayerOwner;
	DesktopOwner = InArgs._DesktopOwner;

	TSharedRef<SScrollBar> ExternalScrollbar = SNew(SScrollBar);

	OnlineSubsystem = IOnlineSubsystem::Get();
	if (OnlineSubsystem) OnlineIdentityInterface = OnlineSubsystem->GetIdentityInterface();
	if (OnlineSubsystem) OnlineSessionInterface = OnlineSubsystem->GetSessionInterface();

	bool bIsLoggedIn = OnlineIdentityInterface.IsValid() && OnlineIdentityInterface->GetLoginStatus(0);

	this->ChildSlot
	[

		SNew( SVerticalBox )
		
		 //Login panel

		+SVerticalBox::Slot()
		.AutoHeight()
		.Padding( 5.0f )
		[
			SNew( SHorizontalBox )
			+SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				SNew(STextBlock).Text(NSLOCTEXT("SUWServerBrowser","UserName","User Name:"))
			]
			+SHorizontalBox::Slot().AutoWidth() .Padding(20,0,0,0) .VAlign(VAlign_Center)
			[
				SAssignNew(UserNameEditBox, SEditableTextBox)
				.OnTextCommitted(this, &SUWServerBrowser::OnTextCommited)
				.MinDesiredWidth(300.0f)
			]
			+SHorizontalBox::Slot().AutoWidth()
			.Padding(5.0f)
			[
				// Press rebuild to clear out the old data items and create the new ones (however many are specified by SEditableTextBox)
				SAssignNew(RefreshButton, SButton)
				.Text(bIsLoggedIn ? NSLOCTEXT("SUWServerBrowser","Refresh","Refresh") : NSLOCTEXT("SUWServerBrowser","Login","Login"))
				.OnClicked(this, &SUWServerBrowser::OnRefreshClick)
			]
		]


		// The Server Browser itself
		+SVerticalBox::Slot()
		.FillHeight(1)
		[
			SNew(SBox) .HeightOverride(500.0f)
			[
				SNew( SSplitter )
				+ SSplitter::Slot()
				. Value(1)
				[
					SNew(SBorder)
					[
						SNew(SHorizontalBox)
						+SHorizontalBox::Slot()
						.AutoWidth()
						[
							ExternalScrollbar
						]
						+SHorizontalBox::Slot()
						.FillWidth(1)
						[
							// The list view being tested
							SAssignNew( InternetServerList, SListView< TSharedPtr<FServerData> > )
							.ExternalScrollbar( ExternalScrollbar )
							// List view items are this tall
							.ItemHeight(24)
							// Tell the list view where to get its source data
							.ListItemsSource( &InternetServers)
							// When the list view needs to generate a widget for some data item, use this method
							.OnGenerateRow( this, &SUWServerBrowser::OnGenerateWidgetForList )
							.SelectionMode(ESelectionMode::Single)
							.HeaderRow
							(
								SNew(SHeaderRow)
								+ SHeaderRow::Column("ServerName") . DefaultLabel(NSLOCTEXT("SUWServerBrowser","ServerNameColumn", "Server Name"))
								+ SHeaderRow::Column("ServerIP") .DefaultLabel(NSLOCTEXT("SUWServerBrowser","ServerIPColumn", "IP"))
								+ SHeaderRow::Column("ServerGame") .DefaultLabel(NSLOCTEXT("SUWServerBrowser","ServerGameColumn", "Game"))
								+ SHeaderRow::Column("ServerMap") .DefaultLabel(NSLOCTEXT("SUWServerBrowser","ServerMapColumn", "Map"))
								+ SHeaderRow::Column("ServerNumPlayers") .DefaultLabel(NSLOCTEXT("SUWServerBrowser","ServerNumPlayerColumn", "Players"))
								+ SHeaderRow::Column("ServerNumSpecs") .DefaultLabel(NSLOCTEXT("SUWServerBrowser","ServerNumSpecsColumn", "Spectators"))
								+ SHeaderRow::Column("ServerVer") .DefaultLabel(NSLOCTEXT("SUWServerBrowser","ServerVerColumn", "Version"))
								+ SHeaderRow::Column("ServerPing") .DefaultLabel(NSLOCTEXT("SUWServerBrowser","ServerPingColumn", "Ping"))
							)
							.OnMouseButtonDoubleClick(this, &SUWServerBrowser::OnListMouseButtonDoubleClick)
						]
					]
				]
			]
		]

		 //Connect Panel

		+SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Fill)
		.Padding( 5.0f )
		[
			SNew( SHorizontalBox )
			+SHorizontalBox::Slot().HAlign(HAlign_Fill)
			.Padding(10.0f)
			[
				// Press rebuild to clear out the old data items and create the new ones (however many are specified by SEditableTextBox)
				SAssignNew(StatusText, STextBlock)
			]
			+SHorizontalBox::Slot().AutoWidth()
			.Padding(5.0f)
			[
				// Press rebuild to clear out the old data items and create the new ones (however many are specified by SEditableTextBox)
				SAssignNew(RefreshButton, SButton)
				.Text(NSLOCTEXT("SUWServerBrowser","Spectate","Spectate"))
				.OnClicked(this, &SUWServerBrowser::OnJoinClick,true)
			]
			+SHorizontalBox::Slot().AutoWidth() .Padding(20,0,0,0) .VAlign(VAlign_Center)
			[
				// Press rebuild to clear out the old data items and create the new ones (however many are specified by SEditableTextBox)
				SAssignNew(RefreshButton, SButton)
				.Text(NSLOCTEXT("SUWServerBrowser","Join","Join"))
				.OnClicked(this, &SUWServerBrowser::OnJoinClick,false)
			]
		]
	];

	UUTGameUserSettings* Settings;
	Settings = Cast<UUTGameUserSettings>(GEngine->GetGameUserSettings());
	if (Settings != NULL)
	{
		FString EpicID = Settings->GetEpicID();
		if (!EpicID.IsEmpty() && EpicID != TEXT(""))
		{
			UserNameEditBox->SetText(FText::FromString(EpicID));
			FString AuthToken = Settings->GetEpicAuth();
			if (!AuthToken.IsEmpty() && AuthToken != TEXT(""))
			{
				// Attempt to Login....
				if (!bIsLoggedIn)
				{
					AttemptLogin(EpicID, AuthToken,true);						
				}
			}
		}
	}
/*
	// Create some fake servers until I can hook up the server list
	InternetServers.Empty();
	for( int32 ItemIndex = 0; ItemIndex < 200; ++ItemIndex )
	{
		FString ServerName = FString::Printf(TEXT("Server %i"), ItemIndex);
		FString ServerIP = FString::Printf(TEXT("%i.%i.%i.%i"), FMath::RandRange(0,255),FMath::RandRange(0,255),FMath::RandRange(0,255),FMath::RandRange(0,255));
		FString ServerGame = TEXT("Capture the Flag");
		FString ServerMap = TEXT("CTF-Face");
		FString ServerVer = TEXT("Alpha");
		uint32 ServerPing = FMath::RandRange(35,1024);
		TSharedRef<FServerData> NewServer = FServerData::Make( ServerName, ServerIP, ServerGame, ServerMap, ServerVer, ServerPing);
		InternetServers.Add( NewServer );
	}
	InternetServerList->RequestListRefresh();
*/

	if (bIsLoggedIn)
	{
		RefreshServers();
	}
	else
	{
		SetBrowserState(FName(TEXT("NotLoggedIn")));
	}
}

/** @return Given a data item return a new widget to represent it in the ListView */
TSharedRef<ITableRow> SUWServerBrowser::OnGenerateWidgetForList( TSharedPtr<FServerData> InItem, const TSharedRef<STableViewBase>& OwnerTable )
{
	return	SNew( SServerBrowserRow, OwnerTable ).ServerData( InItem );
}

void SUWServerBrowser::SetBrowserState(FName NewBrowserState)
{
	BrowserState = NewBrowserState;
	if (BrowserState == FName(TEXT("NotLoggedIn"))) 
	{
		StatusText->SetText(NSLOCTEXT("SUWServerBrowser","NotLoggedIn","Login Required!"));
		RefreshButton->SetVisibility(EVisibility::All);
	}
	else if (BrowserState == FName(TEXT("Idle"))) 
	{
		StatusText->SetText(NSLOCTEXT("SUWServerBrowser","Idle","Idle!"));
		RefreshButton->SetVisibility(EVisibility::All);
	}
	else if (BrowserState == FName(TEXT("Auth"))) 
	{
		StatusText->SetText(NSLOCTEXT("SUWServerBrowser","Auth","Authenticating..."));
		RefreshButton->SetVisibility(EVisibility::Hidden);
	}
	else if (BrowserState == FName(TEXT("RequestingServerList"))) 
	{
		StatusText->SetText(NSLOCTEXT("SUWServerBrowser","RecieveingServer","Receiving Server List..."));
		RefreshButton->SetVisibility(EVisibility::Hidden);
	}
}



void SUWServerBrowser::OnTextCommited(const FText& NewText, ETextCommit::Type CommitType)
{
	if (CommitType == ETextCommit::OnEnter)
	{
		OnRefreshClick();
	}
}

FReply SUWServerBrowser::OnRefreshClick()
{
	bool bIsLoggedIn = OnlineIdentityInterface.IsValid() && OnlineIdentityInterface->GetLoginStatus(0);
	if (bIsLoggedIn)
	{
		RefreshServers();
	}
	else
	{
		// We need to get the password for this login id.  So Start that process.

		PlayerOwner->OpenDialog(
							SNew(SUWInputBox)
							.OnDialogResult(this, &SUWServerBrowser::CommitPassword)
							.PlayerOwner(PlayerOwner)
							.MessageTitle(NSLOCTEXT("UTGameViewportClient", "PasswordRequireTitle", "Password is Required"))
							.MessageText(NSLOCTEXT("SUWServerBrowser", "LoginPasswordRequiredText", "Please enter your password:"))
							);

	}
	return FReply::Handled();
}

void SUWServerBrowser::CommitPassword(const FString& InputText, bool bCancelled)
{
	if (!bCancelled)
	{
		AttemptLogin(UserNameEditBox->GetText().ToString(), *InputText);
	}
}

void SUWServerBrowser::AttemptLogin(FString UserID, FString Password, bool bIsToken)
{
	if (!OnLoginCompleteDelegate.IsBound())
	{
		OnLoginCompleteDelegate.BindSP(this, &SUWServerBrowser::OnLoginComplete);
		OnlineIdentityInterface->AddOnLoginCompleteDelegate(0, OnLoginCompleteDelegate);
	}

	UUTGameUserSettings* Settings;
	Settings = Cast<UUTGameUserSettings>(GEngine->GetGameUserSettings());
	if (Settings != NULL)
	{
		Settings->SetEpicID(UserID);
		Settings->SaveConfig();
	}


	FOnlineAccountCredentials AccountCreds(TEXT("epic"), UserID, Password);
	if (bIsToken) AccountCreds.Type = TEXT("refresh");
	if (!OnlineIdentityInterface->Login(0, AccountCreds) )
	{
		SetBrowserState(FName(TEXT("Auth")));
		PlayerOwner->ShowMessage(NSLOCTEXT("SUWServerBrowser","LoginFailedTitle","Login Failure"),
				NSLOCTEXT("SUWServerBrowser","LoginFailed","Could not log in.  Please check your credentials."), UTDIALOG_BUTTON_OK, NULL);

		return;
	}
}

void SUWServerBrowser::OnLoginComplete(int32 LocalUserNum, bool bWasSuccessful, const FUniqueNetId& UserId, const FString& Error)
{
	UUTGameUserSettings* Settings;
	Settings = Cast<UUTGameUserSettings>(GEngine->GetGameUserSettings());
	if (bWasSuccessful)
	{
		if (Settings != NULL)
		{
			TSharedPtr<FUserOnlineAccount> Account = OnlineIdentityInterface->GetUserAccount(UserId);
			if (Account.IsValid())
			{
				FString AuthToken;
				Account->GetAuthAttribute(TEXT("refresh_token"), AuthToken);
				Settings->SetEpicAuth(AuthToken);
				Settings->SaveConfig();
			}
		}

		RefreshServers();
	}
	else
	{
		if (Settings != NULL)
		{
			Settings->SetEpicAuth(TEXT(""));
			Settings->SaveConfig();
		}

		SetBrowserState(FName(TEXT("NotLoggedIn")));
		PlayerOwner->ShowMessage(NSLOCTEXT("SUWServerBrowser","LoginFailedTitle","Login Failure"),
				NSLOCTEXT("SUWServerBrowser","LoginFailed","Could not log in.  Please check your credentials and try again"), UTDIALOG_BUTTON_OK, NULL);

	}
}

void SUWServerBrowser::RefreshServers()
{
	bool bIsLoggedIn = OnlineIdentityInterface.IsValid() && OnlineIdentityInterface->GetLoginStatus(0);
	if (bIsLoggedIn && OnlineSessionInterface.IsValid())
	{
		SetBrowserState(FName(TEXT("RequestingServerList")));

		SearchSettings = MakeShareable(new FUTOnlineGameSearchBase(false));
		SearchSettings->MaxSearchResults = 10000;
		TSharedRef<FUTOnlineGameSearchBase> SearchSettingsRef = SearchSettings.ToSharedRef();

		if (!OnFindSessionCompleteDelegate.IsBound())
		{
			OnFindSessionCompleteDelegate.BindSP(this, &SUWServerBrowser::OnFindSessionsComplete);
			OnlineSessionInterface->AddOnFindSessionsCompleteDelegate(OnFindSessionCompleteDelegate);
		}

		OnlineSessionInterface->FindSessions(0, SearchSettingsRef);
	}
	else
	{
		SetBrowserState(FName(TEXT("NotLoggedIn")));	
	}
}

void SUWServerBrowser::OnFindSessionsComplete(bool bWasSuccessful)
{
	if (bWasSuccessful)
	{
		// See the server list
		InternetServers.Empty();
		if (SearchSettings->SearchResults.Num() > 0)
		{
			for (int32 ServerIndex = 0; ServerIndex < SearchSettings->SearchResults.Num(); ServerIndex++)
			{
				FOnlineSessionSearchResult Result = SearchSettings->SearchResults[ServerIndex];
				FString ServerName;
				Result.Session.SessionSettings.Get(SETTING_SERVERNAME,ServerName);
				FString ServerIP;
				OnlineSessionInterface->GetResolvedConnectString(Result,FName(TEXT("GamePort")), ServerIP);
				FString ServerGame;
				Result.Session.SessionSettings.Get(SETTING_GAMEMODE,ServerGame);
				FString ServerMap;
				Result.Session.SessionSettings.Get(SETTING_MAPNAME,ServerMap);
				int32 ServerNoPlayers = 0;
				Result.Session.SessionSettings.Get(SETTING_PLAYERSONLINE, ServerNoPlayers);
				int32 ServerNoSpecs = 0;
				Result.Session.SessionSettings.Get(SETTING_SPECTATORSONLINE, ServerNoSpecs);
				FString ServerVer = TEXT("Alpha");
				uint32 ServerPing = 0;
				TSharedRef<FServerData> NewServer = FServerData::Make( ServerName, ServerIP, ServerGame, ServerMap, ServerNoPlayers, ServerNoSpecs, ServerVer, ServerPing);
				InternetServers.Add( NewServer );
			}
		}
		InternetServerList->RequestListRefresh();
	}

	SetBrowserState(FName(TEXT("Idle")));	

}

void SUWServerBrowser::OnListMouseButtonDoubleClick(TSharedPtr<FServerData> SelectedServer)
{
	ConnectTo(*SelectedServer,false);
}

FReply SUWServerBrowser::OnJoinClick(bool bSpectate)
{

	TArray<TSharedPtr<FServerData>> SelectedItems = InternetServerList->GetSelectedItems();
	if (SelectedItems.Num() > 0)
	{
		ConnectTo(*SelectedItems[0],bSpectate);
	}
	return FReply::Handled();
}
void SUWServerBrowser::ConnectTo(FServerData ServerData,bool bSpectate)
{
	PlayerOwner->HideMenu();
	FString Command = FString::Printf(TEXT("open %s"), *ServerData.IP);
	PlayerOwner->Exec(PlayerOwner->GetWorld(), *Command, *GLog);
}