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
#include "UTGameEngine.h"

/** List Sort helpers */

struct FCompareServerByName		{FORCEINLINE bool operator()( const TSharedPtr< FServerData > A, const TSharedPtr< FServerData > B ) const {return ( A->Name < B->Name);}};
struct FCompareServerByNameDesc	{FORCEINLINE bool operator()( const TSharedPtr< FServerData > A, const TSharedPtr< FServerData > B ) const {return ( A->Name > B->Name);}};

struct FCompareServerByIP		{FORCEINLINE bool operator()( const TSharedPtr< FServerData > A, const TSharedPtr< FServerData > B ) const {return ( A->IP < B->IP);}};
struct FCompareServerByIPDesc	{FORCEINLINE bool operator()( const TSharedPtr< FServerData > A, const TSharedPtr< FServerData > B ) const {return ( A->IP > B->IP);}};

struct FCompareServerByGameMode		{FORCEINLINE bool operator()( const TSharedPtr< FServerData > A, const TSharedPtr< FServerData > B ) const {return ( A->GameMode > B->GameMode);}};
struct FCompareServerByGameModeDesc	{FORCEINLINE bool operator()( const TSharedPtr< FServerData > A, const TSharedPtr< FServerData > B ) const {return ( A->GameMode > B->GameMode);}};

struct FCompareServerByMap		{FORCEINLINE bool operator()( const TSharedPtr< FServerData > A, const TSharedPtr< FServerData > B ) const	{ return ( A->Map < B->Map); }};
struct FCompareServerByMapDesc	{FORCEINLINE bool operator()( const TSharedPtr< FServerData > A, const TSharedPtr< FServerData > B ) const	{ return ( A->Map > B->Map); }};

struct FCompareServerByNumPlayers		{FORCEINLINE bool operator()( const TSharedPtr< FServerData > A, const TSharedPtr< FServerData > B ) const { return ( A->NumPlayers < B->NumPlayers);}};
struct FCompareServerByNumPlayersDesc	{FORCEINLINE bool operator()( const TSharedPtr< FServerData > A, const TSharedPtr< FServerData > B ) const { return ( A->NumPlayers > B->NumPlayers);}};

struct FCompareServerByNumSpectators		{FORCEINLINE bool operator()( const TSharedPtr< FServerData > A, const TSharedPtr< FServerData > B ) const {return ( A->NumPlayers < B->NumPlayers);}};
struct FCompareServerByNumSpectatorsDesc	{FORCEINLINE bool operator()( const TSharedPtr< FServerData > A, const TSharedPtr< FServerData > B ) const {return ( A->NumPlayers > B->NumPlayers);}};

struct FCompareServerByVersion		{FORCEINLINE bool operator()( const TSharedPtr< FServerData > A, const TSharedPtr< FServerData > B ) const {return ( A->Version < B->Version);}};
struct FCompareServerByVersionDesc	{FORCEINLINE bool operator()( const TSharedPtr< FServerData > A, const TSharedPtr< FServerData > B ) const {return ( A->Version > B->Version);}};

struct FCompareServerByPing		{FORCEINLINE bool operator()( const TSharedPtr< FServerData > A, const TSharedPtr< FServerData > B ) const {return ( A->Ping < B->Ping);	}};
struct FCompareServerByPingDesc	{FORCEINLINE bool operator()( const TSharedPtr< FServerData > A, const TSharedPtr< FServerData > B ) const {return ( A->Ping > B->Ping);	}};



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
		SNew(SOverlay)
		+ SOverlay::Slot()
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.VAlign(VAlign_Fill)
			.HAlign(HAlign_Fill)
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.HAlign(HAlign_Fill)
				[
					SNew(SImage)
					.Image(SUWindowsStyle::Get().GetBrush("UWindows.Standard.Backdrop"))
				]
			]
		]

		+SOverlay::Slot()
		[
			SNew( SVerticalBox )
		
			 //Login panel

			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding( 25.0f, 5.0f, 5.0f, 5.0f )
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
					SNew(SBox)
					.VAlign(VAlign_Center)
					[
						// Press rebuild to clear out the old data items and create the new ones (however many are specified by SEditableTextBox)
						SAssignNew(RefreshButton, SButton)
						.Text(bIsLoggedIn ? NSLOCTEXT("SUWServerBrowser","Refresh","Refresh") : NSLOCTEXT("SUWServerBrowser","Login","Login"))
						.OnClicked(this, &SUWServerBrowser::OnRefreshClick)
					]
				]
				+SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					SNew(STextBlock).Text(NSLOCTEXT("SUWServerBrowser","GameFilter","Game Mode:"))
				]
				+SHorizontalBox::Slot().AutoWidth()
				.Padding(5.0f)
				[
					SAssignNew(GameFilter, SComboButton)
					.Method(SMenuAnchor::UseCurrentWindow)
					.HasDownArrow(false)
					.ButtonStyle(SUWindowsStyle::Get(), "UWindows.Standard.MenuButton")
					.ButtonContent()
					[
						SAssignNew(GameFilterText, STextBlock)
						.TextStyle(SUWindowsStyle::Get(), "UWindows.Standard.MainMenuButton.TextStyle")
					]
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
							.Padding(5,0,5,0)
							.FillWidth(1)
							[
								// The list view being tested
								SAssignNew( InternetServerList, SListView< TSharedPtr<FServerData> > )
								// List view items are this tall
								.ItemHeight(24)
								// Tell the list view where to get its source data
								.ListItemsSource( &FilteredServers)
								// When the list view needs to generate a widget for some data item, use this method
								.OnGenerateRow( this, &SUWServerBrowser::OnGenerateWidgetForList )
								.SelectionMode(ESelectionMode::Single)

								.HeaderRow
								(
									SNew(SHeaderRow) 
									.Style(SUWindowsStyle::Get(),"UWindows.Standard.ServerBrowser.Header")

									+ SHeaderRow::Column("ServerName").OnSort(this, &SUWServerBrowser::OnSort)
											.HeaderContent()
											[
												SNew(STextBlock)
													.Text(NSLOCTEXT("SUWServerBrowser","ServerNameColumn", "Server Name"))
													.ToolTipText( NSLOCTEXT("SUWServerBrowser","ServerNameColumnToolTip", "The name of this server.") )
													.TextStyle(SUWindowsStyle::Get(), "UWindows.Standard.ServerBrowser.Header.TextStyle")
											]

									+ SHeaderRow::Column("ServerIP") .DefaultLabel(NSLOCTEXT("SUWServerBrowser","ServerIPColumn", "IP")) .HAlignCell(HAlign_Center) .HAlignHeader(HAlign_Center).OnSort(this, &SUWServerBrowser::OnSort)
											.HeaderContent()
											[
												SNew(STextBlock)
													.Text(NSLOCTEXT("SUWServerBrowser","ServerIPColumn", "IP"))
													.ToolTipText( NSLOCTEXT("SUWServerBrowser","ServerIPColumnToolTip", "This server's IP address.") )
													.TextStyle(SUWindowsStyle::Get(), "UWindows.Standard.ServerBrowser.Header.TextStyle")
											]

									+ SHeaderRow::Column("ServerGame") .DefaultLabel(NSLOCTEXT("SUWServerBrowser","ServerGameColumn", "Game")).HAlignCell(HAlign_Center) .HAlignHeader(HAlign_Center).OnSort(this, &SUWServerBrowser::OnSort)
											.HeaderContent()
											[
												SNew(STextBlock)
													.Text(NSLOCTEXT("SUWServerBrowser","ServerGameColumn", "Game"))
													.ToolTipText( NSLOCTEXT("SUWServerBrowser","ServerGameColumnToolTip", "The Game type.") )
													.TextStyle(SUWindowsStyle::Get(), "UWindows.Standard.ServerBrowser.Header.TextStyle")
											]

									+ SHeaderRow::Column("ServerMap") .DefaultLabel(NSLOCTEXT("SUWServerBrowser","ServerMapColumn", "Map")).HAlignCell(HAlign_Center) .HAlignHeader(HAlign_Center).OnSort(this, &SUWServerBrowser::OnSort)
											.HeaderContent()
											[
												SNew(STextBlock)
													.Text(NSLOCTEXT("SUWServerBrowser","ServerMapColumn", "Map"))
													.ToolTipText( NSLOCTEXT("SUWServerBrowser","ServerMapColumnToolTip", "The current map.") )
													.TextStyle(SUWindowsStyle::Get(), "UWindows.Standard.ServerBrowser.Header.TextStyle")
											]

									+ SHeaderRow::Column("ServerNumPlayers") .DefaultLabel(NSLOCTEXT("SUWServerBrowser","ServerNumPlayerColumn", "Players")).HAlignCell(HAlign_Center) .HAlignHeader(HAlign_Center).OnSort(this, &SUWServerBrowser::OnSort)
											.HeaderContent()
											[
												SNew(STextBlock)
													.Text(NSLOCTEXT("SUWServerBrowser","ServerNumPlayerColumn", "Players"))
													.ToolTipText( NSLOCTEXT("SUWServerBrowser","ServerNumPlayerColumnToolTip", "The # of Players on this server.") )
													.TextStyle(SUWindowsStyle::Get(), "UWindows.Standard.ServerBrowser.Header.TextStyle")
											]
							
									+ SHeaderRow::Column("ServerNumSpecs") .DefaultLabel(NSLOCTEXT("SUWServerBrowser","ServerNumSpecsColumn", "Spectators")).HAlignCell(HAlign_Center) .HAlignHeader(HAlign_Center).OnSort(this, &SUWServerBrowser::OnSort)
											.HeaderContent()
											[
												SNew(STextBlock)
													.Text(NSLOCTEXT("SUWServerBrowser","ServerNumSpecsColumn", "Spectators"))
													.ToolTipText( NSLOCTEXT("SUWServerBrowser","ServerNumSpecsColumnToolTip", "The # of spectators on this server.") )
													.TextStyle(SUWindowsStyle::Get(), "UWindows.Standard.ServerBrowser.Header.TextStyle")
											]
									+ SHeaderRow::Column("ServerVer") .DefaultLabel(NSLOCTEXT("SUWServerBrowser","ServerVerColumn", "Version")).HAlignCell(HAlign_Center) .HAlignHeader(HAlign_Center).OnSort(this, &SUWServerBrowser::OnSort)
											.HeaderContent()
											[
												SNew(STextBlock)
													.Text(NSLOCTEXT("SUWServerBrowser","ServerVerColumn", "Version"))
													.ToolTipText( NSLOCTEXT("SUWServerBrowser","ServerVerColumnToolTip", "The version of UT this server is running.") )
													.TextStyle(SUWindowsStyle::Get(), "UWindows.Standard.ServerBrowser.Header.TextStyle")
											]
								
									+ SHeaderRow::Column("ServerPing") .DefaultLabel(NSLOCTEXT("SUWServerBrowser","ServerPingColumn", "Ping")).HAlignCell(HAlign_Right) .HAlignHeader(HAlign_Right).OnSort(this, &SUWServerBrowser::OnSort)
											.HeaderContent()
											[
												SNew(STextBlock)
													.Text(NSLOCTEXT("SUWServerBrowser","ServerPingColumn", "Ping"))
													.ToolTipText( NSLOCTEXT("SUWServerBrowser","ServerPingColumnToolTip", "Your connection speed to the server.") )
													.TextStyle(SUWindowsStyle::Get(), "UWindows.Standard.ServerBrowser.Header.TextStyle")
											]
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
				+SHorizontalBox::Slot().AutoWidth() .Padding(20,0,20,0) .VAlign(VAlign_Center)
				[
					// Press rebuild to clear out the old data items and create the new ones (however many are specified by SEditableTextBox)
					SAssignNew(RefreshButton, SButton)
					.Text(NSLOCTEXT("SUWServerBrowser","Join","Join"))
					.OnClicked(this, &SUWServerBrowser::OnJoinClick,false)
				]
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

	bDescendingSort = false;
	CurrentSortColumn = FName(TEXT("ServerPing"));



	// Create some fake servers until I can hook up the server list
	InternetServers.Empty();
	LanServers.Empty();
/*
	for( int32 ItemIndex = 0; ItemIndex < 200; ++ItemIndex )
	{
		FString ServerName = FString::Printf(TEXT("Server %i"), ItemIndex);
		FString ServerIP = FString::Printf(TEXT("%i.%i.%i.%i"), FMath::RandRange(0,255),FMath::RandRange(0,255),FMath::RandRange(0,255),FMath::RandRange(0,255));
		int32 R = FMath::Rand() % 3;

		FString ServerGame = TEXT("Capture the Flag");
		switch (R)
		{
			case 0 : ServerGame = TEXT("Deathmatch"); break;
			case 1 : ServerGame = TEXT("Team DM"); break;
			default : ServerGame = TEXT("Capture the Flag"); break;
		}

		FString ServerMap = TEXT("CTF-Face");
		FString ServerVer = TEXT("Alpha");
		uint32 ServerPing = FMath::RandRange(35,1024);
		TSharedRef<FServerData> NewServer = FServerData::Make( ServerName, ServerIP, ServerGame, ServerMap, 8,8,32, ServerVer, ServerPing);
		InternetServers.Add( NewServer );
	}
*/
	AddGameFilters();
	FilterServers();
	InternetServerList->RequestListRefresh();

	if (bIsLoggedIn)
	{
		RefreshServers();
	}
	else
	{
		SetBrowserState(FName(TEXT("NotLoggedIn")));
	}


}

void SUWServerBrowser::AddGameFilters()
{
	TArray<FString> GameTypes;
	GameTypes.Add(TEXT("All"));
	for (int i=0;i<InternetServers.Num();i++)
	{
		int32 idx = GameTypes.Find(InternetServers[i]->GameMode);
		if (idx < 0)
		{
			GameTypes.Add(InternetServers[i]->GameMode);
		}
	}

	if ( GameFilter.IsValid() ) 
	{
		TSharedPtr<SVerticalBox> Menu = NULL;
		SAssignNew(Menu, SVerticalBox);
		if (Menu.IsValid())
		{

			for (int i=0;i<GameTypes.Num();i++)
			{
				(*Menu).AddSlot()
				.AutoHeight()
				[
					SNew(SButton)
					.ButtonStyle(SUWindowsStyle::Get(), "UWindows.Standard.MenuList")
					.ContentPadding(FMargin(10.0f, 5.0f))
					.Text(FText::FromString(GameTypes[i]))
					.TextStyle(SUWindowsStyle::Get(), "UWindows.Standard.MainMenuButton.SubMenu.TextStyle")
					.OnClicked(this, &SUWServerBrowser::OnGameFilterSelection, GameTypes[i])
				];
			}
			GameFilter->SetMenuContent(Menu.ToSharedRef());
		}
	}

	GameFilterText->SetText(NSLOCTEXT("SUWServerBrowser", "GameFilterAll", "All"));
}

FReply SUWServerBrowser::OnGameFilterSelection(FString Filter)
{
	GameFilterText->SetText(Filter);
	GameFilter->SetIsOpen(false);
	FilterServers();
	return FReply::Handled();
}

void SUWServerBrowser::OnSort(EColumnSortPriority::Type Priority, const FName& ColumnName, EColumnSortMode::Type SortType)
{
	if (ColumnName == CurrentSortColumn)
	{
		bDescendingSort = !bDescendingSort;
	}

	SortServers(ColumnName);
}

void SUWServerBrowser::SortServers(FName ColumnName)
{
	if (ColumnName == FName(TEXT("ServerName"))) bDescendingSort ? FilteredServers.Sort(FCompareServerByNameDesc()) : FilteredServers.Sort(FCompareServerByName());
	else if (ColumnName == FName(TEXT("ServerIP"))) bDescendingSort ? FilteredServers.Sort(FCompareServerByIPDesc()) : FilteredServers.Sort(FCompareServerByIP());
	else if (ColumnName == FName(TEXT("ServerGame"))) bDescendingSort ? FilteredServers.Sort(FCompareServerByGameModeDesc()) : FilteredServers.Sort(FCompareServerByGameMode());
	else if (ColumnName == FName(TEXT("ServerMap"))) bDescendingSort ? FilteredServers.Sort(FCompareServerByMapDesc()) : FilteredServers.Sort(FCompareServerByMap());
	else if (ColumnName == FName(TEXT("ServerVer"))) bDescendingSort ? FilteredServers.Sort(FCompareServerByVersionDesc()) : FilteredServers.Sort(FCompareServerByVersion());
	else if (ColumnName == FName(TEXT("ServerNumPlayers"))) bDescendingSort ? FilteredServers.Sort(FCompareServerByNumPlayersDesc()) : FilteredServers.Sort(FCompareServerByNumPlayers());
	else if (ColumnName == FName(TEXT("ServerNumSpecs"))) bDescendingSort ? FilteredServers.Sort(FCompareServerByNumSpectatorsDesc()) : FilteredServers.Sort(FCompareServerByNumSpectators());
	else if (ColumnName == FName(TEXT("ServerPing"))) bDescendingSort ? FilteredServers.Sort(FCompareServerByPingDesc()) : FilteredServers.Sort(FCompareServerByPing());

	InternetServerList->RequestListRefresh();
	CurrentSortColumn = ColumnName;
}

/** @return Given a data item return a new widget to represent it in the ListView */
TSharedRef<ITableRow> SUWServerBrowser::OnGenerateWidgetForList( TSharedPtr<FServerData> InItem, const TSharedRef<STableViewBase>& OwnerTable )
{
	return SNew( SServerBrowserRow, OwnerTable).ServerData( InItem ).Style(SUWindowsStyle::Get(),"UWindows.Standard.ServerBrowser.Row");
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
		RefreshButton->SetContent( SNew(STextBlock).Text(NSLOCTEXT("SUWServerBrowser","Refresh","Refresh")));
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
		FString GameVer = FString::Printf(TEXT("%i"),GetDefault<UUTGameEngine>()->GameNetworkVersion);
		SearchSettings->QuerySettings.Set(SETTING_SERVERVERSION, GameVer, EOnlineComparisonOp::Equals);
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
		//InternetServers.Empty();
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
				int32 ServerMaxPlayers = Result.Session.NumOpenPublicConnections;
				FString ServerVer; 
				Result.Session.SessionSettings.Get(SETTING_SERVERVERSION, ServerVer);
				uint32 ServerPing = 0;
				TSharedRef<FServerData> NewServer = FServerData::Make( ServerName, ServerIP, ServerGame, ServerMap, ServerNoPlayers, ServerNoSpecs, ServerMaxPlayers, ServerVer, ServerPing);
				InternetServers.Add( NewServer );
			}
		}
		FilterServers();
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


void SUWServerBrowser::FilterServers()
{
	FilteredServers.Empty();

	FString GameFilter = GameFilterText->GetText();
	for (int i=0;i<InternetServers.Num();i++)
	{
		if (GameFilter.IsEmpty() || GameFilter == TEXT("All") || InternetServers[i]->GameMode == GameFilterText->GetText())
		{
			FilteredServers.Add(InternetServers[i]);
		}
	}
	SortServers(CurrentSortColumn);
}