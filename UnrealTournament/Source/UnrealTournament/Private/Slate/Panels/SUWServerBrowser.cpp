// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTLocalPlayer.h"
#include "Slate.h"
#include "Slate/SlateGameResources.h"
#include "../SUWPanel.h"
#include "../SUWindowsDesktop.h"
#include "../SUWindowsStyle.h"
#include "SUWServerBrowser.h"
#include "Online.h"
#include "UTOnlineGameSearchBase.h"
#include "UTOnlineGameSettingsBase.h"
#include "OnlineSubsystemTypes.h"
#include "../SUWMessageBox.h"
#include "../SUWInputBox.h"
#include "UTGameEngine.h"
#include "UTServerBeaconClient.h"


#if !UE_SERVER
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

struct FCompareRulesByRule		{FORCEINLINE bool operator()( const TSharedPtr< FServerRuleData > A, const TSharedPtr< FServerRuleData > B ) const {return ( A->Rule > B->Rule);	}};
struct FCompareRulesByRuleDesc	{FORCEINLINE bool operator()( const TSharedPtr< FServerRuleData > A, const TSharedPtr< FServerRuleData > B ) const {return ( A->Rule < B->Rule);	}};
struct FCompareRulesByValue		{FORCEINLINE bool operator()( const TSharedPtr< FServerRuleData > A, const TSharedPtr< FServerRuleData > B ) const {return ( A->Value > B->Value);	}};
struct FCompareRulesByValueDesc	{FORCEINLINE bool operator()( const TSharedPtr< FServerRuleData > A, const TSharedPtr< FServerRuleData > B ) const {return ( A->Value < B->Value);	}};

struct FComparePlayersByName		{FORCEINLINE bool operator()( const TSharedPtr< FServerPlayerData > A, const TSharedPtr< FServerPlayerData > B ) const {return ( A->PlayerName > B->PlayerName);	}};
struct FComparePlayersByNameDesc	{FORCEINLINE bool operator()( const TSharedPtr< FServerPlayerData > A, const TSharedPtr< FServerPlayerData > B ) const {return ( A->PlayerName < B->PlayerName);	}};
struct FComparePlayersByScore		{FORCEINLINE bool operator()( const TSharedPtr< FServerPlayerData > A, const TSharedPtr< FServerPlayerData > B ) const {return ( A->Score > B->Score);	}};
struct FComparePlayersByScoreDesc	{FORCEINLINE bool operator()( const TSharedPtr< FServerPlayerData > A, const TSharedPtr< FServerPlayerData > B ) const {return ( A->Score < B->Score);	}};

void SUWServerBrowser::BuildPage(FVector2D ViewportSize)
{
	Tag = FName(TEXT("ServerBrowser"));

	bShowingLobbies = false;
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
				.HeightOverride(32)
				[
					SNew( SHorizontalBox )

					+SHorizontalBox::Slot()
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
								.Text(NSLOCTEXT("SUWServerBrowser","ShowLobbies","Show Lobbies"))
								.TextStyle(SUWindowsStyle::Get(), "UWindows.Standard.ServerBrowser.NormalText")

							]
						]
						.OnClicked(this, &SUWServerBrowser::BrowserTypeChanged)
					]


					+SHorizontalBox::Slot()
					.AutoWidth()
					[
						SAssignNew(GameFilter, SComboButton)
						.Method(SMenuAnchor::UseCurrentWindow)
						.HasDownArrow(false)
						.ButtonStyle(SUWindowsStyle::Get(), "UWindows.Standard.ServerBrowser.Button")
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
									.TextStyle(SUWindowsStyle::Get(), "UWindows.Standard.ServerBrowser.NormalText")
								]
								+SHorizontalBox::Slot()
								.AutoWidth()
								[
									SAssignNew(GameFilterText, STextBlock)
									.TextStyle(SUWindowsStyle::Get(), "UWindows.Standard.ServerBrowser.BoldText")
								]
							]
						]
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
							.HeightOverride(32)
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
							[
								SNew(SHorizontalBox)
								+SHorizontalBox::Slot()
								.AutoWidth()
								.Padding(20.0f, 0.0f, 20.0f, 0.0f)
								[
									SNew(STextBlock)
									.Text(NSLOCTEXT("SUWServerBrowser","QuickFilter","Search:"))
									.TextStyle(SUWindowsStyle::Get(), "UWindows.Standard.ServerBrowser.NormalText")
								]
								+SHorizontalBox::Slot()
								[
									SNew(SBox)
									.HeightOverride(26)
									[
										SAssignNew(QuickFilterText, SEditableTextBox)
										.Style(SUWindowsStyle::Get(),"UWindows.Standard.ServerBrowser.EditBox")
										.OnTextCommitted(this, &SUWServerBrowser::OnQuickFilterTextCommited)
										.ClearKeyboardFocusOnCommit(false)
										.MinDesiredWidth(300.0f)
										.Text(FText::GetEmpty())
									]
								]
							]
						]
					]
				]
			]

			// The Server Browser itself
			+SVerticalBox::Slot()
			.FillHeight(1)
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
				.HeightOverride(32)
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
							.TextStyle(SUWindowsStyle::Get(), "UWindows.Standard.ServerBrowser.NormalText")
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
							]
						]
					]
					+SHorizontalBox::Slot().AutoWidth()
					[
						// Press rebuild to clear out the old data items and create the new ones (however many are specified by SEditableTextBox)
						SAssignNew(SpectateButton, SButton)
						.ButtonStyle(SUWindowsStyle::Get(), "UWindows.Standard.ServerBrowser.BlankButton")
						.ContentPadding(FMargin(10.0f, 5.0f, 10.0f, 5.0))
						.Text(NSLOCTEXT("SUWServerBrowser","Spectate","Spectate"))
						.TextStyle(SUWindowsStyle::Get(), "UWindows.Standard.ServerBrowser.NormalText")
						.OnClicked(this, &SUWServerBrowser::OnJoinClick,true)
					]
					+SHorizontalBox::Slot().AutoWidth() .VAlign(VAlign_Center)
					[
						// Press rebuild to clear out the old data items and create the new ones (however many are specified by SEditableTextBox)
						SAssignNew(JoinButton, SButton)
						.ButtonStyle(SUWindowsStyle::Get(), "UWindows.Standard.ServerBrowser.RightButton")
						.ContentPadding(FMargin(10.0f, 5.0f, 10.0f, 5.0))
						.Text(NSLOCTEXT("SUWServerBrowser","Join","Join"))
						.TextStyle(SUWindowsStyle::Get(), "UWindows.Standard.ServerBrowser.NormalText")
						.OnClicked(this, &SUWServerBrowser::OnJoinClick,false)
					]
				]
			]
		]

	];

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
	FilterServers();

*/
	InternetServerList->RequestListRefresh();

	if (PlayerOwner->IsLoggedIn())
	{
		SetBrowserState(BrowserState::NAME_ServerIdle);
	}
	else
	{	
		SetBrowserState(BrowserState::NAME_NotLoggedIn);
	}

	OnRefreshClick();

	PlayerOnlineStatusChangedDelegate.BindSP(this, &SUWServerBrowser::OwnerLoginStatusChanged);
	PlayerOwner->AddPlayerLoginStatusChangedDelegate(PlayerOnlineStatusChangedDelegate);

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

	AddGameFilters();
}

TSharedRef<SWidget> SUWServerBrowser::BuildServerBrowser()
{
	return SAssignNew(InternetServerBrowser, SBox) 
			.HeightOverride(500.0f)
			[
				SAssignNew( VertSplitter, SSplitter )
				.Orientation(Orient_Horizontal)
				.OnSplitterFinishedResizing(this, &SUWServerBrowser::VertSplitterResized)

				+ SSplitter::Slot()
				.Value(0.99)
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
							.OnSelectionChanged(this, &SUWServerBrowser::OnServerListSelectionChanged)
							.SelectionMode(ESelectionMode::Single)

							.HeaderRow
							(
								SAssignNew(HeaderRow, SHeaderRow) 
								.Style(SUWindowsStyle::Get(),"UWindows.Standard.ServerBrowser.Header")

								+ SHeaderRow::Column("ServerName")
										.OnSort(this, &SUWServerBrowser::OnSort)
										.HeaderContent()
										[
											SNew(STextBlock)
												.Text(NSLOCTEXT("SUWServerBrowser","ServerNameColumn", "Server Name"))
												.ToolTipText( NSLOCTEXT("SUWServerBrowser","ServerNameColumnToolTip", "The name of this server.") )
												.TextStyle(SUWindowsStyle::Get(), "UWindows.Standard.ServerBrowser.Header.TextStyle")
										]

								+ SHeaderRow::Column("ServerIP") 
										.DefaultLabel(NSLOCTEXT("SUWServerBrowser","ServerIPColumn", "IP")) 
										.HAlignCell(HAlign_Center) 
										.HAlignHeader(HAlign_Center)
										.OnSort(this, &SUWServerBrowser::OnSort)
										.HeaderContent()
										[
											SNew(STextBlock)
												.Text(NSLOCTEXT("SUWServerBrowser","ServerIPColumn", "IP"))
												.ToolTipText( NSLOCTEXT("SUWServerBrowser","ServerIPColumnToolTip", "This server's IP address.") )
												.TextStyle(SUWindowsStyle::Get(), "UWindows.Standard.ServerBrowser.Header.TextStyle")
										]

								+ SHeaderRow::Column("ServerGame") 
										.DefaultLabel(NSLOCTEXT("SUWServerBrowser","ServerGameColumn", "Game"))
										.HAlignCell(HAlign_Center) 
										.HAlignHeader(HAlign_Center)
										.OnSort(this, &SUWServerBrowser::OnSort)
										.HeaderContent()
										[
											SNew(STextBlock)
												.Text(NSLOCTEXT("SUWServerBrowser","ServerGameColumn", "Game"))
												.ToolTipText( NSLOCTEXT("SUWServerBrowser","ServerGameColumnToolTip", "The Game type.") )
												.TextStyle(SUWindowsStyle::Get(), "UWindows.Standard.ServerBrowser.Header.TextStyle")
										]

								+ SHeaderRow::Column("ServerMap") 
										.DefaultLabel(NSLOCTEXT("SUWServerBrowser","ServerMapColumn", "Map"))
										.HAlignCell(HAlign_Center) 
										.HAlignHeader(HAlign_Center)
										.OnSort(this, &SUWServerBrowser::OnSort)
										.HeaderContent()
										[
											SNew(STextBlock)
												.Text(NSLOCTEXT("SUWServerBrowser","ServerMapColumn", "Map"))
												.ToolTipText( NSLOCTEXT("SUWServerBrowser","ServerMapColumnToolTip", "The current map.") )
												.TextStyle(SUWindowsStyle::Get(), "UWindows.Standard.ServerBrowser.Header.TextStyle")
										]

								+ SHeaderRow::Column("ServerNumPlayers") 
										.DefaultLabel(NSLOCTEXT("SUWServerBrowser","ServerNumPlayerColumn", "Players"))
										.HAlignCell(HAlign_Center) 
										.HAlignHeader(HAlign_Center)
										.OnSort(this, &SUWServerBrowser::OnSort)
										.HeaderContent()
										[
											SNew(STextBlock)
												.Text(NSLOCTEXT("SUWServerBrowser","ServerNumPlayerColumn", "Players"))
												.ToolTipText( NSLOCTEXT("SUWServerBrowser","ServerNumPlayerColumnToolTip", "The # of Players on this server.") )
												.TextStyle(SUWindowsStyle::Get(), "UWindows.Standard.ServerBrowser.Header.TextStyle")
										]
							
								+ SHeaderRow::Column("ServerNumSpecs") 
										.DefaultLabel(NSLOCTEXT("SUWServerBrowser","ServerNumSpecsColumn", "Spectators"))
										.HAlignCell(HAlign_Center) 
										.HAlignHeader(HAlign_Center)
										.OnSort(this, &SUWServerBrowser::OnSort)
										.HeaderContent()
										[
											SNew(STextBlock)
												.Text(NSLOCTEXT("SUWServerBrowser","ServerNumSpecsColumn", "Spectators"))
												.ToolTipText( NSLOCTEXT("SUWServerBrowser","ServerNumSpecsColumnToolTip", "The # of spectators on this server.") )
												.TextStyle(SUWindowsStyle::Get(), "UWindows.Standard.ServerBrowser.Header.TextStyle")
										]

								+ SHeaderRow::Column("ServerVer") 
										.DefaultLabel(NSLOCTEXT("SUWServerBrowser","ServerVerColumn", "Version"))
										.HAlignCell(HAlign_Center) 
										.HAlignHeader(HAlign_Center)
										.OnSort(this, &SUWServerBrowser::OnSort)
										.HeaderContent()
										[
											SNew(STextBlock)
												.Text(NSLOCTEXT("SUWServerBrowser","ServerVerColumn", "Version"))
												.ToolTipText( NSLOCTEXT("SUWServerBrowser","ServerVerColumnToolTip", "The version of UT this server is running.") )
												.TextStyle(SUWindowsStyle::Get(), "UWindows.Standard.ServerBrowser.Header.TextStyle")
										]

								+ SHeaderRow::Column("ServerFlags") 
										.DefaultLabel(NSLOCTEXT("SUWServerBrowser","ServerFlagsColumn", "Flags"))
										.HAlignCell(HAlign_Center) 
										.HAlignHeader(HAlign_Center)
										.OnSort(this, &SUWServerBrowser::OnSort)
										.HeaderContent()
										[
											SNew(STextBlock)
												.Text(NSLOCTEXT("SUWServerBrowser","ServerFlagsColumn", "Flags"))
												.ToolTipText( NSLOCTEXT("SUWServerBrowser","ServerFlagsColumnToolTip", "Server Flags") )
												.TextStyle(SUWindowsStyle::Get(), "UWindows.Standard.ServerBrowser.Header.TextStyle")
										]
								
								+ SHeaderRow::Column("ServerPing") 
										.DefaultLabel(NSLOCTEXT("SUWServerBrowser","ServerPingColumn", "Ping"))
										.HAlignCell(HAlign_Right) 
										.HAlignHeader(HAlign_Right)
										.OnSort(this, &SUWServerBrowser::OnSort)
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
				+ SSplitter::Slot()
				. Value(0.019)
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
						SNew(SBorder)
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
						]
					]

				]

			];
}

TSharedRef<SWidget> SUWServerBrowser::BuildLobbyBrowser()
{
	return SAssignNew(LobbyBrowser, SBox) 
			.Visibility(EVisibility::Hidden)
			.HeightOverride(500.0f)
			[
				SNew(SVerticalBox)
				+SVerticalBox::Slot()
				.VAlign(VAlign_Fill)
				.HAlign(HAlign_Center)
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.VAlign(VAlign_Center)
					.AutoWidth()
					[
						SNew(STextBlock)
						.Text(NSLOCTEXT("SUWServerBrowser","LobbyComingSoon", "!! Coming Soon !!"))
						.TextStyle(SUWindowsStyle::Get(), "UWindows.Standard.ServerBrowser.HugeText")
					]
				]
			];
}


void SUWServerBrowser::OwnerLoginStatusChanged(UUTLocalPlayer* LocalPlayerOwner, ELoginStatus::Type NewStatus, const FUniqueNetId& UniqueID)
{
	if (NewStatus == ELoginStatus::LoggedIn)
	{
		RefreshButton->SetContent( SNew(STextBlock).Text(NSLOCTEXT("SUWServerBrowser","Refresh","Refresh")).TextStyle(SUWindowsStyle::Get(), "UWindows.Standard.ServerBrowser.NormalText"));
		if (bAutoRefresh)
		{
			SetBrowserState(BrowserState::NAME_ServerIdle);
			bAutoRefresh = false;
			OnRefreshClick();
		}
	}
	else
	{
		SetBrowserState(BrowserState::NAME_NotLoggedIn);
	}
}


void SUWServerBrowser::AddGameFilters()
{
	TArray<FString> GameTypes;
	GameTypes.Add(TEXT("All"));
	for (int i=0;i<PingList.Num();i++)
	{
		int32 idx = GameTypes.Find(PingList[i]->GameMode);
		if (idx < 0)
		{
			GameTypes.Add(PingList[i]->GameMode);
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

void SUWServerBrowser::SetBrowserState(FName NewBrowserState)
{
	BrowserState = NewBrowserState;
	if (BrowserState == BrowserState::NAME_NotLoggedIn) 
	{
		StatusText->SetText(NSLOCTEXT("SUWServerBrowser","NotLoggedIn","Login Required!"));
		RefreshButton->SetContent( SNew(STextBlock).Text(NSLOCTEXT("SUWServerBrowser","Login","Login")).TextStyle(SUWindowsStyle::Get(), "UWindows.Standard.ServerBrowser.NormalText"));
		RefreshButton->SetVisibility(EVisibility::All);

		JoinButton->SetEnabled(false);
		SpectateButton->SetEnabled(false);

	}
	else if (BrowserState == BrowserState::NAME_ServerIdle) 
	{
		StatusText->SetText(NSLOCTEXT("SUWServerBrowser","Idle","Idle!"));
		RefreshButton->SetVisibility(EVisibility::All);
	}
	else if (BrowserState == BrowserState::NAME_AuthInProgress) 
	{
		StatusText->SetText(NSLOCTEXT("SUWServerBrowser","Auth","Authenticating..."));
		RefreshButton->SetVisibility(EVisibility::Hidden);

		JoinButton->SetEnabled(false);
		SpectateButton->SetEnabled(false);
	}
	else if (BrowserState == BrowserState::NAME_RequestInProgress) 
	{
		StatusText->SetText(NSLOCTEXT("SUWServerBrowser","RecieveingServer","Receiving Server List..."));
		RefreshButton->SetVisibility(EVisibility::Hidden);

		InternetServers.Empty();
		FilteredServers.Empty();
		PlayersListSource.Empty();
		RulesListSource.Empty();

		InternetServerList->RequestListRefresh();
		PlayersList->RequestListRefresh();
		RulesList->RequestListRefresh();

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
	if (PlayerOwner->IsLoggedIn() && OnlineSessionInterface.IsValid() && BrowserState == BrowserState::NAME_ServerIdle)
	{
		SetBrowserState(BrowserState::NAME_RequestInProgress);

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
		SetBrowserState(BrowserState::NAME_NotLoggedIn);	
	}
}

void SUWServerBrowser::OnFindSessionsComplete(bool bWasSuccessful)
{
	CleanupQoS();

	if (bWasSuccessful)
	{
		// See the server list
		InternetServers.Empty();
		FilteredServers.Empty();
		PingList.Empty();

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
				int32 ServerFlags = 0x0000;
				Result.Session.SessionSettings.Get(SETTING_SERVERFLAGS, ServerFlags);
				uint32 ServerPing = 0;

				FString BeaconIP;
				OnlineSessionInterface->GetResolvedConnectString(Result,FName(TEXT("BeaconPort")), BeaconIP);
				TSharedRef<FServerData> NewServer = FServerData::Make( ServerName, ServerIP, BeaconIP, ServerGame, ServerMap, ServerNoPlayers, ServerNoSpecs, ServerMaxPlayers, ServerVer, ServerPing, ServerFlags);
				PingList.Add( NewServer );
			}
		}

/*
		UE_LOG(UT,Log,TEXT("Recieved a list of %i Servers....."), PingList.Num());
		for (int i=0;i<PingList.Num();i++)
		{
			UE_LOG(UT,Log,TEXT("Recieved Server %i - %s %s"), i, *PingList[i]->Name, *PingList[i]->IP);
		}
		UE_LOG(UT,Log, TEXT("----------------------------------------------"));
*/
		AddGameFilters();
		InternetServerList->RequestListRefresh();
		PingNextServer();
	}
	else
	{
		UE_LOG(UT,Log,TEXT("Server List Request Failed!!!"));
	}

	SetBrowserState(BrowserState::NAME_ServerIdle);	
}

void SUWServerBrowser::CleanupQoS()
{
	for (int i=0;i<PingTrackers.Num();i++)
	{
		PingTrackers[i].Beacon->DestroyBeacon();
	}
	PingTrackers.Empty();
}

void SUWServerBrowser::PingNextServer()
{
	while (PingList.Num() > 0 && PingTrackers.Num() < 10)
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
		FString BeaconNetDriverName = FString::Printf(TEXT("BeaconDriver%s"), *ServerToPing->BeaconIP);
		Beacon->SetBeaconNetDriverName(BeaconNetDriverName);
		Beacon->OnServerRequestResults = FServerRequestResultsDelegate::CreateSP(this, &SUWServerBrowser::OnServerBeaconResult );
		Beacon->OnServerRequestFailure = FServerRequestFailureDelegate::CreateSP(this, &SUWServerBrowser::OnServerBeaconFailure);
		FURL BeaconURL(nullptr, *ServerToPing->BeaconIP, TRAVEL_Absolute);
		Beacon->InitClient(BeaconURL);
		PingTrackers.Add(FServerPingTracker(ServerToPing, Beacon));
	}
}

void SUWServerBrowser::OnServerBeaconFailure(AUTServerBeaconClient* Sender)
{
	for (int32 i=0; i < PingTrackers.Num(); i++)
	{
		if (PingTrackers[i].Beacon == Sender)
		{
			PingTrackers[i].Beacon->DestroyBeacon();
			PingTrackers.RemoveAt(i,1);
			return;
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

			TArray<FString> PlayerData;
			int Cnt = ServerInfo.ServerPlayers.ParseIntoArray(&PlayerData, TEXT("\t"), true);
			for (int p=0;p+2 < Cnt; p+=3)
			{
				FString Name = PlayerData[p];
				FString Score = PlayerData[p+1];
				FString Id = PlayerData[p+2];

				PingTrackers[i].Server->AddPlayer(Name, Score, Id);
			}

			TArray<FString> RulesData;
			Cnt = ServerInfo.ServerRules.ParseIntoArray(&RulesData, TEXT("\t"), true);
			for (int r=0; r+1 < Cnt; r+=2)
			{
				FString Rule = RulesData[r];
				FString Value = RulesData[r+1];

				PingTrackers[i].Server->AddRule(Rule, Value);
			}

			InternetServers.Add(PingTrackers[i].Server);

			FilterServer(PingTrackers[i].Server);

			//FilteredServers.Add(PingTrackers[i].Server);

			PingTrackers[i].Beacon->DestroyBeacon();
			PingTrackers.RemoveAt(i,1);

			// Look to see if there are more servers to Ping...
			PingNextServer();
		}
	}

	InternetServerList->RequestListRefresh();
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
	CleanupQoS();
	PlayerOwner->HideMenu();
	FString Command = FString::Printf(TEXT("open %s"), *ServerData.IP);
	if (bSpectate)
	{
		Command += FString(TEXT("?spectatoronly=1"));
	}
	PlayerOwner->Exec(PlayerOwner->GetWorld(), *Command, *GLog);
}

void SUWServerBrowser::FilterAllServers()
{
	FilteredServers.Empty();
	for (int32 i=0;i<InternetServers.Num();i++)
	{
		FilterServer(InternetServers[i], false);
	}
	SortServers(CurrentSortColumn);
}

void SUWServerBrowser::FilterServer(TSharedPtr< FServerData > NewServer, bool bSortAndUpdate)
{
	FString GameFilter = GameFilterText->GetText().ToString();
	if (GameFilter.IsEmpty() || GameFilter == TEXT("All") || NewServer->GameMode == GameFilter)
	{
		if (QuickFilterText->GetText().IsEmpty() || NewServer->Name.Find(QuickFilterText->GetText().ToString()) >= 0)
		{
			FilteredServers.Add(NewServer);
		}
	}

	if (bSortAndUpdate)
	{
		SortServers(CurrentSortColumn);
	}
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

	if (BrowserState != BrowserState::NAME_ServerIdle) 
	{
		JoinButton->SetEnabled(false);
		SpectateButton->SetEnabled(false);
	}
	else
	{
		if (PingList.Num() > 0)
		{
			StatusText->SetText( FText::Format( NSLOCTEXT("SUWServerBrowser","PingingMsg","Pinging {0} Servers..."), FText::AsNumber(PingList.Num() + PingTrackers.Num())));
		}
		else
		{
			StatusText->SetText(FText::Format(NSLOCTEXT("SUWServerBrowser","ServerCounts","Showing {0} servers out of {1} total recieved."), FText::AsNumber(FilteredServers.Num()), FText::AsNumber(InternetServers.Num())));
		}
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
		RulesListSource.Empty();
		for (int i=0;i<SelectedItem->Rules.Num();i++)
		{
			RulesListSource.Add(SelectedItem->Rules[i]);
		}
		RulesList->RequestListRefresh();

		PlayersListSource.Empty();
		if (SelectedItem.IsValid())
		{
			for (int i=0;i<SelectedItem->Players.Num();i++)
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
		FilterAllServers();
	}
}

FReply SUWServerBrowser::BrowserTypeChanged()
{
	if (bShowingLobbies)
	{
		bShowingLobbies = false;
		LobbyBrowser->SetVisibility(EVisibility::Hidden);
		InternetServerBrowser->SetVisibility(EVisibility::All);
		BrowserTypeText->SetText(NSLOCTEXT("SUWServerBrowser","ShowLobbies","Show Lobbies"));
	}
	else
	{
		bShowingLobbies = true;
		LobbyBrowser->SetVisibility(EVisibility::All);
		InternetServerBrowser->SetVisibility(EVisibility::Hidden);
		BrowserTypeText->SetText(NSLOCTEXT("SUWServerBrowser","ShowServers","Show Servers"));
	}

	return FReply::Handled();
}

#endif
