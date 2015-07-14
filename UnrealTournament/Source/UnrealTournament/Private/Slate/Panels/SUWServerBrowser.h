// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Slate/SlateGameResources.h"
#include "../SUWindowsStyle.h"
#include "../SUWPanel.h"
#include "UTLocalPlayer.h"
#include "UTOnlineGameSearchBase.h"
#include "UTOnlineGameSettingsBase.h"
#include "UTServerBeaconClient.h"

#if !UE_SERVER
/*
	Holds data about a server.  
*/

class FServerRuleData 
{
public:
	FString Rule;
	FString Value;

	FServerRuleData(const FString& inRule, const FString& inValue)
		: Rule(inRule)
		, Value(inValue)
	{};

	static TSharedRef<FServerRuleData> Make(const FString& inRule, const FString& inValue)
	{
		return MakeShareable( new FServerRuleData( inRule, inValue ) );
	}
};


class UNREALTOURNAMENT_API SServerRuleRow : public SMultiColumnTableRow< TSharedPtr<FServerRuleData> >
{
public:
	
	SLATE_BEGIN_ARGS( SServerRuleRow )
		: _ServerRuleData()
		{}

		SLATE_ARGUMENT( TSharedPtr<FServerRuleData>, ServerRuleData)
		SLATE_STYLE_ARGUMENT( FTableRowStyle, Style )


	SLATE_END_ARGS()
	
	/**
	 * Construct the widget
	 *
	 * @param InArgs   A declaration from which to construct the widget
	 */
	void Construct( const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView )
	{
		ServerRuleData = InArgs._ServerRuleData;
		
		FSuperRowType::Construct( FSuperRowType::FArguments().Padding(1).Style(InArgs._Style), InOwnerTableView );	
	}

	virtual TSharedRef<SWidget> GenerateWidgetForColumn( const FName& ColumnName ) override
	{
		FSlateFontInfo ItemEditorFont = SUWindowsStyle::Get().GetFontStyle("UWindows.Standard.Font.Small"); //::Get().GetFontStyle(TEXT("NormalFont"));

		FText ColumnText;
		if (ServerRuleData.IsValid())
		{
			if (ColumnName == FName(TEXT("Rule"))) ColumnText = FText::FromString(ServerRuleData->Rule);
			else if (ColumnName == FName(TEXT("Value"))) ColumnText = FText::FromString(ServerRuleData->Value);
		}
		else
		{
			ColumnText = NSLOCTEXT("SUWServerBrowser","UnknownColumnText","n/a");
		}

		return SNew( STextBlock )
			.Font( ItemEditorFont )
			.Text( ColumnText );
	}

	
private:

	/** A pointer to the data item that we visualize/edit */
	TSharedPtr<FServerRuleData> ServerRuleData;
};


class FServerPlayerData 
{
public:
	FString PlayerName;
	FString Score;
	FString Id;

	FServerPlayerData(const FString& inPlayer, const FString& inScore, const FString& inId)
		: PlayerName(inPlayer)
		, Score(inScore)
		, Id(inId)
	{};

	static TSharedRef<FServerPlayerData> Make(const FString& inPlayerName, const FString& inScore, const FString& inId)
	{
		return MakeShareable( new FServerPlayerData( inPlayerName, inScore, inId ) );
	}
};

class UNREALTOURNAMENT_API SServerPlayerRow : public SMultiColumnTableRow< TSharedPtr<FServerPlayerData> >
{
public:
	
	SLATE_BEGIN_ARGS( SServerPlayerRow )
		: _ServerPlayerData()
		{}

		SLATE_ARGUMENT( TSharedPtr<FServerPlayerData>, ServerPlayerData)
		SLATE_STYLE_ARGUMENT( FTableRowStyle, Style )


	SLATE_END_ARGS()
	
	/**
	 * Construct the widget
	 *
	 * @param InArgs   A declaration from which to construct the widget
	 */
	void Construct( const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView )
	{
		ServerPlayerData = InArgs._ServerPlayerData;
		
		FSuperRowType::Construct( FSuperRowType::FArguments().Padding(1).Style(InArgs._Style), InOwnerTableView );	
	}

	virtual TSharedRef<SWidget> GenerateWidgetForColumn( const FName& ColumnName ) override
	{
		FSlateFontInfo ItemEditorFont = SUWindowsStyle::Get().GetFontStyle("UWindows.Standard.Font.Small"); //::Get().GetFontStyle(TEXT("NormalFont"));

		FText ColumnText;
		if (ServerPlayerData.IsValid())
		{
			if (ColumnName == FName(TEXT("Name"))) ColumnText = FText::FromString(ServerPlayerData->PlayerName);
			else if (ColumnName == FName(TEXT("Score"))) ColumnText = FText::FromString(ServerPlayerData->Score);
		}
		else
		{
			ColumnText = NSLOCTEXT("SUWServerBrowser","UnknownColumnText","n/a");
		}

		return SNew( STextBlock )
			.Font( ItemEditorFont )
			.Text( ColumnText );
	}

	
private:

	/** A pointer to the data item that we visualize/edit */
	TSharedPtr<FServerPlayerData> ServerPlayerData;
};

static const FString LOBBY_GAME_PATH = TEXT("/Script/UnrealTournament.UTLobbyGameMode");

class FServerData : public TSharedFromThis<FServerData>
{
public: 
	// The Server's name
	FString Name;

	// The Server IP Address
	FString IP;

	// The Server Beacon IP Address
	FString BeaconIP;

	// The Game mode running on the server (class path)
	FString GameModePath;
	// The game mode running on the server (localized name)
	FString GameModeName;

	// The Map
	FString Map;

	// Number of players on this server
	int32 NumPlayers;

	// Number of Spectators
	int32 NumSpectators;

	// Max. # of players allowed on this server
	int32 MaxPlayers;

	// Max. # of players allowed on this server
	int32 MaxSpectators;

	// # of friends on this server
	int32 NumFriends;

	// HUB - returns the # of matches currently happening in this hub
	int32 NumMatches;

	// How many stars to display
	float Rating;

	// What is the min. rank that is allowed on this server
	int32 MinRank;

	// What is the max. rank that is allowed on this server
	int32 MaxRank;

	// What UT/UE4 version the server is running
	FString Version;

	// What is the player's current ping to this server
	int32 Ping;

	// Server Flags
	int32 Flags;

	// The Server trust level
	int32 TrustLevel;

	FString MOTD;

	FOnlineSessionSearchResult SearchResult;

	bool bFakeHUB;

	TArray<TSharedPtr<FServerInstanceData>> HUBInstances;
	TArray<TSharedPtr<FServerRuleData>> Rules;
	TArray<TSharedPtr<FServerPlayerData>> Players;

	FServerData(const FString& inName, const FString& inIP, const FString& inBeaconIP, const FString& inGameModePath, const FString& inGameModeName, const FString& inMap, int32 inNumPlayers, int32 inNumSpecators, int32 inMaxPlayers, int32 inMaxSpectators, int32 inNumMatches, int32 inMinRank, int32 inMaxRank, const FString& inVersion, int32 inPing, int32 inFlags, int32 inTrustLevel)
	: Name( inName )
	, IP( inIP )
	, BeaconIP( inBeaconIP )
	, GameModePath( inGameModePath )
	, GameModeName( inGameModeName )
	, Map ( inMap )
	, NumPlayers( inNumPlayers )
	, NumSpectators( inNumSpecators )
	, MaxPlayers( inMaxPlayers )
	, MaxSpectators( inMaxSpectators )
	, NumMatches( inNumMatches) 
	, MinRank ( inMinRank )
	, MaxRank ( inMaxRank )
	, Version( inVersion ) 
	, Ping( inPing )
	, Flags( inFlags )
	, TrustLevel( inTrustLevel )
	{		
		Rules.Empty();
		Players.Empty();
		NumFriends = 0;	// Move me once implemented
		MOTD = TEXT("");
		bFakeHUB = false;
	}

	static TSharedRef<FServerData> Make(const FString& inName, const FString& inIP, const FString& inBeaconIP, const FString& inGameModePath, const FString& inGameModeName, const FString& inMap, int32 inNumPlayers, int32 inNumSpecators, int32 inMaxPlayers,  int32 inMaxSpectators, int32 inNumMatches, int32 inMinRank, int32 inMaxRank, const FString& inVersion, int32 inPing, int32 inFlags, int32 inTrustLevel)
	{
		return MakeShareable( new FServerData( inName, inIP, inBeaconIP, inGameModePath, inGameModeName, inMap, inNumPlayers, inNumSpecators, inMaxPlayers, inMaxSpectators, inNumMatches, inMinRank, inMaxRank, inVersion, inPing, inFlags, inTrustLevel ) );
	}

	void Update(TSharedPtr<FServerData> NewData)
	{
		Name = NewData->Name;
		IP = NewData->IP;
		BeaconIP = NewData->BeaconIP;
		GameModePath = NewData->GameModePath;
		GameModeName = NewData->GameModeName;
		Map = NewData->Map;
		NumPlayers = NewData->NumPlayers;
		NumSpectators = NewData->NumSpectators;
		MaxPlayers = NewData->MaxPlayers;
		MaxSpectators = NewData->MaxSpectators;
		NumFriends = NewData->NumFriends;
		NumMatches = NewData->NumMatches;
		Rating = NewData->Rating;
		MinRank = NewData->MinRank;
		MaxRank = NewData->MaxRank;
		Version = NewData->Version;
		Ping = NewData->Ping;
		Flags = NewData->Flags;
		MOTD = NewData->MOTD;
		TrustLevel = NewData->TrustLevel;
		SearchResult = NewData->SearchResult;



		HUBInstances.Empty(); Rules.Empty(); Players.Empty();
		for (int32 i=0;i<NewData->HUBInstances.Num();i++) HUBInstances.Add(NewData->HUBInstances[i]);
		for (int32 i=0;i<NewData->Rules.Num();i++) Rules.Add(NewData->Rules[i]);
		for (int32 i=0;i<NewData->Players.Num();i++) Players.Add(NewData->Players[i]);
	}


	void AddPlayer(const FString& PlayerName, const FString& Score, const FString& PlayerId)
	{
		Players.Add(FServerPlayerData::Make(PlayerName, Score, PlayerId));
	}

	void AddRule(const FString& Rule, const FString& Value)
	{
		Rules.Add( FServerRuleData::Make(Rule, Value));
	}

	FSlateColor GetDrawColorAndOpacity() const
	{
		return Ping >= 0 ? FSlateColor(FLinearColor::White) : FSlateColor(FLinearColor::Red);
	}

	FText GetBrowserName() 
	{ 
		return FText::FromString(Name); 
	}

	FText GetBrowserIP() 
	{ 
		return FText::FromString(IP); 
	}

	FText GetBrowserGameMode() 
	{ 
		return FText::FromString(GameModeName); 
	}

	FText GetBrowserMapName() 
	{ 
		return FText::FromString(Map); 
	}

	FText GetBrowserPing() 
	{ 
		if (Ping < 0)
		{
			return NSLOCTEXT("ServerBrowser", "BadPing", "---");
		}
	
		return FText::AsNumber(Ping); 
	}

	FText GetHubPing()
	{
		if (Ping < 0)
		{
			return NSLOCTEXT("ServerBrowser", "BadPing", "---");
		}
		return FText::Format(NSLOCTEXT("ServerBrowser","PingFormat","{0}ms"), FText::AsNumber(Ping));	
	}

	FText GetBrowserNumPlayers()
	{
		return FText::Format(NSLOCTEXT("ServerBrowser", "NumPlayers", "{0}/{1}"), FText::AsNumber(NumPlayers),FText::AsNumber(MaxPlayers) );
	}

	FText GetBrowserNumSpectators()
	{
		return FText::Format(NSLOCTEXT("ServerBrowser", "NumSpectators", "{0}/{1}"), FText::AsNumber(NumSpectators), FText::AsNumber(MaxSpectators));
	}

	FText GetBrowserNumFriends()
	{
		return FText::Format(NSLOCTEXT("ServerBrowser", "NumFriends", "{0}"), FText::AsNumber(NumFriends));
	}

	FText GetNumServers()
	{
		return FText::Format(NSLOCTEXT("HUBBrowser", "NumServersFormat", "{0} Servers"), FText::AsNumber(NumMatches));
	}


	FText GetNumMatches()
	{
		return FText::Format(NSLOCTEXT("HUBBrowser","NumMatchesFormat","{0} Matches"), FText::AsNumber(NumMatches));
	}

	FText GetNumPlayers()
	{
		return FText::Format(NSLOCTEXT("HUBBrowser", "NumPlayersFormat", "{0} Players"), FText::AsNumber(NumPlayers));
	}

	FText GetNumFriends()
	{
		return FText::Format(NSLOCTEXT("HUBBrowser", "NumFriendsFormat", "{0} Friends"), FText::AsNumber(NumFriends));
	}

};


/**
 * An Item Editor used by list testing.
 * It visualizes a string and edits its contents.
 */
class UNREALTOURNAMENT_API SServerBrowserRow : public SMultiColumnTableRow< TSharedPtr<FServerData> >
{
public:
	
	SLATE_BEGIN_ARGS( SServerBrowserRow )
		: _ServerData()
		{}

		SLATE_ARGUMENT( TSharedPtr<FServerData>, ServerData)
		SLATE_STYLE_ARGUMENT( FTableRowStyle, Style )


	SLATE_END_ARGS()
	
	/**
	 * Construct the widget
	 *
	 * @param InArgs   A declaration from which to construct the widget
	 */
	void Construct( const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView )
	{
		ServerData = InArgs._ServerData;
		
		FSuperRowType::Construct( FSuperRowType::FArguments().Padding(1).Style(InArgs._Style), InOwnerTableView );	
	}

	virtual TSharedRef<SWidget> GenerateWidgetForColumn( const FName& ColumnName ) override
	{
		FSlateFontInfo ItemEditorFont = SUWindowsStyle::Get().GetFontStyle("UWindows.Standard.Font.Small"); //::Get().GetFontStyle(TEXT("NormalFont"));
		bool bErrorPing = (ServerData->Ping < 0);
		FText ColumnText;
		if (ServerData.IsValid())
		{
			if (ColumnName == FName(TEXT("ServerName"))) 
			{
				return SNew(STextBlock)
					.Font(ItemEditorFont)
					.Text(TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateSP(ServerData.Get(), &FServerData::GetBrowserName)))
					.ColorAndOpacity(TAttribute<FSlateColor>::Create(TAttribute<FSlateColor>::FGetter::CreateSP(ServerData.Get(), &FServerData::GetDrawColorAndOpacity)));
			}
			else if (ColumnName == FName(TEXT("ServerIP"))) 
			{
				return SNew(STextBlock)
					.Font(ItemEditorFont)
					.Text(TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateSP(ServerData.Get(), &FServerData::GetBrowserIP)))
					.ColorAndOpacity(TAttribute<FSlateColor>::Create(TAttribute<FSlateColor>::FGetter::CreateSP(ServerData.Get(), &FServerData::GetDrawColorAndOpacity)));
			}
			else if (ColumnName == FName(TEXT("ServerGame"))) 
			{
				return SNew(STextBlock)
					.Font(ItemEditorFont)
					.Text(TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateSP(ServerData.Get(), &FServerData::GetBrowserGameMode)))
					.ColorAndOpacity(TAttribute<FSlateColor>::Create(TAttribute<FSlateColor>::FGetter::CreateSP(ServerData.Get(), &FServerData::GetDrawColorAndOpacity)));
			}
			else if (ColumnName == FName(TEXT("ServerMap"))) 
			{
				return SNew(STextBlock)
					.Font(ItemEditorFont)
					.Text(TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateSP(ServerData.Get(), &FServerData::GetBrowserMapName)))
					.ColorAndOpacity(TAttribute<FSlateColor>::Create(TAttribute<FSlateColor>::FGetter::CreateSP(ServerData.Get(), &FServerData::GetDrawColorAndOpacity)));
			}
			else if (ColumnName == FName(TEXT("ServerNumPlayers")))
			{
				return SNew(STextBlock)
					.Font(ItemEditorFont)
					.Text(TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateSP(ServerData.Get(), &FServerData::GetBrowserNumPlayers)))
					.ColorAndOpacity(TAttribute<FSlateColor>::Create(TAttribute<FSlateColor>::FGetter::CreateSP(ServerData.Get(), &FServerData::GetDrawColorAndOpacity)));
			}
			else if (ColumnName == FName(TEXT("ServerNumSpecs"))) 
			{
				return SNew(STextBlock)
					.Font(ItemEditorFont)
					.Text(TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateSP(ServerData.Get(), &FServerData::GetBrowserNumSpectators)))
					.ColorAndOpacity(TAttribute<FSlateColor>::Create(TAttribute<FSlateColor>::FGetter::CreateSP(ServerData.Get(), &FServerData::GetDrawColorAndOpacity)));
			}
			else if (ColumnName == FName(TEXT("ServerNumFriends")))
			{
				return SNew(STextBlock)
					.Font(ItemEditorFont)
					.Text(TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateSP(ServerData.Get(), &FServerData::GetBrowserNumFriends)))
					.ColorAndOpacity(TAttribute<FSlateColor>::Create(TAttribute<FSlateColor>::FGetter::CreateSP(ServerData.Get(), &FServerData::GetDrawColorAndOpacity)));
			}
			else if (ColumnName == FName(TEXT("ServerPing"))) 
			{
				return SNew(STextBlock)
					.Font(ItemEditorFont)
					.Text(TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateSP(ServerData.Get(), &FServerData::GetBrowserPing)))
					.ColorAndOpacity(TAttribute<FSlateColor>::Create(TAttribute<FSlateColor>::FGetter::CreateSP(ServerData.Get(), &FServerData::GetDrawColorAndOpacity)));
			}
			else if (ColumnName == FName(TEXT("ServerFlags"))) 
			{
				TSharedPtr<SHorizontalBox> IconBox;
				SAssignNew(IconBox, SHorizontalBox);
			

				if (ServerData->SearchResult.Session.SessionSettings.bIsLANMatch)
				{
					IconBox->AddSlot()
						[
							SNew( SImage )		
								.Image(SUWindowsStyle::Get().GetBrush("UWindows.Standard.ServerBrowser.Lan"))
						];
				
				}

				if ( (ServerData->Flags & 0x01) != 0)
				{
					IconBox->AddSlot()
						[
							SNew( SImage )		
								.Image(SUWindowsStyle::Get().GetBrush("UWindows.Standard.ServerBrowser.Lock"))
						];
				}

				return IconBox->AsShared();
			}
			
			else ColumnText = NSLOCTEXT("SUWServerBrowser","UnknownColumnText","n/a");
		}
		else
		{
			ColumnText = NSLOCTEXT("SUWServerBrowser","UnknownColumnText","n/a");
		}

		if (bErrorPing)
		{
			return SNew( STextBlock )
				.Font( ItemEditorFont )
				.Text( ColumnText )
				.ColorAndOpacity(FLinearColor(0.33f,0.0f,0.0f,1.0f));

		}

		return SNew( STextBlock )
			.Font( ItemEditorFont )
			.Text( ColumnText );

	}

	
private:

	/** A pointer to the data item that we visualize/edit */
	TSharedPtr<FServerData> ServerData;
};


struct FServerPingTracker 
{
	TSharedPtr<FServerData> Server;
	TWeakObjectPtr<AUTServerBeaconClient> Beacon;

	FServerPingTracker(TSharedPtr<FServerData> inServer, AUTServerBeaconClient* inBeacon)
		: Server(inServer)
		, Beacon(inBeacon)
	{};
};

namespace EBrowserState
{
	// Compressed Texture Formats
	static FName NotLoggedIn = TEXT("NotLoggedIn");
	static FName BrowserIdle = TEXT("BrowserIdle");
	static FName AuthInProgress = TEXT("AuthInProgress");
	static FName RefreshInProgress = TEXT("RefreshInProgress");
}

class UNREALTOURNAMENT_API SUWServerBrowser : public SUWPanel
{
public:

	~SUWServerBrowser();

	virtual FName GetBrowserState();
	virtual bool IsRefreshing()
	{
		return BrowserState == EBrowserState::RefreshInProgress;
	}

private:

	virtual void ConstructPanel(FVector2D ViewportSize);	

protected:

	FName BrowserState;

	bool bShowingHubs;

	IOnlineSubsystem* OnlineSubsystem;
	IOnlineIdentityPtr OnlineIdentityInterface;
	IOnlineSessionPtr OnlineSessionInterface;

	TSharedPtr<SDPIScaler> UnScaler;

	TSharedPtr<class SButton> JoinIPButton;
	TSharedPtr<class SButton> RefreshButton;
	TSharedPtr<class SButton> SpectateButton;
	TSharedPtr<class SButton> JoinButton;
	TSharedPtr<class STextBlock> StatusText;
	TSharedPtr<class STextBlock> BrowserTypeText;
	TSharedPtr<class SComboButton> GameFilter;
	TSharedPtr<class STextBlock> GameFilterText;

	// The Slate widget that displays the current list
	TSharedPtr< SListView< TSharedPtr<FServerData> > > InternetServerList;
	TSharedPtr< SListView< TSharedPtr<FServerData> > > HUBServerList;

	TSharedPtr< SListView< TSharedPtr<FServerRuleData> > > RulesList;
	TSharedPtr< SListView< TSharedPtr<FServerPlayerData> > > PlayersList;
	
	// SList doesn't allow you to change the item source after construction, so
	// we have to maintain our own list and copy.  Yech!

	TArray< TSharedPtr< FServerRuleData > > RulesListSource;
	TArray< TSharedPtr< FServerPlayerData > > PlayersListSource;

	// The list being displays.  It only includes servers that have met the filter requirement.
	TArray< TSharedPtr< FServerData > > FilteredServersSource;

	// The list being displays.  It only includes servers that have met the filter requirement.
	TArray< TSharedPtr< FServerData > > FilteredHubsSource;

	// When there is a server in this list, then the game will attempt to ping them.  
	TArray< TSharedPtr< FServerData > > PingList;

	// The whole list of non-Hub Servers
	TArray< TSharedPtr< FServerData > > AllInternetServers;

	// The whole list of Hubs
	TArray< TSharedPtr< FServerData > > AllHubServers;

	// A list of "pending ping requests".
	TArray< FServerPingTracker> PingTrackers;

	TSharedRef<ITableRow> OnGenerateWidgetForList( TSharedPtr<FServerData> InItem, const TSharedRef<STableViewBase>& OwnerTable );
	TSharedRef<ITableRow> OnGenerateWidgetForRulesList( TSharedPtr<FServerRuleData> InItem, const TSharedRef<STableViewBase>& OwnerTable );
	TSharedRef<ITableRow> OnGenerateWidgetForPlayersList( TSharedPtr<FServerPlayerData> InItem, const TSharedRef<STableViewBase>& OwnerTable );
	TSharedRef<ITableRow> OnGenerateWidgetForHUBList(TSharedPtr<FServerData> InItem, const TSharedRef<STableViewBase>& OwnerTable );

	virtual FReply OnRefreshClick();
	virtual void RefreshServers();

	void OnFindSessionsComplete(bool bWasSuccessful);
	void OnFindLANSessionsComplete(bool bWasSuccessful);

	virtual void SetBrowserState(FName NewBrowserState);
	virtual void CleanupQoS();

	virtual void OnListMouseButtonDoubleClick(TSharedPtr<FServerData> SelectedServer);
	virtual void OnSort(EColumnSortPriority::Type, const FName&, EColumnSortMode::Type);
	virtual FReply OnJoinClick(bool bSpectate);
	virtual void ConnectTo(FServerData ServerData,bool bSpectate);

	virtual void OnRuleSort(EColumnSortPriority::Type, const FName&, EColumnSortMode::Type);
	virtual void OnPlayerSort(EColumnSortPriority::Type, const FName&, EColumnSortMode::Type);

	virtual void AddGameFilters();
	virtual FReply OnGameFilterSelection(FString Filter);

	void SortServers(FName ColumnName);
	void SortHUBs();
	virtual void FilterAllServers();
	virtual void FilterServer(TSharedPtr< FServerData > NewServer, bool bSortAndUpdate = true, int32 BestPing = 0);
	virtual void FilterAllHUBs();
	virtual void FilterHUB(TSharedPtr< FServerData > NewServer, bool bSortAndUpdate = true, int32 BestPing = 0);

	FDelegateHandle PlayerOnlineStatusChangedDelegate;
	virtual void OwnerLoginStatusChanged(UUTLocalPlayer* LocalPlayerOwner, ELoginStatus::Type NewStatus, const FUniqueNetId& UniqueID);

	TSharedPtr<class SHeaderRow> HeaderRow;
	TSharedPtr<class SHeaderRow> RulesHeaderRow;
	TSharedPtr<class SHeaderRow> PlayersHeaderRow;

	TSharedPtr<class SEditableTextBox> QuickFilterText;
	TSharedPtr<class SBox> InternetServerBrowser;
	TSharedPtr<class SBox> LobbyBrowser;

	TSharedPtr<class SSplitter> VertSplitter;
	TSharedPtr<class SSplitter> HorzSplitter;

	TSharedPtr<class SSplitter> LobbySplitter;
	TSharedPtr<class SVerticalBox> LobbyInfoBox;

	bool bRequireSave;
	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime );

	void OnServerListSelectionChanged(TSharedPtr<FServerData> SelectedItem, ESelectInfo::Type SelectInfo);
	void OnHUBListSelectionChanged(TSharedPtr<FServerData> SelectedItem, ESelectInfo::Type SelectInfo);

	virtual void OnShowPanel(TSharedPtr<SUWindowsDesktop> inParentWindow);

protected:

	// If true, the server browser will refresh itself as soon as it's loaded.
	bool bNeedsRefresh;

	virtual void OnServerBeaconResult(AUTServerBeaconClient* Sender, FServerBeaconInfo ServerInfo);
	virtual void OnServerBeaconFailure(AUTServerBeaconClient* Sender);
	virtual void PingNextServer();
	virtual void PingServer(TSharedPtr<FServerData> ServerToPing);

	virtual TSharedRef<SWidget> BuildPlayerList();

	virtual void VertSplitterResized();
	virtual void HorzSplitterResized();

	virtual TSharedRef<SWidget> BuildServerBrowser();
	virtual TSharedRef<SWidget> BuildLobbyBrowser();

	virtual void OnQuickFilterTextCommited(const FText& NewText, ETextCommit::Type CommitType);
	virtual FReply BrowserTypeChanged();
	
	// Walk over both the lists and expire out any servers not available on the MCP
	virtual void ExpireDeadServers();

	// Adds a server if it doesn't exist
	virtual void AddServer(TSharedPtr<FServerData> Server);

	// Adds a hub if it doesn't exist
	virtual void AddHub(TSharedPtr<FServerData> Hub);

	virtual void ShowServers(FString InitialGameType);
	virtual void ShowHUBs();

	virtual TSharedRef<SWidget> AddHUBBadge(TSharedPtr<FServerData> HUB);
	virtual TSharedRef<SWidget> AddStars(TSharedPtr<FServerData> HUB);
	virtual TSharedRef<SWidget> AddHUBInstances(TSharedPtr<FServerData> HUB);
	virtual void AddHUBInfo(TSharedPtr<FServerData> HUB);
	virtual void BuildServerListControlBox();

	virtual void TallyInternetServers(int32& Players, int32& Spectators, int32& Friends);

private:
	bool bAutoRefresh;

	bool bDescendingSort;
	bool bDescendingRulesSort;
	bool bDescendingPlayersSort;
	
	FName CurrentSortColumn;
	FName CurrentRulesSortColumn;
	FName CurrentPlayersSortColumn;

	FDelegateHandle OnFindSessionCompleteDelegate;
	TSharedPtr<class FUTOnlineGameSearchBase> SearchSettings;
	TSharedPtr<class FUTOnlineGameSearchBase> LanSearchSettings;

	TSharedPtr<FServerData> RandomHUB;
	TSharedPtr<SHorizontalBox> ServerListControlBox;

	float GetReverseScale() const;
	
	void OnReadFriendsListComplete(int32 LocalUserNum, bool bWasSuccessful, const FString& ListName, const FString& ErrorStr);

	virtual FText GetStatusText() const;

protected:
	ECheckBoxState ShouldHideUnresponsiveServers() const;
	void OnHideUnresponsiveServersChanged(const ECheckBoxState NewState);
	TSharedPtr<SCheckBox> HideUnresponsiveServersCheckbox;
	bool bHideUnresponsiveServers;

	bool IsUnresponsive(TSharedPtr<FServerData> Server, int32 BestPing = 0);

	bool bWantsAFullRefilter;

	int32 TotalPlayersPlaying;

	void FoundServer(FOnlineSessionSearchResult& Result);
};

#endif