// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTLocalPlayer.h"
#include "SlateBasics.h"
#include "SlateExtras.h"
#include "Slate/SlateGameResources.h"
#include "../SUWPanel.h"
#include "../SUWindowsDesktop.h"
#include "../SUWindowsStyle.h"
#include "SUWServerBrowser.h"
#include "Online.h"
#include "UTBaseGameMode.h"
#include "UTOnlineGameSearchBase.h"
#include "UTOnlineGameSettingsBase.h"
#include "OnlineSubsystemTypes.h"
#include "../SUWMessageBox.h"
#include "../SUWInputBox.h"
#include "UTGameEngine.h"
#include "UTServerBeaconClient.h"
#include "../SUWScaleBox.h"
#include "Engine/UserInterfaceSettings.h"
#include "UnrealNetwork.h"

#if !UE_SERVER
/** List Sort helpers */

struct FCompareServerByName		{FORCEINLINE bool operator()( const TSharedPtr< FServerData > A, const TSharedPtr< FServerData > B ) const {return ( A->Name < B->Name);}};
struct FCompareServerByNameDesc	{FORCEINLINE bool operator()( const TSharedPtr< FServerData > A, const TSharedPtr< FServerData > B ) const {return ( A->Name > B->Name);}};

struct FCompareServerByIP		{FORCEINLINE bool operator()( const TSharedPtr< FServerData > A, const TSharedPtr< FServerData > B ) const {return ( A->IP < B->IP);}};
struct FCompareServerByIPDesc	{FORCEINLINE bool operator()( const TSharedPtr< FServerData > A, const TSharedPtr< FServerData > B ) const {return ( A->IP > B->IP);}};

struct FCompareServerByGameMode		{FORCEINLINE bool operator()( const TSharedPtr< FServerData > A, const TSharedPtr< FServerData > B ) const {return ( A->GameModePath > B->GameModePath);}};
struct FCompareServerByGameModeDesc	{FORCEINLINE bool operator()( const TSharedPtr< FServerData > A, const TSharedPtr< FServerData > B ) const {return ( A->GameModePath > B->GameModePath);}};

struct FCompareServerByMap		{FORCEINLINE bool operator()( const TSharedPtr< FServerData > A, const TSharedPtr< FServerData > B ) const	{ return ( A->Map < B->Map); }};
struct FCompareServerByMapDesc	{FORCEINLINE bool operator()( const TSharedPtr< FServerData > A, const TSharedPtr< FServerData > B ) const	{ return ( A->Map > B->Map); }};

struct FCompareServerByNumPlayers		{FORCEINLINE bool operator()( const TSharedPtr< FServerData > A, const TSharedPtr< FServerData > B ) const { return ( A->NumPlayers < B->NumPlayers);}};
struct FCompareServerByNumPlayersDesc	{FORCEINLINE bool operator()( const TSharedPtr< FServerData > A, const TSharedPtr< FServerData > B ) const { return ( A->NumPlayers > B->NumPlayers);}};

struct FCompareServerByNumSpectators		{FORCEINLINE bool operator()( const TSharedPtr< FServerData > A, const TSharedPtr< FServerData > B ) const {return ( A->NumSpectators < B->NumSpectators);}};
struct FCompareServerByNumSpectatorsDesc	{FORCEINLINE bool operator()( const TSharedPtr< FServerData > A, const TSharedPtr< FServerData > B ) const {return ( A->NumSpectators> B->NumSpectators);}};

struct FCompareServerByNumFriends { FORCEINLINE bool operator()(const TSharedPtr< FServerData > A, const TSharedPtr< FServerData > B) const { return (A->NumFriends < B->NumFriends); } };
struct FCompareServerByNumFriendsDesc { FORCEINLINE bool operator()(const TSharedPtr< FServerData > A, const TSharedPtr< FServerData > B) const { return (A->NumFriends > B->NumFriends); } };

struct FCompareServerByVersion		{FORCEINLINE bool operator()( const TSharedPtr< FServerData > A, const TSharedPtr< FServerData > B ) const {return ( A->Version < B->Version);}};
struct FCompareServerByVersionDesc	{FORCEINLINE bool operator()( const TSharedPtr< FServerData > A, const TSharedPtr< FServerData > B ) const {return ( A->Version > B->Version);}};

struct FCompareServerByPing		
{
	FORCEINLINE bool operator()	( const TSharedPtr< FServerData > A, const TSharedPtr< FServerData > B ) const 
	{
		return (B->Ping < 0 || (A->Ping >= 0 && A->Ping < B->Ping));	
	}
};

struct FCompareServerByPingDesc	
{
	FORCEINLINE bool operator()( const TSharedPtr< FServerData > A, const TSharedPtr< FServerData > B ) const 
	{
		return ( A->Ping < 0 || (B->Ping >= 0 && A->Ping > B->Ping));	
	}
};


struct FCompareRulesByRule		{FORCEINLINE bool operator()( const TSharedPtr< FServerRuleData > A, const TSharedPtr< FServerRuleData > B ) const {return ( A->Rule > B->Rule);	}};
struct FCompareRulesByRuleDesc	{FORCEINLINE bool operator()( const TSharedPtr< FServerRuleData > A, const TSharedPtr< FServerRuleData > B ) const {return ( A->Rule < B->Rule);	}};
struct FCompareRulesByValue		{FORCEINLINE bool operator()( const TSharedPtr< FServerRuleData > A, const TSharedPtr< FServerRuleData > B ) const {return ( A->Value > B->Value);	}};
struct FCompareRulesByValueDesc	{FORCEINLINE bool operator()( const TSharedPtr< FServerRuleData > A, const TSharedPtr< FServerRuleData > B ) const {return ( A->Value < B->Value);	}};

struct FComparePlayersByName		{FORCEINLINE bool operator()( const TSharedPtr< FServerPlayerData > A, const TSharedPtr< FServerPlayerData > B ) const {return ( A->PlayerName > B->PlayerName);	}};
struct FComparePlayersByNameDesc	{FORCEINLINE bool operator()( const TSharedPtr< FServerPlayerData > A, const TSharedPtr< FServerPlayerData > B ) const {return ( A->PlayerName < B->PlayerName);	}};
struct FComparePlayersByScore		{FORCEINLINE bool operator()( const TSharedPtr< FServerPlayerData > A, const TSharedPtr< FServerPlayerData > B ) const {return ( A->Score > B->Score);	}};
struct FComparePlayersByScoreDesc	{FORCEINLINE bool operator()( const TSharedPtr< FServerPlayerData > A, const TSharedPtr< FServerPlayerData > B ) const {return ( A->Score < B->Score);	}};

SUWServerBrowser::~SUWServerBrowser()
{
	if (PlayerOwner.IsValid())
	{
		PlayerOwner->RemovePlayerOnlineStatusChangedDelegate(PlayerOnlineStatusChangedDelegate);
		PlayerOnlineStatusChangedDelegate.Reset();
	}
}

void SUWServerBrowser::ConstructPanel(FVector2D ViewportSize)
{
	Tag = FName(TEXT("ServerBrowser"));


	bWantsAFullRefilter = false;
	bHideUnresponsiveServers = true;	
	bShowingHubs = false;
	bAutoRefresh = false;
	TSharedRef<SScrollBar> ExternalScrollbar = SNew(SScrollBar);

	OnlineSubsystem = IOnlineSubsystem::Get();
	if (OnlineSubsystem) OnlineIdentityInterface = OnlineSubsystem->GetIdentityInterface();
	if (OnlineSubsystem) OnlineSessionInterface = OnlineSubsystem->GetSessionInterface();

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
					.Image(SUWindowsStyle::Get().GetBrush("UWindows.Standard.ServerBrowser.Backdrop"))
				]
			]
		]

		+SOverlay::Slot()
		[
			SNew( SVerticalBox )
		
			 //Login panel

			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding( 25.0f, 0.0f, 0.0f, 0.0f )
			[
				SNew(SBox)
				.HeightOverride(64)
				[
					SNew( SHorizontalBox )
					+SHorizontalBox::Slot()
					.VAlign(VAlign_Center)
					.Padding(0.0f,0.0f,15.0f,0.0f)
					.AutoWidth()
					[
						SAssignNew( ServerListControlBox, SHorizontalBox )
					]

					+SHorizontalBox::Slot()
					.Padding(15.0f, 0.0f, 0.0f, 0.0f)
					.VAlign(VAlign_Fill)
					.HAlign(HAlign_Fill)
					[
						SNew(SOverlay)
						+SOverlay::Slot()
						[
							SNew(SBox)
							.HeightOverride(64)
							[
								SNew(SImage)
								.Image(SUWindowsStyle::Get().GetBrush("UWindows.Standard.ServerBrowser.TopSubBar"))
							]
						]
						+SOverlay::Slot()
						[
							SNew(SVerticalBox)
							+SVerticalBox::Slot()
							.VAlign(VAlign_Center)
							.HAlign(HAlign_Fill)
							.Padding(64.0f, 0.0f, 16.0f, 0.0f)
							[
								SNew(SHorizontalBox)
								+SHorizontalBox::Slot()
								.AutoWidth()
								[
									SNew(STextBlock)
									.Text(NSLOCTEXT("SUWServerBrowser","QuickFilter","Filter Results by:"))
									.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText")
								]
								+SHorizontalBox::Slot()
								.Padding(16.0f, 0.0f, 0.0f, 0.0f)
								.FillWidth(1.0)
								[
									SNew(SBox)
									.HeightOverride(36)
									[
										SAssignNew(QuickFilterText, SEditableTextBox)
										.Style(SUWindowsStyle::Get(),"UT.Common.Editbox")
										.OnTextCommitted(this, &SUWServerBrowser::OnQuickFilterTextCommited)
										.ClearKeyboardFocusOnCommit(false)
										.MinDesiredWidth(300.0f)
										.Text(FText::GetEmpty())
									]
								]
								+SHorizontalBox::Slot()
								.Padding(16.0f, 0.0f, 0.0f, 0.0f)
								.AutoWidth()
								[
									SAssignNew(HideUnresponsiveServersCheckbox, SCheckBox)
									.Style(SUWindowsStyle::Get(), "UT.Common.CheckBox")
									.ForegroundColor(FLinearColor::White)
									.IsChecked(this, &SUWServerBrowser::ShouldHideUnresponsiveServers)
									.OnCheckStateChanged(this, &SUWServerBrowser::OnHideUnresponsiveServersChanged)
								]
								+SHorizontalBox::Slot()
								.Padding(16.0f, 0.0f, 30.0f, 0.0f)
								.AutoWidth()
								[
									SNew(STextBlock)
									.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText")
									.Text(NSLOCTEXT("SUWSeverBrowser", "HideUnresponsive", "Hide Unresponsive Servers"))
								]

							]
						]
					]
				]
			]

			// The Server Browser itself
			+SVerticalBox::Slot()
			.FillHeight(1)
			.HAlign(HAlign_Fill)
			[
				SNew(SOverlay)
				+SOverlay::Slot()
				[
					BuildServerBrowser()
				]

				+SOverlay::Slot()
				[
					BuildLobbyBrowser()
				]
			]

			 //Connect Panel

			+SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Fill)
			[
				SNew(SBox)
				.HeightOverride(48)
				[
					SNew( SHorizontalBox )
					+SHorizontalBox::Slot().AutoWidth().Padding(10.0f,0.0f,10.0f,0.0f)
					[
						SNew(SBox)
						.VAlign(VAlign_Center)
						[
							// Press rebuild to clear out the old data items and create the new ones (however many are specified by SEditableTextBox)
							SAssignNew(RefreshButton, SButton)
							.ButtonStyle(SUWindowsStyle::Get(), "UWindows.Standard.ServerBrowser.BlankButton")
							.ContentPadding(FMargin(10.0f, 5.0f, 15.0f, 5.0))

							.Text( NSLOCTEXT("SUWServerBrowser","Refresh","Refresh"))
							.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText")
							.OnClicked(this, &SUWServerBrowser::OnRefreshClick)
						]
					]
					+SHorizontalBox::Slot().HAlign(HAlign_Fill)
					[
						SNew(SOverlay)
						+SOverlay::Slot()
						[
							SNew(SImage)
							.Image(SUWindowsStyle::Get().GetBrush("UWindows.Standard.ServerBrowser.BottomSubBar"))
						]
						+SOverlay::Slot()
						[
							SNew(SHorizontalBox)
							+SHorizontalBox::Slot()
							.VAlign(VAlign_Center)
							.Padding(20.0f, 0.0f, 20.0f, 0.0f)
							[
								SAssignNew(StatusText, STextBlock)
								.Text(this, &SUWServerBrowser::GetStatusText)
								.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText")
							]
						]
					]

					+SHorizontalBox::Slot().AutoWidth()
					.VAlign(VAlign_Center)
					[
						// Press rebuild to clear out the old data items and create the new ones (however many are specified by SEditableTextBox)
						SAssignNew(SpectateButton, SButton)
						.ButtonStyle(SUWindowsStyle::Get(), "UWindows.Standard.ServerBrowser.Button")
						.ContentPadding(FMargin(10.0f, 5.0f, 10.0f, 5.0))
						.Text(NSLOCTEXT("SUWServerBrowser","Spectate","Spectate"))
						.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText")
						.OnClicked(this, &SUWServerBrowser::OnJoinClick,true)
					]


					+SHorizontalBox::Slot().AutoWidth() .VAlign(VAlign_Center)
					.VAlign(VAlign_Center)
					[
						// Press rebuild to clear out the old data items and create the new ones (however many are specified by SEditableTextBox)
						SAssignNew(JoinButton, SButton)
						.ButtonStyle(SUWindowsStyle::Get(), "UWindows.Standard.ServerBrowser.RightButton")
						.ContentPadding(FMargin(10.0f, 5.0f, 10.0f, 5.0))
						.Text(NSLOCTEXT("SUWServerBrowser","Join","Join"))
						.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText")
						.OnClicked(this, &SUWServerBrowser::OnJoinClick,false)
					]
				]
			]
		]

	];

	bDescendingSort = false;
	CurrentSortColumn = FName(TEXT("ServerPing"));

	AllInternetServers.Empty();
	AllHubServers.Empty();
	AllLanServers.Empty();

	FilteredServersSource.Empty();
	FilteredHubsSource.Empty();

	InternetServerList->RequestListRefresh();
	HUBServerList->RequestListRefresh();

	if (PlayerOwner->IsLoggedIn())
	{
		SetBrowserState(EBrowserState::BrowserIdle);
	}
	else
	{	
		SetBrowserState(EBrowserState::NotLoggedIn);
	}

	OnRefreshClick();
	
	if (!PlayerOnlineStatusChangedDelegate.IsValid())
	{
		PlayerOnlineStatusChangedDelegate = PlayerOwner->RegisterPlayerOnlineStatusChangedDelegate(FPlayerOnlineStatusChanged::FDelegate::CreateSP(this, &SUWServerBrowser::OwnerLoginStatusChanged));
	}

	UUTGameUserSettings* GS = Cast<UUTGameUserSettings>(GEngine->GetGameUserSettings());
	if (GS)
	{
		const TIndirectArray<SHeaderRow::FColumn>& Columns = HeaderRow->GetColumns();		
		for (int32 i=0;i<Columns.Num();i++)
		{
			if (GS->GetServerBrowserColumnWidth(i) > 0.0)
			{
				HeaderRow->SetColumnWidth(Columns[i].ColumnId, GS->GetServerBrowserColumnWidth(i) );
			}
		}

		if (GS->GetServerBrowserRulesColumnWidth(0) > 0.0)
		{
			RulesHeaderRow->SetColumnWidth(TEXT("Rule"), GS->GetServerBrowserRulesColumnWidth(0));
			RulesHeaderRow->SetColumnWidth(TEXT("Value"), GS->GetServerBrowserRulesColumnWidth(1));
		}

		if (GS->GetServerBrowserPlayersColumnWidth(0) > 0.0)
		{
			PlayersHeaderRow->SetColumnWidth(TEXT("Name"), GS->GetServerBrowserPlayersColumnWidth(0));
			PlayersHeaderRow->SetColumnWidth(TEXT("Score"), GS->GetServerBrowserPlayersColumnWidth(1));
		}

		if (GS->GetServerBrowserSplitterPositions(0) > 0.0)
		{
			VertSplitter->SlotAt(0).SizeValue.Set(GS->GetServerBrowserSplitterPositions(0));
			VertSplitter->SlotAt(1).SizeValue.Set(GS->GetServerBrowserSplitterPositions(1));
		}

		if (GS->GetServerBrowserSplitterPositions(2) > 0.0)
		{
			HorzSplitter->SlotAt(0).SizeValue.Set(GS->GetServerBrowserSplitterPositions(2));
			HorzSplitter->SlotAt(1).SizeValue.Set(GS->GetServerBrowserSplitterPositions(3));
		}
	}	

	BrowserTypeChanged();
	AddGameFilters();
}

TSharedRef<SWidget> SUWServerBrowser::BuildPlayerList()
{
	return SNew(SBorder)
	[
		SNew(SHorizontalBox)
		+SHorizontalBox::Slot()
		.Padding(5,0,5,0)
		.FillWidth(1)
		[
			// The list view being tested
			SAssignNew( PlayersList, SListView< TSharedPtr<FServerPlayerData> > )
			// List view items are this tall
			.ItemHeight(24)
			// When the list view needs to generate a widget for some data item, use this method
			.OnGenerateRow( this, &SUWServerBrowser::OnGenerateWidgetForPlayersList )
			.SelectionMode(ESelectionMode::Single)
			.ListItemsSource( &PlayersListSource)

			.HeaderRow
			(
				SAssignNew(PlayersHeaderRow, SHeaderRow) 
				.Style(SUWindowsStyle::Get(), "UWindows.Standard.ServerBrowser.Header")

				+ SHeaderRow::Column("Name")
						.OnSort(this, &SUWServerBrowser::OnPlayerSort)
						.HeaderContent()
						[
							SNew(STextBlock)
								.Text(NSLOCTEXT("SUWServerBrowser","PlayerNameColumn", "Name"))
								.ToolTipText( NSLOCTEXT("SUWServerBrowser","PlayerNameColumnToolTip", "This player's name.") )
								.TextStyle(SUWindowsStyle::Get(), "UWindows.Standard.ServerBrowser.Header.TextStyle")
						]

				+ SHeaderRow::Column("Score") 
						.DefaultLabel(NSLOCTEXT("SUWServerBrowser","PlayerScoreColumn", "Score")) 
						.HAlignCell(HAlign_Center) 
						.HAlignHeader(HAlign_Center)
						.OnSort(this, &SUWServerBrowser::OnPlayerSort)
						.HeaderContent()
						[
							SNew(STextBlock)
								.Text(NSLOCTEXT("SUWServerBrowser","PlayerScoreColumn", "IP"))
								.ToolTipText( NSLOCTEXT("SUWServerBrowser","PlayerScoreColumnToolTip", "This player's score.") )
								.TextStyle(SUWindowsStyle::Get(), "UWindows.Standard.ServerBrowser.Header.TextStyle")
						]
			)
		]
	];

}

float SUWServerBrowser::GetReverseScale() const
{
	if (PlayerOwner.IsValid() && PlayerOwner->ViewportClient)
	{
		FVector2D ViewportSize;
		PlayerOwner->ViewportClient->GetViewportSize(ViewportSize);
		float NewScale = 1.0f / GetDefault<UUserInterfaceSettings>(UUserInterfaceSettings::StaticClass())->GetDPIScaleBasedOnSize(FIntPoint(ViewportSize.X, ViewportSize.Y));

		// Hack in a scale change for 800x600.  It won't look great but it will look ok in this case.
		return ViewportSize.X < 1024 ? NewScale * 0.75 : NewScale;
	}
	return 1.0f;
}


TSharedRef<SWidget> SUWServerBrowser::BuildServerBrowser()
{
	return SAssignNew(UnScaler, SDPIScaler)
		.DPIScale(this, &SUWServerBrowser::GetReverseScale)
		[
			SAssignNew(InternetServerBrowser, SBox) 
			.HeightOverride(500.0f)
			[
				SAssignNew( VertSplitter, SSplitter )
				.Orientation(Orient_Horizontal)
				.OnSplitterFinishedResizing(this, &SUWServerBrowser::VertSplitterResized)

				+ SSplitter::Slot()
				.Value(0.85)
				[
					SNew(SBorder)
					[
						SNew(SHorizontalBox)
						+SHorizontalBox::Slot()
						.Padding(5,0,5,0)
						.FillWidth(1)
						[
							// The list view being tested
							SAssignNew(InternetServerList, SListView< TSharedPtr<FServerData> >)
							// List view items are this tall
							.ItemHeight(24)
							// Tell the list view where to get its source data
							.ListItemsSource(&FilteredServersSource)
							// When the list view needs to generate a widget for some data item, use this method
							.OnGenerateRow(this, &SUWServerBrowser::OnGenerateWidgetForList)
							.OnSelectionChanged(this, &SUWServerBrowser::OnServerListSelectionChanged)
							.OnMouseButtonDoubleClick(this, &SUWServerBrowser::OnListMouseButtonDoubleClick)
							.SelectionMode(ESelectionMode::Single)
							.HeaderRow
							(
								SAssignNew(HeaderRow, SHeaderRow)
								.Style(SUWindowsStyle::Get(), "UWindows.Standard.ServerBrowser.Header")

								+ SHeaderRow::Column("ServerName")
									.OnSort(this, &SUWServerBrowser::OnSort)
									.HeaderContent()
									[
										SNew(STextBlock)
										.Text(NSLOCTEXT("SUWServerBrowser", "ServerNameColumn", "Server Name"))
										.ToolTipText(NSLOCTEXT("SUWServerBrowser", "ServerNameColumnToolTip", "The name of this server."))
										.TextStyle(SUWindowsStyle::Get(), "UWindows.Standard.ServerBrowser.Header.TextStyle")
									]

								+ SHeaderRow::Column("ServerGame")
									.DefaultLabel(NSLOCTEXT("SUWServerBrowser", "ServerGameColumn", "Game"))
									.HAlignCell(HAlign_Center)
									.HAlignHeader(HAlign_Center)
									.OnSort(this, &SUWServerBrowser::OnSort)
									.HeaderContent()
									[
										SNew(STextBlock)
										.Text(NSLOCTEXT("SUWServerBrowser", "ServerGameColumn", "Game"))
										.ToolTipText(NSLOCTEXT("SUWServerBrowser", "ServerGameColumnToolTip", "The Game type."))
										.TextStyle(SUWindowsStyle::Get(), "UWindows.Standard.ServerBrowser.Header.TextStyle")
									]

								+ SHeaderRow::Column("ServerMap")
									.DefaultLabel(NSLOCTEXT("SUWServerBrowser", "ServerMapColumn", "Map"))
									.HAlignCell(HAlign_Center)
									.HAlignHeader(HAlign_Center)
									.OnSort(this, &SUWServerBrowser::OnSort)
									.HeaderContent()
									[
										SNew(STextBlock)
										.Text(NSLOCTEXT("SUWServerBrowser", "ServerMapColumn", "Map"))
										.ToolTipText(NSLOCTEXT("SUWServerBrowser", "ServerMapColumnToolTip", "The current map."))
										.TextStyle(SUWindowsStyle::Get(), "UWindows.Standard.ServerBrowser.Header.TextStyle")
									]

								+ SHeaderRow::Column("ServerNumPlayers")
									.DefaultLabel(NSLOCTEXT("SUWServerBrowser", "ServerNumPlayerColumn", "Players"))
									.HAlignCell(HAlign_Center)
									.HAlignHeader(HAlign_Center)
									.OnSort(this, &SUWServerBrowser::OnSort)
									.HeaderContent()
									[
										SNew(STextBlock)
										.Text(NSLOCTEXT("SUWServerBrowser", "ServerNumPlayerColumn", "Players"))
										.ToolTipText(NSLOCTEXT("SUWServerBrowser", "ServerNumPlayerColumnToolTip", "The # of Players on this server."))
										.TextStyle(SUWindowsStyle::Get(), "UWindows.Standard.ServerBrowser.Header.TextStyle")
									]

								+ SHeaderRow::Column("ServerNumSpecs")
									.DefaultLabel(NSLOCTEXT("SUWServerBrowser", "ServerNumSpecsColumn", "Spectators"))
									.HAlignCell(HAlign_Center)
									.HAlignHeader(HAlign_Center)
									.OnSort(this, &SUWServerBrowser::OnSort)
									.HeaderContent()
									[
										SNew(STextBlock)
										.Text(NSLOCTEXT("SUWServerBrowser", "ServerNumSpecsColumn", "Spectators"))
										.ToolTipText(NSLOCTEXT("SUWServerBrowser", "ServerNumSpecsColumnToolTip", "The # of spectators on this server."))
										.TextStyle(SUWindowsStyle::Get(), "UWindows.Standard.ServerBrowser.Header.TextStyle")
									]

								+ SHeaderRow::Column("ServerNumFriends")
									.DefaultLabel(NSLOCTEXT("SUWServerBrowser", "ServerNumFriendsColumn", "Friends"))
									.HAlignCell(HAlign_Center)
									.HAlignHeader(HAlign_Center)
									.OnSort(this, &SUWServerBrowser::OnSort)
									.HeaderContent()
									[
										SNew(STextBlock)
										.Text(NSLOCTEXT("SUWServerBrowser", "ServerNumFriendsColumn", "Friends"))
										.ToolTipText(NSLOCTEXT("SUWServerBrowser", "ServerNumFriendsColumnToolTip", "The # of friends on this server."))
										.TextStyle(SUWindowsStyle::Get(), "UWindows.Standard.ServerBrowser.Header.TextStyle")
									]

								+ SHeaderRow::Column("ServerFlags")
									.DefaultLabel(NSLOCTEXT("SUWServerBrowser", "ServerFlagsColumn", "Flags"))
									.HAlignCell(HAlign_Center)
									.HAlignHeader(HAlign_Center)
									.OnSort(this, &SUWServerBrowser::OnSort)
									.HeaderContent()
									[
										SNew(STextBlock)
										.Text(NSLOCTEXT("SUWServerBrowser", "ServerFlagsColumn", "Flags"))
										.ToolTipText(NSLOCTEXT("SUWServerBrowser", "ServerFlagsColumnToolTip", "Server Flags"))
										.TextStyle(SUWindowsStyle::Get(), "UWindows.Standard.ServerBrowser.Header.TextStyle")
									]

								+ SHeaderRow::Column("ServerPing")
									.DefaultLabel(NSLOCTEXT("SUWServerBrowser", "ServerPingColumn", "Ping"))
									.HAlignCell(HAlign_Right)
									.HAlignHeader(HAlign_Right)
									.OnSort(this, &SUWServerBrowser::OnSort)
									.HeaderContent()
									[
										SNew(STextBlock)
										.Text(NSLOCTEXT("SUWServerBrowser", "ServerPingColumn", "Ping"))
										.ToolTipText(NSLOCTEXT("SUWServerBrowser", "ServerPingColumnToolTip", "Your connection speed to the server."))
										.TextStyle(SUWindowsStyle::Get(), "UWindows.Standard.ServerBrowser.Header.TextStyle")
									]
							)
						]
					]
				]
				+ SSplitter::Slot()
				. Value(0.15)
				[

					SAssignNew( HorzSplitter,SSplitter )
					.Orientation(Orient_Vertical)
					.OnSplitterFinishedResizing(this, &SUWServerBrowser::HorzSplitterResized)

					// Game Rules

					+ SSplitter::Slot()
					.Value(0.5)
					[
						SNew(SBorder)
						[
							SNew(SHorizontalBox)
							+SHorizontalBox::Slot()
							.Padding(5,0,5,0)
							.FillWidth(1)
							[
								// The list view being tested
								SAssignNew( RulesList, SListView< TSharedPtr<FServerRuleData> > )
								// List view items are this tall
								.ItemHeight(24)
								// When the list view needs to generate a widget for some data item, use this method
								.OnGenerateRow( this, &SUWServerBrowser::OnGenerateWidgetForRulesList )
								.SelectionMode(ESelectionMode::Single)
								.ListItemsSource( &RulesListSource)

								.HeaderRow
								(
									SAssignNew(RulesHeaderRow, SHeaderRow) 
									.Style(SUWindowsStyle::Get(),"UWindows.Standard.ServerBrowser.Header")

									+ SHeaderRow::Column("Rule")
										.OnSort(this, &SUWServerBrowser::OnRuleSort)
										.HeaderContent()
										[
											SNew(STextBlock)
											.Text(NSLOCTEXT("SUWServerBrowser","RuleRuleColumn", "Rule"))
											.ToolTipText( NSLOCTEXT("SUWServerBrowser","RuleRuleColumnToolTip", "The name of the rule.") )
											.TextStyle(SUWindowsStyle::Get(), "UWindows.Standard.ServerBrowser.Header.TextStyle")
										]

									+ SHeaderRow::Column("Value") 
										.DefaultLabel(NSLOCTEXT("SUWServerBrowser","RuleValueColumn", "Value")) 
										.HAlignCell(HAlign_Center) 
										.HAlignHeader(HAlign_Center)
										.OnSort(this, &SUWServerBrowser::OnRuleSort)
										.HeaderContent()
										[
											SNew(STextBlock)
											.Text(NSLOCTEXT("SUWServerBrowser","RuleValueColumn", "Value"))
											.ToolTipText( NSLOCTEXT("SUWServerBrowser","RuleValueColumnToolTip", "The Value") )
											.TextStyle(SUWindowsStyle::Get(), "UWindows.Standard.ServerBrowser.Header.TextStyle")
										]

								)
							]
						]
					]

					// Player Info

					+ SSplitter::Slot()
					.Value(0.5)
					[
						BuildPlayerList()
					]

				]
			]
		];
}

TSharedRef<SWidget> SUWServerBrowser::BuildLobbyBrowser()
{
	return SNew(SDPIScaler)
		.DPIScale(this, &SUWServerBrowser::GetReverseScale)
		[
			SAssignNew(LobbyBrowser, SBox)
			.Visibility(EVisibility::Hidden)
				.HeightOverride(500.0f)
				[
					SAssignNew(LobbySplitter, SSplitter)
					.Orientation(Orient_Horizontal)
					.OnSplitterFinishedResizing(this, &SUWServerBrowser::VertSplitterResized)

					+ SSplitter::Slot()
					.Value(0.7)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot()
						.VAlign(VAlign_Top)
						.HAlign(HAlign_Fill)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot()
							.FillWidth(1.0)
							.VAlign(VAlign_Center)
							[
								// The list view being tested
								SAssignNew(HUBServerList, SListView< TSharedPtr<FServerData> >)
								// List view items are this tall
								.ItemHeight(64)
								// When the list view needs to generate a widget for some data item, use this method
								.OnGenerateRow(this, &SUWServerBrowser::OnGenerateWidgetForHUBList)
								.SelectionMode(ESelectionMode::Single)
								.ListItemsSource(&FilteredHubsSource)
								.OnMouseButtonDoubleClick(this, &SUWServerBrowser::OnListMouseButtonDoubleClick)
								.OnSelectionChanged(this, &SUWServerBrowser::OnHUBListSelectionChanged)
							]
						]
					]
					+ SSplitter::Slot()
					.Value(0.3)
					[
						SNew(SOverlay)
						+ SOverlay::Slot()
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot()
							.FillWidth(1.0)
							.VAlign(VAlign_Fill)
							[
								SNew(SImage)
								.Image(SUWindowsStyle::Get().GetBrush("UWindows.Standard.Dialog.Background"))
							]
						]
						+ SOverlay::Slot()
						[
							SNew(SScrollBox)
							+ SScrollBox::Slot()
							.Padding(FMargin(0.0f, 5.0f, 0.0f, 5.0f))
							[
								SAssignNew(LobbyInfoBox, SVerticalBox)

								+ SVerticalBox::Slot()
								.Padding(5.0, 5.0, 5.0, 5.0)
								[
									SNew(STextBlock)
									.Text(NSLOCTEXT("HUBBrowser", "NoneSelected", "No Server Selected!"))
									.TextStyle(SUWindowsStyle::Get(), "UWindows.Standard.HUBBrowser.NormalText")

								]
							]

						]
					]
				]
		];

}

ECheckBoxState SUWServerBrowser::ShouldHideUnresponsiveServers() const
{
	return bHideUnresponsiveServers ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SUWServerBrowser::OnHideUnresponsiveServersChanged(const ECheckBoxState NewState)
{
	bHideUnresponsiveServers = NewState == ECheckBoxState::Checked;
	if (bShowingHubs)
	{
		FilterAllHUBs();
	}
	else
	{
		FilterAllServers();
	}
}

void SUWServerBrowser::OwnerLoginStatusChanged(UUTLocalPlayer* LocalPlayerOwner, ELoginStatus::Type NewStatus, const FUniqueNetId& UniqueID)
{
	if (NewStatus == ELoginStatus::LoggedIn)
	{
		SetBrowserState(EBrowserState::BrowserIdle);
		RefreshButton->SetContent( SNew(STextBlock).Text(NSLOCTEXT("SUWServerBrowser","Refresh","Refresh")).TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText"));
		if (bAutoRefresh)
		{
			bAutoRefresh = false;
			OnRefreshClick();
		}
	}
	else
	{
		SetBrowserState(EBrowserState::NotLoggedIn);
	}
}


void SUWServerBrowser::AddGameFilters()
{
	TArray<FString> GameTypes;
	GameTypes.Add(TEXT("All"));
	for (int32 i=0;i<AllInternetServers.Num();i++)
	{
		int32 idx = GameTypes.Find(AllInternetServers[i]->GameModeName);
		if (idx < 0)
		{
			GameTypes.Add(AllInternetServers[i]->GameModeName);
		}
	}

	for (int32 i=0;i<PingList.Num();i++)
	{
		int32 idx = GameTypes.Find(PingList[i]->GameModeName);
		if (idx < 0)
		{
			GameTypes.Add(PingList[i]->GameModeName);
		}
	}

	if ( GameFilter.IsValid() ) 
	{
		TSharedPtr<SVerticalBox> Menu = NULL;
		SAssignNew(Menu, SVerticalBox);
		if (Menu.IsValid())
		{

			for (int32 i=0;i<GameTypes.Num();i++)
			{
				(*Menu).AddSlot()
				.AutoHeight()
				[
					SNew(SButton)
					.ButtonStyle(SUWindowsStyle::Get(), "UT.ContextMenu.Button")
					.ContentPadding(FMargin(10.0f, 5.0f))
					.Text(FText::FromString(GameTypes[i]))
					.TextStyle(SUWindowsStyle::Get(), "UT.ContextMenu.TextStyle")
					.OnClicked(this, &SUWServerBrowser::OnGameFilterSelection, GameTypes[i])
				];
			}
			GameFilter->SetMenuContent(Menu.ToSharedRef());
		}
	}

	if (GameFilterText.IsValid())
	{
		GameFilterText->SetText(NSLOCTEXT("SUWServerBrowser", "GameFilterAll", "All"));
	}
}

FReply SUWServerBrowser::OnGameFilterSelection(FString Filter)
{
	GameFilterText->SetText(Filter);
	GameFilter->SetIsOpen(false);
	FilterAllServers();
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

void SUWServerBrowser::OnRuleSort(EColumnSortPriority::Type Priority, const FName& ColumnName, EColumnSortMode::Type SortType)
{
	if (ColumnName == CurrentRulesSortColumn)
	{
		bDescendingRulesSort = !bDescendingRulesSort;
	}

	CurrentRulesSortColumn = ColumnName;


	if (ColumnName == FName("Rule")) bDescendingRulesSort ? RulesListSource.Sort(FCompareRulesByRuleDesc()) : RulesListSource.Sort(FCompareRulesByRule());
	else if (ColumnName == FName("Value")) bDescendingRulesSort ? RulesListSource.Sort(FCompareRulesByValueDesc()) : RulesListSource.Sort(FCompareRulesByValue());

	RulesList->RequestListRefresh();

}

void SUWServerBrowser::OnPlayerSort(EColumnSortPriority::Type Priority, const FName& ColumnName, EColumnSortMode::Type SortType)
{
	if (ColumnName == CurrentRulesSortColumn)
	{
		bDescendingPlayersSort = !bDescendingPlayersSort;
	}

	CurrentRulesSortColumn = ColumnName;

	if (ColumnName == FName("Name")) bDescendingPlayersSort ? PlayersListSource.Sort(FComparePlayersByNameDesc()) : PlayersListSource.Sort(FComparePlayersByName());
	else if (ColumnName == FName("Score")) bDescendingPlayersSort ? PlayersListSource.Sort(FComparePlayersByScoreDesc()) : PlayersListSource.Sort(FComparePlayersByScore());

	PlayersList->RequestListRefresh();

}



void SUWServerBrowser::SortServers(FName ColumnName)
{
	if (ColumnName == FName(TEXT("ServerName"))) bDescendingSort ? FilteredServersSource.Sort(FCompareServerByNameDesc()) : FilteredServersSource.Sort(FCompareServerByName());
	else if (ColumnName == FName(TEXT("ServerIP"))) bDescendingSort ? FilteredServersSource.Sort(FCompareServerByIPDesc()) : FilteredServersSource.Sort(FCompareServerByIP());
	else if (ColumnName == FName(TEXT("ServerGame"))) bDescendingSort ? FilteredServersSource.Sort(FCompareServerByGameModeDesc()) : FilteredServersSource.Sort(FCompareServerByGameMode());
	else if (ColumnName == FName(TEXT("ServerMap"))) bDescendingSort ? FilteredServersSource.Sort(FCompareServerByMapDesc()) : FilteredServersSource.Sort(FCompareServerByMap());
	else if (ColumnName == FName(TEXT("ServerVer"))) bDescendingSort ? FilteredServersSource.Sort(FCompareServerByVersionDesc()) : FilteredServersSource.Sort(FCompareServerByVersion());
	else if (ColumnName == FName(TEXT("ServerNumPlayers"))) bDescendingSort ? FilteredServersSource.Sort(FCompareServerByNumPlayersDesc()) : FilteredServersSource.Sort(FCompareServerByNumPlayers());
	else if (ColumnName == FName(TEXT("ServerNumSpecs"))) bDescendingSort ? FilteredServersSource.Sort(FCompareServerByNumSpectatorsDesc()) : FilteredServersSource.Sort(FCompareServerByNumSpectators());
	else if (ColumnName == FName(TEXT("ServerNumFriends"))) bDescendingSort ? FilteredServersSource.Sort(FCompareServerByNumFriendsDesc()) : FilteredServersSource.Sort(FCompareServerByNumFriends());
	else if (ColumnName == FName(TEXT("ServerPing"))) bDescendingSort ? FilteredServersSource.Sort(FCompareServerByPingDesc()) : FilteredServersSource.Sort(FCompareServerByPing());

	InternetServerList->RequestListRefresh();
	CurrentSortColumn = ColumnName;
}

void SUWServerBrowser::SortHUBs()
{
	FilteredHubsSource.Sort(FCompareServerByPing());
	HUBServerList->RequestListRefresh();
}

TSharedRef<ITableRow> SUWServerBrowser::OnGenerateWidgetForList( TSharedPtr<FServerData> InItem, const TSharedRef<STableViewBase>& OwnerTable )
{
	return SNew( SServerBrowserRow, OwnerTable).ServerData( InItem ).Style(SUWindowsStyle::Get(),"UWindows.Standard.ServerBrowser.Row");
}

TSharedRef<ITableRow> SUWServerBrowser::OnGenerateWidgetForRulesList( TSharedPtr<FServerRuleData> InItem, const TSharedRef<STableViewBase>& OwnerTable )
{
	return SNew( SServerRuleRow, OwnerTable).ServerRuleData( InItem ).Style(SUWindowsStyle::Get(),"UWindows.Standard.ServerBrowser.Row");
}

TSharedRef<ITableRow> SUWServerBrowser::OnGenerateWidgetForPlayersList( TSharedPtr<FServerPlayerData> InItem, const TSharedRef<STableViewBase>& OwnerTable )
{
	return SNew( SServerPlayerRow, OwnerTable).ServerPlayerData( InItem ).Style(SUWindowsStyle::Get(),"UWindows.Standard.ServerBrowser.Row");
}

FText SUWServerBrowser::GetStatusText() const
{
	if (BrowserState == EBrowserState::NotLoggedIn)
	{
		return NSLOCTEXT("SUWServerBrowser","NotLoggedIn","Login Required!");
	}
	else if (BrowserState == EBrowserState::AuthInProgress) 
	{
		return NSLOCTEXT("SUWServerBrowser","Auth","Authenticating...");
	}
	else 
	{
		int32 PingCount = PingList.Num() + PingTrackers.Num();


		FFormatNamedArguments Args;
		Args.Add( TEXT("PingCount"), FText::AsNumber(PingCount) );
		Args.Add( TEXT("FilteredHubCount"), FText::AsNumber(FilteredHubsSource.Num() > 0 ? FilteredHubsSource.Num() : AllHubServers.Num()) );
		Args.Add( TEXT("HubCount"), FText::AsNumber(AllHubServers.Num()) );
		Args.Add( TEXT("FilteredServerCount"), FText::AsNumber(FilteredServersSource.Num()> 0 ? FilteredServersSource.Num() : AllInternetServers.Num()) );
		Args.Add( TEXT("ServerCount"), FText::AsNumber(AllInternetServers.Num()) );
		Args.Add( TEXT("TotalPlayers"), FText::AsNumber(TotalPlayersPlaying) );

		int32 PlayerCount = 0;
		if (bShowingHubs)
		{
			for (int32 i=0; i < FilteredHubsSource.Num(); i++)
			{
				PlayerCount += FilteredHubsSource[i]->NumPlayers + FilteredHubsSource[i]->NumSpectators;
			}
		}
		else
		{
			for (int32 i=0; i < FilteredServersSource.Num(); i++)
			{
				PlayerCount += FilteredServersSource[i]->NumPlayers + FilteredServersSource[i]->NumSpectators;
			}
		}

		Args.Add( TEXT("PlayerCount"), FText::AsNumber(PlayerCount) );

		if (PingCount > 0)
		{
			return FText::Format( NSLOCTEXT("SUWServerBrowser","PingingStatusMsg","Showing {FilteredHubCount} of {HubCount} Hubs, {FilteredServerCount} of {ServerCount} Servers - Pinging {PingCount} Servers..."), Args);
		}
		else
		{
			return FText::Format( NSLOCTEXT("SUWServerBrowser","IdleStatusMsg","Showing {FilteredHubCount} of {HubCount} Hubs, {FilteredServerCount} of {ServerCount} Servers -- {PlayerCount} of {TotalPlayers} Players"), Args);
		}
	}
}

void SUWServerBrowser::SetBrowserState(FName NewBrowserState)
{
	BrowserState = NewBrowserState;
	if (BrowserState == EBrowserState::NotLoggedIn) 
	{
		RefreshButton->SetContent( SNew(STextBlock).Text(NSLOCTEXT("SUWServerBrowser","Login","Login")).TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText"));
		RefreshButton->SetVisibility(EVisibility::All);

		JoinButton->SetEnabled(false);
		SpectateButton->SetEnabled(false);

	}
	else if (BrowserState == EBrowserState::BrowserIdle) 
	{
		RefreshButton->SetVisibility(EVisibility::All);
	}
	else if (BrowserState == EBrowserState::AuthInProgress) 
	{
		RefreshButton->SetVisibility(EVisibility::Hidden);

		JoinButton->SetEnabled(false);
		SpectateButton->SetEnabled(false);
	}
	else if (BrowserState == EBrowserState::RefreshInProgress) 
	{
		RefreshButton->SetVisibility(EVisibility::Hidden);
		JoinButton->SetEnabled(false);
		SpectateButton->SetEnabled(false);
	}
}

FReply SUWServerBrowser::OnRefreshClick()
{
	if (PlayerOwner->IsLoggedIn())
	{
		RefreshServers();
	}
	else
	{
		bAutoRefresh = true;
		PlayerOwner->LoginOnline(TEXT(""),TEXT(""),false);
	}
	return FReply::Handled();
}

void SUWServerBrowser::RefreshServers()
{
	bWantsAFullRefilter = true;
	if (PlayerOwner->IsLoggedIn() && OnlineSessionInterface.IsValid() && BrowserState == EBrowserState::BrowserIdle)
	{
		SetBrowserState(EBrowserState::RefreshInProgress);

		bNeedsRefresh = false;
		CleanupQoS();

		SearchSettings = MakeShareable(new FUTOnlineGameSearchBase(false));
		SearchSettings->MaxSearchResults = 10000;
		FString GameVer = FString::Printf(TEXT("%i"), FNetworkVersion::GetLocalNetworkVersion());
		//SearchSettings->QuerySettings.Set(SETTING_SERVERVERSION, GameVer, EOnlineComparisonOp::Equals);											// Must equal the game version
		SearchSettings->QuerySettings.Set(SETTING_GAMEINSTANCE, 1, EOnlineComparisonOp::NotEquals);												// Must not be a Hub server instance

		TSharedRef<FUTOnlineGameSearchBase> SearchSettingsRef = SearchSettings.ToSharedRef();

		FOnFindSessionsCompleteDelegate Delegate;
		Delegate.BindSP(this, &SUWServerBrowser::OnFindSessionsComplete);
		OnFindSessionCompleteDelegate = OnlineSessionInterface->AddOnFindSessionsCompleteDelegate_Handle(Delegate);

		OnlineSessionInterface->FindSessions(0, SearchSettingsRef);
	}
	else
	{
		SetBrowserState(EBrowserState::NotLoggedIn);	
	}
}

void SUWServerBrowser::OnFindSessionsComplete(bool bWasSuccessful)
{
	OnlineSessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(OnFindSessionCompleteDelegate);

	if (bWasSuccessful)
	{
		TotalPlayersPlaying = 0;
		if (SearchSettings->SearchResults.Num() > 0)
		{
			for (int32 ServerIndex = 0; ServerIndex < SearchSettings->SearchResults.Num(); ServerIndex++)
			{
				FOnlineSessionSearchResult Result = SearchSettings->SearchResults[ServerIndex];
				FString ServerName;
				Result.Session.SessionSettings.Get(SETTING_SERVERNAME,ServerName);
				FString ServerIP;
				OnlineSessionInterface->GetResolvedConnectString(Result,FName(TEXT("GamePort")), ServerIP);
				// game class path
				FString ServerGamePath;
				Result.Session.SessionSettings.Get(SETTING_GAMEMODE,ServerGamePath);
				// game name
				FString ServerGameName;
				// prefer using the name in the client's language, if available
				// TODO: would be nice to not have to load the class, but the localization system doesn't guarantee any particular lookup location for the data,
				//		so we have no way to know where it is
				UClass* GameClass = LoadClass<AUTBaseGameMode>(NULL, *ServerGamePath, NULL, LOAD_NoWarn | LOAD_Quiet, NULL);
				if (GameClass != NULL)
				{
					ServerGameName = GameClass->GetDefaultObject<AUTBaseGameMode>()->DisplayName.ToString();
				}
				else
				{
					// FIXME: legacy compatibility, remove later
					if (!Result.Session.SessionSettings.Get(SETTING_GAMENAME, ServerGameName))
					{
						ServerGameName = ServerGamePath;
					}
				}
			

				FString ServerMap;
				Result.Session.SessionSettings.Get(SETTING_MAPNAME,ServerMap);
				
				int32 ServerNoPlayers = 0;
				Result.Session.SessionSettings.Get(SETTING_PLAYERSONLINE, ServerNoPlayers);
				
				int32 ServerNoSpecs = 0;
				Result.Session.SessionSettings.Get(SETTING_SPECTATORSONLINE, ServerNoSpecs);
				
				int32 ServerMaxPlayers = 0;
				Result.Session.SessionSettings.Get(SETTING_UTMAXPLAYERS, ServerMaxPlayers);
				
				int32 ServerMaxSpectators = 0;
				Result.Session.SessionSettings.Get(SETTING_UTMAXSPECTATORS, ServerMaxSpectators);

				int32 ServerNumMatches = 0;
				Result.Session.SessionSettings.Get(SETTING_NUMMATCHES, ServerNumMatches);
				
				int32 ServerMinRank = 0;
				Result.Session.SessionSettings.Get(SETTING_MINELO, ServerMinRank);

				int32 ServerMaxRank = 0;
				Result.Session.SessionSettings.Get(SETTING_MAXELO, ServerMaxRank);

				FString ServerVer; 
				Result.Session.SessionSettings.Get(SETTING_SERVERVERSION, ServerVer);
				
				int32 ServerFlags = 0x0000;
				Result.Session.SessionSettings.Get(SETTING_SERVERFLAGS, ServerFlags);
				
				uint32 ServerPing = -1;

				int32 ServerTrustLevel = 2;
				Result.Session.SessionSettings.Get(SETTING_TRUSTLEVEL, ServerTrustLevel);

				FString BeaconIP;
				OnlineSessionInterface->GetResolvedConnectString(Result,FName(TEXT("BeaconPort")), BeaconIP);
				TSharedRef<FServerData> NewServer = FServerData::Make( ServerName, ServerIP, BeaconIP, ServerGamePath, ServerGameName, ServerMap, ServerNoPlayers, ServerNoSpecs, ServerMaxPlayers, ServerMaxSpectators, ServerNumMatches, ServerMinRank, ServerMaxRank, ServerVer, ServerPing, ServerFlags,ServerTrustLevel);
				NewServer->SearchResult = Result;

				if (PingList.Num() == 0 || ServerGamePath != LOBBY_GAME_PATH )
				{
					PingList.Add( NewServer );
				}
				else
				{
					PingList.Insert(NewServer,0);
				}

				TotalPlayersPlaying += ServerNoPlayers + ServerNoSpecs;
			}
		}

		if ( FParse::Param(FCommandLine::Get(), TEXT("DumpServers")) )
		{
			UE_LOG(UT,Log,TEXT("Received a list of %i Servers....."), PingList.Num());
			for (int32 i=0;i<PingList.Num();i++)
			{
				UE_LOG(UT,Log,TEXT("Received Server %i - %s %s  : Players %i/%i"), i, *PingList[i]->Name, *PingList[i]->IP, PingList[i]->NumPlayers, PingList[i]->MaxPlayers);
			}
			UE_LOG(UT,Log, TEXT("----------------------------------------------"));
		}

		// If a server exists in either of the lists but not in the PingList, then let's kill it.
		ExpireDeadServers();

		if (OnlineSubsystem)
		{
			IOnlineFriendsPtr FriendsInterface = OnlineSubsystem->GetFriendsInterface();
			if (FriendsInterface.IsValid())
			{
				// Grab a list of my online friends.
				FriendsInterface->ReadFriendsList(0, EFriendsLists::ToString(EFriendsLists::InGamePlayers), FOnReadFriendsListComplete::CreateRaw(this, &SUWServerBrowser::OnReadFriendsListComplete));
			}
		}

		AddGameFilters();
		InternetServerList->RequestListRefresh();
		HUBServerList->RequestListRefresh();
		PingNextServer();


	}
	else
	{
		UE_LOG(UT,Log,TEXT("Server List Request Failed!!!"));
	}

	SetBrowserState(EBrowserState::BrowserIdle);

}

void SUWServerBrowser::ExpireDeadServers()
{
	int32 i = 0;
	while (i < AllInternetServers.Num())
	{
		bool bFound = false;
		for (int32 j=0; j < PingList.Num(); j++)
		{
			if (AllInternetServers[i]->SearchResult.Session.SessionInfo->GetSessionId() == PingList[j]->SearchResult.Session.SessionInfo->GetSessionId())	
			{
				bFound = true;
				break;
			}
		}

		if (bFound)
		{
			i++;
		}
		else
		{
			AllInternetServers.RemoveAt(i);
		}
	
	} 

	i = 0;
	while (i < AllHubServers.Num())
	{
		if (!AllHubServers[i]->bFakeHUB)
		{
			bool bFound = false;
			for (int32 j=0; j < PingList.Num(); j++)
			{
				if (AllHubServers[i]->SearchResult.Session.SessionInfo->GetSessionId() == PingList[j]->SearchResult.Session.SessionInfo->GetSessionId())	
				{
					bFound = true;
					break;
				}
			}

			if (bFound)
			{
				i++;
			}
			else
			{
				AllHubServers.RemoveAt(i);
			}
		}
		else
		{
			i++;
		}

	} 
}

void SUWServerBrowser::OnReadFriendsListComplete(int32 LocalUserNum, bool bWasSuccessful, const FString& ListName, const FString& ErrorStr)
{
	if (bWasSuccessful)
	{
		bool bRequiresUpdate = false;
		if (OnlineSubsystem)
		{
			IOnlineFriendsPtr FriendsInterface = OnlineSubsystem->GetFriendsInterface();
			if (FriendsInterface.IsValid())
			{
				TArray< TSharedRef<FOnlineFriend> > FriendsCache;
				FriendsInterface->GetFriendsList(0, EFriendsLists::ToString(EFriendsLists::InGamePlayers), FriendsCache);

				for (int32 PlayerIndex=0; PlayerIndex < FriendsCache.Num(); PlayerIndex++)
				{
					const FOnlineFriend& Friend = *FriendsCache[PlayerIndex];
					const FOnlineUserPresence& FriendPresence = Friend.GetPresence();

					if (FriendPresence.bIsOnline)
					{
						FString SessionIdAsString = TEXT("");
						
						// Look to see if we have a HUB id.  If we do, use that otherwise use the actual session Id if there is one

						const FVariantData* HUBSessionId = FriendPresence.Status.Properties.Find(HUBSessionIdKey);
						if (HUBSessionId != nullptr && HUBSessionId->GetType() == EOnlineKeyValuePairDataType::String)
						{
							HUBSessionId->GetValue(SessionIdAsString);
						}

						// If we didn't have a HUB Id, then grab the player's current session Id if they have one.
						if (SessionIdAsString == TEXT("") && FriendPresence.SessionId.IsValid())
						{
							SessionIdAsString = FriendPresence.SessionId->ToString();
						}

						// Itterate over all servers and update their friend counts

						for (int32 ServerIndex = 0; ServerIndex < AllInternetServers.Num(); ServerIndex++)
						{
							if (AllInternetServers[ServerIndex]->SearchResult.Session.SessionInfo->GetSessionId().ToString() == SessionIdAsString)
							{
								AllInternetServers[ServerIndex]->NumFriends++;
								bRequiresUpdate = true;
								break;
							}
						}

						for (int32 HUBIndex = 0; HUBIndex < AllHubServers.Num(); HUBIndex++)
						{
							// NOTE: We have to check the search result here because of the fake HUB.  
							if (AllHubServers[HUBIndex]->SearchResult.IsValid() && AllHubServers[HUBIndex]->SearchResult.Session.SessionInfo->GetSessionId().ToString() == SessionIdAsString)
							{
								AllHubServers[HUBIndex]->NumFriends++;
								bRequiresUpdate = true;
								break;
							}
						}
					}
				}
			}
		}

		if (bRequiresUpdate)
		{
			InternetServerList->RequestListRefresh();
			HUBServerList->RequestListRefresh();
		}

	}
	SetBrowserState(EBrowserState::BrowserIdle);	
}


void SUWServerBrowser::CleanupQoS()
{
	for (int32 i=0;i<PingTrackers.Num();i++)
	{
		if (PingTrackers[i].Beacon.IsValid())
		{
			PingTrackers[i].Beacon->DestroyBeacon();

		}
	}
	PingTrackers.Empty();
	PingList.Empty();
}

void SUWServerBrowser::PingNextServer()
{

	if (PingList.Num() <= 0 && PingTrackers.Num() <= 0)
	{
		if (bWantsAFullRefilter)
		{
			// We are done.  Perform all filtering again since we can grab the real best ping
			if (bShowingHubs)
			{
				FilterAllHUBs();
			}
			else
			{
				FilterAllServers();
			}
		}

		bWantsAFullRefilter = false;
	}

	while (PingList.Num() > 0 && PingTrackers.Num() < PlayerOwner->ServerPingBlockSize)
	{
		PingServer(PingList[0]);
		PingList.RemoveAt(0,1);
	}

}

void SUWServerBrowser::PingServer(TSharedPtr<FServerData> ServerToPing)
{
	// Build the beacon
	AUTServerBeaconClient* Beacon = PlayerOwner->GetWorld()->SpawnActor<AUTServerBeaconClient>(AUTServerBeaconClient::StaticClass());
	if (Beacon)
	{
		Beacon->OnServerRequestResults = FServerRequestResultsDelegate::CreateSP(this, &SUWServerBrowser::OnServerBeaconResult );
		Beacon->OnServerRequestFailure = FServerRequestFailureDelegate::CreateSP(this, &SUWServerBrowser::OnServerBeaconFailure);
		FURL BeaconURL(nullptr, *ServerToPing->BeaconIP, TRAVEL_Absolute);
		Beacon->InitClient(BeaconURL);
		PingTrackers.Add(FServerPingTracker(ServerToPing, Beacon));
	}
}

void SUWServerBrowser::AddServer(TSharedPtr<FServerData> Server)
{
	for (int32 i=0; i < AllInternetServers.Num() ; i++)
	{
		if (AllInternetServers[i]->SearchResult.Session.SessionInfo->GetSessionId() == Server->SearchResult.Session.SessionInfo->GetSessionId())
		{
			// Same session id, so see if they are the same

			if (AllInternetServers[i] != Server)
			{
				AllInternetServers[i]->Update(Server);
			}

			return; 
		}
	}

	AllInternetServers.Add(Server);
	FilterServer(Server);
}

void SUWServerBrowser::AddHub(TSharedPtr<FServerData> Hub)
{
	for (int32 i=0; i < AllHubServers.Num() ; i++)
	{
		if (!AllHubServers[i]->bFakeHUB)
		{
			if (AllHubServers[i]->SearchResult.Session.SessionInfo->GetSessionId() == Hub->SearchResult.Session.SessionInfo->GetSessionId())
			{
				// Same session id, so see if they are the same

				if (AllHubServers[i] != Hub)
				{
					AllHubServers[i]->Update(Hub);
				}

				return; 
			}
		}
	}

	AllHubServers.Add(Hub);
	FilterHUB(Hub);
}

void SUWServerBrowser::OnServerBeaconFailure(AUTServerBeaconClient* Sender)
{
	for (int32 i=0; i < PingTrackers.Num(); i++)
	{
		if (PingTrackers[i].Beacon == Sender)
		{

			AddServer(PingTrackers[i].Server);
			PingTrackers[i].Beacon->DestroyBeacon();
			PingTrackers.RemoveAt(i,1);

			PingNextServer();
		}
	}
}

void SUWServerBrowser::OnServerBeaconResult(AUTServerBeaconClient* Sender, FServerBeaconInfo ServerInfo)
{
	for (int32 i=0; i < PingTrackers.Num(); i++)
	{
		if (PingTrackers[i].Beacon == Sender)
		{
			// Matched... store the data.
			PingTrackers[i].Server->Ping = Sender->Ping;
			PingTrackers[i].Server->MOTD = ServerInfo.MOTD;
			PingTrackers[i].Server->Map = ServerInfo.CurrentMap;

			PingTrackers[i].Server->Players.Empty();
			TArray<FString> PlayerData;
			int32 Cnt = ServerInfo.ServerPlayers.ParseIntoArray(PlayerData, TEXT("\t"), true);
			for (int32 p=0;p+2 < Cnt; p+=3)
			{
				FString Name = PlayerData[p];
				FString Score = PlayerData[p+1];
				FString Id = PlayerData[p+2];

				PingTrackers[i].Server->AddPlayer(Name, Score, Id);
			}

			PingTrackers[i].Server->Rules.Empty();
			TArray<FString> RulesData;
			Cnt = ServerInfo.ServerRules.ParseIntoArray(RulesData, TEXT("\t"), true);
			for (int32 r=0; r+1 < Cnt; r+=2)
			{
				FString Rule = RulesData[r];
				FString Value = RulesData[r+1];

				PingTrackers[i].Server->AddRule(Rule, Value);
			}

			TArray<FString> BrokenIP;
			if (PingTrackers[i].Server->IP.ParseIntoArray(BrokenIP,TEXT(":"),true) == 2)
			{
				PingTrackers[i].Server->AddRule(TEXT("IP"), BrokenIP[0]);
				PingTrackers[i].Server->AddRule(TEXT("Port"), BrokenIP[1]);
			}
			else
			{
				PingTrackers[i].Server->AddRule(TEXT("IP"), PingTrackers[i].Server->IP);
			}

			PingTrackers[i].Server->AddRule(TEXT("Version"), PingTrackers[i].Server->Version);

			PingTrackers[i].Server->HUBInstances.Empty();
			for (int32 InstIndex=0; InstIndex < PingTrackers[i].Beacon->InstanceInfo.Num(); InstIndex++ )
			{
				PingTrackers[i].Server->HUBInstances.Add(FServerInstanceData::Make(PingTrackers[i].Beacon->InstanceInfo[InstIndex]));	
			}

			if (PingTrackers[i].Server->GameModePath == LOBBY_GAME_PATH)
			{
				AddHub(PingTrackers[i].Server);
			}
			else
			{
				AddServer(PingTrackers[i].Server);
			}

			PingTrackers[i].Beacon->DestroyBeacon();
			PingTrackers.RemoveAt(i,1);

			// Look to see if there are more servers to Ping...
			PingNextServer();

			RulesList->RequestListRefresh();
			PlayersList->RequestListRefresh();
		}
	}
}

void SUWServerBrowser::OnListMouseButtonDoubleClick(TSharedPtr<FServerData> SelectedServer)
{
	if (SelectedServer->IP.Left(1) == "@")
	{
		ShowServers(SelectedServer->BeaconIP);
	}
	else
	{
		ConnectTo(*SelectedServer,false);
	}
}


FReply SUWServerBrowser::OnJoinClick(bool bSpectate)
{
	TArray<TSharedPtr<FServerData>> SelectedItems = (bShowingHubs ? HUBServerList->GetSelectedItems() : InternetServerList->GetSelectedItems());
	if (SelectedItems.Num() > 0)
	{
		ConnectTo(*SelectedItems[0],bSpectate);
	}
	return FReply::Handled();
}

void SUWServerBrowser::ConnectTo(FServerData ServerData,bool bSpectate)
{
	// Flag the browser as needing a refresh the next time it is shown
	bNeedsRefresh = true;
	PlayerOwner->JoinSession(ServerData.SearchResult, bSpectate);
	CleanupQoS();
	PlayerOwner->HideMenu();
}

void SUWServerBrowser::FilterAllServers()
{
	FilteredServersSource.Empty();
	if (AllInternetServers.Num() > 0)
	{
		int32 BestPing = AllInternetServers[0]->Ping;
		for (int32 i=0;i<AllInternetServers.Num();i++)
		{
			if (AllInternetServers[i]->Ping < BestPing) BestPing = AllInternetServers[i]->Ping;
		}

		for (int32 i=0;i<AllInternetServers.Num();i++)
		{
			FilterServer(AllInternetServers[i], false);
		}
		SortServers(CurrentSortColumn);
	}
}

void SUWServerBrowser::FilterServer(TSharedPtr< FServerData > NewServer, bool bSortAndUpdate, int32 BestPing)
{
	if (GameFilterText.IsValid())
	{
		FString GameFilterString = GameFilterText->GetText().ToString();
		if (GameFilterString.IsEmpty() || GameFilterString == TEXT("All") || NewServer->GameModeName == GameFilterString)
		{
			if (QuickFilterText->GetText().IsEmpty() || NewServer->Name.Find(QuickFilterText->GetText().ToString()) >= 0)
			{

				if ( !IsUnresponsive(NewServer, BestPing) )
				{
					FilteredServersSource.Add(NewServer);
				}
			}
		}

		if (bSortAndUpdate)
		{
			SortServers(CurrentSortColumn);
		}
	}
}

void SUWServerBrowser::FilterAllHUBs()
{
	FilteredHubsSource.Empty();
	if (AllHubServers.Num() > 0)
	{
		int32 BestPing = AllHubServers[0]->Ping;
		for (int32 i=0;i<AllHubServers.Num();i++)
		{
			if (AllHubServers[i]->Ping < BestPing) BestPing = AllHubServers[i]->Ping;
		}

		for (int32 i=0;i<AllHubServers.Num();i++)
		{
			FilterHUB(AllHubServers[i], false, BestPing);
		}
		SortHUBs();
	}
}


void SUWServerBrowser::FilterHUB(TSharedPtr< FServerData > NewServer, bool bSortAndUpdate, int32 BestPing)
{
	if (QuickFilterText->GetText().IsEmpty() || NewServer->Name.Find(QuickFilterText->GetText().ToString()) >= 0)
	{
		int32 BaseRank = PlayerOwner->GetBaseELORank();
		if (NewServer->bFakeHUB)
		{
			FilteredHubsSource.Add(NewServer);
		}
		else
		{
			if ( (NewServer->MinRank <= 0 || BaseRank >= NewServer->MinRank) && (NewServer->MaxRank <= 0 || BaseRank <= NewServer->MaxRank))
			{
				if ( !IsUnresponsive(NewServer, BestPing) )
				{
					FilteredHubsSource.Add(NewServer);
				}
			}
		}
	}

	if (bSortAndUpdate)
	{
		SortHUBs();
	}
}

bool SUWServerBrowser::IsUnresponsive(TSharedPtr<FServerData> Server, int32 BestPing)
{
	// If we aren't hiding unresponsive servers, we don't care so just return false.
	if (!bHideUnresponsiveServers)
	{
		return false;
	}

	if (Server->Ping >= 0)			
	{
		int32  WorstPing = FMath::Max<int32>(BestPing * 2, 100);
		if ( Server->NumPlayers > 0 || Server->Ping <= WorstPing )
		{
			return false;
		}
	}

	return true;
}

void SUWServerBrowser::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	bool bRequiresUpdate = false;
	UUTGameUserSettings* GS = Cast<UUTGameUserSettings>(GEngine->GetGameUserSettings());
	if (GS != NULL)
	{
		const TIndirectArray<SHeaderRow::FColumn>& Columns = HeaderRow->GetColumns();		
		for (int32 i=0;i<Columns.Num();i++)
		{
			if (GS->GetServerBrowserColumnWidth(i) != Columns[i].GetWidth())
			{
				GS->SetServerBrowserColumnWidth(i, Columns[i].GetWidth());
				bRequiresUpdate = true;
				break;
			}
		}

		const TIndirectArray<SHeaderRow::FColumn>& RulesColumns = RulesHeaderRow->GetColumns();		
		for (int32 i=0;i<RulesColumns.Num();i++)
		{
			if (GS->GetServerBrowserRulesColumnWidth(i) != RulesColumns[i].GetWidth())
			{
				GS->SetServerBrowserRulesColumnWidth(i, RulesColumns[i].GetWidth());
				bRequiresUpdate = true;
				break;
			}
		}

		const TIndirectArray<SHeaderRow::FColumn>& PlayersColumns = PlayersHeaderRow->GetColumns();		
		for (int32 i=0;i<PlayersColumns.Num();i++)
		{
			if (GS->GetServerBrowserPlayersColumnWidth(i) != PlayersColumns[i].GetWidth())
			{
				GS->SetServerBrowserPlayersColumnWidth(i, PlayersColumns[i].GetWidth());
				bRequiresUpdate = true;
				break;
			}
		}

		if (bRequiresUpdate && !bRequireSave)
		{
			bRequireSave = true;
		}

		if (!bRequiresUpdate && bRequireSave)
		{
			GS->SaveConfig();
			bRequireSave = false;
		}
	}

	if (BrowserState != EBrowserState::BrowserIdle) 
	{
		JoinButton->SetEnabled(false);
		SpectateButton->SetEnabled(false);
	}
	

	if (!RandomHUB.IsValid() && AllInternetServers.Num() > 0)
	{
		int32 NumPlayers = 0;
		int32 NumSpectators = 0;
		int32 NumFriends = 0;
		TallyInternetServers(NumPlayers, NumSpectators, NumFriends);

		RandomHUB = FServerData::Make( TEXT("[Internet] Individual Servers"), TEXT("@RandomServers"), TEXT("ALL"), LOBBY_GAME_PATH, TEXT("HUB"), TEXT(""),NumPlayers,NumSpectators,0,0,AllInternetServers.Num(),0,0,TEXT(""),0,0x00,0);
		RandomHUB->NumFriends = NumFriends;
		RandomHUB->MOTD = TEXT("Browse a random collection of servers on the internet.");
		RandomHUB->bFakeHUB = true;

		AllHubServers.Add( RandomHUB );
		FilterAllHUBs();
		HUBServerList->RequestListRefresh();
	}

	if (RandomHUB.IsValid() && RandomHUB->NumMatches != AllInternetServers.Num())
	{
		int32 NumPlayers = 0;
		int32 NumSpectators = 0;
		int32 NumFriends = 0;
		TallyInternetServers(NumPlayers, NumSpectators, NumFriends);

		AllHubServers.Remove(RandomHUB);
		RandomHUB = FServerData::Make( TEXT("[Internet] Individual Servers"), TEXT("@RandomServers"), TEXT("ALL"), LOBBY_GAME_PATH, TEXT("HUB"), TEXT(""),NumPlayers,NumSpectators,0,0,AllInternetServers.Num(),0,0,TEXT(""),0,0x00,0);
		RandomHUB->NumFriends = NumFriends;
		RandomHUB->MOTD = TEXT("Browse a random collection of servers on the internet.");
		RandomHUB->bFakeHUB = true;
		AllHubServers.Add(RandomHUB);
		FilterAllHUBs();
		HUBServerList->RequestListRefresh();
	}		

}

void SUWServerBrowser::TallyInternetServers(int32& Players, int32& Spectators, int32& Friends)
{
	Players = 0;
	Spectators = 0;
	Friends = 0;

	for (int32 i=0;i<AllInternetServers.Num();i++)
	{
		Players += AllInternetServers[i]->NumPlayers;
		Spectators += AllInternetServers[i]->NumSpectators;
		Friends += AllInternetServers[i]->NumFriends;
	}
}


void SUWServerBrowser::VertSplitterResized()
{
	UUTGameUserSettings* GS = Cast<UUTGameUserSettings>(GEngine->GetGameUserSettings());
	if (GS != NULL)
	{

		FChildren const* const SplitterChildren = VertSplitter->GetChildren();
		for (int32 SlotIndex = 0; SlotIndex < SplitterChildren->Num(); ++SlotIndex)
		{
			SSplitter::FSlot const& SplitterSlot = VertSplitter->SlotAt(SlotIndex);
			GS->SetServerBrowserSplitterPositions(0+SlotIndex, SplitterSlot.SizeValue.Get());
		}
		GS->SaveConfig();
		bRequireSave = false;
	}
}

void SUWServerBrowser::HorzSplitterResized()
{
	UUTGameUserSettings* GS = Cast<UUTGameUserSettings>(GEngine->GetGameUserSettings());
	if (GS != NULL)
	{

		FChildren const* const SplitterChildren = HorzSplitter->GetChildren();
		for (int32 SlotIndex = 0; SlotIndex < SplitterChildren->Num(); ++SlotIndex)
		{
			SSplitter::FSlot const& SplitterSlot = HorzSplitter->SlotAt(SlotIndex);
			GS->SetServerBrowserSplitterPositions(0+SlotIndex, SplitterSlot.SizeValue.Get());
		}
		GS->SaveConfig();
		bRequireSave = false;
	}
}

void SUWServerBrowser::OnServerListSelectionChanged(TSharedPtr<FServerData> SelectedItem, ESelectInfo::Type SelectInfo)
{
	if (SelectedItem.IsValid())
	{
		PingServer(SelectedItem);

		RulesListSource.Empty();
		for (int32 i=0;i<SelectedItem->Rules.Num();i++)
		{
			RulesListSource.Add(SelectedItem->Rules[i]);
		}
		RulesList->RequestListRefresh();

		PlayersListSource.Empty();
		if (SelectedItem.IsValid())
		{
			for (int32 i=0;i<SelectedItem->Players.Num();i++)
			{
				PlayersListSource.Add(SelectedItem->Players[i]);
			}
		}
		PlayersList->RequestListRefresh();

		JoinButton->SetEnabled(true);
		SpectateButton->SetEnabled(true);
	}
}

void SUWServerBrowser::OnQuickFilterTextCommited(const FText& NewText, ETextCommit::Type CommitType)
{
	if (CommitType == ETextCommit::OnEnter)
	{
		if (bShowingHubs)
		{
			FilterAllHUBs();
		}
		else
		{
			FilterAllServers();
		}
	}
}

FReply SUWServerBrowser::BrowserTypeChanged()
{
	ShowHUBs();
	return FReply::Handled();
}

void SUWServerBrowser::ShowServers(FString InitialGameType)
{
	bShowingHubs = false;
	BuildServerListControlBox();
	LobbyBrowser->SetVisibility(EVisibility::Hidden);
	InternetServerBrowser->SetVisibility(EVisibility::All);
	ServerListControlBox->SetVisibility(EVisibility::All);
	FilterAllServers();
	AddGameFilters();
	InternetServerList->RequestListRefresh();
}

void SUWServerBrowser::ShowHUBs()
{
	bShowingHubs = true;
	BuildServerListControlBox();
	LobbyBrowser->SetVisibility(EVisibility::All);
	InternetServerBrowser->SetVisibility(EVisibility::Hidden);
	ServerListControlBox->SetVisibility(EVisibility::Hidden);
	FilterAllHUBs();
	HUBServerList->RequestListRefresh();
}


TSharedRef<ITableRow> SUWServerBrowser::OnGenerateWidgetForHUBList(TSharedPtr<FServerData> InItem, const TSharedRef<STableViewBase>& OwnerTable )
{
	return SNew(STableRow<TSharedPtr<FServerData>>, OwnerTable)
		.Style(SUWindowsStyle::Get(),"UWindows.Standard.HUBBrowser.Row")
		[
			SNew(SBox)
			.HeightOverride(64)
			[
				SNew(SVerticalBox)
				+SVerticalBox::Slot()
				.Padding(10.0, 5.0, 10.0, 5.0)
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(0.0,0.0,5.0,0.0)
					[
						AddHUBBadge(InItem)
					]
					
					+SHorizontalBox::Slot()
					.FillWidth(1)
					[
						SNew(SVerticalBox)
						+SVerticalBox::Slot()
						.VAlign(VAlign_Top)
						.AutoHeight()
						[
							SNew(STextBlock)
							.Text(FText::FromString(InItem->Name))
							.TextStyle(SUWindowsStyle::Get(), "UWindows.Standard.HUBBrowser.TitleText")
						]
						+SVerticalBox::Slot()
						.FillHeight(1.0)
						.VAlign(VAlign_Fill)
						[
							SNew(SHorizontalBox)
							+SHorizontalBox::Slot()
							.FillWidth(1.0)
							.Padding(5.0,0.0,20.0,0.0)
							.VAlign(VAlign_Top)
							[
								SNew(SVerticalBox)
								+SVerticalBox::Slot()
								.HAlign(HAlign_Fill)
								.AutoHeight()
								[
									SNew(SHorizontalBox)
									+SHorizontalBox::Slot()
									.Padding(0.0,0.0,20.0,0.0)
									.VAlign(VAlign_Center)
									.AutoWidth()
									[
										SNew(SBox)
										.WidthOverride(125)
										.HeightOverride(24)
										[
											AddStars(InItem)
										]
									]

									+SHorizontalBox::Slot()
									.Padding(0.0,0.0,20.0,0.0)
									.AutoWidth()
									.VAlign(VAlign_Center)
									[
										SNew(STextBlock)
										.Text(InItem->bFakeHUB ? 
											TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateSP(InItem.Get(), &FServerData::GetNumServers)) :
											TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateSP(InItem.Get(), &FServerData::GetNumMatches)))
										.TextStyle(SUWindowsStyle::Get(), "UWindows.Standard.HUBBrowser.NormalText")
									]

									+SHorizontalBox::Slot()
									.Padding(0.0,0.0,20.0,0.0)
									.AutoWidth()
									.VAlign(VAlign_Center)
									[
										SNew(STextBlock)
										.Text(TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateSP(InItem.Get(), &FServerData::GetNumPlayers)))
										.TextStyle(SUWindowsStyle::Get(), "UWindows.Standard.HUBBrowser.NormalText")
									]

									+SHorizontalBox::Slot()
									.Padding(0.0,0.0,20.0,0.0)
									.AutoWidth()
									.VAlign(VAlign_Center)
									[
										SNew(STextBlock)
										.Text(TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateSP(InItem.Get(), &FServerData::GetNumFriends)))
										.TextStyle(SUWindowsStyle::Get(), "UWindows.Standard.HUBBrowser.NormalText")
									]
									+SHorizontalBox::Slot()
									.Padding(10.0,0.0,20.0,0.0)
									.FillWidth(1.0)
									.VAlign(VAlign_Center)
									[
										SNew(SVerticalBox)
										+SVerticalBox::Slot()
										.HAlign(HAlign_Right)
										[
											SNew(STextBlock)
											.Text(TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateSP(InItem.Get(), &FServerData::GetHubPing)))
											.TextStyle(SUWindowsStyle::Get(), "UWindows.Standard.HUBBrowser.SmallText")
										]
									]
								]
							]
						]
					]

				]
			]
		];
}

TSharedRef<SWidget> SUWServerBrowser::AddHUBBadge(TSharedPtr<FServerData> HUB)
{

	if (HUB->bFakeHUB || HUB->TrustLevel > 1)
	{
		return 	SNew(SBox)						// First the overlaid box that controls everything....
			.HeightOverride(54)
			.WidthOverride(54)
			[
				SNew(SImage)
				.Image(SUWindowsStyle::Get().GetBrush("UT.Icon.Globe"))
			];
	}
	else
	{
		if (HUB->TrustLevel == 0)
		{
			return 	SNew(SBox).HeightOverride(54).WidthOverride(54)
				[
					SNew(SImage)
					.Image(SUWindowsStyle::Get().GetBrush("UT.Icon.Epic"))
				];
		}
		else
		{
			return 	SNew(SBox).HeightOverride(54).WidthOverride(54)
				[
					SNew(SImage)
					.Image(SUWindowsStyle::Get().GetBrush("UT.Icon.Raxxy"))
				];
		}
	
	}

}

TSharedRef<SWidget> SUWServerBrowser::AddStars(TSharedPtr<FServerData> HUB)
{
	TSharedPtr<SHorizontalBox> StarBox;

	SAssignNew(StarBox, SHorizontalBox);

	if (StarBox.IsValid())
	{
		for (int32 i=0;i<5;i++)
		{
			StarBox->AddSlot()
			.AutoWidth()
			[
				SNew(SBox)
				.WidthOverride(24)
				.HeightOverride(24)
				[
					SNew(SImage)
					.Image(SUWindowsStyle::Get().GetBrush("UWindows.Standard.Star24"))
				]
			];
		}
	}

	return StarBox.ToSharedRef();
}

TSharedRef<SWidget> SUWServerBrowser::AddHUBInstances(TSharedPtr<FServerData> HUB)
{
	if (!HUB->bFakeHUB && HUB->HUBInstances.Num() > 0)
	{
		TSharedPtr<SVerticalBox> VBox;
		SAssignNew(VBox,SVerticalBox);
		TSharedPtr<SHorizontalBox> Box;
		int32 Column = 0;
		for (int32 i=0;i<HUB->HUBInstances.Num();i++)
		{

			HUB->HUBInstances[i]->BadgeTexture = LoadObject<UTexture2D>(nullptr, *HUB->HUBInstances[i]->RuleSetIcon, nullptr, LOAD_None, nullptr);
			HUB->HUBInstances[i]->SlateBadge = new FSlateDynamicImageBrush(HUB->HUBInstances[i]->BadgeTexture, FVector2D(256.0f, 256.0f), NAME_None);

			if (Column == 0)
			{
				VBox->AddSlot()
				.AutoHeight()
				.HAlign(HAlign_Left)
				[
					SAssignNew(Box, SHorizontalBox)
				];
			}

			Box->AddSlot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(5.0,0.0,5.0,5.0)
			[
				SNew(SDPIScaler)
				.DPIScale(0.75)
				[
					SNew(SBox)
					.WidthOverride(202)
					.HeightOverride(202)
					[
						SNew(SOverlay)
						+SOverlay::Slot()
						[
							SNew(SBox)
							.HeightOverride(192)
							.WidthOverride(192)
							[
								SNew(SImage)
								.Image(HUB->HUBInstances[i]->SlateBadge)
							]
						]
						+SOverlay::Slot()
						[
							SNew(SVerticalBox)
							+SVerticalBox::Slot()
							.HAlign(HAlign_Center)
							.VAlign(VAlign_Center)
							[
								SNew(SHorizontalBox)
								+SHorizontalBox::Slot()
								.Padding(5.0,5.0,5.0,5.0)
								.HAlign(HAlign_Center)
								[
									SNew(SRichTextBlock)
									.TextStyle(SUWindowsStyle::Get(),"UWindows.Chat.Text.Global")
									.Justification(ETextJustify::Center)
									.DecoratorStyleSet( &SUWindowsStyle::Get() )
									.AutoWrapText( true )
									.Text(FText::FromString(*HUB->HUBInstances[i]->Description))
								]
							]
						]
					]

				]
			];

			Column++;
			if (Column > 2) Column = 0;
		}
		return VBox.ToSharedRef();
	}

	return SNew(STextBlock)
		.Text(NSLOCTEXT("ServerBrowser","NoInstances","No Game Sessions Available"))
		.TextStyle(SUWindowsStyle::Get(), "UWindows.Standard.ServerBrowser.NormalText");
}

void SUWServerBrowser::AddHUBInfo(TSharedPtr<FServerData> HUB)
{
	LobbyInfoBox->ClearChildren();

	LobbyInfoBox->AddSlot()
	.AutoHeight()
	.Padding(5.0,5.0,5.0,5.0)
	[
		SNew(SRichTextBlock)
		.TextStyle(SUWindowsStyle::Get(),"MOTD.Normal")
		.Justification(ETextJustify::Center)
		.DecoratorStyleSet( &SUWindowsStyle::Get() )
		.AutoWrapText( true )
		.Text(FText::FromString(*HUB->MOTD))
	];

	LobbyInfoBox->AddSlot()
	.AutoHeight()
	.HAlign(HAlign_Center)
	.Padding(5.0,10.0,5.0,10.0)
	[
		SNew(STextBlock)	
		.Text(NSLOCTEXT("ServerBrowser","InstaceHeader","-- Available Matches --"))
		.TextStyle(SUWindowsStyle::Get(), "UWindows.Standard.ServerBrowser.BoldText")
	];

	LobbyInfoBox->AddSlot()
	.AutoHeight()
	.HAlign(HAlign_Center)
	.Padding(5.0,5.0,5.0,5.0)
	[
		AddHUBInstances(HUB)
	];

}

void SUWServerBrowser::OnHUBListSelectionChanged(TSharedPtr<FServerData> SelectedItem, ESelectInfo::Type SelectInfo)
{
	if (SelectedItem.IsValid())
	{
		if (!SelectedItem->bFakeHUB)
		{
			PingServer(SelectedItem);
		}

		AddHUBInfo(SelectedItem);
		JoinButton->SetEnabled(true);
	}
}

void SUWServerBrowser::BuildServerListControlBox()
{
	if (ServerListControlBox.IsValid())
	{
		ServerListControlBox->ClearChildren();
		if (!bShowingHubs)
		{
			ServerListControlBox->AddSlot()
				.VAlign(VAlign_Center)
				.Padding(0.0f,0.0f,15.0f,0.0f)
				.AutoWidth()
				[
					SNew(SButton)
					.ContentPadding(FMargin(10.0f, 5.0f, 15.0f, 5.0))
					.ButtonStyle(SUWindowsStyle::Get(), "UWindows.Standard.ServerBrowser.Button")
					[
						SNew(SHorizontalBox)
						+SHorizontalBox::Slot()
						.VAlign(VAlign_Center)
						.Padding(0.0f, 3.0f, 5.0f, 0.0f)
						.AutoWidth()
						[
							SNew(SImage)
							.Image(SUWindowsStyle::Get().GetBrush("UWindows.Standard.ServerBrowser.LeftArrow"))
						]
						+SHorizontalBox::Slot()
						.VAlign(VAlign_Center)
						.AutoWidth()
						[
							SAssignNew(BrowserTypeText, STextBlock)
							.Text(NSLOCTEXT("SUWServerBrowser","ShowLobbies","SHOW HUBS"))
							.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText")

						]
					]
					.OnClicked(this, &SUWServerBrowser::BrowserTypeChanged)
				];

			ServerListControlBox->AddSlot()
				.AutoWidth()
				[
					SAssignNew(GameFilter, SComboButton)
					.HasDownArrow(false)
					.ButtonStyle(SUWindowsStyle::Get(), "UWindows.Standard.ServerBrowser.BlankButton")
					.ContentPadding(FMargin(10.0f, 0.0f, 10.0f, 0.0))
					.ButtonContent()
					[
						SNew(SVerticalBox)
						+SVerticalBox::Slot()
						.HAlign(HAlign_Fill)
						[
							SNew(SHorizontalBox)
							+SHorizontalBox::Slot()
							.Padding(0.0f,0.0f,5.0f,0.0f)
							.AutoWidth()
							[
								SNew(STextBlock)
								.Text(NSLOCTEXT("SUWServerBrowser","GameFilter","Game Mode:"))
								.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText")
							]
							+SHorizontalBox::Slot()
							.AutoWidth()
							[
								SAssignNew(GameFilterText, STextBlock)
								.TextStyle(SUWindowsStyle::Get(), "UT.Common.BoldText")
							]
						]
					]
				];
		}
	}

}

void SUWServerBrowser::OnShowPanel(TSharedPtr<SUWindowsDesktop> inParentWindow)
{
	SUWPanel::OnShowPanel(inParentWindow);
	if (bNeedsRefresh)
	{
		RefreshServers();
	}
}

FName SUWServerBrowser::GetBrowserState()
{
	return BrowserState;
}


#endif
