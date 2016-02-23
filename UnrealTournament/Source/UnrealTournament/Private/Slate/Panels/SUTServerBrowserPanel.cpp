// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTLocalPlayer.h"
#include "SlateBasics.h"
#include "SlateExtras.h"
#include "Slate/SlateGameResources.h"
#include "SUTServerBrowserPanel.h"
#include "../SUTStyle.h"
#include "Online.h"
#include "UTBaseGameMode.h"
#include "UTOnlineGameSearchBase.h"
#include "UTOnlineGameSettingsBase.h"
#include "OnlineSubsystemTypes.h"
#include "../Dialogs/SUTMessageBoxDialog.h"
#include "../Dialogs/SUTInputBoxDialog.h"
#include "UTGameEngine.h"
#include "UTServerBeaconClient.h"
#include "../Widgets/SUTScaleBox.h"
#include "../Widgets/SUTBorder.h"
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

struct FCompareHub
{
	FORCEINLINE bool operator()	( const TSharedPtr< FServerData > A, const TSharedPtr< FServerData > B ) const 
	{
		// Sort works like this. There are 4 categories, Ping Group, Trust Level, Emptiness and actual ping.  
		// Ping group is worth 1000 - 3000
		// Trust Level is worth hundreds 100-300
		// Emptiness is worth 20 - 99
		// Actual ping is the decimal (so Ping / 1000.0)

		// Lower numbers are better!

		float AValue = ( 	
							(A->Ping <= 80) ? 1000 : ( (A->Ping <= 200) ? 2000 : 3000) ) +
							(A->TrustLevel == 0 ? 100.0f : ( (A->TrustLevel == 1) ? 200.0f : 300.0f)) + 
							(A->NumFriends > 0 ? 20 : ((int32(1.0 - FMath::Clamp<float>( float(A->NumPlayers) / 600.0, 0.0f, 1.0f)) * 79) + 20) +
							(float(A->Ping) / 1000.0f) 
						);

		float BValue = (	
							(B->Ping <= 80) ? 1000 : ((B->Ping <= 200) ? 2000 : 3000)) +
							(B->TrustLevel == 0 ? 100.0f : ((B->TrustLevel == 1) ? 200.0f : 300.0f)) +
							(B->NumFriends > 0 ? 20 : ((int32(1.0 - FMath::Clamp<float>(float(B->NumPlayers) / 600.0, 0.0f, 1.0f)) * 79) + 20) +
							(float(B->Ping) / 1000.0f)
						);

		return AValue < BValue;
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

// Const Variables
const int SUTServerBrowserPanel::MAXCHARCOUNTFORSERVERFILTER = 150;

SUTServerBrowserPanel::~SUTServerBrowserPanel()
{
	if (PlayerOwner.IsValid())
	{
		PlayerOwner->RemovePlayerOnlineStatusChangedDelegate(PlayerOnlineStatusChangedDelegate);
		PlayerOnlineStatusChangedDelegate.Reset();
	}
}

void SUTServerBrowserPanel::ConstructPanel(FVector2D ViewportSize)
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
		SAssignNew(AnimWidget, SUTBorder).OnAnimEnd(this, &SUTServerBrowserPanel::AnimEnd)
		.BorderImage(SUTStyle::Get().GetBrush("UT.HeaderBackground.Dark"))
		.BorderBackgroundColor(FLinearColor(1.0f,1.0f,1.0f,0.85f))
		[
			SNew( SVerticalBox )
		
			 //Login panel

			+SVerticalBox::Slot()
			.AutoHeight()
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
						SNew(SBox).WidthOverride(300)
						[
							SNew(SHorizontalBox)
							+SHorizontalBox::Slot()
							.VAlign(VAlign_Center)
							.Padding(0.0f, 0.0f, 15.0f, 0.0f)
							.AutoWidth()
							[
								SAssignNew(HubButton, SUTButton)
								.ContentPadding(FMargin(10.0f, 5.0f, 15.0f, 5.0))
								.ButtonStyle(SUTStyle::Get(), "UT.Button.MenuBar")
								.OnClicked(this, &SUTServerBrowserPanel::BrowserTypeChanged, 0)
								.IsToggleButton(true)
								[
									SNew(SHorizontalBox)
									+ SHorizontalBox::Slot()
									.VAlign(VAlign_Center)
									.AutoWidth()
									.Padding(0.0f, 0.0f, 5.0f, 0.0f)
									[
										SNew(STextBlock)
										.Text(NSLOCTEXT("SUTServerBrowserPanel", "HubList", "Hubs"))
										.ColorAndOpacity(this, &SUTServerBrowserPanel::GetButtonSlateColor, 0)
										.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Medium")
									]
									+ SHorizontalBox::Slot()
									.VAlign(VAlign_Top)
									.AutoWidth()
									[
										SNew(STextBlock)
										.Text(this, &SUTServerBrowserPanel::GetShowHubButtonText)
										.ColorAndOpacity(this, &SUTServerBrowserPanel::GetButtonSlateColor, 0)
										.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Small")
									]

								]
							]

							+SHorizontalBox::Slot()
							.VAlign(VAlign_Center)
							.Padding(0.0f, 0.0f, 15.0f, 0.0f)
							.AutoWidth()
							[
								SAssignNew(ServerButton, SUTButton)
								.ContentPadding(FMargin(10.0f, 5.0f, 15.0f, 5.0))
								.ButtonStyle(SUTStyle::Get(), "UT.Button.MenuBar")
								.OnClicked(this, &SUTServerBrowserPanel::BrowserTypeChanged, 1)
								.IsToggleButton(true)
								[
									SNew(SHorizontalBox)
									+ SHorizontalBox::Slot()
									.VAlign(VAlign_Center)
									.AutoWidth()
									.Padding(0.0f, 0.0f, 5.0f, 0.0f)
									[
										SNew(STextBlock)
										.Text(NSLOCTEXT("SUTServerBrowserPanel", "ServerList", "Servers"))
										.ColorAndOpacity(this, &SUTServerBrowserPanel::GetButtonSlateColor, 1)
										.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Medium")
									]
									+ SHorizontalBox::Slot()
									.VAlign(VAlign_Top)
									.AutoWidth()
									[
										SNew(STextBlock)
										.Text(this, &SUTServerBrowserPanel::GetShowServerButtonText)
										.ColorAndOpacity(this, &SUTServerBrowserPanel::GetButtonSlateColor, 1)
										.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Small")
									]
								]
							]
						]
					]

					+SHorizontalBox::Slot()
					.Padding(0.0f, 0.0f, 0.0f, 0.0f)
					.VAlign(VAlign_Fill)
					.HAlign(HAlign_Fill)
					[
						SNew(SBorder)
						.BorderImage(SUTStyle::Get().GetBrush("UT.HeaderBackground.Medium"))
						[
							SNew(SVerticalBox)
							+SVerticalBox::Slot()
							.VAlign(VAlign_Center)
							.HAlign(HAlign_Fill)
							.Padding(15.0f, 0.0f, 16.0f, 0.0f)
							[
								SNew(SHorizontalBox)
								+SHorizontalBox::Slot()
								.VAlign(VAlign_Center)
								.AutoWidth()
								[
									SAssignNew(ServerListControlBox, SHorizontalBox)
								]
								+SHorizontalBox::Slot()
								.VAlign(VAlign_Center)
								.AutoWidth()
								[
									SNew(STextBlock)
									.Text(NSLOCTEXT("SUTServerBrowserPanel","QuickFilter","Filter Results by:"))
									.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Small")
								]
								+SHorizontalBox::Slot()
								.Padding(16.0f, 0.0f, 0.0f, 0.0f)
								.FillWidth(1.0)
								[
									SNew(SBox)
									.HeightOverride(36)
									[

										SNew(SOverlay)
										+SOverlay::Slot()
										[
											SNew(SVerticalBox)
											+SVerticalBox::Slot()
											.Padding(8.0, 5.0)
											[
												SAssignNew(FilterMsg, STextBlock)
												.Text(FText::FromString(TEXT("type your filter text here")))
												.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Small")
												.ColorAndOpacity(FLinearColor(1.0,1.0,1.0,0.3))
											]
										]
										+SOverlay::Slot()
										[
											SAssignNew(QuickFilterText, SEditableTextBox)
											.Style(SUTStyle::Get(),"UT.EditBox")
											.OnTextCommitted(this, &SUTServerBrowserPanel::OnQuickFilterTextCommited)
											.OnTextChanged(this, &SUTServerBrowserPanel::OnFilterTextChanged)
											.ClearKeyboardFocusOnCommit(false)
											.MinDesiredWidth(300.0f)
											.Text(FText::GetEmpty())
										]
									]
								]
								+SHorizontalBox::Slot()
								.Padding(16.0f, 0.0f, 0.0f, 0.0f)
								.AutoWidth()
								[
									SAssignNew(HideUnresponsiveServersCheckbox, SCheckBox)
									.Style(SUTStyle::Get(), "UT.CheckBox")
									.ForegroundColor(FLinearColor::White)
									.IsChecked(this, &SUTServerBrowserPanel::ShouldHideUnresponsiveServers)
									.OnCheckStateChanged(this, &SUTServerBrowserPanel::OnHideUnresponsiveServersChanged)
								]
								+SHorizontalBox::Slot()
								.VAlign(VAlign_Center)
								.Padding(16.0f, 0.0f, 30.0f, 0.0f)
								.AutoWidth()
								[
									SNew(STextBlock)
									.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Small")
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
							SAssignNew(RefreshButton, SUTButton)
							.ButtonStyle(SUTStyle::Get(), "UT.Button.MenuBar")
							.ContentPadding(FMargin(10.0f, 5.0f, 15.0f, 5.0))

							.Text( NSLOCTEXT("SUTServerBrowserPanel","Refresh","Refresh"))
							.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Medium")
							.OnClicked(this, &SUTServerBrowserPanel::OnRefreshClick)
						]
					]
					+SHorizontalBox::Slot().HAlign(HAlign_Fill)
					[
						SNew(SBorder)
						.BorderImage(SUTStyle::Get().GetBrush("UT.HeaderBackground.Dark"))
						[
							SNew(SHorizontalBox)
							+SHorizontalBox::Slot()
							.VAlign(VAlign_Center)
							.Padding(20.0f, 0.0f, 20.0f, 0.0f)
							[
								SAssignNew(StatusText, STextBlock)
								.Text(this, &SUTServerBrowserPanel::GetStatusText)
								.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Small.Bold")
							]
						]
					]

					+SHorizontalBox::Slot().AutoWidth()
					.VAlign(VAlign_Center)
					[
						SAssignNew(ConnectBox,SHorizontalBox)
					]
				]
			]
		]

	];

	BuildConnectBox();

	bDescendingSort = false;
	CurrentSortColumn = FName(TEXT("ServerPing"));

	AllInternetServers.Empty();
	AllHubServers.Empty();

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
		PlayerOnlineStatusChangedDelegate = PlayerOwner->RegisterPlayerOnlineStatusChangedDelegate(FPlayerOnlineStatusChanged::FDelegate::CreateSP(this, &SUTServerBrowserPanel::OwnerLoginStatusChanged));
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

	BrowserTypeChanged(0);
	AddGameFilters();
}

TSharedRef<SWidget> SUTServerBrowserPanel::BuildPlayerList()
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
			.OnGenerateRow( this, &SUTServerBrowserPanel::OnGenerateWidgetForPlayersList )
			.SelectionMode(ESelectionMode::Single)
			.ListItemsSource( &PlayersListSource)

			.HeaderRow
			(
				SAssignNew(PlayersHeaderRow, SHeaderRow) 
				.Style(SUTStyle::Get(), "UT.List.Header")

				+ SHeaderRow::Column("Name")
						.OnSort(this, &SUTServerBrowserPanel::OnPlayerSort)
						.HeaderContent()
						[
							SNew(STextBlock)
								.Text(NSLOCTEXT("SUTServerBrowserPanel","PlayerNameColumn", "Name"))
								.ToolTipText( NSLOCTEXT("SUTServerBrowserPanel","PlayerNameColumnToolTip", "This player's name.") )
								.TextStyle(SUTStyle::Get(), "UT.Font.ServerBrowser.List.Header")
						]

				+ SHeaderRow::Column("Score") 
						.DefaultLabel(NSLOCTEXT("SUTServerBrowserPanel","PlayerScoreColumn", "Score")) 
						.HAlignCell(HAlign_Center) 
						.HAlignHeader(HAlign_Center)
						.OnSort(this, &SUTServerBrowserPanel::OnPlayerSort)
						.HeaderContent()
						[
							SNew(STextBlock)
								.Text(NSLOCTEXT("SUTServerBrowserPanel","PlayerScoreColumn", "IP"))
								.ToolTipText( NSLOCTEXT("SUTServerBrowserPanel","PlayerScoreColumnToolTip", "This player's score.") )
								.TextStyle(SUTStyle::Get(), "UT.Font.ServerBrowser.List.Header")
						]
			)
		]
	];

}

TSharedRef<SWidget> SUTServerBrowserPanel::BuildServerBrowser()
{
	return 	SAssignNew(InternetServerBrowser, SBox) 
		.HeightOverride(500.0f)
		[
			SAssignNew( VertSplitter, SSplitter )
			.Orientation(Orient_Horizontal)
			.OnSplitterFinishedResizing(this, &SUTServerBrowserPanel::VertSplitterResized)

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
						.OnGenerateRow(this, &SUTServerBrowserPanel::OnGenerateWidgetForList)
						.OnSelectionChanged(this, &SUTServerBrowserPanel::OnServerListSelectionChanged)
						.OnMouseButtonDoubleClick(this, &SUTServerBrowserPanel::OnListMouseButtonDoubleClick)
						.SelectionMode(ESelectionMode::Single)
						.HeaderRow
						(
							SAssignNew(HeaderRow, SHeaderRow)
							.Style(SUTStyle::Get(), "UT.List.Header")

							+ SHeaderRow::Column("ServerName")
								.OnSort(this, &SUTServerBrowserPanel::OnSort)
								.HeaderContent()
								[
									SNew(STextBlock)
									.Text(NSLOCTEXT("SUTServerBrowserPanel", "ServerNameColumn", "Server Name"))
									.ToolTipText(NSLOCTEXT("SUTServerBrowserPanel", "ServerNameColumnToolTip", "The name of this server."))
									.TextStyle(SUTStyle::Get(), "UT.Font.ServerBrowser.List.Header")
								]

							+ SHeaderRow::Column("ServerGame")
								.DefaultLabel(NSLOCTEXT("SUTServerBrowserPanel", "ServerGameColumn", "Game"))
								.HAlignCell(HAlign_Center)
								.OnSort(this, &SUTServerBrowserPanel::OnSort)
								.HeaderContentPadding(FMargin(5.0))
								.HeaderContent()
								[
									SNew(SBox)
									.HAlign(HAlign_Center)
									[
										SNew(STextBlock)
										.Text(NSLOCTEXT("SUTServerBrowserPanel", "ServerGameColumn", "Game"))
										.ToolTipText(NSLOCTEXT("SUTServerBrowserPanel", "ServerGameColumnToolTip", "The Game type."))
										.TextStyle(SUTStyle::Get(), "UT.Font.ServerBrowser.List.Header")
									]
								]

							+ SHeaderRow::Column("ServerMap")
								.DefaultLabel(NSLOCTEXT("SUTServerBrowserPanel", "ServerMapColumn", "Map"))
								.HAlignCell(HAlign_Center)
								.OnSort(this, &SUTServerBrowserPanel::OnSort)
								.HeaderContent()
								[
									SNew(SBox)
									.HAlign(HAlign_Center)
									[
										SNew(STextBlock)
										.Text(NSLOCTEXT("SUTServerBrowserPanel", "ServerMapColumn", "Map"))
										.ToolTipText(NSLOCTEXT("SUTServerBrowserPanel", "ServerMapColumnToolTip", "The current map."))
										.TextStyle(SUTStyle::Get(), "UT.Font.ServerBrowser.List.Header")
									]
								]

							+ SHeaderRow::Column("ServerNumPlayers")
								.DefaultLabel(NSLOCTEXT("SUTServerBrowserPanel", "ServerNumPlayerColumn", "Players"))
								.HAlignCell(HAlign_Center)
								.OnSort(this, &SUTServerBrowserPanel::OnSort)
								.HeaderContent()
								[
									SNew(SBox)
									.HAlign(HAlign_Center)
									[
										SNew(STextBlock)
										.Text(NSLOCTEXT("SUTServerBrowserPanel", "ServerNumPlayerColumn", "Players"))
										.ToolTipText(NSLOCTEXT("SUTServerBrowserPanel", "ServerNumPlayerColumnToolTip", "The # of Players on this server."))
										.TextStyle(SUTStyle::Get(), "UT.Font.ServerBrowser.List.Header")
									]
								]

							+ SHeaderRow::Column("ServerNumSpecs")
								.DefaultLabel(NSLOCTEXT("SUTServerBrowserPanel", "ServerNumSpecsColumn", "Spectators"))
								.HAlignCell(HAlign_Center)
								.OnSort(this, &SUTServerBrowserPanel::OnSort)
								.HeaderContent()
								[
									SNew(SBox)
									.HAlign(HAlign_Center)
									[
										SNew(STextBlock)
										.Text(NSLOCTEXT("SUTServerBrowserPanel", "ServerNumSpecsColumn", "Spectators"))
										.ToolTipText(NSLOCTEXT("SUTServerBrowserPanel", "ServerNumSpecsColumnToolTip", "The # of spectators on this server."))
										.TextStyle(SUTStyle::Get(), "UT.Font.ServerBrowser.List.Header")
									]
								]

							+ SHeaderRow::Column("ServerNumFriends")
								.DefaultLabel(NSLOCTEXT("SUTServerBrowserPanel", "ServerNumFriendsColumn", "Friends"))
								.HAlignCell(HAlign_Center)
								.OnSort(this, &SUTServerBrowserPanel::OnSort)
								.HeaderContent()
								[
									SNew(SBox)
									.HAlign(HAlign_Center)
									[
										SNew(STextBlock)
										.Text(NSLOCTEXT("SUTServerBrowserPanel", "ServerNumFriendsColumn", "Friends"))
										.ToolTipText(NSLOCTEXT("SUTServerBrowserPanel", "ServerNumFriendsColumnToolTip", "The # of friends on this server."))
										.TextStyle(SUTStyle::Get(), "UT.Font.ServerBrowser.List.Header")
									]
								]

							+ SHeaderRow::Column("ServerFlags")
								.DefaultLabel(NSLOCTEXT("SUTServerBrowserPanel", "ServerFlagsColumn", "Options"))
								.HAlignCell(HAlign_Center)
								.OnSort(this, &SUTServerBrowserPanel::OnSort)
								.HeaderContent()
								[
									SNew(SBox)
									.HAlign(HAlign_Center)
									[
										SNew(STextBlock)
										.Text(NSLOCTEXT("SUTServerBrowserPanel", "ServerFlagsColumn", "Options"))
										.ToolTipText(NSLOCTEXT("SUTServerBrowserPanel", "ServerFlagsColumnToolTip", "Server Options"))
										.TextStyle(SUTStyle::Get(), "UT.Font.ServerBrowser.List.Header")
									]
								]

							+ SHeaderRow::Column("ServerPing")
								.DefaultLabel(NSLOCTEXT("SUTServerBrowserPanel", "ServerPingColumn", "Ping"))
								.HAlignCell(HAlign_Center)
								.OnSort(this, &SUTServerBrowserPanel::OnSort)
								.HeaderContent()
								[
									SNew(SBox)
									.HAlign(HAlign_Center)
									[
										SNew(STextBlock)
										.Text(NSLOCTEXT("SUTServerBrowserPanel", "ServerPingColumn", "Ping"))
										.ToolTipText(NSLOCTEXT("SUTServerBrowserPanel", "ServerPingColumnToolTip", "Your connection speed to the server."))
										.TextStyle(SUTStyle::Get(), "UT.Font.ServerBrowser.List.Header")
									]
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
				.OnSplitterFinishedResizing(this, &SUTServerBrowserPanel::HorzSplitterResized)

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
							.OnGenerateRow( this, &SUTServerBrowserPanel::OnGenerateWidgetForRulesList )
							.SelectionMode(ESelectionMode::Single)
							.ListItemsSource( &RulesListSource)

							.HeaderRow
							(
								SAssignNew(RulesHeaderRow, SHeaderRow) 
								.Style(SUTStyle::Get(),"UT.List.Header")

								+ SHeaderRow::Column("Rule")
									.OnSort(this, &SUTServerBrowserPanel::OnRuleSort)
									.HeaderContent()
									[
										SNew(STextBlock)
										.Text(NSLOCTEXT("SUTServerBrowserPanel","RuleRuleColumn", "Rule"))
										.ToolTipText( NSLOCTEXT("SUTServerBrowserPanel","RuleRuleColumnToolTip", "The name of the rule.") )
										.TextStyle(SUTStyle::Get(), "UT.Font.ServerBrowser.List.Header")
									]

								+ SHeaderRow::Column("Value") 
									.DefaultLabel(NSLOCTEXT("SUTServerBrowserPanel","RuleValueColumn", "Value")) 
									.HAlignCell(HAlign_Center) 
									.HAlignHeader(HAlign_Center)
									.OnSort(this, &SUTServerBrowserPanel::OnRuleSort)
									.HeaderContent()
									[
										SNew(STextBlock)
										.Text(NSLOCTEXT("SUTServerBrowserPanel","RuleValueColumn", "Value"))
										.ToolTipText( NSLOCTEXT("SUTServerBrowserPanel","RuleValueColumnToolTip", "The Value") )
										.TextStyle(SUTStyle::Get(), "UT.Font.ServerBrowser.List.Header")
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
		];
}

TSharedRef<SWidget> SUTServerBrowserPanel::BuildLobbyBrowser()
{
	return SAssignNew(LobbyBrowser, SVerticalBox)
		+SVerticalBox::Slot()
		.Padding(64.0, 15.0, 64.0, 15.0)
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SBox).WidthOverride(1792).HeightOverride(860)
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.Padding(0.0f,0.0f,6.0f,0.0f)
					.AutoWidth()
					[
						SNew(SBox).WidthOverride(826)
						[
							SNew(SBorder)
							.BorderImage(SUTStyle::Get().GetBrush("UT.HeaderBackground.Dark"))
							[
								SNew(SVerticalBox)
								+SVerticalBox::Slot()
								.AutoHeight()
								.HAlign(HAlign_Fill)
								[
									SNew(SBox).HeightOverride(860)
									[
										// The list view being tested
										SAssignNew(HUBServerList, SListView< TSharedPtr<FServerData> >)
										// List view items are this tall
										.ItemHeight(64)
										// When the list view needs to generate a widget for some data item, use this method
										.OnGenerateRow(this, &SUTServerBrowserPanel::OnGenerateWidgetForHUBList)
										.SelectionMode(ESelectionMode::Single)
										.ListItemsSource(&FilteredHubsSource)
										.OnMouseButtonDoubleClick(this, &SUTServerBrowserPanel::OnListMouseButtonDoubleClick)
										.OnSelectionChanged(this, &SUTServerBrowserPanel::OnHUBListSelectionChanged)								
									]
								]
							]
						]
					]
					+SHorizontalBox::Slot()
					.Padding(6.0f,0.0f,0.0f,0.0f)
					.AutoWidth()
					[

						SNew(SVerticalBox)
						+SVerticalBox::Slot()
						.HAlign(HAlign_Fill)
						.AutoHeight()
						[
							SNew(SBox).HeightOverride(200)
							[
								SNew(SBorder)
								.BorderImage(SUTStyle::Get().GetBrush("UT.HeaderBackground.SuperDark"))
								[
									SNew(SScrollBox)
									+ SScrollBox::Slot()
									.Padding(FMargin(0.0f, 5.0f, 0.0f, 5.0f))
									[
										SAssignNew(LobbyInfoText, SRichTextBlock)
										.Text(NSLOCTEXT("HUBBrowser", "NoneSelected", "No Server Selected!"))
										.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Small")
										.Justification(ETextJustify::Center)
										.DecoratorStyleSet(&SUTStyle::Get())
										.AutoWrapText(true)
									]
								]
							]
						]
						+SVerticalBox::Slot()
						.HAlign(HAlign_Fill)
						.FillHeight(1.0)
						.Padding(0.0,12.0,0.0,0.0)
						[
							SNew(SBox).HeightOverride(648)
							[
								SAssignNew(LobbyMatchPanel, SUTMatchPanel).PlayerOwner(PlayerOwner)
								.bExpectLiveData(false)
								.OnJoinMatchDelegate(this, &SUTServerBrowserPanel::JoinQuickInstance)
							]
						]
					]
				]
			]
		];
}

ECheckBoxState SUTServerBrowserPanel::ShouldHideUnresponsiveServers() const
{
	return bHideUnresponsiveServers ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SUTServerBrowserPanel::OnHideUnresponsiveServersChanged(const ECheckBoxState NewState)
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

void SUTServerBrowserPanel::OwnerLoginStatusChanged(UUTLocalPlayer* LocalPlayerOwner, ELoginStatus::Type NewStatus, const FUniqueNetId& UniqueID)
{
	if (NewStatus == ELoginStatus::LoggedIn)
	{
		SetBrowserState(EBrowserState::BrowserIdle);
		RefreshButton->SetContent( SNew(STextBlock).Text(NSLOCTEXT("SUTServerBrowserPanel","Refresh","Refresh")).TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Medium"));
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


void SUTServerBrowserPanel::AddGameFilters()
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
					.ButtonStyle(SUTStyle::Get(), "UT.ContextMenu.Item")
					.ContentPadding(FMargin(10.0f, 5.0f))
					.Text(FText::FromString(GameTypes[i]))
					.TextStyle(SUTStyle::Get(), "UT.Font.ContextMenuItem")
					.OnClicked(this, &SUTServerBrowserPanel::OnGameFilterSelection, GameTypes[i])
				];
			}
			GameFilter->SetMenuContent(Menu.ToSharedRef());
		}
	}

	GameFilterText = NSLOCTEXT("SUTServerBrowserPanel", "GameFilterAll", "All");
}

FText SUTServerBrowserPanel::GetGameFilterText() const
{
	return GameFilterText;
}

FReply SUTServerBrowserPanel::OnGameFilterSelection(FString Filter)
{
	GameFilterText = FText::FromString(Filter);
	GameFilter->SetIsOpen(false);
	FilterAllServers();
	return FReply::Handled();
}

void SUTServerBrowserPanel::OnSort(EColumnSortPriority::Type Priority, const FName& ColumnName, EColumnSortMode::Type SortType)
{
	if (ColumnName == CurrentSortColumn)
	{
		bDescendingSort = !bDescendingSort;
	}

	SortServers(ColumnName);
}

void SUTServerBrowserPanel::OnRuleSort(EColumnSortPriority::Type Priority, const FName& ColumnName, EColumnSortMode::Type SortType)
{
	if (ColumnName == CurrentRulesSortColumn)
	{
		bDescendingRulesSort = !bDescendingRulesSort;
	}

	CurrentRulesSortColumn = ColumnName;


	if (ColumnName == FName("Rule")) bDescendingRulesSort ? RulesListSource.StableSort(FCompareRulesByRuleDesc()) : RulesListSource.StableSort(FCompareRulesByRule());
	else if (ColumnName == FName("Value")) bDescendingRulesSort ? RulesListSource.StableSort(FCompareRulesByValueDesc()) : RulesListSource.StableSort(FCompareRulesByValue());

	RulesList->RequestListRefresh();

}

void SUTServerBrowserPanel::OnPlayerSort(EColumnSortPriority::Type Priority, const FName& ColumnName, EColumnSortMode::Type SortType)
{
	if (ColumnName == CurrentRulesSortColumn)
	{
		bDescendingPlayersSort = !bDescendingPlayersSort;
	}

	CurrentRulesSortColumn = ColumnName;

	if (ColumnName == FName("Name")) bDescendingPlayersSort ? PlayersListSource.StableSort(FComparePlayersByNameDesc()) : PlayersListSource.StableSort(FComparePlayersByName());
	else if (ColumnName == FName("Score")) bDescendingPlayersSort ? PlayersListSource.StableSort(FComparePlayersByScoreDesc()) : PlayersListSource.StableSort(FComparePlayersByScore());

	PlayersList->RequestListRefresh();

}



void SUTServerBrowserPanel::SortServers(FName ColumnName)
{
	if (ColumnName == FName(TEXT("ServerName"))) bDescendingSort ? FilteredServersSource.StableSort(FCompareServerByNameDesc()) : FilteredServersSource.StableSort(FCompareServerByName());
	else if (ColumnName == FName(TEXT("ServerIP"))) bDescendingSort ? FilteredServersSource.StableSort(FCompareServerByIPDesc()) : FilteredServersSource.StableSort(FCompareServerByIP());
	else if (ColumnName == FName(TEXT("ServerGame"))) bDescendingSort ? FilteredServersSource.StableSort(FCompareServerByGameModeDesc()) : FilteredServersSource.StableSort(FCompareServerByGameMode());
	else if (ColumnName == FName(TEXT("ServerMap"))) bDescendingSort ? FilteredServersSource.StableSort(FCompareServerByMapDesc()) : FilteredServersSource.StableSort(FCompareServerByMap());
	else if (ColumnName == FName(TEXT("ServerVer"))) bDescendingSort ? FilteredServersSource.StableSort(FCompareServerByVersionDesc()) : FilteredServersSource.StableSort(FCompareServerByVersion());
	else if (ColumnName == FName(TEXT("ServerNumPlayers"))) bDescendingSort ? FilteredServersSource.StableSort(FCompareServerByNumPlayersDesc()) : FilteredServersSource.StableSort(FCompareServerByNumPlayers());
	else if (ColumnName == FName(TEXT("ServerNumSpecs"))) bDescendingSort ? FilteredServersSource.StableSort(FCompareServerByNumSpectatorsDesc()) : FilteredServersSource.StableSort(FCompareServerByNumSpectators());
	else if (ColumnName == FName(TEXT("ServerNumFriends"))) bDescendingSort ? FilteredServersSource.StableSort(FCompareServerByNumFriendsDesc()) : FilteredServersSource.StableSort(FCompareServerByNumFriends());
	else if (ColumnName == FName(TEXT("ServerPing"))) bDescendingSort ? FilteredServersSource.StableSort(FCompareServerByPingDesc()) : FilteredServersSource.StableSort(FCompareServerByPing());

	InternetServerList->RequestListRefresh();
	CurrentSortColumn = ColumnName;
}

void SUTServerBrowserPanel::SortHUBs()
{
	FilteredHubsSource.StableSort(FCompareHub());
	HUBServerList->RequestListRefresh();
}

TSharedRef<ITableRow> SUTServerBrowserPanel::OnGenerateWidgetForList( TSharedPtr<FServerData> InItem, const TSharedRef<STableViewBase>& OwnerTable )
{
	return SNew( SServerBrowserRow, OwnerTable).ServerData( InItem ).Style(SUTStyle::Get(),"UT.List.Row");
}

TSharedRef<ITableRow> SUTServerBrowserPanel::OnGenerateWidgetForRulesList( TSharedPtr<FServerRuleData> InItem, const TSharedRef<STableViewBase>& OwnerTable )
{
	return SNew( SServerRuleRow, OwnerTable).ServerRuleData( InItem ).Style(SUTStyle::Get(),"UT.List.Row");
}

TSharedRef<ITableRow> SUTServerBrowserPanel::OnGenerateWidgetForPlayersList( TSharedPtr<FServerPlayerData> InItem, const TSharedRef<STableViewBase>& OwnerTable )
{
	return SNew( SServerPlayerRow, OwnerTable).ServerPlayerData( InItem ).Style(SUTStyle::Get(),"UT.List.Row");
}

FText SUTServerBrowserPanel::GetStatusText() const
{
	if (BrowserState == EBrowserState::NotLoggedIn)
	{
		return NSLOCTEXT("SUTServerBrowserPanel","NotLoggedIn","Login Required!");
	}
	else if (BrowserState == EBrowserState::AuthInProgress) 
	{
		return NSLOCTEXT("SUTServerBrowserPanel","Auth","Authenticating...");
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

		if (BrowserState == EBrowserState::RefreshInProgress)
		{
			return FText::Format( NSLOCTEXT("SUTServerBrowserPanel","RefreshingStatusMsg","Getting new Server list from MCP -- Showing {FilteredHubCount} of {HubCount} Hubs, {FilteredServerCount} of {ServerCount} Servers"), Args);
		}
		else
		{
			if (PingCount > 0)
			{
				return FText::Format( NSLOCTEXT("SUTServerBrowserPanel","PingingStatusMsg","Pinging {PingCount} Servers -- Showing {FilteredHubCount} of {HubCount} Hubs, {FilteredServerCount} of {ServerCount} Servers"), Args);
			}
			else
			{
				return FText::Format( NSLOCTEXT("SUTServerBrowserPanel","IdleStatusMsg","Showing {FilteredHubCount} of {HubCount} Hubs, {FilteredServerCount} of {ServerCount} Servers -- {PlayerCount} of {TotalPlayers} Players"), Args);
			}
		}
	}
}

void SUTServerBrowserPanel::SetBrowserState(FName NewBrowserState)
{
	BrowserState = NewBrowserState;
	if (BrowserState == EBrowserState::NotLoggedIn) 
	{
		RefreshButton->SetContent( SNew(STextBlock).Text(NSLOCTEXT("SUTServerBrowserPanel","Login","Login")).TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Medium"));
		RefreshButton->SetVisibility(EVisibility::All);
	}
	else if (BrowserState == EBrowserState::BrowserIdle) 
	{
		RefreshButton->SetVisibility(EVisibility::All);
	}
	else if (BrowserState == EBrowserState::AuthInProgress) 
	{
		RefreshButton->SetVisibility(EVisibility::Collapsed);
	}
	else if (BrowserState == EBrowserState::RefreshInProgress) 
	{
		RefreshButton->SetVisibility(EVisibility::Collapsed);
	}
	
	BuildConnectBox();
}

FReply SUTServerBrowserPanel::OnRefreshClick()
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

void SUTServerBrowserPanel::RefreshServers()
{
	bWantsAFullRefilter = true;
	if (PlayerOwner->IsLoggedIn() && OnlineSessionInterface.IsValid() && BrowserState == EBrowserState::BrowserIdle)
	{
		SetBrowserState(EBrowserState::RefreshInProgress);

		bNeedsRefresh = false;
		CleanupQoS();

		// Search for Internet Servers

		SearchSettings = MakeShareable(new FUTOnlineGameSearchBase(false));
		SearchSettings->MaxSearchResults = 10000;
		SearchSettings->bIsLanQuery = false;
		SearchSettings->QuerySettings.Set(SETTING_GAMEINSTANCE, 1, EOnlineComparisonOp::NotEquals);												// Must not be a Hub server instance

		TSharedRef<FUTOnlineGameSearchBase> SearchSettingsRef = SearchSettings.ToSharedRef();

		FOnFindSessionsCompleteDelegate Delegate;
		Delegate.BindSP(this, &SUTServerBrowserPanel::OnFindSessionsComplete);
		OnFindSessionCompleteDelegate = OnlineSessionInterface->AddOnFindSessionsCompleteDelegate_Handle(Delegate);

		OnlineSessionInterface->FindSessions(0, SearchSettingsRef);
	}
	else
	{
		SetBrowserState(EBrowserState::NotLoggedIn);	
	}
}

void SUTServerBrowserPanel::FoundServer(FOnlineSessionSearchResult& Result)
{
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

void SUTServerBrowserPanel::OnFindSessionsComplete(bool bWasSuccessful)
{
	OnlineSessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(OnFindSessionCompleteDelegate);

	if (bWasSuccessful)
	{
		TotalPlayersPlaying = 0;
		if (SearchSettings->SearchResults.Num() > 0)
		{
			for (int32 ServerIndex = 0; ServerIndex < SearchSettings->SearchResults.Num(); ServerIndex++)
			{
				FoundServer(SearchSettings->SearchResults[ServerIndex]);
			}
		}

		// If a server exists in either of the lists but not in the PingList, then let's kill it.
		ExpireDeadServers(false);

		if ( FParse::Param(FCommandLine::Get(), TEXT("DumpServers")) )
		{
			UE_LOG(UT,Log,TEXT("Received a list of %i Internet Servers....."), PingList.Num());
			for (int32 i=0;i<PingList.Num();i++)
			{
				UE_LOG(UT,Log,TEXT("Received Server %i - %s %s  : Players %i/%i"), i, *PingList[i]->Name, *PingList[i]->IP, PingList[i]->NumPlayers, PingList[i]->MaxPlayers);
			}
			UE_LOG(UT,Log, TEXT("----------------------------------------------"));
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

	// Search for LAN Servers

	LanSearchSettings = MakeShareable(new FUTOnlineGameSearchBase(false));
	LanSearchSettings->MaxSearchResults = 10000;
	LanSearchSettings->bIsLanQuery = true;
	LanSearchSettings->TimeoutInSeconds = 2.0;
	LanSearchSettings->QuerySettings.Set(SETTING_GAMEINSTANCE, 1, EOnlineComparisonOp::NotEquals);												// Must not be a Hub server instance

	TSharedRef<FUTOnlineGameSearchBase> LanSearchSettingsRef = LanSearchSettings.ToSharedRef();
	FOnFindSessionsCompleteDelegate Delegate;
	Delegate.BindSP(this, &SUTServerBrowserPanel::OnFindLANSessionsComplete);
	OnFindLANSessionCompleteDelegate = OnlineSessionInterface->AddOnFindSessionsCompleteDelegate_Handle(Delegate);
	OnlineSessionInterface->FindSessions(0, LanSearchSettingsRef);
}


void SUTServerBrowserPanel::OnFindLANSessionsComplete(bool bWasSuccessful)
{
	OnlineSessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(OnFindLANSessionCompleteDelegate);

	if (bWasSuccessful)
	{
		TotalPlayersPlaying = 0;
		if (LanSearchSettings->SearchResults.Num() > 0)
		{
			for (int32 ServerIndex = 0; ServerIndex < LanSearchSettings->SearchResults.Num(); ServerIndex++)
			{
				FoundServer(LanSearchSettings->SearchResults[ServerIndex]);
			}
		}

		ExpireDeadServers(true);

		if ( FParse::Param(FCommandLine::Get(), TEXT("DumpServers")) )
		{
			UE_LOG(UT,Log,TEXT("Received a list of %i Servers....."), PingList.Num());
			for (int32 i=0;i<PingList.Num();i++)
			{
				UE_LOG(UT,Log,TEXT("Received Server %i - %s %s  : Players %i/%i"), i, *PingList[i]->Name, *PingList[i]->IP, PingList[i]->NumPlayers, PingList[i]->MaxPlayers);
			}
			UE_LOG(UT,Log, TEXT("----------------------------------------------"));
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


void SUTServerBrowserPanel::ExpireDeadServers(bool bLANServers)
{
	int32 i = 0;
	while (i < AllInternetServers.Num())
	{
		bool bFound = false;

		// Make sure the server is of the proper type.....
		
		if (bLANServers == AllInternetServers[i]->SearchResult.Session.SessionSettings.bIsLANMatch)
		{
			for (int32 j=0; j < PingList.Num(); j++)
			{
				if (AllInternetServers[i]->GetId() == PingList[j]->GetId())	
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
		else
		{
			i++;
		}


	
	} 

	i = 0;
	while (i < AllHubServers.Num())
	{
		if (!AllHubServers[i]->bFakeHUB && bLANServers == AllHubServers[i]->SearchResult.Session.SessionSettings.bIsLANMatch)
		{
			bool bFound = false;
			for (int32 j=0; j < PingList.Num(); j++)
			{
				if (AllHubServers[i]->GetId() == PingList[j]->GetId())	
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


void SUTServerBrowserPanel::CleanupQoS()
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

void SUTServerBrowserPanel::PingNextServer()
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

void SUTServerBrowserPanel::PingServer(TSharedPtr<FServerData> ServerToPing)
{
	// Build the beacon
	AUTServerBeaconClient* Beacon = PlayerOwner->GetWorld()->SpawnActor<AUTServerBeaconClient>(AUTServerBeaconClient::StaticClass());
	if (Beacon && !ServerToPing->BeaconIP.IsEmpty())
	{
		Beacon->OnServerRequestResults = FServerRequestResultsDelegate::CreateSP(this, &SUTServerBrowserPanel::OnServerBeaconResult );
		Beacon->OnServerRequestFailure = FServerRequestFailureDelegate::CreateSP(this, &SUTServerBrowserPanel::OnServerBeaconFailure);
		FURL BeaconURL(nullptr, *ServerToPing->BeaconIP, TRAVEL_Absolute);
		Beacon->InitClient(BeaconURL);
		PingTrackers.Add(FServerPingTracker(ServerToPing, Beacon));
	}
}

void SUTServerBrowserPanel::AddServer(TSharedPtr<FServerData> Server)
{
	Server->UpdateFriends(PlayerOwner);
	for (int32 i=0; i < AllInternetServers.Num() ; i++)
	{
		if (AllInternetServers[i]->GetId() == Server->GetId())
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

void SUTServerBrowserPanel::AddHub(TSharedPtr<FServerData> Hub)
{
	AUTPlayerState* PlayerState = Cast<AUTPlayerState>(GetPlayerOwner()->PlayerController->PlayerState);

	Hub->UpdateFriends(PlayerOwner);

	int32 ServerTrustLevel; 
	Hub->SearchResult.Session.SessionSettings.Get(SETTING_TRUSTLEVEL, ServerTrustLevel);

	if (HUBServerList->GetNumItemsSelected() > 0)
	{
		TArray<TSharedPtr<FServerData>> Hubs = HUBServerList->GetSelectedItems();
		if (Hubs.Num() >= 1 && Hubs[0].IsValid() && Hubs[0]->SearchResult.IsValid() && Hubs[0]->GetId() == Hub->GetId())
		{
			AddHUBInfo(Hub);
		}
	}

	for (int32 i=0; i < AllHubServers.Num() ; i++)
	{
		if (AllHubServers[i].IsValid() && AllHubServers[i]->SearchResult.IsValid() && !AllHubServers[i]->bFakeHUB)
		{
			if (AllHubServers[i]->GetId() == Hub->GetId())
			{
				if (AllHubServers[i] != Hub)
				{
					AllHubServers[i]->Update(Hub);
				}
				return; 
			}
		}
	}

	UE_LOG(UT,Log,TEXT("ADDing HUB"));
	AllHubServers.Add(Hub);
	FilterHUB(Hub);
}

void SUTServerBrowserPanel::OnServerBeaconFailure(AUTServerBeaconClient* Sender)
{
	for (int32 i=0; i < PingTrackers.Num(); i++)
	{
		if (PingTrackers[i].Beacon == Sender)
		{
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
			PingNextServer();
		}
	}
}

void SUTServerBrowserPanel::OnServerBeaconResult(AUTServerBeaconClient* Sender, FServerBeaconInfo ServerInfo)
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

			if (PingTrackers[i].Server->GameModePath == LOBBY_GAME_PATH)
			{
				PingTrackers[i].Server->NumPlayers = Cnt / 3;
				PingTrackers[i].Server->UpdateFriends(PlayerOwner);
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
			for (int32 InstIndex=0; InstIndex < PingTrackers[i].Beacon->Instances.Num(); InstIndex++ )
			{
				PingTrackers[i].Server->HUBInstances.Add(PingTrackers[i].Beacon->Instances[InstIndex]);	
			}

			if (PingTrackers[i].Server->GameModePath == LOBBY_GAME_PATH)
			{
				PingTrackers[i].Server->NumMatches = PingTrackers[i].Beacon->Instances.Num();
			}

			if (PingTrackers[i].Server->GameModePath == LOBBY_GAME_PATH)
			{

				PingTrackers[i].Server->Ping = int32(float(PingTrackers[i].Beacon->Ping) - 0.5 * float(PingTrackers[i].Beacon->ServerTickRate) + (1.0/120.0));
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

void SUTServerBrowserPanel::OnListMouseButtonDoubleClick(TSharedPtr<FServerData> SelectedServer)
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


FReply SUTServerBrowserPanel::OnJoinClick(bool bSpectate)
{
	TArray<TSharedPtr<FServerData>> SelectedItems = (bShowingHubs ? HUBServerList->GetSelectedItems() : InternetServerList->GetSelectedItems());
	if (SelectedItems.Num() > 0)
	{
		ConnectTo(*SelectedItems[0],bSpectate);
	}
	return FReply::Handled();
}

void SUTServerBrowserPanel::RestrictedWarning()
{
	PlayerOwner->MessageBox(NSLOCTEXT("SUTServerBrowserPanel","RestrictedServerTitle","Unable to join server"), NSLOCTEXT("SUTServerBrowserPanel","RestrictedServerMsg","Sorry, but your skill level is too high to join the hub or server you have selected.  Please choose another one."));
}

void SUTServerBrowserPanel::ConnectTo(FServerData ServerData,bool bSpectate)
{
	if ((ServerData.Flags & SERVERFLAG_Restricted) > 0)
	{
		RestrictedWarning();
		return;
	}

	ConnectToServerName = FText::Format(NSLOCTEXT("SUTServerBrowserPanel","ConnectToFormat","Connecting to {0}... "), ServerData.GetBrowserName());
	SetBrowserState(EBrowserState::ConnectInProgress);	

	// Flag the browser as needing a refresh the next time it is shown
	bNeedsRefresh = true;
	PlayerOwner->JoinSession(ServerData.SearchResult, bSpectate);
	CleanupQoS();
}

void SUTServerBrowserPanel::FilterAllServers()
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

void SUTServerBrowserPanel::FilterServer(TSharedPtr< FServerData > NewServer, bool bSortAndUpdate, int32 BestPing)
{
	FString GameFilterString = GameFilterText.ToString();
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

void SUTServerBrowserPanel::FilterAllHUBs()
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


void SUTServerBrowserPanel::FilterHUB(TSharedPtr< FServerData > NewServer, bool bSortAndUpdate, int32 BestPing)
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

bool SUTServerBrowserPanel::IsUnresponsive(TSharedPtr<FServerData> Server, int32 BestPing)
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

void SUTServerBrowserPanel::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
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

	
/*
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
*/
}

void SUTServerBrowserPanel::TallyInternetServers(int32& Players, int32& Spectators, int32& Friends)
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


void SUTServerBrowserPanel::VertSplitterResized()
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

void SUTServerBrowserPanel::HorzSplitterResized()
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

void SUTServerBrowserPanel::OnServerListSelectionChanged(TSharedPtr<FServerData> SelectedItem, ESelectInfo::Type SelectInfo)
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
	}
}

void SUTServerBrowserPanel::OnQuickFilterTextCommited(const FText& NewText, ETextCommit::Type CommitType)
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
	FilterMsg->SetVisibility( NewText.IsEmpty() ? EVisibility::Visible : EVisibility::Hidden);
}

void SUTServerBrowserPanel::OnFilterTextChanged(const FText& NewText)
{
	const FString& QuickFilterTextAsString = QuickFilterText->GetText().ToString();
	
	if (QuickFilterTextAsString.Len() > MAXCHARCOUNTFORSERVERFILTER)
	{
		QuickFilterText->SetText(FText::FromString(QuickFilterTextAsString.Left(MAXCHARCOUNTFORSERVERFILTER)));
	}
	else
	{
		FilterMsg->SetVisibility(NewText.IsEmpty() ? EVisibility::Visible : EVisibility::Hidden);
	}
}


FReply SUTServerBrowserPanel::BrowserTypeChanged(int32 TabIndex)
{
	if (TabIndex == 0 && !bShowingHubs) ShowHUBs();
	if (TabIndex == 1 && bShowingHubs) ShowServers(TEXT("ALL"));
	return FReply::Handled();
}

void SUTServerBrowserPanel::ShowServers(FString InitialGameType)
{
	bShowingHubs = false;

	HubButton->UnPressed();
	ServerButton->BePressed();

	BuildServerListControlBox();
	LobbyBrowser->SetVisibility(EVisibility::Hidden);
	InternetServerBrowser->SetVisibility(EVisibility::All);
	ServerListControlBox->SetVisibility(EVisibility::All);
	FilterAllServers();
	AddGameFilters();
	InternetServerList->RequestListRefresh();
}

void SUTServerBrowserPanel::ShowHUBs()
{
	bShowingHubs = true;

	HubButton->BePressed();
	ServerButton->UnPressed();

	BuildServerListControlBox();
	LobbyBrowser->SetVisibility(EVisibility::All);
	InternetServerBrowser->SetVisibility(EVisibility::Hidden);
	ServerListControlBox->SetVisibility(EVisibility::Collapsed);
	FilterAllHUBs();
	HUBServerList->RequestListRefresh();
}


TSharedRef<ITableRow> SUTServerBrowserPanel::OnGenerateWidgetForHUBList(TSharedPtr<FServerData> InItem, const TSharedRef<STableViewBase>& OwnerTable )
{
	bool bPassword = (InItem->Flags & SERVERFLAG_RequiresPassword )  > 0;
	bool bRestricted = (InItem->Flags & SERVERFLAG_Restricted )  > 0;

	return SNew(STableRow<TSharedPtr<FServerData>>, OwnerTable)
		.Style(SUTStyle::Get(),"UT.List.Row")
		[
			SNew(SBox)
			.HeightOverride(80)
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
						SNew(SVerticalBox)
						+SVerticalBox::Slot()
						.FillHeight(1.0)
						.VAlign(VAlign_Center)
						[
							AddHUBBadge(InItem)
						]
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
							.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Medium")
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
/*
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
*/
									+SHorizontalBox::Slot()
									.Padding(0.0,0.0,20.0,0.0)
									.AutoWidth()
									.VAlign(VAlign_Center)
									[
										SNew(STextBlock)
										.Text(InItem->bFakeHUB ? 
											TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateSP(InItem.Get(), &FServerData::GetNumServers)) :
											TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateSP(InItem.Get(), &FServerData::GetNumMatches)))
										.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Small")
									]

									+SHorizontalBox::Slot()
									.Padding(0.0,0.0,20.0,0.0)
									.AutoWidth()
									.VAlign(VAlign_Center)
									[
										SNew(STextBlock)
										.Text(TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateSP(InItem.Get(), &FServerData::GetNumPlayers)))
										.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Small")
									]

									+SHorizontalBox::Slot()
									.Padding(0.0,0.0,20.0,0.0)
									.AutoWidth()
									.VAlign(VAlign_Center)
									[
										SNew(STextBlock)
										.Text(TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateSP(InItem.Get(), &FServerData::GetNumFriends)))
										.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Small")
									]
									+ SHorizontalBox::Slot()
									.FillWidth(1.0)
									+ SHorizontalBox::Slot()
									.Padding(0.0, 0.0, 20.0, 0.0)
									.AutoWidth()
									.VAlign(VAlign_Center)
									[
										SNew(SBox)
										.HeightOverride(18)
										.WidthOverride(18)
										[
											SNew(SImage)
											.Visibility(bPassword || bRestricted ? EVisibility::HitTestInvisible : EVisibility::Hidden)
											.Image(SUTStyle::Get().GetBrush("UT.Icon.Lock.Small"))
										]
									]
									+SHorizontalBox::Slot()
									.Padding(10.0,0.0,20.0,0.0)
									.AutoWidth()
									.VAlign(VAlign_Center)
									[
										SNew(SVerticalBox)
										+SVerticalBox::Slot()
										.HAlign(HAlign_Right)
										[
											SNew(STextBlock)
											.Text(TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateSP(InItem.Get(), &FServerData::GetHubPing)))
											.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Small")
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

TSharedRef<SWidget> SUTServerBrowserPanel::AddHUBBadge(TSharedPtr<FServerData> HUB)
{

	if (HUB->bFakeHUB || HUB->TrustLevel > 1)
	{
		return 	SNew(SBox)						// First the overlaid box that controls everything....
			.HeightOverride(54)
			.WidthOverride(54)
			[
				SNew(SImage)
				.Image(SUTStyle::Get().GetBrush("UT.Icon.Server.Untrusted"))
			];
	}
	else
	{
		if (HUB->TrustLevel == 0)
		{
			return 	SNew(SBox).HeightOverride(54).WidthOverride(54)
				[
					SNew(SImage)
					.Image(SUTStyle::Get().GetBrush("UT.Icon.Server.Epic"))
				];
		}
		else
		{
			return 	SNew(SBox).HeightOverride(54).WidthOverride(54)
				[
					SNew(SImage)
					.Image(SUTStyle::Get().GetBrush("UT.Icon.Server.Trusted"))
				];
		}
	
	}

}

TSharedRef<SWidget> SUTServerBrowserPanel::AddStars(TSharedPtr<FServerData> HUB)
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
					.Image(SUTStyle::Get().GetBrush("UT.Icon.Star.24x24"))
				]
			];
		}
	}

	return StarBox.ToSharedRef();
}


void SUTServerBrowserPanel::AddHUBInfo(TSharedPtr<FServerData> HUB)
{
	if (LobbyInfoText.IsValid())
	{
		LobbyInfoText->SetText(FText::FromString(HUB->MOTD));
	}
	LobbyMatchPanel->SetServerData(HUB);

}

void SUTServerBrowserPanel::OnHUBListSelectionChanged(TSharedPtr<FServerData> SelectedItem, ESelectInfo::Type SelectInfo)
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

void SUTServerBrowserPanel::BuildServerListControlBox()
{
	if (ServerListControlBox.IsValid())
	{
		TSharedPtr<SUTButton> Button;
		ServerListControlBox->ClearChildren();

		if (!bShowingHubs)
		{
			ServerListControlBox->AddSlot()
				.AutoWidth()
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
							SNew(SVerticalBox)
							+SVerticalBox::Slot()
							.VAlign(VAlign_Center)
							[
								SNew(STextBlock)
								.Text(NSLOCTEXT("SUTServerBrowserPanel","GameFilter","Game Mode:"))
								.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Small")
							]
						]
						+SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(0.0f, 0.0f, 15.0f,0.0f)
						[
							SAssignNew(GameFilter, SUTComboButton)
							.HasDownArrow(false)
							.ButtonStyle(SUTStyle::Get(), "UT.Button.MenuBar")
							.ContentPadding(FMargin(10.0f, 0.0f, 10.0f, 0.0))
							.Text(this, &SUTServerBrowserPanel::GetGameFilterText)
							.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Small.Bold")
						]
					]
				];
		}
	}

}

void SUTServerBrowserPanel::OnShowPanel(TSharedPtr<SUTMenuBase> inParentWindow)
{
	SUTPanelBase::OnShowPanel(inParentWindow);

	if (AnimWidget.IsValid())
	{
		AnimWidget->Animate(FVector2D(200, 0), FVector2D(0, 0), 0.0f, 1.0f, 0.3f);
	}

	if (bNeedsRefresh)
	{
		RefreshServers();
	}

	PlayerOwner->GetWorld()->GetTimerManager().SetTimer(RefreshTimerHandle, FTimerDelegate::CreateSP(this, &SUTServerBrowserPanel::RefreshSelectedServer), 30.f, true);
}

void SUTServerBrowserPanel::OnHidePanel()
{
	bClosing = true;
	if (AnimWidget.IsValid())
	{
		AnimWidget->Animate(FVector2D(0, 0), FVector2D(200, 0), 1.0f, 0.0f, 0.3f);
	}
	else
	{
		SUTPanelBase::OnHidePanel();
	}

	PlayerOwner->GetWorld()->GetTimerManager().ClearTimer(RefreshTimerHandle);
}

void SUTServerBrowserPanel::RefreshSelectedServer()
{
	TArray<TSharedPtr<FServerData>> SelectedItems = (bShowingHubs ? HUBServerList->GetSelectedItems() : InternetServerList->GetSelectedItems());
	if (SelectedItems.Num() > 0)
	{
		PingServer(SelectedItems[0]);
	}
}

void SUTServerBrowserPanel::AnimEnd()
{
	if (bClosing)
	{
		bClosing = false;
		TSharedPtr<SWidget> Panel = this->AsShared();
		ParentWindow->PanelHidden(Panel);
		ParentWindow.Reset();
	}
}


FName SUTServerBrowserPanel::GetBrowserState()
{
	return BrowserState;
}

void SUTServerBrowserPanel::JoinQuickInstance(const FString& InstanceGuid, bool bAsSpectator)
{
	TArray<TSharedPtr<FServerData>> SelectedHubs = HUBServerList->GetSelectedItems();

	if (SelectedHubs.Num() > 0 && SelectedHubs[0].IsValid())
	{
		if ((SelectedHubs[0]->Flags & SERVERFLAG_Restricted) > 0)
		{
			RestrictedWarning();
			return;
		}

		PlayerOwner->AttemptJoinInstance(SelectedHubs[0], InstanceGuid, bAsSpectator);
	}

}

FText SUTServerBrowserPanel::GetShowHubButtonText() const
{
	FText Count = FText::AsNumber(FilteredHubsSource.Num() > 0 ? FilteredHubsSource.Num() : AllHubServers.Num());
	return FText::Format(NSLOCTEXT("SUTServerBrowserPanel","HubButtonFormat","({0})"), Count);
}

FText SUTServerBrowserPanel::GetShowServerButtonText() const
{
	FText Count = FText::AsNumber(FilteredServersSource.Num() > 0 ? FilteredServersSource.Num() : AllInternetServers.Num());
	return FText::Format(NSLOCTEXT("SUTServerBrowserPanel", "HubButtonFormat", "({0})"), Count);
}

FSlateColor SUTServerBrowserPanel::GetButtonSlateColor(int32 TabIndex) const
{
	TSharedPtr<SUTButton> Button = TabIndex == 0 ? HubButton : ServerButton;
	return ( (TabIndex == 0 && bShowingHubs) || (TabIndex == 1 && !bShowingHubs) || (Button.IsValid() && Button->IsHovered())) ? FSlateColor(FLinearColor::Black) : FSlateColor(FLinearColor::White);
}


EVisibility SUTServerBrowserPanel::JoinEnable() const
{
	if (BrowserState == EBrowserState::BrowserIdle) 
	{
		TArray<TSharedPtr<FServerData>> Selected;
		if (bShowingHubs && HUBServerList.IsValid() && HUBServerList->GetNumItemsSelected() > 0) Selected = HUBServerList->GetSelectedItems();
		else if (!bShowingHubs && InternetServerList.IsValid() && InternetServerList->GetNumItemsSelected() > 0) Selected = InternetServerList->GetSelectedItems();

		if ( Selected.Num() > 0 && Selected[0].IsValid() && (Selected[0]->Flags & SERVERFLAG_Restricted) == 0 )
		{
			return EVisibility::Visible;
		}
	}
	return EVisibility::Collapsed;
}

EVisibility SUTServerBrowserPanel::SpectateEnable() const
{
	if (!bShowingHubs && BrowserState == EBrowserState::BrowserIdle) 
	{
		TArray<TSharedPtr<FServerData>> Selected;
		if (bShowingHubs && HUBServerList.IsValid() && HUBServerList->GetNumItemsSelected() > 0) Selected = HUBServerList->GetSelectedItems();
		else if (!bShowingHubs && InternetServerList.IsValid() && InternetServerList->GetNumItemsSelected() > 0) Selected = InternetServerList->GetSelectedItems();

		if ( Selected.Num() > 0 && Selected[0].IsValid() && (Selected[0]->Flags & SERVERFLAG_Restricted) == 0 )
		{
			return EVisibility::Visible;
		}
	}
	return EVisibility::Collapsed;
}

EVisibility SUTServerBrowserPanel::AbortEnable() const
{
	return BrowserState == EBrowserState::ConnectInProgress ? EVisibility::Visible : EVisibility::Collapsed;
}


void SUTServerBrowserPanel::BuildConnectBox()
{
	if (ConnectBox.IsValid())
	{
		ConnectBox->ClearChildren();
		if (BrowserState != EBrowserState::ConnectInProgress)
		{
			ConnectBox->AddSlot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0.0f,0.0f,10.0f,0.0f)
			[
				// Press rebuild to clear out the old data items and create the new ones (however many are specified by SEditableTextBox)
				SAssignNew(SpectateButton, SUTButton)
				.ButtonStyle(SUTStyle::Get(), "UT.Button.MenuBar")
				.ContentPadding(FMargin(10.0f, 5.0f, 10.0f, 5.0))
				.Text(NSLOCTEXT("SUTServerBrowserPanel", "IP", "Connect via IP"))
				.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Medium")
				.OnClicked(this, &SUTServerBrowserPanel::OnIPClick)
			];

			ConnectBox->AddSlot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0.0f,0.0f,10.0f,0.0f)
			[
				// Press rebuild to clear out the old data items and create the new ones (however many are specified by SEditableTextBox)
				SAssignNew(SpectateButton, SUTButton)
				.ButtonStyle(SUTStyle::Get(), "UT.Button.MenuBar")
				.ContentPadding(FMargin(10.0f, 5.0f, 10.0f, 5.0))
				.Text(NSLOCTEXT("SUTServerBrowserPanel", "Spectate", "Spectate"))
				.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Medium")
				.OnClicked(this, &SUTServerBrowserPanel::OnJoinClick, true)
				.Visibility(this, &SUTServerBrowserPanel::SpectateEnable)
			];

			ConnectBox->AddSlot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				// Press rebuild to clear out the old data items and create the new ones (however many are specified by SEditableTextBox)
				SAssignNew(JoinButton, SUTButton)
				.ButtonStyle(SUTStyle::Get(), "UT.Button.MenuBar")
				.ContentPadding(FMargin(10.0f, 5.0f, 10.0f, 5.0))
				.Text(NSLOCTEXT("SUTServerBrowserPanel", "Join", "Join"))
				.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Medium")
				.OnClicked(this, &SUTServerBrowserPanel::OnJoinClick, false)
				.Visibility(this, &SUTServerBrowserPanel::JoinEnable)

			];
		}
		else
		{
			ConnectBox->AddSlot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0.0f, 0.0f, 10.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text(ConnectToServerName)
				.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Small")
			];

			ConnectBox->AddSlot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0.0f, 0.0f, 10.0f, 0.0f)
			[
				// Press rebuild to clear out the old data items and create the new ones (however many are specified by SEditableTextBox)
				SAssignNew(SpectateButton, SUTButton)
				.ButtonStyle(SUTStyle::Get(), "UT.Button.MenuBar")
				.ContentPadding(FMargin(10.0f, 5.0f, 10.0f, 5.0))
				.Text(NSLOCTEXT("SUTServerBrowserPanel", "CancelJoin", "Abort"))
				.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Medium")
				.OnClicked(this, &SUTServerBrowserPanel::OnCancelJoinClick)
				.Visibility(this, &SUTServerBrowserPanel::AbortEnable)
			];
		}
	}
}

FReply SUTServerBrowserPanel::OnCancelJoinClick()
{
	return FReply::Handled();
}


FReply SUTServerBrowserPanel::OnIPClick()
{
	PlayerOwner->OpenDialog(
		SNew(SUTInputBoxDialog)
		.DefaultInput(PlayerOwner->LastConnectToIP)
		.DialogSize(FVector2D(700, 300))
		.OnDialogResult(this, &SUTServerBrowserPanel::ConnectIPDialogResult)
		.PlayerOwner(PlayerOwner)
		.DialogTitle(NSLOCTEXT("SUTServerBrowserPanel", "ConnectToIP", "Connect to IP"))
		.MessageText(NSLOCTEXT("SUTServerBrowserPanel", "ConnecToIPDesc", "Enter address to connect to:"))
		.ButtonMask(UTDIALOG_BUTTON_OK | UTDIALOG_BUTTON_CANCEL)
		.IsScrollable(false)
	);
	return FReply::Handled();
}

void SUTServerBrowserPanel::ConnectIPDialogResult(TSharedPtr<SCompoundWidget> Widget, uint16 ButtonID)
{
	if (ButtonID != UTDIALOG_BUTTON_CANCEL)
	{
		TSharedPtr<SUTInputBoxDialog> Box = StaticCastSharedPtr<SUTInputBoxDialog>(Widget);
		if (Box.IsValid())
		{
			FString InputText = Box->GetInputText();
			if (InputText.Len() > 0 && PlayerOwner.IsValid())
			{
				FString AdjustedText = InputText.Replace(TEXT("://"), TEXT(""));
				PlayerOwner->LastConnectToIP = AdjustedText;
				PlayerOwner->SaveConfig();
				PlayerOwner->ViewportClient->ConsoleCommand(*FString::Printf(TEXT("open %s"), *AdjustedText));
				PlayerOwner->ShowConnectingDialog();
			}
		}
	}
}


#endif
